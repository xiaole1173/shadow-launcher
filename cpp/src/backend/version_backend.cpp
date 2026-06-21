// Shadow Launcher — VersionBackend
// QML-facing backend for version list, installation, and lifecycle management.
// Bridges VersionManager (fetch/cache) and VersionDownloader (install pipeline).

#include "version_backend.h"
#include "../utils/logger.h"
#include "../core/version_manager.h"
#include "../core/version_downloader.h"
#include "../core/version_isolation.h"
#include "../core/mod_loader_installer.h"

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

    // Throttle activeInstallsChanged to 300ms intervals (avoid flicker)
    m_activeInstallsThrottle.setSingleShot(true);
    m_activeInstallsThrottle.setInterval(300);
    connect(&m_activeInstallsThrottle, &QTimer::timeout, this, [this]() {
        if (m_activeInstallsPending) {
            m_activeInstallsPending = false;
            emitActiveInstallsChanged();
        }
    });
    m_stepChangeTime.start();

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

    // ModLoaderInstaller init
    m_mlInstaller = new ModLoaderInstaller(this);
    connect(m_mlInstaller, &ModLoaderInstaller::progressChanged, this,
            [this](int step, int total, const QString& desc) {
        m_installProgress = step;
        m_installTotal = total;
        emit installProgressChanged();
        setInstallPhase(QStringLiteral("模组加载器: ") + desc);
        // Update step model: current step active
        // In merged mode, steps are offset by 3 (MC steps 0-2)
        int stepIdx = m_isMergedInstall ? (step - 1 + 3) : (step - 1);
        updateStep(stepIdx, QStringLiteral("active"), 0);
    });
    connect(m_mlInstaller, &ModLoaderInstaller::finished, this,
            [this](bool success, const QString& error) {
        if (success) {
            // Mark all steps completed
            if (m_isMergedInstall) {
                for (int i = 0; i < m_installSteps.size(); i++)
                    updateStep(i, QStringLiteral("completed"), 100);
                // Merged install complete — clean up
                m_isMergedInstall = false;
                m_mergedMcVersion.clear();
                m_mergedLoaderType.clear();
                m_mergedLoaderVer.clear();
            } else {
                for (int i = 0; i < m_installSteps.size(); i++)
                    updateStep(i, QStringLiteral("completed"), 100);
            }
            emit logMessage(QStringLiteral("[ModLoader] 安装完成"));
            setInstallPhase("done");
            updateInstalledList();
        } else {
            emit logMessage(QStringLiteral("[ModLoader] 安装失败: ") + error);
            // Mark last step as failed, keep card visible for diagnostics
            for (int i = m_installSteps.size() - 1; i >= 0; i--) {
                QVariantMap s = m_installSteps[i].toMap();
                if (s["status"].toString() == QStringLiteral("active")) {
                    s["status"] = QStringLiteral("failed");
                    s["percentage"] = 0;
                    m_installSteps[i] = s;
                    m_installFailed = true;
                    m_installError = error;
                    emit installStepsChanged();
                    break;
                }
            }
            if (m_isMergedInstall) {
                m_isMergedInstall = false;
                m_mergedMcVersion.clear();
                m_mergedLoaderType.clear();
                m_mergedLoaderVer.clear();
            }
        }
        setInstalling(false);
        emit installFinished(success);
    });

    // Pause between verify and install (merged install: wait for MC)
    connect(m_mlInstaller, &ModLoaderInstaller::waitingForMC, this, [this]() {
        setInstallPhase(QStringLiteral("等待MC下载完成..."));
    });

    // Byte-level download progress → update current active step
    connect(m_mlInstaller, &ModLoaderInstaller::byteProgress, this,
            [this](const QString& file, qint64 received, qint64 total, qint64 speed) {
        m_installFile = file;
        m_installBytesDl = received;
        m_installBytesTotal = total;
        m_installSpeed = speed;
        emit installFileProgress(file);
        // Update current active step with byte progress
        for (int i = 0; i < m_installSteps.size(); i++) {
            if (m_installSteps[i].toMap()["status"].toString() == QStringLiteral("active")) {
                int pct = total > 0 ? (int)(received * 100 / total) : 0;
                updateStep(i, QStringLiteral("active"), pct, received, total);
                break;
            }
        }
    });

    // Verification
    connect(m_mlInstaller, &ModLoaderInstaller::verifyStarted, this,
            [this]() {
        setInstallPhase("verifying");
        m_verifyChecked = 0;
        m_verifyTotal = 1;
        emit verifyStarted();
        emit verifyProgress(0, 1);
    });
    connect(m_mlInstaller, &ModLoaderInstaller::verifyFinished, this,
            [this](bool ok) {
        Q_UNUSED(ok);
        m_verifyChecked = 1;
        emit verifyProgress(1, 1);
        emit verifyFinished(ok);
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
    // ── Check if already active ──
    if (m_activeIds.contains(versionId)) {
        emit logMessage(QStringLiteral("⚠ %1 正在下载中").arg(versionId));
        return;
    }

    // ── Check queue for duplicates ──
    for (const auto& entry : m_installQueue) {
        if (entry.first == versionId) {
            emit logMessage(QStringLiteral("⏳ %1 已在下载队列中").arg(versionId));
            return;
        }
    }

    // ── Queue: if at max concurrency, enqueue ──
    if (m_activeCount >= MAX_CONCURRENT) {
        m_installQueue.enqueue({versionId, sourceIndex});
        qCDebug(logLaunch) << "[DOWNLOAD] enqueued=" << versionId << "pos=" << m_installQueue.size() << "active=" << m_activeCount;
        emit logMessage(QStringLiteral("⏳ %1 已加入下载队列 (位置 %2)")
                            .arg(versionId)
                            .arg(m_installQueue.size()));
        emit installStateChanged();
        emit downloadQueueChanged();
        return;
    }

    qCDebug(logLaunch) << "[DOWNLOAD] starting=" << versionId << "active=" << m_activeCount << "/" << MAX_CONCURRENT;

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

    // ── Set installing state BEFORE async version JSON fetch ──
    // Critical: the UI must react immediately on click, not wait 500ms+ network RTT
    m_activeCount++;
    m_activeIds.append(versionId);
    setInstallPhase(QStringLiteral("准备中..."));
    setInstalling(true);
    emit logMessage(QStringLiteral("正在获取 %1 版本信息...").arg(versionId));
    qCDebug(logLaunch) << "[DOWNLOAD] state-set=" << versionId
                        << "active=" << m_activeCount << "/" << MAX_CONCURRENT;

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
                    // Rollback: version JSON fetch failed
                    emit logMessage(
                        QStringLiteral("获取版本信息失败: %1")
                            .arg(reply->errorString()));
                    cancelActiveDownload(versionId);
                    return;
                }

                QJsonParseError parseErr;
                QJsonDocument doc = QJsonDocument::fromJson(
                    reply->readAll(), &parseErr);

                if (parseErr.error != QJsonParseError::NoError ||
                    !doc.isObject()) {
                    // Rollback: JSON parse failed
                    emit logMessage(
                        QStringLiteral("版本信息格式错误: %1")
                            .arg(parseErr.errorString()));
                    cancelActiveDownload(versionId);
                    return;
                }

                QJsonObject versionJson = doc.object();

                // ── Cancel any existing downloader for this version ──
                if (m_downloaders.contains(versionId)) {
                    m_downloaders[versionId]->cancel();
                    m_downloaders[versionId]->disconnect();
                    m_downloaders[versionId]->deleteLater();
                    m_downloaders.remove(versionId);
                }

                // ── Create & configure downloader ──
                auto* downloader = new VersionDownloader(this);
                downloader->setMirror(mirror);
                downloader->setMinecraftDir(m_versionMgr->gameDir());
                downloader->setMaxWorkers(32);

                // ── Connect downloader signals (lambdas capture versionId) ──
                connect(downloader,
                        &VersionDownloader::progressChanged, this,
                        [this, versionId](int cf, int tf, qint64 db, qint64 tb) {
                            updateDownloadProgress(versionId, cf, tf, db, tb);
                        });

                connect(downloader,
                        &VersionDownloader::fileProgress, this,
                        [this, versionId](const QString& fileName,
                               qint64 received,
                               qint64 total) {
                            updateDownloadFile(versionId, fileName, received, total);
                        });

                connect(downloader,
                        &VersionDownloader::verifyProgressChanged, this,
                        [this, versionId](int checked, int total) {
                            // Update per-download state
                            auto& st = m_dlStates[versionId];
                            st.phase = QStringLiteral("校验中...");
                            st.progress = checked;
                            st.total = total;
                            // Sync primary
                            syncPrimaryProgress();
                        });

                connect(downloader,
                        &VersionDownloader::logMessage, this,
                        &VersionBackend::onVersionDownloadLog);

                connect(downloader,
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

                // ── Install started (state already set synchronously above) ──
                qCInfo(logVersion) << "Install started:" << versionId
                                   << "mirror:" << mirror.name
                                   << "(active:" << m_activeCount << "/" << MAX_CONCURRENT << ")";

                // ── Track downloader in map ──
                m_downloaders[versionId] = downloader;

                downloader->downloadVersion(versionJson,
                                              versionId);
            });
}

