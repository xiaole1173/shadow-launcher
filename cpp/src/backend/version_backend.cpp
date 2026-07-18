// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
// Shadow Launcher — VersionBackend
// QML-facing backend for version list, installation, and lifecycle management.
// Bridges VersionManager (fetch/cache) and VersionDownloader (install pipeline).

#include "version_backend.h"
#include "shadow_backend.h"
#include "userdata_backend.h"
#include "../utils/logger.h"
#include "../core/version_manager.h"
#include "../core/version_downloader.h"
#include "../core/version_isolation.h"
#include "../core/mod_loader_installer.h"
#include "../core/download_coordinator.h"
#include "../core/mc_language.h"

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
#include <QtCore/private/qzipreader_p.h>
#include <QtCore/private/qzipwriter_p.h>
#include <QUrl>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSet>
#include <QMutex>
#include <QMutexLocker>
#include <QSharedPointer>
#include <QDebug>

// Tracing tag
#define LOG_CARDS() if(qEnvironmentVariableIsSet("SHADOW_DEBUG_CARDS")) qDebug() << "[CARDS]"
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
    qCInfo(logVersion) << QStringLiteral("版本后端已初始化");

    m_installCardsModel = new InstallCardModel(this);

    // ProgressTracker — unified speed/progress engine (200ms tick)
    m_progressTracker = new ProgressTracker(this);
    // Speed changes trigger card rebuild (throttled)
    connect(m_progressTracker, &ProgressTracker::speedChanged, this, [this](qint64) {
        rebuildInstallCards();
    });
    connect(m_progressTracker, &ProgressTracker::progressChanged, this, [this](qreal) {
        rebuildInstallCards();
    });

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
        QString curStatus = curStep.value(QStringLiteral("status")).toString();
        if (curStatus != QStringLiteral("active") && curStatus != QStringLiteral("completed")) {
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
        QString st = (percentage >= 100) ? QStringLiteral("completed") : QStringLiteral("active");
        qCDebug(logVersion).noquote()
            << QString("[stepProg] mlStep=%1 mapped=%2 pct=%3 status=%4 merged=%5")
               .arg(step).arg(stepIdx).arg(percentage).arg(st).arg(ses.isMerged ? 1 : 0);
        updateStep(mlId, stepIdx, st, percentage);

        // Non-merged installs: stepProgress drives smoothProgress during install phases
        if (!ses.isMerged) {
            qreal raw = percentage / 100.0;
            ses.totalProgress = raw;
            if (ses.smoothProgress <= 0.0)
                ses.smoothProgress = raw * 0.7;
            else
                ses.smoothProgress = ses.smoothProgress * 0.3 + raw * 0.7;
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
            for (int i = 0; i < ses.steps.size(); i++) {
                // Skip step 7 (Fabric API) if still pending — let API callback mark it
                if (i == 7 && ses.fabricApiPending) continue;
                updateStep(mlId, i, QStringLiteral("completed"), 100);
            }
            // Force progress to 1.0 (EMA may lag on instant/cached completion)
            ses.totalProgress = 1.0;
            ses.smoothProgress = 1.0;
            emit logMessage(tr("[ModLoader] 安装完成"));
            setInstallPhase(tr("完成"));
            updateInstalledList();

            // Only emit "all done" if all sessions are complete
            bool allDone = true;
            for (auto& s : m_sessions) {
                if (s.hasPendingLoader || (s.optifineJarParallel && s.isMerged)) {
                    allDone = false; break;
                }
            }
            if (allDone) {
                if (m_progressTracker) m_progressTracker->reset();
                emit logMessage(tr("[完成] 所有版本安装完成！"));
            }

            // If Fabric API is still downloading in parallel, don't finalize yet
            if (ses.fabricApiPending) {
                qDebug() << "[install] Fabric loader done, waiting for parallel API download";
                return;
            }
            // For merged Fabric install: MC may still be downloading
            // when the loader finishes (Fabric starts immediately in parallel).
            if (ses.isMerged && !ses.mcDownloadDone) {
                qDebug() << "[install] Loader done, waiting for MC download to complete";
                ses.loaderFinishedWaitingMC = true;
                return;
            }

            // Merged install complete — delete residual vanilla MC version folder
            // (Moved here AFTER fabricApiPending/mcDownloadDone checks so isMerged stays true
            //  while sub-items like Fabric API are still in progress, keeping the card alive)
            if (ses.isMerged) {
                bool otherUsingSameMC = false;
                for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
                    if (it.key() != mlId && it.value().isMerged && it.value().mcVersion == ses.mcVersion) {
                        otherUsingSameMC = true;
                        break;
                    }
                }
                if (!otherUsingSameMC) {
                    QString vanillaVerDir = m_gameDir + "/versions/" + ses.mcVersion;
                    QDir vd(vanillaVerDir);
                    if (vd.exists()) {
                        vd.removeRecursively();
                        emit logMessage(tr("[完成] 原版版本文件夹已清理: %1").arg(vanillaVerDir));
                    }
                } else {
                    qCDebug(logVersion) << "Skipping MC cleanup:" << ses.mcVersion << "still in use by other install";
                }
                ses.isMerged = false;
                ses.mcVersion.clear();
                ses.loaderType.clear();
                ses.loaderVer.clear();
            }
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
                // Clean up residual vanilla MC version folder on failure — only if no
                // other active merged install still uses this MC version
                bool otherUsingSameMC = false;
                for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
                    if (it.key() != mlId && it.value().isMerged && it.value().mcVersion == ses.mcVersion) {
                        otherUsingSameMC = true;
                        break;
                    }
                }
                if (!otherUsingSameMC) {
                    QString vanillaVerDir = m_gameDir + "/versions/" + ses.mcVersion;
                    QDir vd(vanillaVerDir);
                    if (vd.exists()) {
                        vd.removeRecursively();
                        emit logMessage(tr("清理残留原版文件夹: %1").arg(vanillaVerDir));
                    }
                }
                ses.isMerged = false;
                ses.mcVersion.clear();
                ses.loaderType.clear();
                ses.loaderVer.clear();
                // Clean up temp Fabric API download on installation failure
                if (!ses.fabricApiSavePath.isEmpty() && QFile::exists(ses.fabricApiSavePath)) {
                    QFile::remove(ses.fabricApiSavePath);
                }
            }
        }
        setInstalling(false);
        emit installFinished(success);
        if (success) {
            // Ensure version isolation for new OptiFine versions
            auto& ses = session(mlId);
            if (ses.loaderType == QStringLiteral("optifine") && m_isolation) {
                m_isolation->migrateToIsolated(mlId);
            }
            finishInstall(mlId);
        }
    });

    // Pause between verify and install (merged install: wait for MC)
    connect(m_mlInstaller, &ModLoaderInstaller::waitingForMC, this, [this]() {
        setInstallPhase(tr("等待MC下载完成..."));
        // Traverse all sessions so we don't rely on m_modLoaderInstallId
        // being stable across async boundaries.
        for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
            auto& ses = it.value();
            if (!ses.isMerged) continue;
            int verifyStep = ses.loaderVerifyStep;
            if (verifyStep >= 0 && verifyStep < ses.steps.size()) {
                updateStep(it.key(), verifyStep, QStringLiteral("completed"), 100);
            }
            ses.loaderDownloadReady = true;
            if (ses.mcDownloadDone) {
                proceedToLoaderInstall(it.key());
            }
        }
    });

    // Byte-level download progress → update current active step
    connect(m_mlInstaller, &ModLoaderInstaller::byteProgress, this,
            [this](const QString& file, qint64 received, qint64 total, qint64 speed) {
                // Speed: feed bytes to ProgressTracker (200ms timer handles EWMA)
        const QString mlId = m_modLoaderInstallId;
        auto& ses = mlId.isEmpty() ? activeSession() : session(mlId);
        if (m_progressTracker && received > ses.mlSpeedLastBytes) {
            // Restart tracker if MC download reset it (e.g. merged installs)
            if (m_progressTracker->phase() != ProgressTracker::Download) {
                m_progressTracker->setPhase(ProgressTracker::Download);
            }
            m_progressTracker->addBytes(received - ses.mlSpeedLastBytes);
        }
        ses.mlSpeedLastBytes = received;
        ses.mlSpeedLastTimeMs = QDateTime::currentMSecsSinceEpoch();
        ses.mlRawSpeed = speed;  // raw download speed for card display
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
                QString st = (pct >= 100) ? QStringLiteral("completed") : QStringLiteral("active");
                updateStep(mlId, ses.loaderStepIdx, st, pct, ses.mlBytesDl, ses.mlBytesAll);
            }
            // Weighted total progress: MC phase 50/50 cat1+cat2, ML phase byte-weighted
            qreal mcPct1 = ses.mcStepTotal[1] > 0 ? (qreal)ses.mcStepDone[1] / ses.mcStepTotal[1] : 0.0;
            qreal mcPct2 = ses.mcStepTotal[2] > 0 ? (qreal)ses.mcStepDone[2] / ses.mcStepTotal[2] : 0.0;
            qreal mcRaw = qMin(1.0, 0.5 * mcPct1 + 0.5 * mcPct2);
            qreal mlRaw = ses.mlBytesAll > 0 ? qMin(1.0, (qreal)ses.mlBytesDl / ses.mlBytesAll) : 0.0;
            qreal raw = mlRaw > 0.0 ? qMin(1.0, (mcRaw + mlRaw) / 2.0) : mcRaw;
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
                    QString st = (pct >= 100) ? QStringLiteral("completed") : QStringLiteral("active");
                    updateStep(mlId, activeIdx, st, pct, received, total);
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
        prefetchVersionJson(versionId);  // background prefetch for install latency
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
    qCInfo(logVersion) << QStringLiteral("开始刷新版本列表");
    emit logMessage(tr("正在获取版本列表..."));
    // Re-scan local versions directory so installed status reflects reality
    // (user may have manually deleted version folders)
    refreshInstalled();
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

void VersionBackend::installVersion(const QString& versionId)
{
    // ── Check if already active ──
    if (m_activeIds.contains(versionId)) {
        emit logMessage(tr("[警告] %1 正在下载中").arg(versionId));
        return;
    }

    // ── Check queue for duplicates ──
    for (const auto& entry : m_installQueue) {
        if (entry == versionId) {
            emit logMessage(tr("[等待] %1 已在下载队列中").arg(versionId));
            return;
        }
    }

    // ── Full: reject instead of queuing ──
    if (m_activeCount >= MAX_CONCURRENT) {
        qCDebug(logLaunch) << "[DOWNLOAD] rejected=" << versionId << "active=" << m_activeCount;
        emit downloadQueueFull(versionId);
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
        emit logMessage(tr("[下载] 发现断点续传文件: %1").arg(versionId));
    }

    // Check for individual chunk .checkpoint.json files
    QDir vDir(versionDir);
    const QStringList checkpointFiles = vDir.entryList(
        {QStringLiteral("*.checkpoint.json")},
        QDir::Files | QDir::NoDotAndDotDot);
    if (!checkpointFiles.isEmpty()) {
        emit logMessage(tr("[下载] 发现 %1 个断点续传块文件")
                            .arg(checkpointFiles.size()));
    }

    // ── Resolve mirror (BMCLAPI for domestic network speed) ──
    MirrorSource mirror = MirrorSource::bmclapi();

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
    setInstallPhase(tr("正在获取 %1 版本信息...").arg(versionId));
    setInstalling(true);
    emit logMessage(tr("正在获取 %1 版本信息...").arg(versionId));
    // Sync pure-MC card phase so it shows progress (not stuck at "处理本地文件")
    m_dlStates[versionId].phase = tr("正在获取 %1 版本信息...").arg(versionId);
    qCDebug(logLaunch) << "[DOWNLOAD] state-set=" << versionId
                        << "active=" << m_activeCount << "/" << MAX_CONCURRENT;

    // ── Fetch version JSON — try local cache first ──
    const QString cachedJsonPath = m_gameDir
        + QStringLiteral("/versions/") + versionId
        + QStringLiteral("/") + versionId + QStringLiteral(".json");
    QFile cachedFile(cachedJsonPath);
    QByteArray cachedData;
    if (cachedFile.exists() && cachedFile.open(QIODevice::ReadOnly)) {
        cachedData = cachedFile.readAll();
        cachedFile.close();
    }

    // ── Race: fetch from BMCLAPI + two Mojang CDN endpoints simultaneously ──
    struct RaceCtx {
        QMutex mtx;
        bool hasWinner = false;
        bool bmclapiDone  = false;
        bool mojangDone   = false;
        bool launchermetaDone = false;
    };
    auto raceCtx = QSharedPointer<RaceCtx>::create();

    // ── Common post-fetch processing (parse JSON → create downloader) ──
    auto processVersionJson = [this, versionId, mirror, targetVersion]
        (const QByteArray& jsonData, const QString& sourceName) {
            qCInfo(logVersion) << QStringLiteral("版本JSON获取成功 源=%1").arg(sourceName);
            emit logMessage(QStringLiteral("[版本] 版本JSON获取成功 %1 %2").arg(sourceName, versionId));

            QJsonParseError parseErr;
            QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseErr);

            if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
                emit logMessage(
                    tr("版本信息格式错误: %1").arg(parseErr.errorString()));
                cancelActiveDownload(versionId);
                return;
            }

            QJsonObject versionJson = doc.object();

            // ── Notify that MC version JSON is ready ──
            emit mcJsonReady(versionId);

            // ── Cancel any existing downloader ──
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

            // Sync download settings from parent ShadowBackend
            auto* sb = qobject_cast<ShadowBackend*>(parent());
            if (sb) {
                DownloadConfig cfg;
                cfg.fileSource = static_cast<DownloadSourcePolicy>(sb->fileDownloadSource());
                cfg.listSource = static_cast<DownloadSourcePolicy>(sb->listDownloadSource());
                cfg.maxWorkers = sb->maxDownloadThreads();
                cfg.speedLimitMB = sb->downloadSpeedLimitMB();
                downloader->setDownloadConfig(cfg);
            }


            // ── Connect downloader signals ──
            connect(downloader,
                    &VersionDownloader::progressChanged, this,
                    [this, versionId, downloader](int cf, int tf, qint64 db, qint64 tb) {
                        // Set per-category totals once (pre-computed from task list, stable).
                        // Re-assigning on every progressChanged can cause percentage to drift backwards.
                        auto& st = m_dlStates[versionId];
                        for (int ci = 0; ci < 3; ci++) {
                            qint64 ct = downloader->categoryTotalBytes(ci);
                            if (ct > 0 && st.catBytesTotal[ci] <= 0)
                                st.catBytesTotal[ci] = ct;
                        }
                        bool firstPulse = (st.bytesDl == 0 && db > 0);
                        if (firstPulse && m_progressTracker) {
                            m_progressTracker->setPhase(ProgressTracker::Download);
                        }
                        // Also inject into session for merged install mod_loader card
                        for (auto sit = m_sessions.begin(); sit != m_sessions.end(); ++sit) {
                            if (sit.value().isMerged && sit.value().mcVersion == versionId) {
                                // Set step totals ONCE (prevent percentage regression)
                                for (int ci = 0; ci < 3; ci++) {
                                    if (sit.value().mcStepTotal[ci] <= 0) {
                                        qint64 ct = downloader->categoryTotalBytes(ci);
                                        if (ct > 0)
                                            sit.value().mcStepTotal[ci] = ct;
                                    }
                                }
                                if (firstPulse) {
                                    // Activate steps 1-2 immediately so they show as downloading
                                    // (even at 0% — avoids gray/pending while waiting for first file of each category)
                                    updateStep(sit.key(), 1, QStringLiteral("active"), 0, 0, 0);
                                    updateStep(sit.key(), 2, QStringLiteral("active"), 0, 0, 0);
                                }
                                break;
                            }
                        }
                        updateDownloadProgress(versionId, cf, tf, db, tb);
                    });

            connect(downloader,
                    &VersionDownloader::fileProgress, this,
                    [this, versionId](const QString& url,
                           const QString& fileName,
                           qint64 received,
                           qint64 total,
                           const QString& savePath) {
                        updateDownloadFile(versionId, url, fileName, received, total, savePath);
                    });

            connect(downloader,
                    &VersionDownloader::verifyProgressChanged, this,
                    [this, versionId](int checked, int total) {
                        // Update per-download state
                        auto& st = m_dlStates[versionId];
                        st.phase = tr("校验中...");
                        st.verifyChecked = checked;
                        st.verifyTotal = total;
                        st.speed = 0;  // No download speed during verify
                        st.downloadsDone = true;  // sticky: downloads are complete
                        // Activate MC verify step for merged installs
                        for (auto sit = m_sessions.begin(); sit != m_sessions.end(); ++sit) {
                            if (sit.value().isMerged && sit.value().mcVersion == versionId) {
                                // MC download is done — mark steps 0-2 as completed
                                for (int i = 0; i < 3 && i < sit.value().steps.size(); i++) {
                                    updateStep(sit.key(), i, QStringLiteral("completed"), 100);
                                }
                                qCInfo(logVersion).noquote()
                                    << QString("[verify] MC download done, verify started. steps 0-2 → completed");
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
                        // Sync primary + ensure cards rebuild for standalone downloads
                        // (merged install path already triggers rebuild via showStep/updateStep above,
                        // but standalone downloads have no session entry, so they never hit that path)
                        syncPrimaryProgress();
                        rebuildInstallCards();
                    });

            connect(downloader,
                    &VersionDownloader::logMessage, this,
                    &VersionBackend::onVersionDownloadLog);

            connect(downloader,
                    &VersionDownloader::downloadFinished, this,
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
            qCInfo(logVersion) << QStringLiteral("开始安装 版本=%1 活跃=%2/%3").arg(versionId).arg(m_activeCount).arg(MAX_CONCURRENT);

            // ── Track downloader in map ──
            m_downloaders[versionId] = downloader;

            emit logMessage(QStringLiteral("[下载] 开始下载版本 %1").arg(versionId));
            downloader->downloadVersion(versionJson, versionId);
        };

    // ── Check prefetch cache (populated by setSelectedVersion background fetch) ──
    if (m_prefetchedJson.contains(versionId)) {
        QByteArray prefetched = m_prefetchedJson.take(versionId);
        qCInfo(logVersion) << QStringLiteral("使用预缓存JSON 版本=%1 大小=%2KB").arg(versionId).arg(prefetched.size() / 1024);
        emit logMessage(tr("[完成] 版本信息已就绪 (预取)"));
        processVersionJson(prefetched, QStringLiteral("prefetch"));
        return;
    }

    // ── Per-source network request helper ──
    auto launchRequest = [this, versionId, raceCtx, processVersionJson, cachedData, cachedJsonPath]
        (const QString& url, const QString& sourceName) {

            auto* nam = new QNetworkAccessManager(this);
            auto req = QNetworkRequest(QUrl(url));
            req.setRawHeader("User-Agent", "ShadowLauncher/1.0");
            req.setTransferTimeout(8000);
            QNetworkReply* reply = nam->get(req);

            // ── Progress feedback ──
            connect(reply, &QNetworkReply::downloadProgress, this,
                    [this, versionId, raceCtx](qint64 recv, qint64 total) {
                        // Race already decided — don't interfere with started downloader
                        {
                            QMutexLocker lock(&raceCtx->mtx);
                            if (raceCtx->hasWinner) return;
                        }
                        int pct = total > 0 ? (int)(recv * 100 / total) : 0;
                        QString phaseText = tr("获取 %1 版本信息: %2%").arg(versionId).arg(pct);
                        setInstallPhase(phaseText);
                        if (m_dlStates.contains(versionId))
                            m_dlStates[versionId].phase = phaseText;
                        for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
                            if (it.value().mcVersion == versionId && !it.value().steps.isEmpty())
                                updateStep(it.key(), 0, QStringLiteral("active"), pct, recv, total);
                        }
                        if (m_sessions.contains(versionId) && !m_sessions[versionId].steps.isEmpty())
                            updateStep(versionId, 0, QStringLiteral("active"), pct, recv, total);
                    });

            // ── Finished handler with race logic ──
            connect(reply, &QNetworkReply::finished, this,
                    [this, reply, nam, versionId, raceCtx, processVersionJson, cachedData, cachedJsonPath, sourceName]() {
                        reply->deleteLater();
                        nam->deleteLater();

                        QByteArray jsonData;
                        bool iAmWinner = false;
                        {
                            QMutexLocker lock(&raceCtx->mtx);
                            if (raceCtx->hasWinner)
                                return;  // already decided

                            if (reply->error() == QNetworkReply::NoError) {
                                raceCtx->hasWinner = true;
                                iAmWinner = true;
                                jsonData = reply->readAll();
                            } else {
                                qCWarning(logVersion) << QStringLiteral("JSON获取失败 源=%1 版本=%2 错误=%3").arg(sourceName, versionId, reply->errorString());
                                if (sourceName == QStringLiteral("BMCLAPI"))
                                    raceCtx->bmclapiDone = true;
                                else if (sourceName == QStringLiteral("LauncherMeta"))
                                    raceCtx->launchermetaDone = true;
                                else
                                    raceCtx->mojangDone = true;
                            }
                        }

                        if (iAmWinner) {
                            qCInfo(logVersion) << QStringLiteral("双源竞速胜出 源=%1").arg(sourceName);
                            processVersionJson(jsonData, sourceName);
                            return;
                        }

                        // Check if both failed → fallback to cache
                        {
                            QMutexLocker lock(&raceCtx->mtx);
                            if (raceCtx->bmclapiDone && raceCtx->mojangDone && raceCtx->launchermetaDone && !raceCtx->hasWinner) {
                                qCWarning(logVersion) << QStringLiteral("双源均失败 回退到本地缓存");
                                lock.unlock();
                                if (!cachedData.isEmpty()) {
                                    qCWarning(logVersion) << QStringLiteral("使用本地缓存版本JSON 版本=%1 路径=%2").arg(versionId, cachedJsonPath);
                                    processVersionJson(cachedData, QStringLiteral("cache"));
                                } else {
                                    emit logMessage(
                                        tr("获取版本信息失败: %1")
                                            .arg(reply->errorString()));
                                    cancelActiveDownload(versionId);
                                }
                            }
                        }
                    });
        };

    // ── Launch BMCLAPI request (no throttle — version JSON is critical path) ──
    QString bmclapiUrl =
        QStringLiteral("https://bmclapi2.bangbang93.com/version/%1/json")
            .arg(versionId);
    launchRequest(bmclapiUrl, QStringLiteral("BMCLAPI"));

    // ── Launch Mojang official request ──
    launchRequest(targetVersion.url, QStringLiteral("Mojang"));

    // ── Launch LauncherMeta CDN request (faster than piston-meta from China) ──
    // targetVersion.url is e.g. https://piston-meta.mojang.com/v1/packages/HASH/VERSION.json
    // Construct alternative: https://launchermeta.mojang.com/v1/packages/HASH/VERSION.json
    QString lmUrl = targetVersion.url;
    lmUrl.replace(QStringLiteral("piston-meta.mojang.com"),
                  QStringLiteral("launchermeta.mojang.com"));
    if (lmUrl != targetVersion.url) {
        launchRequest(lmUrl, QStringLiteral("LauncherMeta"));
    } else {
        // No alternative URL to try — mark as done immediately
        raceCtx->launchermetaDone = true;
    }
}

void VersionBackend::cancelInstall()
{
    // ── Cancel all active downloads safely ──
    // Collect IDs first to avoid iterator invalidation during cancel() callbacks
    QStringList ids;
    for (auto it = m_downloaders.keyBegin(); it != m_downloaders.keyEnd(); ++it) {
        ids.append(*it);
    }
    for (const QString& id : ids) {
        cancelVersionInstall(id);
    }
}

void VersionBackend::cancelCurrentInstall()
{
    // Single-install cancel: find first active and cancel
    if (!m_activeIds.isEmpty()) {
        cancelVersionInstall(m_activeIds.first());
        return;
    }
    // No active downloads — check queue
    if (!m_installQueue.isEmpty()) {
        QString vid = m_installQueue.first();
        m_installQueue.removeFirst();
        emit logMessage(tr("已取消队列中的 %1").arg(vid));
        emit installStateChanged();
        emit downloadQueueChanged();
    }
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
        // Don't close the UI if a loader install is still in progress
        bool loaderRunning = m_mlInstaller && m_mlInstaller->isRunning();
        bool anyMergedPending = false;
        for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
            if (it.value().isMerged && !it.value().mcDownloadDone) {
                anyMergedPending = true;
                break;
            }
        }
        if (!loaderRunning && !anyMergedPending) {
            setInstalling(false);
            setInstallPhase(tr("空闲"));
        }
    }
    emit installStateChanged();
}

