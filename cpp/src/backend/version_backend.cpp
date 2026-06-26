// Shadow Launcher — VersionBackend
// QML-facing backend for version list, installation, and lifecycle management.
// Bridges VersionManager (fetch/cache) and VersionDownloader (install pipeline).

#include "version_backend.h"
#include "../utils/logger.h"
#include "../core/version_manager.h"
#include "../core/version_downloader.h"
#include "../core/version_isolation.h"
#include "../core/mod_loader_installer.h"
#include "../core/download_coordinator.h"

#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSet>
#include <QDebug>

// Tracing tag
#define LOG_CARDS() qDebug() << "[CARDS]"
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

    m_installCardsModel = new InstallCardModel(this);

    // Throttle activeInstallsChanged to 300ms intervals (avoid flicker)
    m_cardsRebuildThrottle.setSingleShot(true);
    m_cardsRebuildThrottle.setInterval(300);
    connect(&m_cardsRebuildThrottle, &QTimer::timeout, this, [this]() {
        if (m_cardsRebuildPending) {
            m_cardsRebuildPending = false;
            rebuildInstallCards();
        }
    });

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
                    tr("版本列表已加载 (%1 个版本)")
                        .arg(m_versionIds.size()));
                emit versionListReady();
            });

    // Manifest fetch error
    connect(m_versionMgr, &VersionManager::fetchError, this,
            [this](const QString& err) {
                emit logMessage(
                    tr("获取版本列表失败: %1").arg(err));
            });

    // ModLoaderInstaller init
    m_mlInstaller = new ModLoaderInstaller(this);
    connect(m_mlInstaller, &ModLoaderInstaller::progressChanged, this,
            [this](int step, int totalSteps, const QString& desc) {
                setInstallPhase(tr("模组加载器: ") + desc);
        const QString mlId = m_modLoaderInstallId;
        if (mlId.isEmpty()) return;
        auto& ses = session(mlId);
        // Merged: MC steps 0-2, MC verify step 3, loader steps 4+ (offset=4)
        // Standalone: loader steps start at 0 (offset=0)
        int stepIdx = ses.isMerged ? (step - 1 + 4) : (step - 1);
        // ── Mark previous step as completed when advancing ──
        if (step > 1) {
            int prevIdx = ses.isMerged ? (step - 2 + 4) : (step - 2);
            updateStep(mlId, prevIdx, QStringLiteral("completed"), 100);
        }
        // Only reset to 0% if this step wasn't already active
        // (prevents progress dip during prefetch→installer transition)
        QVariantMap curStep = (stepIdx >= 0 && stepIdx < ses.steps.size()) ? ses.steps[stepIdx].toMap() : QVariantMap{};
        if (curStep.value(QStringLiteral("status")).toString() != QStringLiteral("active")) {
            // If step was hidden (verify step), make it visible first
            if (!curStep.value(QStringLiteral("show")).toBool()) {
                showStep(mlId, stepIdx);
            }
            updateStep(mlId, stepIdx, QStringLiteral("active"), 0);
            ses.loaderStepIdx = stepIdx;
        }
    });
    connect(m_mlInstaller, &ModLoaderInstaller::stepProgress, this,
            [this](int step, int percentage) {
        const QString mlId = m_modLoaderInstallId;
        if (mlId.isEmpty()) return;
        auto& ses = session(mlId);
        // Merged: MC steps 0-2, MC verify 3, loader steps 4+ (offset=4)
        int stepIdx = ses.isMerged ? (step - 1 + 4) : (step - 1);
        updateStep(mlId, stepIdx, QStringLiteral("active"), percentage);

        // Non-merged installs: stepProgress drives smoothProgress during install phases
        if (!ses.isMerged) {
            qreal raw = percentage / 100.0;
            ses.totalProgress = raw;
            if (ses.smoothProgress <= 0.0)
                ses.smoothProgress = raw * 0.3;
            else
                ses.smoothProgress = ses.smoothProgress * 0.7 + raw * 0.3;
                                    rebuildInstallCards();
        }
    });
    connect(m_mlInstaller, &ModLoaderInstaller::finished, this,
            [this](bool success, const QString& errMsg) {
        const QString mlId = m_modLoaderInstallId;
        auto& ses = mlId.isEmpty() ? activeSession() : session(mlId);
        if (!mlId.isEmpty() && !m_sessions.contains(mlId))
            return;
        if (success) {
            // Mark all steps completed
            for (int i = 0; i < ses.steps.size(); i++)
                updateStep(mlId, i, QStringLiteral("completed"), 100);
            // Merged install complete — clean up session
            if (ses.isMerged) {
                ses.isMerged = false;
                ses.mcVersion.clear();
                ses.loaderType.clear();
                ses.loaderVer.clear();
            }
            emit logMessage(tr("[ModLoader] 安装完成"));
            setInstallPhase(tr("完成"));
            updateInstalledList();
        } else {
            emit logMessage(tr("[ModLoader] 安装失败: ") + errMsg);
            // Mark last step as failed, keep card visible for diagnostics
            for (int i = ses.steps.size() - 1; i >= 0; i--) {
                QVariantMap s = ses.steps[i].toMap();
                if (s["status"].toString() == QStringLiteral("active")) {
                    s["status"] = QStringLiteral("failed");
                    s["percentage"] = 0;
                    ses.steps[i] = s;
                    ses.failed = true;
                    ses.error = errMsg;
                                        break;
                }
            }
            if (ses.isMerged) {
                ses.isMerged = false;
                ses.mcVersion.clear();
                ses.loaderType.clear();
                ses.loaderVer.clear();
            }
        }
        setInstalling(false);
        emit installFinished(success);
        if (success) emit installComplete(mlId);
    });

    // Pause between verify and install (merged install: wait for MC)
    connect(m_mlInstaller, &ModLoaderInstaller::waitingForMC, this, [this]() {
        setInstallPhase(tr("等待MC下载完成..."));
        // Mark verify step completed
        const QString mlId = m_modLoaderInstallId;
        if (!mlId.isEmpty() && m_sessions.contains(mlId)) {
            auto& ses = session(mlId);
            int verifyStep = ses.loaderVerifyStep;
            if (verifyStep >= 0 && verifyStep < ses.steps.size()) {
                updateStep(mlId, verifyStep, QStringLiteral("completed"), 100);
            }
            ses.loaderDownloadReady = true;
            if (ses.mcDownloadDone) {
                proceedToLoaderInstall(mlId);
            }
        }
    });

    // Byte-level download progress → update current active step
    connect(m_mlInstaller, &ModLoaderInstaller::byteProgress, this,
            [this](const QString& file, qint64 received, qint64 total, qint64 speed) {
                // ── EWMA speed smoothing (same algorithm as updateDownloadProgress) ──
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        const QString mlId = m_modLoaderInstallId;
        auto& ses = mlId.isEmpty() ? activeSession() : session(mlId);
        if (ses.mlSpeedLastBytes > 0 && received > ses.mlSpeedLastBytes) {
            qint64 dt = qMax(now - ses.mlSpeedLastTimeMs, qint64(100));
            qint64 instant = (received - ses.mlSpeedLastBytes) * 1000 / dt;
            m_installSpeed = (m_installSpeed > 0) ? (m_installSpeed * 7 / 10 + instant * 3 / 10) : 0;
        } else {
            m_installSpeed = 0;
        }
        ses.mlSpeedLastBytes = received;
        ses.mlSpeedLastTimeMs = now;
                // ── Merged install: accumulate ML phase bytes (cross-file aggregation) ──
        if (ses.isMerged) {
            // Detect file transition: total changes → previous file completed
            if (total > 0 && total != ses.mlFileTotal && ses.mlFileTotal > 0) {
                ses.mlBytesDone += ses.mlFileTotal;
            }

            // ── Tier (a)/(b): real Content-Length available ──
            if (total > 0) {
                ses.mlFileTotal = total;
                ses.mlBytesAll = ses.mlBytesDone + total;
            }
            // ── Tier (c): dynamic adaptive — no Content-Length, estimate from downloaded bytes ──
            else if (received > 0) {
                // Estimate: assume file is ~1.2× what we've downloaded (shrinks as download progresses)
                qint64 dynEst = received + qMax(received / 5, qint64(524288));  // at least 512KB headroom
                if (dynEst > ses.mlBytesAll)
                    ses.mlBytesAll = ses.mlBytesDone + dynEst;
                ses.mlFileTotal = dynEst;
            }

            ses.mlBytesDl = ses.mlBytesDone + received;
            // Push byte progress to the active ML step
            if (ses.loaderStepIdx >= 0 && ses.loaderStepIdx < ses.steps.size() && ses.mlBytesAll > 0) {
                int pct = (int)(ses.mlBytesDl * 100 / ses.mlBytesAll);
                updateStep(mlId, ses.loaderStepIdx, QStringLiteral("active"), pct, ses.mlBytesDl, ses.mlBytesAll);
            }
            // Byte-weighted total progress (跨阶段)
            qint64 grandDone = ses.mcBytesDl + ses.mlBytesDl;
            qint64 grandTotal = ses.mcBytesAll + ses.mlBytesAll;
            qreal raw = (grandTotal > 0) ? qMin(1.0, (qreal)grandDone / grandTotal) : 0.0;
            ses.totalProgress = raw;
            // EMA smoothing
            if (ses.smoothProgress <= 0.0) {
                ses.smoothProgress = raw * 0.3;
            } else {
                ses.smoothProgress = ses.smoothProgress * 0.7 + raw * 0.3;
            }
                                    rebuildInstallCards();
        } else {
            m_installBytesDl = received;
            m_installBytesTotal = total;
                        // Non-merged byte-weighted smoothProgress (download phases only)
            if (total > 0) {
                qreal raw = qMin(1.0, (qreal)received / total);
                ses.totalProgress = raw;
                if (ses.smoothProgress <= 0.0)
                    ses.smoothProgress = raw * 0.3;
                else
                    ses.smoothProgress = ses.smoothProgress * 0.7 + raw * 0.3;
                                                rebuildInstallCards();
            }
        }

        // Update the active step card (display only) from byte progress during download phases.
        // In install phases, stepProgress from ModLoaderInstaller is the canonical percentage source.
        {
            int activeIdx = -1;
            for (int i = ses.steps.size() - 1; i >= 0; i--) {
                if (ses.steps[i].toMap()["status"].toString() == QStringLiteral("active")) {
                    activeIdx = i;
                    break;
                }
            }
            if (activeIdx >= 0) {
                QString stepName = ses.steps[activeIdx].toMap()["name"].toString();
                // Only for download/verify steps — install steps use stepProgress
                if (stepName.contains(QStringLiteral("\u4e0b\u8f7d"))       // 下载
                    || stepName.contains(QStringLiteral("\u6821\u9a8c"))    // 校验
                    || stepName.contains(QStringLiteral("Downloading"))
                    || stepName.contains(QStringLiteral("Verifying"))) {
                    int pct = total > 0 ? (int)(received * 100 / total) : 0;
                    updateStep(mlId, activeIdx, QStringLiteral("active"), pct, received, total);
                }
            }
        }
    });

    // Verification
    connect(m_mlInstaller, &ModLoaderInstaller::verifyStarted, this,
            [this]() {
        setInstallPhase(tr("校验中"));
        m_verifyChecked = 0;
        m_verifyTotal = 1;
        emit verifyStarted();
        emit verifyProgress(0, 1);
        const QString mlId = m_modLoaderInstallId;
        if (!mlId.isEmpty()) {
            auto& ses = session(mlId);
            // Reset byte accumulators for verify phase (SHA1 is ~40B, independent of JAR download)
            ses.mlBytesDl = 0;
            ses.mlBytesDone = 0;
            ses.mlBytesAll = 0;
            ses.mlFileTotal = 0;
            updateStep(mlId, ses.loaderStepIdx, QStringLiteral("active"), 0);
        }
    });
    connect(m_mlInstaller, &ModLoaderInstaller::verifyFinished, this,
            [this](bool ok) {
        Q_UNUSED(ok);
        m_verifyChecked = 1;
        emit verifyProgress(1, 1);
        emit verifyFinished(ok);
        const QString mlId = m_modLoaderInstallId;
        if (!mlId.isEmpty()) {
            auto& ses = session(mlId);
            updateStep(mlId, ses.loaderStepIdx, QStringLiteral("completed"), 100);
        }
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
    emit logMessage(tr("正在获取版本列表..."));
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
        emit logMessage(tr("⚠ %1 正在下载中").arg(versionId));
        return;
    }

    // ── Check queue for duplicates ──
    for (const auto& entry : m_installQueue) {
        if (entry.first == versionId) {
            emit logMessage(tr("⏳ %1 已在下载队列中").arg(versionId));
            return;
        }
    }

    // ── Queue: if at max concurrency, enqueue ──
    if (m_activeCount >= MAX_CONCURRENT) {
        m_installQueue.enqueue({versionId, sourceIndex});
        qCDebug(logLaunch) << "[DOWNLOAD] enqueued=" << versionId << "pos=" << m_installQueue.size() << "active=" << m_activeCount;
        emit logMessage(tr("⏳ %1 已加入下载队列 (位置 %2)")
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
        emit logMessage(tr("📦 发现断点续传文件: %1").arg(versionId));
    }

    // Check for individual chunk .checkpoint.json files
    QDir vDir(versionDir);
    const QStringList checkpointFiles = vDir.entryList(
        {QStringLiteral("*.checkpoint.json")},
        QDir::Files | QDir::NoDotAndDotDot);
    if (!checkpointFiles.isEmpty()) {
        emit logMessage(tr("📦 发现 %1 个断点续传块文件")
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
            tr("未找到版本: %1").arg(versionId));
        return;
    }

    // ── Set installing state BEFORE async version JSON fetch ──
    // Critical: the UI must react immediately on click, not wait 500ms+ network RTT
    m_activeCount++;
    m_activeIds.append(versionId);
    setInstallPhase(tr("准备中..."));
    setInstalling(true);
    emit logMessage(tr("正在获取 %1 版本信息...").arg(versionId));
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
                        tr("获取版本信息失败: %1")
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
                        tr("版本信息格式错误: %1")
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
                        [this, versionId, downloader](int cf, int tf, qint64 db, qint64 tb) {
                            // Set authoritative category totals (pre-computed from task list)
                            auto& st = m_dlStates[versionId];
                            st.catBytesTotal[0] = downloader->categoryTotalBytes(0);
                            st.catBytesTotal[1] = downloader->categoryTotalBytes(1);
                            st.catBytesTotal[2] = downloader->categoryTotalBytes(2);
                            bool firstPulse = (st.bytesDl == 0 && db > 0);
                            // Also inject into session for merged install mod_loader card
                            for (auto sit = m_sessions.begin(); sit != m_sessions.end(); ++sit) {
                                if (sit.value().isMerged && sit.value().mcVersion == versionId) {
                                    sit.value().mcStepTotal[0] = downloader->categoryTotalBytes(0);
                                    sit.value().mcStepTotal[1] = downloader->categoryTotalBytes(1);
                                    sit.value().mcStepTotal[2] = downloader->categoryTotalBytes(2);
                                    if (firstPulse) {
                                        updateStep(sit.key(), 0, QStringLiteral("completed"), 100);
                                    }
                                    break;
                                }
                            }
                            updateDownloadProgress(versionId, cf, tf, db, tb);
                            // ── Merged install: keep mcBytesAll accurate from real tb ──
                            for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
                                if (it.value().isMerged && it.value().mcVersion == versionId) {
                                    if (tb > 0) it.value().mcBytesAll = qMax(it.value().mcBytesAll, tb);
                                    break;
                                }
                            }
                        });

                connect(downloader,
                        &VersionDownloader::fileProgress, this,
                        [this, versionId](const QString& url,
                               const QString& fileName,
                               qint64 received,
                               qint64 total) {
                            updateDownloadFile(versionId, url, fileName, received, total);
                        });

                connect(downloader,
                        &VersionDownloader::verifyProgressChanged, this,
                        [this, versionId](int checked, int total) {
                            // Update per-download state
                            auto& st = m_dlStates[versionId];
                            st.phase = tr("校验中...");
                            st.progress = checked;
                            st.total = total;
                            st.speed = 0;  // No download speed during verify
                            // Activate MC verify step for merged installs
                            for (auto sit = m_sessions.begin(); sit != m_sessions.end(); ++sit) {
                                if (sit.value().isMerged && sit.value().mcVersion == versionId) {
                                    // Mark MC download steps (0-2) as completed
                                    for (int i = 0; i < 3 && i < sit.value().steps.size(); i++) {
                                        updateStep(sit.key(), i, QStringLiteral("completed"), 100);
                                    }
                                    // Show MC verify step (index 3, initially hidden)
                                    int verifyIdx = 3;
                                    if (verifyIdx < sit.value().steps.size()) {
                                        showStep(sit.key(), verifyIdx);
                                        int pct = total > 0 ? (checked * 100 / total) : 0;
                                        updateStep(sit.key(), verifyIdx, QStringLiteral("active"), pct, checked, total);
                                    }
                                    break;
                                }
                            }
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
    emit logMessage(tr("所有安装已取消"));
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
        setInstallPhase(tr("空闲"));
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
                emit logMessage(tr("已取消队列中的 %1").arg(versionId));
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

    emit logMessage(tr("已取消 %1 的安装").arg(versionId));

    // Update installing state
    if (m_activeCount == 0) {
        setInstalling(false);
    }

    // Sync primary progress display
    syncPrimaryProgress();

    // Try to start next from queue
    startNextFromQueue();
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
            tr("✅ %1 安装完成")
                .arg(finishedId));
        refreshInstalled();
    } else {
        const auto& st = m_dlStates.value(finishedId);
        bool wasVerifying = (st.phase == tr("校验中..."));
        if (wasVerifying) {
            QString errDetail = error.isEmpty()
                                    ? tr("校验失败")
                                    : error;
            qCCritical(logVersion) << "Verify failed for" << finishedId << ":" << errDetail;
            emit logMessage(
                tr("❌ %1 校验失败: %2")
                    .arg(finishedId, errDetail));
        } else {
            QString errDetail = error.isEmpty()
                                    ? tr("未知错误")
                                    : error;
            qCCritical(logVersion) << "Install failed for" << finishedId << ":" << errDetail;
            emit logMessage(
                tr("❌ %1 安装失败: %2")
                    .arg(finishedId, errDetail));
        }
    }

    // ── Emit finished signal (per-download) ──
    emit installFinished(success);
    if (success) {
        // For merged installs, let onModLoaderFinished emit the full name
        bool isMergedId = false;
        for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
            if (it.value().isMerged && it.value().mcVersion == finishedId) {
                isMergedId = true;
                break;
            }
        }
        if (!isMergedId) emit installComplete(finishedId);
    }

    // ── Update overall state ──
    if (m_activeCount == 0) {
        // Check ALL sessions for merged installs waiting on this MC version
        bool anyMergedLaunched = false;
        for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
            auto& ses = it.value();
            if (ses.isMerged && ses.mcVersion == finishedId) {
                // Merged install: MC done, check if loader also done
                qDebug() << "[install] Merged: MC complete" << ses.loaderType;
                ses.mcBytesDl = ses.mcBytesAll;
                for (int i = 0; i < 3 && i < ses.steps.size(); i++) {
                    updateStep(it.key(), i, QStringLiteral("completed"), 100);
                }
                int verifyIdx = 3;
                if (verifyIdx < ses.steps.size()) {
                    updateStep(it.key(), verifyIdx, QStringLiteral("completed"), 100);
                }
                ses.mcDownloadDone = true;
                if (ses.loaderDownloadReady) {
                    proceedToLoaderInstall(it.key());
                } else {
                    qDebug() << "[install] MC done, waiting for loader download...";
                }
                anyMergedLaunched = true;
            }
        }
        if (!anyMergedLaunched) {
            setInstalling(false);
        }
        setInstallPhase(tr("完成"));
        m_installSpeed = 0;
                emit logMessage(tr("🎉 所有版本安装完成！"));
    } else {
        // Still have active downloads — sync primary display AND notify QML to re-read
        syncPrimaryProgress();
        emit installStateChanged();
    }

    // ── Try to start next from queue ──
    startNextFromQueue();
    qCDebug(logLaunch) << "[DOWNLOAD] finished=" << finishedId << " active=" << m_activeCount << "/" << MAX_CONCURRENT << " queue=" << m_installQueue.size();

    // Check ALL sessions for pending loaders waiting for this MC version
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        auto& ses = it.value();
        if (ses.hasPendingLoader && success && ses.pendingLoaderMc == finishedId) {
            qDebug() << "[install] MC" << finishedId << "installed, starting pending loader:" << ses.pendingLoaderName;
            ses.hasPendingLoader = false;
            if (ses.pendingLoaderType == QStringLiteral("optifine")) {
                installOptifine(ses.pendingLoaderMc, ses.pendingLoaderVer, QString(), ses.pendingLoaderName);
            } else {
                installModLoader(ses.pendingLoaderMc, ses.pendingLoaderType, ses.pendingLoaderVer, ses.pendingLoaderName);
            }
            break;  // Only one pending loader per MC version
        }
    }
}