void VersionBackend::cancelInstall()
{
    // ── Cancel all active downloads ──
    for (auto it = m_downloaders.begin(); it != m_downloaders.end(); ++it) {
        it.value()->cancel();
        it.value()->disconnect();
        it.value()->deleteLater();
    }
    m_downloaders.clear();
    m_dlStates.clear();
    m_activeIds.clear();
    m_activeCount = 0;
    setInstalling(false);
    emit logMessage(QStringLiteral("所有安装已取消"));
}

void VersionBackend::cancelActiveDownload(const QString& versionId)
{
    // Rollback an active download that has no downloader yet
    // (used when version JSON fetch fails before downloader is created)
    if (m_downloaders.contains(versionId)) {
        m_downloaders[versionId]->cancel();
        m_downloaders[versionId]->disconnect();
        m_downloaders[versionId]->deleteLater();
        m_downloaders.remove(versionId);
    }
    m_dlStates.remove(versionId);
    m_activeIds.removeOne(versionId);
    if (m_activeCount > 0) m_activeCount--;
    if (m_activeCount == 0) {
        setInstalling(false);
        setInstallPhase(QStringLiteral("idle"));
    }
    emit installStateChanged();
}

void VersionBackend::cancelInstall(const QString& versionId)
{
    if (!m_downloaders.contains(versionId)) {
        // Check queue
        for (int i = 0; i < m_installQueue.size(); ++i) {
            if (m_installQueue[i].first == versionId) {
                m_installQueue.removeAt(i);
                emit logMessage(QStringLiteral("已取消队列中的 %1").arg(versionId));
                emit installStateChanged();
                emit downloadQueueChanged();
                return;
            }
        }
        return;  // Not found
    }

    // Cancel and cleanup
    m_downloaders[versionId]->cancel();
    m_downloaders[versionId]->disconnect();
    m_downloaders[versionId]->deleteLater();
    m_downloaders.remove(versionId);
    m_dlStates.remove(versionId);
    m_activeIds.removeOne(versionId);
    m_activeCount--;

    emit logMessage(QStringLiteral("已取消 %1 的安装").arg(versionId));

    // Update installing state
    if (m_activeCount == 0) {
        setInstalling(false);
    }

    // Sync primary progress display
    syncPrimaryProgress();

    // Try to start next from queue
    startNextFromQueue();
}

