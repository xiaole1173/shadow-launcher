// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
// Shadow Launcher — VersionDownloader
// Minecraft version installation pipeline: client.jar + libraries + assets.
// Uses ParallelDownloader for concurrent multi-file downloads and
// HttpClient for single-file asset index retrieval.

#include "version_downloader.h"
#include "file_downloader.h"
#include "http_client.h"
#include "../utils/logger.h"

#include <QDir>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QEventLoop>
#include <QTimer>
#include <QAtomicInt>
#include <QUrl>

namespace ShadowLauncher {

// ═══════════════════════════════════════════════════════════
// MirrorSource — predefined mirrors
// ═══════════════════════════════════════════════════════════

MirrorSource MirrorSource::bmclapi()
{
    MirrorSource m;
    m.name           = QStringLiteral("BMCLAPI");
    m.desc           = QStringLiteral("国内高速 · 默认推荐");
    m.manifestUrl    = QStringLiteral("https://bmclapi2.bangbang93.com/mc/game/version_manifest.json");
    m.versionMetaHost = QStringLiteral("bmclapi2.bangbang93.com");
    m.resourceBase   = QStringLiteral("https://bmclapi2.bangbang93.com/assets");
    m.libraryBase    = QStringLiteral("https://bmclapi2.bangbang93.com/maven");
    m.jarHost        = QStringLiteral("bmclapi2.bangbang93.com");
    m.isDefault      = true;
    m.healthCheckUrl = QStringLiteral("https://bmclapi2.bangbang93.com/");
    m.attribution    = QStringLiteral("资源由 BMCLAPI 镜像加速 | 文件归源站点所有");
    return m;
}

MirrorSource MirrorSource::mojang()
{
    MirrorSource m;
    m.name           = QStringLiteral("Mojang 官方");
    m.desc           = QStringLiteral("官方源 · 较慢但权威");
    m.manifestUrl    = QStringLiteral("https://launchermeta.mojang.com/mc/game/version_manifest.json");
    m.versionMetaHost = QStringLiteral("launchermeta.mojang.com");
    m.resourceBase   = QStringLiteral("https://resources.download.minecraft.net");
    m.libraryBase    = QStringLiteral("https://libraries.minecraft.net");
    m.jarHost        = QStringLiteral("launcher.mojang.com");
    m.healthCheckUrl = QStringLiteral("https://launchermeta.mojang.com/");
    // Mojang 官方源不需要标注
    return m;
}

QVector<MirrorSource> MirrorSource::allMirrors()
{
    return { bmclapi(), mojang() };
}

// ═══════════════════════════════════════════════════════════
// Construction / Destruction
// ═══════════════════════════════════════════════════════════

VersionDownloader::VersionDownloader(QObject* parent)
    : QObject(parent)
    , m_mirror(MirrorSource::bmclapi())  // BMCLAPI优先
    , m_minecraftDir(QDir::homePath() + QStringLiteral("/.shadow/minecraft"))
{
    m_downloadCfg.maxWorkers = m_maxWorkers;
    m_downloader = new ShadowDownloader::FileDownloader(this);
    m_downloader->setMaxThreads(m_maxWorkers);
    m_downloader->setSpeedLimitMB(m_downloadCfg.speedLimitMB);

    // Forward progress signals
    connect(m_downloader, &ShadowDownloader::FileDownloader::progressChanged,
            this, [this](int completed, int total, qint64 dlBytes, qint64 totBytes) {
        m_completedFiles.storeRelaxed(completed);
        m_totalFiles.storeRelaxed(total);
        m_downloadedBytes.storeRelaxed(dlBytes);
        m_totalBytes.storeRelaxed(totBytes);
        emit progressChanged(completed, total, dlBytes, totBytes);
    });

    connect(m_downloader, &ShadowDownloader::FileDownloader::logMessage,
            this, &VersionDownloader::logMessage);

    connect(m_downloader, &ShadowDownloader::FileDownloader::fileProgress,
            this, [this](const QString& url, const QString& fileName, qint64 rx, qint64 total, const QString& savePath) {
        emit fileProgress(url, fileName, rx, total, savePath);
    });

    connect(m_downloader, &ShadowDownloader::FileDownloader::allFinished,
            this, &VersionDownloader::onAllFinishedV2,
            Qt::QueuedConnection);
}

VersionDownloader::~VersionDownloader()
{
    // ParallelDownloader is a QObject child; auto-deleted
}

// ═══════════════════════════════════════════════════════════
// Configuration
// ═══════════════════════════════════════════════════════════

void VersionDownloader::setMirror(const MirrorSource& mirror)
{
    m_mirror = mirror;
}

void VersionDownloader::setMinecraftDir(const QString& dir)
{
    m_minecraftDir = dir;
}

void VersionDownloader::setMaxWorkers(int workers)
{
    m_maxWorkers = qBound(1, workers, 128);
    m_downloadCfg.maxWorkers = m_maxWorkers;
    if (m_downloader) m_downloader->setMaxThreads(m_maxWorkers);
}

void VersionDownloader::setDownloadConfig(const DownloadConfig& config)
{
    m_downloadCfg = config;
    m_maxWorkers = config.maxWorkers;
    if (m_downloader) {
        m_downloader->setMaxThreads(m_maxWorkers);
        m_downloader->setSpeedLimitMB(config.speedLimitMB);
    }
}

// ═══════════════════════════════════════════════════════════
// Main pipeline: downloadVersion
// ═══════════════════════════════════════════════════════════

void VersionDownloader::downloadVersion(const QJsonObject& versionJson,
                                         const QString& versionId)
{
    if (m_state == Running || m_state == Paused) {
        emit logMessage(tr("[警告] 已有下载任务进行中"));
        return;
    }

    m_state = Running;
    m_currentVersionId = versionId;
    m_currentVersionJson = versionJson;
    m_assetObjects.clear();
    m_taskDestPaths.clear();

    m_completedFiles.storeRelaxed(0);
    m_totalFiles.storeRelaxed(0);
    m_totalBytes.storeRelaxed(0);
    m_downloadedBytes.storeRelaxed(0);

    m_fallbackIndex = 0;
    m_fallbackChain.clear();
    m_fallbackChain.append(m_mirror);
    const auto all = MirrorSource::allMirrors();
    for (const auto& m : all) {
        if (m.name != m_mirror.name)
            m_fallbackChain.append(m);
    }

    emit stateChanged();
    emit logMessage(QString::fromUtf8("开始下载版本 %1 | 镜像=%2 | 目录=%3")
                        .arg(versionId, m_mirror.name, m_minecraftDir));

    // --- Step 1: Save version JSON to disk ---
    const QString versionDir = m_minecraftDir + QStringLiteral("/versions/") + versionId;
    QDir().mkpath(versionDir);

    const QString jsonPath = versionDir + QStringLiteral("/") + versionId + QStringLiteral(".json");
    QJsonDocument doc(versionJson);
    QFile jsonFile(jsonPath);
    if (jsonFile.open(QIODevice::WriteOnly)) {
        jsonFile.write(doc.toJson(QJsonDocument::Indented));
        jsonFile.close();
        emit logMessage(tr("已保存版本清单: %1").arg(jsonPath));
    } else {
        emit logMessage(tr("[警告] 无法保存版本清单: %1").arg(jsonPath));
    }

    // --- Step 2-4: Download asset index async, then collect & start tasks ---
    QJsonObject assetIdx = versionJson.value(QStringLiteral("assetIndex")).toObject();

    auto startTasks = [this, versionJson, versionId, assetIdx]() {
        if (!assetIdx.isEmpty()) {
            m_assetObjects = parseAssetIndex(assetIdx);
        }
        QVector<DownloadTask> tasks;
        collectTasks(versionJson, versionId, m_assetObjects, tasks);
        m_totalFiles.storeRelaxed(tasks.size());
        if (tasks.isEmpty()) {
            m_state = Done;
            emit downloadFinished(true, QString());
            emit stateChanged();
            return;
        }
        qint64 totalEstimate = 0;
        for (const auto& t : tasks) totalEstimate += t.totalBytes;
        m_totalBytes.storeRelaxed(totalEstimate);
        emit logMessage(tr("准备下载 %1 个文件").arg(tasks.size()));

        // Feed tasks → addFile with source ordering by policy
        bool preferOfficial = (m_downloadCfg.fileSource == DownloadSourcePolicy::PreferOfficial ||
                               m_downloadCfg.fileSource == DownloadSourcePolicy::AutoSwitch);
        for (const auto& t : tasks) {
            QStringList sources;
            // Apply source order policy
            if (preferOfficial) {
                bool mojangAdded = false;
                for (const auto& m : t.mirrors) {
                    if (!mojangAdded && (m.contains("mojang.com") || m.contains("minecraft.net"))) {
                        sources.append(m);
                        mojangAdded = true;
                    }
                }
                if (!t.url.contains("mojang.com") && !t.url.contains("minecraft.net"))
                sources.append(t.url);  // BMCLAPI as fallback
                for (const auto& m : t.mirrors) {
                    if (!m.contains("mojang.com") && !m.contains("minecraft.net"))
                        sources.append(m);
                }
            } else {
                sources.append(t.url);
                for (const auto& m : t.mirrors)
                    sources.append(m);
            }
            QByteArray sha1 = t.sha1.toUtf8();
            m_downloader->addFile(t.savePath, t.name, sources,
                                  t.totalBytes, sha1, false);
        }

        m_downloader->start();
    };

    if (assetIdx.isEmpty()) { startTasks(); return; }

    const QString idxUrl = assetIdx.value(QStringLiteral("url")).toString();
    const QString idxId = assetIdx.value(QStringLiteral("id")).toString(QStringLiteral("legacy"));
    const QString idxPath = m_minecraftDir + QStringLiteral("/assets/indexes/") + idxId + QStringLiteral(".json");

    if (idxUrl.isEmpty() || QFileInfo::exists(idxPath)) { startTasks(); return; }

    emit logMessage(tr("正在下载资源索引..."));
    QDir().mkpath(QFileInfo(idxPath).absolutePath());
    HttpClient::instance().downloadWithFallback(idxUrl, idxPath, nullptr,
        [this, startTasks](bool ok, const QString& err) {
            if (!ok) emit logMessage(tr("资源索引下载失败（已尝试镜像）: %1").arg(err));
            // Ensure startTasks() runs on main thread (HttpClient callback may be async)
            QMetaObject::invokeMethod(this, [startTasks]() { startTasks(); }, Qt::QueuedConnection);
        });
}

// ═══════════════════════════════════════════════════════════
// Lifecycle: pause / resume / cancel
// ═══════════════════════════════════════════════════════════

void VersionDownloader::pause()
{
    if (m_state != Running) return;
    m_state = Paused;
    m_downloader->pause();
    emit stateChanged();
}

void VersionDownloader::resume()
{
    if (m_state != Paused) return;
    m_state = Running;
    m_downloader->resume();
    emit stateChanged();
}

void VersionDownloader::cancel()
{
    if (m_state != Running && m_state != Paused) return;
    m_state = Cancelled;
    m_downloader->cancel();
    emit stateChanged();
}

// ═══════════════════════════════════════════════════════════
// State queries
// ═══════════════════════════════════════════════════════════

int VersionDownloader::completedFiles() const
{
    return m_completedFiles.loadRelaxed();
}

int VersionDownloader::totalFiles() const
{
    return m_totalFiles.loadRelaxed();
}

qint64 VersionDownloader::downloadedBytes() const
{
    return m_downloadedBytes.loadRelaxed();
}

qint64 VersionDownloader::totalBytes() const
{
    return m_totalBytes.loadRelaxed();
}

QString VersionDownloader::stateStr() const
{
    switch (m_state) {
    case Idle:       return QStringLiteral("idle");
    case Running:    return QStringLiteral("running");
    case Paused:     return QStringLiteral("paused");
    case Cancelled:  return QStringLiteral("cancelled");
    case Verifying:  return QStringLiteral("verifying");
    case Done:       return QStringLiteral("done");
    case Failed:     return QStringLiteral("failed");
    }
    return QStringLiteral("idle");
}

bool VersionDownloader::isRunning() const
{
    return m_state == Running;
}

// ═══════════════════════════════════════════════════════════
// allFinished (FileDownloader v8) → adapt to existing logic
// ═══════════════════════════════════════════════════════════

void VersionDownloader::onAllFinishedV2()
{
    if (m_state == Cancelled) {
        emit downloadFinished(false, tr("下载已取消"));
        return;
    }

    int failedCount = m_downloader->failedFiles();
    QStringList failedPaths;
    // TODO: collect failed file paths from fileFinished signals

    if (failedCount > 0) {
        emit downloadFailedFiles(failedPaths);
        emit logMessage(tr("[警告] 下载阶段: %1 个文件下载失败").arg(failedCount));

        const double failRate = static_cast<double>(failedCount) / m_totalFiles.loadRelaxed();
        if (m_fallbackIndex + 1 < m_fallbackChain.size() && failRate >= kFallbackThreshold) {
            const auto& next = m_fallbackChain[m_fallbackIndex + 1];
            emit logMessage(tr("[重试] %1%% 文件下载失败, 切换到 %2 重试...")
                                .arg(static_cast<int>(failRate * 100))
                                .arg(next.name));
            retryWithNextMirror();
            return;
        }
    }

    // --- Integrity verification ---
    m_state = Verifying;
    m_downloadFailedCount = failedCount;
    emit stateChanged();
    emit logMessage(tr("[搜索] 正在进行完整性校验..."));

    QVector<VerifyItem> items = collectVerifyItems(m_currentVersionJson, m_currentVersionId);
    startAsyncVerify(items);
}

// ═══════════════════════════════════════════════════════════
// allFinished → verify → emit downloadFinished
// ═══════════════════════════════════════════════════════════

void VersionDownloader::onAllFinished(bool success, int failedCount,
                                        const QStringList& failedFiles)
{
    if (m_state == Cancelled) {
        emit downloadFinished(false, tr("下载已取消"));
        return;
    }

    // Report failed files to QML (even before verify — allows partial-resume UI)
    if (!failedFiles.isEmpty()) {
        emit downloadFailedFiles(failedFiles);
        emit logMessage(tr("[警告] 下载阶段: %1 个文件下载失败 (已尝试所有镜像)")
                            .arg(failedFiles.size()));

        // ── Mirror fallback: significant failures → retry with next mirror ──
        const double failRate = static_cast<double>(failedCount) / m_totalFiles.loadRelaxed();
        if (m_fallbackIndex + 1 < m_fallbackChain.size() && failRate >= kFallbackThreshold) {
            const auto& next = m_fallbackChain[m_fallbackIndex + 1];
            emit logMessage(tr("[重试] %1%% 文件下载失败, 切换到 %2 重试...")
                                .arg(static_cast<int>(failRate * 100))
                                .arg(next.name));
            retryWithNextMirror();
            return;
        }
    }

    // --- Integrity verification ---
    m_state = Verifying;
    m_downloadFailedCount = failedCount;
    emit stateChanged();
    emit logMessage(tr("[搜索] 正在进行完整性校验..."));

    QVector<VerifyItem> items = collectVerifyItems(m_currentVersionJson, m_currentVersionId);
    startAsyncVerify(items);
}

// ═══════════════════════════════════════════════════════════
// Asset index download (single-file, blocking via QEventLoop)
// ═══════════════════════════════════════════════════════════

bool VersionDownloader::downloadAssetIndex(const QJsonObject& assetIdx)
{
    const QString idxUrl = assetIdx.value(QStringLiteral("url")).toString();
    if (idxUrl.isEmpty()) return false;

    const QString mirrorUrl = buildMirrorUrl(idxUrl, QStringLiteral("meta"));
    const QString idxId = assetIdx.value(QStringLiteral("id")).toString(QStringLiteral("legacy"));
    const QString idxPath = m_minecraftDir + QStringLiteral("/assets/indexes/")
                            + idxId + QStringLiteral(".json");
    QDir().mkpath(QFileInfo(idxPath).absolutePath());

    QStringList urls;
    urls << mirrorUrl;
    if (mirrorUrl != idxUrl)
        urls << idxUrl;

    for (const QString& url : urls) {
        if (m_state == Cancelled) return false;

        QEventLoop loop;
        bool ok = false;
        QString errMsg;

        HttpClient::instance().downloadWithFallback(url, idxPath, nullptr,
            [&](bool success, const QString& error) {
                ok = success;
                errMsg = error;
                loop.quit();
            });

        loop.exec();

        if (ok && QFileInfo::exists(idxPath))
            return true;
    }

    return false;
}

// ═══════════════════════════════════════════════════════════
// Parse local asset index JSON → objects map
// ═══════════════════════════════════════════════════════════

QMap<QString, QJsonObject> VersionDownloader::parseAssetIndex(const QJsonObject& assetIdx)
{
    const QString idxId = assetIdx.value(QStringLiteral("id")).toString(QStringLiteral("legacy"));
    const QString idxPath = m_minecraftDir + QStringLiteral("/assets/indexes/")
                            + idxId + QStringLiteral(".json");

    QFile f(idxPath);
    if (!f.open(QIODevice::ReadOnly)) return {};

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();

    if (!doc.isObject()) return {};

    QJsonObject objects = doc.object().value(QStringLiteral("objects")).toObject();
    QMap<QString, QJsonObject> result;
    for (auto it = objects.begin(); it != objects.end(); ++it)
        result.insert(it.key(), it.value().toObject());

    return result;
}

// ═══════════════════════════════════════════════════════════
// Task collection: client.jar + libraries + assets
// ═══════════════════════════════════════════════════════════

void VersionDownloader::collectTasks(const QJsonObject& versionJson,
                                      const QString& versionId,
                                      const QMap<QString, QJsonObject>& assetObjects,
                                      QVector<DownloadTask>& tasks)
{
    const QString versionDir = m_minecraftDir + QStringLiteral("/versions/") + versionId;

    // --- client.jar ---
    QJsonObject client = versionJson.value(QStringLiteral("downloads"))
                                 .toObject()
                                 .value(QStringLiteral("client"))
                                 .toObject();
    if (!client.value(QStringLiteral("url")).toString().isEmpty()) {
        const QString origUrl = client.value(QStringLiteral("url")).toString();
        const int lastSlash = origUrl.lastIndexOf(QLatin1Char('/'));
        const QString origFileName = (lastSlash >= 0) ? origUrl.mid(lastSlash + 1) : versionId + QStringLiteral(".jar");

        // Unified: always use BMCLAPI /version/ endpoint (works for both launcher
        // and piston-data origins). The old buildMirrorUrl path kept /v1/objects/<sha1>/
        // which contains neither /version/ nor /versions/, causing client.jar to be
        // excluded from sub-step byte counting (cat=-1).
        const QString jarUrl = QStringLiteral("https://%1/version/%2/%3")
                                   .arg(m_mirror.jarHost, versionId, origFileName);

        DownloadTask jarTask;
        jarTask.name      = versionId + QStringLiteral(".jar");
        jarTask.url       = jarUrl;
        jarTask.savePath  = versionDir + QStringLiteral("/") + versionId + QStringLiteral(".jar");
        jarTask.sha1      = client.value(QStringLiteral("sha1")).toString();
        jarTask.totalBytes = static_cast<qint64>(client.value(QStringLiteral("size")).toDouble());

        // Build fallback mirrors: BMCLAPI /version/ first, Mojang direct as last resort
        QStringList jarMirrors;
        jarMirrors << jarUrl;
        if (!jarMirrors.contains(origUrl))
            jarMirrors << origUrl;
        // Also add any alternate mirror /version/ endpoints
        for (const MirrorSource& m : MirrorSource::allMirrors()) {
            if (m.name != m_mirror.name && m.name != tr("Mojang 官方")) {
                QString altUrl = QStringLiteral("https://%1/version/%2/%3")
                                     .arg(m.jarHost, versionId, origFileName);
                if (!jarMirrors.contains(altUrl))
                    jarMirrors << altUrl;
            }
        }
        jarTask.mirrors = jarMirrors;

        tasks.append(jarTask);
        m_taskDestPaths.append(jarTask.savePath);
    }

    // --- libraries ---
    QJsonArray libraries = versionJson.value(QStringLiteral("libraries")).toArray();
    for (const QJsonValue& libVal : libraries) {
        QJsonObject lib = libVal.toObject();
        if (shouldDownloadLibrary(lib))
            addLibraryTasks(lib, tasks);
    }

    // --- asset objects ---
    const QString objectsDir = m_minecraftDir + QStringLiteral("/assets/objects");

    // Build ordered mirror base URL list for assets.
    // Each base includes the full path prefix (e.g. /assets/) — no host-only construction.
    QStringList assetMirrorBases;
    // Primary mirror (selected by user)
    assetMirrorBases << m_mirror.resourceBase;
    // Alternate mirrors, excluding Mojang official
    for (const MirrorSource& m : MirrorSource::allMirrors()) {
        if (m.name != m_mirror.name && m.name != tr("Mojang 官方")) {
            if (!assetMirrorBases.contains(m.resourceBase))
                assetMirrorBases << m.resourceBase;
        }
    }
    // Mojang official always as final fallback
    const QString mojangAssetBase = QStringLiteral("https://resources.download.minecraft.net");

    for (auto it = assetObjects.begin(); it != assetObjects.end(); ++it) {
        const QJsonObject& obj = it.value();
        const QString sha1 = obj.value(QStringLiteral("hash")).toString();
        const QString prefix = sha1.left(2);
        const QString dest = objectsDir + QStringLiteral("/") + prefix
                             + QStringLiteral("/") + sha1;

        QStringList mirrors;
        // All mirror bases: they all use <base>/<prefix>/<hash> pattern
        for (const QString& base : assetMirrorBases) {
            mirrors << QStringLiteral("%1/%2/%3").arg(base, prefix, sha1);
        }
        // Mojang official always as final fallback
        mirrors << QStringLiteral("%1/%2/%3").arg(mojangAssetBase, prefix, sha1);

        DownloadTask assetTask;
        assetTask.name       = sha1;   // assets identified by hash
        assetTask.url        = mirrors.first();
        assetTask.savePath   = dest;
        assetTask.sha1       = sha1;
        assetTask.totalBytes = static_cast<qint64>(obj.value(QStringLiteral("size")).toDouble());
        assetTask.mirrors    = mirrors;

        tasks.append(assetTask);
        m_taskDestPaths.append(dest);
    }

    // ── Log task breakdown ──
    emit logMessage(QString::fromUtf8("收集下载任务: 合计 %1 个文件, 大小 %2")
                        .arg(tasks.size()).arg(formatSize(tasks.size() > 0 ? tasks[0].totalBytes : 0)));
    // ── Pre-compute category totals (for concurrent step display) ──
    m_categoryTotalBytes[0] = 0;
    m_categoryTotalBytes[1] = 0;
    m_categoryTotalBytes[2] = 0;
    for (const auto& t : tasks) {
        // Category 0: version JSON (downloaded via HTTP race in installVersion, not by downloader)
        // Category 1: client.jar + libraries (/version/, /versions/, /libraries/, /maven/)
        // Category 2: assets (/assets/)
        // NOTE: Must NOT use t.name.endsWith(".jar") — library files also end with .jar!
        // Classify by savePath (stable regardless of mirror), not by URL (mirror-dependent)
        if (t.savePath.contains(QStringLiteral("/versions/")) || t.savePath.contains(QStringLiteral("/libraries/"))
            || t.savePath.contains(QStringLiteral("/maven/")))
            m_categoryTotalBytes[1] += t.totalBytes;
        else if (t.savePath.contains(QStringLiteral("/assets/")))
            m_categoryTotalBytes[2] += t.totalBytes;
    }
}

// ═══════════════════════════════════════════════════════════
// Mirror fallback: switch to next mirror and retry
// ═══════════════════════════════════════════════════════════

void VersionDownloader::retryWithNextMirror()
{
    if (m_fallbackIndex + 1 >= m_fallbackChain.size())
        return;

    m_fallbackIndex++;
    const auto& next = m_fallbackChain[m_fallbackIndex];
    setMirror(next);

    // Reset counters
    m_completedFiles.storeRelaxed(0);
    m_downloadedBytes.storeRelaxed(0);
    m_taskDestPaths.clear();

    // Create fresh downloader (old one is in Done/Failed state, can't reuse)
    if (m_downloader) {
        m_downloader->deleteLater();
        m_downloader = nullptr;
    }
    m_downloader = new ShadowDownloader::FileDownloader(this);
    m_downloader->setMaxThreads(m_maxWorkers);
    m_downloader->setSpeedLimitMB(m_downloadCfg.speedLimitMB);
    connect(m_downloader, &ShadowDownloader::FileDownloader::progressChanged,
            this, [this](int completed, int total, qint64 rx, qint64 totalBytes) {
        m_completedFiles.storeRelaxed(completed);
        m_totalFiles.storeRelaxed(total);
        m_downloadedBytes.storeRelaxed(rx);
        m_totalBytes.storeRelaxed(totalBytes);
        emit progressChanged(completed, total, rx, totalBytes);
    });
    connect(m_downloader, &ShadowDownloader::FileDownloader::logMessage,
            this, &VersionDownloader::logMessage);
    connect(m_downloader, &ShadowDownloader::FileDownloader::allFinished,
            this, &VersionDownloader::onAllFinishedV2,
            Qt::QueuedConnection);

    // Re-collect tasks with the new mirror
    QVector<DownloadTask> tasks;
    collectTasks(m_currentVersionJson, m_currentVersionId, m_assetObjects, tasks);
    m_totalFiles.storeRelaxed(tasks.size());

    if (tasks.isEmpty()) {
        m_state = Done;
        emit downloadFinished(true, QString());
        emit stateChanged();
        return;
    }

    qint64 totalEstimate = 0;
    for (const auto& t : tasks) totalEstimate += t.totalBytes;
    m_totalBytes.storeRelaxed(totalEstimate);

    m_state = Running;
    emit stateChanged();
    emit logMessage(tr("开始下载版本 %1 | 镜像=%2").arg(m_currentVersionId, next.name));
    emit logMessage(tr("[重试] 已切换到 %1 (%2/%3), 重新下载 %4 个文件")
                        .arg(next.name)
                        .arg(m_fallbackIndex + 1)
                        .arg(m_fallbackChain.size())
                        .arg(tasks.size()));

    // Feed tasks to FileDownloader
    for (const auto& t : tasks) {
        QStringList sources;
        sources.append(t.url);
        for (const auto& m : t.mirrors) sources.append(m);
        m_downloader->addFile(t.savePath, t.name, sources,
                              t.totalBytes, t.sha1.toUtf8(), false);
    }
    m_downloader->start();
}

// ═══════════════════════════════════════════════════════════
// OS rule filtering for libraries
// ═══════════════════════════════════════════════════════════

bool VersionDownloader::shouldDownloadLibrary(const QJsonObject& lib) const
{
    QJsonArray rules = lib.value(QStringLiteral("rules")).toArray();
    if (rules.isEmpty())
        return true;    // no rules → always download

    for (const QJsonValue& ruleVal : rules) {
        QJsonObject rule = ruleVal.toObject();
        QJsonObject osCond = rule.value(QStringLiteral("os")).toObject();
        QString action = rule.value(QStringLiteral("action")).toString(QStringLiteral("allow"));

        if (osCond.isEmpty()) {
            // No OS condition → rule always matches
            return action == QStringLiteral("allow");
        }

        QString osName = osCond.value(QStringLiteral("name")).toString().toLower();
        bool isMatch = false;

#ifdef Q_OS_WIN
        isMatch = osName.contains(QStringLiteral("windows"));
#elif defined(Q_OS_LINUX)
        isMatch = osName.contains(QStringLiteral("linux"));
#elif defined(Q_OS_MACOS)
        isMatch = (osName.contains(QStringLiteral("osx"))
                   || osName.contains(QStringLiteral("macos")));
#endif

        if (isMatch)
            return action == QStringLiteral("allow");
    }

    return false;
}

// ═══════════════════════════════════════════════════════════
// Library task creation (artifact + native classifiers)
// ═══════════════════════════════════════════════════════════

void VersionDownloader::addLibraryTasks(const QJsonObject& lib,
                                         QVector<DownloadTask>& tasks)
{
    QJsonObject downloads = lib.value(QStringLiteral("downloads")).toObject();

    auto addArtifact = [&](const QJsonObject& art) {
        QString url  = art.value(QStringLiteral("url")).toString();
        QString path = art.value(QStringLiteral("path")).toString();
        if (url.isEmpty() || path.isEmpty()) return;

        DownloadTask task;
        task.name      = QFileInfo(path).fileName();
        task.url       = buildMirrorUrl(url, QStringLiteral("library"));
        task.savePath  = m_minecraftDir + QStringLiteral("/libraries/") + path;
        task.sha1      = art.value(QStringLiteral("sha1")).toString();
        task.totalBytes = static_cast<qint64>(art.value(QStringLiteral("size")).toDouble());

        // Build fallback mirrors for resilience:
        //   mirror[0]: primary (already set as task.url via buildMirrorUrl)
        //   mirror[1..]: alternate mirrors
        //   mirror[last]: Mojang direct (final fallback)
        QStringList libMirrors;
        libMirrors << url;  // Mojang direct (original URL)
        for (const MirrorSource& m : MirrorSource::allMirrors()) {
            if (m.name != m_mirror.name && m.name != tr("Mojang 官方")) {
                // Replace the Mojang library host with the alternate mirror's libraryBase
                QString altUrl = QString(url).replace(
                    QStringLiteral("https://libraries.minecraft.net"),
                    m.libraryBase);
                if (altUrl != task.url && !libMirrors.contains(altUrl))
                    libMirrors << altUrl;
            }
        }
        task.mirrors = libMirrors;

        tasks.append(task);
        m_taskDestPaths.append(task.savePath);
    };

    // Main artifact
    QJsonObject artifact = downloads.value(QStringLiteral("artifact")).toObject();
    if (!artifact.isEmpty())
        addArtifact(artifact);

    // Native classifiers (platform-dependent)
    QJsonObject classifiers = downloads.value(QStringLiteral("classifiers")).toObject();
    if (!classifiers.isEmpty()) {
        for (auto it = classifiers.begin(); it != classifiers.end(); ++it) {
            const QString clsName = it.key().toLower();
            bool match = false;
#ifdef Q_OS_WIN
            match = clsName.contains(QStringLiteral("natives-windows"));
#elif defined(Q_OS_LINUX)
            match = clsName.contains(QStringLiteral("natives-linux"));
#elif defined(Q_OS_MACOS)
            match = (clsName.contains(QStringLiteral("natives-osx"))
                     || clsName.contains(QStringLiteral("natives-macos")));
#endif
            if (match)
                addArtifact(it.value().toObject());
        }
    }
}

// ═══════════════════════════════════════════════════════════
// Mirror URL building
// ═══════════════════════════════════════════════════════════

QString VersionDownloader::buildMirrorUrl(const QString& originalUrl,
                                           const QString& kind) const
{
    if (originalUrl.isEmpty() || m_mirror.name == tr("Mojang 官方"))
        return originalUrl;

    // Replace the full Mojang URL prefix (scheme+host) with the mirror's base URL.
    // Uses startsWith + mid() to preserve trailing paths and avoid double-scheme bugs
    // that occur with plain .replace() on host-only substrings.

    // Jar downloads: launcher.mojang.com → mirror jar host
    const QString mojangJar = QStringLiteral("https://launcher.mojang.com");
    if (originalUrl.startsWith(mojangJar))
        return QStringLiteral("https://%1").arg(m_mirror.jarHost)
               + originalUrl.mid(mojangJar.length());

    // Version metadata: launchermeta.mojang.com → mirror versionMeta host
    const QString mojangMeta = QStringLiteral("https://launchermeta.mojang.com");
    if (originalUrl.startsWith(mojangMeta))
        return QStringLiteral("https://%1").arg(m_mirror.versionMetaHost)
               + originalUrl.mid(mojangMeta.length());

    // Libraries: libraries.minecraft.net → mirror libraryBase (includes /maven/ path)
    const QString mojangLib = QStringLiteral("https://libraries.minecraft.net");
    if (originalUrl.startsWith(mojangLib))
        return m_mirror.libraryBase + originalUrl.mid(mojangLib.length());

    // Resource assets: resources.download.minecraft.net → mirror resourceBase (includes /assets/ path)
    const QString mojangRes = QStringLiteral("https://resources.download.minecraft.net");
    if (originalUrl.startsWith(mojangRes))
        return m_mirror.resourceBase + originalUrl.mid(mojangRes.length());

    return originalUrl;
}

// ═══════════════════════════════════════════════════════════
// Integrity verification (post-download SHA1 scan)
// ═══════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════
// Phase A: Collect verify items (sync, fast — just JSON iteration)
// ═══════════════════════════════════════════════════════════

QVector<VersionDownloader::VerifyItem>
VersionDownloader::collectVerifyItems(const QJsonObject& versionJson,
                                       const QString& versionId)
{
    QVector<VerifyItem> items;
    const QString versionDir = m_minecraftDir + QStringLiteral("/versions/") + versionId;

    // Estimate total for progress bar (won't be exact but gives visual feedback)
    int approxTotal = 1;  // client.jar

    // 1. client.jar
    items.append({versionDir + QStringLiteral("/") + versionId + QStringLiteral(".jar"),
                  QString(),
                  QStringLiteral("versions/%1/%1.jar").arg(versionId)});

    // 2. Libraries
    QJsonArray libraries = versionJson.value(QStringLiteral("libraries")).toArray();
    approxTotal += libraries.size();
    int collected = items.size();
    for (const QJsonValue& lv : libraries) {
        QJsonObject lib = lv.toObject();
        if (!shouldDownloadLibrary(lib)) continue;

        QJsonObject dl = lib.value(QStringLiteral("downloads")).toObject();
        auto addArtifact = [&](const QJsonObject& art) {
            if (!art.isEmpty()) {
                QString path = art.value(QStringLiteral("path")).toString();
                items.append({m_minecraftDir + QStringLiteral("/libraries/") + path,
                              art.value(QStringLiteral("sha1")).toString(),
                              QStringLiteral("libraries/%1").arg(path)});
                collected++;
            }
        };
        addArtifact(dl.value(QStringLiteral("artifact")).toObject());

        QJsonObject classifiers = dl.value(QStringLiteral("classifiers")).toObject();
        for (auto it = classifiers.begin(); it != classifiers.end(); ++it) {
            QString clsName = it.key().toLower();
#ifdef Q_OS_WIN
            if (clsName.contains(QStringLiteral("natives-windows")))
#elif defined(Q_OS_LINUX)
            if (clsName.contains(QStringLiteral("natives-linux")))
#elif defined(Q_OS_MACOS)
            if (clsName.contains(QStringLiteral("natives-osx"))
                || clsName.contains(QStringLiteral("natives-macos")))
#endif
                addArtifact(it.value().toObject());
        }
    }
    emit verifyProgressChanged(collected, approxTotal);
    QCoreApplication::processEvents();

    // 3. Assets from the already-downloaded index
    const QString objectsDir = m_minecraftDir + QStringLiteral("/assets/objects");
    QJsonObject assetIdx = versionJson.value(QStringLiteral("assetIndex")).toObject();
    QString idxId = assetIdx.value(QStringLiteral("id")).toString(QStringLiteral("legacy"));
    QString idxPath = m_minecraftDir + QStringLiteral("/assets/indexes/")
                      + idxId + QStringLiteral(".json");

    if (!assetIdx.isEmpty() && QFileInfo::exists(idxPath)) {
        QFile f(idxPath);
        if (f.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            f.close();
            QJsonObject objs = doc.object().value(QStringLiteral("objects")).toObject();
            approxTotal += objs.size();
            for (auto it = objs.begin(); it != objs.end(); ++it) {
                QJsonObject obj = it.value().toObject();
                QString sha1 = obj.value(QStringLiteral("hash")).toString();
                QString prefix = sha1.left(2);
                items.append({objectsDir + QStringLiteral("/") + prefix + QStringLiteral("/") + sha1,
                              sha1,
                              QStringLiteral("assets/%1").arg(it.key())});
                collected++;
                if (collected % 500 == 0) {
                    emit verifyProgressChanged(collected, approxTotal);
                    QCoreApplication::processEvents();
                }
            }
        }
    }

    int totalItems = items.size();
    qCInfo(logVersion) << QStringLiteral("[校验] 收集到%1个文件待SHA1校验").arg(totalItems);
    // Phase A complete — mark as done, then Phase B starts at 0
    emit verifyProgressChanged(totalItems, totalItems);
    QCoreApplication::processEvents();

    // Notify UI that verification is starting
    emit verifyProgressChanged(0, totalItems);
    QCoreApplication::processEvents();

    return items;
}

// ═══════════════════════════════════════════════════════════
// Phase B: Start async SHA1 verification on worker thread
// ═══════════════════════════════════════════════════════════

void VersionDownloader::startAsyncVerify(const QVector<VerifyItem>& items)
{
    // Clean up previous worker/thread if any
    if (m_verifyWorkerObj) {
        m_verifyWorkerObj->cancel();
        if (m_verifyThread) {
            m_verifyThread->quit();
            m_verifyThread->wait(3000);
            delete m_verifyThread;
            m_verifyThread = nullptr;
        }
        delete m_verifyWorkerObj;
        m_verifyWorkerObj = nullptr;
    }

    m_verifyThread = new QThread(this);
    m_verifyWorkerObj = new AsyncVerifyWorker;
    m_verifyWorkerObj->setItems(items);
    m_verifyWorkerObj->moveToThread(m_verifyThread);

    // Forward progress from worker thread
    QPointer<VersionDownloader> self(this);
    connect(m_verifyWorkerObj, &AsyncVerifyWorker::progressChecked, this,
            [self](int checked, int total) {
                if (!self) return;
                emit self->verifyProgressChanged(checked, total);
            }, Qt::QueuedConnection);

    // Handle cancellation
    connect(m_verifyWorkerObj, &AsyncVerifyWorker::cancelled, this,
            [self](int checked, int total) {
                if (!self) return;
                emit self->verifyProgressChanged(checked, total);
            }, Qt::QueuedConnection);

    // Handle completion
    connect(m_verifyWorkerObj, &AsyncVerifyWorker::finished, this,
            [self](bool allPassed, const QStringList& missedLabels,
                   const QStringList& missedPaths) {
                if (!self) return;
                self->onVerifyFinished(allPassed, missedLabels, missedPaths);
            }, Qt::QueuedConnection);

    // Start worker
    connect(m_verifyThread, &QThread::started,
            m_verifyWorkerObj, &AsyncVerifyWorker::process);
    m_verifyThread->start();
}

// ═══════════════════════════════════════════════════════════
// onVerifyFinished — handle async verify result
// ═══════════════════════════════════════════════════════════

void VersionDownloader::onVerifyFinished(bool allPassed,
                                          const QStringList& missedLabels,
                                          const QStringList& missedPaths)
{
    Q_UNUSED(missedPaths)

    // Cleanup thread — direct delete is safe because quit() + wait()
    // guarantees the thread event loop has stopped before we proceed.
    if (m_verifyThread) {
        m_verifyThread->quit();
        m_verifyThread->wait(3000);
        delete m_verifyThread;
        m_verifyThread = nullptr;
    }
    if (m_verifyWorkerObj) {
        delete m_verifyWorkerObj;
        m_verifyWorkerObj = nullptr;
    }

    if (m_state == Cancelled) {
        emit downloadFinished(false, tr("下载已取消"));
        return;
    }

    // Restore completedFiles so UI shows 100% after verify
    m_completedFiles.storeRelaxed(m_totalFiles.loadRelaxed());

    if (!allPassed) {
        emit logMessage(tr("[警告] 完整性检查: %1 个文件缺失").arg(missedLabels.size()));
        for (int i = 0; i < qMin(missedLabels.size(), 10); ++i)
            emit logMessage(tr("  缺失: %1").arg(missedLabels[i]));
        if (missedLabels.size() > 10)
            emit logMessage(tr("  ... 共 %1 个").arg(missedLabels.size()));

        emit downloadFailedFiles(missedLabels);

        // ── Mirror fallback: missing files → retry with next mirror ──
        if (m_fallbackIndex + 1 < m_fallbackChain.size()) {
            const auto& next = m_fallbackChain[m_fallbackIndex + 1];
            emit logMessage(tr("[重试] 完整性校验未通过 (%1 缺失), 切换到 %2 重试...")
                                .arg(missedLabels.size())
                                .arg(next.name));
            retryWithNextMirror();
            return;
        }

        m_state = Failed;
        emit stateChanged();
        emit downloadFinished(false,
            tr("%1 个文件缺失或校验失败").arg(missedLabels.size()));
        return;
    }

    // Final progress emit
    int totalItems = m_totalFiles.loadRelaxed() > 0
                     ? m_totalFiles.loadRelaxed() : m_completedFiles.loadRelaxed();
    emit verifyProgressChanged(totalItems, totalItems);

    if (m_downloadFailedCount > 0) {
        emit logMessage(tr("[警告] %1 个文件下载失败，但已通过完整性校验").arg(m_downloadFailedCount));
    }

    m_state = Done;
    emit stateChanged();
    emit logMessage(tr("[完成] 下载完成，所有文件通过校验"));
    emit downloadFinished(true, QString());
}

// ═══════════════════════════════════════════════════════════
// Helpers
// ═══════════════════════════════════════════════════════════

bool VersionDownloader::verifySha1(const QString& filePath, const QString& expected)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) return false;

    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(&f);
    f.close();

    return QString::fromLatin1(hash.result().toHex())
        .compare(expected, Qt::CaseInsensitive) == 0;
}

