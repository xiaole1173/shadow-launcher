// Shadow Launcher — VersionBackend
// QML-facing backend for version list, installation, and lifecycle management.
// Bridges VersionManager (fetch/cache) and VersionDownloader (install pipeline).

#include "version_backend.h"
#include "../utils/logger.h"
#include "../core/version_manager.h"
#include "../core/version_downloader.h"
#include "../core/version_isolation.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QCryptographicHash>

#ifdef Q_OS_WIN
#include <QClipboard>
#include <QApplication>
#endif

namespace ShadowLauncher {

// ============================================================
// Construction / Destruction
// ============================================================

VersionBackend::VersionBackend(QObject* parent)
    : QObject(parent)
{
    qCInfo(logVersion) << "VersionBackend constructed";

    // ── VersionManager: fetch + cache version manifest ──
    m_versionMgr = new VersionManager(this);
    
    // Manifest ready → populate m_versionIds → notify QML
    connect(m_versionMgr, &VersionManager::versionsReady, this,
            [this](const QVector<McVersion>& versions) {
                m_versionIds.clear();
                for (const auto& v : versions) {
                    m_versionIds.append(v.id);
                }
                emit logMessage(
                    QStringLiteral("版本列表已加载 (%1 个版本)")
                        .arg(m_versionIds.size()));
                emit versionListReady();
            });

    // Manifest fetch error
    connect(m_versionMgr, &VersionManager::fetchError, this,
            [this](const QString& err) {
                emit logMessage(
                    QStringLiteral("获取版本列表失败: %1").arg(err));
            });

    // Defer initial fetch until setGameDir() sets m_dataDir
    // (refreshVersionList needs data dir for cache, refreshInstalled needs game dir)
    m_initialFetchDone = false;
}

VersionBackend::~VersionBackend() = default;

// ============================================================
// Slots — version selection
// ============================================================

void VersionBackend::setGameDir(const QString& dir)
{
    if (m_gameDir != dir) {
        m_gameDir = dir;
        m_versionMgr->setDataDir(dir + QStringLiteral("/.."));
        m_versionMgr->setGameDir(dir);
        // Defer disk I/O to event loop — avoid blocking construction
        QTimer::singleShot(0, this, [this]() {
            refreshInstalled();
            // Initial version list fetch: now that data dir is set, cache works
            if (!m_initialFetchDone) {
                m_initialFetchDone = true;
                refreshVersionList();
            }
        });
    }
}

void VersionBackend::setSelectedVersion(const QString& versionId)
{
    if (m_selectedVersion != versionId) {
        m_selectedVersion = versionId;
        emit selectedVersionChanged();
    }
}

// ============================================================
// Slots — version list
// ============================================================

QVariantList VersionBackend::versionInfoList() const
{
    QVariantList list;
    QVector<McVersion> versions = m_versionMgr->cachedVersions();
    for (const auto& v : versions) {
        QVariantMap m;
        m[QStringLiteral("id")] = v.id;
        m[QStringLiteral("type")] = v.type;
        list.append(m);
    }
    return list;
}

QVector<McVersion> VersionBackend::cachedMcVersions() const
{
    return m_versionMgr->cachedVersions();
}

void VersionBackend::refreshVersionList()
{
    qCInfo(logVersion) << "Refreshing version list";
    emit logMessage(QStringLiteral("正在获取版本列表..."));
    m_versionMgr->fetchVersions();
}

void VersionBackend::refreshInstalled()
{
    updateInstalledList();
    emit installedVersionsChanged();
}

// ============================================================
// Slots — install lifecycle
// ============================================================

void VersionBackend::installVersion(const QString& versionId, int sourceIndex)
{
    // ── Queue: if already installing, enqueue ──
    if (m_installing) {
        // Avoid duplicate queue entries
        for (const auto& entry : m_installQueue) {
            if (entry.first == versionId) {
                emit logMessage(QStringLiteral("⏳ %1 已在下载队列中").arg(versionId));
                return;
            }
        }
        // Don't queue if it's the same as currently installing
        if (m_installVersionId == versionId) {
            emit logMessage(QStringLiteral("⚠ %1 正在下载中").arg(versionId));
            return;
        }
        m_installQueue.enqueue({versionId, sourceIndex});
        emit logMessage(QStringLiteral("⏳ %1 已加入下载队列 (位置 %2)")
                            .arg(versionId)
                            .arg(m_installQueue.size()));
        // Notify property changes for UI
        emit installStateChanged();
        return;
    }

    // ── Check for resume checkpoint files in version dir ──
    const QString versionDir = m_versionMgr->gameDir()
                               + QStringLiteral("/versions/")
                               + versionId;
    const QString progressMarker = versionDir
                                   + QStringLiteral("/.download_progress.json");
    if (QFileInfo::exists(progressMarker)) {
        emit logMessage(QStringLiteral("📦 发现断点续传文件: %1").arg(versionId));
    }

    // Check for individual chunk .checkpoint.json files
    QDir vDir(versionDir);
    const QStringList checkpointFiles = vDir.entryList(
        {QStringLiteral("*.checkpoint.json")},
        QDir::Files | QDir::NoDotAndDotDot);
    if (!checkpointFiles.isEmpty()) {
        emit logMessage(QStringLiteral("📦 发现 %1 个断点续传块文件")
                            .arg(checkpointFiles.size()));
    }

    // ── Resolve mirror source ──
    QVector<MirrorSource> mirrors = MirrorSource::allMirrors();
    MirrorSource mirror = (sourceIndex >= 0 && sourceIndex < mirrors.size())
                              ? mirrors[sourceIndex]
                              : mirrors[0]; // default: BMCLAPI

    // ── Find version metadata in cached list ──
    QVector<McVersion> versions = m_versionMgr->cachedVersions();
    McVersion targetVersion;
    bool found = false;
    for (const auto& v : versions) {
        if (v.id == versionId) {
            targetVersion = v;
            found = true;
            break;
        }
    }

    if (!found) {
        emit logMessage(
            QStringLiteral("未找到版本: %1").arg(versionId));
        return;
    }

    // ── Fetch version JSON using mirror URL (BMCLAPI for China) ──
    auto* nam = new QNetworkAccessManager(this);
    // Apply mirror: replace Mojang host with BMCLAPI
    QString versionJsonUrl = targetVersion.url;
    if (mirror.name.contains(QStringLiteral("BMCLAPI"))) {
        versionJsonUrl.replace(QStringLiteral("launchermeta.mojang.com"),
                              QStringLiteral("bmclapi2.bangbang93.com"));
    }
    QUrl qurl(versionJsonUrl);
    QNetworkRequest req(qurl);
    req.setRawHeader("User-Agent", "ShadowLauncher/1.0");
    req.setTransferTimeout(30000);
    QNetworkReply* reply = nam->get(req);

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, nam, versionId, mirror]() {
                reply->deleteLater();
                nam->deleteLater();

                if (reply->error() != QNetworkReply::NoError) {
                    emit logMessage(
                        QStringLiteral("获取版本信息失败: %1")
                            .arg(reply->errorString()));
                    return;
                }

                QJsonParseError parseErr;
                QJsonDocument doc = QJsonDocument::fromJson(
                    reply->readAll(), &parseErr);

                if (parseErr.error != QJsonParseError::NoError ||
                    !doc.isObject()) {
                    emit logMessage(
                        QStringLiteral("版本信息格式错误: %1")
                            .arg(parseErr.errorString()));
                    return;
                }

                QJsonObject versionJson = doc.object();

                // ── Cancel any running download first ──
                if (m_downloader) {
                    m_downloader->cancel();
                    m_downloader->disconnect();
                    m_downloader->deleteLater();
                    m_downloader = nullptr;
                }

                // ── Create & configure downloader ──
                m_downloader = new VersionDownloader(this);
                m_downloader->setMirror(mirror);
                m_downloader->setMinecraftDir(m_versionMgr->gameDir());
                m_downloader->setMaxWorkers(32);

                // ── Connect downloader signals ──
                connect(m_downloader,
                        &VersionDownloader::progressChanged, this,
                        &VersionBackend::onVersionDownloadProgress);

                connect(m_downloader,
                        &VersionDownloader::fileProgress, this,
                        [this](const QString& fileName,
                               qint64 /*received*/,
                               qint64 /*total*/) {
                            m_installFile = fileName;
                            emit installFileProgress(fileName);
                        });

                connect(m_downloader,
                        &VersionDownloader::verifyProgressChanged, this,
                        [this](int checked, int total) {
                            setInstallPhase(
                                QStringLiteral("校验中..."));
                            m_installProgress = checked;
                            m_installTotal  = total;
                            emit installProgressChanged();
                        });

                connect(m_downloader,
                        &VersionDownloader::logMessage, this,
                        &VersionBackend::onVersionDownloadLog);

                connect(m_downloader,
                        &VersionDownloader::downloadFinished,
                        this,
                        &VersionBackend::onVersionDownloadFinished);

                // ── Create download progress marker for resume detection ──
                const QString markerPath = m_versionMgr->gameDir()
                    + QStringLiteral("/versions/") + versionId
                    + QStringLiteral("/.download_progress.json");
                QDir().mkpath(QFileInfo(markerPath).absolutePath());
                {
                    QFile marker(markerPath);
                    if (marker.open(QIODevice::WriteOnly)) {
                        QJsonObject root;
                        root[QStringLiteral("versionId")] = versionId;
                        root[QStringLiteral("timestamp")]
                            = QDateTime::currentDateTime()
                              .toString(Qt::ISODate);
                        root[QStringLiteral("status")]
                            = QStringLiteral("downloading");
                        marker.write(
                            QJsonDocument(root).toJson());
                        marker.close();
                    }
                }

                // ── Start install ──
                setInstallPhase(QStringLiteral("准备中..."));
                m_installVersionId = versionId;
                setInstalling(true);
                qCInfo(logVersion) << "Install started:" << versionId
                                   << "mirror:" << mirror.name;
                emit logMessage(
                    QStringLiteral("开始安装 %1...").arg(versionId));

                m_downloader->downloadVersion(versionJson,
                                              versionId);
            });
}

