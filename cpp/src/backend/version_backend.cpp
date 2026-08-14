// SPDX-License-Identifier: AGPL-3.0-or-later

// Copyright (C) 2025-2026 影 / Shadow / xiaole1173

// Shadow Launcher — VersionBackend

// QML-facing backend for version list, installation, and lifecycle management.

// Bridges VersionManager (fetch/cache) and VersionDownloader (install pipeline).



#include "version_backend.h"
#include "../utils/hash_utils.h"

#include "shadow_backend.h"

#include "userdata_backend.h"

#include "../utils/logger.h"
#include "../utils/temp_tracker.h"

#include "../core/version_manager.h"

#include "../core/version_downloader.h"

#include "../core/version_isolation.h"

#include "../core/mod_loader_installer.h"

#include "../core/step_pipeline.h"

#include "../core/mc_language.h"



#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

#include <QDateTime>

#include <QUuid>

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








    // Throttle incremental card updates (100ms — avoid UI freeze from rapid progress signals)
    m_cardUpdateThrottle.setSingleShot(false);
    m_cardUpdateThrottle.setInterval(200);
    connect(&m_cardUpdateThrottle, &QTimer::timeout, this, [this]() {
        for (const auto& id : m_pendingCardUpdates)
            updateCardFromSession(id);  // 全量（内部已优化：steps 未变时仅发射 progress+speed）
        m_pendingCardUpdates.clear();
        m_cardUpdateThrottle.stop();
    });


    // ── VersionManager: fetch + cache version manifest ──

    m_versionMgr = new VersionManager(this);
    m_versionMgr->setPreferOfficial(downloadPreferOfficial());   // 版本清单跟随全局源策略

    

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

    // ModLoaderInstaller instances are created per-task via createLoaderInstaller()

    // Defer initial fetch until setGameDir() sets m_dataDir

    // (refreshVersionList needs data dir for cache, refreshInstalled needs game dir)

    m_initialFetchDone = false;



    // ── Auto-repair: 下载失败后分两阶段修复 ──

    // 阶段1：verifyVersion 找出损坏文件 → 阶段2：repairVersion 修复

    connect(this, &VersionBackend::verifyFinished, this, [this](bool allPassed) {

        if (m_autoRepairVersionId.isEmpty())

            return;

        if (!m_autoRepairVerifyDone) {

            // 阶段1：verify 完成

            m_autoRepairVerifyDone = true;

            if (allPassed) {

                // 没坏文件，直接成功

                qCInfo(logVersion) << QStringLiteral("自动修复无需修复 版本=%1").arg(m_autoRepairVersionId);

                m_autoRepairVersionId.clear();
                QString ver = m_autoRepairVersionId;

                driveMergedAfterRepair(ver);   // merged：驱动继续（loader 流程）
                emit installFinished(true);

            } else if (!m_failedPathsCache.isEmpty()) {

                // 有损坏文件，进入阶段2：实际修复

                qCInfo(logVersion) << QStringLiteral("自动修复开始下载修复 版本=%1").arg(m_autoRepairVersionId);

                repairVersion(m_autoRepairVersionId);

                // 等 repairVersion 完成后再发射 verifyFinished

            } else {

                qCInfo(logVersion) << QStringLiteral("自动修复verify失败但缓存为空 版本=%1").arg(m_autoRepairVersionId);

                m_autoRepairVersionId.clear();
                QString ver = m_autoRepairVersionId;

                for (auto it = m_mergedContexts.begin(); it != m_mergedContexts.end(); ++it) {
                    auto* ctx = it.value();
                    if (ctx->mcVersion == ver && !ctx->mcDownloadDone) {
                        ctx->failed = true;
                        ctx->mcDownloadDone = true;
                        finishInstall(ctx->installId);   // 失败收尾（不卡死）
                    }
                }
                emit installFinished(false);

            }

        } else {

            // 阶段2：repairVersion 完成

            QString ver = m_autoRepairVersionId;

            m_autoRepairVersionId.clear();

            qCInfo(logVersion) << QStringLiteral("自动修复%1 版本=%2").arg(allPassed ? QStringLiteral("成功") : QStringLiteral("失败"), ver);

                if (allPassed)
                    driveMergedAfterRepair(ver);   // merged：修复成功 → 驱动继续
                else {
                    // 修复仍失败：merged 失败收尾（不卡死）
                    for (auto it = m_mergedContexts.begin(); it != m_mergedContexts.end(); ++it) {
                        auto* ctx = it.value();
                        if (ctx->mcVersion == ver && !ctx->mcDownloadDone) {
                            ctx->failed = true;
                            ctx->mcDownloadDone = true;
                            finishInstall(ctx->installId);
                        }
                    }
                }
            emit installFinished(allPassed);

        }

    });

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

bool VersionBackend::ensureSelectedVersionValid()

{

    const QString cur = m_selectedVersion;

    if (!cur.isEmpty() && m_installedIds.contains(cur))

        return false;   // 当前选中有效

    // 自愈：选第一个已安装版本；无已安装版本则清空

    const QString fallback = m_installedIds.isEmpty() ? QString() : m_installedIds.first();

    if (fallback == cur)

        return false;

    setSelectedVersion(fallback);

    qCInfo(logMod) << QStringLiteral("[版本] 选中版本自愈: %1 → %2")

                          .arg(cur.isEmpty() ? QStringLiteral("(空)") : cur,
                               fallback.isEmpty() ? QStringLiteral("(空)") : fallback);

    return true;

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

    // ── 异步刷新：扫盘在 worker 线程，避免点击/改名等高频调用阻塞主线程 UI ──
    if (m_refreshInstalledBusy)
        return;
    m_refreshInstalledBusy = true;

    const QString gameDir = m_gameDir;
    QtConcurrent::run([this, gameDir]() {
        // 纯读扫描（线程安全）：遍历 versions 目录，fast path 只 stat jar/json
        QStringList found;
        if (!gameDir.isEmpty()) {
            const QString versionsDir = gameDir + QStringLiteral("/versions");
            QDir dir(versionsDir);
            if (dir.exists()) {
                const QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
                for (const QString& subDir : subDirs) {
                    const QString verPath = versionsDir + QStringLiteral("/") + subDir;
                    const QString fastJar = verPath + QStringLiteral("/") + subDir + QStringLiteral(".jar");
                    const QString fastJson = verPath + QStringLiteral("/") + subDir + QStringLiteral(".json");
                    if (QFileInfo::exists(fastJar) || QFileInfo::exists(fastJson)) {
                        found.append(subDir);
                        continue;
                    }
                    if (!findVersionJson(verPath, subDir).isEmpty())
                        found.append(subDir);
                }
            }
        }

        QMetaObject::invokeMethod(this, [this, found]() {
            m_installedIds = found;
            m_refreshInstalledBusy = false;
            emit installedVersionsChanged();
        });
    });

}



// ============================================================

// Slots — install lifecycle

// ============================================================



void VersionBackend::installVersion(const QString& versionId)