QString VersionDownloader::formatSize(qint64 bytes)
{
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    return QStringLiteral("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
}

void VersionDownloader::emitProgress(const QString& name)
{
    emit progressChanged(m_completedFiles.loadRelaxed(),
                         m_totalFiles.loadRelaxed(),
                         m_downloadedBytes.loadRelaxed(),
                         m_totalBytes.loadRelaxed());
}

// ═══════════════════════════════════════════════════════════
// AsyncVerifyWorker implementation
// ═══════════════════════════════════════════════════════════

QString VersionDownloader::AsyncVerifyWorker::sha1FileFast(const QString& filePath) {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha1);
    while (!f.atEnd()) {
        hash.addData(f.read(65536));
    }
    f.close();
    return QString::fromLatin1(hash.result().toHex());
}

void VersionDownloader::AsyncVerifyWorker::process() {
    const int kEmitInterval = 10;
    m_failed = 0;
    m_failedLabels.clear();
    m_failedPaths.clear();

    for (int i = 0; i < m_items.size(); ++i) {
        if (m_cancelled.loadAcquire()) {
            emit cancelled(i + 1, m_items.size());
            emit finished(false, m_failedLabels, m_failedPaths);
            return;
        }

        const auto& item = m_items[i];
        if (!QFileInfo::exists(item.fullPath)) {
            m_failed++;
            m_failedLabels.append(item.label);
            m_failedPaths.append(item.fullPath);
        } else if (!item.expectedSha1.isEmpty()) {
            QString actual = sha1FileFast(item.fullPath);
            if (actual.compare(item.expectedSha1, Qt::CaseInsensitive) != 0) {
                m_failed++;
                m_failedLabels.append(item.label + QStringLiteral(" (SHA1)"));
                m_failedPaths.append(item.fullPath);
            }
        }

        if ((i + 1) % kEmitInterval == 0 || (i + 1) == m_items.size()) {
            emit progressChecked(i + 1, m_items.size());
        }
    }

    bool allPassed = (m_failed == 0);
    emit finished(allPassed, m_failedLabels, m_failedPaths);
}

} // namespace ShadowLauncher