void VersionBackend::cancelInstall()
{
    if (m_downloader) {
        m_downloader->cancel();
    }
    setInstalling(false);
    emit logMessage(QStringLiteral("安装已取消"));
}

void VersionBackend::pauseInstall()
{
    if (m_downloader) {
        m_downloader->pause();
        m_installPaused = true;
        emit installPausedChanged(true);
        emit logMessage(QStringLiteral("⏸ 安装已暂停 (断点文件已保留)"));
    }
}

void VersionBackend::resumeInstall()
{
    if (m_downloader) {
        m_downloader->resume();
        m_installPaused = false;
        emit installPausedChanged(false);
        emit logMessage(QStringLiteral("▶ 安装已恢复"));
    }
}

// ============================================================
// Version isolation
// ============================================================

QString VersionBackend::getVersionGameDir(const QString& versionId) const
{
    if (m_isolation) {
        return m_isolation->getVersionGameDir(versionId);
    }
    return m_gameDir;
}

// ============================================================
// Private slots — signal forwarding
// ============================================================

void VersionBackend::onVersionDownloadProgress(int cf, int tf,
                                               qint64 db, qint64 tb)
{
    m_installProgress    = cf;
    m_installTotal       = tf;
    m_installBytesDl     = db;
    m_installBytesTotal  = tb;

    // Speed calculation (bytes/sec)
    if (!m_speedTimer.isValid()) {
        m_speedTimer.start();
        m_lastSpeedBytes = db;
        m_installSpeed = 0;
    } else {
        qint64 elapsedMs = m_speedTimer.elapsed();
        if (elapsedMs >= 500) {
            qint64 deltaBytes = db - m_lastSpeedBytes;
            if (deltaBytes > 0 && elapsedMs > 0) {
                m_installSpeed = deltaBytes * 1000 / elapsedMs;
            } else {
                m_installSpeed = 0;
            }
            m_speedTimer.restart();
            m_lastSpeedBytes = db;
            emit installSpeedChanged(m_installSpeed);
        }
    }

    // Phase detection: if file count > 0 we're downloading
    if (m_installPhase != QStringLiteral("校验中...")) {
        setInstallPhase(QStringLiteral("下载中..."));
    }

    emit installProgressChanged();
    emit installTotalChanged();
    emit installBytesProgress(db, tb);
}

