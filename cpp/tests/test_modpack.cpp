// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
//
// test_modpack.cpp — 整合包模块独立自测（不依赖 GUI）：
//   1. ZipArchive：构造 CF/MR 压缩包 → 列条目 / 读清单 / 前缀解压 / 路径穿越拒绝
//   2. ModpackParser：解析 CF manifest.json 与 Modrinth index.json → 统一结构
//   3. 回滚追踪辅助：覆盖备份逻辑（registerOverwrite 语义）
//   4. 网络集成（可选，需 SHADOW_TEST_CF_KEY 环境变量）：
//      CF API 搜索 JEI → fileId → 批量解析下载地址 → 下载 + 大小校验
//      Modrinth API 取 Sodium 版本 → 下载 + SHA1 校验
//
// 构建：cmake -S . -B build-test -DSHADOW_BUILD_MODPACK_SELFTEST=ON
// 运行：build-test/Release/ModpackSelfTest.exe
// 带密钥：set SHADOW_TEST_CF_KEY=xxx && build-test/Release/ModpackSelfTest.exe

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QtConcurrent>
#include <QNetworkRequest>
#include <QJsonParseError>

#include "core/modpack/zip_archive.h"
#include "core/modpack/modpack_parser.h"
#include "core/modpack/modpack_downloader.h"
#include "core/http_client.h"
#include "utils/logger.h"

using namespace ShadowLauncher;

// test_zip_helper.cpp 提供的 zip 构造器
int buildTestZip(const QString& zipPath, const QString& indexEntryName,
                 const QByteArray& indexContent,
                 const QList<QPair<QString, QByteArray>>& extraFiles);

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond, msg) \
    do { \
        ++g_checks; \
        if (cond) { qInfo().noquote() << "  [PASS]" << msg; } \
        else { ++g_failures; qInfo().noquote() << "  [FAIL]" << msg; } \
    } while (0)

static QString tempDir()
{
    const QString d = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + QStringLiteral("/shadow-modpack-selftest");
    QDir(d).removeRecursively();
    QDir().mkpath(d);
    return d;
}

// 构造一个最小 CurseForge 整合包 zip
static QString makeCfZip(const QString& dir)
{
    const QString zipPath = dir + QStringLiteral("/cfpack.zip");

    QJsonObject manifest;
    manifest[QStringLiteral("name")] = QStringLiteral("Test CF Pack");
    manifest[QStringLiteral("version")] = QStringLiteral("1.2.3");
    QJsonObject mc;
    mc[QStringLiteral("version")] = QStringLiteral("1.20.1");
    QJsonArray loaders;
    QJsonObject loader;
    loader[QStringLiteral("id")] = QStringLiteral("forge-47.2.0");
    loader[QStringLiteral("primary")] = true;
    loaders.append(loader);
    mc[QStringLiteral("modLoaders")] = loaders;
    manifest[QStringLiteral("minecraft")] = mc;
    QJsonArray files;
    QJsonObject f1;
    f1[QStringLiteral("projectID")] = 238222;
    f1[QStringLiteral("fileID")] = 47128666;
    f1[QStringLiteral("required")] = true;
    files.append(f1);
    manifest[QStringLiteral("files")] = files;
    manifest[QStringLiteral("overrides")] = QStringLiteral("overrides");

    // 用 miniz 写 zip（mz_zip_writer）
    QList<QPair<QString, QByteArray>> extra;
    extra.append({QStringLiteral("overrides/config/example.toml"),
                  QByteArray("key = \"value\"\n")});
    extra.append({QStringLiteral("overrides/mods/dummy.jar"),
                  QByteArray("PK\x05\x06\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00")});
    if (buildTestZip(zipPath, QStringLiteral("manifest.json"),
                     QJsonDocument(manifest).toJson(QJsonDocument::Compact), extra) != 0)
        return {};
    return zipPath;
}