void VersionBackend::cancelQueuedDownload(const QString& versionId)
{
    // Remove from queue
    for (int i = 0; i < m_installQueue.size(); ++i) {
        if (m_installQueue[i].first == versionId) {
            m_installQueue.removeAt(i);
            emit logMessage(tr("已取消队列中的 %1").arg(versionId));
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
        } else {
            // Forge/NeoForge/Fabric versions only have a .json (inherit jar from vanilla)
            const QString jsonPath =
                versionsDir + QStringLiteral("/") + subDir +
                QStringLiteral("/") + subDir + QStringLiteral(".json");
            if (QFileInfo::exists(jsonPath)) {
                m_installedIds.append(subDir);
            }
        }
    }
}

void VersionBackend::proceedToLoaderInstall(const QString& installId) {
    if (!m_mlInstaller || !m_sessions.contains(installId)) return;
    auto& ses = session(installId);
    qDebug() << "[install] Both downloads complete, starting" << ses.loaderType << "verify/install";
    emit logMessage(tr("MC 和 %1 下载完成，开始安装...").arg(ses.loaderType));

    m_mlInstaller->setGameDir(m_gameDir);
    if (ses.loaderType == QStringLiteral("forge")) {
        m_mlInstaller->forgeContinueInstall();
    } else if (ses.loaderType == QStringLiteral("neoforge")) {
        m_mlInstaller->neoForgeContinueInstall();
    } else if (ses.loaderType == QStringLiteral("fabric")) {
        m_mlInstaller->installFabric(ses.mcVersion, ses.loaderVer, installId);
    }
}