void VersionBackend::cancelVersionInstall(const QString& versionId)
{
    qCDebug(logVersion).noquote() << "[cancelInstall] ENTER versionId=" << versionId;

    // ── Resolve session-id to mc-version-id (merged installs key by session id) ──
    QString resolvedId = versionId;
    if (!m_downloaders.contains(versionId) && m_sessions.contains(versionId)) {
        // This is a merged-install session id — cancel the MC downloader
        const QString& mcVer = m_sessions[versionId].mcVersion;
        if (m_downloaders.contains(mcVer)) {
            resolvedId = mcVer;
            qCDebug(logVersion).noquote() << "[cancelInstall] resolved " << versionId << " -> mc=" << mcVer;
        }
    }

    if (!m_downloaders.contains(resolvedId)) {
        // Check queue
        for (int i = 0; i < m_installQueue.size(); ++i) {
            if (m_installQueue[i] == versionId || m_installQueue[i] == resolvedId) {
                m_installQueue.removeAt(i);
                emit logMessage(tr("已取消队列中的 %1").arg(versionId));
                emit installStateChanged();
                emit downloadQueueChanged();
                return;
            }
        }
        return;  // Not found
    }

    // Mark as user-cancelled (onVersionDownloadFinished handles full cleanup)
    m_userCancelledIds.insert(resolvedId);

    // Cancel the downloader (just sets flag; downloader finishes naturally)
    auto dl = m_downloaders.value(resolvedId, nullptr);
    if (dl) {
        dl->cancel();
    }

    emit logMessage(tr("已取消 %1 的安装").arg(versionId));

    // ── Update installing state (UI only; counts handled by finish handler) ──
    m_activeIds.removeOne(resolvedId);
    if (m_activeIds.isEmpty()) {
        setInstalling(false);
    }

    // ── Sync UI immediately ──
    syncPrimaryProgress();
    rebuildInstallCards();

    // Try to start next from queue
    startNextFromQueue();
}

