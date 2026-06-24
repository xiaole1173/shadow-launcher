// Shadow Launcher — VersionDownloader
// Minecraft version installation pipeline: client.jar + libraries + assets.
// Uses ParallelDownloader for concurrent multi-file downloads and
// HttpClient for single-file asset index retrieval.

#include "version_downloader.h"
#include "parallel_downloader.h"
#include "http_client.h"

#include <QDir>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QEventLoop>
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
    m_downloader = new ParallelDownloader(m_maxWorkers, this);

    // Forward progress signals
    connect(m_downloader, &ParallelDownloader::progressChanged,
            this, [this](int completed, int total, qint64 dlBytes, qint64 totBytes) {
        m_completedFiles.storeRelaxed(completed);
        m_totalFiles.storeRelaxed(total);
        m_downloadedBytes.storeRelaxed(dlBytes);
        m_totalBytes.storeRelaxed(totBytes);
        emit progressChanged(completed, total, dlBytes, totBytes);
    });

    connect(m_downloader, &ParallelDownloader::fileProgress,
            this, &VersionDownloader::fileProgress);

    connect(m_downloader, &ParallelDownloader::logMessage,
            this, &VersionDownloader::logMessage);

    connect(m_downloader, &ParallelDownloader::allFinished,
            this, &VersionDownloader::onAllFinished,
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
    m_maxWorkers = qBound(1, workers, 64);
}

// ═══════════════════════════════════════════════════════════
// Main pipeline: downloadVersion
// ═══════════════════════════════════════════════════════════