void VersionBackend::onVersionDownloadLog(const QString& msg)
{
    emit logMessage(msg);
}

void VersionBackend::onVersionDownloadFinished(bool success,
                                               const QString& error)
{
    setInstalling(false);
    setInstallPhase(QStringLiteral("idle"));
    // Reset speed tracking
    m_installSpeed = 0;
    m_speedTimer.invalidate();
    m_lastSpeedBytes = 0;
    emit installSpeedChanged(0);
    emit installFinished(success);

    if (success) {
        // Clean up download progress marker on success
        const QString markerPath = m_versionMgr->gameDir()
            + QStringLiteral("/versions/") + m_installVersionId
            + QStringLiteral("/.download_progress.json");
        QFile::remove(markerPath);

        qCInfo(logVersion) << "Install completed:" << m_installVersionId;
        emit logMessage(
            QStringLiteral("✅ %1 安装完成")
                .arg(m_installVersionId));
        refreshInstalled();
    } else if (m_installPhase == QStringLiteral("校验中...")) {
        // Verify failure — keep checkpoint for possible retry
        QString errDetail = error.isEmpty()
                                ? QStringLiteral("校验失败")
                                : error;
        qCCritical(logVersion) << "Verify failed for" << m_installVersionId << ":" << errDetail;
        emit logMessage(
            QStringLiteral("❌ %1 校验失败: %2")
                .arg(m_installVersionId, errDetail));
    } else {
        // Download failure — keep .tmp + checkpoint for resume
        QString errDetail = error.isEmpty()
                                ? QStringLiteral("未知错误")
                                : error;
        qCCritical(logVersion) << "Install failed for" << m_installVersionId << ":" << errDetail;
        emit logMessage(
            QStringLiteral("❌ %1 安装失败: %2")
                .arg(m_installVersionId, errDetail));
    }

    // ── Process next in queue ──
    if (!m_installQueue.isEmpty()) {
        auto next = m_installQueue.dequeue();
        QString nextId = next.first;
        int nextSource = next.second;
        emit logMessage(QStringLiteral("▶ 开始队列中下一个版本: %1").arg(nextId));
        QTimer::singleShot(200, this, [this, nextId, nextSource]() {
            installVersion(nextId, nextSource);
        });
    }
}