void VersionBackend::setInstalling(bool v)
{
    bool wasInstalling = m_installing || (m_activeCount > 0);
    m_installing = v;
    bool isNowInstalling = m_installing || (m_activeCount > 0);
    qDebug() << "[install] setInstalling" << v << "was:" << wasInstalling << "now:" << isNowInstalling
             << "m_activeCount:" << m_activeCount << "ml_running:" << (m_mlInstaller ? m_mlInstaller->isRunning() : false)
             << "ml_steps:" << (m_sessions.isEmpty() ? 0 : m_sessions.first().steps.size());
    if (wasInstalling != isNowInstalling) {
        emit installStateChanged();
    }
    rebuildInstallCards();
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
        m_installBytesDl = 0;
        m_installBytesTotal = 0;
        m_installSpeed = 0;
        setInstallPhase(tr("空闲"));
                                emit installStateChanged();
        return;
    }

    const auto& st = m_dlStates[pid];
    m_installBytesDl = st.bytesDl;
    m_installBytesTotal = st.bytesTotal;
    m_installSpeed = st.speed;

    // Two-segment progress: download 0-90%, verify 90-100%
    bool verifying = (st.phase == QStringLiteral("\u6821\u9a8c\u4e2d..."));
    if (verifying) {
        qreal seg = (st.total > 0) ? (qreal)st.progress / st.total : 0.0;
    } else {
        qreal seg = (st.total > 0) ? (qreal)st.progress / st.total : 0.0;
    }
    setInstallPhase(st.phase);

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

    // ── Speed: EWMA (α=0.3) ──
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (st.speedLastBytes > 0 && st.speedLastTimeMs > 0 && db > st.speedLastBytes) {
        qint64 dt = qMax(now - st.speedLastTimeMs, qint64(100));  // min 100ms to cap burst spikes
        qint64 delta = db - st.speedLastBytes;
        qint64 instant = delta * 1000 / dt;
        // Always use EWMA — no raw first-sample passthrough
        st.speed = (st.speed > 0) ? (st.speed * 7 / 10 + instant * 3 / 10) : (instant * 3 / 10);
    }
    st.speedLastBytes = db;
    st.speedLastTimeMs = now;

    if (st.phase != tr("校验中...")) {
        st.phase = tr("下载中...");
    }

    // ── Merged install: accumulate MC phase bytes ──
    // Find session whose merged MC version matches this download
    QString mergedSessionId;
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        if (it.value().isMerged && it.value().mcVersion == versionId) {
            mergedSessionId = it.key();
            break;
        }
    }
    if (!mergedSessionId.isEmpty()) {
        auto& mSes = m_sessions[mergedSessionId];
        mSes.mcBytesDl = db;
        // Tier (a)/(b): real total from version JSON or Content-Length
        if (tb > 0) {
            mSes.mcBytesAll = tb;
        }
        // Tier (c): dynamic adaptive — no total available
        else if (db > 0) {
            qint64 dynEst = db + qMax(db / 5, qint64(524288));
            if (dynEst > mSes.mcBytesAll)
                mSes.mcBytesAll = dynEst;
        }
        // Push real per-category byte progress to all 3 MC download steps
        for (int ci = 0; ci < 3; ci++) {
            if (mSes.mcStepTotal[ci] > 0) {
                int pct = (int)(mSes.mcStepDone[ci] * 100 / mSes.mcStepTotal[ci]);
                updateStep(mergedSessionId, ci, QStringLiteral("active"), pct,
                           mSes.mcStepDone[ci], mSes.mcStepTotal[ci]);
            }
        }
        // Byte-weighted progress for merged install MC phase
        {
            qint64 grandDone = mSes.mcBytesDl + mSes.mlBytesDl;
            qint64 grandTotal = mSes.mcBytesAll + mSes.mlBytesAll;
            qreal raw = (grandTotal > 0) ? qMin(1.0, (qreal)grandDone / grandTotal) : 0.0;
            mSes.totalProgress = raw;
            if (mSes.smoothProgress <= 0.0)
                mSes.smoothProgress = raw * 0.3;
            else
                mSes.smoothProgress = mSes.smoothProgress * 0.7 + raw * 0.3;
        }
    }

    // ── If this is the primary download, sync to main properties ──
    if (versionId == primaryVersionId()) {
        m_installBytesDl = db;
        m_installBytesTotal = tb;
        m_installSpeed = st.speed;

        // Byte-weighted total progress ── raw ──
        // Find merged session for grand-total calculation
        qreal rawTotalProgress = 0.0;
        if (!mergedSessionId.isEmpty()) {
            qint64 grandTotal = session(mergedSessionId).mcBytesAll + session(mergedSessionId).mlBytesAll;
            qint64 grandDone = session(mergedSessionId).mcBytesDl + session(mergedSessionId).mlBytesDl;
            rawTotalProgress = (grandTotal > 0)
                ? (qreal)grandDone / grandTotal
                : (tb > 0 ? (qreal)db / tb : 0.0);
        } else {
            rawTotalProgress = (tb > 0) ? (qreal)db / tb : 0.0;
        }

        // Update session total/smooth progress for THIS session (not activeSession which may be wrong)
        if (!mergedSessionId.isEmpty()) {
            auto& mSes = m_sessions[mergedSessionId];
            mSes.totalProgress = rawTotalProgress;
            // ── EMA smoothing ──
            if (mSes.smoothProgress <= 0.0 || rawTotalProgress > mSes.smoothProgress + 0.5) {
                mSes.smoothProgress = rawTotalProgress;
            } else {
                mSes.smoothProgress = mSes.smoothProgress * 0.7 + rawTotalProgress * 0.3;
            }
        }

        if (m_installPhase != tr("校验中...")) {
            setInstallPhase(tr("下载中..."));
        }

        rebuildInstallCards();
    }
}