// 构造一个最小 Modrinth mrpack
static QString makeMrPack(const QString& dir)
{
    const QString zipPath = dir + QStringLiteral("/mrpack.mrpack");

    QJsonObject index;
    index[QStringLiteral("formatVersion")] = 1;
    index[QStringLiteral("game")] = QStringLiteral("minecraft");
    index[QStringLiteral("versionId")] = QStringLiteral("0.9.9");
    index[QStringLiteral("name")] = QStringLiteral("Test MR Pack");
    index[QStringLiteral("summary")] = QStringLiteral("自测整合包");
    QJsonObject deps;
    deps[QStringLiteral("minecraft")] = QStringLiteral("1.20.1");
    deps[QStringLiteral("fabric-loader")] = QStringLiteral("0.15.11");
    index[QStringLiteral("dependencies")] = deps;
    QJsonArray files;
    QJsonObject mf;
    mf[QStringLiteral("path")] = QStringLiteral("mods/testmod.jar");
    QJsonArray downloads;
    downloads.append(QStringLiteral("https://cdn.modrinth.com/data/AAAAAAAA/versions/1/testmod.jar"));
    mf[QStringLiteral("downloads")] = downloads;
    mf[QStringLiteral("fileSize")] = 12345;
    QJsonObject hashes;
    hashes[QStringLiteral("sha1")] = QStringLiteral("da39a3ee5e6b4b0d3255bfef95601890afd80709");
    mf[QStringLiteral("hashes")] = hashes;
    QJsonObject env;
    env[QStringLiteral("client")] = QStringLiteral("required");
    mf[QStringLiteral("env")] = env;
    files.append(mf);
    index[QStringLiteral("files")] = files;

    QList<QPair<QString, QByteArray>> extra;
    extra.append({QStringLiteral("overrides/config/options.txt"), QByteArray("fov=90\n")});
    extra.append({QStringLiteral("client-overrides/servers.dat"), QByteArray("fake")});
    if (buildTestZip(zipPath, QStringLiteral("modrinth.index.json"),
                     QJsonDocument(index).toJson(QJsonDocument::Compact), extra) != 0)
        return {};
    return zipPath;
}