void VersionBackend::cleanupCanceledVersion(const QString& versionId, const QString& gameDir)
{
    // On cancel/fail: delete the entire version folder
    // Safety guard: if saves/ already exists, the version has been played — don't touch it.
    const QString versionDir = gameDir + QStringLiteral("/versions/") + versionId;
    const QString savesDir = versionDir + QStringLiteral("/saves");

    if (QDir(savesDir).exists()) {
        qCInfo(logVersion).noquote()
            << QStringLiteral("[cancel-cleanup] %1: 版本已独立启动, 不清理 (saves/ 存在)")
                   .arg(versionId);
        return;
    }

    qCInfo(logVersion).noquote()
        << QStringLiteral("[cancel-cleanup] %1: 清理版本文件夹 → %2")
               .arg(versionId, versionDir);

    QDir dir(versionDir);
    if (dir.exists()) {
        if (!dir.removeRecursively()) {
            qCWarning(logVersion).noquote()
                << QStringLiteral("[cancel-cleanup] %1: 清理失败 (部分文件可能被占用)")
                       .arg(versionId);
        }
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
        qCWarning(logVersion) << QStringLiteral("下载完成回调收到未知发送者");
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
        qCWarning(logVersion) << QStringLiteral("下载完成回调找不到对应下载器");
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

        qCInfo(logVersion) << QStringLiteral("安装完成 版本=%1").arg(finishedId);
        emit logMessage(
            tr("[完成] %1 安装完成")
                .arg(finishedId));

        // ── Auto-language: IP region mode — write options.txt after install ──
        if (m_autoLangMode == 2 && !m_detectedRegion.isEmpty()) {
            QString gameDirPath = m_versionMgr->gameDir()
                + QStringLiteral("/versions/") + finishedId
                + QStringLiteral("/game");
            QString mcLang = mc_language::regionToMinecraftLang(m_detectedRegion);
            mc_language::writeOptionsTxt(gameDirPath, mcLang);
            qCInfo(logVersion) << QStringLiteral("安装后语言调整 区域=%1 lang=%2 版本=%3")
                .arg(m_detectedRegion, mcLang, finishedId);
        }

        refreshInstalled();
    } else if (m_userCancelledIds.remove(finishedId)) {
        // User cancelled — delete entire version folder
        qCDebug(logVersion).noquote() << "onVersionDownloadFinished: user cancelled" << finishedId;
        cleanupCanceledVersion(finishedId, m_versionMgr->gameDir());
    } else {
        const auto& st = m_dlStates.value(finishedId);
        bool wasVerifying = (st.phase == tr("校验中..."));
        if (wasVerifying) {
            QString errDetail = error.isEmpty()
                                    ? tr("校验失败")
                                    : error;
            qCCritical(logVersion) << QStringLiteral("校验失败 版本=%1 详情=%2").arg(finishedId, errDetail);
            emit logMessage(
                tr("[失败] %1 校验失败: %2")
                    .arg(finishedId, errDetail));
        } else {
            QString errDetail = error.isEmpty()
                                    ? tr("未知错误")
                                    : error;
            qCCritical(logVersion) << QStringLiteral("安装失败 版本=%1 详情=%2").arg(finishedId, errDetail);
            emit logMessage(
                tr("[失败] %1 安装失败: %2")
                    .arg(finishedId, errDetail));
        }
    }

    // ── Emit finished signal (per-download) ──
    emit installFinished(success);

    // ── Update overall state ──
    // Snapshot whether this download belongs to a merged install BEFORE
    // proceedToLoaderInstall can trigger onModLoaderFinished which clears
    // isMerged + mcVersion in a re-entrant call (Fabric finalize is synchronous).
    bool isMergedInstall = false;

    if (m_activeCount == 0) {
        // Check ALL sessions for merged installs waiting on this MC version
        bool anyMergedLaunched = false;
        for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
            auto& ses = it.value();
            if (ses.isMerged && ses.mcVersion == finishedId) {
                isMergedInstall = true;
                // Merged install: MC done, check if loader also done
                qDebug() << "[install] Merged: MC complete" << ses.loaderType;
                ses.mcBytesDl = ses.mcBytesAll;
                for (int i = 0; i < 3 && i < ses.steps.size(); i++) {
                    updateStep(it.key(), i, QStringLiteral("completed"), 100);
                }
                int verifyIdx = 3;
                // Don't mark step 3 as done for OptiFine parallel mode (JAR still downloading)
                if (verifyIdx < ses.steps.size() && !ses.optifineJarParallel) {
                    updateStep(it.key(), verifyIdx, QStringLiteral("completed"), 100);
                }
                ses.mcDownloadDone = true;
                // For parallel OptiFine: check if JAR is also done
                if (ses.optifineJarParallel && ses.optifineJarDone) {
                    emit logMessage(tr("[完成] MC 和 OptiFine 均下载完成"));
                    onParallelOptifineDone(it.key(), QByteArray());
                } else if (ses.loaderDownloadReady) {
                    proceedToLoaderInstall(it.key());
                } else if (ses.loaderFinishedWaitingMC) {
                    qDebug() << "[install] MC done, loader was already finished — triggering installFinalize";
                    ses.loaderFinishedWaitingMC = false;
                    finishInstall(it.key());
                } else {
                    qDebug() << "[install] MC done, waiting for loader download...";
                }
                anyMergedLaunched = true;
            }
        }
        if (!anyMergedLaunched) {
            setInstalling(false);
            setInstallPhase(tr("完成"));
            if (m_progressTracker) m_progressTracker->reset();
            emit logMessage(tr("[完成] 所有版本安装完成！"));
        }
        // When anyMergedLaunched is true, the OptiFine installer is still running
        // — completion announcement is deferred to onModLoaderFinished
    } else {
        // Still have active downloads — sync primary display AND notify QML to re-read
        syncPrimaryProgress();
        emit installStateChanged();
    }

    // Emit installComplete for non-merged installs ONLY.
    // Uses isMergedInstall captured above — not re-checked after
    // proceedToLoaderInstall may have cleared session state.
    if (success && !isMergedInstall) {
        // Check for pending user data import (pure MC install)
        if (m_pendingImports.contains(finishedId)) {
            QString archivePath = m_pendingImports.take(finishedId);
            // Run import synchronously
            emit logMessage(tr("正在导入用户数据..."));
            QZipReader zip(archivePath);
            if (zip.status() == QZipReader::NoError) {
                QString targetGame = m_gameDir + "/versions/" + finishedId + "/game";
                QDir().mkpath(targetGame);
                auto allFiles = zip.fileInfoList();
                for (const auto& fi : allFiles) {
                    if (!fi.filePath.startsWith("game/")) continue;
                    if (fi.isDir) continue;
                    QString relPath = fi.filePath.mid(5);
                    QString dstPath = targetGame + "/" + relPath;
                    QDir().mkpath(QFileInfo(dstPath).absolutePath());
                    if (QFileInfo::exists(dstPath)) {
                        QString fn = QFileInfo(dstPath).fileName();
                        if (fn == "options.txt" || fn == "servers.dat") continue;
                    }
                    QFile out(dstPath);
                    if (out.open(QIODevice::WriteOnly)) {
                        out.write(zip.fileData(fi.filePath));
                        out.close();
                    }
                }
                zip.close();
                emit logMessage(tr("用户数据导入完成"));
            } else {
                emit logMessage(tr("用户数据导入失败：ZIP文件读取失败"));
            }
        }
        emit installComplete(finishedId);
    }

    // ── Try to start next from queue ──
    startNextFromQueue();
    qCDebug(logLaunch) << "[DOWNLOAD] finished=" << finishedId << " active=" << m_activeCount << "/" << MAX_CONCURRENT << " queue=" << m_installQueue.size();

    // Check ALL sessions for pending loaders waiting for this MC version
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        auto& ses = it.value();
        if (ses.hasPendingLoader && success && ses.pendingLoaderMc == finishedId) {
            qDebug() << "[install] MC" << finishedId << "installed, checking pending loader:" << ses.pendingLoaderName;
            
            // If loader was downloaded in parallel, the merged flow above already handled it
            if (ses.loaderDownloadReady || ses.isMerged) {
                qDebug() << "[install] Pending loader already parallel-downloaded, merged flow handles install";
                ses.hasPendingLoader = false;
                continue;
            }
            if (ses.pendingLoaderType == QStringLiteral("optifine")) {
                if (ses.isMerged) {
                    if (ses.optifineJarParallel) {
                        // Parallel mode: MC just finished, check if OptiFine JAR is also done
                        if (!ses.mcDownloadDone) {
                            ses.mcDownloadDone = true;
                            if (ses.optifineJarDone) {
                                onParallelOptifineDone(ses.pendingLoaderName, QByteArray());
                            }
                        }
                    } else {
                        // Sequential mode: MC done, now download OptiFine JAR
                        finishOptifineMerged(ses.pendingLoaderMc, ses.pendingLoaderName);
                    }
                } else {
                    installOptifine(ses.pendingLoaderMc, ses.pendingLoaderVer, QString(), ses.pendingLoaderName);
                }
            } else {
                // Forge / NeoForge / Fabric: call installModLoader with stored params
                installModLoader(ses.pendingLoaderMc, ses.pendingLoaderType,
                                 ses.pendingLoaderVer, ses.pendingLoaderName,
                                 ses.fabricApiVersion, ses.fabricApiUrl,
                                 ses.fabricApiSavePath, ses.forgeInstallerSha1);
            }
            break;  // Only one pending loader per MC version
        }
    }
}

void VersionBackend::cancelQueuedDownload(const QString& versionId)
{
    // Remove from queue
    for (int i = 0; i < m_installQueue.size(); ++i) {
        if (m_installQueue[i] == versionId) {
            m_installQueue.removeAt(i);
            emit logMessage(tr("已取消队列中的 %1").arg(versionId));
            emit installStateChanged();
            emit downloadQueueChanged();
            return;
        }
    }
    // If it's currently installing, cancel that specific one
    if (m_activeIds.contains(versionId)) {
        cancelVersionInstall(versionId);
    }
}

QVariantList VersionBackend::downloadQueue() const
{
    QVariantList list;
    for (int i = 0; i < m_installQueue.size(); ++i) {
        QVariantMap entry;
        entry[QStringLiteral("versionId")] = m_installQueue[i];
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
        m_mlInstaller->fabricFinalize();
    }
}

void VersionBackend::finishInstall(const QString& installName)
{
    qCInfo(logVersion) << QStringLiteral("安装完成 版本=%1").arg(installName);
    if (installName.isEmpty()) {
        emit installComplete(QString());
        emit installFinished(true);
        setInstalling(false);
        return;
    }
    auto& ses = session(installName);

    // Check for pending user data import
    if (ses.hasImportPending && !ses.importArchivePath.isEmpty()) {
        // Start user data import before completing
        startUserDataImport(installName);
        return;  // installComplete will be emitted by startUserDataImport after import finishes
    }

    // Move Fabric API from temp to mods/ (on any completion path)
    if (!ses.fabricApiSavePath.isEmpty() && !ses.fabricApiFinalPath.isEmpty()) {
        if (QFile::exists(ses.fabricApiSavePath)) {
            QDir().mkpath(QFileInfo(ses.fabricApiFinalPath).absolutePath());
            if (QFile::exists(ses.fabricApiFinalPath)) QFile::remove(ses.fabricApiFinalPath);
            if (QFile::rename(ses.fabricApiSavePath, ses.fabricApiFinalPath)) {
                qDebug() << "[install] Fabric API moved to mods/:" << ses.fabricApiFinalPath;
            } else {
                qWarning() << "[install] Fabric API move failed:" << ses.fabricApiSavePath << "->" << ses.fabricApiFinalPath;
            }
        }
        // Clean up temp directory
        QDir tempParent = QFileInfo(ses.fabricApiSavePath).dir();
        tempParent.rmdir(QFileInfo(ses.fabricApiSavePath).fileName());
        QDir tempRoot(QDir::tempPath() + QStringLiteral("/shadow-fabric-api"));
        if (tempRoot.exists() && tempRoot.isEmpty()) tempRoot.rmdir(QStringLiteral("."));
    }

    updateStep(installName, 7, QStringLiteral("completed"), 100);
    // Clear merged flag so the card can retire after installComplete signal
    if (ses.isMerged) {
        ses.isMerged = false;
        ses.mcVersion.clear();
        ses.loaderType.clear();
        ses.loaderVer.clear();
    }
    // Emit complete BEFORE setInstalling(false) so QML can show final state
    emit installComplete(installName);
    emit installFinished(true);
    // Defer hiding install card so installComplete signal reaches QML first
    QTimer::singleShot(0, this, [this]() {
        setInstalling(false);
    });
}

