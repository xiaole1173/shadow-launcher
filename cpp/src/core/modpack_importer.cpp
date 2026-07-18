// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "modpack_importer.h"
#include "http_client.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QDebug>
#include <QRegularExpression>
#include <QDateTime>
#include <QCryptographicHash>
#include <QEventLoop>
#include "../backend/version_backend.h"

using namespace ShadowLauncher;

// ── Helpers ──

static QString readZipEntry(const QString& zipPath, const QString& entryPath) {
    QProcess proc;
    QString escapedZip = QString(zipPath).replace("'", "''");
    QString escapedEntry = QString(entryPath).replace("'", "''");
    QString cmd = QStringLiteral(
        "& { Add-Type -AssemblyName 'System.IO.Compression.FileSystem'; "
        "$z = [System.IO.Compression.ZipFile]::OpenRead('%1'); "
        "$e = $z.GetEntry('%2'); "
        "if ($e) { $sr = New-Object IO.StreamReader($e.Open()); "
        "$c = $sr.ReadToEnd(); $sr.Dispose() } "
        "$z.Dispose(); "
        "if ($e) { Write-Output $c } else { Write-Output '' } }"
    ).arg(escapedZip, escapedEntry);
    proc.start("powershell", QStringList{"-NoProfile", "-Command", cmd});
    proc.waitForFinished(15000);
    QByteArray out = proc.readAllStandardOutput();
    // Extract JSON content from PowerShell output
    int braceStart = out.indexOf('{');
    if (braceStart < 0) return {};
    int braceEnd = out.lastIndexOf('}');
    if (braceEnd <= braceStart) return {};
    return QString::fromUtf8(out.mid(braceStart, braceEnd - braceStart + 1));
}

static bool extractZip(const QString& zipPath, const QString& destDir) {
    QDir().mkpath(destDir);
    QProcess proc;
    QString escapedZip = QString(zipPath).replace("'", "''");
    QString escapedDest = QString(destDir).replace("'", "''");
    QString cmd = QStringLiteral("Expand-Archive -Path '%1' -DestinationPath '%2' -Force")
        .arg(escapedZip, escapedDest);
    proc.start("powershell", QStringList{"-NoProfile", "-Command", cmd});
    proc.waitForFinished(30000);
    return proc.exitCode() == 0;
}

static QStringList listZipEntries(const QString& zipPath, const QString& prefix = {}) {
    QStringList result;
    QProcess proc;
    QString escapedZip = QString(zipPath).replace("'", "''");
    QString cmd = QStringLiteral(
        "& { Add-Type -AssemblyName 'System.IO.Compression.FileSystem'; "
        "$z = [System.IO.Compression.ZipFile]::OpenRead('%1'); "
        "$z.Entries | ForEach-Object { $_.FullName }; $z.Dispose() }"
    ).arg(escapedZip);
    proc.start("powershell", QStringList{"-NoProfile", "-Command", cmd});
    proc.waitForFinished(15000);
    QByteArray outBytes = proc.readAllStandardOutput();
    QString out = QString::fromUtf8(outBytes);
    const QStringList lines = out.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        QString t = line.trimmed();
        if (!t.isEmpty()) {
            if (prefix.isEmpty() || t.startsWith(prefix))
                result.append(t);
        }
    }
    return result;
}

// ── Constructor / Destructor ──

ModpackImporter::ModpackImporter(QObject* parent)
    : QObject(parent)
    , m_http(&HttpClient::instance())
    , m_downloadQueue(new DownloadQueue(5, this))
{
}

ModpackImporter::~ModpackImporter() = default;

QByteArray ModpackImporter::downloadUrl(const QString& url)
{
    QNetworkReply* reply = m_http->getRaw(QNetworkRequest(QUrl(url)));
    if (!reply) return {};

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);

    QTimer cancelPoller;
    cancelPoller.setInterval(200);

    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(&cancelPoller, &QTimer::timeout, &loop, [&]() {
        if (m_cancelled.load()) {
            reply->abort();
            loop.quit();
        }
    });

    timeout.start(30000);
    cancelPoller.start();
    loop.exec();
    cancelPoller.stop();
    timeout.stop();

    if (m_cancelled.load()) {
        reply->deleteLater();
        return {};
    }
    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "[modpack] HTTP GET failed:" << url << reply->errorString();
        reply->deleteLater();
        return {};
    }
    QByteArray data = reply->readAll();
    reply->deleteLater();
    return data;
}