{

    // ── 清理上次取消残留 ──
    // 版本 JSON 下载阶段取消时下载器未创建，onVersionDownloadFinished 不会触发
    // → m_activeIds/m_userCancelledIds 残留 → 重新下载被 contains 检查卡死。
    // 仅清理「activeIds 有但下载器/队列都没有」的真残留，不误伤正在下载的任务。
    if (m_activeIds.contains(versionId) && !m_downloaders.contains(versionId)
        && !m_installQueue.contains(versionId)) {
        m_activeIds.removeOne(versionId);
        if (m_activeCount > 0) m_activeCount--;
    }
    m_userCancelledIds.remove(versionId);

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



    // ── Resolve mirror（按全局源策略：官方源优先时选 Mojang，否则 BMCLAPI 加速）──
    // 修复：此前无条件 bmclapi() → collectTasks 的 primaryUrl/源数组永远 BMCLAPI 在前，
    // "尽量使用官方源"配置在下载调度层完全失效（日志"调度首源"永久固定镜像）。
    MirrorSource mirror = downloadPreferOfficial()
        ? MirrorSource::mojang()
        : MirrorSource::bmclapi();


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
    // Reset per-file dedup set for a fresh download
    m_dlStates[versionId].catBytesCountedPaths.clear();


    // ── Build step pipeline for pure MC download (skipped if part of merged install) ──
    {
        bool isMergedMc = false;
        for (auto mit = m_downloadSessions.constBegin(); mit != m_downloadSessions.constEnd(); ++mit) {
            if (mit.value() && mit.value()->isMerged() && mit.value()->mcVersion == versionId) {
                isMergedMc = true;
                break;
            }
        }
        if (!isMergedMc) {
            auto* ds = ensureSession(versionId);
            if (ds) {
                rebuildSteps(versionId,
                    {tr("下载版本JSON"), tr("下载支持库"), tr("下载资源文件"), tr("校验游戏资源完整性")},
                    {1.0, 3.0, 5.0, 1.0},  // weights
                    {false, true, true, true}  // JSON step hidden before download, others visible
                );
                // Show card immediately — don't wait for first HTTP byte (progressUpdated signal)
                updateCardFromSession(versionId, versionId, QStringLiteral("version"));
            }
        }
    }

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
        int roundId = 0;   // 当前竞速轮次（多层重试轮次隔离）

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

            // ── Check if this MC version was cancelled (async JSON fetch completed after cancel) ──
            if (m_userCancelledIds.contains(versionId)) {
                qCInfo(logVersion).noquote() << "[cancel] version" << versionId << "was cancelled, aborting downloader creation";
                cancelActiveDownload(versionId);
                return;
            }

            // ── Notify that MC version JSON is ready ──

            emit mcJsonReady(versionId);



            // ── Cancel any existing downloader ──
            // take() prevents re-entrancy; let natural signal chain handle cleanup.
            if (m_downloaders.contains(versionId)) {

                auto* oldDl = m_downloaders.take(versionId);

                oldDl->cancel();

            }



            // ── Create & configure downloader ──

            auto* downloader = new VersionDownloader(this);

            downloader->setMirror(mirror);

            // ── If this MC download is for a merged install, redirect to temp dir ──
            QString mcDir = m_versionMgr->gameDir();
            for (auto cIt = m_mergedContexts.constBegin(); cIt != m_mergedContexts.constEnd(); ++cIt) {
                if (cIt.value() && cIt.value()->mcVersion == versionId) {
                    mcDir = cIt.value()->tempDir;
                    qDebug() << "[install] Merged MC download redirected to temp:" << mcDir;
                    break;
                }
            }
            downloader->setMinecraftDir(mcDir);

            // For merged installs (mcDir is a tempDir), set the real gameDir
            // as cache fallback so already-downloaded files are reused.
            downloader->setCacheFallbackDir(m_versionMgr->gameDir());


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

                        // Set per-category totals (update when downloader discovers more files).

                        // The downloader may discover additional files after initial task planning

                        // (especially for assets/indices), so totals can grow over time.

                        auto& st = m_dlStates[versionId];

                        for (int ci = 0; ci < 3; ci++) {

                            qint64 ct = downloader->categoryTotalBytes(ci);

                            if (ct > st.catBytesTotal[ci])

                                st.catBytesTotal[ci] = ct;

                        }

                        bool firstPulse = (st.bytesDl == 0 && db > 0);

                        if (firstPulse) {
                            // Activate steps 1-2 immediately so they show as downloading
                            // (even at 0% — avoids gray/pending while waiting for first file of each category)
                            // Pure MC version
                            auto* pureDs = dlSession(versionId);
                            if (pureDs && !pureDs->isMerged() && pureDs->steps.size() >= 3) {
                                updateStep(versionId, 1, QStringLiteral("active"), 0, 0, 0);
                                updateStep(versionId, 2, QStringLiteral("active"), 0, 0, 0);
                            }
                        }

                        // Also inject into session for merged install mod_loader card

                        for (auto sit = m_downloadSessions.begin(); sit != m_downloadSessions.end(); ++sit) {

                            auto* d = dlSession(sit.key());

                            if (d && d->isMerged() && d->mcVersion == versionId) {

                                // Update step totals when downloader discovers more files

                                for (int ci = 0; ci < 3; ci++) {

                                    qint64 ct = downloader->categoryTotalBytes(ci);

                                    if (ct > d->mcStepTotal[ci])

                                        d->mcStepTotal[ci] = ct;

                                }

                                if (firstPulse) {

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

                        // ── Activate MC verify step for merged installs ──

                        for (auto sit = m_downloadSessions.begin(); sit != m_downloadSessions.end(); ++sit) {

                            auto* d = dlSession(sit.key());

                            if (d && d->isMerged() && d->mcVersion == versionId) {

                                // MC download is done — mark steps 0-2 as completed

                                for (int i = 0; i < 3 && i < d->steps.size(); i++) {

                                    updateStep(sit.key(), i, QStringLiteral("completed"), 100);

                                }

                                // 验证步骤"已启动"日志只打一次（verify 进度回调每次都会进这里）

                                if (!st.verifyStartLogged) {

                                    st.verifyStartLogged = true;

                                    qCInfo(logVersion) << QStringLiteral("MC下载完成 验证步骤已启动");

                                }

                                // Show MC verify step (index 3, initially hidden)

                                int verifyIdx = 3;

                                if (verifyIdx < d->steps.size()) {

                                    showStep(sit.key(), verifyIdx);

                                    int pct = total > 0 ? (checked * 100 / total) : 0;

                                    updateStep(sit.key(), verifyIdx, QStringLiteral("active"), pct, checked, total);

                                }

                                break;

                            }

                        }

                        // ── Pure MC: route verify progress into ds->steps ──

                        auto* pureVerifyDs = dlSession(versionId);

                        if (pureVerifyDs && !pureVerifyDs->isMerged() && pureVerifyDs->steps.size() >= 4) {

                            // Mark download steps (0-2) as completed

                            for (int i = 0; i < 3 && i < pureVerifyDs->steps.size(); i++) {

                                updateStep(versionId, i, QStringLiteral("completed"), 100);

                            }

                            // Show and update verify step (index 3)

                            int verifyIdx = 3;

                            if (verifyIdx < pureVerifyDs->steps.size()) {

                                showStep(versionId, verifyIdx);

                                int pct = total > 0 ? (checked * 100 / total) : 0;

                                updateStep(versionId, verifyIdx, QStringLiteral("active"), pct, checked, total);

                            }

                            if (!st.verifyStartLogged) {

                                st.verifyStartLogged = true;

                                qCInfo(logVersion) << QStringLiteral("纯MC下载 验证步骤已启动 版本=%1").arg(versionId);

                            }

                        }

                        // 验证步骤"已完成"日志只打一次

                        if (total > 0 && checked >= total && !st.verifyDoneLogged) {

                            st.verifyDoneLogged = true;

                            qCInfo(logVersion) << QStringLiteral("MC下载完成 验证步骤已完成 版本=%1").arg(versionId);

                        }

                        // Sync primary progress for the download page indicator

                        // (cards already updated via showStep/updateStep → updateCardFromSession above)

                        syncPrimaryProgress();

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



            m_installStartEpoch = QDateTime::currentMSecsSinceEpoch();

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

    // 多层重试：全部源失败后整体重试（阶梯超时递增），最多 kJsonFetchMaxRounds 轮
    static constexpr int kJsonFetchMaxRounds = 3;
    // 2026-08-11 修复（内测崩溃 0xc0000005 @ 0x1b9712）：原实现为栈上 std::function 且闭包按引用捕获
    // （&startRound/&launchRequest），fetchVersionJson 返回后即悬空；网络差（三源全失败）触发重试时
    // use-after-free 崩溃。改为堆上 shared_ptr，闭包按值捕获；launchRequest 侧持 weak_ptr 防循环引用。
    auto startRound    = std::make_shared<std::function<void(int)>>();
    auto launchRequest = std::make_shared<std::function<void(const QString&, const QString&, int)>>();
    const std::weak_ptr<std::function<void(int)>> startRoundWeak = startRound;

    *launchRequest = [this, versionId, raceCtx, processVersionJson, cachedData, cachedJsonPath, startRoundWeak]

        (const QString& url, const QString& sourceName, int round) {



            auto* nam = new QNetworkAccessManager(this);

            auto req = QNetworkRequest(QUrl(url));

            req.setRawHeader("User-Agent", "ShadowLauncher/1.0");

            req.setTransferTimeout(8000 + round * 4000);   // 阶梯超时 8s→12s→16s（多层重试）

            QNetworkReply* reply = nam->get(req);
            m_pendingJsonReplies[versionId].append(reply);   // 供取消时 abort（防取消后旧响应竞态）



            // ── Progress feedback ──

            connect(reply, &QNetworkReply::downloadProgress, this,

                    [this, versionId, raceCtx](qint64 recv, qint64 total) {

                        // Race already decided — don't interfere with started downloader

                        {

                        }

                        // Cancel check: if version was cancelled during JSON fetch, skip progress update
                        if (m_userCancelledIds.contains(versionId)) return;

                        int pct = total > 0 ? (int)(recv * 100 / total) : 0;

                        QString phaseText = tr("获取 %1 版本信息: %2%").arg(versionId).arg(pct);

                        setInstallPhase(phaseText);

                        if (m_dlStates.contains(versionId))

                            m_dlStates[versionId].phase = phaseText;

                        for (auto it = m_downloadSessions.begin(); it != m_downloadSessions.end(); ++it) {

                            auto* d = dlSession(it.key());

                            if (d && d->mcVersion == versionId && !d->steps.isEmpty())

                                updateStep(it.key(), 0, QStringLiteral("active"), pct, recv, total);

                        }

                        if (auto* ds2 = dlSession(versionId); m_downloadSessions.contains(versionId) && ds2 && !ds2->steps.isEmpty())

                            updateStep(versionId, 0, QStringLiteral("active"), pct, recv, total);

                    });



            // ── Finished handler with race logic ──

            // launchRequest 由 startRound 强持有（闭包链），此处 lock 必然成功；防御性短路
            auto startRoundStrong = startRoundWeak.lock();
            if (!startRoundStrong) {
                nam->deleteLater();
                reply->abort();
                reply->deleteLater();
                return;
            }

            connect(reply, &QNetworkReply::finished, this,

                    [this, reply, nam, versionId, raceCtx, processVersionJson, cachedData, cachedJsonPath, sourceName, round, startRoundStrong]() {

                        reply->deleteLater();

                        nam->deleteLater();

                        m_pendingJsonReplies[versionId].removeOne(reply);

                        // 轮次隔离：仅当前轮的源参与竞速判定（旧轮慢请求不干扰新轮）
                        if (raceCtx->roundId != round) return;



                        QByteArray jsonData;

                        bool iAmWinner = false;

                        {

                            QMutexLocker lock(&raceCtx->mtx);

                            if (raceCtx->hasWinner)

                                return;  // already decided




                                // Cancel check: don't declare winner if version was cancelled
                                if (m_userCancelledIds.contains(versionId))
                                    return;

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



                        // Check if all sources failed → next retry round / cache / fail

                        {

                            QMutexLocker lock(&raceCtx->mtx);

                            if (raceCtx->bmclapiDone && raceCtx->mojangDone && raceCtx->launchermetaDone && !raceCtx->hasWinner) {

                                lock.unlock();

                                // 多层重试：全部源失败 → 整体重试下一轮（阶梯超时递增），直至轮次上限
                                if (round + 1 < kJsonFetchMaxRounds) {
                                    qCWarning(logVersion) << QStringLiteral("版本JSON全部源失败 第%1轮 发起第%2轮重试").arg(round + 1).arg(round + 2);
                                    emit logMessage(tr("[版本] 版本信息拉取失败（第 %1 轮），稍后整体重试…").arg(round + 1));
                                    QTimer::singleShot(500, this, [startRoundStrong, round]() { (*startRoundStrong)(round + 1); });
                                    return;
                                }

                                qCWarning(logVersion) << QStringLiteral("全部轮次失败 回退到本地缓存");

                                if (!cachedData.isEmpty()) {

                                    qCWarning(logVersion) << QStringLiteral("使用本地缓存版本JSON 版本=%1 路径=%2").arg(versionId, cachedJsonPath);

                                    processVersionJson(cachedData, QStringLiteral("cache"));

                                } else {

                                    emit logMessage(

                                        tr("获取版本信息失败（已重试 %1 轮，全部源不可用）: %2")

                                            .arg(kJsonFetchMaxRounds).arg(reply->errorString()));

                                    cancelActiveDownload(versionId);

                                }

                            }

                        }

                    });

        };



    // ── 多层重试调度：每轮三源并发竞速，全部失败后整体重试（阶梯超时 8s→12s→16s）──

    QString bmclapiUrl =

        QStringLiteral("https://bmclapi2.bangbang93.com/version/%1/json")

            .arg(versionId);



    // targetVersion.url 形如 https://piston-meta.mojang.com/v1/packages/HASH/VERSION.json

    // 构造替代 CDN: https://launchermeta.mojang.com/v1/packages/HASH/VERSION.json

    QString lmUrl = targetVersion.url;

    lmUrl.replace(QStringLiteral("piston-meta.mojang.com"),

                  QStringLiteral("launchermeta.mojang.com"));



    *startRound = [this, versionId, raceCtx, processVersionJson, cachedData, cachedJsonPath,

                   bmclapiUrl, targetVersion, lmUrl, launchRequest](int round) {

        raceCtx->roundId = round;

        raceCtx->hasWinner = false;

        raceCtx->bmclapiDone = false;

        raceCtx->mojangDone = false;

        raceCtx->launchermetaDone = false;

        emit logMessage(tr("[版本] 拉取版本信息 第 %1/%2 轮（超时 %3s）")

                            .arg(round + 1).arg(kJsonFetchMaxRounds).arg(8 + round * 4));

        (*launchRequest)(bmclapiUrl, QStringLiteral("BMCLAPI"), round);

        (*launchRequest)(targetVersion.url, QStringLiteral("Mojang"), round);

        if (lmUrl != targetVersion.url) {

            (*launchRequest)(lmUrl, QStringLiteral("LauncherMeta"), round);

        } else {

            raceCtx->launchermetaDone = true;   // 无替代地址，立即视为完成

        }

    };

    (*startRound)(0);

}



void VersionBackend::cancelActiveDownload(const QString& versionId)

{
    qCDebug(logVersion).noquote() << "[cancelDbg] cancelActiveDownload ENTER versionId=" << versionId << " inMap=" << m_downloaders.contains(versionId);

    // Rollback an active download that has no downloader yet
    // (used when version JSON fetch fails before downloader is created)
    // NOTE: We do NOT take() from m_downloaders here — the natural signal chain
    // (FileDownloader -> VersionDownloader -> onVersionDownloadFinished) handles
    // all state cleanup (m_dlStates, m_activeIds, m_activeCount). take() causes
    // onVersionDownloadFinished to not find the sender in m_downloaders → return
    // early → userCancelledIds never cleaned, cancellation silently lost.
    //
    // All signal connections are Qt::QueuedConnection, so cancel() does not
    // synchronously trigger onVersionDownloadFinished → no re-entrancy risk.
    if (m_downloaders.contains(versionId)) {
        m_downloaders[versionId]->cancel();
    }

    if (m_activeCount == 0) {

        // Don't close the UI if a loader install is still in progress

        bool loaderRunning = isModLoaderInstalling();

        bool anyMergedPending = false;

        for (auto it = m_downloadSessions.begin(); it != m_downloadSessions.end(); ++it) {

            auto* ds = dlSession(it.key());

            if (ds && ds->isMerged() && !ds->mcDownloadDone) {

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



    // ── Abort in-flight version-JSON race replies for this id ──
    // abort() 使旧请求 finished 必走 error 分支 → 永不进入 processVersionJson →
    // 取消后重新下载时旧响应不会 take()/cancel 新下载器。
    auto abortPendingJson = [this](const QString& vid) {
        auto it = m_pendingJsonReplies.find(vid);
        if (it == m_pendingJsonReplies.end()) return;
        for (auto* r : it.value()) { if (r) r->abort(); }
        m_pendingJsonReplies.remove(vid);
    };



    // ── 整合包任务卡片：原生取消控件转发到任务（任务自行回滚/收尾）──
    if (versionId == m_modpackCardId && m_modpackCancelHandler) {
        m_modpackCancelHandler();
        return;
    }



    // ── Merged install card: clean only this task's resources ──
    // MC downloads to a shared temp dir (shared across contexts with same mcVersion).
    // destroyMergedContext deletes temp dir only when last user goes.
    if (m_mergedContexts.contains(versionId)) {
        QString mcVer;
        auto* ctx = m_mergedContexts.value(versionId, nullptr);
        if (ctx) {
            mcVer = ctx->mcVersion;
            if (ctx->installer) ctx->installer->cancel();
            if (!mcVer.isEmpty()) abortPendingJson(mcVer);
            // ── Abort in-flight loader/API downloads BEFORE any destroy ──
            // 它们的完成回调 capture 裸 ctx*，ctx 一旦 delete 即悬垂崩溃。
            // abort 同步触发 finished（排队）→ 回调在 destroy 前执行（ctx 存活）→
            // 回调内检查 ctx->failed（本分支已置 true）提前返回。
            if (ctx->loaderDlHandle) {
                ctx->loaderDlHandle->abort();
                ctx->loaderDlHandle = nullptr;
            }
            if (ctx->fabricApiReply) {
                ctx->fabricApiReply->abort();
            }
            // OptiFine JAR 竞速下载（BMCLAPI+官方）
            for (auto* r : qAsConst(ctx->optifineJarReplies)) {
                if (r) r->abort();
            }
            ctx->optifineJarReplies.clear();
            // ── 清理 Fabric API 临时文件（系统 temp 下载残留）──
            auto* dsTmp = dlSession(versionId);
            if (dsTmp && !dsTmp->fabricApiSavePath.isEmpty()) {
                QFile::remove(dsTmp->fabricApiSavePath);
                QDir tempParent = QFileInfo(dsTmp->fabricApiSavePath).dir();
                tempParent.rmdir(QFileInfo(dsTmp->fabricApiSavePath).fileName());
                QDir tempRoot(QDir::tempPath() + QStringLiteral("/shadow-fabric-api"));
                if (tempRoot.exists() && tempRoot.isEmpty())
                    tempRoot.rmdir(QDir::tempPath() + QStringLiteral("/shadow-fabric-api"));
            }
        }

        // CRITICAL: Cancel MC download BEFORE destroying temp dir.
        // FileDownloader may be writing to {tempDir}/versions/, and removing
        // the directory while files are open causes access violation (0xc0000005).
        //
        // BUGFIX: Exclude the current context itself from the "still needed" check.
        // The context hasn't been removed from m_mergedContexts yet, so the old loop
        // always found itself and never cancelled the MC download.
        ctx->failed = true;
        ctx->failReason = tr("已取消");

        bool mcStillNeeded = false;
        bool mcDownloadInProgress = false;
        if (!mcVer.isEmpty()) {
            int otherUsers = 0;
            for (auto it = m_mergedContexts.constBegin(); it != m_mergedContexts.constEnd(); ++it) {
                if (it.key() != versionId && it.value() && it.value()->mcVersion == mcVer) {
                    otherUsers++;
                }
            }
            mcStillNeeded = (otherUsers > 0);
            mcDownloadInProgress = m_downloaders.contains(mcVer);
        }

        if (!mcStillNeeded) {
            // ── This is the last context using this MC version — stop the download ──
            if (!mcVer.isEmpty())
                m_userCancelledIds.insert(mcVer);

            // 清理 installVersion(mcVer) append 的 activeIds/count（下载器未创建时
            // onVersionDownloadFinished 不会触发，残留会导致重装卡死）
            if (!mcVer.isEmpty() && m_activeIds.removeOne(mcVer))
                if (m_activeCount > 0) m_activeCount--;

            if (mcDownloadInProgress) {
                // MC download is in progress: cancelActiveDownload queues abort to
                // worker threads via QueuedConnection. Workers may still be writing to
                // tempDir — defer cleanup to onVersionDownloadFinished (which fires after
                // all workers have actually stopped).
                if (!mcVer.isEmpty())
                    cancelActiveDownload(mcVer);
            } else if (ctx->installer && ctx->installer->syncInstallRunning()) {
                // installLegacy2/1 正在主线程同步执行（QEventLoop 等驿道下载）：
                // 不能立即 destroy——installer 的 finished 信号还连着，取消后它会
                // emit finished(false) → finished handler 检测 ctx->failed → 统一销毁
                // （那时 syncInstallRunning 已复位，tempDir 可安全删除，不产生孤儿目录）
                qCInfo(logVersion) << QStringLiteral("[取消] 同步安装执行中，等待 installer finished 后统一销毁 id=%1")
                    .arg(versionId);
                // 不动 m_mergedContexts，等 finished handler 销毁
            } else {
                // MC download is already done (e.g. Forge installer running), or no
                // MC version to download. No worker threads writing to tempDir —
                // safe to destroy the merged context immediately.
                destroyMergedContext(versionId);
                if (!m_gameDir.isEmpty())
                    cleanupCanceledVersion(versionId, m_gameDir);
            }
        }
        // else: MC still needed by other contexts. Keep context alive (shared tempDir).
        // No cleanup needed. The installer was cancelled above.

        auto* ds = dlSession(versionId);
        if (ds) {
            ds->markFailed(tr("已取消"));
            ds->resetSpeed();
            ds->mlSpeed = 0;
            // Mark all steps as failed so the card shows proper state
            for (int i = 0; i < ds->steps.size(); i++)
                updateStep(versionId, i, QStringLiteral("failed"), 0);
        }

        // Bypass throttle: force immediate card rebuild so QML poll picks up the failed state
        m_cardsRebuildPending = false;
        m_cardsTimer.restart();
        doRebuildInstallCards();

        // Also queue a throttled update for safety (covers deferred/throttled paths)
        rebuildInstallCards();

        qDebug() << "[cancelVersionInstall] Merged card cancelled:" << versionId;
        // Do NOT emit installComplete here — that would trigger "下载完成" toast in QML.
        // cancelNotification already shows the correct cancel toast.
        emit cancelNotification(versionId, tr("已取消 %1 的安装").arg(versionId));
        emit logMessage(tr("已取消 %1 的安装").arg(versionId));
        setInstalling(false);
        emit installStateChanged();
        startNextFromQueue();
        return;
    }


    // ── Pure MC version card (existing logic) ──

    QString resolvedId = versionId;

    if (!m_downloaders.contains(versionId) && m_downloadSessions.contains(versionId)) {
        auto* d = dlSession(versionId);
        const QString mcVer = d ? d->mcVersion : QString{};
        if (!mcVer.isEmpty() && m_downloaders.contains(mcVer)) {
            resolvedId = mcVer;
            qCDebug(logVersion).noquote() << "[cancelInstall] resolved " << versionId << " -> mc=" << mcVer;
        }
    }

    if (!m_downloaders.contains(resolvedId)) {
        for (int i = 0; i < m_installQueue.size(); ++i) {
            if (m_installQueue[i] == versionId || m_installQueue[i] == resolvedId) {
                m_installQueue.removeAt(i);
                emit logMessage(tr("已取消队列中的 %1").arg(versionId));
                emit installStateChanged();
                emit downloadQueueChanged();
                return;
            }
        }
        // ── 下载器未创建（版本 JSON 下载阶段）的取消：
        // 标记取消（JSON 回来后 processVersionJson 会 abort 不创建下载器）+
        // 清理 activeIds/count —— 否则取消无效且 activeIds 残留导致重装卡死
        m_userCancelledIds.insert(resolvedId);
        abortPendingJson(resolvedId);
        if (m_activeIds.removeOne(resolvedId))
            if (m_activeCount > 0) m_activeCount--;
        if (m_activeIds.isEmpty()) setInstalling(false);

        auto* jsonDs = dlSession(resolvedId);
        if (jsonDs && !jsonDs->isMerged()) {
            jsonDs->markFailed(tr("已取消"));
            jsonDs->resetSpeed();
            updateCardFromSession(resolvedId, versionId, QStringLiteral("version"));
        }
        emit cancelNotification(versionId, tr("已取消 %1 的安装").arg(versionId));
        emit logMessage(tr("已取消 %1 的安装").arg(versionId));
        emit installStateChanged();
        return;
    }


    m_userCancelledIds.insert(resolvedId);
    abortPendingJson(resolvedId);
    auto dl = m_downloaders.value(resolvedId, nullptr);
    if (dl) dl->cancel();

    emit logMessage(tr("已取消 %1 的安装").arg(versionId));

    m_activeIds.removeOne(resolvedId);
    if (m_activeIds.isEmpty()) setInstalling(false);

    syncPrimaryProgress();
    if (!m_gameDir.isEmpty())
        cleanupCanceledVersion(resolvedId, m_gameDir);

    auto* cancelDs = dlSession(resolvedId);
    if (cancelDs && !cancelDs->isMerged()) {
        cancelDs->markFailed(tr("已取消"));
        cancelDs->resetSpeed();
        updateCardFromSession(resolvedId, versionId, QStringLiteral("version"));
    }

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

    // removeOne returns true if item was present → guard against double-decrement
    if (m_activeIds.removeOne(finishedId))
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

            QString gameDirPath = m_isolation

                ? m_isolation->getVersionGameDir(finishedId)

                : (m_versionMgr->gameDir()

                   + QStringLiteral("/versions/") + finishedId

                   + QStringLiteral("/game"));

            QString mcLang = mc_language::regionToMinecraftLang(m_detectedRegion);

            mc_language::writeOptionsTxt(gameDirPath, mcLang);

            qCInfo(logVersion) << QStringLiteral("安装后语言调整 区域=%1 lang=%2 版本=%3")

                .arg(m_detectedRegion, mcLang, finishedId);

        }




        // ── Pure MC: mark verify step as completed ──
        auto* pureFinishDs = dlSession(finishedId);
        if (pureFinishDs && !pureFinishDs->isMerged()) {
            int verifyIdx = 3;
            if (verifyIdx < pureFinishDs->steps.size())
                updateStep(finishedId, verifyIdx, QStringLiteral("completed"), 100);
        }



        refreshInstalled();
    } else if (m_userCancelledIds.remove(finishedId)) {

        // User cancelled — delete entire version folder, skip installFinished to avoid "失败" toast

        qCInfo(logVersion).noquote() << "onVersionDownloadFinished: user cancelled" << finishedId;

        // ── Deferred merged context cleanup ──
        // cancelVersionInstall defers destroyMergedContext because the worker threads
        // may still be writing to the temp dir. Now that the download is fully stopped
        // (all workers done, allFinished emitted), it's safe to delete temp dirs.
        // Find any merged contexts that were waiting on this MC version download.
        {
            auto contextKeys = m_mergedContexts.keys();
            for (const auto& installId : contextKeys) {
                auto* mergeCtx = m_mergedContexts.value(installId);
                if (mergeCtx && mergeCtx->failed && mergeCtx->mcVersion == finishedId) {
                    // mcDownloader inside ctx is the same object as finishedDl —
                    // already disconnected and deleteLater'd in the cleanup block above.
                    // destroyMergedContext handles this gracefully (extra disconnect/deleteLater is harmless).
                    destroyMergedContext(installId);
                    if (!m_gameDir.isEmpty())
                        cleanupCanceledVersion(installId, m_gameDir);
                }
            }
        }

        // Also clean up the MC version folder from gameDir (it was downloaded to tempDir,
        // so this is a no-op if never copied; safety cleanup if partially copied).
        // NOTE: Use m_versionMgr->gameDir() for the pure-MC version folder (tempDir is same path).
        cleanupCanceledVersion(finishedId, m_versionMgr->gameDir());

        setInstallPhase(tr("取消"));

        refreshInstalled();

        return;

    } else if (!m_autoRepairVersionId.isEmpty()) {

        // ── Auto-repair 完成后的回调 ──

        // 此时 finishedId 来自 repairVersion 的后续操作，不是原始下载

        qCInfo(logVersion) << QStringLiteral("自动修复完成 版本=%1").arg(finishedId);

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

            // 自动修复（verify/repair 感知 merged tempDir，见 repairRootFor）
            qCInfo(logVersion) << QStringLiteral("安装失败，启动自动修复 版本=%1 详情=%2").arg(finishedId, errDetail);
            emit logMessage(
                tr("[重试] %1 安装失败 (%2)，正在尝试自动修复损坏文件...")
                    .arg(finishedId, errDetail));
            m_autoRepairVersionId = finishedId;
            repairVersion(finishedId);
            return;  // repairVersion 完成后会通过 verifyFinished -> installFinished 发射结果


        }

    }



    // ── Emit finished signal (per-download) ──

    emit installFinished(success);



    // ── Update overall state ──

    // Snapshot whether this download belongs to a merged install BEFORE

    // proceedToLoaderInstall can trigger onModLoaderFinished which clears

    // isMerged + mcVersion in a re-entrant call (Fabric finalize is synchronous).

    bool isMergedInstall = false;
    bool anyMergedLaunched = false;


    qCDebug(logVersion).noquote() << QStringLiteral("[TRACE] Phase1 ENTER: finishedId=%1 activeCount=%2 sessions=%3")
        .arg(finishedId).arg(m_activeCount).arg(m_downloadSessions.size());

    // ── Process merged sessions waiting for this MC version ──
    for (auto it = m_mergedContexts.begin(); it != m_mergedContexts.end(); ++it) {
        auto* ctx = it.value();
        if (ctx->mcVersion == finishedId && !ctx->mcDownloadDone) {
            ctx->mcDownloadDone = true;
            isMergedInstall = true;
            qDebug() << "[install] Merged: MC complete for" << ctx->installId;

            // Update session steps
            auto* sessionDs = dlSession(ctx->installId);
            if (sessionDs) {
                sessionDs->mcBytesDl = sessionDs->mcBytesAll;
                for (int i = 0; i < 3 && i < sessionDs->steps.size(); i++)
                    updateStep(ctx->installId, i, QStringLiteral("completed"), 100);
                int verifyIdx = 3;
                if (verifyIdx < sessionDs->steps.size() && !sessionDs->optifineJarParallel)
                    updateStep(ctx->installId, verifyIdx, QStringLiteral("completed"), 100);
            }

            // Check if loader is ready
            if (ctx->loaderJarReady) {
                if (ctx->failed) {
                    qDebug() << "[install] MC done, loader previously failed — finalizing as vanilla" << ctx->installId;
                    finishInstall(ctx->installId);
                } else {
                    qDebug() << "[install] MC done, loader ready — proceeding to install";
                    proceedToLoaderInstall(ctx->installId);
                }
            } else if (ctx->failed) {
                // MC 下载失败且 loader 未就绪：也必须失败收尾（否则卡片卡死无响应）
                qDebug() << "[install] MC failed, loader not ready, finalizing as failed" << ctx->installId;
                finishInstall(ctx->installId);
            }
        }
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

    // ── 纯 MC 收尾：清理 installing 标志（2026-08-05 修复）──
    // 旧代码只在取消/merged 路径 setInstalling(false)，纯 MC 下载完成后
    // m_installing 永久 true → isInstalling() 恒 true → 整合包导入被拒
    // 「已有版本安装任务正在进行」，且后续重复失败被 fail() 的 phase 守卫静默吞掉。
    // merged 场景 m_activeIds 空但 mergedContexts 非空 → isInstalling 仍 true，安全。
    if (m_activeIds.isEmpty())

        setInstalling(false);

    qCDebug(logLaunch) << "[DOWNLOAD] finished=" << finishedId << " active=" << m_activeCount << "/" << MAX_CONCURRENT << " queue=" << m_installQueue.size();

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



// ── 静态工具：在版本目录中查找有效的版本 JSON ──

QString VersionBackend::findVersionJson(const QString& verDir, const QString& dirName)

{

    // Fast path: {dirName}/{dirName}.json — 只做存在性检查，不做 parse

    // 避免每次打开版本选择界面都解析所有 JSON（旧代码同样只检查 exists）

    QString path = verDir + QStringLiteral("/") + dirName + QStringLiteral(".json");

    if (QFileInfo::exists(path)) return path;



    // Slow path: scan all .json files for valid version descriptors

    // 此处需要 parse 验证以区分版本 JSON 和辅助配置（如 authlib-injector）

    QDir vdir(verDir);

    const QStringList jsons = vdir.entryList(QStringList() << QStringLiteral("*.json"), QDir::Files);

    for (const QString& jf : jsons) {

        if (jf == dirName + QStringLiteral(".json")) continue;

        // Skip known auxiliary configs

        if (jf == QStringLiteral("authlib-injector.json")

            || jf == QStringLiteral("fabric-installer.json"))

            continue;

        path = verDir + QStringLiteral("/") + jf;

        QFile jf2(path);

        if (jf2.open(QIODevice::ReadOnly)) {

            QJsonParseError err;

            QJsonDocument doc = QJsonDocument::fromJson(jf2.readAll(), &err);

            jf2.close();

            if (err.error == QJsonParseError::NoError && doc.isObject()) {

                QJsonObject obj = doc.object();

                if (obj.contains(QStringLiteral("mainClass"))

                    && obj.contains(QStringLiteral("type"))

                    && obj.contains(QStringLiteral("id")))

                    return path;

            }

        }

    }

    return QString();

}



// ── 静态工具：在版本目录中查找主 JAR ──

QString VersionBackend::findVersionJar(const QString& verDir, const QString& dirName)

{

    // 1. Try {dir}/{dir}.jar

    QString jarPath = verDir + QStringLiteral("/") + dirName + QStringLiteral(".jar");

    if (QFileInfo::exists(jarPath)) return jarPath;



    // 2. Try JSON's "jar" field (requires parsing version.json)

    const QString json = findVersionJson(verDir, dirName);

    if (!json.isEmpty()) {

        QFile jf(json);

        if (jf.open(QIODevice::ReadOnly)) {

            QJsonDocument doc = QJsonDocument::fromJson(jf.readAll());

            jf.close();

            if (doc.isObject()) {

                QJsonObject obj = doc.object();

                if (obj.contains(QStringLiteral("jar"))) {

                    QString jarName = obj.value(QStringLiteral("jar")).toString();

                    if (!jarName.isEmpty() && jarName != dirName) {

                        QString alt = verDir + QStringLiteral("/") + jarName + QStringLiteral(".jar");

                        if (QFileInfo::exists(alt)) return alt;

                    }

                }

            }

        }

    }



    // 3. Scan any .jar files in the directory (skip non-version jars)

    QDir vd(verDir);

    const QStringList jars = vd.entryList(QStringList() << QStringLiteral("*.jar"), QDir::Files);

    for (const QString& jfile : jars) {

        if (jfile.contains(QStringLiteral("authlib-injector"))

            || jfile.contains(QStringLiteral("nide8auth")))

            continue;

        QString candPath = verDir + QStringLiteral("/") + jfile;

        // Skip files < 1MB — too small to be MC main jar

        QFileInfo fi(candPath);

        if (fi.size() < 1024 * 1024) continue;

        return candPath;

    }



    return QString();  // Not found—loader versions inherit from vanilla

}



void VersionBackend::updateInstalledList()

{

    m_installedIds.clear();

    if (m_gameDir.isEmpty()) return;



    const QString versionsDir = m_gameDir + QStringLiteral("/versions");

    QDir dir(versionsDir);



    if (!dir.exists()) return;



    const QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);



    for (const QString& subDir : subDirs) {

        const QString verPath = versionsDir + QStringLiteral("/") + subDir;



        // Fast path: standard {name}/{name}.jar or {name}/{name}.json

        const QString fastJar = verPath + QStringLiteral("/") + subDir + QStringLiteral(".jar");

        const QString fastJson = verPath + QStringLiteral("/") + subDir + QStringLiteral(".json");

        if (QFileInfo::exists(fastJar) || QFileInfo::exists(fastJson)) {

            m_installedIds.append(subDir);

            continue;

        }



        // Flexible path: use static helper

        if (!findVersionJson(verPath, subDir).isEmpty()) {

            m_installedIds.append(subDir);

        }

    }

}



void VersionBackend::proceedToLoaderInstall(const QString& installId) {

    if (!m_downloadSessions.contains(installId)) return;

    ensureSession(installId);

    auto* ds = dlSession(installId);

    qCDebug(logVersion).noquote() << QStringLiteral("[TRACE] proceedToLoaderInstall: id=%1 type=%2 merged=%3")
        .arg(installId).arg(ds->loaderType).arg(ds->isMerged() ? 1 : 0);

    qDebug() << "[install] Both downloads complete, starting" << ds->loaderType << "verify/install";

    emit logMessage(tr("MC 和 %1 下载完成，开始安装...").arg(ds->loaderType));

    // Use merged context installer if available (new architecture),
    // otherwise fall back to m_mlInstallers (legacy optifine path).
    auto* ml = m_mlInstallers.value(installId, nullptr);
    auto* ctx = m_mergedContexts.value(installId, nullptr);
    if (ctx && ctx->installer) {
        ml = ctx->installer;
        ml->setGameDir(ctx->tempDir);
    } else if (!ml) {
        ml = createLoaderInstaller(installId);
    } else {
        ml->setGameDir(m_gameDir);
    }

    if (ds->loaderType == QStringLiteral("forge")) {

        ml->forgeContinueInstall();

    } else if (ds->loaderType == QStringLiteral("neoforge")) {

        ml->neoForgeContinueInstall();

    } else if (ds->loaderType == QStringLiteral("fabric")) {

        ml->fabricFinalize();

    } else if (ds->loaderType == QStringLiteral("optifine")) {

        // ── OptiFine：jar 先于 MC 完成（jar 小、下载快，通常必先完成）──
        // MC 完成后经 onVersionDownloadFinished merged 分支走到这里；
        // 原来无 optifine 分支 → 安装永不启动（挂起）。
        // 走 delegateOptifineInstall（内部选 ctx->installer + tempDir，与 Fabric 一致）。
        QByteArray jarData = ds->optifineJarData;
        if (jarData.isEmpty()) {
            qCWarning(logVersion) << QStringLiteral("[install] OptiFine jar 数据缺失（proceed 时），无法安装");
            if (ds) ds->markFailed(tr("OptiFine jar 数据缺失"));
            return;
        }
        delegateOptifineInstall(ds->mcVersion, installId, jarData);
    }

}



void VersionBackend::finishInstall(const QString& installName)

{

    qCInfo(logVersion) << QStringLiteral("安装完成 版本=%1").arg(installName);
    qCInfo(logVersion).noquote() << QStringLiteral("[TRACE] finishInstall called: installName=%1 merged=%2 mlInstalling=%3")
        .arg(installName)
        .arg(dlSession(installName) ? (dlSession(installName)->isMerged() ? 1 : 0) : -1)
        .arg(isModLoaderInstalling() ? 1 : 0);

    if (installName.isEmpty()) {

        emit installComplete(QString());

        emit installFinished(true);

        setInstalling(false);

        return;

    }

    ensureSession(installName);

    auto* ds = dlSession(installName);

    // Check for pending user data import

    if (ds->hasImportPending && !ds->importArchivePath.isEmpty()) {

        // Start user data import before completing

        startUserDataImport(installName);

        return;  // installComplete will be emitted by startUserDataImport after import finishes

    }



    // Move Fabric API from temp to mods/ (on any completion path)

    if (!ds->fabricApiSavePath.isEmpty() && !ds->fabricApiFinalPath.isEmpty()) {

        if (QFile::exists(ds->fabricApiSavePath)) {

            QDir().mkpath(QFileInfo(ds->fabricApiFinalPath).absolutePath());

            if (QFile::exists(ds->fabricApiFinalPath)) QFile::remove(ds->fabricApiFinalPath);

            if (QFile::rename(ds->fabricApiSavePath, ds->fabricApiFinalPath)) {

                qDebug() << "[install] Fabric API moved to mods/:" << ds->fabricApiFinalPath;

            } else {

                // 跨盘（系统 temp C: → 游戏目录 D:）rename 失败 → copy + remove 兜底

                qWarning() << "[install] Fabric API rename failed (cross-volume?), fallback copy:"

                           << ds->fabricApiSavePath << "->" << ds->fabricApiFinalPath;

                if (QFile::copy(ds->fabricApiSavePath, ds->fabricApiFinalPath))

                    QFile::remove(ds->fabricApiSavePath);

            }

        } else {

            qWarning() << "[install] Fabric API temp file missing:" << ds->fabricApiSavePath;

        }

        // Clean up temp directory

        QDir tempParent = QFileInfo(ds->fabricApiSavePath).dir();

        tempParent.rmdir(QFileInfo(ds->fabricApiSavePath).fileName());

        QDir tempRoot(QDir::tempPath() + QStringLiteral("/shadow-fabric-api"));

        if (tempRoot.exists() && tempRoot.isEmpty()) tempRoot.rmdir(QStringLiteral("."));

    }



    // Fabric API 步骤收尾（2026-08-10）：恒为最后一步；失败时保持 error 显示不覆盖
    if (ds && ds->steps.size() > 1) {
        auto* apiNode = ds->pipeline() ? ds->pipeline()->stepNode(ds->steps.size() - 1) : nullptr;
        if (!apiNode || apiNode->status() != StepStatus::Failed)
            updateStep(installName, ds->steps.size() - 1, QStringLiteral("completed"), 100);
    }

    // Clear merged flag so the card can retire after installComplete signal

    if (ds->isMerged()) {

        ds->setMerged(false);

        ds->mcVersion.clear();

        ds->loaderType.clear();

        ds->loaderVer.clear();

    }

    // Emit complete BEFORE setInstalling(false) so QML can show final state

    qCInfo(logVersion) << QStringLiteral("[追踪] finishInstall 发射 installComplete/installFinished, receivers=%1")
        .arg(receivers(SIGNAL(installFinished(bool))));

    emit installComplete(installName);

    emit installFinished(true);

    // Clean up merged context (temp dir + children) if this was a merged install
    destroyMergedContext(installName);

    // Defer hiding install card so installComplete signal reaches QML first

    QTimer::singleShot(0, this, [this]() {

        setInstalling(false);

    });

}



void VersionBackend::setInstalling(bool v)

{

    bool wasInstalling = m_installing || (m_activeCount > 0) || !m_mergedContexts.isEmpty();

    m_installing = v;

    bool isNowInstalling = m_installing || (m_activeCount > 0) || !m_mergedContexts.isEmpty();

    qCInfo(logVersion) << QStringLiteral("安装状态变更 安装中=%1 活跃任务=%2 合并上下文=%3").arg(v ? QStringLiteral("是") : QStringLiteral("否")).arg(m_activeCount).arg(m_mergedContexts.size());

    if (wasInstalling != isNowInstalling) {

        emit installStateChanged();

    }

    // 不再调用 rebuildInstallCards()——安装状态变化不需要重建卡片列表
    // (卡片 phase 由 setInstallPhase 独立精准更新, 进度/速度由节流器处理)

}



void VersionBackend::setInstallPhase(const QString& phase)

{

    if (m_installPhase == phase) return;

    m_installPhase = phase;

    emit installPhaseChanged(phase);

    // ── 精准更新: 仅更新活跃 mod_loader 卡片的 PhaseRole ──
    if (m_installCardsModel && m_installCardsModel->count() > 0) {
        bool updated = false;
        // 查找第一个活跃的 installer session key
        if (!m_mlInstallers.isEmpty()) {
            QString firstId = m_mlInstallers.keys().first();
            int row = m_installCardsModel->findRowByIid(firstId);
            if (row >= 0) {
                auto idx = m_installCardsModel->index(row, 0);
                QString t = m_installCardsModel->data(idx, InstallCardModel::TypeRole).toString();
                if (t == QStringLiteral("mod_loader")) {
                    m_installCardsModel->updatePhase(row, phase);
                    updated = true;
                }
            }
        }
        // 兜底: 遍历所有卡片, 更新非失败非等待的 mod_loader 类型
        if (!updated) {
            for (int i = 0; i < m_installCardsModel->count(); ++i) {
                auto idx = m_installCardsModel->index(i);
                QString t = m_installCardsModel->data(idx, InstallCardModel::TypeRole).toString();
                bool failed = m_installCardsModel->data(idx, InstallCardModel::FailedRole).toBool();
                if (t == QStringLiteral("mod_loader") && !failed) {
                    QString ph = m_installCardsModel->data(idx, InstallCardModel::PhaseRole).toString();
                    if (!ph.startsWith(QStringLiteral("等待原版")))
                        m_installCardsModel->updatePhase(i, phase);
                }
            }
        }
    } else {
        // 首次调用: 卡片尚未创建 → 全量重建以创建初始卡片
        rebuildInstallCards();
    }
}



// ============================================================

// Multi-downloader helpers

// ============================================================



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

        setInstallPhase(tr("空闲"));

                                emit installStateChanged();

        return;

    }



    const auto& st = m_dlStates[pid];

    // ── 两段式主进度（2026-08-05 修正）──
    // 旧实现：下载段用字节爬到 100%，校验段直接换成 verifyChecked/verifyTotal
    // （文件数）从 0 开始 → 进度条 100%→0%→100% 回跳。
    // 现在统一折算为 0-100 进度刻度：下载 ×0.9 + 校验 ×0.1（注释宣称的两段式）。
    // 注：卡片进度由步骤管线加权（totalProgress）驱动，本字段无 QML 消费者，
    // 仅保留主进度语义供未来接入（syncPrimaryProgress 仍由进度变化触发）。
    bool verifying = (st.phase == QStringLiteral("\u6821\u9a8c\u4e2d..."));
    qreal segP = 0.0;
    if (verifying) {
        if (st.verifyTotal > 0)
            segP = 0.9 + 0.1 * (qreal)st.verifyChecked / st.verifyTotal;
        else
            segP = 0.9;
    } else if (st.bytesTotal > 0) {
        segP = 0.9 * (qreal)st.bytesDl / st.bytesTotal;
    }
    m_installBytesDl = static_cast<qint64>(segP * 100.0);
    m_installBytesTotal = 100;

    setInstallPhase(st.phase);



                    }



void VersionBackend::updateDownloadProgress(const QString& versionId,

                                            int cf, int tf,

                                            qint64 db, qint64 tb)

{

    // Silently ignore stale queued signals from a cancelled download.
    if (m_userCancelledIds.contains(versionId)) return;

    // ── Update per-download state ──

    auto& st = m_dlStates[versionId];

    st.progress = cf;

    st.total = tf;

    // ── Speed: 单一数据源——网络字节（排除 cache）EMA，带时间常数衰减 ──
    //    updateDownloadProgress 由引擎 speedTick 每 100ms 持续驱动（停流也触发），
    //    每次调用先做指数衰减：无数据时数值平滑回落归零（τ≈1.5s，替代旧的
    //    "30s 无数据才跳零"——后者导致界面速度悬浮不落）。有数据时以 α=0.15
    //    吸收瞬时速度。日志 [速度] MC= 与卡片推送同源同值。
    qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    // Query network bytes from the downloader (excludes cached bytes)
    auto* dl = m_downloaders.value(versionId);
    qint64 cacheBytes = dl ? dl->cachedBytes() : 0;
    qint64 netDb = db - cacheBytes;
    qint64 netDelta = netDb - st.networkBytesDl;

    if (st.speedLastTimeMs > 0) {
        const double dtSec = double(nowMs - st.speedLastTimeMs) / 1000.0;
        if (dtSec > 0) {
            constexpr double kTau = 1.5;   // 时间常数 1.5s：停流后平滑回落
            st.smoothSpeed *= std::exp(-dtSec / kTau);
            if (netDelta > 0) {
                double instantBps = double(netDelta) / dtSec;
                constexpr double kAlpha = 0.15;
                st.smoothSpeed = kAlpha * instantBps + (1.0 - kAlpha) * st.smoothSpeed;
            }
            st.speed = (st.smoothSpeed <= 1.0) ? 0 : static_cast<qint64>(st.smoothSpeed);
        }
    } else if (netDelta > 0) {
        st.smoothSpeed = 0.0;   // 首个真实网络增量：先播种，不显示速度
    }
    st.speedLastTimeMs = nowMs;
    st.networkBytesDl = netDb;

    st.bytesDl = db;

    st.bytesTotal = tb;

    // ── 会话速度同步：DownloadSession 不再自算（recordBytes 已去速度），
    //    统一由本处推送引擎 EMA，保证所有 toCard 路径取值一致 ──
    if (auto* ds = dlSession(versionId)) {
        ds->setSpeed(st.speed);
        ds->recordBytes(db, tb);
    }
    // Route speed to ALL merged sessions sharing this MC version
    for (auto it = m_downloadSessions.begin(); it != m_downloadSessions.end(); ++it) {
        auto* d = dlSession(it.key());
        if (d && d->isMerged() && d->mcVersion == versionId) {
            d->setSpeed(st.speed);
            d->recordBytes(db, tb);
        }
    }

    if (st.phase != tr("校验中...")) {

        st.phase = tr("下载中...");

    }



    // ── Merged install: accumulate MC phase bytes ──

    // Find session whose merged MC version matches this download

    QString mergedSessionId;

    for (auto it = m_downloadSessions.begin(); it != m_downloadSessions.end(); ++it) {

        auto* d = dlSession(it.key());

        if (d && d->isMerged() && d->mcVersion == versionId) {

            mergedSessionId = it.key();

            break;

        }

    }

    if (!mergedSessionId.isEmpty()) {

        auto* mSes = m_downloadSessions[mergedSessionId];
        auto* ds = dlSession(mergedSessionId);

        // Both numerator and denominator come from per-category accounting,

        // not the downloader's global db/tb (which may include uncategorized files

        // like Mojang fallback URLs that don't match /version/, /libraries/, /assets/).

        qint64 catSum = st.catBytesTotal[0] + st.catBytesTotal[1] + st.catBytesTotal[2];

        qint64 catDl = st.catBytesDl[0] + st.catBytesDl[1] + st.catBytesDl[2];

        ds->mcBytesDl = catDl;

        if (catSum > 0) {

            ds->mcBytesAll = catSum;

        }

        // Push real per-category byte progress to all 3 MC download steps

        // Only during download phase — during verify, steps are already marked completed

        if (st.phase != tr("校验中...")) {

            // 步骤 0（下载版本JSON）：经 HTTP race 下载、不走分类统计（cat0 total=0），
            // 下载一旦有字节流即完成——否则 JSON 步骤(权重 3.0)永远 pending 拖低总进度
            if (st.bytesDl > 0 && ds->steps.size() > 0) {
                QVariantMap s0map = ds->steps[0].toMap();
                if (s0map.value("status").toString() != QStringLiteral("completed"))
                    updateStep(mergedSessionId, 0, QStringLiteral("completed"), 100,
                               st.catBytesDl[0], st.catBytesTotal[0]);
            }

            bool allDone = true;

            for (int ci = 0; ci < 3; ci++) {

                if (ds->mcStepTotal[ci] > 0) {

                    // Cap at 100% — mcStepDone (from per-file accounting) may exceed
                    // mcStepTotal (from downloader category totals) when asset counts
                    // differ (discovered after initial task planning).
                    int pct = qMin((int)(ds->mcStepDone[ci] * 100 / ds->mcStepTotal[ci]), 100);

                    QString st = (pct >= 100) ? QStringLiteral("completed") : QStringLiteral("active");

                    updateStep(mergedSessionId, ci, st, pct,

                               ds->mcStepDone[ci], ds->mcStepTotal[ci]);

                    if (pct < 100) allDone = false;

                } else {

                    // mcStepTotal is 0 — this category may be genuinely empty, or the
                    // downloader hasn't populated it yet (MC still in early download).
                    // Either way, don't assume all downloads are done — let
                    // verifyProgressChanged / activateVerifyOnDownloadsDone activate
                    // the verify step when the downloader genuinely enters that phase.
                    allDone = false;

                }

            }

            // When all 3 download steps reach 100%, proactively show verify step (index 3)

            // to avoid a blank gap before the downloader enters the verify phase.

            if (allDone) {

                int verifyIdx = 3;

                QVariantMap vstep = ds->steps.value(verifyIdx).toMap();

                if (verifyIdx < ds->steps.size() && !vstep.value("show").toBool()) {

                    showStep(mergedSessionId, verifyIdx);

                    updateStep(mergedSessionId, verifyIdx, QStringLiteral("active"), 0,

                               0, ds->mcBytesAll);

                    qCInfo(logVersion) << QStringLiteral("提前显示MC验证步骤 session=%1").arg(mergedSessionId);

                }

            }

        }

        // ── merged 卡片进度统一走步骤管线加权（2026-08-05）──

        // 旧实现（已废弃）：mcRaw = 0.5×支持库 + 0.5×资源、mlRaw 加载器进度，

        // raw = (mcRaw+mlRaw)/2 写 smoothProgress——与下方 primaryVersionId 段的

        // grandTotal 公式（含 cat0、无加载器时 0.5 封顶）互相覆盖，两套口径打架，

        // 主版本与其他版本进度算法不一致。现 merged 卡片 progress = totalProgress()

        // （步骤管线加权，与纯 MC 同构；步骤 1 支持库含 client.jar，天然计入）。



    }




    // ── Pure MC: route category progress into ds->steps via updateStep ──
    if (mergedSessionId.isEmpty()) {
        auto* pureDs = dlSession(versionId);
        if (pureDs && !pureDs->isMerged() && pureDs->steps.size() >= 4) {
            auto& st2 = m_dlStates[versionId];
            bool verifying = (st2.phase == tr("校验中..."));
            if (!verifying) {
                // Step 0 (JSON): mark completed once any bytes flow
                if (st2.bytesDl > 0) {
                    QVariantMap step0 = pureDs->steps[0].toMap();
                    if (step0["status"].toString() != QStringLiteral("completed")) {
                        updateStep(versionId, 0, QStringLiteral("completed"), 100, st2.catBytesDl[0], st2.catBytesTotal[0]);
                    }
                }
                // Step 1 (libraries): category index 1
                {
                    QString st1;
                    if (st2.catBytesTotal[1] <= 0) {
                        // 总量未统计（阶段 A 未完成）→ pending 0%，不臆断完成
                        //（旧逻辑 bytesDl>0 就显示 completed/100%——bytesDl 是全局
                        // 下载字节，其他步骤在下载也会 >0 → 步骤创建瞬间误显 100%）
                        st1 = QStringLiteral("pending");
                    } else {
                        st1 = (st2.catBytesDl[1] >= st2.catBytesTotal[1]) ? QStringLiteral("completed") : QStringLiteral("active");
                    }
                    int raw1 = st2.catBytesTotal[1] > 0 ? (int)(st2.catBytesDl[1] * 100 / st2.catBytesTotal[1]) : 0;
                    if (raw1 > 100)
                        qCWarning(logVersion) << QStringLiteral("[pctOverflow] ver=%1 step=1 rawPct=%2 dl=%3KB total=%4KB")
                            .arg(versionId).arg(raw1).arg(st2.catBytesDl[1]/1024).arg(st2.catBytesTotal[1]/1024);
                    int pct1 = qMin(raw1, 100);
                    updateStep(versionId, 1, st1, pct1, st2.catBytesDl[1], st2.catBytesTotal[1]);
                }
                // Step 2 (assets): category index 2
                {
                    QString st2s;
                    if (st2.catBytesTotal[2] <= 0) {
                        st2s = QStringLiteral("pending");
                    } else {
                        st2s = (st2.catBytesDl[2] >= st2.catBytesTotal[2]) ? QStringLiteral("completed") : QStringLiteral("active");
                    }
                    int raw2 = st2.catBytesTotal[2] > 0 ? (int)(st2.catBytesDl[2] * 100 / st2.catBytesTotal[2]) : 0;
                    if (raw2 > 100)
                        qCWarning(logVersion) << QStringLiteral("[pctOverflow] ver=%1 step=2 rawPct=%2 dl=%3KB total=%4KB")
                            .arg(versionId).arg(raw2).arg(st2.catBytesDl[2]/1024).arg(st2.catBytesTotal[2]/1024);
                    int pct2 = qMin(raw2, 100);
                    updateStep(versionId, 2, st2s, pct2, st2.catBytesDl[2], st2.catBytesTotal[2]);
                }
            }
        }
    }



    if (versionId == primaryVersionId()) {

        // speed comes from DownloadSession

        // （m_installBytesDl/Total 由 syncPrimaryProgress 统一两段式折算，2026-08-05）




        // ── 卡片总进度统一（2026-08-05）──

        // 旧公式 B（grandTotal 字节加权 + 无加载器 0.5 封顶）已废弃：与公式 A

        // 双写覆盖 smoothProgress，主版本/次版本口径不一致。merged 卡片进度

        // 由步骤管线加权（totalProgress）驱动，见 updateCardFromSession。




        // ── Route MC step progress to ALL merged session pipelines ──
        for (auto sit = m_downloadSessions.begin(); sit != m_downloadSessions.end(); ++sit) {
            auto* mDs = dlSession(sit.key());
            if (!mDs || !mDs->isMerged() || mDs->mcVersion != versionId) continue;

            auto& mst = m_dlStates[versionId];
            bool mVerifying = (mst.phase == tr("校验中..."));
            if (!mVerifying && !mDs->isFailed() && mDs->steps.size() >= 4) {
                if (mst.bytesDl > 0) {
                    auto step0map = mDs->steps[0].toMap();
                    if (step0map["status"].toString() != QStringLiteral("completed")) {
                        updateStep(sit.key(), 0, QStringLiteral("completed"), 100,
                                   mst.catBytesDl[0], mst.catBytesTotal[0]);
                    }
                }
                {
                    QString s1 = (mst.catBytesTotal[1] <= 0)
                        ? QStringLiteral("pending")
                        : ((mst.catBytesDl[1] >= mst.catBytesTotal[1]) ? QStringLiteral("completed") : QStringLiteral("active"));
                    int mraw1 = mst.catBytesTotal[1] > 0 ? (int)(mst.catBytesDl[1] * 100 / mst.catBytesTotal[1]) : 0;
                    if (mraw1 > 100)
                        qCWarning(logVersion) << QStringLiteral("[pctOverflow:M] ver=%1 step=1 rawPct=%2 dl=%3KB total=%4KB")
                            .arg(sit.key()).arg(mraw1).arg(mst.catBytesDl[1]/1024).arg(mst.catBytesTotal[1]/1024);
                    updateStep(sit.key(), 1, s1, qMin(mraw1, 100),
                        mst.catBytesDl[1], mst.catBytesTotal[1]);
                }
                {
                    QString s2 = (mst.catBytesTotal[2] <= 0)
                        ? QStringLiteral("pending")
                        : ((mst.catBytesDl[2] >= mst.catBytesTotal[2]) ? QStringLiteral("completed") : QStringLiteral("active"));
                    int mraw2 = mst.catBytesTotal[2] > 0 ? (int)(mst.catBytesDl[2] * 100 / mst.catBytesTotal[2]) : 0;
                    if (mraw2 > 100)
                        qCWarning(logVersion) << QStringLiteral("[pctOverflow:M] ver=%1 step=2 rawPct=%2 dl=%3KB total=%4KB")
                            .arg(sit.key()).arg(mraw2).arg(mst.catBytesDl[2]/1024).arg(mst.catBytesTotal[2]/1024);
                    updateStep(sit.key(), 2, s2, qMin(mraw2, 100),
                        mst.catBytesDl[2], mst.catBytesTotal[2]);
                }
            }

            // ── 卡片进度统一（2026-08-05）──
            // merged 卡片 progress 改由步骤管线加权驱动（totalProgress），
            // 此处不再写 smoothProgress（旧的字节 EMA 口径已废弃）。
        }



        if (m_installPhase != tr("校验中...")) {

            setInstallPhase(tr("下载中..."));

        }


    }

}



void VersionBackend::updateDownloadFile(const QString& versionId,

                                        const QString& url,

                                        const QString& fileName,

                                        qint64 received,

                                        qint64 total,

                                        const QString& savePath)

{

    // Silently ignore stale queued signals from a cancelled download.
    // Qt QueuedConnection delivers already-queued events even after disconnect().
    if (m_userCancelledIds.contains(versionId)) return;

    auto& st = m_dlStates[versionId];

    st.file = fileName;



    // ── Determine category from downloader's task-group mapping ──
    //    VersionDownloader::collectTasks() populates m_fileCategory[savePath] = cat
    //    for every file in the download task list. Files downloaded outside the task
    //    list (e.g. asset index via HttpClient) have no mapping → cat=-1 → ignored.
    //    This ensures numerator and denominator come from the same task grouping.

    int cat = -1;
    if (auto* dl = m_downloaders.value(versionId))
        cat = dl->fileCategory(savePath);
    if (cat < 0)
        qCInfo(logVersion) << QStringLiteral("[catNoMap] ver=%1 path=%2").arg(versionId).arg(savePath);


    if (versionId == primaryVersionId()) {

            }



    // ── Per-category done-byte tracking for DlState (version cards) ──

    // Denominator (catBytesTotal) comes from progressChanged->categoryTotalBytes()

    if (cat >= 0 && cat <= 2) {

        // ── Ensure catBytesTotal is set (from downloader task-group totals) ──
        //    progressChanged may fire later; set it here ASAP so numerator/denominator match.
        if (st.catBytesTotal[cat] <= 0) {
            if (auto* dl = m_downloaders.value(versionId)) {
                qint64 ct = dl->categoryTotalBytes(cat);
                if (ct > 0)
                    st.catBytesTotal[cat] = ct;
            }
        }

        qint64 catDone = st.catBytesDoneBase[cat] + received;

        if (received >= total && total > 0) {

            // ── Dedup: only count each file once ──
            //    The downloader may emit fileProgress with received==total multiple times
            //    for the same file (chunked downloads, retries). Track by savePath.
            bool firstTime = !st.catBytesCountedPaths.contains(savePath);

            if (firstTime) {
                st.catBytesDoneBase[cat] += total;
                st.catBytesCountedPaths.insert(savePath);
            }

            catDone = st.catBytesDoneBase[cat];

            // Log completed file (throttled: only every 20th file or files > 1MB)
            // Avoids flooding the UI thread with 5000+ QML binding updates from DebugLogger
            if (total > 1024 * 1024 || (st.logFileCounter++ % 20) == 0)
                emit logMessage(QStringLiteral("[下载] ") + fileName + QStringLiteral(" (%1 KB)").arg(total/1024));



        }

        // Use max() so in-flight progress during download completion doesn't get
        // overwritten by a later fileProgress with smaller received (DEDUP or stale signal).
        // Clamp at catBytesTotal to prevent >100% overflow, with WARN when mismatch.
        if (st.catBytesTotal[cat] > 0 && catDone > st.catBytesTotal[cat]) {
            qCWarning(logVersion) << QStringLiteral("[catOverflow] ver=%1 cat=%2 dl=%3KB > total=%4KB file=%5 dedup=%6")
                .arg(versionId).arg(cat).arg(catDone/1024).arg(st.catBytesTotal[cat]/1024).arg(fileName)
                .arg(st.catBytesCountedPaths.contains(savePath));
            catDone = st.catBytesTotal[cat];
        }
        st.catBytesDl[cat] = qMax(st.catBytesDl[cat], catDone);



        qCDebug(logVersion).noquote()

            << QString("[catDl] version=%1 cat=%2 dl=%3/%4KB (%5%) file=%6")

               .arg(versionId).arg(cat)

               .arg(catDone/1024).arg(st.catBytesTotal[cat]/1024)

               .arg(st.catBytesTotal[cat] > 0 ? catDone * 100 / st.catBytesTotal[cat] : 0)

               .arg(fileName);

    }



    // ── Merged install: route MC download phase to steps 0-2 ──

    QString mergedSessionId;

    for (auto it = m_downloadSessions.begin(); it != m_downloadSessions.end(); ++it) {

        auto* d = dlSession(it.key());

        if (d && d->isMerged() && d->mcVersion == versionId) {

            mergedSessionId = it.key();

            break;

        }

    }



    if (!mergedSessionId.isEmpty()) {

        if (cat >= 0 && cat <= 2) {

            ensureSession(mergedSessionId);

            auto* ds = dlSession(mergedSessionId);            // Use per-category partial-progress accounting (includes in-flight file),

            // same as total progress numerator, so sub-steps and total bar stay in lockstep.

            ds->mcStepDone[cat] = st.catBytesDl[cat];

            qint64 after = ds->mcStepDone[cat];

            qint64 ct = ds->mcStepTotal[cat];

            if (ct > 0) {

                // Per-category step progress: debug-only (fires on every file progress update)
                qCDebug(logVersion) << QStringLiteral("下载步骤 cat=%1 文件=%2 总计=%3KB 已下载=%4/%5KB (%6%)")

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




// ============================================================
// Verify / repair root (merged-aware)
// ============================================================

QString VersionBackend::repairRootFor(const QString& versionId) const
{
    for (auto it = m_mergedContexts.begin(); it != m_mergedContexts.end(); ++it) {
        const auto* ctx = it.value();
        if (ctx->mcVersion == versionId && !ctx->mcDownloadDone && !ctx->tempDir.isEmpty())
            return ctx->tempDir;
    }
    return m_gameDir;
}

void VersionBackend::driveMergedAfterRepair(const QString& versionId)
{
    for (auto it = m_mergedContexts.begin(); it != m_mergedContexts.end(); ++it) {
        auto* ctx = it.value();
        if (ctx->mcVersion != versionId || ctx->mcDownloadDone)
            continue;
        ctx->mcDownloadDone = true;
        // 会话步骤：MC 下载三步 + 校验步骤标记完成
        auto* ds = dlSession(ctx->installId);
        if (ds) {
            ds->mcDownloadDone = true;   // 对齐 ds 标志（fabric 回调检查它）
            ds->mcBytesDl = ds->mcBytesAll;
            for (int i = 0; i < 3 && i < ds->steps.size(); i++)
                updateStep(ctx->installId, i, QStringLiteral("completed"), 100);
            int verifyIdx = 3;
            if (verifyIdx < ds->steps.size() && !ds->optifineJarParallel)
                updateStep(ctx->installId, verifyIdx, QStringLiteral("completed"), 100);
        }
        if (ctx->loaderJarReady)
            proceedToLoaderInstall(ctx->installId);
            // loader 未就绪：不 finishInstall（merged 必有 loader），
            // 等 loader 下载完成回调（检查 ctx->mcDownloadDone → proceedToLoaderInstall）
    }
}

void VersionBackend::verifyVersion(const QString& versionId)
{


    // merged 安装：MC 版本在 tempDir，校验/修复须在该根上进行
    const QString rootDir = repairRootFor(versionId);


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



    const QString versionDir = rootDir + QStringLiteral("/versions/") + versionId;

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

        QString idxPath2 = rootDir + QStringLiteral("/assets/indexes/") + idxId2 + QStringLiteral(".json");

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

    QString libsDir = rootDir + QStringLiteral("/libraries");

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

        QString idxPath = rootDir + QStringLiteral("/assets/indexes/") + idxId + QStringLiteral(".json");

        if (QFileInfo::exists(idxPath)) {

            QFile idxFile(idxPath);

            if (idxFile.open(QIODevice::ReadOnly)) {

                QJsonDocument idxDoc = QJsonDocument::fromJson(idxFile.readAll());

                idxFile.close();

                QJsonObject objects = idxDoc.object().value(QStringLiteral("objects")).toObject();

                QString objectsDir = rootDir + QStringLiteral("/assets/objects");

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

// Inline repair pipeline: download & verify each failed file

// ============================================================

void VersionBackend::repairVersion(const QString& versionId)
{

    // merged 安装：MC 版本在 tempDir，校验/修复须在该根上进行
    const QString rootDir = repairRootFor(versionId);


    if (m_failedPathsCache.isEmpty()) {

        emit logMessage(tr("没有待修复的文件缓存，正在重新校验..."));

        verifyVersion(versionId);

        return;

    }



    // ── Read version JSON to build file → {sha1, url} lookup ──

    QString jsonPath = rootDir + QStringLiteral("/versions/") + versionId

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



    const QString libsDir    = rootDir + QStringLiteral("/libraries");

    const QString objectsDir = rootDir + QStringLiteral("/assets/objects");

    const QString versionDir = rootDir + QStringLiteral("/versions/") + versionId;



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

            QString idxPath = rootDir + QStringLiteral("/assets/indexes/") + idxId + QStringLiteral(".json");

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

                    const QString hash = sha1Hex(data);

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
                                       const QString& forgeInstallerSha1,
                                       const QString& forgeInstallerBranch) {
    // Forge Maven 完整版本 ID：整合包依赖里 loaderVersion 已是完整 ID
    // （如 "26.2-65.1.0"，mrpack dependencies.forge 规范）；手动安装传纯版本
    // （如 "65.1.0"）——此时才需要拼 MC 前缀。原实现无条件拼接导致
    // 完整 ID 变成 "26.2-26.2-65.1.0" → forge 下载 404
    QString m_forgeMavenVer = loaderVersion;
    if (!loaderVersion.startsWith(mcVersion + QLatin1Char('-')))
        m_forgeMavenVer = mcVersion + QStringLiteral("-") + loaderVersion;
    if (!forgeInstallerBranch.isEmpty())
        m_forgeMavenVer += QStringLiteral("-") + forgeInstallerBranch;

    // Create MergedInstallContext for this install
    auto* ctx = createMergedContext(installName, mcVersion, loaderType, loaderVersion);
    if (!ctx) return;

    ensureSession(installName);
    auto* ds = dlSession(installName);
    if (ds) ds->clearFailure();

    // ── Merged install: always MC + loader ──
    qDebug() << "[install] Merged install: MC" << mcVersion << "+" << loaderType << loaderVersion;

    ds->setMerged(true);
    ds->mcVersion = mcVersion;
    ds->loaderType = loaderType;
    ds->loaderVer = loaderVersion;

    // Reset byte accumulators
    ds->mcBytesDl = 0; ds->mcBytesAll = 0;
    ds->mlBytesDl = 0; ds->mlBytesAll = 0; ds->mlBytesDone = 0; ds->mlFileTotal = 0;
    for (int i = 0; i < 3; i++) { ds->mcStepDone[i] = 0; ds->mcStepTotal[i] = 0; }
    ds->mcFileAdded.clear();

    // Build step list
    QString loaderLabel = QStringLiteral("Forge");
    if (loaderType == QStringLiteral("neoforge")) loaderLabel = QStringLiteral("NeoForge");
    else if (loaderType == QStringLiteral("fabric")) loaderLabel = QStringLiteral("Fabric");

    if (loaderType == QStringLiteral("fabric")) {
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
        QVector<bool> shows = {true, true, true, true, true, true, true};
        if (!fabricApiUrl.isEmpty()) {
            stepNames.append(tr("下载 Fabric API"));
            weights.append(0.05);
            // 2026-08-10 修：原 shows=false（hidden）→ weightedProgress 跳过 hidden 步骤 →
            // Fabric API 下载不计入总进度 → 26.2+Fabric 完成瞬间可见步骤全完成=1.0 → 卡片变绿
            // （API 下载中 99.7%，完成后才真正 100%）。改为显示 + 参与加权。
            shows.append(true);
        }
        rebuildSteps(installName, stepNames, weights, shows);
    } else {
        // ── 2026-08-15：顺序换回 校验(5) → 安装器库(6) ──
        // 用户确认 NeoForge 实际时序：主文件下载 → SHA1 校验(verify) → 安装器库下载 → 安装。
        // 上一版把安装器库排在校验前，UI 步骤顺序与真实执行顺序相反（日志实测
        // 23:52:02.404 idx=6 校验 completed 先于 idx=5 安装器库 active）。
        // 真实顺序：主文件(4) → 校验(5) → 安装器库(6) → 安装(7)。
        rebuildSteps(installName, {
            tr("下载原版 JSON 文件"),
            tr("下载原版支持库文件"),
            tr("下载原版资源文件"),
            tr("校验游戏资源完整性"),
            tr("下载 %1 主文件").arg(loaderLabel),
            tr("校验 %1 完整性").arg(loaderLabel),
            tr("下载 %1 安装器库").arg(loaderLabel),
            tr("安装 %1").arg(loaderLabel)
        }, {3.0, 8.0, 5.0, 0.5, 6.0, 0.5, 4.0, 6.0},
         {true, true, true, true, true, true, true, true});
    }

    updateStep(installName, 0, QStringLiteral("active"), 0);
    updateCardFromSession(installName, installName, QStringLiteral("mod_loader"));
    ds->loadedStep = 1;
    setInstalling(true);

    // ── Start MC download ──
    installVersion(mcVersion);
    
    // ── Record ctx under mcVersion for onVersionDownloadFinished routing ──
    // (m_activeIds already checked inside installVersion)

    // ── 先启动 Fabric API 并行下载（2026-08-10 修）──
    // 原实现放在 installFabric() 之后：installFabric 同步阻塞（内部等 finished 信号）→
    // finished handler 执行时 fabricApiPending 还是 false → 步骤 7 被提前标 completed →
    // 加权 1.0 → 卡片变绿一瞬间（实测 17:40:49.023 completed → 49.392 才恢复）；
    // 且 API 下载白白晚启动（未与 Fabric 并行）。提前启动 → 真并行 + 时序正确。
    if (loaderType == QStringLiteral("fabric") && !fabricApiUrl.isEmpty()) {
        auto* ds2 = ensureSession(installName);
        ds2->fabricApiPending = true;
        ds2->fabricApiFinalPath = fabricApiSavePath;
        QString tempDir = QDir::tempPath() + QStringLiteral("/shadow-fabric-api");
        QDir().mkpath(tempDir);
        QString tempApiPath = tempDir + QStringLiteral("/") + QFileInfo(fabricApiSavePath).fileName();
        ds2->fabricApiSavePath = tempApiPath;

        const int apiStepIdx = ds2->steps.size() - 1;   // Fabric API 恒为最后一步
        showStep(installName, apiStepIdx);
        updateStep(installName, apiStepIdx, QStringLiteral("active"), 0);

        auto* apiNam = new QNetworkAccessManager(this);
        QUrl apiUrlObj(fabricApiUrl);
        QNetworkRequest apiReq(apiUrlObj);
        QNetworkReply* apiReply = apiNam->get(apiReq);
        if (auto* actx = m_mergedContexts.value(installName, nullptr))
            actx->fabricApiReply = apiReply;   // 供取消时 abort（回调 capture 裸指针）

        connect(apiReply, &QNetworkReply::downloadProgress, this,
                [this, installName](qint64 recv, qint64 total) {
            int pct = total > 0 ? (int)(recv * 100 / total) : 0;
            auto* dsP = dlSession(installName);
            const int idxP = dsP ? dsP->steps.size() - 1 : 7;
            updateStep(installName, idxP, QStringLiteral("active"), pct);
            if (dsP) {
                qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
                qint64 delta = recv - dsP->fabSpeedLastBytes;
                qint64 timeDelta = nowMs - dsP->fabSpeedLastMs;
                if (delta > 0 && timeDelta >= 200) {
                    dsP->fabSpeed = delta * 1000 / timeDelta;
                    dsP->fabSpeedLastBytes = recv;
                    dsP->fabSpeedLastMs = nowMs;
                }
            }
        });

        connect(apiReply, &QNetworkReply::finished, this, [this, apiReply, apiNam, installName, tempApiPath]() {
            apiReply->deleteLater();
            apiNam->deleteLater();
            if (auto* actx = m_mergedContexts.value(installName, nullptr))
                actx->fabricApiReply = nullptr;
            ensureSession(installName);
            auto* ds = dlSession(installName);
            ds->fabricApiPending = false;
            const int apiIdx = ds->steps.size() - 1;
            if (apiReply->error() != QNetworkReply::NoError) {
                if (auto* actx = mergedContext(installName); actx && actx->failed) {
                    // 取消中：清理临时文件，不再收尾（取消分支已处理卡片与通知）
                    QFile::remove(tempApiPath);
                    return;
                }
                qWarning() << "[install] Fabric API download failed:" << apiReply->errorString();
                updateStep(installName, apiIdx, QStringLiteral("error"), 0);
                setInstallPhase(tr("Fabric API 下载失败"));
                // API 失败也收尾（不带 API），避免封装完成等 API 死等
                auto* mcCtx = mergedContext(installName);
                if (mcCtx && mcCtx->mcDownloadDone && mcCtx->bootstrapperDone) {
                    qCInfo(logVersion) << QStringLiteral("[TRACE-f] API 下载失败，封装已就绪，收尾（不带 API）");
                    finishInstall(installName);
                }
            } else {
                QByteArray data = apiReply->readAll();
                QFile f(tempApiPath);
                if (f.open(QIODevice::WriteOnly)) { f.write(data); f.close(); }
                qDebug() << "[install] Fabric API downloaded to temp:" << tempApiPath << data.size() << "bytes";
                updateStep(installName, apiIdx, QStringLiteral("completed"), 100);
                // 收尾条件：封装完成（bootstrapperDone，installer finished 已跑 copy）才 finishInstall；
                // 封装未完成 → 等 installer finished 路径收尾（那里已检查 fabricApiPending）
                auto* mcCtx = mergedContext(installName);
                qCInfo(logVersion) << QStringLiteral("[TRACE-f] API 完成回调: ctx=%1 mcDone=%2 bootstrapperDone=%3")
                    .arg(mcCtx ? QStringLiteral("yes") : QStringLiteral("NULL"))
                    .arg(mcCtx ? (mcCtx->mcDownloadDone ? 1 : 0) : -1)
                    .arg(mcCtx ? (mcCtx->bootstrapperDone ? 1 : 0) : -1);
                if (mcCtx && mcCtx->mcDownloadDone && mcCtx->bootstrapperDone) {
                    qCInfo(logVersion) << QStringLiteral("[TRACE-f] Fabric API 完成，封装已就绪，收尾");
                    finishInstall(installName);
                } else {
                    qCInfo(logVersion) << QStringLiteral("[TRACE-f] Fabric API 完成，封装未就绪，等待 installer finished 收尾");
                }
            }
        });
    }

    // ── Start loader in parallel ──
    if (loaderType == QStringLiteral("fabric")) {
        ctx->installer->setGameDir(ctx->tempDir);
        ctx->installer->setParallelMode(true);
        ctx->installer->installFabric(mcVersion, loaderVersion, installName);
        return;
    }


    // ── Forge/NeoForge: download installer JAR（按下载源策略选主源，失败走下方 fallback 列表）──
    QString fmv = m_forgeMavenVer;
    QString loaderDlUrl;
    // ── 2026-08-15：加载器主文件下载默认镜像源优先（BMCLAPI）──
    // 不跟随全局 fileSource（那是 MC 下载的源策略，改它会波及夸父/山海经）。
    // 加载器独立策略：镜像先试，失败 fallback 官方——国内网络下官方 maven
    // （maven.neoforged.net / maven.minecraftforge.net）显著拖慢安装。
    // 若后续要开放加载器源设置，在此接入即可（当前固定镜像优先）。
    const bool preferOfficial = false;
    if (loaderType == QStringLiteral("forge")) {
        loaderDlUrl = preferOfficial
            ? QStringLiteral("https://maven.minecraftforge.net/net/minecraftforge/forge/%1/forge-%1-installer.jar").arg(fmv)
            : QStringLiteral("https://bmclapi2.bangbang93.com/maven/net/minecraftforge/forge/%1/forge-%1-installer.jar").arg(fmv);
    } else if (loaderType == QStringLiteral("neoforge")) {
        loaderDlUrl = preferOfficial
            ? QStringLiteral("https://maven.neoforged.net/releases/net/neoforged/neoforge/%1/neoforge-%1-installer.jar").arg(loaderVersion)
            : QStringLiteral("https://bmclapi2.bangbang93.com/maven/net/neoforged/neoforge/%1/neoforge-%1-installer.jar").arg(loaderVersion);
    }

if (!loaderDlUrl.isEmpty()) {
        int loaderDlStepIdx = 4;

        qDebug() << "[Coordinator] Loader download:" << loaderDlUrl;

        // ── 驿道下载（2026-08-07：merged 主文件从裸 QNAM 整合到驿道）──
        // downloadWithReply 下载到临时文件（>4MB 自动分片，驿道内建停滞检测/重试）；
        // 完成回调读入内存 → handleLoaderData（原语义完整保留：进度/100KB 校验/fallback/取消）。
        auto handleLoaderData = [this, installName, loaderType, mcVersion, loaderVersion, forgeInstallerBranch, loaderDlStepIdx, ctx](const QByteArray& data) {
            if (data.size() < 102400) {
                qWarning() << "[Coordinator] Loader download too small:" << data.size();
                updateStep(installName, loaderDlStepIdx, QStringLiteral("failed"), 0, data.size(), 0);
                ctx->failed = true;
                ctx->errorMessage = tr("下载文件异常");
                if (ctx->mcDownloadDone) finishInstall(installName);
                return;
            }
            if (!m_downloadSessions.contains(installName)) return;

            ensureSession(installName);
            auto* ds = dlSession(installName);
            ds->loaderDownloadData = data;

            updateStep(installName, loaderDlStepIdx, QStringLiteral("completed"), 100, data.size(), data.size());
            // ── 2026-08-14 修复：校验步骤按名字找，不再依赖 loaderDlStepIdx+1 ──
            // 步骤顺序已调整为：主文件(idx=4) → 安装器库(idx=5) → 校验(idx=6)。
            // +1 会错指向安装器库。
            int verifyStep = -1;
            for (int i = 0; i < ds->steps.size(); ++i) {
                QString nm = ds->steps[i].toMap().value(QStringLiteral("name")).toString();
                if (nm.contains(QStringLiteral("校验")) && !nm.contains(QStringLiteral("资源"))) {
                    verifyStep = i;
                    break;
                }
            }
            if (verifyStep >= 0 && ds->steps.size() > verifyStep) {
                showStep(installName, verifyStep);
                updateStep(installName, verifyStep, QStringLiteral("active"), 0);
            }

            ctx->installer->setGameDir(ctx->tempDir);
            ctx->installer->setForgeBranch(forgeInstallerBranch);

            // CRITICAL: feed JAR data to installer BEFORE marking ready
            if (loaderType == QStringLiteral("neoforge")) {
                ctx->installer->installNeoForgeFromData(data, ds->mcVersion, ds->loaderVer, installName);
            } else {
                ctx->installer->installForgeFromData(data, ds->mcVersion, ds->loaderVer, installName);
            }

            // ⚠ 不在此时设 loaderJarReady / 触发 proceedToLoaderInstall：
            // Forge/NeoForge 的 installXxxFromData 是异步的（SHA1 校验走网络 fallback），
            // verify 完成才发 waitingForMC → 那里统一设 loaderJarReady + proceed。
            // 提前设会竞态：MC 已完成时立即 forgeContinueInstall → m_cachedJar 还没缓存 →
            // "没有缓存的 Forge 安装程序，无法继续" → 安装停摆（盘古并行化后 MC 更快，竞态必现）。
        };

        // 单源下载封装（驿道）：临时文件 → 读内存 → done(data) 或 fail
        auto downloadViaYidao = [this, installName, loaderDlStepIdx, ctx](const QString& url,
                                    std::function<void(const QByteArray&)> onData,
                                    std::function<void()> onFail) {
            const QString tmpPath = QDir::tempPath() + QStringLiteral("/sl_loader_")
                + QString::number(QRandomGenerator::global()->generate() % 1000000) + QStringLiteral(".jar");
            auto speedState = QSharedPointer<QPair<qint64,qint64>>::create(0, 0);
            // ── 2026-08-15：主文件下载进行中 → 速度计入卡片 ──
            // 此前聚合条件用 isModLoaderInstalling()，但驿道下载主文件阶段
            // installer 未 running（verify-only 模式）→ 主文件速度恒 0。
            // 显式标志驱动：开始 active，完成/失败回调复位。
            if (auto* ds = dlSession(installName))
                ds->loaderDownloadActive = true;
            HttpClient::DownloadHandle* h = HttpClient::instance().downloadWithReply(url, tmpPath,
                [this, installName, loaderDlStepIdx, speedState](qint64 recv, qint64 total) {
                    // ── 2026-08-14 修复：进度真实反映下载状态，不提前到 100% ──
                    // 驿道分片下载 recvBytes 是各片累计已收（只增不减），可能因
                    // 分片超额接收（服务器 206 每片略多）超过 total；且完成回调
                    // 触发前下载仍在进行。
                    // 旧实现 qMin(recv*100/total, 100) 会提前显示 100%，但实际
                    // 下载未完成 → UI"100% 迟迟不进下一步"（掩盖而非解决）。
                    // 正确做法：进度按真实比例推进，完成前上限 99%；真正的 100%
                    // 由 handleLoaderData 完成回调置 completed 时触发。
                    qint64 pct = (total > 0) ? qMin(recv * 100 / total, qint64(99)) : 0;
                    if (pct < 0) pct = 0;
                    // ── 2026-08-15：不再传 bytesRecv/bytesTotal ──
                    // StepNode::setByteProgress 会用 recv*100/total 覆盖 percentage，
                    // 驿道分片超额接收时 recv≥total → 显式 pct(99) 被覆盖成 100，
                    // 而 status 仍是 active → UI 显示"100% + 蓝点"永不转完成态
                    // （用户实测"主文件 100% 不转为完成态"）。
                    // 只传显式 pct（完成前上限 99），真正的 100%+completed 由
                    // handleLoaderData 完成回调置位，杜绝"100% 但 active"。
                    updateStep(installName, loaderDlStepIdx, QStringLiteral("active"),
                               static_cast<int>(pct));
                    qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
                    qint64 delta = recv - speedState->first;
                    qint64 timeDelta = nowMs - speedState->second;
                    qint64 instant = 0;
                    if (timeDelta >= 200 && speedState->second > 0 && delta > 0)
                        instant = delta * 1000 / timeDelta;
                    speedState->first = recv;
                    speedState->second = nowMs;
                    if (auto* ds = dlSession(installName)) {
                        ds->mlBytesDl = recv;
                        ds->mlBytesAll = total;
                        if (instant > 0) ds->mlSpeed = instant;
                    }
                },
                [this, installName, tmpPath, ctx, onData, onFail](bool ok, const QString& err) {
                    if (ctx) ctx->loaderDlHandle = nullptr;
                    if (auto* ds = dlSession(installName))
                        ds->loaderDownloadActive = false;   // 2026-08-15 复位
                    if (!ok) {
                        qWarning() << "[Coordinator] Loader download failed (驿道):" << err;
                        QFile::remove(tmpPath);
                        onFail();
                        return;
                    }
                    QFile f(tmpPath);
                    QByteArray data;
                    if (f.open(QIODevice::ReadOnly)) { data = f.readAll(); f.close(); }
                    QFile::remove(tmpPath);
                    if (data.size() < 102400) { onFail(); return; }
                    onData(data);
                });
            if (ctx) ctx->loaderDlHandle = h;   // 供取消时 abort
        };

        // 主 URL（下载源策略决定官方/镜像）
        downloadViaYidao(loaderDlUrl,
            [handleLoaderData](const QByteArray& data) { handleLoaderData(data); },
            [this, installName, loaderType, loaderVersion, mcVersion, forgeInstallerBranch, loaderDlStepIdx, ctx, downloadViaYidao, handleLoaderData, loaderDlUrl]() {
                // 主 URL 失败 → fallback 链（与主 URL 同一套版本 ID 语义）
                QStringList fallbackUrls;
                if (loaderType == QStringLiteral("forge")) {
                    QString baseVer = loaderVersion;
                    if (!baseVer.startsWith(mcVersion + QLatin1Char('-')))
                        baseVer = mcVersion + QStringLiteral("-") + loaderVersion;
                    // 1.5.2 及更早版本已全部屏蔽（列表层过滤）→ 只需 installer.jar 类别
                    auto addFb = [&](const QString& base, const QString& ver) {
                        fallbackUrls << QStringLiteral("%1/net/minecraftforge/forge/%2/forge-%2-installer.jar").arg(base, ver);
                    };
                    QString branchVer = baseVer;
                    if (!forgeInstallerBranch.isEmpty()) branchVer += QStringLiteral("-") + forgeInstallerBranch;
                    addFb(QStringLiteral("https://bmclapi2.bangbang93.com/maven"), baseVer);
                    addFb(QStringLiteral("https://maven.minecraftforge.net"), branchVer);
                    addFb(QStringLiteral("https://maven.minecraftforge.net"), baseVer);
                } else if (loaderType == QStringLiteral("neoforge")) {
                    // ── 2026-08-15：fallback 补 BMCLAPI 镜像 ──
                    // 此前 NeoForge 主文件 fallback 只有官方 maven.neoforged.net——
                    // 官方源失败时无镜像可退（BMCLAPI 镜像 neoForge 主文件，
                    // 路径同官方）。若主源已走镜像则 fallback 官方；否则镜像先行。
                    const QString rel = QStringLiteral("net/neoforged/neoforge/%1/neoforge-%1-installer.jar").arg(loaderVersion);
                    if (loaderDlUrl.contains(QStringLiteral("bmclapi")))
                        fallbackUrls << QStringLiteral("https://maven.neoforged.net/releases/") + rel;
                    else {
                        fallbackUrls << QStringLiteral("https://bmclapi2.bangbang93.com/maven/") + rel;
                        fallbackUrls << QStringLiteral("https://maven.neoforged.net/releases/") + rel;
                    }
                }

                auto fbIdx = QSharedPointer<int>::create(0);
                auto tryFb = QSharedPointer<std::function<void()>>::create();
                *tryFb = [=]() {
                    if (*fbIdx >= fallbackUrls.size()) {
                        if (ctx->failed) {
                            // 取消中：不再 fallback / 不再收尾（取消分支已处理卡片与通知）
                            updateStep(installName, loaderDlStepIdx, QStringLiteral("failed"), 0, 0, 0);
                            return;
                        }
                        qWarning() << "[Coordinator] Loader download FAILED (all fallbacks)";
                        emit logMessage(QStringLiteral(" %1 下载失败: 所有源均不可用").arg(loaderType));
                        emit logMessage(tr("⚠ %1 下载失败，将以原版安装").arg(loaderType));
                        updateStep(installName, loaderDlStepIdx, QStringLiteral("failed"), 0, 0, 0);
                        ctx->failed = true;
                        ctx->errorMessage = tr("所有源均不可用");
                        if (ctx->mcDownloadDone) finishInstall(installName);
                        return;
                    }
                    const QString url = fallbackUrls[(*fbIdx)++];
                    qWarning() << "[Coordinator] Trying fallback:" << url;
                    downloadViaYidao(url,
                        [handleLoaderData](const QByteArray& data) { handleLoaderData(data); },
                        [tryFb]() { (*tryFb)(); });
                };
                (*tryFb)();
            });
    }
}
void VersionBackend::installOptifine(const QString& mcVersion, const QString& optifineVersion,

        const QString& forgeVersion, const QString& installName,

        const QString& bmclType, const QString& bmclPatch) {

    // Create MergedInstallContext
    auto* ctx = createMergedContext(installName, mcVersion, "optifine", optifineVersion);
    if (!ctx) return;

    ctx->installer->setGameDir(ctx->tempDir);
    if (m_isolation && m_isolation->isVersionIsolated(installName)) {
        ctx->installer->setModsDir(m_isolation->getVersionGameDir(installName) + "/mods");
    } else {
        ctx->installer->setModsDir(QString());
    }

    ensureSession(installName);
    auto* ds = dlSession(installName);
    if (ds) ds->clearFailure();

    ds->setMerged(true);
    ds->mcVersion = mcVersion;
    ds->loaderType = QStringLiteral("optifine");
    ds->loaderVer = optifineVersion;
    ds->bmclType = bmclType;
    ds->bmclPatch = bmclPatch;

    for (int i = 0; i < 3; i++) { ds->mcStepDone[i] = 0; ds->mcStepTotal[i] = 0; }
    ds->mcFileAdded.clear();

    rebuildSteps(installName, {
        tr("下载原版 JSON 文件"),
        tr("下载原版支持库文件"),
        tr("下载原版资源文件"),
        tr("下载 OptiFine 主文件"),
        tr("安装 OptiFine")
    }, {1.0, 8.0, 5.0, 3.0, 1.0},
     {true, true, true, true, false});

    updateStep(installName, 0, QStringLiteral("active"), 0);
    ds->loadedStep = 1;
    setInstalling(true);
    setInstallPhase(tr("下载中..."));

    ds->optifineJarParallel = true;
    installVersion(mcVersion);
    startOptifineJarParallel(installName, mcVersion, optifineVersion, bmclType, bmclPatch);
}


void VersionBackend::delegateOptifineInstall(const QString& mcVersion, const QString& installName,

                                              const QByteArray& jarData) {

    ensureSession(installName);

    auto* ds = dlSession(installName);
    if (ds) ds->optifineInstallTriggered = true;

    showStep(installName, 4);

    updateStep(installName, 4, QStringLiteral("active"), 0);

    setInstallPhase(tr("安装 OptiFine..."));

    auto* optCtx = mergedContext(installName);

    // ── 临时目录适配：优先用 merged ctx 的 installer（已 setGameDir(tempDir)）──
    // 原实现 createLoaderInstaller + setGameDir(m_gameDir) 绕开了临时目录设计：
    // MC 下载在 tempDir，安装器却在真实 gameDir 找 MC → 找不到 → 安装不进行/产物错位。
    // 用 ctx->installer 后安装产物写 tempDir → createMergedContext 的 finished
    // 连接负责 copy 到真实目录 + finishInstall（与 Fabric 同一收尾链）。
    ModLoaderInstaller* ml = (optCtx && optCtx->installer) ? optCtx->installer
                                                           : createLoaderInstaller(installName);
    if (!ml) return;

    ml->setGameDir(optCtx ? optCtx->tempDir : m_gameDir);

    if (m_isolation && m_isolation->isVersionIsolated(installName)) {

        ml->setModsDir(m_isolation->getVersionGameDir(installName) + "/mods");

    } else {

        ml->setModsDir(QString());

    }



    // Connect BEFORE calling installOptifineFromJar so our callback fires first

    // and updates installName before the main handler emits installComplete

    QMetaObject::Connection* conn = new QMetaObject::Connection;

    *conn = connect(ml, &ModLoaderInstaller::finished, this,

        [this, conn, mcVersion, installName](bool success, const QString&) {

            disconnect(*conn);

            delete conn;

            if (!success) return;

            // flattenOptifineVersion has already deleted the inherited MC version folder.
            // Clear the session's merged state NOW (before the main queued handler runs)
            // so it won't attempt redundant cleanup or trigger a re-download.
            if (auto* ds = dlSession(installName)) {
                ds->setMerged(false);
                ds->mcVersion.clear();
                ds->loaderType.clear();
                ds->loaderVer.clear();
            }

            // ── 扫实际版本文件夹名 + 版本隔离迁移：延后到 Queued copy 之后 ──
            // tempDir 模式下 createMergedContext 的 finished（Queued）先执行 copy
            // （版本文件夹此时才落到真实目录），singleShot(0) 在其后执行才扫得到。
            QTimer::singleShot(0, this, [this, mcVersion, installName]() {
                // Scan versions dir for the actual OptiFine folder name (Problem 3 fix)
                QDir verDir(m_gameDir + "/versions");
                QStringList filters; filters << ("*OptiFine*");
                QStringList found = verDir.entryList(filters, QDir::Dirs | QDir::NoDotAndDotDot);
                QString actualId;
                for (const QString& f : found) {
                    if (f != mcVersion) { actualId = f; break; }
                }
                if (!actualId.isEmpty() && actualId != installName) {
                    emit logMessage(tr("[完成] OptiFine 版本 ID: %1").arg(actualId));
                }
                // Ensure version isolation is properly set up for the new OptiFine version
                if (m_isolation) {
                    m_isolation->migrateToIsolated(actualId.isEmpty() ? installName : actualId);
                }
            });

        }, Qt::DirectConnection);  // DirectConnection ensures we fire before queued handlers



    ml->installOptifineFromJar(jarData, mcVersion, installName, ds->bmclType, ds->bmclPatch);

}



void VersionBackend::startOptifineJarParallel(const QString& installName, const QString& mcVersion,

                                                const QString& optifineVersion,

                                                const QString& bmclType, const QString& bmclPatch) {

    // Download OptiFine JAR in parallel with MC download
    // TrueRace: try BMCLAPI + official concurrently

    ensureSession(installName);


    // Build BMCLAPI URL — normalize type/patch from optifineVersion if needed
    QString filename;
    QString bmclUrl;
    {
        QString t = bmclType;
        QString p = bmclPatch;
        if (t.isEmpty() || p.isEmpty()) {
            if (optifineVersion.startsWith(QStringLiteral("HD_U_"))) {
                t = QStringLiteral("HD_U");
                p = optifineVersion.mid(5);
            } else {
                t = QStringLiteral("HD_U");
                p = optifineVersion;
            }
        }
        bmclUrl = QStringLiteral("https://bmclapi2.bangbang93.com/optifine/%1/%2/%3").arg(mcVersion, t, p);
        filename = QStringLiteral("OptiFine_%1_%2_%3.jar").arg(mcVersion, t, p);
    }

    const QString offUrl = ModLoaderInstaller::resolveOptifineOfficialUrl(filename);

    emit logMessage(tr("并行下载 OptiFine: %1").arg(filename));

    // Shared TrueRace state
    auto won = std::make_shared<bool>(false);
    auto pendingCount = std::make_shared<int>(2);

    auto fireUrl = [this, installName, won, pendingCount](const QString& url, const QString& label) {
        auto* nam = new QNetworkAccessManager(this);
        auto* reply = nam->get(QNetworkRequest(QUrl(url)));
        // 供取消时 abort（取消在 destroy 前 abort → 回调先跑且 ctx 存活）
        if (auto* actx = m_mergedContexts.value(installName, nullptr))
            actx->optifineJarReplies.append(reply);

        connect(reply, &QNetworkReply::downloadProgress, this,
            [this, installName, label](qint64 received, qint64 total) {
                if (total > 0) {
                    int pct = (int)(received * 100 / total);
                    updateStep(installName, 3, QStringLiteral("active"), pct, received, total);
                }
                // ── 2026-08-15：OptiFine JAR 下载速度接入卡片 ──
                // 此前 OptiFine 下载完全没算速度（只 updateStep 进度）。
                // 双源竞速只取胜者速度；delta/500ms 计算，更新 fabSpeed
                // 通道（Fabric API 同款），并置 loaderDownloadActive。
                auto* ds = dlSession(installName);
                if (!ds) return;
                ds->loaderDownloadActive = true;
                if (label == QStringLiteral("BMCLAPI") || label == QStringLiteral("官方")) {
                    qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
                    qint64 delta = received - ds->fabSpeedLastBytes;
                    qint64 timeDelta = nowMs - ds->fabSpeedLastMs;
                    if (timeDelta >= 200 && ds->fabSpeedLastMs > 0 && delta > 0)
                        ds->mlSpeed = delta * 1000 / timeDelta;
                    ds->fabSpeedLastBytes = received;
                    ds->fabSpeedLastMs = nowMs;
                }
            });

        connect(reply, &QNetworkReply::finished, this,
            [this, reply, nam, installName, won, pendingCount, label]() {
                if (auto* actx = m_mergedContexts.value(installName, nullptr))
                    actx->optifineJarReplies.removeOne(reply);
                reply->deleteLater();
                nam->deleteLater();

                if (*won) {
                    // 败者源：速度已被胜者接管；若这是唯一活动下载则复位
                    auto* ds = dlSession(installName);
                    if (ds && !ds->optifineJarDone) {
                        // 另一源可能仍在下载，暂不复位；由胜者 finished 复位
                    }
                    return;
                }

                if (reply->error() != QNetworkReply::NoError) {
                    qCInfo(logApp) << QStringLiteral("OptiFine %1 下载失败: %2").arg(label, reply->errorString());
                    (*pendingCount)--;
                    if (*pendingCount <= 0) {
                        *won = true;
                        if (auto* ds = dlSession(installName))
                            ds->loaderDownloadActive = false;   // 2026-08-15
                        emit logMessage(tr("OptiFine 下载失败（BMCLAPI 和官方源均失败）"));
                        updateStep(installName, 3, QStringLiteral("failed"), 0);
                        if (auto* ds = dlSession(installName)) ds->markFailed(tr("OptiFine 下载失败"));
                        setInstalling(false);
                    }
                    return;
                }

                QByteArray jarData = reply->readAll();
                // Validate: BMCLAPI may return HTTP 200 with non-JAR body ("File not found." etc.)
                if (!ModLoaderInstaller::isValidZip(jarData)) {
                    qCWarning(logApp) << QStringLiteral("OptiFine %1 返回无效数据: 大小=%2, 不是有效 ZIP")
                        .arg(label).arg(jarData.size());
                    (*pendingCount)--;
                    if (*pendingCount <= 0) {
                        *won = true;
                        if (auto* ds = dlSession(installName))
                            ds->loaderDownloadActive = false;   // 2026-08-15
                        emit logMessage(tr("OptiFine 下载失败（所有源返回无效数据）"));
                        updateStep(installName, 3, QStringLiteral("failed"), 0);
                        if (auto* ds = dlSession(installName)) ds->markFailed(tr("OptiFine 下载失败"));
                        setInstalling(false);
                    }
                    return;
                }
                *won = true;
                updateStep(installName, 3, QStringLiteral("completed"), 100, jarData.size(), jarData.size());
                onParallelOptifineDone(installName, jarData);
            });
    };

    fireUrl(bmclUrl, QStringLiteral("BMCLAPI"));
    fireUrl(offUrl, QStringLiteral("官方"));
}



void VersionBackend::onParallelOptifineDone(const QString& installName, const QByteArray& jarData) {

    ensureSession(installName);
    auto* ds = dlSession(installName);

    // Guard against double invocation
    if (ds->optifineInstallTriggered) return;
    ds->optifineInstallTriggered = true;
    ds->optifineJarDone = true;
    ds->loaderDownloadActive = false;   // 2026-08-15：OptiFine JAR 下载结束
    if (!jarData.isEmpty()) ds->optifineJarData = jarData;

    // Use merged context for coordination
    auto* ctx = mergedContext(installName);
    if (ctx) {
        ctx->loaderJarReady = true;
        if (!ctx->mcDownloadDone) {
            emit logMessage(tr("OptiFine JAR 下载完成，等待 MC 下载..."));
            return;
        }
    } else if (!ds->mcDownloadDone) {
        emit logMessage(tr("OptiFine JAR 下载完成，等待 MC 下载..."));
        return;
    }

    emit logMessage(tr("[完成] MC 和 OptiFine 均下载完成，开始安装..."));
    QByteArray data = ds->optifineJarData.isEmpty() ? jarData : ds->optifineJarData;
    delegateOptifineInstall(ds->mcVersion, installName, data);
}



void VersionBackend::installOptifineJar(const QString& mcVersion, const QString& optifineVersion,

                                          const QString& bmclType, const QString& bmclPatch,
                                          const QString& installName) {

    // Lightweight: download OptiFine JAR to version's mods/ — no blocking, no installer process

    QString url;
    QString filename;
    {
        QString t = bmclType;
        QString p = bmclPatch;
        if (t.isEmpty() || p.isEmpty()) {
            if (optifineVersion.startsWith(QStringLiteral("HD_U_"))) {
                t = QStringLiteral("HD_U");
                p = optifineVersion.mid(5);
            } else {
                t = QStringLiteral("HD_U");
                p = optifineVersion;
            }
        }
        url = QString("https://bmclapi2.bangbang93.com/optifine/%1/%2/%3").arg(mcVersion, t, p);
        filename = QString("OptiFine_%1_%2_%3.jar").arg(mcVersion, t, p);
    }



    QString targetDir = m_gameDir + "/mods";

    if (m_isolation && m_isolation->isVersionIsolated(installName)) {

        targetDir = m_isolation->getVersionGameDir(installName) + "/mods";

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

            QString offUrl = ModLoaderInstaller::resolveOptifineOfficialUrl(filename);

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



bool VersionBackend::downloadPreferOfficial() const
{
    auto* sb = qobject_cast<const ShadowBackend*>(parent());
    const int s = sb ? sb->fileDownloadSource() : 1;
    // PreferOfficial(1) / AutoSwitch(2) → 官方源优先；PreferMirror(0) → 镜像优先
    return (s == 1 || s == 2);
}

ModLoaderInstaller* VersionBackend::createLoaderInstaller(const QString& installId)
{
    destroyLoaderInstaller(installId);

    auto* ml = new ModLoaderInstaller(this);
    m_mlInstallers[installId] = ml;

    ml->setGameDir(m_gameDir);
    // 2026-08-15：加载器安装器库/依赖库下载默认镜像优先（不跟随全局 fileSource）
    ml->setPreferOfficial(false);

    // --- progressChanged: step-level progress ---
    connect(ml, &ModLoaderInstaller::progressChanged, this,
            [this, installId](int step, int totalSteps, const QString& desc) {

                setInstallPhase(tr("\u6a21\u7ec4\u52a0\u8f7d\u5668: ") + desc);

        auto* ds = dlSession(installId);
        if (!ds) return;

        int stepIdx = ds->isMerged() ? (step - 1 + 4) : (step - 1);

        if (step > 1) {
            int prevIdx = ds->isMerged() ? (step - 2 + 4) : (step - 2);
            updateStep(installId, prevIdx, QStringLiteral("completed"), 100);
        }

        QVariantMap curStep = (stepIdx >= 0 && stepIdx < ds->steps.size()) ? ds->steps[stepIdx].toMap() : QVariantMap{};
        QString curStatus = curStep.value(QStringLiteral("status")).toString();
        if (curStatus != QStringLiteral("active") && curStatus != QStringLiteral("completed")) {
            if (!curStep.value(QStringLiteral("show")).toBool()) {
                showStep(installId, stepIdx);
            }
            updateStep(installId, stepIdx, QStringLiteral("active"), 0);
            ds->loaderStepIdx = stepIdx;
        }
    });

    // --- stepProgress: step-level percentage ---
    connect(ml, &ModLoaderInstaller::stepProgress, this,
            [this, installId](int step, int percentage) {

        auto* ds = dlSession(installId);
        if (!ds) return;

        int stepIdx = ds->isMerged() ? (step - 1 + 4) : (step - 1);
        QString st = (percentage >= 100) ? QStringLiteral("completed") : QStringLiteral("active");
        qCDebug(logVersion).noquote()
            << QString("[stepProg] mlStep=%1 mapped=%2 pct=%3 status=%4 merged=%5")
               .arg(step).arg(stepIdx).arg(percentage).arg(st).arg(ds->isMerged() ? 1 : 0);
        updateStep(installId, stepIdx, st, percentage);

        if (!ds->isMerged()) {
            int totalSteps = qMax(ds->steps.size(), 1);
            int completedSteps = qMin(stepIdx, totalSteps - 1);
            qreal raw = (completedSteps + percentage / 100.0) / totalSteps;
            ds->m_rawTotalProgress = raw;
            if (ds->smoothProgress <= 0.0)
                ds->smoothProgress = raw * 0.7;
            else
                ds->smoothProgress = ds->smoothProgress * 0.3 + raw * 0.7;
        }
    });

    // --- finished: loader install complete ---
    connect(ml, &ModLoaderInstaller::finished, this,
            [this, installId](bool success, const QString& errMsg) {

        auto* ds = dlSession(installId);
        if (!ds || !m_downloadSessions.contains(installId)) {
            qCDebug(logVersion).noquote() << QStringLiteral("[TRACE] finished handler: session gone, destroying installer. id=%1 success=%2 err=%3")
                .arg(installId).arg(success ? 1 : 0).arg(errMsg);
            destroyLoaderInstaller(installId);
            return;
        }

        qCDebug(logVersion).noquote() << QStringLiteral("[TRACE] finished handler ENTER: id=%1 success=%2 err=%3 merged=%4 mcDone=%5 fbPending=%6")
            .arg(installId).arg(success ? 1 : 0).arg(errMsg)
            .arg(ds->isMerged() ? 1 : 0).arg(ds->mcDownloadDone ? 1 : 0).arg(ds->fabricApiPending ? 1 : 0);

        if (success) {
            for (int i = 0; i < ds->steps.size(); i++) {
                // Fabric API 恒为最后一步；pending 时保持 active（否则提前 completed → 卡绿）
                if (i == ds->steps.size() - 1 && ds->fabricApiPending) continue;
                updateStep(installId, i, QStringLiteral("completed"), 100);
            }
            ds->m_rawTotalProgress = 1.0;
            ds->smoothProgress = 1.0;
            emit logMessage(tr("[ModLoader] \u5b89\u88c5\u5b8c\u6210"));
            setInstallPhase(tr("\u5b8c\u6210"));
            refreshInstalled();

            bool allDone = true;
            for (auto it = m_downloadSessions.begin(); it != m_downloadSessions.end(); ++it) {
                auto* ds2 = dlSession(it.key());
                if (ds2->hasPendingLoader || (ds->optifineJarParallel && ds2->isMerged())) {
                    allDone = false; break;
                }
            }
            if (allDone) {
                emit logMessage(tr("[\u5b8c\u6210] \u6240\u6709\u7248\u672c\u5b89\u88c5\u5b8c\u6210\uff01"));
            }

            if (ds->fabricApiPending) {
                qDebug() << "[install] Fabric loader done, waiting for parallel API download";
                return;
            }
            if (ds->isMerged() && !ds->mcDownloadDone) {
                qDebug() << "[install] Loader done, waiting for MC download to complete";
                ds->loaderFinishedWaitingMC = true;
                return;
            }
        } else {
            for (int i = 0; i < ds->steps.size(); i++)
                updateStep(installId, i, QStringLiteral("failed"), 0);
            ds->markFailed(errMsg.isEmpty() ? tr("\u6a21\u7ec4\u52a0\u8f7d\u5668\u5b89\u88c5\u5931\u8d25") : errMsg);
            emit logMessage(tr("[\u5931\u8d25] \u6a21\u7ec4\u52a0\u8f7d\u5668\u5b89\u88c5\u5931\u8d25: %1").arg(errMsg));
        }

        // MC version folder cleanup
        if (success && !ds->mcVersion.isEmpty()) {
            bool otherUsingSameMC = false;
            for (auto it = m_downloadSessions.begin(); it != m_downloadSessions.end(); ++it) {
                auto* d = dlSession(it.key());
                if (it.key() != installId && d && !d->mcVersion.isEmpty() && d->mcVersion == ds->mcVersion) {
                    otherUsingSameMC = true; break;
                }
            }
            if (!otherUsingSameMC) {
                qCInfo(logVersion) << QStringLiteral("MC \u7248\u672c\u6587\u4ef6\u5939\u6e05\u7406: %1 (\u65e0\u5176\u4ed6 session \u4f9d\u8d56)").arg(ds->mcVersion);
                cleanupCanceledVersion(ds->mcVersion, m_gameDir);
                refreshInstalled();
            }
        }

        // finishInstall must be called for the success fall-through path
        // (Timeline B: MC finished before loader, so finishInstall wasn't
        //  called from onVersionDownloadFinished). Timeline A is handled by
        //  onVersionDownloadFinished calling finishInstall directly.
        if (success) {
            finishInstall(installId);
        }

        destroyLoaderInstaller(installId);
        setInstalling(false);
        startNextFromQueue();
        emit installFinished(success);
    });

    // --- waitingForMC ---
    connect(ml, &ModLoaderInstaller::waitingForMC, this, [this, installId]() {
        setInstallPhase(tr("\u7b49\u5f85MC\u4e0b\u8f7d\u5b8c\u6210..."));
        auto* ds = dlSession(installId);
        qCDebug(logVersion).noquote() << QStringLiteral("[TRACE] waitingForMC fired: id=%1 dsFound=%2 merged=%3 mcDone=%4 ldrReady=%5")
            .arg(installId)
            .arg(ds ? 1 : 0)
            .arg(ds ? (ds->isMerged() ? 1 : 0) : -1)
            .arg(ds ? (ds->mcDownloadDone ? 1 : 0) : -1)
            .arg(ds ? (ds->loaderDownloadReady ? 1 : 0) : -1);
        // Only affect the session associated with this installer
        if (ds && ds->isMerged()) {
            qCInfo(logVersion) << QStringLiteral("[TRACE-f] waitingForMC ds=%1 mcDone=%2").arg(installId).arg(ds->mcDownloadDone ? 1 : 0);
            // Mark verify step as completed (mirrors old constructor handler)
            int verifyStep = ds->loaderVerifyStep;
            if (verifyStep >= 0 && verifyStep < ds->steps.size()) {
                updateStep(installId, verifyStep, QStringLiteral("completed"), 100);
            }
            ds->loaderDownloadReady = true;
            // Fabric 路径关键修复：waitingForMC = profile+依赖库已就绪（loader 相当于已备好），
            // 必须同步标记 merged context 的 loaderJarReady —— 否则 onVersionDownloadFinished
            // 检查 ctx->loaderJarReady 永远 false → MC 下载完成后不调 proceedToLoaderInstall →
            // 安装卡死（版本文件夹不写 JSON/不复制 jar）。Forge 在 handleLoaderData 已设置，Fabric 漏了。
            if (auto* ctx = m_mergedContexts.value(installId, nullptr))
                ctx->loaderJarReady = true;
            // Do NOT set loaderFinishedWaitingMC here — that flag means "the installer
            // has already finished its work (emit finished), just waiting for MC".
            // waitingForMC means the installer is PAUSED at verify and needs
            // proceedToLoaderInstall → forgeContinueInstall to continue.
            // If MC already finished, proceed to loader install immediately
            if (ds->mcDownloadDone) {
                proceedToLoaderInstall(installId);
            }
        }
    });

    // --- byteProgress ---
    connect(ml, &ModLoaderInstaller::byteProgress, this,
            [this, installId](const QString& file, qint64 received, qint64 total, qint64 speed) {

        auto* ds = dlSession(installId);
        if (!ds) return;

        int stepIdx = ds->loaderStepIdx;
        if (stepIdx < 0) {
            for (int i = 0; i < ds->steps.size(); i++) {
                auto st = ds->steps[i].toMap();
                QString name = st.value(QStringLiteral("name")).toString();
                if (name.contains(QStringLiteral("\u5b89\u88c5\u5668\u5e93")) ||   // 安装器库
                    name.contains(QStringLiteral("\u4f9d\u8d56\u5e93")) ||         // 依赖库(fabric)
                    name.contains(QStringLiteral("loader"), Qt::CaseInsensitive) ||
                    name.contains(QStringLiteral("\u52a0\u8f7d\u5668"), Qt::CaseInsensitive) ||
                    name.contains(QStringLiteral("\u4e3b\u6587\u4ef6"), Qt::CaseInsensitive) ) {
                    stepIdx = i;
                    break;
                }
            }
        }
        qint64 pct = (total > 0) ? qMin((received * 100) / total, qint64(100)) : 0;
        if (stepIdx < 0) return;
        updateStep(installId, stepIdx, QStringLiteral("active"), static_cast<int>(pct));
        ds->mlBytesDl = received;
        ds->mlBytesAll = total;
        ds->mlSpeed = speed;
        if (total > 0 && received >= total) {
            if (!ds->loaderDownloadReady) {
                ds->loaderDownloadReady = true;
            }
        }
    });

    // --- verifyStarted ---
    connect(ml, &ModLoaderInstaller::verifyStarted, this,
            [this, installId]() {

        setInstallPhase(tr("\u6821\u9a8c\u4e2d"));
        auto* ds = dlSession(installId);
        if (!ds) return;

        int loaderStepIdx = ds->loaderStepIdx;
        if (loaderStepIdx < 0) {
            for (int i = 0; i < ds->steps.size(); i++) {
                if (ds->steps[i].toMap().value(QStringLiteral("name")).toString()
                    .contains(QStringLiteral("verify"), Qt::CaseInsensitive)) {
                    loaderStepIdx = i;
                    break;
                }
            }
        }
        if (loaderStepIdx >= 0) {
            updateStep(installId, loaderStepIdx, QStringLiteral("active"), 0);
            ds->loaderStepIdx = loaderStepIdx;
        }
    });

    // --- verifyFinished ---
    connect(ml, &ModLoaderInstaller::verifyFinished, this,
            [this, installId](bool ok) {

        auto* ds = dlSession(installId);
        if (!ds) return;
        // ── 2026-08-14 修复：不再用 ds->loaderStepIdx ──
        // loaderStepIdx 在 verify 完成瞬间可能已被 installerLibsStarted 改为
        // 安装器库步骤（idx=6），用它标 completed 会把安装器库误标 100% 完成，
        // 随后下载进度又拉回 active → UI 出现"100% + 剩余62"的矛盾闪烁。
        // 明确找"校验"步骤（主文件校验 idx=5）标记完成；找不到则不动。
        for (int i = 0; i < ds->steps.size(); ++i) {
            QString name = ds->steps[i].toMap().value(QStringLiteral("name")).toString();
            if (name.contains(QStringLiteral("校验")) || name.contains(QStringLiteral("verify"), Qt::CaseInsensitive)) {
                // 跳过 MC 资源校验（"校验游戏资源完整性"），只标加载器校验步骤
                if (name.contains(QStringLiteral("资源"))) continue;
                updateStep(installId, i, ok ? QStringLiteral("completed")
                                            : QStringLiteral("failed"), ok ? 100 : 0);
                break;
            }
        }
    });

    // --- logMessage ---
    connect(ml, &ModLoaderInstaller::logMessage, this, &VersionBackend::logMessage);

    // --- toastMessage (e.g. auto Java download during loader install) ---
    connect(ml, &ModLoaderInstaller::toastMessage, this, &VersionBackend::toastMessage);

    return ml;
}

void VersionBackend::destroyLoaderInstaller(const QString& installId)
{
    auto* ml = m_mlInstallers.take(installId);
    if (ml) {
        ml->disconnect();
        ml->deleteLater();
    }
}



bool VersionBackend::isModLoaderInstalling() const {

    for (auto it = m_mlInstallers.constBegin(); it != m_mlInstallers.constEnd(); ++it) {
        if (it.value() && it.value()->isRunning()) return true;
    }
    for (auto it = m_mergedContexts.constBegin(); it != m_mergedContexts.constEnd(); ++it) {
        if (it.value() && it.value()->installer) {
            // ── 2026-08-15：安装器库下载阶段也算"安装中" ──
            // verify-only 模式（installNeoForgeFromData）m_running=false，
            // 但 forgeStepLibs 下载安装器库仍在进行（m_installerLibsRunning=true）。
            // 此前 isModLoaderInstalling()=false → ds->mlSpeed 不计入速度显示
            // → UI 速度恒 0（用户实测"下载安装器库时速度是0但下载确实在进行"）。
            if (it.value()->installer->isRunning()
                || it.value()->installer->installerLibsActive())
                return true;
        }
    }
    return false;

}



// ============================================================

// Unified Step Model

// ============================================================



// ═══════════ Session helpers ═══════════



DownloadSession* VersionBackend::ensureSession(const QString& installId) {
    if (m_downloadSessions.contains(installId))
        return m_downloadSessions[installId];
    auto* ds = new DownloadSession(installId, this);
    m_downloadSessions[installId] = ds;
    QObject::connect(ds, &DownloadSession::progressUpdated, this, [this, installId]() {
        // Deferred update via throttle to avoid hammering QML at 60+ updates/sec
        if (!m_pendingCardUpdates.contains(installId))
            m_pendingCardUpdates.append(installId);
        if (!m_cardUpdateThrottle.isActive())
            m_cardUpdateThrottle.start();
    });
    return ds;
}







DownloadSession* VersionBackend::dlSession(const QString& installId) const {

    return m_downloadSessions.value(installId, nullptr);

}



// ── 轻量更新：仅取 progress + speed，不改 steps/name/phase ──
// 用于下载进度热路径（每 200ms），避免触发 QML 全部绑定重新评估
void VersionBackend::updateCardFromSession(const QString& installId, const QString& name, const QString& type) {

    auto* ds = dlSession(installId);

    if (!ds || !m_installCardsModel) return;

    // 整合包运行期间：MC/加载器会话不生成独立卡片（进度并入任务卡片子步骤）
    if (isModpackSessionSuppressed(installId)) return;

    // ── Shared: build step list from pipeline (real-time status/percentage) ──
    // Pipeline 是实时数据源 (所有 setPercentage/setActive 都写入 pipeline)
    // ds->steps 是陈旧 copy, 仅通过 updateStep 刷新, 不是所有下载路径都用 updateStep
    // 所以直接从 pipeline 构建步骤列表, 确保 QML poll 到最新状态
    auto buildPipelineSteps = [](DownloadSession* ds) -> QVariantList {
        QVariantList steps;
        auto* pipeline = ds ? ds->pipeline() : nullptr;
        if (!pipeline) return steps;
        for (int i = 0; i < pipeline->totalSteps(); ++i) {
            auto* node = pipeline->stepNode(i);
            if (!node) continue;
            QVariantMap s;
            s["name"] = node->name();
            switch (node->status()) {
                case StepStatus::Pending:   s["status"] = QStringLiteral("pending"); break;
                case StepStatus::Active:    s["status"] = QStringLiteral("active"); break;
                case StepStatus::Completed: s["status"] = QStringLiteral("completed"); break;
                case StepStatus::Failed:    s["status"] = QStringLiteral("failed"); break;
                case StepStatus::Skipped:   s["status"] = QStringLiteral("skipped"); break;
            }
            s["percentage"] = node->percentage();
            s["detail"] = node->detail();
            s["show"] = !node->isHidden();
            steps.append(s);
        }
        return steps;
    };

    // ── Helper: derive phase string from pipeline steps ──
    auto derivePhase = [](const QVariantList& steps) -> QString {
        bool allDone = true, anyActive = false, anyFailed = false;
        QString activeName;
        for (const QVariant& vs : steps) {
            QVariantMap s = vs.toMap();
            QString st = s.value("status").toString();
            if (st == "active") { if (!anyActive) activeName = s.value("name").toString(); anyActive = true; allDone = false; }
            else if (st == "pending" || st == "skipped") { allDone = false; }
            else if (st == "failed") { anyFailed = true; }
        }
        if (anyFailed) return QStringLiteral("失败");
        else if (allDone) return QStringLiteral("已完成");
        else if (anyActive) return activeName;
        return QString();
    };

    // Merged installs: in-place update with m_dlStates network speed
    if (ds->isMerged()) {
        int mrow = m_installCardsModel->findRowByIid(installId);
        if (mrow < 0) {
            QString effectiveName = name.isEmpty() ? installId : name;
            QString effectiveType = type.isEmpty() ? QStringLiteral("mod_loader") : type;
            InstallCard mergedCard = ds->toCard(installId, effectiveName, effectiveType);
            // Override steps with pipeline data (more up-to-date)
            mergedCard.steps = buildPipelineSteps(ds);
            mergedCard.type = effectiveType;
            qint64 ts = 0;
            if (m_dlStates.contains(ds->mcVersion) && !ds->mcDownloadDone)
                ts += m_dlStates[ds->mcVersion].speed;
            if (ds->loaderDownloadActive && ds->mlSpeed > 0)
                ts += ds->mlSpeed;
            if (ds->fabricApiPending && ds->fabSpeed > 0)
                ts += ds->fabSpeed;
            mergedCard.speed = ts;
            mergedCard.progress = qBound(0.0, ds->totalProgress(), 1.0);
            mergedCard.phase = derivePhase(mergedCard.steps);
            m_installCardsModel->appendRow(mergedCard);
        } else {
            // Existing merged card: update steps from pipeline + progress/speed
            QVariantList pipelineSteps = buildPipelineSteps(ds);
            const InstallCard* existing = m_installCardsModel->cardAt(mrow);
            bool stepsChanged = false;
            if (existing) {
                stepsChanged = (existing->steps.size() != pipelineSteps.size());
                if (!stepsChanged) {
                    for (int i = 0; i < pipelineSteps.size(); ++i) {
                        QVariantMap oldS = existing->steps[i].toMap();
                        QVariantMap newS = pipelineSteps[i].toMap();
                        if (oldS.value("status") != newS.value("status") ||
                            oldS.value("percentage").toInt() != newS.value("percentage").toInt() ||
                            oldS.value("show").toBool() != newS.value("show").toBool()) {
                            stepsChanged = true;
                            break;
                        }
                    }
                }
            }
            if (stepsChanged) {
                m_installCardsModel->updateStepList(mrow, pipelineSteps);
                // Update phase from new steps
                QString newPhase = derivePhase(pipelineSteps);
                if (!newPhase.isEmpty() && existing && existing->phase != newPhase)
                    m_installCardsModel->updatePhase(mrow, newPhase);
            }

            qreal p = qBound(0.0, ds->totalProgress(), 1.0);
            qint64 s = 0;
            if (m_dlStates.contains(ds->mcVersion) && !ds->mcDownloadDone)
                s += m_dlStates[ds->mcVersion].speed;
            if (ds->loaderDownloadActive && ds->mlSpeed > 0)
                s += ds->mlSpeed;
            if (ds->fabricApiPending && ds->fabSpeed > 0)
                s += ds->fabSpeed;
            m_installCardsModel->updateProgressAndSpeed(mrow, p, s);
        }
        return;
    }

    int row = m_installCardsModel->findRowByIid(installId);

    if (row < 0) {
        // New card — create full card via toCard
        InstallCard card = ds->toCard(installId,
            name.isEmpty() ? installId : name,
            type.isEmpty() ? QStringLiteral("version") : type);
        if (!ds->isMerged() && m_dlStates.contains(installId))
            card.speed = m_dlStates[installId].speed;
        m_installCardsModel->appendRow(card);
        return;
    }

    QVariantList pipelineSteps = buildPipelineSteps(ds);

    // 检查步骤是否与 card 中一致 (检测变化)
    const InstallCard* existing = m_installCardsModel->cardAt(row);
    bool stepsChanged = false;
    if (existing) {
        stepsChanged = (existing->steps.size() != pipelineSteps.size());
        if (!stepsChanged) {
            for (int i = 0; i < pipelineSteps.size(); ++i) {
                QVariantMap oldS = existing->steps[i].toMap();
                QVariantMap newS = pipelineSteps[i].toMap();
                if (oldS.value("status") != newS.value("status") ||
                    oldS.value("percentage").toInt() != newS.value("percentage").toInt() ||
                    oldS.value("show").toBool() != newS.value("show").toBool()) {
                    stepsChanged = true;
                    break;
                }
            }
        }
    } else {
        stepsChanged = true;
    }

    // ── 用 pipeline 数据更新 card 的 steps ──
    // (无信号发射, QML 端通过 Timer+_stepsJson 控制 Repeater 重建)
    m_installCardsModel->updateStepList(row, pipelineSteps);

    // ── 从 pipeline 推导 phase ──
    QString newPhase = derivePhase(pipelineSteps);

    // 更新 progress + speed (每 200ms 都会跑, cheap)
    // 卡片进度统一：merged/纯 MC 均走步骤管线加权 totalProgress（2026-08-05）
    qreal newP = qBound(0.0, ds->totalProgress(), 1.0);  // 2026-08-05 统一：merged/纯MC 均走步骤加权
    qint64 newS = 0;
    if (!ds->isMerged() && m_dlStates.contains(installId))
        newS = m_dlStates[installId].speed;
    m_installCardsModel->updateProgressAndSpeed(row, newP, newS);

    // 更新 phase / failed / completed
    const InstallCard* cur = m_installCardsModel->cardAt(row);
    InstallCard patch;
    bool needPatch = false;

    if (cur) {
        if (!newPhase.isEmpty() && cur->phase != newPhase) {
            patch.phase = newPhase;
            needPatch = true;
        }
        if (ds->isFailed() && !cur->failed) {
            patch.failed = true;
            patch.error = ds->errorMessage();
            patch.canCancel = false;
            needPatch = true;
        }
        if (ds->totalProgress() >= 1.0 && cur->progress < 1.0) {  // 2026-08-05 统一口径
            patch.canCancel = false;
            needPatch = true;
        }
    }

    if (needPatch) {
        InstallCard fullPatch = *cur;
        if (!patch.phase.isEmpty()) fullPatch.phase = patch.phase;
        if (patch.failed) {
            fullPatch.failed = true;
            fullPatch.error = patch.error;
            fullPatch.canCancel = false;
        }
        if (patch.canCancel == false && cur->canCancel != false)
            fullPatch.canCancel = false;
        m_installCardsModel->updateRow(row, fullPatch);
    }

}







void VersionBackend::rebuildSteps(const QString& installId, const QStringList& names, const QVector<qreal>& weights,

                                   const QVector<bool>& showFlags) {

    auto* ds = ensureSession(installId);

    ds->steps.clear();
    ds->pipeline()->clearSteps();

    for (int i = 0; i < names.size(); i++) {

        qreal w = (i < weights.size()) ? weights[i] : 1.0;
        bool show = (i < showFlags.size()) ? showFlags[i] : true;

        // ── Create StepNode via pipeline ──
        auto* node = ds->pipeline()->addStep(
            QString::number(i), names[i], w
        );
        if (node) node->setHidden(!show);

        // ── Sync back to old QVariantList ──
        QVariantMap step;
        step["name"] = names[i];
        step["status"] = QStringLiteral("pending");
        step["percentage"] = 0;
        step["bytesReceived"] = QVariant::fromValue<qint64>(0);
        step["bytesTotal"] = QVariant::fromValue<qint64>(0);
        step["weight"] = w;
        step["show"] = show;
        ds->steps.append(step);

    }

    ds->m_rawTotalProgress = 0.0;
    ds->smoothProgress = 0.0;


    // If this session has a pending user data import, append the import step
    // (only if not already present to avoid duplicates on rebuild)
    if (ds->hasImportPending) {

        bool alreadyHasImport = false;
        for (const auto& st : ds->steps) {
            if (st.toMap().value("name").toString().contains("导入用户数据")) {
                alreadyHasImport = true;
                break;
            }
        }
        if (!alreadyHasImport) {

            // ── Create import StepNode ──
            int idx = ds->steps.size();
            auto* node = ds->pipeline()->addStep(
                QString::number(idx), tr("导入用户数据"), 0.03
            );
            if (node) node->setHidden(false);

            QVariantMap importStep;
            importStep["name"] = tr("导入用户数据");
            importStep["status"] = QStringLiteral("pending");
            importStep["percentage"] = 0;
            importStep["bytesReceived"] = QVariant::fromValue<qint64>(0);
            importStep["bytesTotal"] = QVariant::fromValue<qint64>(0);
            importStep["weight"] = 0.03;
            importStep["show"] = true;
            ds->steps.append(importStep);

        }

    }

    // 首次启动 pipeline（同一 session 多次 rebuild 不会重复 start）
    if (ds->pipeline() && ds->pipeline()->currentStepIndex() < 0)
        ds->pipeline()->start();

}



void VersionBackend::showStep(const QString& installId, int index) {

    auto* ds = ensureSession(installId);

    if (index < 0 || index >= ds->steps.size()) return;

    // ── 2026-08-15：状态机保护——showStep 不得把已完成/失败步骤打回 active ──
    // showStep 原本无条件 setActive + 写 ds->steps["status"]="active"，
    // 绕过 updateStep 的状态机保护（completed 不被降级）。
    // 若 verifyFinished 已把校验步骤标 completed，而后续又有路径（如
    // handleLoaderData 的 showStep(verifyStep) / verifyStarted）对同一索引
    // 调 showStep，会把 completed 打回 active → UI "100% 但冒三点/进行态"
    // 永不转完成（用户实测"校验Neoforge完整性 100% 不转完成态"）。
    // 规则与 updateStep 一致：terminal 状态不允许被 active/pending 覆盖。
    {
        QString curStatus = ds->steps[index].toMap().value(QStringLiteral("status")).toString();
        if (curStatus == QStringLiteral("completed")
            || curStatus == QStringLiteral("failed")
            || curStatus == QStringLiteral("skipped"))
            return;
    }

    // ── Update StepNode: unhide + activate ──
    if (auto* node = ds->pipeline()->stepNode(index)) {
        node->setHidden(false);
        node->setActive();
    }

    // ── Sync to old QVariantList ──
    QVariantMap step = ds->steps[index].toMap();
    step["show"] = true;
    step["status"] = QStringLiteral("active");
    step["percentage"] = 0;
    ds->steps[index] = step;

    updateCardFromSession(installId);

}



// 按步骤名（子串匹配）查找步骤索引；未找到返回 -1（2026-08-14）
// 用于 progressChanged 的语义映射——merged 步骤数组插入"安装器库"后，
// step-1+4 硬编码偏移会错位（step3 安装 → 误标校验/安装器库）。
int VersionBackend::findStepByName(const QString& installId, const QString& namePart)
{
    auto* ds = dlSession(installId);
    if (!ds) return -1;
    for (int i = 0; i < ds->steps.size(); ++i) {
        const QString nm = ds->steps[i].toMap().value(QStringLiteral("name")).toString();
        if (nm.contains(namePart)) return i;
    }
    return -1;
}

// ── 2026-08-15：精确语义查找 ──
// findStepByName 是模糊子串匹配：步骤数组同时存在
//   idx=3 "校验游戏资源完整性"   （MC 下载校验）
//   idx=6 "校验 NeoForge 完整性"（loader 安装器校验）
// 两者都包含"完整性"，从头遍历必然误命中 idx=3 →
// verifyStarted/verifyFinished 驱动错步骤：MC 校验被提前标 active/completed，
// 而 loader 校验永不更新（用户实测"安装都开始了校验还在冒三点"）。
// 同理"安装 NeoForge"含"安装"，但"下载 NeoForge 安装器库"也含"安装"，
// findStepByName("安装") 会误命中安装器库步骤。以下三个函数用
// 包含+排除组合精确定位，杜绝歧义。
int VersionBackend::findLoaderVerifyStep(const QString& installId)
{
    auto* ds = dlSession(installId);
    if (!ds) return -1;
    for (int i = 0; i < ds->steps.size(); ++i) {
        const QString nm = ds->steps[i].toMap().value(QStringLiteral("name")).toString();
        if (nm.contains(QStringLiteral("完整性")) && !nm.contains(QStringLiteral("资源")))
            return i;   // "校验 X 完整性" ✓；"校验游戏资源完整性"被排除
    }
    return -1;
}

int VersionBackend::findLoaderInstallStep(const QString& installId)
{
    auto* ds = dlSession(installId);
    if (!ds) return -1;
    for (int i = 0; i < ds->steps.size(); ++i) {
        const QString nm = ds->steps[i].toMap().value(QStringLiteral("name")).toString();
        if (nm.contains(QStringLiteral("安装")) && !nm.contains(QStringLiteral("安装器库")))
            return i;   // "安装 X" ✓；"下载 X 安装器库"被排除
    }
    return -1;
}

int VersionBackend::findLoaderLibsStep(const QString& installId)
{
    auto* ds = dlSession(installId);
    if (!ds) return -1;
    for (int i = 0; i < ds->steps.size(); ++i) {
        const QString nm = ds->steps[i].toMap().value(QStringLiteral("name")).toString();
        if (nm.contains(QStringLiteral("安装器库")))
            return i;
    }
    return -1;
}

void VersionBackend::updateStep(const QString& installId, int index, const QString& status, int percentage,

                                 qint64 bytesRecv, qint64 bytesTotal) {
    auto* ds = ensureSession(installId);

    if (index < 0 || index >= ds->steps.size()) return;

    // ── 状态机保护（2026-08-14 修复问题3：主文件 100% 不变完成）──
    // 背景：MC 下载与 forge/neoforge 下载并行，多个进度信号（byteProgress /
    // downloadViaYidao progress / installerLibsFileProgress）各自更新不同子步骤，
    // 但 loaderStepIdx 可能被多处指向；下载完成回调置 completed 后，若有迟到
    // 的 active 进度（如驿道 recv==total 的最后一跳）落到同一步骤，会把
    // completed 覆盖回 active → 步骤 100% 却永不"完成"。
    // 规则：非 completed/failed 状态不得降级已完成的步骤（active/pending 迟到更新
    // 一律忽略；显式重新激活由调用方先置 pending 再 active）。
    {
        QString curStatus = ds->steps[index].toMap().value(QStringLiteral("status")).toString();
        const bool terminal = (curStatus == QStringLiteral("completed")
                            || curStatus == QStringLiteral("failed")
                            || curStatus == QStringLiteral("skipped"));
        const bool downgrade = (status == QStringLiteral("active")
                             || status == QStringLiteral("pending"));
        if (terminal && downgrade) {
            // 迟到的 active/pending 更新忽略——不覆盖已完成状态
            return;
        }
    }

    // ── Update StepNode ──
    // 2026-08-14：统一钳制 percentage 到 [0,100]——驿道下载 recv 可能超过 total
    // （分片/206/合并场景），多处调用路径（显式 pct、bytesRecv/bytesTotal 自动计算）
    // 若某一路径未钳制，进度会突破 100%（实测主文件 110%）。
    if (percentage < 0) percentage = 0;
    if (percentage > 100) percentage = 100;
    auto* node = ds->pipeline()->stepNode(index);
    if (node) {
        if (status == "active") {
            node->setActive();
        } else if (status == "completed") {
            node->setCompleted();
        } else if (status == "failed") {
            node->setFailed();
        } else if (status == "pending") {
            node->setStatus(StepStatus::Pending);
        } else if (status == "error") {
            node->setFailed();
        } else if (status == "skipped") {
            node->setSkipped();
        }
        if (bytesRecv > 0 && bytesTotal > 0)
            node->setByteProgress(bytesRecv, bytesTotal);
        // ── 2026-08-15：显式 percentage 必须在 setByteProgress 之后设置 ──
        // StepNode::setByteProgress 内部用 recv*100/total 重算并覆盖 m_percentage。
        // 原顺序（先 setPercentage 再 setByteProgress）会让字节口径覆盖显式 pct：
        // 驿道分片 recv 超额时 recv≥total → 显式 pct(99) 被覆盖成 100，而 status
        // 仍是 active → UI "100% 蓝点"永不转完成态。显式 pct 应优先。
        if (percentage > 0)
            node->setPercentage(percentage);
    }

    // ── Sync to old QVariantList ──
    QVariantMap step = ds->steps[index].toMap();

    // Auto-compute percentage from bytes if provided and no explicit percentage given
    if (percentage == 0 && bytesRecv > 0 && bytesTotal > 0) {
        percentage = (int)((bytesRecv * 100) / bytesTotal);
        if (percentage > 100) percentage = 100;   // 2026-08-14 钳制
    }

    QString oldStatus = step["status"].toString();
    step["status"] = status;
    step["percentage"] = percentage;
    step["bytesReceived"] = QVariant::fromValue<qint64>(bytesRecv);
    step["bytesTotal"] = QVariant::fromValue<qint64>(bytesTotal);
    ds->steps[index] = step;

    // Only log on actual state transitions (pending→active→completed→failed)
    // to avoid repeating identical completed-step info hundreds of times per second
    if (oldStatus != status) {
        qCInfo(logVersion).noquote()
            << QStringLiteral("步骤 id=%1 idx=%2 名称=%3 状态=%4 进度=%5% 字节=%6/%7KB")
               .arg(installId).arg(index)
               .arg(step["name"].toString())
               .arg(status)
               .arg(percentage)
               .arg(bytesRecv / 1024).arg(bytesTotal / 1024);
    }



    // Use throttled card update — direct updateCardFromSession on every progress event
    // would flood the main thread (QNetworkReply fires 60+ events/sec during downloads).
    if (!m_pendingCardUpdates.contains(installId))
        m_pendingCardUpdates.append(installId);
    if (!m_cardUpdateThrottle.isActive())
        m_cardUpdateThrottle.start();

    }







int VersionBackend::installRemainingSteps(const QString& sessionId) const {

    if (sessionId.isEmpty() || !m_downloadSessions.contains(sessionId)) return 0;

    int n = 0;

    auto* ds3 = dlSession(sessionId);
    if (!ds3) return n;

    for (const QVariant& v : ds3->steps) {

        QString s = v.toMap().value(QStringLiteral("status")).toString();

        if (s == QStringLiteral("pending") || s == QStringLiteral("active")) n++;

    }

    return n;

}



InstallCard VersionBackend::resourceCardToInstallCard(const QString& cardId) const
{
    InstallCard c;
    c.iid = cardId;
    const QVariantMap cm = m_extraCards.value(cardId);
    c.type = cm.value(QStringLiteral("type"), QStringLiteral("resource")).toString();
    c.name = cm.value(QStringLiteral("displayName"), cardId).toString();
    c.progress = qBound(0.0, cm.value(QStringLiteral("totalProgress"), 0.0).toReal(), 1.0);
    c.speed = cm.value(QStringLiteral("speed"), QVariant::fromValue<qint64>(0)).value<qint64>();
    c.phase = cm.value(QStringLiteral("installPhase"), QString()).toString();
    c.remaining = 0;
    c.steps = QVariantList{};
    c.failed = cm.value(QStringLiteral("failed"), false).toBool();
    c.error = cm.value(QStringLiteral("error"), QString()).toString();
    c.canCancel = cm.value(QStringLiteral("canCancel"), true).toBool();
    return c;
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
    c["failed"] = false;
    c["error"] = QString();
    c["canCancel"] = true;
    m_extraCards[cardId] = c;

    // ── 精准插入而非全量重建 ──
    if (m_installCardsModel && m_installCardsModel->findRowByIid(cardId) < 0) {
        InstallCard card;
        card.iid = cardId;
        card.name = displayName;
        card.type = QStringLiteral("resource");
        m_installCardsModel->appendRow(card);
    }
}



void VersionBackend::updateResourceCard(const QString& cardId, qreal progress, const QString& status, qint64 speed) {

    if (!m_extraCards.contains(cardId)) return;

    QVariantMap c = m_extraCards[cardId];
    c["totalProgress"] = progress;
    c["speed"] = QVariant::fromValue<qint64>(speed);
    if (!status.isEmpty()) c["installPhase"] = status;
    m_extraCards[cardId] = c;

    // ── 精准更新进度+速度+阶段 ──
    if (m_installCardsModel) {
        int row = m_installCardsModel->findRowByIid(cardId);
        if (row >= 0) {
            m_installCardsModel->updateProgressAndSpeed(row, progress, speed);
            if (!status.isEmpty())
                m_installCardsModel->updatePhase(row, status);
        }
    }
}



void VersionBackend::failResourceCard(const QString& cardId, const QString& error) {

    if (!m_extraCards.contains(cardId)) return;

    QVariantMap c = m_extraCards[cardId];
    c["failed"] = true;
    c["error"] = error;
    c["canCancel"] = true;   // 失败态保留关闭按钮（手动关闭）
    m_extraCards[cardId] = c;

    // ── 全量刷新该行（failed/error/canCancel 各角色一起更新）──
    if (m_installCardsModel) {
        int row = m_installCardsModel->findRowByIid(cardId);
        if (row >= 0)
            m_installCardsModel->updateRow(row, resourceCardToInstallCard(cardId));
    }
}



void VersionBackend::completeResourceCard(const QString& cardId) {

    if (!m_extraCards.contains(cardId)) return;

    QVariantMap c = m_extraCards[cardId];
    c["totalProgress"] = 1.0;
    c["installPhase"] = tr("已完成");
    c["speed"] = QVariant::fromValue<qint64>(0);
    c["failed"] = false;
    c["error"] = QString();
    c["canCancel"] = false;   // 完成态隐藏取消/关闭按钮（绿色定格）
    m_extraCards[cardId] = c;

    if (m_installCardsModel) {
        int row = m_installCardsModel->findRowByIid(cardId);
        if (row >= 0)
            m_installCardsModel->updateRow(row, resourceCardToInstallCard(cardId));
    }
}



void VersionBackend::removeResourceCard(const QString& cardId) {

    m_extraCards.remove(cardId);

    // ── 精准移除而非全量重建 ──
    if (m_installCardsModel) {
        int row = m_installCardsModel->findRowByIid(cardId);
        if (row >= 0)
            m_installCardsModel->removeRow(row);
    }
}



// ═══════════════════════════════════════════════════════════
// 整合包任务卡片（原生卡片通道）
// 卡片生命周期：addTaskCard → updateTaskCard/Steps/Attachments →
//   完成/失败后由 DownloadQueueCard 自动收拢或手动关闭触发 dismissCard
//   → removeTaskCard。步骤格式与普通下载卡片一致（{name,status,percentage,show}），
//   QML 点阵/呼吸动画天然生效。
// ═══════════════════════════════════════════════════════════

InstallCard VersionBackend::taskCardToInstallCard(const QString& cardId) const
{
    InstallCard c;
    c.iid = cardId;
    const QVariantMap m = m_taskCards.value(cardId);
    c.name = m.value(QStringLiteral("displayName"), cardId).toString();
    c.type = QStringLiteral("modpack");
    c.progress = qBound(0.0, m.value(QStringLiteral("totalProgress"), 0.0).toReal(), 1.0);
    c.speed = m.value(QStringLiteral("speed"), QVariant::fromValue<qint64>(0)).value<qint64>();
    c.phase = m.value(QStringLiteral("installPhase"), QString()).toString();
    c.steps = m.value(QStringLiteral("steps")).toList();
    c.mods = m.value(QStringLiteral("mods")).toList();
    c.logs = m.value(QStringLiteral("logs")).toList();
    c.info = m.value(QStringLiteral("info")).toMap();
    c.failed = m.value(QStringLiteral("failed"), false).toBool();
    c.error = m.value(QStringLiteral("error"), QString()).toString();
    c.canCancel = m.value(QStringLiteral("canCancel"), true).toBool();
    return c;
}

void VersionBackend::addTaskCard(const QString& cardId, const QString& displayName)
{
    if (cardId.isEmpty()) return;
    QVariantMap m;
    m[QStringLiteral("displayName")] = displayName;
    m[QStringLiteral("totalProgress")] = 0.0;
    m[QStringLiteral("speed")] = QVariant::fromValue<qint64>(0);
    m[QStringLiteral("installPhase")] = QString();
    m[QStringLiteral("steps")] = QVariantList{};
    m[QStringLiteral("mods")] = QVariantList{};
    m[QStringLiteral("logs")] = QVariantList{};
    m[QStringLiteral("info")] = QVariantMap{};
    m[QStringLiteral("failed")] = false;
    m[QStringLiteral("error")] = QString();
    m[QStringLiteral("canCancel")] = true;
    m_taskCards.insert(cardId, m);

    if (m_installCardsModel && m_installCardsModel->findRowByIid(cardId) < 0)
        m_installCardsModel->appendRow(taskCardToInstallCard(cardId));
}

void VersionBackend::updateTaskCard(const QString& cardId, qreal progress, const QString& phase,
                                    bool failed, const QString& error, qint64 speed,
                                    bool canCancel, const QString& name)
{
    if (!m_taskCards.contains(cardId)) return;
    QVariantMap m = m_taskCards.value(cardId);
    m[QStringLiteral("totalProgress")] = qBound(0.0, progress, 1.0);
    if (!phase.isEmpty()) m[QStringLiteral("installPhase")] = phase;
    m[QStringLiteral("failed")] = failed;
    m[QStringLiteral("error")] = error;
    m[QStringLiteral("speed")] = QVariant::fromValue<qint64>(speed);
    m[QStringLiteral("canCancel")] = canCancel;
    if (!name.isEmpty()) m[QStringLiteral("displayName")] = name;
    m_taskCards.insert(cardId, m);

    if (m_installCardsModel) {
        int row = m_installCardsModel->findRowByIid(cardId);
        if (row >= 0)
            m_installCardsModel->updateRow(row, taskCardToInstallCard(cardId));
    }
}

void VersionBackend::updateTaskCardSteps(const QString& cardId, const QVariantList& steps)
{
    if (!m_taskCards.contains(cardId)) return;
    QVariantMap m = m_taskCards.value(cardId);
    m[QStringLiteral("steps")] = steps;
    m_taskCards.insert(cardId, m);

    if (m_installCardsModel) {
        int row = m_installCardsModel->findRowByIid(cardId);
        if (row >= 0)
            m_installCardsModel->updateRow(row, taskCardToInstallCard(cardId));
    }
}

void VersionBackend::updateTaskCardAttachments(const QString& cardId, const QVariantList& mods,
                                               const QVariantList& logs, const QVariantMap& info)
{
    if (!m_taskCards.contains(cardId)) return;
    QVariantMap m = m_taskCards.value(cardId);
    if (!mods.isEmpty()) m[QStringLiteral("mods")] = mods;
    if (!logs.isEmpty()) m[QStringLiteral("logs")] = logs;
    if (!info.isEmpty()) m[QStringLiteral("info")] = info;
    m_taskCards.insert(cardId, m);

    if (m_installCardsModel) {
        int row = m_installCardsModel->findRowByIid(cardId);
        if (row >= 0)
            m_installCardsModel->updateRow(row, taskCardToInstallCard(cardId));
    }
}

void VersionBackend::removeTaskCard(const QString& cardId)
{
    m_taskCards.remove(cardId);
    if (m_installCardsModel) {
        int row = m_installCardsModel->findRowByIid(cardId);
        if (row >= 0)
            m_installCardsModel->removeRow(row);
    }
    if (cardId == m_modpackCardId) {
        m_modpackCardId.clear();
        m_modpackTargetVersion.clear();
        m_modpackMcVersion.clear();
        m_modpackCancelHandler = nullptr;
    }
}

// ── 整合包任务注册 / 会话卡片抑制 ──

void VersionBackend::setModpackCard(const QString& cardId, std::function<void()> cancelHandler)
{
    m_modpackCardId = cardId;
    m_modpackCancelHandler = std::move(cancelHandler);
}

void VersionBackend::updateModpackCardTargets(const QString& targetVersion, const QString& mcVersion)
{
    m_modpackTargetVersion = targetVersion;
    m_modpackMcVersion = mcVersion;
}

bool VersionBackend::isModpackSessionSuppressed(const QString& installId) const
{
    if (m_modpackCardId.isEmpty()) return false;
    return installId == m_modpackTargetVersion || installId == m_modpackMcVersion;
}

qreal VersionBackend::installProgressOf(const QString& installId) const
{
    auto* ds = dlSession(installId);
    return ds ? qBound(0.0, ds->totalProgress(), 1.0) : 0.0;  // 2026-08-05 统一口径
}

qint64 VersionBackend::installSpeedOf(const QString& installId) const
{
    auto* ds = dlSession(installId);
    if (!ds) return 0;
    if (m_dlStates.contains(ds->mcVersion) && !ds->mcDownloadDone)
        return m_dlStates.value(ds->mcVersion).speed;
    return ds->mlSpeed > 0 ? ds->mlSpeed : 0;
}

QVariantList VersionBackend::sessionSteps(const QString& installId) const
{
    // 整合包任务卡片透传：从会话 pipeline 取实时原子步骤（与 updateCardFromSession
    // 内部 buildPipelineSteps 同源；ds->steps 是陈旧 copy，pipeline 才是实时数据源）
    QVariantList steps;
    auto* ds = dlSession(installId);
    auto* pipeline = ds ? ds->pipeline() : nullptr;
    if (!pipeline) return steps;
    for (int i = 0; i < pipeline->totalSteps(); ++i) {
        auto* node = pipeline->stepNode(i);
        if (!node) continue;
        QVariantMap s;
        s[QStringLiteral("name")] = node->name();
        switch (node->status()) {
            case StepStatus::Pending:   s[QStringLiteral("status")] = QStringLiteral("pending"); break;
            case StepStatus::Active:    s[QStringLiteral("status")] = QStringLiteral("active"); break;
            case StepStatus::Completed: s[QStringLiteral("status")] = QStringLiteral("completed"); break;
            case StepStatus::Failed:    s[QStringLiteral("status")] = QStringLiteral("failed"); break;
            case StepStatus::Skipped:   s[QStringLiteral("status")] = QStringLiteral("skipped"); break;
        }
        s[QStringLiteral("percentage")] = node->percentage();
        s[QStringLiteral("show")] = !node->isHidden();
        steps.append(s);
    }
    return steps;
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



void InstallCardModel::updateProgressAndSpeed(int row, qreal progress, qint64 speed) {
    if (row < 0 || row >= m_cards.size()) return;
    // Poll 模式下只更新内部字段，不发射 dataChanged
    // QML 端通过 Timer 轮询 cardData() 获取最新值
    m_cards[row].progress = progress;
    m_cards[row].speed = speed;
}

const InstallCard* InstallCardModel::cardAt(int row) const {
    if (row < 0 || row >= m_cards.size()) return nullptr;
    return &m_cards[row];
}

void InstallCardModel::updateRow(int row, const InstallCard& card) {
    if (row < 0 || row >= m_cards.size()) return;
    // Poll 模式：只更新字段，不发射 dataChanged
    m_cards[row] = card;
}

// ── 原地更新步骤列表，保持 QVariantList 身份不变 ──
// QML Repeater 不会重建 delegate（因为 model 引用未变）
// 返回是否实际修改了任何字段
bool InstallCardModel::updateStepList(int row, const QVariantList& newSteps) {
    if (row < 0 || row >= m_cards.size()) return false;
    // Poll 模式：整体替换 steps，不发射 dataChanged
    // QML 端通过 cardData() 获取最新值，步骤 Repeater 会重建
    // （步骤变化频率低，重建开销可接受）
    m_cards[row].steps = newSteps;
    return true;
}

void InstallCardModel::updatePhase(int row, const QString& phase) {
    if (row < 0 || row >= m_cards.size()) return;
    // Poll 模式：只写字段，不发射 dataChanged
    m_cards[row].phase = phase;
}

void InstallCardModel::insertRow(int row, const InstallCard& card) {

    if (row < 0 || row > m_cards.size()) return;

    beginInsertRows(QModelIndex(), row, row);

    m_cards.insert(row, card);

    endInsertRows();

    emit generationChanged();

}



void InstallCardModel::appendRow(const InstallCard& card) {

    insertRow(m_cards.size(), card);

}



void InstallCardModel::removeRow(int row) {

    if (row < 0 || row >= m_cards.size()) return;

    beginRemoveRows(QModelIndex(), row, row);

    m_cards.removeAt(row);

    endRemoveRows();

    emit generationChanged();

}



int InstallCardModel::findRowByIid(const QString& iid) const {
    for (int i = 0; i < m_cards.size(); ++i) {
        if (m_cards[i].iid == iid) return i;
    }
    return -1;
}

QVariantMap InstallCardModel::cardData(int row) const {
    QVariantMap map;
    if (row < 0 || row >= m_cards.size()) return map;
    const auto& c = m_cards[row];
    map["iid"] = c.iid;
    map["name"] = c.name;
    map["type"] = c.type;
    map["progress"] = c.progress;
    map["speed"] = static_cast<qint64>(c.speed);
    map["phase"] = c.phase;
    map["remaining"] = c.remaining;
    map["steps"] = c.steps;
    map["failed"] = c.failed;
    map["error"] = c.error;
    map["totalProgressVisible"] = c.totalProgressVisible;
    map["hasUserDataImport"] = c.hasUserDataImport;
    map["canCancel"] = c.canCancel;
    map["importFailedAtMs"] = c.importFailedAtMs;
    // 整合包任务卡片附属数据（QML 轮询消费；普通卡片为空，不影响既有逻辑）
    map["mods"] = c.mods;
    map["logs"] = c.logs;
    map["info"] = c.info;
    return map;
}



void InstallCardModel::rebuild(const QVector<InstallCard>& cards) {

    LOG_CARDS() << "rebuild() START oldSize=" << m_cards.size() << " newSize=" << cards.size()

                 << "gen=" << m_generation;

    for (int i = 0; i < cards.size(); ++i) {

        LOG_CARDS() << "  card[" << i << "] iid=" << cards[i].iid << " name=" << cards[i].name

                     << " progress=" << cards[i].progress << " steps=" << cards[i].steps.size();

    }



    // Poll 模式：同尺寸原地更新字段（不发 dataChanged，QML 靠 Timer 轮询）
    // 不同尺寸：full reset（必须，否则 ListView 计数不同步）
    if (cards.size() == m_cards.size()) {
        for (int i = 0; i < cards.size(); ++i) {
            m_cards[i] = cards[i];
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

    for (auto it = m_downloadSessions.begin(); it != m_downloadSessions.end(); ++it) {

        auto* d = dlSession(it.key());

        if (d && d->isMerged() && d->mcVersion == versionId) {

            qCDebug(logVersion).noquote() << QStringLiteral("[verify-scan]  merged session=%1").arg(it.key())
                << QStringLiteral("mcStepTotal=[%1,%2,%3]")
                    .arg(d->mcStepTotal[0]).arg(d->mcStepTotal[1]).arg(d->mcStepTotal[2])
                << QStringLiteral("mcStepDone=[%1,%2,%3]")
                    .arg(d->mcStepDone[0]).arg(d->mcStepDone[1]).arg(d->mcStepDone[2]);

            bool allDone = true;

            bool anyCategory = false;

            for (int ci = 0; ci < 3; ci++) {

                if (d->mcStepTotal[ci] <= 0) {

                    allDone = false;

                    continue;

                }

                anyCategory = true;

                if (d->mcStepDone[ci] < d->mcStepTotal[ci])

                    allDone = false;

            }

            if (allDone && anyCategory) {

                int verifyIdx = 3;

                if (verifyIdx < d->steps.size()) {

                    QVariantMap vstep = d->steps[verifyIdx].toMap();

                    qCDebug(logVersion) << QStringLiteral("[verify-scan]  allDone=true step.show=%1").arg(vstep.value("show").toBool() ? "true" : "false");

                    if (!vstep.value("show").toBool()) {

                        showStep(it.key(), verifyIdx);

                        updateStep(it.key(), verifyIdx, QStringLiteral("active"), 0);

                    } else {

                        // Step already visible — no action needed
                        qCDebug(logVersion) << QStringLiteral("[verify-scan]  步骤已可见(show=true) 跳过激活");

                    }

                }

            } else {

                // Not yet all done — will re-check on next progress update
                qCDebug(logVersion) << QStringLiteral("[verify-scan]  未满足条件 allDone=%1 anyCategory=%2").arg(allDone).arg(anyCategory);

            }

            return;

        }

    }

    // Pure MC (non-merged): set flag + immediate card rebuild

    if (m_dlStates.contains(versionId)) {

        auto& st = m_dlStates[versionId];

        bool allDone = true;

        bool anyCategory = false;

        for (int ci = 0; ci < 3; ci++) {

            if (st.catBytesTotal[ci] <= 0) {

                allDone = false;

                continue;

            }

            anyCategory = true;

            if (st.catBytesDl[ci] < st.catBytesTotal[ci])

                allDone = false;

        }

        if (allDone && anyCategory && !st.catsFullyDone) {

            st.catsFullyDone = true;

            doRebuildInstallCards();

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





    for (auto it = m_downloadSessions.begin(); it != m_downloadSessions.end(); ++it) {

        auto& ses = it.value();
auto* ds = dlSession(it.key());

    }



    QVector<InstallCard> cards;

    QSet<QString> seen;



    // 1. Iterate sessions: build mod_loader cards from session state

    for (auto sit = m_downloadSessions.constBegin(); sit != m_downloadSessions.constEnd(); ++sit) {

        const QString& sid = sit.key();

        // 整合包运行期间：MC/加载器会话并入任务卡片子步骤，不生成独立卡片（杜绝双卡）
        if (isModpackSessionSuppressed(sid)) continue;

        auto* ds = dlSession(sid);

        bool mlPending = ds->hasPendingLoader && !ds->pendingLoaderName.isEmpty();

        bool mlActive = m_mlInstallers.contains(sid) && m_mlInstallers.value(sid)->isRunning();

        bool mlMerged = ds->isMerged();

        bool mlFailed = ds ? ds->isFailed() : false;

        if (!mlActive && !mlFailed && !mlPending && !mlMerged) continue;



        QString cardId = mlPending ? ds->pendingLoaderName : sid;

        InstallCard c;

        c.iid = cardId;

        c.type = QStringLiteral("mod_loader");

        c.name = cardId;

        c.progress = qBound(0.0, ds->totalProgress(), 1.0); // Show real progress even while MC downloads

        // Speed: aggregate MC + ML installer + Fabric API

        {

            qint64 s = 0;

            if (!mlPending) {

                // Check both session flag (legacy) and merged context for mc-download-done
                bool mcDoneOnSession = ds->mcDownloadDone;
                bool mcDoneOnCtx = false;
                if (auto* mctx = mergedContext(sid)) mcDoneOnCtx = mctx->mcDownloadDone;
                bool mcActive = m_dlStates.contains(ds->mcVersion) && !mcDoneOnSession && !mcDoneOnCtx;

                if (mcActive)  s += m_dlStates[ds->mcVersion].speed;

            }

            // Add ML installer download speed (while actively running)
            // ── 2026-08-15：用显式 loaderDownloadActive 标志 ──
            // isModLoaderInstalling() 覆盖不到驿道下载主文件阶段（verify-only
            // 模式 m_running=false）→ 主文件速度恒 0。显式标志在
            // downloadViaYidao 开始时置位、完成/失败复位，主文件/安装器库
            // 下载阶段都能计入速度。
            if (ds->loaderDownloadActive && ds->mlSpeed > 0)

                s += ds->mlSpeed;

            // Add Fabric API download speed (while pending)

            if (ds->fabricApiPending && ds->fabSpeed > 0)

                s += ds->fabSpeed;

            c.speed = s;

        }

        // Per-task phase from session's own step state (not shared m_installPhase)
        if (mlFailed) {
            c.phase = QStringLiteral("失败");
        } else if (mlPending) {
            c.phase = tr("等待原版 %1 下载完成").arg(ds->pendingLoaderMc);
        } else if (ds) {
            // Find the first active or pending step
            QString computedPhase;
            bool hasActive = false;
            for (int si = 0; si < ds->steps.size(); si++) {
                auto st = ds->steps[si].toMap();
                QString stStatus = st.value("status").toString();
                if (stStatus == QStringLiteral("active")) {
                    computedPhase = st.value("name").toString();
                    hasActive = true;
                    break;
                }
                if (stStatus == QStringLiteral("pending") && computedPhase.isEmpty()) {
                    computedPhase = st.value("name").toString();
                }
            }
            if (hasActive) {
                c.phase = computedPhase;
            } else {
                // Fall back to global phase (for non-step-based status like "校验中" / "连通性测试中")
                c.phase = m_installPhase;
            }
        } else {
            c.phase = m_installPhase;
        }

        c.remaining = mlPending ? 0 : installRemainingSteps(sid);

        c.steps = ds->steps; // ds->steps is populated by rebuildSteps() for merged installs

        c.failed = mlFailed;

        c.error = ds ? ds->errorMessage() : QString{};

        c.hasUserDataImport = ds->hasImportPending;

        c.canCancel = !(ds->hasImportPending && ds->steps.size() > 0 &&

            ds->steps.last().toMap().value("status").toString() == "active");

        c.importFailedAtMs = ds->importFailedAtMs;

        // Total progress bar: visible during download AND install (colors differ in QML)

        {

            bool hasActiveStep = false;

            if (!mlPending && !mlFailed) {

                for (const QVariant& vs : ds->steps) {

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

        if (ds->isMerged() && !ds->mcVersion.isEmpty()) seen.insert(ds->mcVersion);

    }



    // 2. Version cards — from DownloadSession for pure MC installs

    for (auto sit = m_downloadSessions.constBegin(); sit != m_downloadSessions.constEnd(); ++sit) {

        const QString& sid = sit.key();

        if (seen.contains(sid)) continue;

        // 整合包运行期间：纯原版会话卡片抑制（并入任务卡片子步骤）
        if (isModpackSessionSuppressed(sid)) { seen.insert(sid); continue; }

        auto* ds = dlSession(sid);

        if (!ds) continue;

        // Skip merged sessions (already built by Section 1)

        if (ds->isMerged()) continue;

        // Skip sessions with pending loader (shown as loader card in Section 1)

        if (ds->hasPendingLoader && !ds->pendingLoaderName.isEmpty()) continue;



        seen.insert(sid);



        // Build card from DownloadSession data

        InstallCard c = ds->toCard(sid, sid, QStringLiteral("version"));

        // Ensure consistent name/type

        c.name = sid;

        c.type = QStringLiteral("version");

        cards.append(c);

    }


    // 3. Resource cards (from m_extraCards)

    for (auto it = m_extraCards.begin(); it != m_extraCards.end(); ++it) {

        const QString& cardId = it.key();

        if (seen.contains(cardId)) continue;

        cards.append(resourceCardToInstallCard(cardId));

    }

    // 4. 整合包任务卡片（from m_taskCards，全量字段：steps/mods/logs/info）

    for (auto it = m_taskCards.constBegin(); it != m_taskCards.constEnd(); ++it) {

        if (seen.contains(it.key())) continue;

        cards.append(taskCardToInstallCard(it.key()));

        seen.insert(it.key());

    }



    LOG_CARDS() << "  total cards built:" << cards.size() << " (incremental)";

    // Incremental update: avoid full rebuild which causes QML flicker
    // 1. Remove cards no longer present
    for (int i = m_installCardsModel->count() - 1; i >= 0; --i) {
        QModelIndex mi = m_installCardsModel->index(i, 0);
        QVariant existingIid = m_installCardsModel->data(mi, InstallCardModel::IidRole);
        bool found = false;
        for (const auto& c : cards) {
            if (c.iid == existingIid.toString()) { found = true; break; }
        }
        if (!found) {
            m_installCardsModel->removeRow(i);
        }
    }
    // 2. Add or update cards
    for (const auto& c : cards) {
        int row = m_installCardsModel->findRowByIid(c.iid);
        if (row >= 0) {
            m_installCardsModel->updateRow(row, c);
        } else {
            m_installCardsModel->appendRow(c);
        }
    }
    // Cards rebuilt silently (incremental, no flicker)

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

                        qCInfo(logVersion) << QStringLiteral("预缓存版本JSON 版本=%1 大小=%2KB").arg(versionId).arg(data.size() / 1024);

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

    if (m_downloadSessions.contains(installId)) {

        ensureSession(installId);

        auto* ds = dlSession(installId);

        ds->hasImportPending = true;

        ds->importArchivePath = archivePath;



        // Append "导入用户数据" step to existing steps (only if not already there)

        bool alreadyHasImport = false;

        for (const auto& st : ds->steps) {

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

            ds->steps.append(importStep);

        }

    }

    qCInfo(logVersion) << QStringLiteral("用户数据导入已标记 版本=%1 归档=%2").arg(installId, archivePath);

    // ── 精准更新卡片 (导入步骤已追加到 ds->steps) ──
    updateCardFromSession(installId);

}



void VersionBackend::cancelPendingUserDataImport(const QString& installId)

{

    m_pendingImports.remove(installId);

    if (m_downloadSessions.contains(installId)) {

        ensureSession(installId);

        auto* ds = dlSession(installId);

        ds->hasImportPending = false;

        ds->importArchivePath.clear();

        ds->importFailedAtMs = 0;

        // Remove the last step if it's the import step

        if (!ds->steps.isEmpty()) {

            QVariantMap last = ds->steps.last().toMap();

            if (last.value("name").toString().contains("导入用户数据")) {

                ds->steps.removeLast();

            }

        }

    }

    qCInfo(logVersion) << QStringLiteral("用户数据导入已取消 版本=%1").arg(installId);

    // ── 精准更新卡片 (导入步骤已从 ds->steps 移除) ──
    updateCardFromSession(installId);

}



void VersionBackend::dismissCard(const QString& installId)

{

    // ── 资源卡片（模组/光影/资源包文件下载）：关闭即移除卡片并清注册 ──
    if (m_extraCards.contains(installId)) {
        removeResourceCard(installId);
        return;
    }

    // ── 整合包任务卡片：关闭即移除卡片并清注册（卡片已结束，无会话可清）──
    if (m_taskCards.contains(installId)) {
        removeTaskCard(installId);
        return;
    }

    m_downloadSessions.remove(installId);

    m_pendingImports.remove(installId);

    // Clean up DownloadSession if present

    if (auto* ds = m_downloadSessions.take(installId)) {

        ds->deleteLater();

    }

    // Remove from active ids if present

    m_activeIds.removeAll(installId);

    if (m_activeCount > 0) m_activeCount--;

    // ── 精准移除卡片而非全量重建 ──
    if (m_installCardsModel) {
        int row = m_installCardsModel->findRowByIid(installId);
        if (row >= 0)
            m_installCardsModel->removeRow(row);
    }

    // Clean up merged context if present
    destroyMergedContext(installId);

}

void VersionBackend::dismissAllCompleted()

{

    QStringList toDismiss;

    for (auto it = m_downloadSessions.begin(); it != m_downloadSessions.end(); ++it) {

        const auto& ses = it.value();

        auto* ds = dlSession(it.key());

        if (ds && (ds->isFailed() || (ds->totalProgress() >= 1.0 && !ds->hasPendingLoader))) {

            toDismiss.append(it.key());

        }

    }

    // 资源卡片：已失败或已完成（totalProgress>=1.0）的才清
    for (auto it = m_extraCards.constBegin(); it != m_extraCards.constEnd(); ++it) {
        const QVariantMap& cm = it.value();
        if (cm.value(QStringLiteral("failed"), false).toBool()
            || cm.value(QStringLiteral("totalProgress"), 0.0).toReal() >= 1.0)
            toDismiss.append(it.key());
    }
    // 整合包任务卡片：已失败或已完成（totalProgress>=1.0）的才清
    for (auto it = m_taskCards.constBegin(); it != m_taskCards.constEnd(); ++it) {
        const QVariantMap& cm = it.value();
        if (cm.value(QStringLiteral("failed"), false).toBool()
            || cm.value(QStringLiteral("totalProgress"), 0.0).toReal() >= 1.0)
            toDismiss.append(it.key());
    }

    for (const auto& id : toDismiss) {

        dismissCard(id);

    }

}





void VersionBackend::startUserDataImport(const QString& installId)

{

    if (!m_downloadSessions.contains(installId)) return;

    ensureSession(installId);

    auto* ds = dlSession(installId);



    qCInfo(logVersion) << QStringLiteral("开始导入用户数据 版本=%1").arg(installId);

    emit logMessage(tr("正在导入用户数据..."));



    // Show and activate the import step (step 6 for merged installs)

    int importStepIdx = ds->steps.size() - 1;

    updateStep(installId, importStepIdx, QStringLiteral("active"), 0);



    // Run import synchronously (it's just unzipping and copying)

    UserDataBackend importer;

    QString gameDir = m_gameDir;

    QString versionId = installId;



    // The import executes, then we emit finish/complete

    bool ok = false;

    QString error;

    {

        QZipReader zip(ds->importArchivePath);

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

        if (ds) ds->markFailed(tr("用户数据导入失败：%1\n版本已成功安装，但用户数据未能导入。\n可手动从压缩包恢复数据。卡片将在60秒后关闭。")

                        .arg(error));

        ds->importFailedAtMs = QDateTime::currentMSecsSinceEpoch();

        emit logMessage(tr("用户数据导入失败: %1").arg(error));

    }



    ds->hasImportPending = false;

    ds->importArchivePath.clear();



    // Emit final completion

    emit installComplete(installId);

    emit installFinished(ds ? !ds->isFailed() : true);

    setInstalling(false);  // always reset, even on failure

}

// ── Helper: recursive copy skipping existing files ──
// 每个文件复制后校验目标大小与源一致；不一致则删除目标残缺文件并返回 false（调用方应终止安装）。
static bool copyRecursiveMissing(const QString& srcDir, const QString& dstDir)
{
    QDirIterator it(srcDir, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    QDir sDir(srcDir);
    while (it.hasNext()) {
        it.next();
        const QString relPath = sDir.relativeFilePath(it.filePath());
        const QString dstPath = dstDir + QStringLiteral("/") + relPath;
        if (it.fileInfo().isDir()) {
            QDir().mkpath(dstPath);
        } else if (!QFileInfo::exists(dstPath)) {
            QDir().mkpath(QFileInfo(dstPath).absolutePath());
            const qint64 srcSize = it.fileInfo().size();
            if (!QFile::copy(it.filePath(), dstPath)) {
                qCWarning(logVersion) << QStringLiteral("[安装] 文件复制失败: %1").arg(it.filePath());
                return false;
            }
            const qint64 dstSize = QFileInfo(dstPath).size();
            if (srcSize != dstSize) {
                qCCritical(logVersion) << QStringLiteral("[安装] 文件复制校验失败: %1 源 %2 字节 ≠ 目标 %3 字节，删除残缺文件")
                    .arg(dstPath).arg(srcSize).arg(dstSize);
                QFile::remove(dstPath);
                return false;
            }
            // 大文件（JAR）打印实际字节数便于排查残缺小包问题
            if (srcSize >= 1024 * 1024)
                qCInfo(logVersion) << QStringLiteral("[安装] 已复制: %1 (%2 字节)").arg(dstPath).arg(dstSize);
        }
    }
    return true;
}



// ── MergedInstallContext lifecycle ──

MergedInstallContext* VersionBackend::createMergedContext(const QString& installId,
                                                          const QString& mcVersion,
                                                          const QString& loaderType,
                                                          const QString& loaderVersion)
{
    destroyMergedContext(installId);

    auto* ctx = new MergedInstallContext;
    ctx->installId = installId;
    ctx->mcVersion = mcVersion;
    ctx->loaderType = loaderType;
    ctx->loaderVersion = loaderVersion;

    // Create or share temp directory with another context using the same MC version
    QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    ctx->tempDir = QDir::tempPath() + QStringLiteral("/shadow-merged-") + uuid;
    for (auto cIt = m_mergedContexts.constBegin(); cIt != m_mergedContexts.constEnd(); ++cIt) {
        if (cIt.value() && cIt.value()->mcVersion == mcVersion && !cIt.value()->tempDir.isEmpty()) {
            ctx->tempDir = cIt.value()->tempDir;
            qDebug() << "[install] Shared temp dir" << ctx->tempDir << "for MC" << mcVersion;
            break;
        }
    }
    bool isNewTempDir = !QDir(ctx->tempDir).exists();
    if (isNewTempDir) {
        QDir().mkpath(ctx->tempDir);
        TempTracker::record(ctx->tempDir);
        qCInfo(logVersion) << QStringLiteral("[追踪] 新建临时目录: %1").arg(ctx->tempDir);
    }

    // Create ModLoaderInstaller (redirected to temp dir)
    ctx->installer = new ModLoaderInstaller(this);
    ctx->installer->setGameDir(ctx->tempDir);
    // 2026-08-15：加载器安装器库/依赖库下载默认镜像优先（不跟随全局 fileSource）
    ctx->installer->setPreferOfficial(false);

    // ── Signal connections for merged context installers ──

    // finished: bootstrapper done → copy results to game dir
    // The installer writes everything to tempDir; we copy version + libs + assets on success.
    // Cancel = just destroy tempDir, no orphaned files in game dir.
    connect(ctx->installer, &ModLoaderInstaller::finished, this,
        [this, installId, tempDir = ctx->tempDir](bool success, const QString& errMsg) {
            auto* ds = dlSession(installId);
            // 用户已取消（cancelVersionInstall 置 ctx->failed）：不复制产物，
            // 直接销毁 merged context + 清理版本残留（installer 同步安装已停止）
            if (auto* cctx = m_mergedContexts.value(installId, nullptr); cctx && cctx->failed) {
                qCInfo(logVersion) << QStringLiteral("[取消] merged installer finished after cancel, cleanup id=%1")
                    .arg(installId);
                destroyMergedContext(installId);
                if (!m_gameDir.isEmpty())
                    cleanupCanceledVersion(installId, m_gameDir);
                return;
            }
            if (success) {
                if (ds) {
                    for (int i = 0; i < ds->steps.size(); i++)
                        updateStep(installId, i, QStringLiteral("completed"), 100);
                    ds->m_rawTotalProgress = 1.0;
                    ds->smoothProgress = 1.0;
                }

                // ── Copy installer output from temp dir to real game dir ──
                // 1. Copy version folder (installName JSON + JAR)
                const QString srcVer = tempDir + QStringLiteral("/versions/") + installId;
                const QString dstVer = m_gameDir + QStringLiteral("/versions/") + installId;
                bool copyOk = true;
                qCInfo(logVersion) << QStringLiteral("[TRACE-f] copy: srcVer=%1 exists=%2 dstVer=%3")
                    .arg(srcVer).arg(QDir(srcVer).exists() ? 1 : 0).arg(dstVer);
                if (QDir(srcVer).exists()) {
                    QDir().mkpath(dstVer);
                    if (!copyRecursiveMissing(srcVer, dstVer)) {
                        copyOk = false;
                        qCWarning(logVersion) << QStringLiteral("[安装] 版本文件夹拷贝校验失败: %1").arg(srcVer);
                    } else {
                        qDebug() << "[install] Copied version folder:" << srcVer << "->" << dstVer;
                        qCInfo(logVersion) << QStringLiteral("[TRACE-f] copy done, dstVer files: %1")
                            .arg(QDir(dstVer).entryList(QDir::Files).join(QLatin1Char(',')));
                    }
                } else {
                    // ── OptiFine（QProcess 安装器模式）：实际版本文件夹名 ≠ installId ──
                    // （如 1.12.2-OptiFine_HD_U_G5）。扫 tempDir/versions/ 下除原版
                    // mcVersion 外的文件夹全部复制（合成模式写 installName 时 srcVer 已存在，不走这里）。
                    const QString mcVerForScan = ds ? ds->mcVersion : QString();
                    QDir tvDir(tempDir + QStringLiteral("/versions"));
                    const QStringList extraDirs = tvDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
                    bool anyCopied = false;
                    for (const QString& d : extraDirs) {
                        if (d == mcVerForScan) continue;
                        const QString s = tempDir + QStringLiteral("/versions/") + d;
                        const QString t = m_gameDir + QStringLiteral("/versions/") + d;
                        QDir().mkpath(t);
                        if (!copyRecursiveMissing(s, t)) {
                            copyOk = false;
                            qCWarning(logVersion) << QStringLiteral("[安装] OptiFine 版本文件夹拷贝校验失败: %1").arg(s);
                        } else {
                            anyCopied = true;
                            qCInfo(logVersion) << QStringLiteral("[安装] OptiFine 实际版本文件夹已复制: %1 -> %2").arg(s, t);
                        }
                    }
                    if (!anyCopied) {
                        qCWarning(logVersion) << QStringLiteral("[TRACE-f] srcVer 不存在，跳过版本文件夹拷贝: %1").arg(srcVer);
                    }
                }

                // 2. Copy libraries (skip existing)
                const QString srcLib = tempDir + QStringLiteral("/libraries");
                const QString dstLib = m_gameDir + QStringLiteral("/libraries");
                if (copyOk && QDir(srcLib).exists()) {
                    QDir().mkpath(dstLib);
                    if (!copyRecursiveMissing(srcLib, dstLib)) {
                        copyOk = false;
                        qCWarning(logVersion) << QStringLiteral("[安装] 库文件拷贝校验失败: %1").arg(srcLib);
                    } else {
                        qDebug() << "[install] Copied libraries (missing only):" << srcLib;
                    }
                }

                // 3. Copy assets (skip existing)
                const QString srcAssets = tempDir + QStringLiteral("/assets");
                const QString dstAssets = m_gameDir + QStringLiteral("/assets");
                if (copyOk && QDir(srcAssets).exists()) {
                    QDir().mkpath(dstAssets);
                    if (!copyRecursiveMissing(srcAssets, dstAssets)) {
                        copyOk = false;
                        qCWarning(logVersion) << QStringLiteral("[安装] 资源文件拷贝校验失败: %1").arg(srcAssets);
                    } else {
                        qDebug() << "[install] Copied assets (missing only):" << srcAssets;
                    }
                }

                if (!copyOk) {
                    // 残缺拷贝：删除目标版本文件夹并抛出安装失败
                    emit logMessage(tr("[失败] 安装文件拷贝校验失败，已清理残缺产物"));
                    if (ds) {
                        for (int i = 0; i < ds->steps.size(); i++)
                            updateStep(installId, i, QStringLiteral("failed"), 0);
                        ds->markFailed(tr("安装文件拷贝校验失败（文件不完整）"));
                    }
                    if (!m_gameDir.isEmpty())
                        cleanupCanceledVersion(installId, m_gameDir);
                    destroyMergedContext(installId);
                    setInstalling(false);
                    startNextFromQueue();
                    emit installFinished(false);
                    return;
                }

                // ── 清理原版 MC 版本文件夹（无其他任务使用同一 MC 时）──
                // MC 下载在独立临时目录完成；安装成功后把真实游戏目录中残留的原版版本文件夹
                // （含下载进度标记遗留的空壳目录）整体删除，恢复改造前"装完即删原版"的行为。
                if (ds && !ds->mcVersion.isEmpty()) {
                    bool otherUsingSameMC = false;
                    for (auto cIt = m_mergedContexts.constBegin(); cIt != m_mergedContexts.constEnd(); ++cIt) {
                        if (cIt.key() != installId && cIt.value() && cIt.value()->mcVersion == ds->mcVersion) {
                            otherUsingSameMC = true; break;
                        }
                    }
                    if (!otherUsingSameMC) {
                        qCInfo(logVersion) << QStringLiteral("[安装] 清理原版 MC 版本文件夹: %1").arg(ds->mcVersion);
                        cleanupCanceledVersion(ds->mcVersion, m_gameDir);
                        refreshInstalled();
                    }
                }

                emit logMessage(tr("安装完成"));
                setInstallPhase(tr("完成"));
                refreshInstalled();
                refreshInstalled();
                // ── Fabric API 未下载完：标记封装完成但暂不收尾，等 API 完成后统一 finishInstall ──
                // （原逻辑 API 完成回调里 !bootstrapperDone 直接 finishInstall → 封装没跑 → 版本残缺；
                //   这里抢先 finishInstall → API 文件没下载完 → 丢失）
                if (ds && ds->fabricApiPending) {
                    auto* ctx2 = m_mergedContexts.value(installId, nullptr);
                    if (ctx2) ctx2->bootstrapperDone = true;
                    qCInfo(logVersion) << QStringLiteral("[TRACE-f] 封装+copy 完成 ctx2=%1 等待 Fabric API 后收尾")
                        .arg(ctx2 ? QStringLiteral("yes") : QStringLiteral("NULL"));
                    // 等 API：不 finishInstall、不复位安装状态、不发 installFinished（避免 QML 提前收尾）
                    return;
                } else {
                    finishInstall(installId);
                }
            } else {
                if (ds) {
                    for (int i = 0; i < ds->steps.size(); i++)
                        updateStep(installId, i, QStringLiteral("failed"), 0);
                    ds->markFailed(errMsg.isEmpty() ? tr("模组加载器安装失败") : errMsg);
                }
                emit logMessage(tr("[失败] 模组加载器安装失败: %1").arg(errMsg));
            }
            setInstalling(false);
            startNextFromQueue();
            emit installFinished(success);
        });

    // ── waitingForMC：Fabric 库下载完（parallelMode）→ 标记 loader 就绪 ──
    // Fabric 路径关键：merged 的 ctx->installer 必须接上 waitingForMC，否则
    // ctx->loaderJarReady 永远 false → MC 下载完成后 onVersionDownloadFinished 不调
    // proceedToLoaderInstall → fabricFinalize/step3 封装从不执行 → 版本文件夹残缺。
    connect(ctx->installer, &ModLoaderInstaller::waitingForMC, this,
        [this, installId]() {
            auto* ds = dlSession(installId);
            auto* ctx = m_mergedContexts.value(installId, nullptr);
            qCInfo(logVersion) << QStringLiteral("[TRACE-f] waitingForMC(merged) id=%1 dsFound=%2 mcDone=%3")
                .arg(installId).arg(ds ? 1 : 0).arg(ctx ? (ctx->mcDownloadDone ? 1 : 0) : -1);
            if (ds && ds->isMerged()) {
                ds->loaderDownloadReady = true;
                if (ctx)
                    ctx->loaderJarReady = true;
                // ⚠ MC 完成标志必须读 ctx->mcDownloadDone（onVersionDownloadFinished 的
                // merged 汇合处只置 ctx 标志，ds->mcDownloadDone 从未被置位）：
                // MC 先完成（Timeline B）时若读 ds 标志恒为 false → 永不 proceed → 导入永不完成。
                if (ctx && ctx->mcDownloadDone)
                    proceedToLoaderInstall(installId);
            }
        });

    // stepProgress: update step percentage (skip if already completed)
    connect(ctx->installer, &ModLoaderInstaller::stepProgress, this,
        [this, installId](int step, int percentage) {
            auto* ds = dlSession(installId);
            if (!ds) return;
            int stepIdx = ds->isMerged() ? (step - 1 + 4) : (step - 1);
            // Don't overwrite already-completed steps (e.g. forgeStep1_verify maps to download step 4)
            if (stepIdx >= 0 && stepIdx < ds->steps.size()) {
                auto curStep = ds->steps[stepIdx].toMap();
                if (curStep.value(QStringLiteral("status")).toString() == QStringLiteral("completed"))
                    return;
            }
            updateStep(installId, stepIdx, (percentage >= 100) ? QStringLiteral("completed") : QStringLiteral("active"), percentage);
            if (!ds->isMerged()) {
                int totalSteps = qMax(ds->steps.size(), 1);
                qreal raw = (qMin(stepIdx, totalSteps - 1) + percentage / 100.0) / totalSteps;
                ds->m_rawTotalProgress = raw;
                ds->smoothProgress = ds->smoothProgress <= 0.0 ? raw * 0.7 : ds->smoothProgress * 0.3 + raw * 0.7;
            }
        });

    // progressChanged: update step display (compute idx from installer step)
    connect(ctx->installer, &ModLoaderInstaller::progressChanged, this,
        [this, installId](int step, int /*totalSteps*/, const QString& desc) {
            setInstallPhase(tr("模组加载器: ") + desc);
            auto* ds = dlSession(installId);
            if (!ds) return;
            // ── 2026-08-15 重写：merged 步骤映射全用精确语义查找 ──
            // Forge/NeoForge 的 progressChanged step 语义：1=下载主文件、2=校验、3=安装。
            // 步骤数组：主文件(4) → 安装器库(5) → 校验(6) → 安装(7)。
            // ⚠ findStepByName("完整性") 会误命中"校验游戏资源完整性"(idx=3)——
            // 它包含"完整性"三字且排在 idx=6 之前！verify 信号若驱动 idx=3，
            // MC 资源校验被提前标 active/completed，而 loader 校验永不更新
            // （实测"安装都开始了校验还在冒三点"）。必须用排除"资源"的精确查找。
            // ⚠ findStepByName("安装") 会误命中"下载 X 安装器库"（也含"安装"）。
            int stepIdx = -1;
            if (ds->isMerged()) {
                if (step <= 1)      stepIdx = findStepByName(installId, QStringLiteral("主文件"));
                else if (step == 2) stepIdx = findLoaderVerifyStep(installId);
                else if (step >= 3) stepIdx = findLoaderInstallStep(installId);
                if (stepIdx < 0) stepIdx = (step - 1 + 4);  // fallback 旧逻辑
            } else {
                stepIdx = step - 1;
            }
            if (stepIdx >= 0 && stepIdx < ds->steps.size()) {
                // Only show as active if not already completed
                auto curStep = ds->steps[stepIdx].toMap();
                if (curStep.value(QStringLiteral("status")).toString() != QStringLiteral("completed")) {
                    if (!curStep.value(QStringLiteral("show")).toBool())
                        showStep(installId, stepIdx);
                    updateStep(installId, stepIdx, QStringLiteral("active"), 0);
                    ds->loaderStepIdx = stepIdx;
                }
            }
        });

    // byteProgress: download speed display
    connect(ctx->installer, &ModLoaderInstaller::byteProgress, this,
        [this, installId](const QString& /*file*/, qint64 received, qint64 total, qint64 speed) {
            auto* ds = dlSession(installId);
            if (!ds) return;
            ds->mlBytesDl = received;
            ds->mlBytesAll = total;
            ds->mlSpeed = speed;
            // ── 2026-08-15：byteProgress 更新 mlSpeed 后必须排队卡片刷新 ──
            // 速度聚合（updateCardFromSession）只在 throttle 触发时执行，而
            // throttle 此前只由 updateStep（installerLibsFileProgress 文件级）
            // 排队启动。安装器库并发小文件下载时文件级进度低频，mlSpeed
            // 更新后卡片速度刷新滞后 → UI 显示"下载中但速度 0"。
            // 这里直接排队，保证每次速度更新都触发 200ms throttle 刷新。
            if (!m_pendingCardUpdates.contains(installId))
                m_pendingCardUpdates.append(installId);
            if (!m_cardUpdateThrottle.isActive())
                m_cardUpdateThrottle.start();
        });

    // ── verifyStarted / verifyFinished（2026-08-15 重写：精确驱动 loader 校验步骤）──
    // ⚠ 2026-08-14 版本用 findStepByName("完整性")——模糊子串匹配误命中
    // "校验游戏资源完整性"(idx=3)，把 MC 资源校验提前标 active/completed，
    // 而 loader 校验步骤(idx=6)从未被驱动 → 一直 active 冒三点，直到安装完成
    // 被兜底循环标绿（实测 23:11:56.589 idx=6/idx=7 同一毫秒 completed）。
    // 现在用 findLoaderVerifyStep()：只匹配"校验 X 完整性"（排除含"资源"的步骤）。
    connect(ctx->installer, &ModLoaderInstaller::verifyStarted, this,
        [this, installId]() {
            auto* ds = dlSession(installId);
            if (!ds) return;
            int vi = findLoaderVerifyStep(installId);
            if (vi >= 0) {
                showStep(installId, vi);
                updateStep(installId, vi, QStringLiteral("active"), 0);
            }
        });
    connect(ctx->installer, &ModLoaderInstaller::verifyFinished, this,
        [this, installId](bool ok) {
            auto* ds = dlSession(installId);
            if (!ds) return;
            int vi = findLoaderVerifyStep(installId);
            if (vi >= 0)
                updateStep(installId, vi, ok ? QStringLiteral("completed")
                                             : QStringLiteral("failed"), ok ? 100 : 0);
        });

    // 安装器库下载步骤联动（forge/neoforge：与 MC 下载并行）
    connect(ctx->installer, &ModLoaderInstaller::installerLibsStarted, this,
        [this, installId]() {
            auto* ds = dlSession(installId);
            if (!ds) return;
            ds->loaderDownloadActive = true;   // 2026-08-15：安装器库下载计入速度
            for (int i = 0; i < ds->steps.size(); ++i) {
                if (ds->steps[i].toMap().value(QStringLiteral("name")).toString()
                        .contains(QStringLiteral("安装器库"))) {
                    ds->loaderStepIdx = i;   // byteProgress 落到安装器库步骤（不再污染主文件步骤）
                    updateStep(installId, i, QStringLiteral("active"), 0);
                    break;
                }
            }
        });
    connect(ctx->installer, &ModLoaderInstaller::installerLibsFailed, this,
        [this, installId](const QString& err) {
            auto* ds = dlSession(installId);
            if (!ds) return;
            // ── 2026-08-15：安装器库下载失败 → 步骤标 failed（不再"失败也标绿"）──
            // 原实现 installerLibsDone 无条件标 completed，srgutils 所有源失败也
            // 显示 100% 完成（日志铁证 23:08:29.872）。失败应如实显示。
            qCWarning(logVersion) << QStringLiteral("[安装] 安装器库下载失败，安装器库步骤标 failed: %1").arg(err);
            for (int i = 0; i < ds->steps.size(); ++i) {
                if (ds->steps[i].toMap().value(QStringLiteral("name")).toString()
                        .contains(QStringLiteral("安装器库"))) {
                    updateStep(installId, i, QStringLiteral("failed"), 0);
                    break;
                }
            }
        });
    connect(ctx->installer, &ModLoaderInstaller::installerLibsDone, this,
        [this, installId]() {
            auto* ds = dlSession(installId);
            if (!ds) return;
            ds->loaderDownloadActive = false;   // 2026-08-15：安装器库下载结束
            for (int i = 0; i < ds->steps.size(); ++i) {
                if (ds->steps[i].toMap().value(QStringLiteral("name")).toString()
                        .contains(QStringLiteral("安装器库"))) {
                    // ── 2026-08-15：若 installerLibsFailed 已把本步骤标 failed，
                    // 不得再用 completed 覆盖（否则"失败也标绿"）──
                    QString curStatus = ds->steps[i].toMap()
                                            .value(QStringLiteral("status")).toString();
                    if (curStatus != QStringLiteral("failed"))
                        updateStep(installId, i, QStringLiteral("completed"), 100);
                    // ── 2026-08-14 修复：完成后清空 detail ──
                    // 最后一条 installerLibsFileProgress 可能在 done 接近 total 时
                    // 把 detail 设为"剩余 N"，installerLibsDone 标 completed 后若不
                    // 清空，UI 会显示"100% + 剩余 N"自相矛盾。completed 步骤的
                    // detail 应为空。
                    if (auto* node = ds->pipeline()->stepNode(i))
                        node->setDetail(QString());
                    // ── 2026-08-15 重写：loaderStepIdx 复位到安装步骤 ──
                    // 安装器库完成后的 byteProgress（安装阶段）不应再写安装器库步骤，
                    // 否则"安装 NeoForge"进行中安装器库仍显示 active。
                    // ⚠ 2026-08-14 用 nm.startsWith("安装") 匹配——"下载 X 安装器库"
                    // 不以"安装"开头（"下载"在前），但为统一改用精确查找，杜绝歧义。
                    int installIdx = findLoaderInstallStep(installId);
                    if (installIdx >= 0)
                        ds->loaderStepIdx = installIdx;
                    break;
                }
            }
        });
    connect(ctx->installer, &ModLoaderInstaller::installerLibsFileProgress, this,
        [this, installId](int done, int total) {
            auto* ds = dlSession(installId);
            if (!ds) return;
            const int remain = qMax(0, total - done);
            for (int i = 0; i < ds->steps.size(); ++i) {
                if (ds->steps[i].toMap().value(QStringLiteral("name")).toString()
                        .contains(QStringLiteral("安装器库"))) {
                    // ── 2026-08-14 修复：改走 updateStep（统一钳制 + 状态机保护）──
                    // 原实现直接 node->setDetail/setPercentage，绕过 updateStep 的
                    // percentage 钳制与 completed 状态保护，导致：
                    //   · done 超 total 时百分比破 100%（主文件 120% 同类问题）
                    //   · 安装器库已完成（100%）后被迟到的 installerLibsFileProgress
                    //     把 detail 覆盖成"剩余 X 个文件"（自相矛盾显示）
                    // 这里通过 updateStep 更新百分比（钳制 0-100；已完成步骤不被
                    // active 降级），detail 用 StepNode 的 setDetail——但仅在步骤
                    // 未完成时允许覆盖，已完成步骤保留空 detail。
                    int pct = (total > 0) ? qBound(0, done * 100 / total, 100) : 0;
                    // 更新百分比（updateStep 内部会钳制 + 状态保护）
                    updateStep(installId, i, QStringLiteral("active"), pct, 0, 0);
                    // detail：仅未完成时更新（已完成步骤不再显示"剩余 X"）
                    auto* node = ds->pipeline()->stepNode(i);
                    if (node) {
                        QString curStatus = ds->steps[i].toMap()
                                                .value(QStringLiteral("status")).toString();
                        bool terminal = (curStatus == QStringLiteral("completed")
                                      || curStatus == QStringLiteral("failed")
                                      || curStatus == QStringLiteral("skipped"));
                        if (!terminal) {
                            node->setDetail(remain > 0
                                ? QStringLiteral("剩余 %1 个文件").arg(remain)
                                : QString());
                        }
                    }
                    break;
                }
            }
        });

    // logMessage: forward
    connect(ctx->installer, &ModLoaderInstaller::logMessage, this, &VersionBackend::logMessage);

    // toastMessage: forward (e.g. auto Java download during loader install)
    connect(ctx->installer, &ModLoaderInstaller::toastMessage, this, &VersionBackend::toastMessage);

    m_mergedContexts[installId] = ctx;
    return ctx;
}

void VersionBackend::destroyMergedContext(const QString& installId)
{
    auto* ctx = m_mergedContexts.take(installId);
    if (!ctx) return;

    // Only delete temp dir if no other context uses it (shared when same mcVersion)
    bool lastUser = true;
    for (auto cIt = m_mergedContexts.constBegin(); cIt != m_mergedContexts.constEnd(); ++cIt) {
        if (cIt.value() && cIt.value()->tempDir == ctx->tempDir) {
            lastUser = false; break;
        }
    }
    // 安装 worker/bootstrapper/同步安装（installLegacy2/1 主线程跑）仍在写 tempDir
    // （解压/复制/java 安装器）时不能删目录，否则访问冲突崩溃（0xc0000005）
    const bool workerBusy = ctx->installer
        && (ctx->installer->installWorkerBusy() || ctx->installer->bootstrapperRunning()
            || ctx->installer->syncInstallRunning());
    if (!ctx->tempDir.isEmpty() && lastUser && !workerBusy) {
        QDir d(ctx->tempDir);
        if (d.exists()) d.removeRecursively();
        TempTracker::forget(ctx->tempDir);
        qCInfo(logVersion) << QStringLiteral("[追踪] 清理临时目录: %1").arg(ctx->tempDir);
    } else if (workerBusy) {
        qCWarning(logVersion) << QStringLiteral("[追踪] 取消时安装 worker 仍在运行，跳过临时目录删除: %1")
            .arg(ctx->tempDir);
    }

    // Delete owned children
    if (ctx->mcDownloader) {
        ctx->mcDownloader->disconnect();
        ctx->mcDownloader->deleteLater();
    }
    if (ctx->installer) {
        ctx->installer->disconnect();
        ctx->installer->deleteLater();
    }

    delete ctx;

    // If this was the last merged context, update installing state
    if (m_mergedContexts.isEmpty()) {
        bool wasInstalling = true;
        bool isNowInstalling = m_installing || (m_activeCount > 0);
        if (wasInstalling != isNowInstalling) {
            emit installStateChanged();
        }
    }
}

} // namespace ShadowLauncher