void VersionBackend::setInstalling(bool v)
{
    bool wasInstalling = m_installing || (m_activeCount > 0);
    m_installing = v;
    bool isNowInstalling = m_installing || (m_activeCount > 0);
    qCInfo(logVersion) << QStringLiteral("安装状态变更 安装中=%1 活跃任务=%2").arg(v ? QStringLiteral("是") : QStringLiteral("否")).arg(m_activeCount);
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
        rebuildInstallCards();
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
        if (m_progressTracker) m_progressTracker->reset();
        setInstallPhase(tr("空闲"));
                                emit installStateChanged();
        return;
    }

    const auto& st = m_dlStates[pid];
    m_installBytesDl = st.bytesDl;
    m_installBytesTotal = st.bytesTotal;
    // m_installSpeed removed — using ProgressTracker;

    // Two-segment progress: download 0-90%, verify 90-100%
    bool verifying = (st.phase == QStringLiteral("\u6821\u9a8c\u4e2d..."));
    if (verifying) {
        if (st.verifyTotal > 0) {
            m_installBytesDl = st.verifyChecked;
            m_installBytesTotal = st.verifyTotal;
        }
    } else {
        m_installBytesDl = st.bytesDl;
        m_installBytesTotal = st.bytesTotal;
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

    // Feed delta to unified ProgressTracker (200ms timer handles speed)
    qint64 delta = db - st.bytesDl;
    if (delta > 0 && m_progressTracker) m_progressTracker->addBytes(delta);

    // ── Per-State speed (sliding window 3s) ──
    qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    st.speedLastTimeMs = nowMs;
    if (delta > 0) {
        qint64 totalBytes = st.bytesDl + delta;
        st.speedWindow.append({nowMs, totalBytes});
        // Expire samples outside the 3s window
        while (!st.speedWindow.isEmpty() && (nowMs - st.speedWindow.first().timeMs) > st.kSpeedWindowMs)
            st.speedWindow.removeFirst();
        // Calculate speed from oldest-to-newest in window
        if (st.speedWindow.size() >= 2) {
            qint64 winMs = st.speedWindow.back().timeMs - st.speedWindow.front().timeMs;
            if (winMs > 0)
                st.speed = (st.speedWindow.back().bytes - st.speedWindow.front().bytes) * 1000 / winMs;
        }
    }

    st.bytesDl = db;
    st.bytesTotal = tb;

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
        // Both numerator and denominator come from per-category accounting,
        // not the downloader's global db/tb (which may include uncategorized files
        // like Mojang fallback URLs that don't match /version/, /libraries/, /assets/).
        qint64 catSum = st.catBytesTotal[0] + st.catBytesTotal[1] + st.catBytesTotal[2];
        qint64 catDl = st.catBytesDl[0] + st.catBytesDl[1] + st.catBytesDl[2];
        mSes.mcBytesDl = catDl;
        if (catSum > 0) {
            mSes.mcBytesAll = catSum;
        }
        // Push real per-category byte progress to all 3 MC download steps
        // Only during download phase — during verify, steps are already marked completed
        if (st.phase != tr("校验中...")) {
            bool allDone = true;
            for (int ci = 0; ci < 3; ci++) {
                if (mSes.mcStepTotal[ci] > 0) {
                    int pct = (int)(mSes.mcStepDone[ci] * 100 / mSes.mcStepTotal[ci]);
                    QString st = (pct >= 100) ? QStringLiteral("completed") : QStringLiteral("active");
                    updateStep(mergedSessionId, ci, st, pct,
                               mSes.mcStepDone[ci], mSes.mcStepTotal[ci]);
                    if (pct < 100) allDone = false;
                } else {
                    // Empty category (0 bytes) → already done, force display as 100%
                    qCInfo(logVersion) << "Empty cat" << ci << "total=" << mSes.mcStepTotal[ci]
                                       << "done=" << mSes.mcStepDone[ci]
                                       << "→ marking completed for" << mergedSessionId;
                    updateStep(mergedSessionId, ci, QStringLiteral("completed"), 100, 0, 0);
                }
            }
            // When all 3 download steps reach 100%, proactively show verify step (index 3)
            // to avoid a blank gap before the downloader enters the verify phase.
            if (allDone) {
                int verifyIdx = 3;
                QVariantMap vstep = mSes.steps.value(verifyIdx).toMap();
                if (verifyIdx < mSes.steps.size() && !vstep.value("show").toBool()) {
                    showStep(mergedSessionId, verifyIdx);
                    updateStep(mergedSessionId, verifyIdx, QStringLiteral("active"), 0,
                               0, mSes.mcBytesAll);
                    qCInfo(logVersion) << "Proactively showing MC verify step for" << mergedSessionId;
                }
            }
        }
        // Weighted progress for merged install MC phase: cat1 50%, cat2 50%.
        // Each category uses its own byte progress internally.
        {
            qreal mcPct1 = mSes.mcStepTotal[1] > 0 ? (qreal)mSes.mcStepDone[1] / mSes.mcStepTotal[1] : 0.0;
            qreal mcPct2 = mSes.mcStepTotal[2] > 0 ? (qreal)mSes.mcStepDone[2] / mSes.mcStepTotal[2] : 0.0;
            qreal mcRaw = qMin(1.0, 0.5 * mcPct1 + 0.5 * mcPct2);
            qreal mlRaw = mSes.mlBytesAll > 0 ? qMin(1.0, (qreal)mSes.mlBytesDl / mSes.mlBytesAll) : 0.0;
            qreal raw = mlRaw > 0.0 ? qMin(1.0, (mcRaw + mlRaw) / 2.0) : mcRaw;
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
        // m_installSpeed removed — using ProgressTracker;

        // Byte-weighted total progress ── raw ──
        // Find merged session for grand-total calculation
        qreal rawTotalProgress = 0.0;
        if (!mergedSessionId.isEmpty()) {
            qint64 grandTotal = session(mergedSessionId).mcBytesAll + session(mergedSessionId).mlBytesAll;
            qint64 grandDone = session(mergedSessionId).mcBytesDl + session(mergedSessionId).mlBytesDl;
            rawTotalProgress = (grandTotal > 0)
                ? qBound(0.0, (qreal)grandDone / grandTotal, 1.0)
                : (tb > 0 ? qBound(0.0, (qreal)db / tb, 1.0) : 0.0);
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
                                        qint64 total,
                                        const QString& savePath)
{
    auto& st = m_dlStates[versionId];
    st.file = fileName;

    // ── Determine category for merged-install step routing ──
    int cat = -1;
    // Category 0: version JSON (downloaded via HTTP race in installVersion, not by downloader)
    // Category 1: client.jar + libraries (/version/, /versions/, /libraries/, /maven/)
    // Category 2: assets (/assets/)
    // NOTE: Must NOT use fileName.endsWith(".jar") — library files also end with .jar!
    if (savePath.contains(QStringLiteral("/versions/")) || savePath.contains(QStringLiteral("/version/"))
        || savePath.contains(QStringLiteral("/libraries/")) || savePath.contains(QStringLiteral("/maven/"))) cat = 1;
    else if (savePath.contains(QStringLiteral("/assets/"))) cat = 2;

    if (versionId == primaryVersionId()) {
            }

    // ── Per-category done-byte tracking for DlState (version cards) ──
    // Denominator (catBytesTotal) comes from progressChanged->categoryTotalBytes()
    if (cat >= 0 && cat <= 2) {
        qint64 catDone = st.catBytesDoneBase[cat] + received;
        if (received >= total && total > 0) {
            st.catBytesDoneBase[cat] += total;
            catDone = st.catBytesDoneBase[cat];
            // Log completed file
            emit logMessage(QStringLiteral("[下载] ") + fileName + QStringLiteral(" (%1 KB)").arg(total/1024));
        }
        st.catBytesDl[cat] = catDone;

        qCDebug(logVersion).noquote()
            << QString("[catDl] version=%1 cat=%2 dl=%3/%4KB (%5%) file=%6")
               .arg(versionId).arg(cat)
               .arg(catDone/1024).arg(st.catBytesTotal[cat]/1024)
               .arg(st.catBytesTotal[cat] > 0 ? catDone * 100 / st.catBytesTotal[cat] : 0)
               .arg(fileName);
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
            auto& ses = session(mergedSessionId);
            // Use per-category partial-progress accounting (includes in-flight file),
            // same as total progress numerator, so sub-steps and total bar stay in lockstep.
            ses.mcStepDone[cat] = st.catBytesDl[cat];
            qint64 after = ses.mcStepDone[cat];
            qint64 ct = ses.mcStepTotal[cat];
            if (ct > 0) {
                qCInfo(logVersion).noquote()
                    << QString("[step] cat=%1 file=%2 total=%3KB → %4/%5KB (%6%)")
                       .arg(cat).arg(fileName).arg(total/1024)
                       .arg(after/1024).arg(ct/1024)
                       .arg(after * 100 / ct);
            }
        }
    }

    // Unified: check whether all 3 download categories are done
    activateVerifyOnDownloadsDone(versionId);
}

void VersionBackend::startNextFromQueue()
{
    while (m_activeCount < MAX_CONCURRENT && !m_installQueue.isEmpty()) {
        QString nextId = m_installQueue.dequeue();
        qCDebug(logLaunch) << "[DOWNLOAD] dequeue=" << nextId << " remaining=" << m_installQueue.size();
        emit downloadQueueChanged();
        emit logMessage(tr("▶ 开始队列中下一个版本: %1").arg(nextId));
        // Direct recursive call — installVersion will enqueue again if still full
        QTimer::singleShot(200, this, [this, nextId]() {
            installVersion(nextId);
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
    const int REPORT_EVERY = 5;  // report every 5 files for smooth progress
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

        // Report every N items OR on the last item
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
    qCInfo(logVersion) << QStringLiteral("校验开始 版本=%1").arg(versionId);
    emit logMessage(tr("正在校验版本 %1...").arg(versionId));

    const QString versionDir = m_gameDir + QStringLiteral("/versions/") + versionId;
    const QString jsonPath = versionDir + QStringLiteral("/") + versionId + QStringLiteral(".json");

    // Load version JSON
    QFile jsonFile(jsonPath);
    if (!jsonFile.open(QIODevice::ReadOnly)) {
        emit logMessage(tr("[失败] 无法读取版本配置: %1").arg(jsonPath));
        setInstallPhase(tr("空闲"));
        emit verifyFinished(false);
        return;
    }

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(jsonFile.readAll(), &parseErr);
    jsonFile.close();

    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        emit logMessage(tr("[失败] 版本配置解析失败: %1").arg(parseErr.errorString()));
        setInstallPhase(tr("空闲"));
        emit verifyFinished(false);
        return;
    }

    QJsonObject versionJson = doc.object();

    // ── Phase A: Collect SHA1 expected values, emit progress 0→40% ──
    emit verifyProgress(0, 100);
    emit logMessage(tr("正在收集文件校验信息..."));

    // Count items first for progress estimation
    int estTotal = 2; // client.jar + libraries
    QJsonArray libraries = versionJson.value(QStringLiteral("libraries")).toArray();
    estTotal += libraries.size();
    QJsonObject assetIdx2 = versionJson.value(QStringLiteral("assetIndex")).toObject();
    if (!assetIdx2.isEmpty()) {
        QString idxId2 = assetIdx2.value(QStringLiteral("id")).toString(QStringLiteral("legacy"));
        QString idxPath2 = m_gameDir + QStringLiteral("/assets/indexes/") + idxId2 + QStringLiteral(".json");
        if (QFileInfo::exists(idxPath2)) {
            QFile idxFile2(idxPath2);
            if (idxFile2.open(QIODevice::ReadOnly)) {
                QJsonDocument idxDoc2 = QJsonDocument::fromJson(idxFile2.readAll());
                idxFile2.close();
                estTotal += idxDoc2.object().value(QStringLiteral("objects")).toObject().size();
            }
        }
    }

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
    QJsonArray libraries2 = versionJson.value(QStringLiteral("libraries")).toArray();
    for (const QJsonValue& libVal : libraries2) {
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

    // Progress: libraries collected (~10%)
    emit verifyProgress(10, 100);

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
                int assetCount = 0;
                int totalAssets = objects.size();
                for (auto it = objects.begin(); it != objects.end(); ++it) {
                    QJsonObject obj = it.value().toObject();
                    QString sha1 = obj.value(QStringLiteral("hash")).toString();
                    QString prefix = sha1.left(2);
                    VerifyItem item;
                    item.path = objectsDir + QStringLiteral("/") + prefix + QStringLiteral("/") + sha1;
                    item.sha1 = sha1;
                    item.name = QStringLiteral("assets/%1").arg(it.key());
                    items.append(item);
                    assetCount++;
                    // Progress during asset collection: 10→40%
                    if (assetCount % 500 == 0 || assetCount == totalAssets) {
                        int pct = 10 + (assetCount * 30 / totalAssets);
                        emit verifyProgress(pct, 100);
                    }
                }
            }
        }
    }

    const int total = items.size();
    m_verifyTotal = total;
    emit logMessage(tr("校验 %1 个文件...").arg(total));

    // ── Phase A complete: emit 40% (collection done, verification about to start) ──
    emit verifyProgress(40, 100);

    // ── Create worker + thread ──
    m_verifyThread = new QThread(this);
    m_verifyWorker = new VerifyWorker;
    m_verifyWorker->setItems(items);
    m_verifyWorker->moveToThread(m_verifyThread);

    // Progress: forward to QML — Phase B: map 0..total → 40..100%
    connect(m_verifyWorker, &VerifyWorker::progressChecked, this,
            [this](int checked, int total) {
                m_verifyChecked = checked;
                m_verifyTotal = total;
                // Map to 40-100% range
                int pct = total > 0 ? (40 + checked * 60 / total) : 40;
                emit verifyProgress(pct, 100);
            }, Qt::QueuedConnection);

    // Cancelled
    connect(m_verifyWorker, &VerifyWorker::cancelled, this,
            [this](int checked, int total) {
                m_verifyChecked = checked;
                m_verifyTotal = total;
                int pct = total > 0 ? (40 + checked * 60 / total) : 40;
                emit verifyProgress(pct, 100);
                emit logMessage(tr("[警告] 校验已取消"));
                setInstallPhase(tr("空闲"));
                m_verifyRunning.storeRelease(0);
                emit verifyCancelled();
            }, Qt::QueuedConnection);

    // Finished
    connect(m_verifyWorker, &VerifyWorker::finished, this,
            [this](bool allPassed, const QStringList& failedFiles,
                   const QStringList& failedPaths) {
                if (allPassed) {
                    qCInfo(logVersion) << QStringLiteral("校验完成 文件全部通过 数量=%1").arg(m_verifyTotal);
                    emit logMessage(tr("[完成] 校验完成: %1 个文件全部通过").arg(m_verifyTotal));
                } else {
                    int failed = failedFiles.size();
                    qCCritical(logVersion) << QStringLiteral("校验完成 失败=%1/%2").arg(failed).arg(m_verifyTotal);

                    QString detail = failedFiles.join(QStringLiteral(", "));
                    if (detail.length() > 250) {
                        detail = detail.left(250) + QStringLiteral("...");
                    }
                    emit logMessage(tr("[失败] 校验完成: %1/%2 个文件失败").arg(failed).arg(m_verifyTotal));
                    emit logMessage(tr("   失败文件: %1").arg(detail));
                    emit verifyFailedFiles(failedFiles);

                    // Store failed paths for later cleanup
                    m_failedPathsCache = failedPaths;
                }

                setInstallPhase(tr("空闲"));
                emit verifyFinished(allPassed);

                m_verifyRunning.storeRelease(0);

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
        m_verifyRunning.storeRelease(0);
        emit logMessage(tr("正在取消校验..."));
    }
    if (m_repairRunning.loadRelaxed()) {
        m_repairRunning.storeRelease(0);
        emit repairRunningChanged();
        setInstallPhase(tr("空闲"));
        emit logMessage(tr("正在取消修复..."));
        emit verifyCancelled();
        emit verifyFinished(false);
    }
}

// ============================================================
// Version management: clean corrupt files
// ============================================================

void VersionBackend::cleanCorruptVersion(const QString& versionId)
{
    if (m_failedPathsCache.isEmpty()) {
        emit logMessage(tr("没有可清理的校验结果，先执行校验识别损坏文件..."));
        verifyVersion(versionId);
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
    emit logMessage(tr("[清理] 清理完成: 删除 %1 个损坏文件, %2 个文件已不存在")
                        .arg(removed).arg(cleaned));
    emit logMessage(tr("[提示] 请重新下载版本 %1 以恢复缺失文件").arg(versionId));
}

// ============================================================
// Inline repair pipeline: download & verify each failed file without jumping to DownloadProgressPage
// ============================================================
void VersionBackend::repairVersion(const QString& versionId)
{
    if (m_failedPathsCache.isEmpty()) {
        emit logMessage(tr("没有待修复的文件缓存，正在重新校验..."));
        verifyVersion(versionId);
        return;
    }

    // ── Read version JSON to build file → {sha1, url} lookup ──
    QString jsonPath = m_gameDir + QStringLiteral("/versions/") + versionId
                       + QStringLiteral("/") + versionId + QStringLiteral(".json");
    QFile jsonFile(jsonPath);
    if (!jsonFile.open(QIODevice::ReadOnly)) {
        emit logMessage(tr("[失败] 无法读取版本 JSON: %1").arg(jsonPath));
        emit verifyFinished(false);
        return;
    }
    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(jsonFile.readAll(), &parseErr);
    jsonFile.close();
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        emit logMessage(tr("[失败] 版本 JSON 解析失败: %1").arg(parseErr.errorString()));
        emit verifyFinished(false);
        return;
    }
    QJsonObject versionJson = doc.object();

    // Build a struct for each repair target
    struct RepairTarget {
        QString filePath;      // absolute filesystem path to write
        QString primaryUrl;    // BMCLAPI (or preferred mirror)
        QString fallbackUrl;   // Mojang official
        QString sha1;          // expected SHA-1 hash (empty for client.jar without sha1)
    };
    QList<RepairTarget> targets;

    // Helper: get DownloadSourcePolicy from parent (ShadowBackend)
    DownloadSourcePolicy fileSource = DownloadSourcePolicy::PreferMirror;
    auto* sb = qobject_cast<ShadowBackend*>(parent());
    if (sb)
        fileSource = static_cast<DownloadSourcePolicy>(sb->fileDownloadSource());

    // Mirror base URLs
    const QString bmclapiLibBase  = QStringLiteral("https://bmclapi2.bangbang93.com/maven");
    const QString mojangLibBase   = QStringLiteral("https://libraries.minecraft.net");
    const QString bmclapiResBase  = QStringLiteral("https://bmclapi2.bangbang93.com/assets");
    const QString mojangResBase   = QStringLiteral("https://resources.download.minecraft.net");
    const QString bmclapiJarHost  = QStringLiteral("bmclapi2.bangbang93.com");

    const QString libsDir    = m_gameDir + QStringLiteral("/libraries");
    const QString objectsDir = m_gameDir + QStringLiteral("/assets/objects");
    const QString versionDir = m_gameDir + QStringLiteral("/versions/") + versionId;

    // 1) client.jar
    {
        QJsonObject client = versionJson.value(QStringLiteral("downloads")).toObject()
                                        .value(QStringLiteral("client")).toObject();
        QString jarPath = versionDir + QStringLiteral("/") + versionId + QStringLiteral(".jar");
        if (m_failedPathsCache.contains(jarPath)) {
            QString sha1 = client.value(QStringLiteral("sha1")).toString();
            QString origUrl = client.value(QStringLiteral("url")).toString();
            QString primary, fallback;
            if (fileSource == DownloadSourcePolicy::PreferOfficial) {
                primary = origUrl;
                fallback = QStringLiteral("https://%1/version/%2/%2.jar").arg(bmclapiJarHost, versionId);
            } else {
                primary = QStringLiteral("https://%1/version/%2/%2.jar").arg(bmclapiJarHost, versionId);
                fallback = origUrl;
            }
            targets.append({jarPath, primary, fallback, sha1});
        }
    }

    // 2) Libraries
    {
        QJsonArray libs = versionJson.value(QStringLiteral("libraries")).toArray();
        for (const QJsonValue& libVal : libs) {
            QJsonObject lib = libVal.toObject();
            QJsonObject artifact = lib.value(QStringLiteral("downloads")).toObject()
                                     .value(QStringLiteral("artifact")).toObject();
            if (artifact.isEmpty()) continue;
            QString relPath = artifact.value(QStringLiteral("path")).toString();
            if (relPath.isEmpty()) continue;
            QString fullPath = libsDir + QStringLiteral("/") + relPath;
            if (!m_failedPathsCache.contains(fullPath)) continue;

            QString sha1 = artifact.value(QStringLiteral("sha1")).toString();
            QString mojangUrl = artifact.value(QStringLiteral("url")).toString();
            QString bmclapiUrl = bmclapiLibBase + QStringLiteral("/") + relPath;

            QString primary, fallback;
            if (fileSource == DownloadSourcePolicy::PreferOfficial) {
                primary = mojangUrl;
                fallback = bmclapiUrl;
            } else {
                primary = bmclapiUrl;
                fallback = mojangUrl.isEmpty() ? bmclapiUrl : mojangUrl;
            }
            targets.append({fullPath, primary, fallback, sha1});
        }
    }

    // 3) Assets (from the asset index)
    {
        QJsonObject assetIdx = versionJson.value(QStringLiteral("assetIndex")).toObject();
        QString idxId = assetIdx.value(QStringLiteral("id")).toString();
        if (!idxId.isEmpty()) {
            QString idxPath = m_gameDir + QStringLiteral("/assets/indexes/") + idxId + QStringLiteral(".json");
            QFile idxFile(idxPath);
            if (idxFile.open(QIODevice::ReadOnly)) {
                QJsonDocument idxDoc = QJsonDocument::fromJson(idxFile.readAll());
                idxFile.close();
                if (idxDoc.isObject()) {
                    QJsonObject objects = idxDoc.object().value(QStringLiteral("objects")).toObject();
                    for (auto it = objects.begin(); it != objects.end(); ++it) {
                        QJsonObject obj = it.value().toObject();
                        QString sha1 = obj.value(QStringLiteral("hash")).toString();
                        if (sha1.isEmpty()) continue;
                        QString prefix = sha1.left(2);
                        QString assetPath = objectsDir + QStringLiteral("/") + prefix + QStringLiteral("/") + sha1;
                        if (!m_failedPathsCache.contains(assetPath)) continue;

                        QString primary, fallback;
                        if (fileSource == DownloadSourcePolicy::PreferOfficial) {
                            primary = mojangResBase + QStringLiteral("/") + prefix + QStringLiteral("/") + sha1;
                            fallback = bmclapiResBase + QStringLiteral("/") + prefix + QStringLiteral("/") + sha1;
                        } else {
                            primary = bmclapiResBase + QStringLiteral("/") + prefix + QStringLiteral("/") + sha1;
                            fallback = mojangResBase + QStringLiteral("/") + prefix + QStringLiteral("/") + sha1;
                        }
                        targets.append({assetPath, primary, fallback, sha1});
                    }
                }
            }
        }
    }

    // If targets are empty (paths don't match version JSON), fallback to verify
    if (targets.isEmpty()) {
        emit logMessage(tr("[警告] 损坏文件列表与版本 JSON 不匹配，重新校验..."));
        verifyVersion(versionId);
        return;
    }

    // ── Delete corrupt files ──
    int removed = 0;
    for (const RepairTarget& t : targets) {
        if (QFileInfo::exists(t.filePath) && QFile::remove(t.filePath))
            removed++;
    }
    emit logMessage(tr("[修复] 开始修复 %1 个损坏文件").arg(targets.size()));

    // ── Inline download pipeline ──
    m_verifyRunning.storeRelease(1);
    m_repairRunning.storeRelease(1);
    emit repairRunningChanged();
    setInstallPhase(tr("修复中..."));
    m_verifyChecked = 0;
    m_verifyTotal = targets.size();
    emit verifyStarted();
    emit verifyProgress(m_verifyChecked, m_verifyTotal);

    // ── Sequential download pipeline ──
    struct RepairState {
        int index = 0;
        int failCount = 0;
        bool usingFallback = false;
        QStringList remainingFailed;
        VersionBackend* self;
        QNetworkAccessManager* nam;
        QList<RepairTarget> targets;
        std::function<void()> downloadNext;
    };

    auto* state = new RepairState{};
    state->self = this;
    state->targets = targets;
    for (const auto& t : targets)
        state->remainingFailed.append(t.filePath);
    state->nam = new QNetworkAccessManager(this);

    // Recursive function stored in heap-allocated state to avoid dangling refs
    state->downloadNext = [state]() {
        VersionBackend* self = state->self;

        // Check if all targets processed
        while (state->index < static_cast<int>(state->targets.size())) {
            const RepairTarget& target = state->targets[state->index];

            // Determine which URL to try
            QString urlToTry = state->usingFallback ? target.fallbackUrl : target.primaryUrl;

            if (urlToTry.isEmpty()) {
                state->self->emit logMessage(
                    tr("[失败] 下载失败: %1 (无可用源)").arg(QFileInfo(target.filePath).fileName()));
                state->failCount++;
                state->index++;
                state->usingFallback = false;
                self->m_verifyChecked = qMin(self->m_verifyChecked + 1, self->m_verifyTotal);
                emit self->verifyProgress(self->m_verifyChecked, self->m_verifyTotal);
                continue;
            }

            QDir().mkpath(QFileInfo(target.filePath).absolutePath());
            auto* reply = state->nam->get(QNetworkRequest(QUrl(urlToTry)));

            connect(reply, &QNetworkReply::finished, state->self,
                    [state, reply, target]() {
                reply->deleteLater();
                VersionBackend* self = state->self;

                // Helper to advance with failure
                auto failAdvance = [self, state](const QString& msg) {
                    self->emit logMessage(msg);
                    state->failCount++;
                    state->index++;
                    state->usingFallback = false;
                    self->m_verifyChecked = qMin(self->m_verifyChecked + 1, self->m_verifyTotal);
                    emit self->verifyProgress(self->m_verifyChecked, self->m_verifyTotal);
                    state->downloadNext();
                };

                if (reply->error() != QNetworkReply::NoError) {
                    if (!state->usingFallback && !target.fallbackUrl.isEmpty()) {
                        state->usingFallback = true;
                        state->downloadNext();
                    } else {
                        failAdvance(tr("[失败] 下载失败: %1").arg(QFileInfo(target.filePath).fileName()));
                    }
                    return;
                }

                QByteArray data = reply->readAll();

                // SHA1 verification
                if (!target.sha1.isEmpty()) {
                    QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex();
                    if (hash != target.sha1) {
                        if (!state->usingFallback && !target.fallbackUrl.isEmpty()) {
                            state->usingFallback = true;
                            state->downloadNext();
                        } else {
                            failAdvance(tr("[失败] SHA1 不匹配: %1").arg(QFileInfo(target.filePath).fileName()));
                        }
                        return;
                    }
                }

                // Write to disk
                QFile file(target.filePath);
                if (file.open(QIODevice::WriteOnly)) {
                    file.write(data);
                    file.close();
                    state->remainingFailed.removeOne(target.filePath);
                    self->emit logMessage(tr("[完成] 已修复: %1").arg(QFileInfo(target.filePath).fileName()));
                } else {
                    state->failCount++;
                    self->emit logMessage(tr("[失败] 写入文件失败: %1").arg(target.filePath));
                }

                state->index++;
                state->usingFallback = false;
                self->m_verifyChecked = qMin(self->m_verifyChecked + 1, self->m_verifyTotal);
                emit self->verifyProgress(self->m_verifyChecked, self->m_verifyTotal);
                state->downloadNext();
            });

            return;  // waiting for async completion
        }

        // ── All targets processed ──
        state->nam->deleteLater();
        bool allPassed = (state->failCount == 0);
        self->m_failedPathsCache = state->remainingFailed;
        self->m_verifyRunning.storeRelease(0);
        self->m_repairRunning.storeRelease(0);
        emit self->repairRunningChanged();
        self->setInstallPhase(tr("空闲"));

        if (allPassed) {
            self->m_failedPathsCache.clear();
            self->emit logMessage(tr("[完成] 修复完成: 全部 %1 个文件已恢复").arg(state->targets.size()));
            self->emit verifyFailedFiles({});
        } else {
            self->emit logMessage(
                tr("[失败] %1 个文件修复失败，请检查网络后重试").arg(state->remainingFailed.size()));
            self->emit verifyFailedFiles(state->remainingFailed);
        }
        self->emit verifyFinished(allPassed);
        delete state;
    };

    // Kick off
    state->downloadNext();
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
                                       const QString& loaderVersion, const QString& installName,
                                       const QString& fabricApiVersion,
                                       const QString& fabricApiUrl,
                                       const QString& fabricApiSavePath,
                                       const QString& forgeInstallerSha1) {
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
            // Fabric: 7+1 steps (3 MC download + 1 MC verify + 1 profile + 1 libs + 1 install + 1 API)
            QStringList stepNames = {
                tr("下载原版 JSON 文件"),
                tr("下载原版支持库文件"),
                tr("下载原版资源文件"),
                tr("校验游戏资源完整性"),
                tr("下载 Fabric 配置"),
                tr("下载 Fabric 依赖库"),
                tr("安装 Fabric")
            };
            QVector<qreal> weights = {3.0, 8.0, 5.0, 0.5, 0.1, 2.0, 0.5};
            QVector<bool> shows = {true, true, true, false, true, true, true};
            if (!fabricApiUrl.isEmpty()) {
                stepNames.append(tr("下载 Fabric API"));
                weights.append(0.05);
                shows.append(false);  // hidden until step 3 finishes
            }
            rebuildSteps(installName, stepNames, weights, shows);
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

        // ── Start MC download + loader in parallel (no preflight) ──
        setInstalling(true);

        // ── Fabric: start MC + Fabric + Fabric API all at once ──
        if (loaderType == QStringLiteral("fabric")) {
            m_modLoaderInstallId = installName;
            m_mlInstaller->setGameDir(m_gameDir);
            m_mlInstaller->setParallelMode(true);
            m_mlInstaller->installFabric(mcVersion, loaderVersion, installName);

            if (!fabricApiUrl.isEmpty()) {
                auto& ses2 = session(installName);
                ses2.fabricApiPending = true;
                // Save original final path, use temp for download
                ses2.fabricApiFinalPath = fabricApiSavePath;
                QString tempDir = QDir::tempPath() + QStringLiteral("/shadow-fabric-api");
                QDir().mkpath(tempDir);
                QString tempApiPath = tempDir + QStringLiteral("/") + QFileInfo(fabricApiSavePath).fileName();
                ses2.fabricApiSavePath = tempApiPath;

                showStep(installName, 7);
                updateStep(installName, 7, QStringLiteral("active"), 0);

                auto* apiNam = new QNetworkAccessManager(this);
                QUrl apiUrlObj(fabricApiUrl);
                QNetworkRequest apiReq(apiUrlObj);
                QNetworkReply* apiReply = apiNam->get(apiReq);

                connect(apiReply, &QNetworkReply::downloadProgress, this,
                        [this, installName](qint64 recv, qint64 total) {
                    int pct = total > 0 ? (int)(recv * 100 / total) : 0;
                    updateStep(installName, 7, QStringLiteral("active"), pct);
                });

                connect(apiReply, &QNetworkReply::finished, this, [this, apiReply, apiNam, installName, tempApiPath]() {
                    apiReply->deleteLater();
                    apiNam->deleteLater();
                    auto& ses = session(installName);
                    ses.fabricApiPending = false;
                    if (apiReply->error() != QNetworkReply::NoError) {
                        qWarning() << "[install] Fabric API download failed:" << apiReply->errorString();
                        updateStep(installName, 7, QStringLiteral("error"), 0);
                        setInstallPhase(tr("Fabric API 下载失败"));
                    } else {
                        QByteArray data = apiReply->readAll();
                        QFile f(tempApiPath);
                        if (f.open(QIODevice::WriteOnly)) {
                            f.write(data);
                            f.close();
                        }
                        qDebug() << "[install] Fabric API downloaded to temp:" << tempApiPath << data.size() << "bytes";
                        updateStep(installName, 7, QStringLiteral("completed"), 100);
                        if (!ses.hasPendingLoader && !m_mlInstaller->isRunning() && ses.mcDownloadDone) {
                            finishInstall(installName);
                        }
                    }
                });
            }

            installVersion(mcVersion);
            return;
        }

        // ── Forge/NeoForge: start MC download + loader download in parallel ──
        installVersion(mcVersion);

        QString verArg = mcVersion + "-" + loaderVersion;
        QString loaderDlUrl;
        if (loaderType == QStringLiteral("forge")) {
            loaderDlUrl = QStringLiteral("https://bmclapi2.bangbang93.com/maven/net/minecraftforge/forge/%1/forge-%1-installer.jar").arg(verArg);
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

                    auto speedState = QSharedPointer<QPair<qint64,qint64>>::create(0,0);  // lastBytes, lastTimeMs

                    connect(reply, &QNetworkReply::downloadProgress, this,
                            [this, installName, loaderDlStepIdx, speedState](qint64 recv, qint64 total) {
                        updateStep(installName, loaderDlStepIdx, QStringLiteral("active"),
                                   total > 0 ? (int)(recv * 100 / total) : 0, recv, total);
                        // Feed speed to card display
                        qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
                        qint64 delta = recv - speedState->first;
                        qint64 timeDelta = nowMs - speedState->second;
                        if (timeDelta >= 200 && speedState->second > 0 && delta > 0) {
                            qint64 instant = delta * 1000 / timeDelta;
                            auto& ses = session(installName);
                            if (ses.mlRawSpeed == 0)
                                ses.mlRawSpeed = instant;
                            else
                                ses.mlRawSpeed = (ses.mlRawSpeed * 7 + instant * 3) / 10;
                            ses.mlSpeedLastTimeMs = nowMs;
                        }
                        speedState->first = recv;
                        speedState->second = nowMs;
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
                                auto speedState2 = QSharedPointer<QPair<qint64,qint64>>::create(0,0);
                                connect(r2, &QNetworkReply::downloadProgress, this,
                                        [this, installName, loaderDlStepIdx, speedState2](qint64 recv, qint64 total) {
                                    updateStep(installName, loaderDlStepIdx, QStringLiteral("active"),
                                               total > 0 ? (int)(recv * 100 / total) : 0, recv, total);
                                    qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
                                    qint64 delta = recv - speedState2->first;
                                    qint64 timeDelta = nowMs - speedState2->second;
                                    if (timeDelta >= 200 && speedState2->second > 0 && delta > 0) {
                                        qint64 instant = delta * 1000 / timeDelta;
                                        auto& ses = session(installName);
                                        if (ses.mlRawSpeed == 0)
                                            ses.mlRawSpeed = instant;
                                        else
                                            ses.mlRawSpeed = (ses.mlRawSpeed * 7 + instant * 3) / 10;
                                        ses.mlSpeedLastTimeMs = nowMs;
                                    }
                                    speedState2->first = recv;
                                    speedState2->second = nowMs;
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
                                    if (loaderType == QStringLiteral("neoforge")) {
                                        emit logMessage(QStringLiteral("[加载器] NeoForge安装程序下载完成 %1 MB").arg(data.size()/1024/1024.0, 0, 'f', 1));
                                        m_mlInstaller->installNeoForgeFromData(data, ses.mcVersion, ses.loaderVer, installName);
                                    } else {
                                        m_mlInstaller->installForgeFromData(data, ses.mcVersion, ses.loaderVer, installName);
                                    }
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
                        if (loaderType == QStringLiteral("neoforge")) {
                            emit logMessage(QStringLiteral("[加载器] NeoForge安装程序下载完成 %1 MB").arg(data.size()/1024/1024.0, 0, 'f', 1));
                            m_mlInstaller->installNeoForgeFromData(data, ses.mcVersion, ses.loaderVer, installName);
                        } else {
                            m_mlInstaller->installForgeFromData(data, ses.mcVersion, ses.loaderVer, installName);
                        }
                    });
                }

        }

        return;
    }

    // MC is downloading → download loader in PARALLEL, install when MC finishes
    if (m_activeIds.contains(mcVersion)) {
        qDebug() << "[install] MC" << mcVersion << "is downloading, starting parallel" << loaderType << "download";
        ses.hasPendingLoader = true;
        ses.isMerged = true;  // reuse merged install completion logic
        ses.pendingLoaderMc = mcVersion;
        ses.pendingLoaderType = loaderType;
        ses.pendingLoaderVer = loaderVersion;
        ses.pendingLoaderName = installName;
        ses.mcVersion = mcVersion;
        ses.loaderType = loaderType;
        ses.loaderVer = loaderVersion;
        ses.mcDownloadDone = false;
        ses.loaderDownloadReady = false;
        ses.forgeInstallerSha1 = forgeInstallerSha1;
        ses.fabricApiVersion = fabricApiVersion;
        ses.fabricApiUrl = fabricApiUrl;
        ses.fabricApiSavePath = fabricApiSavePath;

        // Build steps: wait MC + download loader + verify + install
        QString loaderLabel = loaderType == "neoforge" ? "NeoForge" : (loaderType == "fabric" ? "Fabric" : "Forge");
        rebuildSteps(installName, {
            tr("等待原版 %1 下载完成").arg(mcVersion),
            tr("下载 %1 主文件").arg(loaderLabel),
            tr("校验 %1 完整性").arg(loaderLabel),
            tr("安装 %1").arg(loaderLabel)
        }, {0.0, 6.0, 0.5, 10.0}, {true, true, false, false});
        updateStep(installName, 0, QStringLiteral("pending"), 0);
        updateStep(installName, 1, QStringLiteral("active"), 0);
        ses.loaderStepIdx = 1;

        // Start loader download in background (same URL logic as merged install)
        QString loaderDlUrl;
        QString verArg = mcVersion + "-" + loaderVersion;
        if (loaderType == "forge") {
            loaderDlUrl = QStringLiteral("https://bmclapi2.bangbang93.com/maven/net/minecraftforge/forge/%1/forge-%1-installer.jar").arg(verArg);
        } else if (loaderType == "neoforge") {
            loaderDlUrl = QStringLiteral("https://bmclapi2.bangbang93.com/maven/net/neoforged/neoforge/%1/neoforge-%1-installer.jar").arg(loaderVersion);
        } else if (loaderType == "fabric") {
            // Fabric: download profile + libs via ModLoaderInstaller (parallel mode)
            m_mlInstaller->setGameDir(m_gameDir);
            m_mlInstaller->setParallelMode(true);
            m_mlInstaller->installFabric(mcVersion, loaderVersion, installName);
            // Fabric API in parallel
            if (!fabricApiUrl.isEmpty()) { ses.fabricApiPending = true; /* simplified for pending */ }
            emit logMessage(tr("Fabric 配置和依赖库下载中，等待原版 %1 完成...").arg(mcVersion));
            return;
        }

        if (!loaderDlUrl.isEmpty()) {
            auto* nam = new QNetworkAccessManager(this);
            QUrl qurl(loaderDlUrl);
            QNetworkRequest req(qurl);
            req.setRawHeader("User-Agent", "ShadowLauncher/1.0");
            req.setTransferTimeout(300000);
            QNetworkReply* reply = nam->get(req);

            connect(reply, &QNetworkReply::downloadProgress, this,
                    [this, installName](qint64 recv, qint64 total) {
                updateStep(installName, 1, QStringLiteral("active"),
                           total > 0 ? (int)(recv * 100 / total) : 0, recv, total);
            });

            connect(reply, &QNetworkReply::finished, this, [this, reply, nam, installName, mcVersion, loaderType, loaderVersion, forgeInstallerSha1]() {
                reply->deleteLater();
                nam->deleteLater();
                auto& ses = session(installName);
                if (reply->error() != QNetworkReply::NoError) {
                    qWarning() << "[pending] Loader download FAILED:" << reply->errorString();
                    updateStep(installName, 1, QStringLiteral("error"), 0);
                    ses.failed = true;
                    ses.error = reply->errorString();
                    rebuildInstallCards();
                } else {
                    QByteArray data = reply->readAll();
                    qDebug() << "[pending] Loader download complete:" << data.size() << "bytes";
                    updateStep(installName, 1, QStringLiteral("completed"), 100);
                    ses.loaderDownloadData = data;
                    ses.loaderDownloadReady = true;
                    
                    // Start verify+extract now (runs while MC still downloading)
                    m_modLoaderInstallId = installName;
                    m_mlInstaller->setGameDir(m_gameDir);
                    if (loaderType == "neoforge")
                        m_mlInstaller->installNeoForgeFromData(data, mcVersion, loaderVersion, installName);
                    else
                        m_mlInstaller->installForgeFromData(data, mcVersion, loaderVersion, installName);

                    // If MC already finished, finalize immediately
                    if (ses.mcDownloadDone) {
                        proceedToLoaderInstall(installName);
                    } else {
                        qDebug() << "[pending] Loader verify/extract started, waiting for MC" << mcVersion;
                    }
                }
            });
        }

        emit logMessage(tr("[等待] %1 下载中，将在原版 %2 下载完成后自动安装")
                            .arg(loaderType).arg(mcVersion));
        return;
    }

    // MC is installed — proceed with standalone loader
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
        m_mlInstaller->installForge(mcVersion, loaderVersion, installName, forgeInstallerSha1);
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
        const QString& forgeVersion, const QString& installName,
        const QString& bmclType, const QString& bmclPatch) {
    if (!m_mlInstaller) return;

    // FIX: Set m_modLoaderInstallId so progress signals are not dropped
    m_modLoaderInstallId = installName;
    auto& ses = session(installName);
    ses.failed = false;
    ses.error.clear();

    // Ensure vanilla MC is installed first
    if (!installedIds().contains(mcVersion) && !m_activeIds.contains(mcVersion)) {
        qDebug() << "[install] Vanilla MC" << mcVersion << "not installed for Optifine, downloading first...";
        ses.isMerged = true;
        ses.mcVersion = mcVersion;
        ses.loaderType = QStringLiteral("optifine");
        ses.loaderVer = optifineVersion;
        ses.bmclType = bmclType;
        ses.bmclPatch = bmclPatch;
        ses.hasPendingLoader = true;
        ses.pendingLoaderMc = mcVersion;
        ses.pendingLoaderType = QStringLiteral("optifine");
        ses.pendingLoaderVer = optifineVersion;
        ses.pendingLoaderName = installName;
        // Reset byte accumulators for merged install
        for (int i = 0; i < 3; i++) { ses.mcStepDone[i] = 0; ses.mcStepTotal[i] = 0; }
        ses.mcFileAdded.clear();

        // Build 5-step card: MC JSON + MC libs + MC assets + OptiFine JAR + Install
        rebuildSteps(installName, {
            tr("下载原版 JSON 文件"),
            tr("下载原版支持库文件"),
            tr("下载原版资源文件"),
            tr("下载 OptiFine 主文件"),
            tr("安装 OptiFine")
        }, {1.0, 8.0, 5.0, 3.0, 1.0},
         {true, true, true, true, false});  // step 4 (install) hidden until downloads done

        updateStep(installName, 0, QStringLiteral("active"), 0);
        ses.loadedStep = 1;

        setInstalling(true);
        setInstallPhase(tr("下载中..."));

        // Start MC and OptiFine JAR downloads in parallel
        ses.optifineJarParallel = true;
        installVersion(mcVersion);
        startOptifineJarParallel(installName, mcVersion, optifineVersion, bmclType, bmclPatch);
        return;
    }

    // MC already installed — 2-step OptiFine only
    rebuildSteps(installName, {
        tr("下载 OptiFine 主文件"),
        tr("安装 OptiFine")
    }, {3.0, 1.0}, {true, true});
    updateStep(installName, 0, QStringLiteral("active"), 0);

    m_mlInstaller->setGameDir(m_gameDir);
    // Respect version isolation for mods/ output
    if (m_isolation && m_isolation->isVersionIsolated(installName)) {
        m_mlInstaller->setModsDir(m_isolation->getVersionGameDir(installName) + "/mods");
    } else {
        m_mlInstaller->setModsDir(QString());  // reset to default (gameDir/mods)
    }

    // Preflight: connectivity test (use manifestUrl, not the actual download URL — HEAD blocked by CDN)
    auto* sb = qobject_cast<ShadowBackend*>(parent());
    auto* coord = new DownloadCoordinator(this);
    int listSrc = sb ? sb->listDownloadSource() : 1;  // 0=镜像, 1=官方, 2=自动(双源)

    if (listSrc == 0) {
        // 仅镜像
        coord->addSource(QStringLiteral("bmclapi"), MirrorSource::bmclapi().manifestUrl);
    } else if (listSrc == 1) {
        // 仅官方
        coord->addSource(QStringLiteral("mojang"), MirrorSource::mojang().manifestUrl);
    } else {
        // 双源竞速（默认行为）
        coord->addSource(QStringLiteral("bmclapi"), MirrorSource::bmclapi().manifestUrl);
        coord->addSource(QStringLiteral("mojang"), MirrorSource::mojang().manifestUrl);
    }
    setInstallPhase(tr("连通性测试中..."));
    setInstalling(true);

    connect(coord, &DownloadCoordinator::ready, this,
            [this, mcVersion, optifineVersion, forgeVersion, installName, bmclType, bmclPatch, coord](int /*sourceIndex*/, qint64) {
        coord->deleteLater();
        emit logMessage(tr("[完成] OptiFine 连通性测试通过"));
        m_mlInstaller->installOptifine(mcVersion, optifineVersion, forgeVersion, installName, bmclType, bmclPatch);
    });
    connect(coord, &DownloadCoordinator::connectivityFailed, this,
            [this, coord, installName](const QString& taskId, const QString& reason) {
        coord->deleteLater();
        emit logMessage(tr("[失败] OptiFine 连通性测试失败: %1").arg(reason));
        auto& ses = session(installName);
        ses.failed = true;
        ses.error = tr("OptiFine 网络不可达");
        rebuildInstallCards();
        setInstalling(false);
    });
    coord->start();
    return;
}

void VersionBackend::finishOptifineMerged(const QString& mcVersion, const QString& installName) {
    // MC steps 0-2 done. Download OptiFine JAR (step 3), then delegate to ModLoaderInstaller (step 4).
    auto& ses = session(installName);

    QString url;
    QString filename;
    if (!ses.bmclType.isEmpty() && !ses.bmclPatch.isEmpty()) {
        url = QString("https://bmclapi2.bangbang93.com/optifine/%1/%2/%3").arg(mcVersion, ses.bmclType, ses.bmclPatch);
        filename = QString("OptiFine_%1_%2_%3.jar").arg(mcVersion, ses.bmclType, ses.bmclPatch);
    } else {
        QString optifineVer = ses.loaderVer;
        filename = optifineVer.startsWith("OptiFine_") || optifineVer.startsWith("preview_OptiFine_")
            ? optifineVer + ".jar"
            : QString("OptiFine_%1_%2.jar").arg(mcVersion, optifineVer);
        url = QString("https://bmclapi2.bangbang93.com/optifine/%1/%2/download").arg(mcVersion, filename);
    }

    showStep(installName, 3);
    updateStep(installName, 3, QStringLiteral("active"), 0, 0, 0);
    setInstallPhase(tr("下载 OptiFine 主文件..."));

    auto* nam = new QNetworkAccessManager(this);
    auto* reply = nam->get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::downloadProgress, this,
        [this, installName](qint64 received, qint64 total) {
            int pct = total > 0 ? (int)(received * 100 / total) : 0;
            updateStep(installName, 3, QStringLiteral("active"), pct, received, total);
        });
    connect(reply, &QNetworkReply::finished, this,
        [this, reply, nam, installName, mcVersion, filename]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                // Fallback to official
                QString offUrl = QString("https://optifine.net/downloadx?f=%1").arg(filename);
                auto* r2 = nam->get(QNetworkRequest(offUrl));
                connect(r2, &QNetworkReply::downloadProgress, this,
                    [this, installName](qint64 received, qint64 total) {
                        int pct = total > 0 ? (int)(received * 100 / total) : 0;
                        updateStep(installName, 3, QStringLiteral("active"), pct, received, total);
                    });
                connect(r2, &QNetworkReply::finished, this,
                    [this, r2, nam, installName, mcVersion]() {
                        r2->deleteLater();
                        nam->deleteLater();
                        if (r2->error() != QNetworkReply::NoError) {
                            emit logMessage(tr("[失败] OptiFine 下载失败（所有源）"));
                            updateStep(installName, 3, QStringLiteral("failed"), 0);
                            auto& ses = session(installName);
                            ses.failed = true;
                            ses.error = tr("OptiFine 下载失败");
                            rebuildInstallCards();
                            setInstalling(false);
                            return;
                        }
                        QByteArray jarData = r2->readAll();
                        updateStep(installName, 3, QStringLiteral("completed"), 100, jarData.size(), jarData.size());
                        delegateOptifineInstall(mcVersion, installName, jarData);
                    });
                return;
            }
            QByteArray jarData = reply->readAll();
            updateStep(installName, 3, QStringLiteral("completed"), 100, jarData.size(), jarData.size());
            nam->deleteLater();
            delegateOptifineInstall(mcVersion, installName, jarData);
        });
}