// ── Public API ──

void ModpackImporter::startImport(const QString& zipFilePath) {
    if (m_busy) return;
    m_cancelled = false;
    m_busy = true;
    m_hasResult = false;
    m_zipPath = zipFilePath;
    m_modItems.clear();
    m_modsTotal = 0;
    m_meta = ModpackMeta{};
    m_targetName.clear();
    emit busyChanged();
    emit modListChanged();

    // Step 1: Parse zip (async via timer to let UI update)
    setStep(StepParsing);
    QTimer::singleShot(50, this, [this]() {
        if (m_cancelled) return;
        if (!parseZip(m_zipPath)) {
            setError(tr("无法解析整合包文件，请确认文件是有效的 Modrinth (.mrpack) 或 CurseForge (.zip) 格式"));
            return;
        }
        // Start installation
        proceedToInstall();
    });
}

void ModpackImporter::setVersionBackend(VersionBackend* vb)
{
    m_versionBackend = vb;
}

void ModpackImporter::cancelImport() {
    m_cancelled = true;
    m_busy = false;
    m_downloadQueue->cancelAll();
    disconnect(m_downloadQueue, nullptr, this, nullptr);
    setProgress(0.0, tr("已取消"), {});
    emit busyChanged();
}

void ModpackImporter::dismissResult() {
    m_hasResult = false;
    m_resultName.clear();
    m_resultVersionId.clear();
    emit hasResultChanged();
}

// ── Format detection ──

ModpackImporter::Format ModpackImporter::detectFormat(const QString& zipPath) {
    QString content = readZipEntry(zipPath, "modrinth.index.json");
    if (!content.isEmpty() && content.contains("\"formatVersion\""))
        return Modrinth;
    content = readZipEntry(zipPath, "manifest.json");
    if (!content.isEmpty() && content.contains("\"manifestType\""))
        return CurseForge;
    return Unknown;
}

QJsonObject ModpackImporter::parseManifest(const QString& zipPath, Format fmt) {
    QString entryName = (fmt == Modrinth) ? "modrinth.index.json" : "manifest.json";
    QString content = readZipEntry(zipPath, entryName);
    QJsonDocument doc = QJsonDocument::fromJson(content.toUtf8());
    return doc.object();
}

// ── Parsing ──