void VersionBackend::pauseInstall()
{
    auto* dl = primaryDownloader();
    if (dl) {
        dl->pause();
        m_installPaused = true;
        emit installPausedChanged(true);
        emit logMessage(QStringLiteral("⏸ 安装已暂停 (断点文件已保留)"));
    }
}

void VersionBackend::resumeInstall()
{
    auto* dl = primaryDownloader();
    if (dl) {
        dl->resume();
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

// NOTE: This slot is no longer directly connected (lambdas call updateDownloadProgress instead).
// Kept for backward compatibility if anything else connects to it.
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
    // ── Identify which downloader finished ──
    auto* finishedDl = qobject_cast<VersionDownloader*>(sender());
    if (!finishedDl) {
        qCWarning(logVersion) << "onVersionDownloadFinished: unknown sender";
        return;
    }

    // Find version ID for this downloader
    QString finishedId;
    for (auto it = m_downloaders.begin(); it != m_downloaders.end(); ++it) {
        if (it.value() == finishedDl) {
            finishedId = it.key();
            break;
        }
    }

    if (finishedId.isEmpty()) {
        qCWarning(logVersion) << "onVersionDownloadFinished: downloader not found in map";
        return;
    }

    // ── Cleanup this downloader ──
    finishedDl->disconnect();
    finishedDl->deleteLater();
    m_downloaders.remove(finishedId);
    m_dlStates.remove(finishedId);
    m_activeIds.removeOne(finishedId);
    m_activeCount--;

    // ── Handle result ──
    if (success) {
        // Clean up download progress marker on success
        const QString markerPath = m_versionMgr->gameDir()
            + QStringLiteral("/versions/") + finishedId
            + QStringLiteral("/.download_progress.json");
        QFile::remove(markerPath);

        qCInfo(logVersion) << "Install completed:" << finishedId;
        emit logMessage(
            QStringLiteral("✅ %1 安装完成")
                .arg(finishedId));
        refreshInstalled();
    } else {
        const auto& st = m_dlStates.value(finishedId);
        bool wasVerifying = (st.phase == QStringLiteral("校验中..."));
        if (wasVerifying) {
            QString errDetail = error.isEmpty()
                                    ? QStringLiteral("校验失败")
                                    : error;
            qCCritical(logVersion) << "Verify failed for" << finishedId << ":" << errDetail;
            emit logMessage(
                QStringLiteral("❌ %1 校验失败: %2")
                    .arg(finishedId, errDetail));
        } else {
            QString errDetail = error.isEmpty()
                                    ? QStringLiteral("未知错误")
                                    : error;
            qCCritical(logVersion) << "Install failed for" << finishedId << ":" << errDetail;
            emit logMessage(
                QStringLiteral("❌ %1 安装失败: %2")
                    .arg(finishedId, errDetail));
        }
    }

    // ── Emit finished signal (per-download) ──
    emit installFinished(success);

    // ── Update overall state ──
    // ── Update overall state ──
    if (m_activeCount == 0) {
        if (m_isMergedInstall && success && m_mergedMcVersion == finishedId) {
            // Merged install: MC done, transitioning to loader — keep card alive
            qDebug() << "[install] Merged: MC download complete, starting" << m_mergedLoaderType;
            // Mark MC steps (0-2) as completed
            for (int i = 0; i < 3 && i < m_installSteps.size(); i++) {
                updateStep(i, QStringLiteral("completed"), 100);
            }
            // Start loader install
            m_mlInstaller->setGameDir(m_gameDir);
            if (m_mergedLoaderType == QStringLiteral("forge")) {
                m_mlInstaller->installForge(m_mergedMcVersion, m_mergedLoaderVer, m_modLoaderInstallId);
            } else if (m_mergedLoaderType == QStringLiteral("neoforge")) {
                m_mlInstaller->installNeoForge(m_mergedMcVersion, m_mergedLoaderVer, m_modLoaderInstallId);
            } else if (m_mergedLoaderType == QStringLiteral("fabric")) {
                m_mlInstaller->installFabric(m_mergedMcVersion, m_mergedLoaderVer, m_modLoaderInstallId);
            }
            // ModLoaderInstaller progress will update merged steps via connectModLoaderSignals
        } else {
            setInstalling(false);
        }
        setInstallPhase(QStringLiteral("done"));
        m_installSpeed = 0;
        m_speedTimer.invalidate();
        m_lastSpeedBytes = 0;
        emit installSpeedChanged(0);
        emit logMessage(QStringLiteral("🎉 所有版本安装完成！"));
    } else {
        // Still have active downloads — sync primary display AND notify QML to re-read
        syncPrimaryProgress();
        emit installStateChanged();
    }

    // ── Try to start next from queue ──
    startNextFromQueue();
    qCDebug(logLaunch) << "[DOWNLOAD] finished=" << finishedId << " active=" << m_activeCount << "/" << MAX_CONCURRENT << " queue=" << m_installQueue.size();

    // Check if a pending loader is waiting for this MC version
    if (m_hasPendingLoader && success && m_pendingLoaderMc == finishedId) {
        qDebug() << "[install] MC" << finishedId << "installed, starting pending loader:" << m_pendingLoaderName;
        m_hasPendingLoader = false;
        if (m_pendingLoaderType == QStringLiteral("optifine")) {
            installOptifine(m_pendingLoaderMc, m_pendingLoaderVer, QString(), m_pendingLoaderName);
        } else {
            installModLoader(m_pendingLoaderMc, m_pendingLoaderType, m_pendingLoaderVer, m_pendingLoaderName);
        }
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
            emit downloadQueueChanged();
            return;
        }
    }
    // If it's currently installing, cancel that specific one
    if (m_activeIds.contains(versionId)) {
        cancelInstall(versionId);
    }
}