void VersionBackend::cancelQueuedDownload(const QString& versionId)
{
    // Remove from queue
    for (int i = 0; i < m_installQueue.size(); ++i) {
        if (m_installQueue[i].first == versionId) {
            m_installQueue.removeAt(i);
            emit logMessage(QStringLiteral("已取消队列中的 %1").arg(versionId));
            emit installStateChanged();
            return;
        }
    }
    // If it's currently installing, cancel entirely
    if (m_installVersionId == versionId && m_installing) {
        cancelInstall();
    }
}

// ============================================================
// Private helpers
// ============================================================

void VersionBackend::updateInstalledList()
{
    m_installedIds.clear();
    if (m_gameDir.isEmpty()) return;

    const QString versionsDir = m_gameDir + QStringLiteral("/versions");
    QDir dir(versionsDir);

    if (!dir.exists()) {
        return;
    }

    const QStringList subDirs =
        dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QString& subDir : subDirs) {
        // Check for {versionId}/{versionId}.jar
        const QString jarPath =
            versionsDir + QStringLiteral("/") + subDir +
            QStringLiteral("/") + subDir + QStringLiteral(".jar");

        if (QFileInfo::exists(jarPath)) {
            m_installedIds.append(subDir);
        }
    }
}

void VersionBackend::setInstalling(bool v)
{
    if (m_installing != v) {
        m_installing = v;
        emit installStateChanged();
    }
}

void VersionBackend::setInstallPhase(const QString& phase)
{
    if (m_installPhase != phase) {
        m_installPhase = phase;
        emit installPhaseChanged(phase);
    }
}

// ============================================================
// VerifyWorker — background SHA1 verification
// ============================================================

QString VersionBackend::VerifyWorker::sha1FileFast(const QString& filePath) {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha1);
    // Read in 64KB chunks to avoid memory spikes on large files
    while (!f.atEnd()) {
        hash.addData(f.read(65536));
    }
    return QString::fromLatin1(hash.result().toHex());
}