bool ModpackImporter::parseZip(const QString& path) {
    setProgress(0.05, tr("正在检测整合包格式…"), QFileInfo(path).fileName());

    Format fmt = detectFormat(path);
    if (fmt == Unknown) {
        qWarning() << "[modpack] Unknown format for" << path;
        return false;
    }
    m_meta.format = fmt;

    setProgress(0.1, tr("正在解析清单文件…"));

    QJsonObject manifest = parseManifest(path, fmt);
    if (manifest.isEmpty()) {
        qWarning() << "[modpack] Failed to parse manifest";
        return false;
    }

    if (fmt == Modrinth) {
        // ── Modrinth format ──
        m_meta.name = manifest.value("name").toString();
        m_meta.versionId = manifest.value("versionId").toString();
        m_meta.summary = manifest.value("summary").toString();

        // Dependencies → MC version + loader
        QJsonObject deps = manifest.value("dependencies").toObject();
        m_meta.mcVersion = deps.value("minecraft").toString();
        if (deps.contains("forge")) {
            m_meta.loaderType = "forge";
            m_meta.loaderVersion = deps.value("forge").toString();
        } else if (deps.contains("neoforge")) {
            m_meta.loaderType = "neoforge";
            m_meta.loaderVersion = deps.value("neoforge").toString();
        } else if (deps.contains("fabric-loader")) {
            m_meta.loaderType = "fabric";
            m_meta.loaderVersion = deps.value("fabric-loader").toString();
        } else if (deps.contains("quilt-loader")) {
            m_meta.loaderType = "quilt";
            m_meta.loaderVersion = deps.value("quilt-loader").toString();
        }

        // Files
        QJsonArray files = manifest.value("files").toArray();
        for (const QJsonValue& val : files) {
            QJsonObject f = val.toObject();
            QVariantMap entry;
            entry["path"] = f.value("path").toString();
            entry["url"] = f.value("downloads").toArray().first().toString();
            entry["size"] = (qint64)f.value("fileSize").toDouble();
            entry["sha1"] = f.value("hashes").toObject().value("sha1").toString();
            entry["status"] = "pending";
            m_meta.mods.append(entry);
        }

        // Overrides
        QStringList entries = listZipEntries(path, "overrides/");
        m_meta.hasOverrides = !entries.isEmpty();
        m_meta.overridesPaths = entries;

    } else if (fmt == CurseForge) {
        // ── CurseForge format ──
        m_meta.name = manifest.value("name").toString();
        m_meta.versionId = manifest.value("version").toString();

        QJsonObject mc = manifest.value("minecraft").toObject();
        m_meta.mcVersion = mc.value("version").toString();
        QJsonArray loaders = mc.value("modLoaders").toArray();
        if (!loaders.isEmpty()) {
            QString loaderId = loaders.first().toObject().value("id").toString();
            // Parse "forge-47.1.0" or "fabric-loader-0.15.0"
            if (loaderId.startsWith("forge")) {
                m_meta.loaderType = "forge";
                m_meta.loaderVersion = loaderId.mid(QStringLiteral("forge-").length());
            } else if (loaderId.startsWith("neoforge")) {
                m_meta.loaderType = "neoforge";
                m_meta.loaderVersion = loaderId.mid(QStringLiteral("neoforge-").length());
            } else if (loaderId.startsWith("fabric")) {
                m_meta.loaderType = "fabric";
                m_meta.loaderVersion = loaderId.mid(QStringLiteral("fabric-loader-").length());
            } else if (loaderId.startsWith("quilt")) {
                m_meta.loaderType = "quilt";
                m_meta.loaderVersion = loaderId.mid(QStringLiteral("quilt-loader-").length());
            } else {
                m_meta.loaderVersion = loaderId; // raw
            }
        }

        // Files — projectID + fileID, need to resolve to URLs via MCIM mirror
        QJsonArray files = manifest.value("files").toArray();
        for (const QJsonValue& val : files) {
            QJsonObject f = val.toObject();
            QVariantMap entry;
            entry["projectID"] = f.value("projectID").toInt();
            entry["fileID"] = f.value("fileID").toInt();
            entry["required"] = f.value("required").toBool(true);
            entry["status"] = "pending";
            // Build MCIM mirror URL for CurseForge
            int pid = f.value("projectID").toInt();
            int fid = f.value("fileID").toInt();
            entry["url"] = QStringLiteral("https://mod.mcimirror.top/curseforge/project/%1/files/%2/download")
                .arg(pid).arg(fid);
            entry["size"] = 0;
            m_meta.mods.append(entry);
        }

        // Overrides
        QStringList entries = listZipEntries(path, "overrides/");
        m_meta.hasOverrides = !entries.isEmpty();
        m_meta.overridesPaths = entries;
    }

    // Build display name
    if (m_meta.name.isEmpty())
        m_meta.name = QFileInfo(path).completeBaseName();
    if (m_meta.versionId.isEmpty())
        m_meta.versionId = "1.0";

    // Target version folder name
    m_targetName = QStringLiteral("Modpack-%1-%2")
        .arg(m_meta.name.simplified().replace(QRegularExpression("[^a-zA-Z0-9_\\-]"), "_"))
        .arg(m_meta.versionId);

    // MC directory
    m_mcDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (m_mcDir.isEmpty()) m_mcDir = QDir::homePath() + "/.minecraft";
    m_versionsDir = m_mcDir + "/versions";
    m_modsDir = m_versionsDir + "/" + m_targetName + "/mods";

    // Update QML mod list for display
    m_modItems.clear();
    for (int i = 0; i < m_meta.mods.size(); i++) {
        QVariantMap item;
        item["index"] = i;
        item["name"] = QFileInfo(m_meta.mods[i]["path"].toString()).fileName();
        item["size"] = m_meta.mods[i]["size"].toLongLong();
        item["status"] = "pending";
        m_modItems.append(item);
    }
    emit modListChanged();

    qDebug() << "[modpack] Parsed:" << m_meta.name
             << "MC:" << m_meta.mcVersion
             << "Loader:" << m_meta.loaderType << m_meta.loaderVersion
             << "Mods:" << m_meta.mods.size()
             << "Overrides:" << m_meta.hasOverrides;
    return true;
}

// ── Installation Flow ──

