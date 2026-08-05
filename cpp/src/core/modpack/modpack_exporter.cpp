// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
//
// modpack_exporter.cpp — 整合包导出（完全对齐主流启动器实现 PageInstanceExport）。
//
// 全部磁盘 IO / 哈希 / 网络查询 / 压缩在 QtConcurrent worker 线程执行，
// 进度与结果经 invokeMethod 回主线程（跨线程写成员有数据竞争，必须回投）。
// ZipArchive 单工作线程串行调用（miniz 内部持 FILE* 状态）。

#include "modpack_exporter.h"

#include "zip_archive.h"
#include "../../utils/logger.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QUrl>
#include <QtConcurrent>

#include <memory>

namespace ShadowLauncher {

ModpackExporter::ModpackExporter(QObject* parent) : QObject(parent) {}

void ModpackExporter::setGameDir(const QString& dir)
{
    m_gameDir = dir;
    // CF API Key 自动加载（与 ModpackImporter 同款）：环境变量 → {gameDir}/config/cf_api_key.json
    if (m_cfApiKey.isEmpty()) {
        QString key = qEnvironmentVariable("SHADOW_CF_API_KEY");
        if (!key.isEmpty()) {
            m_cfApiKey = key;
            return;
        }
        if (!dir.isEmpty()) {
            const QString path = dir + QStringLiteral("/config/cf_api_key.json");
            QFile f(path);
            if (f.exists() && f.open(QIODevice::ReadOnly)) {
                const QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
                m_cfApiKey = obj.value(QStringLiteral("apiKey")).toString();
            }
        }
    }
}

void ModpackExporter::setProgress(qreal p, const QString& text)
{
    // 后台线程调用 → 回主线程更新（跨线程写成员有数据竞争）
    QMetaObject::invokeMethod(this, [this, p, text]() {
        m_progress = p;
        m_statusText = text;
        emit progressChanged();
    });
}

void ModpackExporter::cancel()
{
    m_cancel.storeRelaxed(1);
}

QStringList ModpackExporter::listSaves(const QString& versionId) const
{
    Q_UNUSED(versionId)
    QStringList out;
    const QDir savesDir(m_gameDir + QStringLiteral("/saves"));
    if (savesDir.exists()) {
        const auto entries = savesDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const auto& e : entries)
            out.append(e);
    }
    return out;
}

// ═════════════════════════════════════════════════════════════════════════════
// CurseForge fingerprint：MurmurHash2 32 位，种子 1，计算前剔除 \t\n\r 空格
// （主流启动器 LocalResourceFile.CurseForgeHash 同款，CF 官方指纹算法）
// ═════════════════════════════════════════════════════════════════════════════

static quint32 cfMurmurHash2(const QByteArray& raw)
{
    QByteArray d;
    d.reserve(raw.size());
    for (char b : raw) {
        if (b == 9 || b == 10 || b == 13 || b == 32) continue;
        d.append(b);
    }
    const int len = d.size();
    const auto* p = reinterpret_cast<const unsigned char*>(d.constData());
    quint32 h = 1u ^ static_cast<quint32>(len);   // seed = 1
    const quint32 m = 0x5BD1E995u;
    int i = 0;
    while (i + 4 <= len) {
        quint32 k = p[i] | (p[i + 1] << 8) | (p[i + 2] << 16) | (p[i + 3] << 24);
        i += 4;
        k *= m; k ^= k >> 24; k *= m;
        h *= m; h ^= k;
    }
    switch (len - i) {
    case 3: h ^= p[i] | (p[i + 1] << 8); h ^= static_cast<quint32>(p[i + 2]) << 16; h *= m; break;
    case 2: h ^= p[i] | (p[i + 1] << 8); h *= m; break;
    case 1: h ^= p[i]; h *= m; break;
    }
    h ^= h >> 13; h *= m; h ^= h >> 15;
    return h;
}