void VersionBackend::updateDownloadFile(const QString& versionId,
                                        const QString& url,
                                        const QString& fileName,
                                        qint64 received,
                                        qint64 total)
{
    auto& st = m_dlStates[versionId];
    st.file = fileName;

    // ── Determine category for merged-install step routing ──
    int cat = -1;
    if (url.contains(QStringLiteral("/versions/"))) cat = 0;
    else if (url.contains(QStringLiteral("/libraries/")) || url.contains(QStringLiteral("/maven/"))) cat = 1;
    else if (url.contains(QStringLiteral("/assets/"))) cat = 2;

    if (versionId == primaryVersionId()) {
            }

    // ── Per-category done-byte tracking for DlState (version cards) ──
    // Denominator (catBytesTotal) comes from progressChanged->categoryTotalBytes()
    if (cat >= 0 && cat <= 2) {
        qint64 catDone = st.catBytesDoneBase[cat] + received;
        if (received >= total && total > 0) {
            st.catBytesDoneBase[cat] += total;
            catDone = st.catBytesDoneBase[cat];
        }
        st.catBytesDl[cat] = catDone;
    }

    // ── Merged install: route MC download phase to steps 0-2 ──
    QString mergedSessionId;
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        if (it.value().isMerged && it.value().mcVersion == versionId) {
            mergedSessionId = it.key();
            break;
        }
    }
    if (!mergedSessionId.isEmpty()) {
        if (cat >= 0 && cat <= 2) {
            // Denominator (mcStepTotal) comes from progressChanged->categoryTotalBytes()
            qint64 accDone = session(mergedSessionId).mcStepDone[cat] + received;
            qint64 accTotal = session(mergedSessionId).mcStepTotal[cat];
            if (received >= total && total > 0) {
                session(mergedSessionId).mcStepDone[cat] += total;
                accDone = session(mergedSessionId).mcStepDone[cat];
            }
            updateStep(mergedSessionId, cat, QStringLiteral("active"), 0, accDone, accTotal);
            m_sessions[mergedSessionId].loadedStep = cat + 1;
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
        emit logMessage(tr("▶ 开始队列中下一个版本: %1").arg(nextId));
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
            m_failedFiles.append(item.name + tr(" (缺失)"));
            m_failedPaths.append(item.path);
        } else if (!item.sha1.isEmpty()) {
            QString actual = sha1FileFast(item.path);
            if (actual.compare(item.sha1, Qt::CaseInsensitive) != 0) {
                m_failed++;
                m_failedFiles.append(item.name + tr(" (校验失败)"));
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
        emit logMessage(tr("校验已在运行中"));
        return;
    }

    setInstallPhase(tr("校验中"));
    m_verifyChecked = 0;
    m_verifyTotal = 0;
    emit verifyStarted();
    qCInfo(logVersion) << "Verify started:" << versionId;
    emit logMessage(tr("正在校验版本 %1...").arg(versionId));

    const QString versionDir = m_gameDir + QStringLiteral("/versions/") + versionId;
    const QString jsonPath = versionDir + QStringLiteral("/") + versionId + QStringLiteral(".json");

    // Load version JSON
    QFile jsonFile(jsonPath);
    if (!jsonFile.open(QIODevice::ReadOnly)) {
        emit logMessage(tr("❌ 无法读取版本配置: %1").arg(jsonPath));
        setInstallPhase(tr("空闲"));
        emit verifyFinished(false);
        return;
    }

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(jsonFile.readAll(), &parseErr);
    jsonFile.close();

    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        emit logMessage(tr("❌ 版本配置解析失败: %1").arg(parseErr.errorString()));
        setInstallPhase(tr("空闲"));
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
    emit logMessage(tr("校验 %1 个文件...").arg(total));
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
                emit logMessage(tr("⚠ 校验已取消"));
                setInstallPhase(tr("空闲"));
                emit verifyCancelled();
            }, Qt::QueuedConnection);

    // Finished
    connect(m_verifyWorker, &VerifyWorker::finished, this,
            [this](bool allPassed, const QStringList& failedFiles,
                   const QStringList& failedPaths) {
                if (allPassed) {
                    qCInfo(logVersion) << "Verify completed — all" << m_verifyTotal << "files passed";
                    emit logMessage(tr("✅ 校验完成: %1 个文件全部通过").arg(m_verifyTotal));
                } else {
                    int failed = failedFiles.size();
                    qCCritical(logVersion) << "Verify completed —" << failed << "/" << m_verifyTotal << "files failed";

                    QString detail = failedFiles.join(QStringLiteral(", "));
                    if (detail.length() > 250) {
                        detail = detail.left(250) + QStringLiteral("...");
                    }
                    emit logMessage(tr("❌ 校验完成: %1/%2 个文件失败").arg(failed).arg(m_verifyTotal));
                    emit logMessage(tr("   失败文件: %1").arg(detail));
                    emit verifyFailedFiles(failedFiles);

                    // Store failed paths for later cleanup
                    m_failedPathsCache = failedPaths;
                }

                setInstallPhase(tr("空闲"));
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
        emit logMessage(tr("正在取消校验..."));
    }
}

// ============================================================
// Version management: clean corrupt files
// ============================================================