void VersionBackend::VerifyWorker::process() {
    const int REPORT_EVERY = 100;
    m_failed = 0;
    m_failedFiles.clear();
    m_failedPaths.clear();

    for (int i = 0; i < m_items.size(); ++i) {
        // Check cancellation
        if (m_cancelled.loadAcquire()) {
            emit cancelled(i + 1, m_items.size());
            emit finished(false, m_failedFiles, m_failedPaths);
            return;
        }

        const auto& item = m_items[i];

        if (!QFileInfo::exists(item.path)) {
            m_failed++;
            m_failedFiles.append(item.name + QStringLiteral(" (缺失)"));
            m_failedPaths.append(item.path);
        } else if (!item.sha1.isEmpty()) {
            QString actual = sha1FileFast(item.path);
            if (actual.compare(item.sha1, Qt::CaseInsensitive) != 0) {
                m_failed++;
                m_failedFiles.append(item.name + QStringLiteral(" (校验失败)"));
                m_failedPaths.append(item.path);
            }
        }

        // Report progress every 100 items
        if ((i + 1) % REPORT_EVERY == 0 || (i + 1) == m_items.size()) {
            emit progressChecked(i + 1, m_items.size());
        }
    }

    bool allPassed = (m_failed == 0);
    emit finished(allPassed, m_failedFiles, m_failedPaths);
}