void VersionBackend::delegateOptifineInstall(const QString& mcVersion, const QString& installName,
                                              const QByteArray& jarData) {
    auto& ses = session(installName);
    showStep(installName, 4);
    updateStep(installName, 4, QStringLiteral("active"), 0);
    setInstallPhase(tr("安装 OptiFine..."));

    m_mlInstaller->setGameDir(m_gameDir);
    if (m_isolation && m_isolation->isVersionIsolated(installName)) {
        m_mlInstaller->setModsDir(m_isolation->getVersionGameDir(installName) + "/mods");
    } else {
        m_mlInstaller->setModsDir(QString());
    }

    // Connect BEFORE calling installOptifineFromJar so our callback fires first
    // and updates m_modLoaderInstallId before the main handler emits installComplete
    QMetaObject::Connection* conn = new QMetaObject::Connection;
    *conn = connect(m_mlInstaller, &ModLoaderInstaller::finished, this,
        [this, conn, mcVersion](bool success, const QString&) {
            disconnect(*conn);
            delete conn;
            if (!success) return;
            // Scan versions dir for the actual OptiFine folder name (Problem 3 fix)
            QDir verDir(m_gameDir + "/versions");
            QStringList filters; filters << ("*OptiFine*");
            QStringList found = verDir.entryList(filters, QDir::Dirs | QDir::NoDotAndDotDot);
            QString actualId;
            for (const QString& f : found) {
                if (f != mcVersion) { actualId = f; break; }
            }
            if (!actualId.isEmpty() && actualId != m_modLoaderInstallId) {
                m_modLoaderInstallId = actualId;
                emit logMessage(tr("[完成] OptiFine 版本 ID: %1").arg(actualId));
            }
            // Ensure version isolation is properly set up for the new OptiFine version
            if (m_isolation) {
                m_isolation->migrateToIsolated(actualId.isEmpty() ? m_modLoaderInstallId : actualId);
            }
        }, Qt::DirectConnection);  // DirectConnection ensures we fire before queued handlers

    m_mlInstaller->installOptifineFromJar(jarData, mcVersion, installName, ses.bmclType, ses.bmclPatch);
}