void ModpackImporter::proceedToInstall() {
    if (m_cancelled) return;

    // Step 2: Install MC version
    setStep(StepInstallMc);
    setProgress(0.15, tr("正在安装 Minecraft %1…").arg(m_meta.mcVersion));

    if (!installMcVersion(m_meta.mcVersion, m_targetName)) {
        setError(tr("Minecraft %1 版本安装失败").arg(m_meta.mcVersion));
        return;
    }

    if (m_cancelled) return;

    // Step 3: Install mod loader
    if (!m_meta.loaderType.isEmpty() && !m_meta.loaderVersion.isEmpty()) {
        setStep(StepInstallLoader);
        setProgress(0.35, tr("正在安装 %1 %2…").arg(m_meta.loaderType, m_meta.loaderVersion));

        if (!installModLoader(m_meta.mcVersion, m_meta.loaderType,
                              m_meta.loaderVersion, m_targetName)) {
            setError(tr("%1 %2 安装失败").arg(m_meta.loaderType, m_meta.loaderVersion));
            return;
        }
    }

    if (m_cancelled) return;

    // Step 4: Download mods (async via DownloadQueue)
    proceedToModDownload();
}

void ModpackImporter::proceedToModDownload()
{
    if (m_cancelled) return;

    if (m_meta.mods.isEmpty()) {
        // No mods to download, skip to extraction
        proceedToOverlayExtract();
        return;
    }

    setStep(StepDownloadMods);
    setProgress(0.50, tr("正在下载模组 (0/%1)…").arg(m_meta.mods.size()));

    if (!downloadMods()) {
        return; // downloadMods handles its own errors
    }
}

void ModpackImporter::proceedToOverlayExtract()
{
    if (m_cancelled) return;

    // Step 5: Extract overrides
    if (m_meta.hasOverrides) {
        setStep(StepExtractOverrides);
        setProgress(0.90, tr("正在解压覆盖文件…"));

        QString gameDir = m_versionsDir + "/" + m_targetName;
        if (!extractOverrides(gameDir)) {
            qWarning() << "[modpack] Override extraction had issues (non-fatal)";
        }
    }

    if (m_cancelled) return;

    // Done
    finishImport(m_meta.name, m_targetName);
}

bool ModpackImporter::installMcVersion(const QString& versionId, const QString& targetName) {
    // 1. Fetch version manifest to get the version JSON URL
    QByteArray manifestData = downloadUrl("https://bmclapi2.bangbang93.com/mc/game/version_manifest.json");
    if (manifestData.isEmpty()) {
        manifestData = downloadUrl("https://launchermeta.mojang.com/mc/game/version_manifest.json");
    }
    if (manifestData.isEmpty()) {
        qWarning() << "[modpack] Failed to fetch version manifest";
        return false;
    }

    QJsonDocument manifestDoc = QJsonDocument::fromJson(manifestData);
    QJsonArray versions = manifestDoc.object().value("versions").toArray();

    QString versionUrl;
    for (const QJsonValue& v : versions) {
        QJsonObject vo = v.toObject();
        if (vo.value("id").toString() == versionId) {
            versionUrl = vo.value("url").toString();
            break;
        }
    }

    if (versionUrl.isEmpty()) {
        qWarning() << "[modpack] Version" << versionId << "not found in manifest";
        return false;
    }

    // 2. Download version JSON
    QByteArray versionJsonData = downloadUrl(versionUrl);
    if (versionJsonData.isEmpty()) {
        qWarning() << "[modpack] Failed to download version JSON for" << versionId;
        return false;
    }

    // 3. Create version folder and write JSON (with new ID)
    QDir().mkpath(m_versionsDir + "/" + targetName);
    QString jsonPath = m_versionsDir + "/" + targetName + "/" + targetName + ".json";

    QJsonDocument vDoc = QJsonDocument::fromJson(versionJsonData);
    QJsonObject vObj = vDoc.object();
    vObj["id"] = targetName;
    QFile f(jsonPath);
    if (!f.open(QIODevice::WriteOnly)) {
        qWarning() << "[modpack] Cannot write" << jsonPath;
        return false;
    }
    f.write(QJsonDocument(vObj).toJson());
    f.close();

    // 4. Download client.jar
    QString jarUrl;
    QJsonObject downloads = vObj.value("downloads").toObject();
    QJsonObject client = downloads.value("client").toObject();
    jarUrl = client.value("url").toString();

    if (!jarUrl.isEmpty()) {
        setProgress(0.25, tr("正在下载 Minecraft %1…").arg(versionId), "client.jar");
        QString jarPath = m_versionsDir + "/" + targetName + "/" + targetName + ".jar";
        QByteArray jarData = downloadUrl(jarUrl);
        if (jarData.isEmpty()) {
            // Try BMCLAPI mirror
            QString bmclJar = QStringLiteral("https://bmclapi2.bangbang93.com/version/%1/client/%2")
                .arg(versionId, targetName + ".jar");
            jarData = downloadUrl(bmclJar);
        }
        if (!jarData.isEmpty()) {
            QFile jf(jarPath);
            if (jf.open(QIODevice::WriteOnly)) {
                jf.write(jarData);
                jf.close();
            }
        } else {
            qWarning() << "[modpack] Failed to download client.jar for" << versionId;
            // Non-fatal - jar can be downloaded later by the game
        }
    }

    return true;
}