void VersionBackend::verifyVersion(const QString& versionId)
{
    // Guard: don't allow concurrent verify
    if (m_verifyThread && m_verifyThread->isRunning()) {
        emit logMessage(QStringLiteral("校验已在运行中"));
        return;
    }

    setInstallPhase(QStringLiteral("verifying"));
    m_verifyChecked = 0;
    m_verifyTotal = 0;
    emit verifyStarted();
    qCInfo(logVersion) << "Verify started:" << versionId;
    emit logMessage(QStringLiteral("正在校验版本 %1...").arg(versionId));

    const QString versionDir = m_gameDir + QStringLiteral("/versions/") + versionId;
    const QString jsonPath = versionDir + QStringLiteral("/") + versionId + QStringLiteral(".json");

    // Load version JSON
    QFile jsonFile(jsonPath);
    if (!jsonFile.open(QIODevice::ReadOnly)) {
        emit logMessage(QStringLiteral("❌ 无法读取版本配置: %1").arg(jsonPath));
        setInstallPhase(QStringLiteral("idle"));
        emit verifyFinished(false);
        return;
    }

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(jsonFile.readAll(), &parseErr);
    jsonFile.close();

    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        emit logMessage(QStringLiteral("❌ 版本配置解析失败: %1").arg(parseErr.errorString()));
        setInstallPhase(QStringLiteral("idle"));
        emit verifyFinished(false);
        return;
    }

    QJsonObject versionJson = doc.object();

    // Collect all expected files and their SHA1 hashes
    QVector<VerifyItem> items;

    // --- client.jar ---
    {
        QJsonObject client = versionJson.value(QStringLiteral("downloads"))
                                     .toObject()
                                     .value(QStringLiteral("client"))
                                     .toObject();
        if (!client.isEmpty()) {
            VerifyItem item;
            item.path = versionDir + QStringLiteral("/") + versionId + QStringLiteral(".jar");
            item.sha1 = client.value(QStringLiteral("sha1")).toString();
            item.name = versionId + QStringLiteral(".jar");
            items.append(item);
        }
    }

    // --- Libraries ---
    QString libsDir = m_gameDir + QStringLiteral("/libraries");
    QJsonArray libraries = versionJson.value(QStringLiteral("libraries")).toArray();
    for (const QJsonValue& libVal : libraries) {
        QJsonObject lib = libVal.toObject();

        // Check platform rules — skip if not applicable to Windows
        QJsonArray rules = lib.value(QStringLiteral("rules")).toArray();
        bool allowWindows = true;
        for (const QJsonValue& ruleVal : rules) {
            QJsonObject rule = ruleVal.toObject();
            QString action = rule.value(QStringLiteral("action")).toString();
            QJsonObject os = rule.value(QStringLiteral("os")).toObject();
            QString osName = os.value(QStringLiteral("name")).toString().toLower();
            if (action == QStringLiteral("allow") && !osName.isEmpty() && osName != QStringLiteral("windows")) {
                allowWindows = false;
                break;
            }
            if (action == QStringLiteral("disallow") && (osName.isEmpty() || osName == QStringLiteral("windows"))) {
                allowWindows = false;
                break;
            }
        }
        if (!allowWindows) continue;

        QJsonObject downloads = lib.value(QStringLiteral("downloads")).toObject();

        // Main artifact
        QJsonObject artifact = downloads.value(QStringLiteral("artifact")).toObject();
        if (!artifact.isEmpty()) {
            QString path = artifact.value(QStringLiteral("path")).toString();
            VerifyItem item;
            item.path = libsDir + QStringLiteral("/") + path;
            item.sha1 = artifact.value(QStringLiteral("sha1")).toString();
            item.name = QStringLiteral("libraries/%1").arg(path);
            items.append(item);
        }

        // Native classifiers (Windows)
        QJsonObject classifiers = downloads.value(QStringLiteral("classifiers")).toObject();
        for (auto it = classifiers.begin(); it != classifiers.end(); ++it) {
            if (it.key().toLower().contains(QStringLiteral("natives-windows"))) {
                QJsonObject clsArt = it.value().toObject();
                QString path = clsArt.value(QStringLiteral("path")).toString();
                VerifyItem item;
                item.path = libsDir + QStringLiteral("/") + path;
                item.sha1 = clsArt.value(QStringLiteral("sha1")).toString();
                item.name = QStringLiteral("libraries/%1").arg(path);
                items.append(item);
            }
        }
    }

    // --- Asset objects (from asset index) ---
    QJsonObject assetIdx = versionJson.value(QStringLiteral("assetIndex")).toObject();
    if (!assetIdx.isEmpty()) {
        QString idxId = assetIdx.value(QStringLiteral("id")).toString(QStringLiteral("legacy"));
        QString idxPath = m_gameDir + QStringLiteral("/assets/indexes/") + idxId + QStringLiteral(".json");
        if (QFileInfo::exists(idxPath)) {
            QFile idxFile(idxPath);
            if (idxFile.open(QIODevice::ReadOnly)) {
                QJsonDocument idxDoc = QJsonDocument::fromJson(idxFile.readAll());
                idxFile.close();
                QJsonObject objects = idxDoc.object().value(QStringLiteral("objects")).toObject();
                QString objectsDir = m_gameDir + QStringLiteral("/assets/objects");
                for (auto it = objects.begin(); it != objects.end(); ++it) {
                    QJsonObject obj = it.value().toObject();
                    QString sha1 = obj.value(QStringLiteral("hash")).toString();
                    QString prefix = sha1.left(2);
                    VerifyItem item;
                    item.path = objectsDir + QStringLiteral("/") + prefix + QStringLiteral("/") + sha1;
                    item.sha1 = sha1;
                    item.name = QStringLiteral("assets/%1").arg(it.key());
                    items.append(item);
                }
            }
        }
    }

    const int total = items.size();
    m_verifyTotal = total;
    emit logMessage(QStringLiteral("校验 %1 个文件...").arg(total));
    emit verifyProgress(0, total);

    // ── Create worker + thread ──
    m_verifyThread = new QThread(this);
    m_verifyWorker = new VerifyWorker;
    m_verifyWorker->setItems(items);
    m_verifyWorker->moveToThread(m_verifyThread);

    // Progress: forward to QML
    connect(m_verifyWorker, &VerifyWorker::progressChecked, this,
            [this](int checked, int total) {
                m_verifyChecked = checked;
                m_verifyTotal = total;
                emit verifyProgress(checked, total);
            }, Qt::QueuedConnection);

    // Cancelled
    connect(m_verifyWorker, &VerifyWorker::cancelled, this,
            [this](int checked, int total) {
                m_verifyChecked = checked;
                m_verifyTotal = total;
                emit verifyProgress(checked, total);
                emit logMessage(QStringLiteral("⚠ 校验已取消"));
                setInstallPhase(QStringLiteral("idle"));
                emit verifyCancelled();
            }, Qt::QueuedConnection);

    // Finished
    connect(m_verifyWorker, &VerifyWorker::finished, this,
            [this](bool allPassed, const QStringList& failedFiles,
                   const QStringList& failedPaths) {
                if (allPassed) {
                    qCInfo(logVersion) << "Verify completed — all" << m_verifyTotal << "files passed";
                    emit logMessage(QStringLiteral("✅ 校验完成: %1 个文件全部通过").arg(m_verifyTotal));
                } else {
                    int failed = failedFiles.size();
                    qCCritical(logVersion) << "Verify completed —" << failed << "/" << m_verifyTotal << "files failed";

                    QString detail = failedFiles.join(QStringLiteral(", "));
                    if (detail.length() > 250) {
                        detail = detail.left(250) + QStringLiteral("...");
                    }
                    emit logMessage(QStringLiteral("❌ 校验完成: %1/%2 个文件失败").arg(failed).arg(m_verifyTotal));
                    emit logMessage(QStringLiteral("   失败文件: %1").arg(detail));
                    emit verifyFailedFiles(failedFiles);

                    // Store failed paths for later cleanup
                    m_failedPathsCache = failedPaths;
                }

                setInstallPhase(QStringLiteral("idle"));
                emit verifyFinished(allPassed);

                // Cleanup worker + thread
                m_verifyWorker->deleteLater();
                m_verifyWorker = nullptr;
                if (m_verifyThread) {
                    m_verifyThread->quit();
                    m_verifyThread->wait(3000);
                    m_verifyThread->deleteLater();
                    m_verifyThread = nullptr;
                }
            }, Qt::QueuedConnection);

    // Start worker
    connect(m_verifyThread, &QThread::started,
            m_verifyWorker, &VerifyWorker::process);
    connect(m_verifyThread, &QThread::finished,
            m_verifyWorker, &QObject::deleteLater);
    m_verifyThread->start();
}