void VersionBackend::startOptifineJarParallel(const QString& installName, const QString& mcVersion,
                                                const QString& optifineVersion,
                                                const QString& bmclType, const QString& bmclPatch) {
    // Download OptiFine JAR in parallel with MC download
    auto& ses = session(installName);

    // Use BMCLAPI type/patch format when available (recommended API)
    QString url;
    QString filename;
    if (!bmclType.isEmpty() && !bmclPatch.isEmpty()) {
        url = QString("https://bmclapi2.bangbang93.com/optifine/%1/%2/%3").arg(mcVersion, bmclType, bmclPatch);
        filename = QString("OptiFine_%1_%2_%3.jar").arg(mcVersion, bmclType, bmclPatch);
    } else {
        // Fallback: use Maven path (only works for standard OptiFine_ naming)
        filename = (optifineVersion.startsWith("OptiFine_") || optifineVersion.startsWith("preview_OptiFine_"))
            ? optifineVersion + ".jar"
            : QString("OptiFine_%1_%2.jar").arg(mcVersion, optifineVersion);
        url = QString("https://bmclapi2.bangbang93.com/maven/com/optifine/%1/%2").arg(mcVersion, filename);
    }

    emit logMessage(tr("并行下载 OptiFine: %1").arg(filename));

    auto* nam = new QNetworkAccessManager(this);
    auto* reply = nam->get(QNetworkRequest(url));

    connect(reply, &QNetworkReply::downloadProgress, this,
        [this, installName](qint64 received, qint64 total) {
            int pct = total > 0 ? (int)(received * 100 / total) : 0;
            updateStep(installName, 3, QStringLiteral("active"), pct, received, total);
        });

    connect(reply, &QNetworkReply::finished, this,
        [this, reply, nam, installName, mcVersion, filename]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                // Fallback to official
                QString offUrl = QString("https://optifine.net/downloadx?f=%1").arg(filename);
                auto* r2 = nam->get(QNetworkRequest(offUrl));
                connect(r2, &QNetworkReply::downloadProgress, this,
                    [this, installName](qint64 received, qint64 total) {
                        int pct = total > 0 ? (int)(received * 100 / total) : 0;
                        updateStep(installName, 3, QStringLiteral("active"), pct, received, total);
                    });
                connect(r2, &QNetworkReply::finished, this,
                    [this, r2, nam, installName, mcVersion]() {
                        r2->deleteLater();
                        nam->deleteLater();
                        if (r2->error() != QNetworkReply::NoError) {
                            emit logMessage(tr("[失败] OptiFine 下载失败（所有源）"));
                            updateStep(installName, 3, QStringLiteral("failed"), 0);
                            auto& ses = session(installName);
                            ses.failed = true;
                            ses.error = tr("OptiFine 下载失败");
                            rebuildInstallCards();
                            setInstalling(false);
                            return;
                        }
                        QByteArray jarData = r2->readAll();
                        updateStep(installName, 3, QStringLiteral("completed"), 100, jarData.size(), jarData.size());
                        onParallelOptifineDone(installName, jarData);
                    });
                return;
            }
            QByteArray jarData = reply->readAll();
            updateStep(installName, 3, QStringLiteral("completed"), 100, jarData.size(), jarData.size());
            nam->deleteLater();
            onParallelOptifineDone(installName, jarData);
        });
}

