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
        else { ++g_failures; qWarning().noquote() << "  [FAIL]" << msg; } \
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

    // 1) CF 搜索 JEI 1.20.1 最新文件（请求属性超时优先于 manager 默认 10s）
    QNetworkRequest searchReq{ QUrl(QStringLiteral("https://api.curseforge.com/v1/mods/search?gameId=432&slug=jei")) };
    searchReq.setRawHeader("Accept", "application/json");
    searchReq.setRawHeader("x-api-key", key);
    searchReq.setTransferTimeout(40000);
    QNetworkReply* searchReply = HttpClient::instance().manager()->get(searchReq);

    QEventLoop loop;
    QElapsedTimer tmr; tmr.start();
    QObject::connect(searchReply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    int projectId = 0, fileId = 0;
    const int httpStatus = searchReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(0);
    qInfo().noquote() << "    [dbg] CF search1 status=" << httpStatus
                      << "err=" << searchReply->error() << searchReply->errorString()
                      << "ms=" << tmr.elapsed()
                      << "body=" << QString::fromUtf8(searchReply->readAll().left(160));
    if (searchReply->error() == QNetworkReply::NoError) {
        const QJsonDocument doc = QJsonDocument::fromJson(searchReply->readAll());
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
    searchReply->deleteLater();

    // 重试一次（诊断：首次请求是否因某种 warm-up 慢）
    if (projectId <= 0) {
        QNetworkRequest retryReq{ QUrl(QStringLiteral("https://api.curseforge.com/v1/mods/search?gameId=432&slug=jei")) };
        retryReq.setRawHeader("Accept", "application/json");
        retryReq.setRawHeader("x-api-key", key);
        retryReq.setTransferTimeout(40000);
        QNetworkReply* r2 = HttpClient::instance().manager()->get(retryReq);
        tmr.restart();
        QObject::connect(r2, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        const int st2 = r2->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(0);
        qInfo().noquote() << "    [dbg] CF search2 status=" << st2
                          << "err=" << r2->error() << r2->errorString()
                          << "ms=" << tmr.elapsed();
        if (r2->error() == QNetworkReply::NoError) {
            const QJsonDocument doc2 = QJsonDocument::fromJson(r2->readAll());
            const QJsonArray data2 = doc2.object().value(QStringLiteral("data")).toArray();
            if (!data2.isEmpty()) {
                projectId = data2.first().toObject().value(QStringLiteral("id")).toInt();
                const QJsonArray lf2 = data2.first().toObject().value(QStringLiteral("latestFiles")).toArray();
                for (const QJsonValue& v : lf2) {
                    const QJsonObject fo = v.toObject();
                    if (!fo.value(QStringLiteral("downloadUrl")).toString().isEmpty()) {
                        fileId = fo.value(QStringLiteral("id")).toInt();
                        break;
                    }
                }
            }
        }
        r2->deleteLater();
    }
    CHECK(projectId > 0 && fileId > 0, QStringLiteral("CF 搜索 JEI: project=%1 file=%2").arg(projectId).arg(fileId));

    if (projectId <= 0 || fileId <= 0) return 1;

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
    QObject::connect(&dl, &ModpackDownloader::logLine, [](const QString& m) {
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
    CHECK(okFiles == 2, "两个文件均下载成功且通过校验");
    CHECK(QFileInfo::exists(dlDir + QStringLiteral("/mods/sodium-test.jar")), "Modrinth 文件落盘");
    CHECK(QFileInfo::exists(dlDir + QStringLiteral("/mods/")), "CF 分类目录存在");

    qInfo().noquote() << "==== [在线] 完成 ====";
    return 0;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ShadowLauncher::initLogger(QCoreApplication::applicationDirPath());

    qInfo().noquote() << "ShadowLauncher Modpack 模块自测开始";
    const int r1 = runOfflineTests();
    const int r2 = runOnlineTests();

    qInfo().noquote() << QStringLiteral("结果: %1 项检查, %2 项失败").arg(g_checks).arg(g_failures);
    return (r1 == 0 && r2 == 0 && g_failures == 0) ? 0 : 1;
}