void VersionBackend::cancelVerify()
{
    if (m_verifyWorker) {
        m_verifyWorker->cancel();
        emit logMessage(QStringLiteral("正在取消校验..."));
    }
}

// ============================================================
// Version management: clean corrupt files
// ============================================================

void VersionBackend::cleanCorruptVersion(const QString& versionId)
{
    if (m_failedPathsCache.isEmpty()) {
        emit logMessage(QStringLiteral("没有可清理的校验结果"));
        return;
    }

    int cleaned = 0;
    int removed = 0;

    for (const QString& path : m_failedPathsCache) {
        if (QFileInfo::exists(path)) {
            if (QFile::remove(path)) {
                removed++;
            }
        } else {
            cleaned++; // already gone
        }
    }

    m_failedPathsCache.clear();
    emit logMessage(QStringLiteral("🧹 清理完成: 删除 %1 个损坏文件, %2 个文件已不存在")
                        .arg(removed).arg(cleaned));
    emit logMessage(QStringLiteral("💡 请重新下载版本 %1 以恢复缺失文件").arg(versionId));
}

void VersionBackend::repairVersion(const QString& versionId)
{
    if (m_failedPathsCache.isEmpty()) {
        emit logMessage(QStringLiteral("没有需要修复的文件"));
        return;
    }

    int removed = 0;
    for (const QString& path : m_failedPathsCache) {
        if (QFileInfo::exists(path) && QFile::remove(path)) {
            removed++;
        }
    }

    emit logMessage(QStringLiteral("🔧 修复中: 已删除 %1 个损坏文件，开始重新下载...").arg(removed));

    // 清除缓存，避免 cleanCorruptVersion 重复删除
    m_failedPathsCache.clear();

    // 重新下载版本 — 下载器会跳过 SHA1 匹配的文件，仅下载缺失/损坏的
    if (m_installing && m_installVersionId == versionId) {
        // 已在下载同一版本，先取消再重启
        cancelInstall();
        // 稍等取消完成后再安装
        QTimer::singleShot(200, this, [this, versionId]() {
            installVersion(versionId);
        });
    } else {
        installVersion(versionId);
    }
}

// ============================================================
// Version management: rename
// ============================================================