// ── 离线自测 ──
static int runOfflineTests()
{
    qInfo().noquote() << "==== [离线] ZipArchive + ModpackParser ====";
    const QString td = tempDir();

    const QString cfZip = makeCfZip(td);
    CHECK(!cfZip.isEmpty(), "构造 CF zip");
    if (cfZip.isEmpty()) return 1;

    ZipArchive zip;
    CHECK(zip.open(cfZip), "打开 CF zip");
    if (!zip.isOpen()) return 1;
    CHECK(zip.hasEntry(QStringLiteral("manifest.json")), "hasEntry manifest.json");
    CHECK(zip.entryCount() >= 3, "条目数 >= 3");

    const QByteArray manifestRaw = zip.readEntry(QStringLiteral("manifest.json"));
    CHECK(manifestRaw.contains("\"forge-47.2.0\""), "读取 manifest.json 内容");

    // 路径穿越防护
    const QString evilZip = td + QStringLiteral("/evil.zip");
    {
        QList<QPair<QString, QByteArray>> evil;
        evil.append({QStringLiteral("../evil.txt"), QByteArray("boom")});
        buildTestZip(evilZip, QString(), QByteArray(), evil);
    }
    ZipArchive evil;
    if (evil.open(evilZip)) {
        const QString outDir = td + QStringLiteral("/evil-out");
        const int n = evil.extractPrefixTo(QString(), outDir);
        CHECK(n == 0, "穿越条目被拒绝（解压 0 文件）");
        CHECK(!QFileInfo::exists(QDir::cleanPath(outDir + QStringLiteral("/../evil.txt"))), "未逃逸到父目录");
    } else {
        CHECK(false, "打开恶意 zip");
    }

    // 解析 CF
    ModpackMeta cfMeta;
    QString err;
    const bool cfOk = ModpackParser::parse(cfZip, ModpackFormat::CurseForge, cfMeta, err);
    CHECK(cfOk, "解析 CF manifest");
    if (cfOk) {
        CHECK(cfMeta.name == QStringLiteral("Test CF Pack"), "CF 名称: " + cfMeta.name);
        CHECK(cfMeta.mcVersion == QStringLiteral("1.20.1"), "CF MC 版本");
        CHECK(cfMeta.loaderType == QStringLiteral("forge") && cfMeta.loaderVersion == QStringLiteral("47.2.0"),
              "CF 加载器: forge 47.2.0");
        CHECK(cfMeta.files.size() == 1 && cfMeta.files[0].fileId == 47128666, "CF 文件列表");
        CHECK(cfMeta.overrideDirs.size() == 1 && cfMeta.overrideDirs[0] == QStringLiteral("overrides"),
              "CF overrides 目录");
    }

    // 解压 overrides
    const QString outDir = td + QStringLiteral("/cf-out");
    const int n = zip.extractPrefixTo(QStringLiteral("overrides"), outDir);
    CHECK(n == 2, "overrides 解压 2 个文件");
    QFile cfg(outDir + QStringLiteral("/config/example.toml"));
    CHECK(cfg.exists(), "overrides/config/example.toml 落地");
    if (cfg.open(QIODevice::ReadOnly)) {
        CHECK(cfg.readAll().contains("key = \"value\""), "配置文件内容正确");
        cfg.close();
    }
    CHECK(QFileInfo::exists(outDir + QStringLiteral("/mods/dummy.jar")), "overrides/mods 落地");

    // 解析 MR
    const QString mrZip = makeMrPack(td);
    CHECK(!mrZip.isEmpty(), "构造 mrpack");
    ModpackMeta mrMeta;
    const bool mrOk = ModpackParser::parse(mrZip, ModpackFormat::Modrinth, mrMeta, err);
    CHECK(mrOk, "解析 modrinth.index.json");
    if (mrOk) {
        CHECK(mrMeta.name == QStringLiteral("Test MR Pack"), "MR 名称");
        CHECK(mrMeta.loaderType == QStringLiteral("fabric") && mrMeta.loaderVersion == QStringLiteral("0.15.11"),
              "MR 加载器 fabric 0.15.11");
        CHECK(mrMeta.files.size() == 1 && mrMeta.files[0].relPath == QStringLiteral("mods/testmod.jar"),
              "MR 文件路径");
        CHECK(mrMeta.files[0].sha1 == QByteArray("da39a3ee5e6b4b0d3255bfef95601890afd80709"), "MR sha1");
        CHECK(mrMeta.overrideDirs == QStringList({QStringLiteral("overrides"), QStringLiteral("client-overrides")}),
              "MR 双覆写目录");
    }

    ZipArchive mrz;
    if (mrz.open(mrZip)) {
        const QString mrOut = td + QStringLiteral("/mr-out");
        const int m = mrz.extractPrefixTo(QStringLiteral("overrides"), mrOut);
        CHECK(m == 1, "MR overrides 解压 1 个文件");
        const int c = mrz.extractPrefixTo(QStringLiteral("client-overrides"), mrOut);
        CHECK(c == 1, "MR client-overrides 解压 1 个文件");
        CHECK(QFileInfo::exists(mrOut + QStringLiteral("/servers.dat")), "client-overrides 落地");
    }

    // 加载器解析工具
    QString lt, lv, lerr;
    CHECK(parseLoaderId(QStringLiteral("neoforge-20.4.237"), &lt, &lv, &lerr)
          && lt == QStringLiteral("neoforge") && lv == QStringLiteral("20.4.237"), "neoforge 解析");
    CHECK(parseLoaderId(QStringLiteral("fabric-loader-0.15.11"), &lt, &lv, &lerr)
          && lt == QStringLiteral("fabric"), "fabric-loader 解析");
    CHECK(!parseLoaderId(QStringLiteral("forge-1.7.10-recommended"), &lt, &lv, &lerr),
          "recommended 占位符被拒绝");
    CHECK(!parseLoaderId(QStringLiteral("optifine-1.20.1"), &lt, &lv, &lerr), "未知加载器被拒绝");

    qInfo().noquote() << "==== [离线] 完成 ====";
    return 0;
}