// CF downloadUrl 域名变体展开（主流启动器 ResourceVersion.ParseCurseForgeDownloadUrls 同款）
static QStringList expandCfDownloadUrls(const QString& url)
{
    QStringList out;
    QString v1 = url;
    out << v1.replace(QStringLiteral("-service.overwolf.wtf"), QStringLiteral(".forgecdn.net"))
              .replace(QStringLiteral("://edge."), QStringLiteral("://mediafilez."))
              .replace(QStringLiteral("://media."), QStringLiteral("://mediafilez."));
    QString v2 = url;
    out << v2.replace(QStringLiteral("://edge."), QStringLiteral("://mediafilez."))
              .replace(QStringLiteral("://media."), QStringLiteral("://mediafilez."));
    QString v3 = url;
    out << v3.replace(QStringLiteral("-service.overwolf.wtf"), QStringLiteral(".forgecdn.net"));
    QString v4 = url;
    out << v4.replace(QStringLiteral("://media."), QStringLiteral("://edge."));
    out << url;
    out.removeDuplicates();
    return out;
}

// 同步 POST（worker 线程内 QNAM + QEventLoop，超时 20s），成功返回响应体
static QByteArray postJson(const QUrl& url, const QByteArray& body,
                           const QHash<QByteArray, QByteArray>& headers = {})
{
    QNetworkAccessManager nam;
    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "ShadowLauncher/1.0");
    req.setRawHeader("Content-Type", "application/json");
    for (auto it = headers.begin(); it != headers.end(); ++it)
        req.setRawHeader(it.key(), it.value());
    req.setTransferTimeout(20000);

    QNetworkReply* reply = nam.post(req, body);
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    bool timedOut = false;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, [&]() { timedOut = true; loop.quit(); });
    timeout.start(20000);
    loop.exec();

    // 用 timedOut 标志判断：旧逻辑 `!timeout.isActive()` 在正常完成时恒 false → 误判失败
    const bool ok = !timedOut && (reply->error() == QNetworkReply::NoError);
    const QByteArray data = ok ? reply->readAll() : QByteArray();
    reply->deleteLater();
    return data;
}

// ═════════════════════════════════════════════════════════════════════════════
// 导出主流程
// ═════════════════════════════════════════════════════════════════════════════