void VersionBackend::cleanCorruptVersion(const QString& versionId)
{
    if (m_failedPathsCache.isEmpty()) {
        emit logMessage(tr("没有可清理的校验结果"));
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
    emit logMessage(tr("🧹 清理完成: 删除 %1 个损坏文件, %2 个文件已不存在")
                        .arg(removed).arg(cleaned));
    emit logMessage(tr("💡 请重新下载版本 %1 以恢复缺失文件").arg(versionId));
}

void VersionBackend::repairVersion(const QString& versionId)
{
    if (m_failedPathsCache.isEmpty()) {
        emit logMessage(tr("没有需要修复的文件"));
        return;
    }

    int removed = 0;
    for (const QString& path : m_failedPathsCache) {
        if (QFileInfo::exists(path) && QFile::remove(path)) {
            removed++;
        }
    }

    emit logMessage(tr("🔧 修复中: 已删除 %1 个损坏文件，开始重新下载...").arg(removed));

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
        emit logMessage(tr("重命名失败: ID不能为空"));
        return false;
    }

    const QString versionsDir = m_gameDir + QStringLiteral("/versions");
    const QString oldDir = versionsDir + QStringLiteral("/") + oldId;
    const QString newDir = versionsDir + QStringLiteral("/") + newId;

    if (!QDir(oldDir).exists()) {
        emit logMessage(tr("重命名失败: 版本 %1 不存在").arg(oldId));
        return false;
    }

    if (QDir(newDir).exists()) {
        emit logMessage(tr("重命名失败: 目标 %1 已存在").arg(newId));
        return false;
    }

    // Rename directory
    if (!QDir().rename(oldDir, newDir)) {
        emit logMessage(tr("重命名失败: 无法重命名目录"));
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

    emit logMessage(tr("版本已重命名: %1 → %2").arg(oldId, newId));
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
        emit logMessage(tr("克隆失败: ID不能为空"));
        return false;
    }

    const QString versionsDir = m_gameDir + QStringLiteral("/versions");
    const QString srcDir = versionsDir + QStringLiteral("/") + sourceId;
    const QString dstDir = versionsDir + QStringLiteral("/") + newId;

    if (!QDir(srcDir).exists()) {
        emit logMessage(tr("克隆失败: 源版本 %1 不存在").arg(sourceId));
        return false;
    }

    if (QDir(dstDir).exists()) {
        emit logMessage(tr("克隆失败: 目标 %1 已存在").arg(newId));
        return false;
    }

    // Recursively copy
    if (!copyDirRecursive(srcDir, dstDir)) {
        emit logMessage(tr("克隆失败: 复制过程中出错"));
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

    emit logMessage(tr("版本已克隆: %1 → %2").arg(sourceId, newId));
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

    emit logMessage(tr("已复制路径: %1").arg(QDir::toNativeSeparators(path)));
    return QDir::toNativeSeparators(path);
}

// ============================================================
// Mod Loader Installation
// ============================================================

void VersionBackend::installModLoader(const QString& mcVersion, const QString& loaderType,
                                       const QString& loaderVersion, const QString& installName) {
    if (!m_mlInstaller) return;

    m_modLoaderInstallId = installName;
    auto& ses = session(installName);
    ses.failed = false;
    ses.error.clear();

    // Step 0: Ensure vanilla MC is installed first
    if (!installedIds().contains(mcVersion) && !m_activeIds.contains(mcVersion)) {
        // ── Merged install: MC + loader in ONE card ──
        qDebug() << "[install] Merged install: MC" << mcVersion << "+" << loaderType << loaderVersion;
        ses.isMerged = true;
        ses.mcVersion = mcVersion;
        ses.loaderType = loaderType;
        ses.loaderVer = loaderVersion;
        ses.hasPendingLoader = false;
        // Reset byte accumulators for new merged install
        ses.mcBytesDl = 0;
        ses.mcBytesAll = 0;
        ses.mlBytesDl = 0;
        ses.mlBytesAll = 0;
        ses.mlBytesDone = 0;
        ses.mlFileTotal = 0;
        // Reset cumulative byte tracking for merged install steps
        for (int i = 0; i < 3; i++) { ses.mcStepDone[i] = 0; ses.mcStepTotal[i] = 0; }
        ses.mcFileAdded.clear();

        // Build step list — Forge/NeoForge: 7 steps, Fabric: 7 (profile + libs + install)
        QString loaderLabel = QStringLiteral("Forge");
        if (loaderType == QStringLiteral("neoforge")) loaderLabel = QStringLiteral("NeoForge");
        else if (loaderType == QStringLiteral("fabric")) loaderLabel = QStringLiteral("Fabric");

        if (loaderType == QStringLiteral("fabric")) {
            // Fabric: 7 steps (3 MC download + 1 MC verify + 1 Fabric profile + 1 Fabric libs + 1 install)
            rebuildSteps(installName, {
                tr("下载原版 JSON 文件"),
                tr("下载原版支持库文件"),
                tr("下载原版资源文件"),
                tr("校验游戏资源完整性"),
                tr("下载 Fabric 配置"),
                tr("下载 Fabric 依赖库"),
                tr("安装 Fabric")
            }, {3.0, 8.0, 5.0, 0.5, 0.1, 2.0, 0.5},
             {true, true, true, false, true, true, true});
        } else {
            // Forge/NeoForge: 7 steps (3 MC + 1 MC verify + 1 download + 1 verify + 1 install)
            rebuildSteps(installName, {
                tr("下载原版 JSON 文件"),
                tr("下载原版支持库文件"),
                tr("下载原版资源文件"),
                tr("校验游戏资源完整性"),
                tr("下载 %1 主文件").arg(loaderLabel),
                tr("校验 %1 完整性").arg(loaderLabel),
                tr("安装 %1").arg(loaderLabel)
            }, {3.0, 8.0, 5.0, 0.5, 6.0, 0.5, 10.0},
             {true, true, true, false, true, false, false});
        }
        updateStep(installName, 0, QStringLiteral("active"), 0);
        ses.loadedStep = 1;

        // ── Run preflight: connectivity test + total bytes ──
        setInstalling(true);
        setInstallPhase(tr("连通性测试中..."));

        // Determine mirror URL for connectivity test (use manifestUrl — returns 200 reliably)
        QVector<MirrorSource> mirrors = MirrorSource::allMirrors();
        MirrorSource mirror = mirrors.isEmpty() ? MirrorSource{} : mirrors[0];
        QString mcTestUrl = QStringLiteral("https://bmclapi2.bangbang93.com/mc/game/version_manifest.json");
        if (!mirror.manifestUrl.isEmpty()) {
            mcTestUrl = mirror.manifestUrl;  // guaranteed 200
        }

        // Build loader download URL (unused for coordinator, needed by ModLoaderInstaller later)

        auto* coord = new DownloadCoordinator(this);
        coord->addSource(QStringLiteral("bmclapi"), mcTestUrl);  // One HEAD covers all BMCLAPI sub-services
        coord->addSource(QStringLiteral("mojang"), MirrorSource::mojang().manifestUrl);

        connect(coord, &DownloadCoordinator::ready, this, [this, mcVersion, loaderType, loaderVersion, coord, installName](int sourceIndex, qint64) {
            auto& ses = session(installName);
            qDebug() << "[Coordinator] Preflight OK, starting MC +" << loaderType << "download in PARALLEL (mirror" << sourceIndex << ")";
            emit logMessage(tr("✓ 连通性测试通过，同时下载 MC 和 ") + loaderType);

            // ── 1. Start MC download with the mirror that passed connectivity ──
            installVersion(mcVersion, sourceIndex);

            // ── 2. Fabric: download profile + libraries in parallel with MC ──
            if (loaderType == QStringLiteral("fabric")) {
                m_modLoaderInstallId = installName;
                m_mlInstaller->setGameDir(m_gameDir);
                m_mlInstaller->setParallelMode(true);
                m_mlInstaller->installFabric(mcVersion, loaderVersion, installName);
                // waitingForMC() will fire when profile+libs download completes
                // → sets loaderDownloadReady = true → if MC done, proceedToLoaderInstall
                return;
            }

            // ── 2. Start loader download IN PARALLEL (Forge/NeoForge JAR) ──
            // Use the SAME URL as ModLoaderInstaller (BMCLAPI Forge API, not Maven)
            QString verArg = mcVersion + "-" + loaderVersion;
            QString loaderDlUrl;
            if (loaderType == QStringLiteral("forge")) {
                loaderDlUrl = QStringLiteral("https://bmclapi2.bangbang93.com/forge/download/%1/installer").arg(verArg);
            } else if (loaderType == QStringLiteral("neoforge")) {
                loaderDlUrl = QStringLiteral("https://bmclapi2.bangbang93.com/maven/net/neoforged/neoforge/%1/neoforge-%1-installer.jar").arg(loaderVersion);
            }

                        if (!loaderDlUrl.isEmpty()) {
                // Shared downloader: try BMCLAPI, fall back to official on failure
                auto* nam = new QNetworkAccessManager(this);
                int loaderDlStepIdx = (ses.steps.size() >= 5) ? 4 : 4;

                // Phase 1: try BMCLAPI
                qDebug() << "[Coordinator] Loader download:" << loaderDlUrl;
                {
                    QUrl qurl(loaderDlUrl);
                    QNetworkRequest req(qurl);
                    req.setRawHeader("User-Agent", "ShadowLauncher/1.0");
                    req.setTransferTimeout(300000);
                    QNetworkReply* reply = nam->get(req);

                    connect(reply, &QNetworkReply::downloadProgress, this,
                            [this, installName, loaderDlStepIdx](qint64 recv, qint64 total) {
                        updateStep(installName, loaderDlStepIdx, QStringLiteral("active"),
                                   total > 0 ? (int)(recv * 100 / total) : 0, recv, total);
                    });

                    connect(reply, &QNetworkReply::finished, this,
                            [this, nam, reply, installName, loaderType, loaderVersion, mcVersion, loaderDlStepIdx]() {
                        reply->deleteLater();
                        if (reply->error() != QNetworkReply::NoError) {
                            // Phase 2: try official fallback
                            QString fallbackUrl;
                            if (loaderType == QStringLiteral("forge")) {
                                QString verArg = mcVersion + "-" + loaderVersion;
                                fallbackUrl = QStringLiteral("https://maven.minecraftforge.net/net/minecraftforge/forge/%1/forge-%1-installer.jar").arg(verArg);
                            } else if (loaderType == QStringLiteral("neoforge")) {
                                fallbackUrl = QStringLiteral("https://maven.neoforged.net/releases/net/neoforged/neoforge/%1/neoforge-%1-installer.jar").arg(loaderVersion);
                            }
                            if (!fallbackUrl.isEmpty()) {
                                qWarning() << "[Coordinator] BMCLAPI failed, trying official:" << fallbackUrl;
                                emit logMessage(QStringLiteral(" BMCLAPI \u4e0d\u901a\uff0c\u5c1d\u8bd5\u5b98\u65b9\u6e90..."));
                                QUrl qurl2(fallbackUrl);
                                QNetworkRequest req2(qurl2);
                                req2.setRawHeader("User-Agent", "ShadowLauncher/1.0");
                                req2.setTransferTimeout(300000);
                                QNetworkReply* r2 = nam->get(req2);
                                connect(r2, &QNetworkReply::downloadProgress, this,
                                        [this, installName, loaderDlStepIdx](qint64 recv, qint64 total) {
                                    updateStep(installName, loaderDlStepIdx, QStringLiteral("active"),
                                               total > 0 ? (int)(recv * 100 / total) : 0, recv, total);
                                });
                                connect(r2, &QNetworkReply::finished, this,
                                        [this, nam, r2, installName, loaderType, mcVersion, loaderDlStepIdx]() {
                                    r2->deleteLater();
                                    if (r2->error() != QNetworkReply::NoError) {
                                        qWarning() << "[Coordinator] Loader download FAILED:" << r2->errorString();
                                        emit logMessage(QStringLiteral(" %1 \u4e0b\u8f7d\u5931\u8d25: %2").arg(loaderType).arg(r2->errorString()));
                                        nam->deleteLater();
                                        for (auto it = m_downloaders.begin(); it != m_downloaders.end(); ++it) {
                                            if (it.key() == mcVersion) {
                                                it.value()->cancel();
                                                it.value()->disconnect();
                                                it.value()->deleteLater();
                                                m_downloaders.erase(it);
                                                break;
                                            }
                                        }
                                        m_dlStates.remove(mcVersion);
                                        m_activeIds.removeAll(mcVersion);
                                        if (!m_activeIds.isEmpty()) m_activeCount = m_activeIds.size();
                                        if (m_sessions.contains(installName)) {
                                            auto& ses = session(installName);
                                            ses.failed = true;
                                            ses.error = r2->errorString();
                                        }
                                        rebuildInstallCards();
                                        return;
                                    }
                                    QByteArray data = r2->readAll();
                                    qDebug() << "[Coordinator] Loader FALLBACK download complete:" << data.size() << "bytes";
                                    nam->deleteLater();
                                    if (!m_sessions.contains(installName)) return;
                                    auto& ses = session(installName);
                                    ses.loaderDownloadData = data;
                                    updateStep(installName, loaderDlStepIdx, QStringLiteral("completed"), 100, data.size(), data.size());
                                    ses.loaderVerifyStep = (loaderDlStepIdx == 4) ? 5 : loaderDlStepIdx + 1;
                                    m_mlInstaller->setGameDir(m_gameDir);
                                    if (loaderType == QStringLiteral("neoforge"))
                                        m_mlInstaller->installNeoForgeFromData(data, ses.mcVersion, ses.loaderVer, installName);
                                    else
                                        m_mlInstaller->installForgeFromData(data, ses.mcVersion, ses.loaderVer, installName);
                                });
                                return;
                            }
                            // No fallback URL, fail immediately
                            qWarning() << "[Coordinator] Loader download FAILED (no fallback):" << reply->errorString();
                            emit logMessage(QStringLiteral(" %1 \u4e0b\u8f7d\u5931\u8d25: %2").arg(loaderType).arg(reply->errorString()));
                            nam->deleteLater();
                            for (auto it = m_downloaders.begin(); it != m_downloaders.end(); ++it) {
                                if (it.key() == mcVersion) {
                                    it.value()->cancel();
                                    it.value()->disconnect();
                                    it.value()->deleteLater();
                                    m_downloaders.erase(it);
                                    break;
                                }
                            }
                            m_dlStates.remove(mcVersion);
                            m_activeIds.removeAll(mcVersion);
                            if (!m_activeIds.isEmpty()) m_activeCount = m_activeIds.size();
                            if (m_sessions.contains(installName)) {
                                auto& ses = session(installName);
                                ses.failed = true;
                                ses.error = reply->errorString();
                            }
                            rebuildInstallCards();
                            return;
                        }
                        QByteArray data = reply->readAll();
                        qDebug() << "[Coordinator] Loader download complete:" << data.size() << "bytes";
                        nam->deleteLater();
                        if (!m_sessions.contains(installName)) return;
                        auto& ses = session(installName);
                        ses.loaderDownloadData = data;
                        updateStep(installName, loaderDlStepIdx, QStringLiteral("completed"), 100, data.size(), data.size());
                        ses.loaderVerifyStep = (loaderDlStepIdx == 4) ? 5 : loaderDlStepIdx + 1;
                        m_mlInstaller->setGameDir(m_gameDir);
                        if (loaderType == QStringLiteral("neoforge"))
                            m_mlInstaller->installNeoForgeFromData(data, ses.mcVersion, ses.loaderVer, installName);
                        else
                            m_mlInstaller->installForgeFromData(data, ses.mcVersion, ses.loaderVer, installName);
                    });
                }

            }
        });
        connect(coord, &DownloadCoordinator::connectivityFailed, this,
                [this, coord, installName](const QString& taskId, const QString& reason) {
            emit logMessage(tr("✗ 连通性测试失败: %1 — %2").arg(taskId).arg(reason));
            // Mark install as failed
            auto& ses = session(installName);
            ses.failed = true;
            ses.error = tr("网络不可达: ") + taskId;
            rebuildInstallCards();
            setInstalling(false);
            coord->deleteLater();
        });

        coord->start();
        return;
    }

    // MC is installed or downloading — proceed with loader
    m_mlInstaller->setGameDir(m_gameDir);
    qDebug() << "[install] installModLoader ENTRY" << loaderType << mcVersion << loaderVersion << "->" << installName;

    // Build step list
    if (loaderType == QStringLiteral("forge") || loaderType == QStringLiteral("neoforge")) {
        rebuildSteps(installName, {tr("下载 %1 安装程序").arg(loaderType == QStringLiteral("forge") ? QStringLiteral("Forge") : QStringLiteral("NeoForge")),
                      tr("校验安装程序完整性"),
                      tr("安装 %1").arg(loaderType == QStringLiteral("forge") ? QStringLiteral("Forge") : QStringLiteral("NeoForge"))},
                     {3.0, 0.5, 10.0},  // installer 重量级10
                     {true, false, true});  // verify hidden until download completes
    } else if (loaderType == QStringLiteral("fabric")) {
        // Fabric: 3 steps (download profile + download libraries + install)
        rebuildSteps(installName, {tr("下载 Fabric 配置"),
                      tr("下载 Fabric 依赖库"),
                      tr("安装 Fabric")},
                     {0.1, 2.0, 0.5},
                     {true, true, true});
    } else {
        rebuildSteps(installName, {tr("安装 %1").arg(installName)});
    }
    updateStep(installName, 0, QStringLiteral("active"), 0);

    qDebug() << "[install] installModLoader calling install" << loaderType << ", m_running before:" << m_mlInstaller->isRunning();
    if (loaderType == QStringLiteral("forge")) {
        m_mlInstaller->installForge(mcVersion, loaderVersion, installName);
    } else if (loaderType == QStringLiteral("fabric")) {
        m_mlInstaller->installFabric(mcVersion, loaderVersion, installName);
    } else if (loaderType == QStringLiteral("neoforge")) {
        m_mlInstaller->installNeoForge(mcVersion, loaderVersion, installName);
    }
    qDebug() << "[install] installModLoader after install, m_running:" << m_mlInstaller->isRunning() << "m_steps:" << ses.steps.size();
    setInstalling(true);
    qDebug() << "[install] installModLoader after setInstalling, m_installing:" << m_installing;
}

void VersionBackend::installOptifine(const QString& mcVersion, const QString& optifineVersion,
        const QString& forgeVersion, const QString& installName) {
    if (!m_mlInstaller) return;

    auto& ses = session(installName);

    // Ensure vanilla MC is installed first
    if (!installedIds().contains(mcVersion) && !m_activeIds.contains(mcVersion)) {
        qDebug() << "[install] Vanilla MC" << mcVersion << "not installed for Optifine, downloading first...";
        ses.pendingLoaderMc = mcVersion;
        ses.pendingLoaderType = QStringLiteral("optifine");
        ses.pendingLoaderVer = optifineVersion;
        ses.pendingLoaderName = installName;
        ses.hasPendingLoader = true;
        // Also store forge version so the callback knows
        installVersion(mcVersion, 0);
        return;
    }

    m_mlInstaller->setGameDir(m_gameDir);
    // Respect version isolation for mods/ output
    if (m_isolation && m_isolation->isVersionIsolated(installName)) {
        m_mlInstaller->setModsDir(m_isolation->getVersionGameDir(installName) + "/mods");
    } else {
        m_mlInstaller->setModsDir(QString());  // reset to default (gameDir/mods)
    }

    // Preflight: connectivity test (use manifestUrl, not the actual download URL — HEAD blocked by CDN)
    auto* coord = new DownloadCoordinator(this);
    coord->addSource(QStringLiteral("bmclapi"), QStringLiteral("https://bmclapi2.bangbang93.com/mc/game/version_manifest.json"));
    coord->addSource(QStringLiteral("mojang"), MirrorSource::mojang().manifestUrl);
    setInstallPhase(tr("连通性测试中..."));
    setInstalling(true);

    connect(coord, &DownloadCoordinator::ready, this,
            [this, mcVersion, optifineVersion, forgeVersion, installName, coord](int /*sourceIndex*/, qint64) {
        coord->deleteLater();
        emit logMessage(tr("✓ OptiFine 连通性测试通过"));
        m_mlInstaller->installOptifine(mcVersion, optifineVersion, forgeVersion, installName);
    });
    connect(coord, &DownloadCoordinator::connectivityFailed, this,
            [this, coord, installName](const QString& taskId, const QString& reason) {
        coord->deleteLater();
        emit logMessage(tr("✗ OptiFine 连通性测试失败: %1").arg(reason));
        auto& ses = session(installName);
        ses.failed = true;
        ses.error = tr("OptiFine 网络不可达");
        rebuildInstallCards();
        setInstalling(false);
    });
    coord->start();
    return;
}

void VersionBackend::cancelModLoaderInstall() {
    // Cancel ModLoaderInstaller if running
    if (m_mlInstaller) m_mlInstaller->cancel();
    // Also cancel any active VersionDownloader (merged install MC download phase)
    for (auto it = m_downloaders.begin(); it != m_downloaders.end(); ++it) {
        it.value()->cancel();
        it.value()->disconnect();
        it.value()->deleteLater();
    }
    m_downloaders.clear();
    m_dlStates.clear();
    m_activeIds.clear();
    m_activeCount = 0;
    // Clean up session state
    if (!m_modLoaderInstallId.isEmpty() && m_sessions.contains(m_modLoaderInstallId)) {
        m_sessions[m_modLoaderInstallId].failed = true;
        m_sessions[m_modLoaderInstallId].error = tr("已取消");
    }
    setInstalling(false);
    setInstallPhase(tr("空闲"));
    emit logMessage(tr("安装已取消"));
}

bool VersionBackend::isModLoaderInstalling() const {
    return m_mlInstaller && m_mlInstaller->isRunning();
}

// ============================================================
// Unified Step Model
// ============================================================

// ═══════════ Session helpers ═══════════

InstallSession& VersionBackend::session(const QString& installId) {
    if (!m_sessions.contains(installId)) {
        m_sessions[installId] = InstallSession{};
    }
    return m_sessions[installId];
}

InstallSession& VersionBackend::activeSession() {
    // Return first active session (for backward-compat access)
    static InstallSession s_empty;
    if (m_sessions.isEmpty()) return s_empty;
    return m_sessions.first();
}

void VersionBackend::rebuildSteps(const QString& installId, const QStringList& names, const QVector<qreal>& weights,
                                   const QVector<bool>& showFlags) {
    auto& ses = session(installId);
    ses.steps.clear();
    for (int i = 0; i < names.size(); i++) {
        QVariantMap step;
        step["name"] = names[i];
        step["status"] = QStringLiteral("pending");
        step["percentage"] = 0;
        step["bytesReceived"] = QVariant::fromValue<qint64>(0);
        step["bytesTotal"] = QVariant::fromValue<qint64>(0);
        step["weight"] = (i < weights.size()) ? weights[i] : 1.0;
        step["show"] = (i < showFlags.size()) ? showFlags[i] : true;
        ses.steps.append(step);
    }
    ses.totalProgress = 0.0;
    ses.smoothProgress = 0.0;
}

void VersionBackend::showStep(const QString& installId, int index) {
    auto& s = session(installId);
    if (index < 0 || index >= s.steps.size()) return;
    QVariantMap step = s.steps[index].toMap();
    step["show"] = true;
    step["status"] = QStringLiteral("active");
    step["percentage"] = 0;
    s.steps[index] = step;
    rebuildInstallCards();
}

void VersionBackend::updateStep(const QString& installId, int index, const QString& status, int percentage,
                                 qint64 bytesRecv, qint64 bytesTotal) {
    auto& s = session(installId);
    if (index < 0 || index >= s.steps.size()) return;
    QVariantMap step = s.steps[index].toMap();

    // Auto-compute percentage from bytes if provided and no explicit percentage given
    if (percentage == 0 && bytesRecv > 0 && bytesTotal > 0) {
        percentage = (int)((bytesRecv * 100) / bytesTotal);
    }

    step["status"] = status;
    step["percentage"] = percentage;
    step["bytesReceived"] = QVariant::fromValue<qint64>(bytesRecv);
    step["bytesTotal"] = QVariant::fromValue<qint64>(bytesTotal);
    s.steps[index] = step;

    }



int VersionBackend::installRemainingSteps() const {
    if (m_sessions.isEmpty()) return 0;
    const auto& ses = m_sessions.first();
    int n = 0;
    for (const QVariant& v : ses.steps) {
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
    rebuildInstallCards();
}

void VersionBackend::updateResourceCard(const QString& cardId, qreal progress, const QString& status) {
    if (!m_extraCards.contains(cardId)) return;
    QVariantMap c = m_extraCards[cardId];
    c["totalProgress"] = progress;
    if (!status.isEmpty()) c["installPhase"] = status;
    m_extraCards[cardId] = c;
    rebuildInstallCards();
}

void VersionBackend::removeResourceCard(const QString& cardId) {
    m_extraCards.remove(cardId);
    rebuildInstallCards();
}

// ============================================================
// InstallCardModel  implementation
// ============================================================

InstallCardModel::InstallCardModel(QObject* parent)
    : QAbstractListModel(parent) {
    LOG_CARDS() << "InstallCardModel created";
}

int InstallCardModel::rowCount(const QModelIndex& parent) const {
    int c = parent.isValid() ? 0 : m_cards.size();
    LOG_CARDS() << "rowCount() =>" << c << " parent.valid=" << parent.isValid();
    return c;
}

QVariant InstallCardModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_cards.size()) {
        LOG_CARDS() << "data() SKIP idx=" << index.row() << " valid=" << index.isValid() << " size=" << m_cards.size();
        return QVariant();
    }
    const InstallCard& c = m_cards[index.row()];
    LOG_CARDS() << "data() row=" << index.row() << " role=" << role << " name=" << c.name;
    switch (role) {
    case IidRole:       return c.iid;
    case NameRole:      return c.name;
    case TypeRole:      return c.type;
    case ProgressRole:  return c.progress;
    case SpeedRole:     return QVariant::fromValue<qint64>(c.speed);
    case PhaseRole:     return c.phase;
    case RemainingRole: return c.remaining;
    case StepsRole:     return QVariant::fromValue(c.steps);  // QVariantList → JS array directly
    case FailedRole:    return c.failed;
    case ErrorRole:                return c.error;
    case TotalProgressVisibleRole: return c.totalProgressVisible;
    default:                       return QVariant();
    }
}