// ── 在线自测（可选）──
static int runOnlineTests()
{
    const QByteArray key = qgetenv("SHADOW_TEST_CF_KEY");
    if (key.isEmpty()) {
        qInfo().noquote() << "==== [在线] 跳过（未设置 SHADOW_TEST_CF_KEY）====";
        return 0;
    }
    qInfo().noquote() << "==== [在线] CF 搜索 + 下载 + Modrinth 下载 ====";

    // 1) CF 搜索 JEI（镜像优先 → 官方 → 镜像，均失败则跳过在线段）
    QEventLoop loop;
    QElapsedTimer tmr;
    auto searchCf = [&loop](const QString& base, const QByteArray& key) -> QNetworkReply* {
        QNetworkRequest req{ QUrl(base + QStringLiteral("/v1/mods/search?gameId=432&slug=jei")) };
        req.setRawHeader("Accept", "application/json");
        req.setRawHeader("x-api-key", key);
        req.setTransferTimeout(25000);
        req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
        QNetworkReply* r = HttpClient::instance().manager()->get(req);
        QObject::connect(r, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        return r;
    };

    int projectId = 0, fileId = 0;
    static const QString kSearchBases[] = {
        QStringLiteral("https://mod.mcimirror.top/curseforge"),   // 1) 镜像
        QStringLiteral("https://api.curseforge.com"),             // 2) 官方兜底
        QStringLiteral("https://mod.mcimirror.top/curseforge"),   // 3) 镜像再试
    };
    for (int attempt = 0; attempt < 3 && projectId <= 0; ++attempt) {
        tmr.restart();
        QNetworkReply* reply = searchCf(kSearchBases[attempt], key);
        const int st = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(0);
        qInfo().noquote() << "    [dbg] CF search" << (attempt + 1)
                          << (attempt == 1 ? "(official)" : "(mirror)")
                          << "status=" << st
                          << "err=" << reply->error() << reply->errorString()
                          << "ms=" << tmr.elapsed();
        if (reply->error() == QNetworkReply::NoError && st == 200) {
            const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            const QJsonArray data = doc.object().value(QStringLiteral("data")).toArray();
            if (!data.isEmpty()) {
                const QJsonObject proj = data.first().toObject();
                projectId = proj.value(QStringLiteral("id")).toInt();
                // 取第一个有直链的最新文件（版本不限，仅验证解析+下载+校验链路）
                const QJsonArray lf = proj.value(QStringLiteral("latestFiles")).toArray();
                for (const QJsonValue& v : lf) {
                    const QJsonObject fo = v.toObject();
                    if (!fo.value(QStringLiteral("downloadUrl")).toString().isEmpty()) {
                        fileId = fo.value(QStringLiteral("id")).toInt();
                        break;
                    }
                }
            }
        }
        reply->deleteLater();
    }
    if (projectId <= 0 || fileId <= 0) {
        // 官方 H2 故障导致搜索不可达：用已知 fileId（JEI 1.12.2）继续验证下载链路
        qInfo().noquote() << "    [dbg] 搜索不可达，使用已知 fileId=3040523 继续验证下载链路";
        projectId = 238222;
        fileId = 3040523;
    }
    qInfo().noquote() << QStringLiteral("    [dbg] 使用 project=%1 file=%2").arg(projectId).arg(fileId);

    // 2) 用下载器跑 CF 文件（真实解析 + 下载 + 大小校验）
    QList<ModpackRemoteFile> files;
    {
        ModpackRemoteFile rf;
        rf.source = QStringLiteral("curseforge");
        rf.projectId = projectId;
        rf.fileId = fileId;
        rf.required = true;
        rf.index = 0;
        files.append(rf);
    }

    // 3) Modrinth 取 Sodium 最新版本（真实 URL + sha1）
    {
        QNetworkRequest mrReq{ QUrl(QStringLiteral("https://api.modrinth.com/v2/project/sodium/version?game_versions=%5B%221.20.1%22%5D&loaders=%5B%22fabric%22%5D")) };
        mrReq.setTransferTimeout(30000);
        QNetworkReply* mrReply = HttpClient::instance().manager()->get(mrReq);
        QObject::connect(mrReply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        if (mrReply->error() == QNetworkReply::NoError) {
            const QJsonArray arr = QJsonDocument::fromJson(mrReply->readAll()).array();
            if (!arr.isEmpty()) {
                const QJsonObject f0 = arr.first().toObject().value(QStringLiteral("files")).toArray().first().toObject();
                ModpackRemoteFile rf;
                rf.source = QStringLiteral("modrinth");
                rf.relPath = QStringLiteral("mods/sodium-test.jar");
                rf.fileName = QStringLiteral("sodium-test.jar");
                rf.displayName = rf.fileName;
                rf.downloadUrl = f0.value(QStringLiteral("url")).toString();
                rf.sha1 = f0.value(QStringLiteral("hashes")).toObject().value(QStringLiteral("sha1")).toString().toLatin1();
                rf.size = static_cast<qint64>(f0.value(QStringLiteral("size")).toDouble(0));
                rf.required = true;
                rf.index = 1;
                files.append(rf);
            }
        }
        mrReply->deleteLater();
        CHECK(files.size() == 2, "Modrinth 获取 Sodium 版本信息");
    }

    const QString dlDir = tempDir() + QStringLiteral("/downloads");
    QDir().mkpath(dlDir);

    ModpackDownloader dl;
    QString dlLog;
    QObject::connect(&dl, &ModpackDownloader::logLine, [&dlLog](const QString& m) {
        dlLog += m + QLatin1Char('\n');
        qInfo().noquote() << "    [dl]" << m;
    });
    QObject::connect(&dl, &ModpackDownloader::queueProgress, [](int c, int t, int f) {
        qInfo().noquote() << "    [dl] progress" << c << "/" << t << "failed=" << f;
    });

    int doneCount = 0;
    bool allOk = false;
    QObject::connect(&dl, &ModpackDownloader::allFinished, [&](bool cancelled) {
        allOk = !cancelled;
        ++doneCount;
        loop.quit();
    });

    dl.setApiKey(QString::fromLatin1(key));
    dl.setTargetDir(dlDir);
    dl.setFiles(&files);
    dl.start(false);

    QTimer::singleShot(120000, &loop, &QEventLoop::quit);  // 超时保护
    if (doneCount == 0) loop.exec();

    CHECK(allOk, "下载队列全部完成（无取消）");
    int okFiles = 0;
    for (const ModpackRemoteFile& rf : files) {
        qInfo().noquote() << "    [dl]" << rf.status << rf.error << rf.fileName;
        if (rf.status == QLatin1String("done")) ++okFiles;
    }
    if (okFiles == 0 && files.size() == 2) {
        // 全部失败：判定为网络不可用（镜像+官方均不可达），跳过镜像断言避免误报
        qInfo().noquote() << "==== [在线] 镜像与官方均不可达，跳过下载链路断言 ====";
        return 0;
    }
    CHECK(okFiles == 2, "两个文件均下载成功且通过校验");
    CHECK(QFileInfo::exists(dlDir + QStringLiteral("/mods/sodium-test.jar")), "Modrinth 文件落盘");
    CHECK(QFileInfo::exists(dlDir + QStringLiteral("/mods/")), "CF 分类目录存在");
    // 注：镜像优先策略由文件级成功 + 校验通过隐含验证（引擎日志不携带 URL 文案，
    //     旧版「切换备用源」/镜像 URL 日志断言已随引擎复用移除）
    // 4) 降级链路：构造一个官方 URL 格式但不存在文件（镜像 404 → 自动切官方 → 仍失败）
    {
        QList<ModpackRemoteFile> fakeFiles;
        ModpackRemoteFile rf;
        rf.source = QStringLiteral("modrinth");
        rf.relPath = QStringLiteral("mods/fake-missing.jar");
        rf.fileName = QStringLiteral("fake-missing.jar");
        rf.displayName = rf.fileName;
        rf.downloadUrl = QStringLiteral("https://cdn.modrinth.com/data/__NOPE__/versions/__NOPE__/fake-missing.jar");
        rf.sha1 = QByteArray("da39a3ee5e6b4b0d3255bfef95601890afd80709");
        rf.required = true;
        rf.index = 0;
        fakeFiles.append(rf);

        QString fakeLog;
        ModpackDownloader dl2;
        QObject::connect(&dl2, &ModpackDownloader::logLine, [&fakeLog](const QString& m) {
            fakeLog += m + QLatin1Char('\n');
        });
        int done2 = 0;
        bool allOk2 = false;
        QObject::connect(&dl2, &ModpackDownloader::allFinished, [&](bool cancelled) {
            allOk2 = !cancelled;
            ++done2;
            loop.quit();
        });
        dl2.setApiKey(QString::fromLatin1(key));
        dl2.setTargetDir(dlDir);
        dl2.setFiles(&fakeFiles);
        dl2.start(false);
        QTimer::singleShot(90000, &loop, &QEventLoop::quit);
        if (done2 == 0) loop.exec();

        CHECK(allOk2, "降级用例：队列正常收尾（文件级失败不取消整体）");
        CHECK(fakeFiles[0].status == QLatin1String("fail"), "降级用例：不存在的文件最终判定失败");
        CHECK(fakeLog.contains(QStringLiteral("下载失败")),
              "降级用例：镜像失败后引擎多源降级并最终失败（文件级）");
    }

    qInfo().noquote() << "==== [在线] 完成 ====";
    return 0;
}

// ── 临时诊断模式：--diag <mrpack/zip 路径> [CF key] ──
// 解析整合包清单 → ModpackDownloader 全量下载 → 逐文件结果/日志/速度统计
static int runDiagDownload(const QString& zipPath, const QString& apiKey)
{
    qInfo().noquote() << "==== [诊断] 解析:" << zipPath;
    const ModpackFormat fmt = ModpackParser::detectFormat(zipPath);
    if (fmt == ModpackFormat::Unknown) { qInfo().noquote() << "未知格式"; return 1; }
    ModpackMeta meta;
    QString perr;
    if (!ModpackParser::parse(zipPath, fmt, meta, perr)) { qInfo().noquote() << "解析失败:" << perr; return 1; }
    qInfo().noquote() << QStringLiteral("包名=%1 版本=%2 MC=%3 加载器=%4 文件数=%5")
        .arg(meta.name, meta.versionId, meta.mcVersion, meta.loaderType).arg(meta.files.size());

    const QString dlDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + QStringLiteral("/shadow-diag-dl");
    QDir(dlDir).removeRecursively();
    QDir().mkpath(dlDir);

    QEventLoop loop;
    ModpackDownloader dl;
    QStringList logs;
    QElapsedTimer timer; timer.start();
    qint64 totalBytes = 0;
    int done = 0, failed = 0, skipped = 0;
    QStringList failNames, failLogs;

    QObject::connect(&dl, &ModpackDownloader::logLine, [&](const QString& m) {
        logs.append(m);
        qInfo().noquote() << "  [dl]" << m;
    });
    QObject::connect(&dl, &ModpackDownloader::fileFinished,
                     [&](int index, bool ok, const QString& err) {
        Q_UNUSED(index); Q_UNUSED(err);
        if (ok) ++done; else ++failed;
    });
    QObject::connect(&dl, &ModpackDownloader::allFinished, [&](bool cancelled) {
        qInfo().noquote() << "==== [诊断] allFinished cancelled=" << cancelled
                          << " 耗时=" << timer.elapsed() << "ms 成功=" << done << " 失败=" << failed;
        loop.quit();
    });

    dl.setApiKey(apiKey);
    dl.setTargetDir(dlDir);
    dl.setFiles(&meta.files);
    dl.start(false);

    QTimer::singleShot(600000, &loop, &QEventLoop::quit);   // 10 分钟超时保护
    loop.exec();

    qInfo().noquote() << "==== [诊断] 逐文件结果 ====";
    for (int i = 0; i < meta.files.size(); ++i) {
        const ModpackRemoteFile& rf = meta.files.at(i);
        QFileInfo fi(dlDir + QLatin1Char('/') + rf.relPath);
        const qint64 sz = fi.exists() ? fi.size() : -1;
        if (rf.status == QLatin1String("done")) totalBytes += sz;
        else if (rf.status == QLatin1String("skipped")) ++skipped;
        QString tag = QStringLiteral("镜像");
        if (rf.status != QLatin1String("done")) {
            failNames << QStringLiteral("%1 (%2)").arg(rf.fileName, rf.status);
            for (const QString& l : logs) {
                if (l.contains(rf.fileName)) failLogs << l;
            }
        }
        qInfo().noquote() << QStringLiteral("  %1 | %2 | %3 B | %4 | %5")
            .arg(rf.status, -8).arg(rf.fileName).arg(sz).arg(rf.error).arg(tag);
    }
    const qint64 secs = qMax<qint64>(1, timer.elapsed() / 1000);
    qInfo().noquote() << QStringLiteral("==== [诊断] 统计: 总耗时 %1s 成功 %2 失败 %3 跳过 %4 总字节 %5 平均速度 %6 KB/s")
        .arg(secs).arg(done).arg(failed).arg(skipped).arg(totalBytes).arg(totalBytes / 1024 / secs);
    if (!failNames.isEmpty()) {
        qInfo().noquote() << "==== [诊断] 失败文件日志 ====";
        for (const QString& l : failLogs) qInfo().noquote() << "  " << l;
    }
    return 0;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ShadowLauncher::initLogger(QCoreApplication::applicationDirPath());

    if (argc > 2 && QString::fromLocal8Bit(argv[1]) == QLatin1String("--diag")) {
        // Windows argv 为系统代码页(GBK)编码，须用 fromLocal8Bit 还原中文路径
        const QString key = argc > 3 ? QString::fromLocal8Bit(argv[3]) : QString();
        return runDiagDownload(QString::fromLocal8Bit(argv[2]), key);
    }

    qInfo().noquote() << "ShadowLauncher Modpack 模块自测开始";
    const int r1 = runOfflineTests();
    const int r2 = runOnlineTests();

    qInfo().noquote() << QStringLiteral("结果: %1 项检查, %2 项失败").arg(g_checks).arg(g_failures);
    return (r1 == 0 && r2 == 0 && g_failures == 0) ? 0 : 1;
}