void VersionBackend::onParallelOptifineDone(const QString& installName, const QByteArray& jarData) {
    auto& ses = session(installName);

    // Guard against double invocation (MC completion + pending loader both trigger)
    if (ses.hasPendingLoader) ses.hasPendingLoader = false;
    if (ses.optifineJarDone && ses.mcDownloadDone && !jarData.isEmpty()) {
        // Already handled the first time, skip
        return;
    }

    // Store JAR data (in case MC is still downloading)
    if (!jarData.isEmpty()) {
        ses.optifineJarData = jarData;
    }
    ses.optifineJarDone = true;

    // Check if MC is also done
    if (!ses.mcDownloadDone) {
        emit logMessage(tr("OptiFine JAR 下载完成，等待 MC 下载..."));
        return;
    }

    // Both MC and OptiFine JAR are done
    emit logMessage(tr("[完成] MC 和 OptiFine 均下载完成，开始安装..."));
    QByteArray data = ses.optifineJarData.isEmpty() ? jarData : ses.optifineJarData;
    delegateOptifineInstall(ses.mcVersion, installName, data);
}

void VersionBackend::installOptifineJar(const QString& mcVersion, const QString& optifineVersion,
                                          const QString& bmclType, const QString& bmclPatch) {
    // Lightweight: download OptiFine JAR to version's mods/ — no blocking, no installer process
    QString url;
    QString filename;
    if (!bmclType.isEmpty() && !bmclPatch.isEmpty()) {
        url = QString("https://bmclapi2.bangbang93.com/optifine/%1/%2/%3").arg(mcVersion, bmclType, bmclPatch);
        filename = QString("OptiFine_%1_%2_%3.jar").arg(mcVersion, bmclType, bmclPatch);
    } else {
        filename = (optifineVersion.startsWith("OptiFine_") || optifineVersion.startsWith("preview_OptiFine_"))
            ? optifineVersion + ".jar"
            : QString("OptiFine_%1_%2.jar").arg(mcVersion, optifineVersion);
        url = QString("https://bmclapi2.bangbang93.com/optifine/%1/%2/download").arg(mcVersion, filename);
    }

    QString targetDir = m_gameDir + "/mods";
    if (m_isolation && m_isolation->isVersionIsolated(m_modLoaderInstallId)) {
        targetDir = m_isolation->getVersionGameDir(m_modLoaderInstallId) + "/mods";
    }
    QDir().mkpath(targetDir);
    QString savePath = targetDir + "/" + filename;

    emit logMessage(tr("下载 OptiFine (mods/): %1").arg(filename));

    auto* nam = new QNetworkAccessManager(this);
    auto* reply = nam->get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, nam, reply, filename, savePath]() {
        reply->deleteLater();
        bool saved = false;
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QFile f(savePath);
            if (f.open(QIODevice::WriteOnly)) { f.write(data); f.close(); saved = true; }
        }
        if (!saved) {
            // Fallback to official
            qDebug() << "[OptiFineJar] BMCLAPI failed, trying official...";
            QString offUrl = QString("https://optifine.net/downloadx?f=%1").arg(filename);
            auto* r2 = nam->get(QNetworkRequest(offUrl));
            connect(r2, &QNetworkReply::finished, this, [this, nam, r2, filename, savePath]() {
                nam->deleteLater();
                r2->deleteLater();
                if (r2->error() == QNetworkReply::NoError) {
                    QByteArray data = r2->readAll();
                    QFile f(savePath);
                    if (f.open(QIODevice::WriteOnly)) { f.write(data); f.close(); }
                    emit logMessage(tr("[完成] OptiFine 已保存到 mods/ (%1)").arg(filename));
                } else {
                    emit logMessage(tr("[失败] OptiFine 下载失败 — 网络问题"));
                }
            });
            return;
        }
        nam->deleteLater();
        emit logMessage(tr("[完成] OptiFine 已保存到 mods/ (%1)").arg(filename));
    });
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

    // If this session has a pending user data import, append the import step
    // (only if not already present to avoid duplicates on rebuild)
    if (ses.hasImportPending) {
        bool alreadyHasImport = false;
        for (const auto& st : ses.steps) {
            if (st.toMap().value("name").toString().contains("导入用户数据")) {
                alreadyHasImport = true;
                break;
            }
        }
        if (!alreadyHasImport) {
            QVariantMap importStep;
            importStep["name"] = tr("导入用户数据");
            importStep["status"] = QStringLiteral("pending");
            importStep["percentage"] = 0;
            importStep["bytesReceived"] = QVariant::fromValue<qint64>(0);
            importStep["bytesTotal"] = QVariant::fromValue<qint64>(0);
            importStep["weight"] = 0.03;
            importStep["show"] = true;
            ses.steps.append(importStep);
        }
    }
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

void VersionBackend::hideStep(const QString& installId, int index) {
    auto& s = session(installId);
    if (index < 0 || index >= s.steps.size()) return;
    QVariantMap step = s.steps[index].toMap();
    step["show"] = false;
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

    qCInfo(logVersion).noquote()
        << QString("[step] id=%1 idx=%2 name=\"%3\" status=%4 pct=%5% bytes=%6/%7KB")
           .arg(installId).arg(index)
           .arg(step["name"].toString())
           .arg(status)
           .arg(percentage)
           .arg(bytesRecv / 1024).arg(bytesTotal / 1024);

    rebuildInstallCards();
    }



int VersionBackend::installRemainingSteps(const QString& sessionId) const {
    if (sessionId.isEmpty() || !m_sessions.contains(sessionId)) return 0;
    const auto& ses = m_sessions[sessionId];
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
    case HasUserDataImportRole:    return c.hasUserDataImport;
    case CanCancelRole:            return c.canCancel;
    case ImportFailedAtMsRole:     return QVariant::fromValue<qint64>(c.importFailedAtMs);
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
        {TotalProgressVisibleRole, "totalProgressVisible"},
        {HasUserDataImportRole, "hasUserDataImport"},
        {CanCancelRole, "canCancel"},
        {ImportFailedAtMsRole, "importFailedAtMs"}
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
    // Batch + throttle: no more than every 200ms to avoid UI churn
    static constexpr qint64 kMinIntervalMs = 200;
    qint64 elapsed = m_cardsTimer.isValid() ? m_cardsTimer.elapsed() : kMinIntervalMs;
    
    if (!m_cardsRebuildPending) {
        if (elapsed < kMinIntervalMs) {
            // Still within cooldown — defer
            m_cardsRebuildPending = true;
            QTimer::singleShot(kMinIntervalMs - elapsed, this, [this]() {
                m_cardsRebuildPending = false;
                m_cardsTimer.restart();
                doRebuildInstallCards();
            });
        } else {
            m_cardsRebuildPending = true;
            m_cardsTimer.restart();
            QTimer::singleShot(0, this, [this]() {
                m_cardsRebuildPending = false;
                doRebuildInstallCards();
            });
        }
    }
}

void VersionBackend::activateVerifyOnDownloadsDone(const QString& versionId)
{
    // Check merged install sessions
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        if (it.value().isMerged && it.value().mcVersion == versionId) {
            bool allDone = true;
            bool anyCategory = false;
            for (int ci = 0; ci < 3 && allDone; ci++) {
                if (it.value().mcStepTotal[ci] <= 0)
                    continue;  // empty category → nothing to download, already done
                anyCategory = true;
                if (it.value().mcStepDone[ci] < it.value().mcStepTotal[ci])
                    allDone = false;
            }
            if (allDone && anyCategory) {
                int verifyIdx = 3;
                if (verifyIdx < it.value().steps.size()) {
                    QVariantMap vstep = it.value().steps[verifyIdx].toMap();
                    if (!vstep.value("show").toBool()) {
                        showStep(it.key(), verifyIdx);
                        updateStep(it.key(), verifyIdx, QStringLiteral("active"), 0);
                        qCInfo(logVersion)
                            << "MC downloads done, verify step active for" << it.key();
                    }
                }
            }
            return;
        }
    }
    // Pure MC (non-merged): set flag + immediate card rebuild
    if (m_dlStates.contains(versionId)) {
        auto& st = m_dlStates[versionId];
        bool allDone = true;
        bool anyCategory = false;
        for (int ci = 0; ci < 3 && allDone; ci++) {
            if (st.catBytesTotal[ci] <= 0)
                continue;  // empty category → nothing to download
            anyCategory = true;
            if (st.catBytesDl[ci] < st.catBytesTotal[ci])
                allDone = false;
        }
        if (allDone && anyCategory && !st.catsFullyDone) {
            st.catsFullyDone = true;
            doRebuildInstallCards();
            qCInfo(logVersion)
                << "MC downloads done, verify step active for pure MC" << versionId;
        }
    }
}