QHash<int, QByteArray> InstallCardModel::roleNames() const {
    return {
        {IidRole, "iid"},
        {NameRole, "name"},
        {TypeRole, "type"},
        {ProgressRole, "progress"},
        {SpeedRole, "speed"},
        {PhaseRole, "phase"},
        {RemainingRole, "remaining"},
        {StepsRole, "steps"},
        {FailedRole, "failed"},
        {ErrorRole, "error"},
        {TotalProgressVisibleRole, "totalProgressVisible"}
    };
}

void InstallCardModel::rebuild(const QVector<InstallCard>& cards) {
    LOG_CARDS() << "rebuild() START oldSize=" << m_cards.size() << " newSize=" << cards.size()
                 << "gen=" << m_generation;
    for (int i = 0; i < cards.size(); ++i) {
        LOG_CARDS() << "  card[" << i << "] iid=" << cards[i].iid << " name=" << cards[i].name
                     << " progress=" << cards[i].progress << " steps=" << cards[i].steps.size();
    }

    // Same size: update in-place (dataChanged) — preserves delegates & animations
    // Different size: full reset (beginResetModel/endResetModel)
    if (cards.size() == m_cards.size()) {
        for (int i = 0; i < cards.size(); ++i) {
            m_cards[i] = cards[i];
        }
        if (!cards.isEmpty()) {
            QModelIndex topLeft = index(0);
            QModelIndex bottomRight = index(cards.size() - 1);
            emit dataChanged(topLeft, bottomRight);
        }
    } else {
        beginResetModel();
        m_cards = cards;
        endResetModel();
    }

    m_generation++;
    LOG_CARDS() << "rebuild() END rowCount=" << m_cards.size() << " gen=" << m_generation;
    emit generationChanged();
    LOG_CARDS() << "rebuild() emitted generationChanged";
}