void ModpackExporter::exportVersion(const QString& versionId, const QString& displayName,
                                    const QString& packVersion, bool includeConfig,
                                    const QVariantList& selectedSaves,
                                    bool includeResourcepacks, bool includeShaderpacks,
                                    bool modrinthUploadMode, int format,
                                    const QString& outPath)
{
    if (m_busy) return;
    if (versionId.isEmpty() || outPath.isEmpty()) {
        emit finished(false, outPath, tr("参数不完整"));
        return;
    }
    m_busy = true;
    m_cancel.storeRelaxed(0);
    setProgress(0.0, tr("准备导出..."));
    emit busyChanged();

    const QString gameDir = m_gameDir;
    const QString cfKey = m_cfApiKey;
    const bool cfFormat = (format == 1);

    QtConcurrent::run([this, gameDir, cfKey, cfFormat,
                       versionId, displayName, packVersion,
                       includeConfig, selectedSaves,
                       includeResourcepacks, includeShaderpacks,
                       modrinthUploadMode, outPath]() {
        auto finish = [this, outPath](bool ok, const QString& err) {
            const QString out = outPath;
            QMetaObject::invokeMethod(this, [this, ok, out, err]() {
                m_busy = false;
                emit busyChanged();
                emit finished(ok, out, err);
            });
        };

        // ── 1. 解析版本 JSON：MC 版本 + 加载器依赖 ──
        const QString versionDir = gameDir + QStringLiteral("/versions/") + versionId;
        const QString jsonPath = versionDir + QStringLiteral("/") + versionId + QStringLiteral(".json");
        if (!QFileInfo::exists(jsonPath)) {
            finish(false, tr("版本 %1 不存在或已损坏").arg(versionId));
            return;
        }
        QString mcVersion;
        QMap<QString, QString> deps;
        {
            QFile f(jsonPath);
            if (f.open(QIODevice::ReadOnly)) {
                const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
                mcVersion = root.value(QStringLiteral("id")).toString();
                const QJsonArray libs = root.value(QStringLiteral("libraries")).toArray();
                for (const auto& lv : libs) {
                    const QString name = lv.toObject().value(QStringLiteral("name")).toString();
                    const int lastColon = name.lastIndexOf(QLatin1Char(':'));
                    if (lastColon <= 0) continue;
                    const QString ver = name.mid(lastColon + 1);
                    if (name.startsWith(QStringLiteral("net.minecraftforge:forge:")))
                        deps.insert(QStringLiteral("forge"), ver);
                    else if (name.contains(QStringLiteral("fabric-loader")))
                        deps.insert(QStringLiteral("fabric-loader"), ver);
                    else if (name.startsWith(QStringLiteral("net.neoforged:neoforge:")))
                        deps.insert(QStringLiteral("neoforge"), ver);
                    else if (name.contains(QStringLiteral("quilt-loader")))
                        deps.insert(QStringLiteral("quilt-loader"), ver);
                }
            }
        }
        if (mcVersion.isEmpty()) {
            finish(false, tr("无法解析版本 %1 的元数据").arg(versionId));
            return;
        }

        // ── 2. 收集 mods（排除 .disabled）──
        struct ModFile {
            QString diskPath;
            QString relPath;
            QByteArray sha1;
            QByteArray sha512;
            quint32 cfHash = 0;
            qint64 size = 0;
            QStringList downloads;    // 在线下载 URL 列表
            int cfProjectId = 0;
            int cfFileId = 0;
            bool hosted = false;      // 找到至少一个在线来源
        };
        QList<ModFile> mods;
        QDir modsDir(gameDir + QStringLiteral("/mods"));
        if (modsDir.exists()) {
            QDirIterator it(modsDir.absolutePath(), QStringList() << QStringLiteral("*.jar"),
                            QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString p = it.next();
                if (p.endsWith(QStringLiteral(".disabled"), Qt::CaseInsensitive)) continue;
                ModFile mf;
                mf.diskPath = p;
                mf.relPath = modsDir.relativeFilePath(p);
                mf.size = QFileInfo(p).size();
                mods.append(mf);
            }
        }

        // ── 3. overrides 文件清单（saves 按勾选子项）──
        struct OvFile { QString diskPath; QString relPath; };
        QList<OvFile> ovFiles;
        auto addDir = [&](const QString& sub) {
            const QString d = gameDir + QStringLiteral("/") + sub;
            if (!QDir(d).exists()) return;
            QDirIterator it(d, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString p = it.next();
                ovFiles.append({p, QDir(gameDir).relativeFilePath(p)});
            }
        };
        auto addFile = [&](const QString& name) {
            const QString p = gameDir + QStringLiteral("/") + name;
            if (QFileInfo::exists(p)) ovFiles.append({p, name});
        };
        if (includeConfig) addDir(QStringLiteral("config"));
        addFile(QStringLiteral("options.txt"));
        addFile(QStringLiteral("servers.dat"));
        if (includeResourcepacks) addDir(QStringLiteral("resourcepacks"));
        if (includeShaderpacks)   addDir(QStringLiteral("shaderpacks"));
        {
            // 勾选的存档（主流启动器 按存档子项）
            QStringList wantSaves;
            for (const auto& v : selectedSaves)
                wantSaves.append(v.toString());
            if (!wantSaves.isEmpty()) {
                const QString savesRoot = gameDir + QStringLiteral("/saves");
                for (const auto& s : wantSaves) {
                    const QString d = savesRoot + QStringLiteral("/") + s;
                    if (!QDir(d).exists()) continue;
                    QDirIterator it(d, QDir::Files, QDirIterator::Subdirectories);
                    while (it.hasNext()) {
                        const QString p = it.next();
                        ovFiles.append({p, QStringLiteral("saves/") + s + QStringLiteral("/") + QDir(d).relativeFilePath(p)});
                    }
                }
            }
        }

        // ── 4. 计算模组双哈希（SHA1 + SHA512 + CF MurmurHash2）──
        const int modTotal = mods.size();
        for (int i = 0; i < modTotal; ++i) {
            if (m_cancel.loadRelaxed()) { finish(false, tr("已取消")); return; }
            auto& m = mods[i];
            QFile f(m.diskPath);
            if (!f.open(QIODevice::ReadOnly)) continue;
            const QByteArray data = f.readAll();
            f.close();
            m.sha1 = QCryptographicHash::hash(data, QCryptographicHash::Sha1);
            m.sha512 = QCryptographicHash::hash(data, QCryptographicHash::Sha512);
            m.cfHash = cfMurmurHash2(data);
            setProgress(0.05 + 0.15 * (i + 1) / qMax(1, modTotal),
                        tr("计算模组哈希 %1/%2").arg(i + 1).arg(modTotal));
        }

        // ── 5. 双平台查询在线来源（同主流启动器）──
        int modrinthHits = 0, cfHits = 0;
        if (!mods.isEmpty()) {
            // 5a. Modrinth：批量 sha1 查询
            QJsonArray shaArr;
            for (const auto& m : mods)
                shaArr.append(QString::fromLatin1(m.sha1.toHex()));
            QJsonObject bodyObj;
            bodyObj.insert(QStringLiteral("hashes"), shaArr);
            bodyObj.insert(QStringLiteral("algorithm"), QStringLiteral("sha1"));
            const QByteArray body = QJsonDocument(bodyObj).toJson(QJsonDocument::Compact);
            setProgress(0.22, tr("查询 Modrinth 在线来源..."));
            const QByteArray resp = postJson(QUrl(QStringLiteral("https://api.modrinth.com/v2/version_files")), body);
            if (!resp.isEmpty()) {
                const QJsonObject root = QJsonDocument::fromJson(resp).object();
                for (auto& m : mods) {
                    const QString shaHex = QString::fromLatin1(m.sha1.toHex());
                    const QJsonObject entry = root.value(shaHex).toObject();
                    if (entry.isEmpty()) continue;
                    const QJsonArray files = entry.value(QStringLiteral("files")).toArray();
                    if (files.isEmpty()) continue;
                    const QJsonObject f0 = files.first().toObject();
                    if (f0.value(QStringLiteral("hashes")).toObject().value(QStringLiteral("sha1")).toString() != shaHex)
                        continue;   // 主流启动器：校验返回 sha1 与请求一致
                    const QString url = f0.value(QStringLiteral("url")).toString();
                    if (!url.isEmpty()) {
                        m.downloads.append(url);
                        m.hosted = true;
                        modrinthHits++;
                    }
                }
            } else {
                qCWarning(logMod) << "[导出] Modrinth 查询失败，相关文件将直接打包";
            }
            setProgress(0.5, tr("Modrinth 查询完成（命中 %1）").arg(modrinthHits));

            // 5b. CurseForge：批量 fingerprint 查询（ModrinthUploadMode 跳过）
            if (!modrinthUploadMode && !cfKey.isEmpty()) {
                QJsonArray fpArr;
                for (const auto& m : mods)
                    fpArr.append(static_cast<double>(m.cfHash));
                QJsonObject cfBodyObj;
                cfBodyObj.insert(QStringLiteral("fingerprints"), fpArr);
                const QByteArray cfBody = QJsonDocument(cfBodyObj).toJson(QJsonDocument::Compact);
                setProgress(0.55, tr("查询 CurseForge 在线来源..."));
                const QByteArray cfResp = postJson(
                    QUrl(QStringLiteral("https://api.curseforge.com/v1/fingerprints/432/")), cfBody,
                    {{"x-api-key", cfKey.toUtf8()}});
                if (!cfResp.isEmpty()) {
                    const QJsonObject data = QJsonDocument::fromJson(cfResp).object()
                                                 .value(QStringLiteral("data")).toObject();
                    const QJsonArray matches = data.value(QStringLiteral("exactMatches")).toArray();
                    for (const auto& mv : matches) {
                        const QJsonObject match = mv.toObject();
                        const quint32 fp = static_cast<quint32>(
                            match.value(QStringLiteral("fileFingerprint")).toDouble());
                        const QJsonObject file = match.value(QStringLiteral("file")).toObject();
                        const QString dlUrl = file.value(QStringLiteral("downloadUrl")).toString();
                        if (dlUrl.isEmpty()) continue;
                        for (auto& m : mods) {
                            if (m.cfHash != fp) continue;
                            m.cfProjectId = match.value(QStringLiteral("projectId")).toInt();
                            m.cfFileId = match.value(QStringLiteral("id")).toInt();
                            // 域名变体展开（同主流启动器）
                            const auto urls = expandCfDownloadUrls(dlUrl);
                            for (const auto& u : urls)
                                if (!m.downloads.contains(u))
                                    m.downloads.append(u);
                            m.hosted = true;
                            cfHits++;
                            break;
                        }
                    }
                } else {
                    qCWarning(logMod) << "[导出] CurseForge 查询失败，相关文件将直接打包";
                }
                setProgress(0.78, tr("CurseForge 查询完成（命中 %1）").arg(cfHits));
            } else if (!modrinthUploadMode) {
                qCWarning(logMod) << "[导出] 未配置 CurseForge API Key，CF 在线来源跳过（文件将直接打包）";
            }
        }

        if (m_cancel.loadRelaxed()) { finish(false, tr("已取消")); return; }

        // ── 6. 构建清单 + 打包 ──
        // hosted（有在线来源）→ files[]/manifest 引用，不打包实体；
        // 非 hosted → overrides/mods/ 实体直装（同主流启动器：ModFile.File.Delete() 语义）
        const int hostedCount = mods.size() - [&]() {
            int n = 0;
            for (const auto& m : mods) if (!m.hosted) n++;
            return n;
        }();

        // 实体文件总数（打包进度用）：非 hosted mods + overrides
        qint64 totalBytes = 0;
        int totalFiles = 1;   // 清单文件
        for (const auto& m : mods) if (!m.hosted) { totalFiles++; totalBytes += m.size; }
        for (const auto& o : ovFiles) { totalFiles++; totalBytes += QFileInfo(o.diskPath).size(); }

        ZipArchive zip;
        if (!zip.openForWrite(outPath)) {
            finish(false, zip.error());
            return;
        }
        int done = 0;

        // 6a. 清单（Modrinth index.json / CurseForge manifest.json）
        if (cfFormat) {
            QJsonObject manifest;
            manifest.insert(QStringLiteral("manifestType"), QStringLiteral("minecraftModpack"));
            manifest.insert(QStringLiteral("manifestVersion"), 1);
            manifest.insert(QStringLiteral("name"), displayName);
            manifest.insert(QStringLiteral("version"), packVersion.isEmpty() ? QStringLiteral("1.0.0") : packVersion);
            manifest.insert(QStringLiteral("author"), QString());
            QJsonArray filesArr;
            for (const auto& m : mods) {
                if (!m.hosted || m.cfFileId <= 0) continue;
                QJsonObject f;
                f.insert(QStringLiteral("projectID"), m.cfProjectId);
                f.insert(QStringLiteral("fileID"), m.cfFileId);
                f.insert(QStringLiteral("required"), true);
                filesArr.append(f);
            }
            manifest.insert(QStringLiteral("files"), filesArr);
            manifest.insert(QStringLiteral("overrides"), QStringLiteral("overrides"));
            QJsonObject mc;
            mc.insert(QStringLiteral("version"), mcVersion);
            QJsonArray loaders;
            for (auto it = deps.begin(); it != deps.end(); ++it) {
                QJsonObject l;
                l.insert(QStringLiteral("id"), it.key() + QStringLiteral("-") + it.value());
                l.insert(QStringLiteral("primary"), loaders.isEmpty());
                loaders.append(l);
            }
            mc.insert(QStringLiteral("modLoaders"), loaders);
            manifest.insert(QStringLiteral("minecraft"), mc);
            if (!zip.addData(QStringLiteral("manifest.json"),
                             QJsonDocument(manifest).toJson(QJsonDocument::Indented))) {
                finish(false, zip.error());
                return;
            }
        } else {
            QJsonObject index;
            index.insert(QStringLiteral("formatVersion"), 1);
            index.insert(QStringLiteral("game"), mcVersion);
            index.insert(QStringLiteral("versionId"),
                         packVersion.isEmpty() ? displayName : displayName + QStringLiteral("-") + packVersion);
            index.insert(QStringLiteral("name"), displayName);
            index.insert(QStringLiteral("summary"), QString());
            QJsonArray filesArr;
            for (const auto& m : mods) {
                if (!m.hosted) continue;
                QJsonObject f;
                f.insert(QStringLiteral("path"), QStringLiteral("mods/") + m.relPath);
                QJsonObject hashes;
                hashes.insert(QStringLiteral("sha1"), QString::fromLatin1(m.sha1.toHex()));
                hashes.insert(QStringLiteral("sha512"), QString::fromLatin1(m.sha512.toHex()));
                f.insert(QStringLiteral("hashes"), hashes);
                // downloads：URL 列表，非 Modrinth 优先排序（同主流启动器 OrderBy）
                QJsonArray dlArr;
                QStringList sorted = m.downloads;
                std::stable_sort(sorted.begin(), sorted.end(),
                                 [](const QString& a, const QString& b) {
                                     return a.contains(QStringLiteral("modrinth.com"))
                                         && !b.contains(QStringLiteral("modrinth.com"));
                                 });
                for (const auto& u : sorted) dlArr.append(u);
                f.insert(QStringLiteral("downloads"), dlArr);
                f.insert(QStringLiteral("fileSize"), m.size);
                filesArr.append(f);
            }
            index.insert(QStringLiteral("files"), filesArr);
            QJsonObject depsObj;
            depsObj.insert(QStringLiteral("minecraft"), mcVersion);
            for (auto it = deps.begin(); it != deps.end(); ++it)
                depsObj.insert(it.key(), it.value());
            index.insert(QStringLiteral("dependencies"), depsObj);
            if (!zip.addData(QStringLiteral("modrinth.index.json"),
                             QJsonDocument(index).toJson(QJsonDocument::Indented))) {
                finish(false, zip.error());
                return;
            }
        }
        ++done;
        setProgress(0.8, tr("生成压缩包..."));

        // 6b. 非 hosted mods → overrides/mods/ 实体
        for (const auto& m : mods) {
            if (m.hosted) continue;
            if (m_cancel.loadRelaxed()) { zip.closeWrite(); QFile::remove(outPath); finish(false, tr("已取消")); return; }
            if (!zip.addFile(m.diskPath, QStringLiteral("overrides/mods/") + m.relPath)) {
                finish(false, zip.error());
                return;
            }
            ++done;
            setProgress(0.8 + 0.15 * done / qMax(1, totalFiles), tr("打包模组 %1/%2").arg(done).arg(totalFiles));
        }

        // 6c. overrides 文件
        for (const auto& o : ovFiles) {
            if (m_cancel.loadRelaxed()) { zip.closeWrite(); QFile::remove(outPath); finish(false, tr("已取消")); return; }
            if (!zip.addFile(o.diskPath, QStringLiteral("overrides/") + o.relPath)) {
                finish(false, zip.error());
                return;
            }
            ++done;
            setProgress(0.8 + 0.15 * done / qMax(1, totalFiles), tr("打包覆写文件 %1/%2").arg(done).arg(totalFiles));
        }

        if (!zip.closeWrite()) {
            QFile::remove(outPath);
            finish(false, zip.error());
            return;
        }

        const int localCount = mods.size() - hostedCount;
        const QString note = modrinthUploadMode
            ? tr("（%1 个模组未在 Modrinth 托管，已直接打包）").arg(localCount)
            : tr("（%1 个模组已联网托管，%2 个直接打包）").arg(hostedCount).arg(localCount);
        setProgress(1.0, tr("导出完成 %1").arg(note));
        qCInfo(logMod) << QStringLiteral("[整合包] 导出完成 %1 → %2 (mods=%3 hosted=%4 local=%5)")
            .arg(displayName, outPath).arg(mods.size()).arg(hostedCount).arg(localCount);
        finish(true, note);
    });
}

} // namespace ShadowLauncher