QVariantList VersionBackend::downloadQueue() const
{
    QVariantList list;
    for (int i = 0; i < m_installQueue.size(); ++i) {
        QVariantMap entry;
        entry[QStringLiteral("versionId")] = m_installQueue[i].first;
        entry[QStringLiteral("position")] = i + 1;
        list.append(entry);
    }
    return list;
}

QVariantList VersionBackend::activeDownloads() const
{
    QVariantList list;
    for (const auto& id : m_activeIds) {
        QVariantMap entry;
        entry[QStringLiteral("versionId")] = id;
        const auto& st = m_dlStates.value(id);
        entry[QStringLiteral("progress")] = st.progress;
        entry[QStringLiteral("total")] = st.total;
        entry[QStringLiteral("bytesDownloaded")] = st.bytesDl;
        entry[QStringLiteral("bytesTotal")] = st.bytesTotal;
        entry[QStringLiteral("speed")] = st.speed;
        entry[QStringLiteral("phase")] = st.phase;
        entry[QStringLiteral("file")] = st.file;
        list.append(entry);
    }
    return list;
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
    bool wasInstalling = m_installing || (m_activeCount > 0);
    m_installing = v;
    bool isNowInstalling = m_installing || (m_activeCount > 0);
    qDebug() << "[install] setInstalling" << v << "was:" << wasInstalling << "now:" << isNowInstalling
             << "m_activeCount:" << m_activeCount << "ml_running:" << (m_mlInstaller ? m_mlInstaller->isRunning() : false)
             << "ml_steps:" << m_installSteps.size();
    if (wasInstalling != isNowInstalling) {
        emit installStateChanged();
    }
    emitActiveInstallsChanged();
}