void VersionBackend::doRebuildInstallCards() {
    // Build cards silently (no per-call logging — was drowning the log)
    if (!m_installCardsModel) {
        LOG_CARDS() << "  ABORT: m_installCardsModel is null!";
        return;
    }
    LOG_CARDS() << "  model qobject_cast<QAbstractItemModel>:"
                 << (qobject_cast<QAbstractItemModel*>(m_installCardsModel) ? "OK" : "FAIL")
                 << "qobject_cast<QAbstractListModel>:"
                 << (qobject_cast<QAbstractListModel*>(m_installCardsModel) ? "OK" : "FAIL");

    // ── Speed decay: if no data for >1s, halve each tick (natural fade to 0) ──
    qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    for (auto& st : m_dlStates) {
        if (st.speed > 0 && nowMs - st.speedLastTimeMs > 1000) {
            st.speed /= 2;
            if (st.speed < 1024) st.speed = 0;
        }
    }
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        auto& ses = it.value();
        if (ses.mlRawSpeed > 0 && nowMs - ses.mlSpeedLastTimeMs > 1000) {
            ses.mlRawSpeed /= 2;
            if (ses.mlRawSpeed < 1024) ses.mlRawSpeed = 0;
        }
    }

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
        c.progress = mlPending ? 0.0 : qBound(0.0, ses.smoothProgress, 1.0);
        // Speed: sum MC + loader when both are downloading in parallel
        {
            qint64 s = 0;
            if (!mlPending) {
                bool mcActive = m_dlStates.contains(ses.mcVersion) && !ses.mcDownloadDone;
                bool mlActive = m_mlInstaller && m_mlInstaller->isRunning();
                if (mcActive)  s += m_dlStates[ses.mcVersion].speed;
                if (mlActive)  s += ses.mlRawSpeed;
            }
            c.speed = s;
        }
        c.phase = mlFailed ? QStringLiteral("失败")
            : (mlPending ? tr("等待原版 %1 下载完成").arg(ses.pendingLoaderMc)
            : m_installPhase);
        c.remaining = mlPending ? 0 : installRemainingSteps(sid);
        c.steps = mlPending ? QVariantList{} : ses.steps;
        c.failed = mlFailed;
        c.error = ses.error;
        c.hasUserDataImport = ses.hasImportPending;
        c.canCancel = !(ses.hasImportPending && ses.steps.size() > 0 &&
            ses.steps.last().toMap().value("status").toString() == "active");
        c.importFailedAtMs = ses.importFailedAtMs;
        // Total progress bar: visible during download AND install (colors differ in QML)
        {
            bool hasActiveStep = false;
            if (!mlPending && !mlFailed) {
                for (const QVariant& vs : ses.steps) {
                    QVariantMap step = vs.toMap();
                    if (step.value(QStringLiteral("status")).toString() == QStringLiteral("active")) {
                        hasActiveStep = true;
                        break;
                    }
                }
            }
            c.totalProgressVisible = hasActiveStep;
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
                c.progress = (st.verifyTotal > 0) ? (qreal)st.verifyChecked / st.verifyTotal : 0.0;
                c.totalProgressVisible = false;
            } else {
                // Weighted: cat1 (libraries+client) 50%, cat2 (assets) 50%.
                // Each category internally uses its own byte progress.
                qreal pct1 = st.catBytesTotal[1] > 0 ? (qreal)st.catBytesDl[1] / st.catBytesTotal[1] : 0.0;
                qreal pct2 = st.catBytesTotal[2] > 0 ? (qreal)st.catBytesDl[2] / st.catBytesTotal[2] : 0.0;
                c.progress = qMin(1.0, 0.5 * pct1 + 0.5 * pct2);
            }
            c.speed = m_dlStates[vid].speed;  // per-state speed, isolated per install
            c.phase = st.phase;

            // DEBUG: when 3 sub-categories claim done but speed still flowing
            if (!verifying && st.speed > 0) {
                bool allThreeDone = true;
                for (int ci = 0; ci < 3; ci++) {
                    if (st.catBytesTotal[ci] > 0 && st.catBytesDl[ci] < st.catBytesTotal[ci])
                        allThreeDone = false;
                }
                if (allThreeDone) {
                    qCDebug(logVersion).noquote()
                        << QString("[SPEED-GHOST] %1: speed=%2 catBytes=[%3/%4 %5/%6 %7/%8] catFiles=[%9/%10 %11/%12 %13/%14] cf=%15/%16 bytes=%17/%18 url=%19")
                           .arg(vid)
                           .arg(st.speed)
                           .arg(st.catBytesDl[0]).arg(st.catBytesTotal[0])
                           .arg(st.catBytesDl[1]).arg(st.catBytesTotal[1])
                           .arg(st.catBytesDl[2]).arg(st.catBytesTotal[2])
                           .arg(st.catFilesDone[0]).arg(st.catFilesTotal[0])
                           .arg(st.catFilesDone[1]).arg(st.catFilesTotal[1])
                           .arg(st.catFilesDone[2]).arg(st.catFilesTotal[2])
                           .arg(st.progress).arg(st.total)
                           .arg(st.bytesDl).arg(st.bytesTotal)
                           .arg(st.file);
                }
            }

            // Always build 4 steps (3 download + 1 verify) — verify stays pending until phase transitions
            // When all download files are processed but verify hasn't started yet,
            // proactively show the verify step as active (bridges the gap).
            // Use file-count progress (st.progress/st.total) instead of byte progress
            // because cached/skipped files contribute to file count but not byte count.
            bool catsDone = !verifying && st.bytesDl > 0 && st.total > 0 && st.progress >= st.total;
            // Sticky: once verified phase is reached, downloads are irrevocably done
            if (!catsDone && st.downloadsDone)
                catsDone = true;
            // Also check per-category file counts if populated
            int totalCatFiles = st.catFilesTotal[0] + st.catFilesTotal[1] + st.catFilesTotal[2];
            if (!catsDone && totalCatFiles > 0) {
                int doneCatFiles = st.catFilesDone[0] + st.catFilesDone[1] + st.catFilesDone[2];
                catsDone = (doneCatFiles >= totalCatFiles);
            }

            auto catStatus = [&](int ci) {
                if (verifying || catsDone) return QStringLiteral("completed");  // download steps done
                if (st.catBytesTotal[ci] > 0 && st.catBytesDl[ci] >= st.catBytesTotal[ci])
                    return QStringLiteral("completed");
                if (st.catBytesTotal[ci] <= 0)
                    // Empty category: completed once ANY download activity has started
                    return (st.bytesDl > 0) ? QStringLiteral("completed") : QStringLiteral("pending");
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
                     (st.catBytesTotal[1] > 0) ? (int)(st.catBytesDl[1] * 100 / st.catBytesTotal[1]) : (st.bytesDl > 0 ? 100 : 0), catDl(1), catTot(1));
            addVStep(tr("下载资源文件"), verifying ? QStringLiteral("completed") : catStatus(2),
                     (st.catBytesTotal[2] > 0) ? (int)(st.catBytesDl[2] * 100 / st.catBytesTotal[2]) : (st.bytesDl > 0 ? 100 : 0), catDl(2), catTot(2));
            addVStep(tr("校验游戏资源完整性"),
                     (verifying || catsDone || st.catsFullyDone) ? QStringLiteral("active") : QStringLiteral("pending"),
                     (verifying || catsDone || st.catsFullyDone) ? (st.verifyTotal > 0 ? (int)(st.verifyChecked * 100 / st.verifyTotal) : 0) : 0,
                     (verifying || catsDone || st.catsFullyDone) ? st.verifyChecked : qint64(0),
                     (verifying || catsDone || st.catsFullyDone) ? st.verifyTotal : qint64(0));
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
    // Cards rebuilt silently
}

void VersionBackend::prefetchVersionJson(const QString& versionId)
{
    // Already cached or has pending prefetch
    if (m_prefetchedJson.contains(versionId)) return;
    
    // Check local cache first (fast path)
    const QString cachedJsonPath = m_gameDir
        + QStringLiteral("/versions/") + versionId
        + QStringLiteral("/") + versionId + QStringLiteral(".json");
    QFile cachedFile(cachedJsonPath);
    if (cachedFile.exists() && cachedFile.open(QIODevice::ReadOnly)) {
        QByteArray data = cachedFile.readAll();
        cachedFile.close();
        if (!data.isEmpty()) {
            m_prefetchedJson[versionId] = data;
            qCInfo(logVersion) << QStringLiteral("从本地缓存预取版本JSON 版本=%1").arg(versionId);
            return;
        }
    }

    // Fire BMCLAPI request in background (fire-and-forget)
    auto* nam = new QNetworkAccessManager(this);
    QString url = QStringLiteral("https://bmclapi2.bangbang93.com/version/%1/json")
                      .arg(versionId);
    QNetworkRequest req{QUrl(url)};
    req.setRawHeader("User-Agent", "ShadowLauncher/1.0");
    req.setTransferTimeout(10000);

    QNetworkReply* reply = nam->get(req);
    qCInfo(logVersion) << QStringLiteral("从BMCLAPI预取版本JSON 版本=%1").arg(versionId);

    connect(reply, &QNetworkReply::finished, this,
            [this, nam, reply, versionId]() {
                reply->deleteLater();
                nam->deleteLater();

                if (reply->error() == QNetworkReply::NoError) {
                    QByteArray data = reply->readAll();
                    if (!data.isEmpty()) {
                        m_prefetchedJson[versionId] = data;
                        qCInfo(logVersion)
                            << "Prefetched version JSON:" << versionId
                            << "(" << (data.size() / 1024) << "KB)";
                    }
                } else {
                    qCDebug(logVersion)
                        << "Prefetch failed for" << versionId
                        << ":" << reply->errorString();
                    // Silently ignore — installVersion will do its own three-source race
                }
            });
}

// ─── User data import ─────────────────────────────────────────────

void VersionBackend::setPendingUserDataImport(const QString& installId, const QString& archivePath)
{
    m_pendingImports[installId] = archivePath;

    // Also set on existing session if it exists
    if (m_sessions.contains(installId)) {
        auto& ses = session(installId);
        ses.hasImportPending = true;
        ses.importArchivePath = archivePath;

        // Append "导入用户数据" step to existing steps (only if not already there)
        bool alreadyHasImport = false;
        for (const auto& st : ses.steps) {
            if (st.toMap().value("name").toString().contains("导入用户数据")) {
                alreadyHasImport = true;
                break;
            }
        }
        if (!alreadyHasImport) {
            QVariantMap importStep;
            importStep["name"] = tr("导入用户数据");
            importStep["status"] = QStringLiteral("pending");
            importStep["percentage"] = 0;
            importStep["bytesReceived"] = QVariant::fromValue<qint64>(0);
            importStep["bytesTotal"] = QVariant::fromValue<qint64>(0);
            importStep["weight"] = 0.03;
            importStep["show"] = true;
            ses.steps.append(importStep);
        }
    }
    qCInfo(logVersion) << QStringLiteral("用户数据导入已标记 版本=%1 归档=%2").arg(installId, archivePath);
    rebuildInstallCards();
}

void VersionBackend::cancelPendingUserDataImport(const QString& installId)
{
    m_pendingImports.remove(installId);
    if (m_sessions.contains(installId)) {
        auto& ses = session(installId);
        ses.hasImportPending = false;
        ses.importArchivePath.clear();
        ses.importFailedAtMs = 0;
        // Remove the last step if it's the import step
        if (!ses.steps.isEmpty()) {
            QVariantMap last = ses.steps.last().toMap();
            if (last.value("name").toString().contains("导入用户数据")) {
                ses.steps.removeLast();
            }
        }
    }
    qCInfo(logVersion) << QStringLiteral("用户数据导入已取消 版本=%1").arg(installId);
    rebuildInstallCards();
}

void VersionBackend::dismissCard(const QString& installId)
{
    m_sessions.remove(installId);
    m_pendingImports.remove(installId);
    // Remove from active ids if present
    m_activeIds.removeAll(installId);
    if (m_activeCount > 0) m_activeCount--;
    rebuildInstallCards();
}

void VersionBackend::startUserDataImport(const QString& installId)
{
    if (!m_sessions.contains(installId)) return;
    auto& ses = session(installId);

    qCInfo(logVersion) << QStringLiteral("开始导入用户数据 版本=%1").arg(installId);
    emit logMessage(tr("正在导入用户数据..."));

    // Show and activate the import step (step 6 for merged installs)
    int importStepIdx = ses.steps.size() - 1;
    updateStep(installId, importStepIdx, QStringLiteral("active"), 0);

    // Run import synchronously (it's just unzipping and copying)
    UserDataBackend importer;
    QString gameDir = m_gameDir;
    QString versionId = installId;

    // The import executes, then we emit finish/complete
    bool ok = false;
    QString error;
    {
        QZipReader zip(ses.importArchivePath);
        if (zip.status() != QZipReader::NoError) {
            error = QStringLiteral("ZIP文件读取失败");
        } else {
            QString targetGame = gameDir + "/versions/" + versionId + "/game";
            QDir().mkpath(targetGame);

            QList<QZipReader::FileInfo> allFiles = zip.fileInfoList();
            int totalItems = 0, doneItems = 0;
            for (const auto& fi : allFiles) {
                if (fi.filePath.startsWith("game/") && !fi.isDir) totalItems++;
            }

            bool anyFailed = false;
            for (const auto& fi : allFiles) {
                if (!fi.filePath.startsWith("game/")) continue;
                if (fi.isDir) continue;
                QString relPath = fi.filePath.mid(5);
                QString dstPath = targetGame + "/" + relPath;
                QDir().mkpath(QFileInfo(dstPath).absolutePath());

                if (QFileInfo::exists(dstPath)) {
                    QString fn = QFileInfo(dstPath).fileName();
                    if (fn == "options.txt") { doneItems++; continue; }
                    if (fn == "servers.dat") { doneItems++; continue; }
                }

                QFile out(dstPath);
                if (out.open(QIODevice::WriteOnly)) {
                    QByteArray data = zip.fileData(fi.filePath);
                    out.write(data);
                    out.close();
                } else {
                    anyFailed = true;
                }
                doneItems++;
                if (totalItems > 0 && doneItems % 20 == 0) {
                    int pct = 5 + (doneItems * 90 / totalItems);
                    updateStep(installId, importStepIdx, QStringLiteral("active"), pct);
                }
            }
            zip.close();

            if (anyFailed) {
                error = QStringLiteral("部分文件写入失败，用户数据可能不完整");
                ok = false;
            } else {
                ok = true;
            }
        }
    }

    if (ok) {
        updateStep(installId, importStepIdx, QStringLiteral("completed"), 100);
        emit logMessage(tr("用户数据导入完成"));
    } else {
        // Mark step as failed with error
        updateStep(installId, importStepIdx, QStringLiteral("failed"), 0);
        ses.failed = true;
        ses.error = tr("用户数据导入失败：%1\n版本已成功安装，但用户数据未能导入。\n可手动从压缩包恢复数据。卡片将在60秒后关闭。")
                        .arg(error);
        ses.importFailedAtMs = QDateTime::currentMSecsSinceEpoch();
        emit logMessage(tr("用户数据导入失败: %1").arg(error));
    }

    ses.hasImportPending = false;
    ses.importArchivePath.clear();

    // Emit final completion
    emit installComplete(installId);
    emit installFinished(!ses.failed);
    setInstalling(false);  // always reset, even on failure
}

} // namespace ShadowLauncher