bool VersionBackend::renameVersion(const QString& oldId, const QString& newId)
{
    if (oldId.isEmpty() || newId.isEmpty()) {
        emit logMessage(QStringLiteral("重命名失败: ID不能为空"));
        return false;
    }

    const QString versionsDir = m_gameDir + QStringLiteral("/versions");
    const QString oldDir = versionsDir + QStringLiteral("/") + oldId;
    const QString newDir = versionsDir + QStringLiteral("/") + newId;

    if (!QDir(oldDir).exists()) {
        emit logMessage(QStringLiteral("重命名失败: 版本 %1 不存在").arg(oldId));
        return false;
    }

    if (QDir(newDir).exists()) {
        emit logMessage(QStringLiteral("重命名失败: 目标 %1 已存在").arg(newId));
        return false;
    }

    // Rename directory
    if (!QDir().rename(oldDir, newDir)) {
        emit logMessage(QStringLiteral("重命名失败: 无法重命名目录"));
        return false;
    }

    // Rename JAR file inside
    const QString oldJar = newDir + QStringLiteral("/") + oldId + QStringLiteral(".jar");
    const QString newJar = newDir + QStringLiteral("/") + newId + QStringLiteral(".jar");
    if (QFileInfo::exists(oldJar)) {
        QFile::rename(oldJar, newJar);
    }

    // Rename JSON file inside
    const QString oldJson = newDir + QStringLiteral("/") + oldId + QStringLiteral(".json");
    const QString newJson = newDir + QStringLiteral("/") + newId + QStringLiteral(".json");
    if (QFileInfo::exists(oldJson)) {
        QFile::rename(oldJson, newJson);
    }

    emit logMessage(QStringLiteral("版本已重命名: %1 → %2").arg(oldId, newId));
    refreshInstalled();
    return true;
}

// ============================================================
// Version management: clone
// ============================================================

static bool copyDirRecursive(const QString& srcPath, const QString& dstPath)
{
    QDir srcDir(srcPath);
    if (!srcDir.exists()) return false;

    QDir().mkpath(dstPath);

    for (const QString& entry : srcDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (!copyDirRecursive(srcPath + QStringLiteral("/") + entry,
                               dstPath + QStringLiteral("/") + entry))
            return false;
    }

    for (const QString& entry : srcDir.entryList(QDir::Files)) {
        QString srcFile = srcPath + QStringLiteral("/") + entry;
        QString dstFile = dstPath + QStringLiteral("/") + entry;
        if (!QFile::copy(srcFile, dstFile)) return false;
    }

    return true;
}

bool VersionBackend::cloneVersion(const QString& sourceId, const QString& newId)
{
    if (sourceId.isEmpty() || newId.isEmpty()) {
        emit logMessage(QStringLiteral("克隆失败: ID不能为空"));
        return false;
    }

    const QString versionsDir = m_gameDir + QStringLiteral("/versions");
    const QString srcDir = versionsDir + QStringLiteral("/") + sourceId;
    const QString dstDir = versionsDir + QStringLiteral("/") + newId;

    if (!QDir(srcDir).exists()) {
        emit logMessage(QStringLiteral("克隆失败: 源版本 %1 不存在").arg(sourceId));
        return false;
    }

    if (QDir(dstDir).exists()) {
        emit logMessage(QStringLiteral("克隆失败: 目标 %1 已存在").arg(newId));
        return false;
    }

    // Recursively copy
    if (!copyDirRecursive(srcDir, dstDir)) {
        emit logMessage(QStringLiteral("克隆失败: 复制过程中出错"));
        return false;
    }

    // Rename JAR and JSON inside clone
    const QString oldJar = dstDir + QStringLiteral("/") + sourceId + QStringLiteral(".jar");
    const QString newJar = dstDir + QStringLiteral("/") + newId + QStringLiteral(".jar");
    if (QFileInfo::exists(oldJar)) {
        QFile::rename(oldJar, newJar);
    }

    const QString oldJson = dstDir + QStringLiteral("/") + sourceId + QStringLiteral(".json");
    const QString newJson = dstDir + QStringLiteral("/") + newId + QStringLiteral(".json");
    if (QFileInfo::exists(oldJson)) {
        QFile::rename(oldJson, newJson);
    }

    emit logMessage(QStringLiteral("版本已克隆: %1 → %2").arg(sourceId, newId));
    refreshInstalled();
    return true;
}

// ============================================================
// Version management: copy version path
// ============================================================

QString VersionBackend::copyVersionPath(const QString& versionId)
{
    const QString path = m_gameDir + QStringLiteral("/versions/") + versionId;

#ifdef Q_OS_WIN
    QClipboard* clipboard = QApplication::clipboard();
    if (clipboard) {
        clipboard->setText(QDir::toNativeSeparators(path));
    }
#endif

    emit logMessage(QStringLiteral("已复制路径: %1").arg(QDir::toNativeSeparators(path)));
    return QDir::toNativeSeparators(path);
}

} // namespace ShadowLauncher