void VersionBackend::setInstallPhase(const QString& phase)
{
    if (m_installPhase != phase) {
        m_installPhase = phase;
        emit installPhaseChanged(phase);
    }
}

// ============================================================
// Multi-downloader helpers
// ============================================================

VersionDownloader* VersionBackend::primaryDownloader() const
{
    if (m_activeIds.isEmpty()) return nullptr;
    return m_downloaders.value(m_activeIds.first(), nullptr);
}

QString VersionBackend::primaryVersionId() const
{
    return m_activeIds.isEmpty() ? QString() : m_activeIds.first();
}

void VersionBackend::syncPrimaryProgress()
{
    const QString pid = primaryVersionId();
    if (pid.isEmpty() || !m_dlStates.contains(pid)) {
        // No active download
        m_installProgress = 0;
        m_installTotal = 0;
        m_installBytesDl = 0;
        m_installBytesTotal = 0;
        m_installSpeed = 0;
        m_installFile.clear();
        setInstallPhase(QStringLiteral("idle"));
        emit installProgressChanged();
        emit installTotalChanged();
        emit installSpeedChanged(0);
        emit installStateChanged();
        return;
    }

    const auto& st = m_dlStates[pid];
    m_installProgress = st.progress;
    m_installTotal = st.total;
    m_installBytesDl = st.bytesDl;
    m_installBytesTotal = st.bytesTotal;
    m_installSpeed = st.speed;
    m_installFile = st.file;
    setInstallPhase(st.phase);

    emit installProgressChanged();
    emit installTotalChanged();
    emit installSpeedChanged(st.speed);
    emit installFileProgress(st.file);
    emit installBytesProgress(st.bytesDl, st.bytesTotal);
}

void VersionBackend::updateDownloadProgress(const QString& versionId,
                                            int cf, int tf,
                                            qint64 db, qint64 tb)
{
    // ── Update per-download state ──
    auto& st = m_dlStates[versionId];
    st.progress = cf;
    st.total = tf;
    st.bytesDl = db;
    st.bytesTotal = tb;

    // Speed calculation
    if (!m_speedTimer.isValid()) {
        m_speedTimer.start();
        m_lastSpeedBytes = db;
        st.speed = 0;
    } else {
        qint64 elapsedMs = m_speedTimer.elapsed();
        if (elapsedMs >= 500) {
            qint64 deltaBytes = db - m_lastSpeedBytes;
            if (deltaBytes > 0 && elapsedMs > 0) {
                st.speed = deltaBytes * 1000 / elapsedMs;
            } else {
                st.speed = 0;
            }
            m_speedTimer.restart();
            m_lastSpeedBytes = db;
        }
    }

    if (st.phase != QStringLiteral("校验中...")) {
        st.phase = QStringLiteral("下载中...");
    }

    // ── If this is the primary download, sync to main properties ──
    if (versionId == primaryVersionId()) {
        m_installProgress = cf;
        m_installTotal = tf;
        m_installBytesDl = db;
        m_installBytesTotal = tb;
        m_installSpeed = st.speed;

        if (m_installPhase != QStringLiteral("校验中...")) {
            setInstallPhase(QStringLiteral("下载中..."));
        }

        emit installProgressChanged();
        emit installTotalChanged();
        emit installBytesProgress(db, tb);
        if (st.speed > 0 || m_speedTimer.elapsed() >= 500) {
            emit installSpeedChanged(st.speed);
        }
    }
}

void VersionBackend::updateDownloadFile(const QString& versionId,
                                        const QString& fileName,
                                        qint64 received,
                                        qint64 total)
{
    auto& st = m_dlStates[versionId];
    st.file = fileName;
    st.bytesDl = received;
    st.bytesTotal = total;

    if (versionId == primaryVersionId()) {
        m_installFile = fileName;
        emit installFileProgress(fileName);
    }

    // ── Merged install: route MC download phase to steps 0-2 ──
    if (m_isMergedInstall && versionId == m_mergedMcVersion) {
        int stepIdx = -1;
        if (fileName.contains("/versions/")) stepIdx = 0;
        else if (fileName.contains("/libraries/")) stepIdx = 1;
        else if (fileName.contains("/assets/")) stepIdx = 2;
        if (stepIdx >= 0) {
            updateStep(stepIdx, QStringLiteral("active"), 0, received, total);
            m_mergedLoadedStep = stepIdx + 1;
        }
    }
}