void VersionDownloader::downloadVersion(const QJsonObject& versionJson,
                                         const QString& versionId)
{
    if (m_state == Running || m_state == Paused) {
        emit logMessage(QStringLiteral("⚠ 已有下载任务进行中"));
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

    // --- Step 1: Save version JSON to disk ---
    const QString versionDir = m_minecraftDir + QStringLiteral("/versions/") + versionId;
    QDir().mkpath(versionDir);

    const QString jsonPath = versionDir + QStringLiteral("/") + versionId + QStringLiteral(".json");
    QJsonDocument doc(versionJson);
    QFile jsonFile(jsonPath);
    if (jsonFile.open(QIODevice::WriteOnly)) {
        jsonFile.write(doc.toJson(QJsonDocument::Indented));
        jsonFile.close();
        emit logMessage(QStringLiteral("已保存版本清单: %1").arg(jsonPath));
    } else {
        emit logMessage(QStringLiteral("⚠ 无法保存版本清单: %1").arg(jsonPath));
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
        emit logMessage(QStringLiteral("准备下载 %1 个文件").arg(tasks.size()));
        // Set checkpoint dir for resume support
        const QString versionDir = m_minecraftDir + QStringLiteral("/versions/") + versionId;
        m_downloader->setCheckpointDir(versionDir);

        m_downloader->addTasks(tasks);

        // --- Version-level concurrency queue ---
        // Always start — enqueueVersionDownload handles concurrency limit
        ParallelDownloader::enqueueVersionDownload(m_downloader);
        m_downloader->start();
    };

    if (assetIdx.isEmpty()) { startTasks(); return; }

    const QString idxUrl = assetIdx.value(QStringLiteral("url")).toString();
    const QString idxId = assetIdx.value(QStringLiteral("id")).toString(QStringLiteral("legacy"));
    const QString idxPath = m_minecraftDir + QStringLiteral("/assets/indexes/") + idxId + QStringLiteral(".json");

    if (idxUrl.isEmpty() || QFileInfo::exists(idxPath)) { startTasks(); return; }

    emit logMessage(QStringLiteral("正在下载资源索引..."));
    QDir().mkpath(QFileInfo(idxPath).absolutePath());
    HttpClient::instance().downloadWithFallback(idxUrl, idxPath, nullptr,
        [this, startTasks](bool ok, const QString& err) {
            if (!ok) emit logMessage(QStringLiteral("资源索引下载失败（已尝试镜像）: %1").arg(err));
            startTasks();
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
// allFinished → verify → emit downloadFinished
// ═══════════════════════════════════════════════════════════

void VersionDownloader::onAllFinished(bool success, int failedCount,
                                        const QStringList& failedFiles)
{
    if (m_state == Cancelled) {
        emit downloadFinished(false, QStringLiteral("下载已取消"));
        return;
    }

    // Report failed files to QML (even before verify — allows partial-resume UI)
    if (!failedFiles.isEmpty()) {
        emit downloadFailedFiles(failedFiles);
        emit logMessage(QStringLiteral("⚠ 下载阶段: %1 个文件下载失败 (已尝试所有镜像)")
                            .arg(failedFiles.size()));

        // ── Mirror fallback: significant failures → retry with next mirror ──
        const double failRate = static_cast<double>(failedCount) / m_totalFiles.loadRelaxed();
        if (m_fallbackIndex + 1 < m_fallbackChain.size() && failRate >= kFallbackThreshold) {
            const auto& next = m_fallbackChain[m_fallbackIndex + 1];
            emit logMessage(QStringLiteral("🔄 %1%% 文件下载失败, 切换到 %2 重试...")
                                .arg(static_cast<int>(failRate * 100))
                                .arg(next.name));
            retryWithNextMirror();
            return;
        }
    }

    // --- Integrity verification ---
    m_state = Verifying;
    emit stateChanged();
    emit logMessage(QStringLiteral("🔍 正在进行完整性校验..."));

    const QStringList missing = verifyAllFiles(m_currentVersionJson, m_currentVersionId);

    // Restore completedFiles so UI shows 100% after verify
    m_completedFiles.storeRelaxed(m_totalFiles.loadRelaxed());

    if (!missing.isEmpty()) {
        emit logMessage(QStringLiteral("⚠ 完整性检查: %1 个文件缺失").arg(missing.size()));
        for (int i = 0; i < qMin(missing.size(), 10); ++i)
            emit logMessage(QStringLiteral("  缺失: %1").arg(missing[i]));
        if (missing.size() > 10)
            emit logMessage(QStringLiteral("  ... 共 %1 个").arg(missing.size()));

        emit downloadFailedFiles(missing);

        // ── Mirror fallback: missing files → retry with next mirror ──
        if (m_fallbackIndex + 1 < m_fallbackChain.size()) {
            const auto& next = m_fallbackChain[m_fallbackIndex + 1];
            emit logMessage(QStringLiteral("🔄 完整性校验未通过 (%1 缺失), 切换到 %2 重试...")
                                .arg(missing.size())
                                .arg(next.name));
            retryWithNextMirror();
            return;
        }

        m_state = Failed;
        emit stateChanged();
        emit downloadFinished(false,
            QStringLiteral("%1 个文件缺失或校验失败").arg(missing.size()));
        return;
    }

    if (failedCount > 0) {
        emit logMessage(QStringLiteral("⚠ %1 个文件下载失败，但已通过完整性校验").arg(failedCount));
    }

    m_state = Done;
    emit stateChanged();
    emit logMessage(QStringLiteral("✅ 下载完成，所有文件通过校验"));
    emit downloadFinished(true, QString());
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
        DownloadTask jarTask;
        jarTask.name      = versionId + QStringLiteral(".jar");
        jarTask.url       = buildMirrorUrl(origUrl, QStringLiteral("jar"));
        jarTask.savePath  = versionDir + QStringLiteral("/") + versionId + QStringLiteral(".jar");
        jarTask.sha1      = client.value(QStringLiteral("sha1")).toString();
        jarTask.totalBytes = static_cast<qint64>(client.value(QStringLiteral("size")).toDouble());

        // Build fallback mirrors: Mojang direct
        QStringList jarMirrors;
        jarMirrors << origUrl;  // Mojang direct (final fallback)
        for (const MirrorSource& m : MirrorSource::allMirrors()) {
            if (m.name != m_mirror.name && m.name != QStringLiteral("Mojang 官方")) {
                QString altUrl = QString(origUrl).replace(
                    QStringLiteral("launcher.mojang.com"), m.jarHost);
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
        if (m.name != m_mirror.name && m.name != QStringLiteral("Mojang 官方")) {
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

    // ── Pre-compute category totals (for concurrent step display) ──
    m_categoryTotalBytes[0] = 0;
    m_categoryTotalBytes[1] = 0;
    m_categoryTotalBytes[2] = 0;
    for (const auto& t : tasks) {
        if (t.url.contains(QStringLiteral("/versions/"))) 
            m_categoryTotalBytes[0] += t.totalBytes;
        else if (t.url.contains(QStringLiteral("/libraries/")) || t.url.contains(QStringLiteral("/maven/"))) 
            m_categoryTotalBytes[1] += t.totalBytes;
        else if (t.url.contains(QStringLiteral("/assets/"))) 
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
    m_downloader = new ParallelDownloader(m_maxWorkers, this);
    connect(m_downloader, &ParallelDownloader::progressChanged,
            this, [this](int completed, int total, qint64 rx, qint64 totalBytes) {
        m_completedFiles.storeRelaxed(completed);
        m_totalFiles.storeRelaxed(total);
        m_downloadedBytes.storeRelaxed(rx);
        m_totalBytes.storeRelaxed(totalBytes);
        emit progressChanged(completed, total, rx, totalBytes);
    });
    connect(m_downloader, &ParallelDownloader::fileProgress,
            this, &VersionDownloader::fileProgress);
    connect(m_downloader, &ParallelDownloader::logMessage,
            this, &VersionDownloader::logMessage);
    connect(m_downloader, &ParallelDownloader::allFinished,
            this, &VersionDownloader::onAllFinished);

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

    const QString versionDir = m_minecraftDir + QStringLiteral("/versions/") + m_currentVersionId;
    m_downloader->setCheckpointDir(versionDir);
    m_downloader->addTasks(tasks);

    m_state = Running;
    emit stateChanged();
    emit logMessage(QStringLiteral("🔄 已切换到 %1 (%2/%3), 重新下载 %4 个文件")
                        .arg(next.name)
                        .arg(m_fallbackIndex + 1)
                        .arg(m_fallbackChain.size())
                        .arg(tasks.size()));

    ParallelDownloader::enqueueVersionDownload(m_downloader);
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
            if (m.name != m_mirror.name && m.name != QStringLiteral("Mojang 官方")) {
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
    if (originalUrl.isEmpty() || m_mirror.name == QStringLiteral("Mojang 官方"))
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

QStringList VersionDownloader::verifyAllFiles(const QJsonObject& versionJson,
                                               const QString& versionId)
{
    QStringList missing;
    const QString versionDir = m_minecraftDir + QStringLiteral("/versions/") + versionId;

    // --- Count total items for progress ---
    int totalItems = 1;   // jar

    QList<QJsonObject> libArts;
    QJsonArray libraries = versionJson.value(QStringLiteral("libraries")).toArray();
    for (const QJsonValue& lv : libraries) {
        QJsonObject lib = lv.toObject();
        if (!shouldDownloadLibrary(lib)) continue;

        QJsonObject dl = lib.value(QStringLiteral("downloads")).toObject();
        QJsonObject art = dl.value(QStringLiteral("artifact")).toObject();
        if (!art.isEmpty()) libArts.append(art);

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
                libArts.append(it.value().toObject());
        }
    }
    totalItems += libArts.size();

    // Asset count from the already-downloaded index
    QJsonObject assetIdx = versionJson.value(QStringLiteral("assetIndex")).toObject();
    QString idxId = assetIdx.value(QStringLiteral("id")).toString(QStringLiteral("legacy"));
    QString idxPath = m_minecraftDir + QStringLiteral("/assets/indexes/")
                      + idxId + QStringLiteral(".json");

    QMap<QString, QJsonObject> assetObjects;
    int assetCount = 0;
    if (!assetIdx.isEmpty() && QFileInfo::exists(idxPath)) {
        QFile f(idxPath);
        if (f.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            f.close();
            QJsonObject objs = doc.object().value(QStringLiteral("objects")).toObject();
            for (auto it = objs.begin(); it != objs.end(); ++it)
                assetObjects.insert(it.key(), it.value().toObject());
            assetCount = assetObjects.size();
        }
    }
    totalItems += assetCount;

    int checked = 0;

    // Start at 0 so UI sees "verify in progress" immediately
    emit verifyProgressChanged(0, totalItems);
    QCoreApplication::processEvents();

    // --- 1. client.jar ---
    const QString jarPath = versionDir + QStringLiteral("/") + versionId + QStringLiteral(".jar");
    checked++;
    emit verifyProgressChanged(checked, totalItems);
    QCoreApplication::processEvents();
    if (!QFileInfo::exists(jarPath))
        missing.append(QStringLiteral("versions/%1/%1.jar").arg(versionId));

    // --- 2. Libraries ---
    for (const QJsonObject& art : libArts) {
        checked++;
        if (checked % 10 == 0) {
            emit verifyProgressChanged(checked, totalItems);
            QCoreApplication::processEvents();
        }

        QString path = art.value(QStringLiteral("path")).toString();
        QString dest = m_minecraftDir + QStringLiteral("/libraries/") + path;
        if (!QFileInfo::exists(dest)) {
            missing.append(QStringLiteral("libraries/%1").arg(path));
        } else {
            QString sha1 = art.value(QStringLiteral("sha1")).toString();
            if (!sha1.isEmpty() && !verifySha1(dest, sha1))
                missing.append(QStringLiteral("libraries/%1 (SHA1)").arg(path));
        }
    }

    // --- 3. Asset objects ---
    const QString objectsDir = m_minecraftDir + QStringLiteral("/assets/objects");
    int batch = 0;
    for (auto it = assetObjects.begin(); it != assetObjects.end(); ++it) {
        checked++;
        batch++;
        if (batch % 100 == 0) {
            emit verifyProgressChanged(checked, totalItems);
            QCoreApplication::processEvents();
        }

        const QJsonObject& obj = it.value();
        QString sha1 = obj.value(QStringLiteral("hash")).toString();
        QString prefix = sha1.left(2);
        QString dest = objectsDir + QStringLiteral("/") + prefix
                       + QStringLiteral("/") + sha1;
        if (!QFileInfo::exists(dest)) {
            missing.append(QStringLiteral("assets/%1").arg(it.key()));
        } else if (!verifySha1(dest, sha1)) {
            missing.append(QStringLiteral("assets/%1 (SHA1)").arg(it.key()));
        }
    }

    emit verifyProgressChanged(totalItems, totalItems);
    QCoreApplication::processEvents();
    return missing;
}

// ═══════════════════════════════════════════════════════════
// Public integrity verification entry point
// ═══════════════════════════════════════════════════════════

QStringList VersionDownloader::verifyIntegrity(const QJsonObject& versionJson,
                                                const QString& versionId)
{
    return verifyAllFiles(versionJson, versionId);
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

} // namespace ShadowLauncher