// ============================================================
// rebuildInstallCards  -- unified card construction
// ============================================================

void VersionBackend::rebuildInstallCards() {
    // Batch: coalesce multiple calls within one event-loop tick
    if (!m_cardsRebuildPending) {
        m_cardsRebuildPending = true;
        QTimer::singleShot(0, this, [this]() {
            m_cardsRebuildPending = false;
            doRebuildInstallCards();
        });
    }
}

void VersionBackend::doRebuildInstallCards() {
    LOG_CARDS() << "=== doRebuildInstallCards() CALLED ===";
    if (!m_installCardsModel) {
        LOG_CARDS() << "  ABORT: m_installCardsModel is null!";
        return;
    }
    LOG_CARDS() << "  model qobject_cast<QAbstractItemModel>:"
                 << (qobject_cast<QAbstractItemModel*>(m_installCardsModel) ? "OK" : "FAIL")
                 << "qobject_cast<QAbstractListModel>:"
                 << (qobject_cast<QAbstractListModel*>(m_installCardsModel) ? "OK" : "FAIL");

    QVector<InstallCard> cards;
    QSet<QString> seen;

    // 1. Iterate sessions: build mod_loader cards from session state
    for (auto sit = m_sessions.constBegin(); sit != m_sessions.constEnd(); ++sit) {
        const QString& sid = sit.key();
        const InstallSession& ses = sit.value();
        bool mlPending = ses.hasPendingLoader && !ses.pendingLoaderName.isEmpty();
        bool mlActive = m_mlInstaller && m_mlInstaller->isRunning() && m_modLoaderInstallId == sid;
        bool mlMerged = ses.isMerged;
        bool mlFailed = ses.failed;
        if (!mlActive && !mlFailed && !mlPending && !mlMerged) continue;

        QString cardId = mlPending ? ses.pendingLoaderName : sid;
        InstallCard c;
        c.iid = cardId;
        c.type = QStringLiteral("mod_loader");
        c.name = cardId;
        c.progress = mlPending ? 0.0 : ses.smoothProgress;
        c.speed = mlPending ? 0 : m_installSpeed;
        c.phase = mlFailed ? QStringLiteral("\u5931\u8d25") : m_installPhase;
        c.remaining = mlPending ? 0 : installRemainingSteps();
        c.steps = mlPending ? QVariantList{} : ses.steps;
        c.failed = mlFailed;
        c.error = ses.error;
        // Total progress bar: visible only when a download step is active
        {
            bool hasActiveDownload = false;
            if (!mlPending && !mlFailed) {
                for (const QVariant& vs : ses.steps) {
                    QVariantMap step = vs.toMap();
                    if (step.value(QStringLiteral("status")).toString() == QStringLiteral("active")
                        && step.value(QStringLiteral("name")).toString().contains(QStringLiteral("\u4e0b\u8f7d"))) {
                        hasActiveDownload = true;
                        break;
                    }
                }
            }
            c.totalProgressVisible = hasActiveDownload;
        }
        cards.append(c);
        seen.insert(cardId);
        if (ses.isMerged && !ses.mcVersion.isEmpty()) seen.insert(ses.mcVersion);
    }

    // 2. Version cards
    for (const QString& vid : m_activeIds) {
        if (seen.contains(vid)) continue;
        seen.insert(vid);

        InstallCard c;
        c.iid = vid;
        c.type = QStringLiteral("version");
        c.name = vid;
        c.remaining = 0;
        c.steps = QVariantList{};
        c.totalProgressVisible = true;

        // Always build 4-step template (even before first progress event)
        QVariantList vSteps;
        auto addVStep = [&](const QString& name, const QString& status, int pct, qint64 dl, qint64 tot) {
            vSteps.append(QVariantMap{{"name", name}, {"status", status}, {"percentage", pct}, {"show", true}});
        };

        if (m_dlStates.contains(vid)) {
            const DlState& st = m_dlStates[vid];
            bool verifying = (st.phase == QStringLiteral("\u6821\u9a8c\u4e2d..."));
            if (verifying) {
                c.progress = (st.total > 0) ? (qreal)st.progress / st.total : 0.0;
                c.totalProgressVisible = false;
            } else {
                c.progress = qMin(1.0, st.bytesTotal > 0 ? (qreal)st.bytesDl / st.bytesTotal : 0.0);
            }
            c.speed = st.speed;
            c.phase = st.phase;

            // Always build 4 steps (3 download + 1 verify) — verify stays pending until phase transitions
            auto catStatus = [&](int ci) {
                if (verifying) return QStringLiteral("active");
                if (st.catBytesTotal[ci] <= 0) {
                    // Not yet populated by first file, but overall download is active
                    return (st.bytesDl > 0) ? QStringLiteral("active") : QStringLiteral("pending");
                }
                return QStringLiteral("active");
            };
            auto catDl = [&](int ci) { return st.catBytesDl[ci]; };
            auto catTot = [&](int ci) { return st.catBytesTotal[ci]; };

            // JSON downloaded separately before parallel start: completed once download begins
            addVStep(tr("下载版本JSON"),
                     verifying ? QStringLiteral("completed")
                               : (st.bytesDl > 0 ? QStringLiteral("completed") : catStatus(0)),
                     verifying ? 100 : (st.bytesDl > 0 ? 100
                         : ((st.catBytesTotal[0] > 0) ? (int)(st.catBytesDl[0] * 100 / st.catBytesTotal[0]) : 0)),
                     catDl(0), catTot(0));
            addVStep(tr("下载支持库"), verifying ? QStringLiteral("completed") : catStatus(1),
                     (st.catBytesTotal[1] > 0) ? (int)(st.catBytesDl[1] * 100 / st.catBytesTotal[1]) : 0, catDl(1), catTot(1));
            addVStep(tr("下载资源文件"), verifying ? QStringLiteral("completed") : catStatus(2),
                     (st.catBytesTotal[2] > 0) ? (int)(st.catBytesDl[2] * 100 / st.catBytesTotal[2]) : 0, catDl(2), catTot(2));
            addVStep(tr("校验游戏资源完整性"),
                     verifying ? QStringLiteral("active") : QStringLiteral("pending"),
                     verifying ? (st.total > 0 ? (int)(st.progress * 100 / st.total) : 0) : 0,
                     verifying ? st.progress : qint64(0),
                     verifying ? st.total : qint64(0));
            c.remaining = verifying ? 1 : 0;
            c.steps = vSteps;
        } else {
            // DlState not created yet (no progress signal) — build pending template
            addVStep(tr("下载版本JSON"), QStringLiteral("pending"), 0, 0, 0);
            addVStep(tr("下载支持库"), QStringLiteral("pending"), 0, 0, 0);
            addVStep(tr("下载资源文件"), QStringLiteral("pending"), 0, 0, 0);
            addVStep(tr("校验游戏资源完整性"), QStringLiteral("pending"), 0, 0, 0);
            c.steps = vSteps;
        }
        cards.append(c);
    }

    // 3. Resource cards (from m_extraCards)
    for (auto it = m_extraCards.begin(); it != m_extraCards.end(); ++it) {
        const QString& cardId = it.key();
        if (seen.contains(cardId)) continue;
        const QVariantMap& cm = it.value();
        InstallCard c;
        c.iid = cardId;
        c.type = cm.value(QStringLiteral("type"), QStringLiteral("resource")).toString();
        c.name = cm.value(QStringLiteral("displayName"), cardId).toString();
        c.progress = cm.value(QStringLiteral("totalProgress"), 0.0).toReal();
        c.speed = cm.value(QStringLiteral("speed"), QVariant::fromValue<qint64>(0)).value<qint64>();
        c.phase = cm.value(QStringLiteral("installPhase"), QString()).toString();
        c.remaining = 0;
        c.steps = QVariantList{};
        cards.append(c);
    }

    LOG_CARDS() << "  total cards built:" << cards.size();
    m_installCardsModel->rebuild(cards);
    LOG_CARDS() << "=== doRebuildInstallCards() DONE ===";
}

} // namespace ShadowLauncher