void VersionBackend::startNextFromQueue()
{
    while (m_activeCount < MAX_CONCURRENT && !m_installQueue.isEmpty()) {
        auto next = m_installQueue.dequeue();
        QString nextId = next.first;
        int nextSource = next.second;
        qCDebug(logLaunch) << "[DOWNLOAD] dequeue=" << nextId << " remaining=" << m_installQueue.size();
        emit downloadQueueChanged();
        emit logMessage(QStringLiteral("▶ 开始队列中下一个版本: %1").arg(nextId));
        // Direct recursive call — installVersion will enqueue again if still full
        QTimer::singleShot(200, this, [this, nextId, nextSource]() {
            installVersion(nextId, nextSource);
        });
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
    if (m_activeIds.contains(versionId)) {
        // 已在下载同一版本，先取消再重启
        cancelInstall(versionId);
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

// ============================================================
// Mod Loader Installation
// ============================================================

void VersionBackend::installModLoader(const QString& mcVersion, const QString& loaderType,
                                       const QString& loaderVersion, const QString& installName) {
    if (!m_mlInstaller) return;

    // Step 0: Ensure vanilla MC is installed first
    if (!installedIds().contains(mcVersion) && !m_activeIds.contains(mcVersion)) {
        // ── Merged install: MC + loader in ONE card ──
        qDebug() << "[install] Merged install: MC" << mcVersion << "+" << loaderType << loaderVersion;
        m_isMergedInstall = true;
        m_mergedMcVersion = mcVersion;
        m_mergedLoaderType = loaderType;
        m_mergedLoaderVer = loaderVersion;
        m_hasPendingLoader = false;
        m_modLoaderInstallId = installName;
        m_installFailed = false;
        m_installError.clear();

        // Build 9-step list
        QString loaderLabel = QStringLiteral("Forge");
        if (loaderType == QStringLiteral("neoforge")) loaderLabel = QStringLiteral("NeoForge");
        else if (loaderType == QStringLiteral("fabric")) loaderLabel = QStringLiteral("Fabric");
        rebuildSteps({
            QStringLiteral("下载原版 json文件"),
            QStringLiteral("下载原版支持库文件"),
            QStringLiteral("下载原版资源文件"),
            QStringLiteral("下载 Forge 主文件"),
            QStringLiteral("校验 Forge 完整性"),
            QStringLiteral("安装 Forge")
        });
        updateStep(0, QStringLiteral("active"), 0);
        m_mergedLoadedStep = 1;

        // Start MC download (will NOT create separate version card — suppressed in activeInstalls)
        setInstalling(true);
        installVersion(mcVersion, 0);
        return;
    }

    // MC is installed or downloading — proceed with loader
    m_mlInstaller->setGameDir(m_gameDir);
    m_modLoaderInstallId = installName;
    m_installFailed = false;
    m_installError.clear();
    qDebug() << "[install] installModLoader ENTRY" << loaderType << mcVersion << loaderVersion << "->" << installName;

    // Build step list
    if (loaderType == QStringLiteral("forge") || loaderType == QStringLiteral("neoforge")) {
        rebuildSteps({QStringLiteral("下载 %1 安装程序").arg(loaderType == QStringLiteral("forge") ? QStringLiteral("Forge") : QStringLiteral("NeoForge")),
                      QStringLiteral("校验安装程序完整性"),
                      QStringLiteral("安装 %1").arg(loaderType == QStringLiteral("forge") ? QStringLiteral("Forge") : QStringLiteral("NeoForge"))});
    } else if (loaderType == QStringLiteral("fabric")) {
        rebuildSteps({QStringLiteral("下载 Fabric 配置"),
                      QStringLiteral("校验配置完整性"),
                      QStringLiteral("创建版本配置")});
    } else {
        rebuildSteps({QStringLiteral("安装 %1").arg(installName)});
    }
    updateStep(0, QStringLiteral("active"), 0);

    qDebug() << "[install] installModLoader calling installForge, m_running before:" << m_mlInstaller->isRunning();
    if (loaderType == QStringLiteral("forge")) {
        m_mlInstaller->installForge(mcVersion, loaderVersion, installName);
    } else if (loaderType == QStringLiteral("fabric")) {
        m_mlInstaller->installFabric(mcVersion, loaderVersion, installName);
    } else if (loaderType == QStringLiteral("neoforge")) {
        m_mlInstaller->installNeoForge(mcVersion, loaderVersion, installName);
    }
    qDebug() << "[install] installModLoader after installForge, m_running:" << m_mlInstaller->isRunning() << "m_steps:" << m_installSteps.size();
    setInstalling(true);
    qDebug() << "[install] installModLoader after setInstalling, m_installing:" << m_installing;
}

void VersionBackend::installOptifine(const QString& mcVersion, const QString& optifineVersion,
        const QString& forgeVersion, const QString& installName) {
    if (!m_mlInstaller) return;

    // Ensure vanilla MC is installed first
    if (!installedIds().contains(mcVersion) && !m_activeIds.contains(mcVersion)) {
        qDebug() << "[install] Vanilla MC" << mcVersion << "not installed for Optifine, downloading first...";
        m_pendingLoaderMc = mcVersion;
        m_pendingLoaderType = QStringLiteral("optifine");
        m_pendingLoaderVer = optifineVersion;
        m_pendingLoaderName = installName;
        m_hasPendingLoader = true;
        // Also store forge version so the callback knows
        installVersion(mcVersion, 0);
        return;
    }

    m_mlInstaller->setGameDir(m_gameDir);
    setInstalling(true);
    m_mlInstaller->installOptifine(mcVersion, optifineVersion, forgeVersion, installName);
}

void VersionBackend::cancelModLoaderInstall() {
    if (m_mlInstaller) m_mlInstaller->cancel();
}

bool VersionBackend::isModLoaderInstalling() const {
    return m_mlInstaller && m_mlInstaller->isRunning();
}

// ============================================================
// Unified Step Model
// ============================================================

void VersionBackend::rebuildSteps(const QStringList& names) {
    m_installSteps.clear();
    for (const QString& n : names) {
        QVariantMap s;
        s["name"] = n;
        s["status"] = QStringLiteral("pending");
        s["percentage"] = 0;
        s["bytesReceived"] = QVariant::fromValue<qint64>(0);
        s["bytesTotal"] = QVariant::fromValue<qint64>(0);
        m_installSteps.append(s);
    }
    m_installTotalProgress = 0.0;
    emit installStepsChanged();
    emit installTotalProgressChanged();
}

void VersionBackend::updateStep(int index, const QString& status, int percentage,
                                 qint64 bytesRecv, qint64 bytesTotal) {
    if (index < 0 || index >= m_installSteps.size()) return;
    QVariantMap s = m_installSteps[index].toMap();
    QString oldStatus = s["status"].toString();

    // Minimum step display: if step activated < 200ms ago, don't mark previous as completed yet
    if (status == QStringLiteral("active") && oldStatus != QStringLiteral("active")) {
        if (!m_stepChangeTime.isValid() || m_stepChangeTime.elapsed() >= m_stepMinDisplayMs) {
            // OK to transition — mark previous steps completed
            for (int i = 0; i < index; i++) {
                QVariantMap prev = m_installSteps[i].toMap();
                if (prev["status"].toString() != QStringLiteral("completed")) {
                    prev["status"] = QStringLiteral("completed");
                    prev["percentage"] = 100;
                    m_installSteps[i] = prev;
                }
            }
            m_stepChangeTime.restart();
        }
        // else: skip this step change — previous steps still show as active/pending
    }

    s["status"] = status;
    s["percentage"] = percentage;
    s["bytesReceived"] = QVariant::fromValue<qint64>(bytesRecv);
    s["bytesTotal"] = QVariant::fromValue<qint64>(bytesTotal);
    m_installSteps[index] = s;

    computeTotalProgress();
    emit installStepsChanged();
}

void VersionBackend::emitActiveInstallsChanged() {
    if (m_activeInstallsPending) return;  // already scheduled
    m_activeInstallsPending = true;
    m_activeInstallsThrottle.start();
}

void VersionBackend::computeTotalProgress() {
    if (m_installSteps.isEmpty()) { m_installTotalProgress = 0.0; return; }
    qreal sum = 0.0;
    for (const QVariant& v : m_installSteps) {
        QVariantMap s = v.toMap();
        QString st = s["status"].toString();
        if (st == QStringLiteral("completed")) sum += 1.0;
        else if (st == QStringLiteral("active")) sum += qBound(0.0, s["percentage"].toReal() / 100.0, 1.0);
    }
    m_installTotalProgress = sum / m_installSteps.size();
    emit installTotalProgressChanged();
    emitActiveInstallsChanged();
}

QVariantList VersionBackend::activeInstalls() const {
    QVariantList list;
    // 1. Mod loader install (running, pending, merged, or recently failed)
    bool mlPending = m_hasPendingLoader && !m_pendingLoaderName.isEmpty();
    bool mlActive = m_mlInstaller && m_mlInstaller->isRunning() && !m_modLoaderInstallId.isEmpty();
    bool mlMerged = m_isMergedInstall && !m_modLoaderInstallId.isEmpty();
    bool mlFailed = m_installFailed && !m_modLoaderInstallId.isEmpty();
    if (mlActive || mlFailed || mlPending || mlMerged) {
        QString cardId = mlPending ? m_pendingLoaderName : m_modLoaderInstallId;
        qDebug() << "[install] activeInstalls: mod_loader card" << cardId
                 << "totalProgress:" << m_installTotalProgress << "steps:" << m_installSteps.size()
                 << (mlFailed ? "FAILED" : (mlMerged ? "MERGED" : (mlPending ? "PENDING" : "running")));
        QVariantMap card;
        card["installId"] = cardId;
        card["installType"] = QStringLiteral("mod_loader");
        card["displayName"] = cardId;
        card["totalProgress"] = mlPending ? 0.0 : m_installTotalProgress;
        card["speed"] = mlPending ? QVariant::fromValue<qint64>(0LL) : QVariant::fromValue<qint64>(m_installSpeed);
        card["installPhase"] = m_installFailed ? QStringLiteral("失败")
                              : mlMerged ? m_installPhase
                              : mlPending ? QStringLiteral("等待MC本体下载完成...")
                              : m_installPhase;
        card["remainingSteps"] = mlPending ? 0 : installRemainingSteps();
        card["installFailed"] = m_installFailed;
        card["installError"] = m_installError;
        card["steps"] = mlPending ? QVariantList{} : m_installSteps;
        list.append(card);
    }
    // 2. Active version downloads (hide the merged MC one)
    for (const QString& vid : m_activeIds) {
        if (m_isMergedInstall && vid == m_mergedMcVersion) continue;  // shown in merged card
        QVariantMap card;
        card["installId"] = vid;
        card["installType"] = QStringLiteral("version");
        card["displayName"] = vid;
        card["remainingSteps"] = 0;
        card["steps"] = QVariantList{};
        if (m_dlStates.contains(vid)) {
            const DlState& st = m_dlStates[vid];
            card["totalProgress"] = st.total > 0 ? (qreal)st.progress / st.total : 0.0;
            card["speed"] = QVariant::fromValue<qint64>(st.speed);
            card["installPhase"] = st.phase;
        } else {
            // Placeholder — download just started, no progress data yet
            card["totalProgress"] = 0.0;
            card["speed"] = QVariant::fromValue<qint64>(0LL);
            card["installPhase"] = QStringLiteral("准备中...");
        }
        list.append(card);
    }
    // 3. Extra cards (resource / mod)
    for (auto it = m_extraCards.cbegin(); it != m_extraCards.cend(); ++it) {
        list.append(it.value());
    }
    qDebug() << "[install] activeInstalls returning" << list.size() << "cards";
    if (list.isEmpty()) {
        qDebug() << "[install] activeInstalls EMPTY — mlInstaller:" << !!m_mlInstaller
                 << "isRunning:" << (m_mlInstaller ? m_mlInstaller->isRunning() : false)
                 << "mlId:" << m_modLoaderInstallId
                 << "activeCount:" << m_activeCount << "extraCards:" << m_extraCards.size();
    }
    return list;
}

int VersionBackend::installRemainingSteps() const {
    int n = 0;
    for (const QVariant& v : m_installSteps) {
        QString s = v.toMap().value(QStringLiteral("status")).toString();
        if (s == QStringLiteral("pending") || s == QStringLiteral("active")) n++;
    }
    return n;
}

void VersionBackend::addResourceCard(const QString& cardId, const QString& displayName) {
    QVariantMap c;
    c["installId"] = cardId;
    c["installType"] = QStringLiteral("resource");
    c["displayName"] = displayName;
    c["totalProgress"] = 0.0;
    c["speed"] = QVariant::fromValue<qint64>(0);
    c["installPhase"] = QString();
    c["remainingSteps"] = 0;
    c["steps"] = QVariantList{};
    m_extraCards[cardId] = c;
    emitActiveInstallsChanged();
}

void VersionBackend::updateResourceCard(const QString& cardId, qreal progress, const QString& status) {
    if (!m_extraCards.contains(cardId)) return;
    QVariantMap c = m_extraCards[cardId];
    c["totalProgress"] = progress;
    if (!status.isEmpty()) c["installPhase"] = status;
    m_extraCards[cardId] = c;
    emitActiveInstallsChanged();
}

void VersionBackend::removeResourceCard(const QString& cardId) {
    m_extraCards.remove(cardId);
    emitActiveInstallsChanged();
}

} // namespace ShadowLauncher