bool ModpackImporter::installModLoader(const QString& mcVersion, const QString& loaderType,
                                        const QString& loaderVersion, const QString& targetName)
{
    if (!m_versionBackend) {
        qWarning() << "[modpack] No VersionBackend set, skipping loader installation";
        return true;
    }

    qDebug() << "[modpack] Installing loader" << loaderType << loaderVersion << "for" << mcVersion << "->" << targetName;

    // Map loaderType to what VersionBackend expects
    QString vbLoaderType = loaderType;
    if (vbLoaderType == QStringLiteral("fabric"))
        vbLoaderType = QStringLiteral("fabric-loader");
    else if (vbLoaderType == QStringLiteral("quilt"))
        vbLoaderType = QStringLiteral("quilt-loader");

    // Use a nested event loop to wait for the async installation
    QEventLoop loop;
    bool success = false;
    bool done = false;

    // Store connection handles so we can disconnect before returning
    QMetaObject::Connection finishedConn;
    QMetaObject::Connection stateConn;

    finishedConn = connect(m_versionBackend, &VersionBackend::installFinished,
                           this, [&](bool ok) {
        success = ok;
        done = true;
        loop.quit();
    });

    stateConn = connect(m_versionBackend, &VersionBackend::installStateChanged,
                        this, [&]() {
        if (!m_versionBackend->isInstalling() && !done) {
            done = true;
            success = false;
            loop.quit();
        }
    });

    m_versionBackend->installModLoader(mcVersion, vbLoaderType, loaderVersion, targetName);

    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeoutTimer.start(120000);

    if (!done) {
        loop.exec();
    }
    timeoutTimer.stop();

    // Disconnect both connections BEFORE any return path
    disconnect(finishedConn);
    disconnect(stateConn);

    if (!done) {
        qWarning() << "[modpack] Loader installation timeout";
        return false;
    }

    if (!success) {
        qWarning() << "[modpack] Loader installation failed";
        return false;
    }

    qDebug() << "[modpack] Loader installation complete";
    return true;
}

bool ModpackImporter::downloadMods() {
    if (m_meta.mods.isEmpty()) return true;

    QDir().mkpath(m_modsDir);
    m_modsTotal = m_meta.mods.size();


    // Disconnect old connections to avoid duplicates on retry
    disconnect(m_downloadQueue, &DownloadQueue::allCompleted,
               this, &ModpackImporter::proceedToOverlayExtract);
    disconnect(m_downloadQueue, &DownloadQueue::progressChanged,
               this, nullptr);

    // When all downloads complete, proceed to extraction
    connect(m_downloadQueue, &DownloadQueue::allCompleted, this, &ModpackImporter::proceedToOverlayExtract);

    // Track per-file completion for QML display
    connect(m_downloadQueue, &DownloadQueue::progressChanged, this, [this](int completed, int total) {
        Q_UNUSED(total);

        // Estimate overall progress: 50% → 90% during downloads
        qreal p = 0.50 + 0.40 * (qreal)completed / qMax(1, m_modsTotal);
        setProgress(p, tr("正在下载模组 (%1/%2)…").arg(completed).arg(m_modsTotal),
                    m_currentFile);
    });

    // Enqueue all mod downloads
    for (int i = 0; i < m_meta.mods.size(); i++) {
        if (m_cancelled) return false;

        const QVariantMap& entry = m_meta.mods[i];
        QString url = entry.value("url").toString();
        QString relPath = entry.value("path").toString();

        // Modrinth paths strip "mods/" prefix
        if (relPath.startsWith("mods/") || relPath.startsWith("mods\\"))
            relPath = relPath.mid(5);
        QString savePath = m_modsDir + "/" + QFileInfo(relPath).fileName();
        QByteArray sha1Hex = entry.value("sha1").toString().toUtf8();
        int index = i;  // capture for lambda

        // Per-file progress
        auto progressCb = [this, index, relPath](qint64 received, qint64 total) {
            Q_UNUSED(received); Q_UNUSED(total);
            m_currentFile = QFileInfo(relPath).fileName();
        };

        // Per-file completion: update QML item status
        auto doneCb = [this, index, savePath, sha1Hex](bool success, const QString& err) {
            Q_UNUSED(err);

            // Optional SHA1 validation on success
            if (success && !sha1Hex.isEmpty()) {
                QFile f(savePath);
                if (f.open(QIODevice::ReadOnly)) {
                    QByteArray hash = QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha1).toHex();
                    f.close();
                    if (hash != sha1Hex) {
                        qWarning() << "[modpack] SHA1 mismatch for" << savePath
                                   << "expected:" << sha1Hex << "got:" << hash;
                        QFile::remove(savePath);
                        success = false;
                    }
                }
            }

            // Update QML list item
            if (index < m_modItems.size()) {
                QVariantMap item = m_modItems[index].toMap();
                item["status"] = success ? "done" : "fail";
                m_modItems[index] = item;
            }
            emit modListChanged();
        };

        m_downloadQueue->enqueue(url, savePath, std::move(progressCb), std::move(doneCb));
    }

    return true;  // Returns immediately; actual results come via signals
}

bool ModpackImporter::extractOverrides(const QString& gameDir) {
    // Extract the entire zip to a temp dir, then copy overrides/ to gameDir
    QString tmpDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                     + "/shadow_modpack_" + QString::number(QDateTime::currentMSecsSinceEpoch());
    if (!extractZip(m_zipPath, tmpDir)) {
        qWarning() << "[modpack] Failed to extract zip for overrides";
        return false;
    }

    QString overridesSrc = tmpDir + "/overrides";
    QDir overridesDir(overridesSrc);
    if (!overridesDir.exists()) {
        // Clean up
        QDir(tmpDir).removeRecursively();
        qWarning() << "[modpack] No overrides/ directory found in extracted zip";
        return false;
    }

    // Copy all files from overrides/ to gameDir, preserving structure
    std::function<bool(const QString&, const QString&)> copyDir;
    copyDir = [&copyDir](const QString& src, const QString& dst) -> bool {
        QDir srcDir(src);
        QDir dstDir(dst);
        if (!srcDir.exists()) return false;
        dstDir.mkpath(".");

        for (const QFileInfo& fi : srcDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
            QString relName = fi.fileName();
            if (fi.isDir()) {
                copyDir(fi.absoluteFilePath(), dst + "/" + relName);
            } else {
                QString dstPath = dst + "/" + relName;
                // Only copy if file doesn't exist (don't overwrite mods)
                if (!QFile::exists(dstPath)) {
                    QFile::copy(fi.absoluteFilePath(), dstPath);
                }
            }
        }
        return true;
    };

    copyDir(overridesSrc, gameDir);

    // Clean up temp
    QDir(tmpDir).removeRecursively();

    return true;
}

// ── State management ──

void ModpackImporter::setStep(Step s) {
    m_currentStep = s;
}

void ModpackImporter::setProgress(qreal p, const QString& text, const QString& file) {
    m_progress = qBound(0.0, p, 1.0);
    m_statusText = text;
    m_currentFile = file;
    emit progressChanged();
}

void ModpackImporter::setError(const QString& message) {
    m_currentStep = StepError;
    m_statusText = message;
    m_busy = false;
    emit progressChanged();
    emit busyChanged();
    emit importFinished(false, {}, message);
    qWarning() << "[modpack] Error:" << message;
}

void ModpackImporter::finishImport(const QString& name, const QString& verId) {
    m_currentStep = StepFinished;
    m_progress = 1.0;
    m_statusText = tr("导入完成");
    m_busy = false;
    m_hasResult = true;
    m_resultName = name;
    m_resultVersionId = verId;
    emit progressChanged();
    emit busyChanged();
    emit hasResultChanged();
    emit importFinished(true, name, tr("整合包「%1」已成功导入为版本「%2」").arg(name, verId));
    qDebug() << "[modpack] Import finished:" << name << "→" << verId;
}
