// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
// Shadow Launcher — VersionDownloader
// Minecraft version installation pipeline: client.jar + libraries + assets.
// Uses FileDownloader (批量库文件/版本文件) + AssetDownloader (assets 专项) for concurrent
// multi-file downloads and HttpClient for single-file asset index retrieval.

#include "version_downloader.h"
#include "engine_identity.h"
#include "file_downloader.h"
#include "asset_downloader.h"
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
#include <QUrl>
#include <QNetworkRequest>
#include <QtConcurrent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
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
        // ── 两个下载器聚合（修复 db 跳变）──
        // 旧代码只写夸父自身的值 → 与山海经发射点（写两者之和）交替覆盖 →
        // 上层 netDb 差分被污染：缓存命中爆发时（山海经几百 MB 瞬间累加）+
        // 紧邻夸父 100ms tick → 瞬时速度飙到 G/s。两个发射点都写"当前两者之和"
        // → db 单调递增，差分稳定。
        const int assetCompleted = m_assetDownloader ? m_assetDownloader->completedFiles() : 0;
        const int assetTotal     = m_assetDownloader ? m_assetDownloader->totalFiles() : 0;
        const qint64 assetBytes  = m_assetDownloader ? m_assetDownloader->downloadedBytes() : 0;
        const qint64 assetTotB   = m_assetDownloader ? m_assetDownloader->totalBytes() : 0;
        const int mergedCompleted = completed + assetCompleted;
        const int mergedTotal     = total + assetTotal;
        const qint64 mergedBytes  = dlBytes + assetBytes;
        const qint64 mergedTotB   = totBytes + assetTotB;
        m_completedFiles.storeRelaxed(mergedCompleted);
        m_totalFiles.storeRelaxed(mergedTotal);
        m_downloadedBytes.storeRelaxed(mergedBytes);
        m_totalBytes.storeRelaxed(mergedTotB);
        emit progressChanged(mergedCompleted, mergedTotal, mergedBytes, mergedTotB);
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

    // ── Asset-dedicated downloader v2 (2× HTTP/1.1 QNAMs, adaptive concurrency) ──
    m_assetDownloader = new AssetDownloader(this);
    m_assetDownloader->setMaxConcurrent(m_maxWorkers);
    if (m_downloadCfg.speedLimitMB > 0)
        m_assetDownloader->setSpeedLimitMB(m_downloadCfg.speedLimitMB);

    connect(m_assetDownloader, &AssetDownloader::progressChanged,
            this, [this](int completed, int total, qint64 dlBytes, qint64 totBytes) {
        int libTotal = m_downloader ? m_downloader->totalFiles() : 0;
        int libCompleted = m_downloader ? m_downloader->completedFiles() : 0;
        int mergedCompleted = libCompleted + completed;
        int mergedTotal = libTotal + total;
        qint64 libBytes = m_downloader ? m_downloader->downloadedBytes() : 0;
        qint64 libTotalBytes = m_downloader ? m_downloader->totalBytes() : 0;
        m_completedFiles.storeRelaxed(mergedCompleted);
        m_totalFiles.storeRelaxed(mergedTotal);
        m_downloadedBytes.storeRelaxed(libBytes + dlBytes);
        m_totalBytes.storeRelaxed(libTotalBytes + totBytes);
        emit progressChanged(mergedCompleted, mergedTotal, libBytes + dlBytes, libTotalBytes + totBytes);
    });

    connect(m_assetDownloader, &AssetDownloader::logMessage,
            this, &VersionDownloader::logMessage);

    // Forward per-file progress so VersionBackend::updateDownloadFile can
    // track per-category done bytes (cat = fileCategory(savePath)) and speed.
    connect(m_assetDownloader, &AssetDownloader::fileProgress,
            this, &VersionDownloader::fileProgress);

    connect(m_assetDownloader, &AssetDownloader::allFinished,
            this, [this](bool success, int failedCount, const QStringList& failedFiles) {
        m_assetTasksDone = true;
        if (failedCount > 0) {
            emit logMessage(QStringLiteral("[盘古] %1 个文件下载失败").arg(failedCount));
            emit downloadFailedFiles(failedFiles);
        }
        checkBothDownloadersDone();
    });
}

VersionDownloader::~VersionDownloader()
{
    // FileDownloader / AssetDownloader 均为 QObject child; auto-deleted
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
    // FileDownloader needs the working dir to compute cache fallback paths
    if (m_downloader) m_downloader->setMinecraftDir(dir);
}

void VersionDownloader::setCacheFallbackDir(const QString& dir)
{
    m_cacheFallbackDir = dir;
    if (m_downloader) m_downloader->setCacheFallbackDir(dir);
    if (m_assetDownloader) m_assetDownloader->setFallbackCacheDir(dir);
}

void VersionDownloader::setDownloadConfig(const DownloadConfig& config)
{
    m_downloadCfg = config;
    m_maxWorkers = config.maxWorkers;
    if (m_downloader) {
        m_downloader->setMaxThreads(m_maxWorkers);
        m_downloader->setSpeedLimitMB(config.speedLimitMB);
    }
    if (m_assetDownloader) {
        m_assetDownloader->setMaxConcurrent(m_maxWorkers);
        if (config.speedLimitMB > 0)
            m_assetDownloader->setSpeedLimitMB(config.speedLimitMB);
    }
}

// ═══════════════════════════════════════════════════════════
// Main pipeline: downloadVersion
// ═══════════════════════════════════════════════════════════

void VersionDownloader::downloadVersion(const QJsonObject& versionJson,
                                         const QString& versionId)
{
    if (m_state == Running || m_state == Paused) {
        emit logMessage(QStringLiteral("[盘古] [警告] 已有下载任务进行中"));
        return;
    }

    m_state = Running;
    m_currentVersionId = versionId;
    m_currentVersionJson = versionJson;
    m_assetObjects.clear();
    m_taskDestPaths.clear();
    m_fileCategory.clear();

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
    qCInfo(logDownload) << engineBanner("pangu");
    emit logMessage(engineBanner("pangu"));
    emit logMessage(QStringLiteral("[盘古] 开始下载版本 %1").arg(versionId));

    // --- Step 1: Save version JSON to disk ---
    const QString versionDir = m_minecraftDir + QStringLiteral("/versions/") + versionId;
    QDir().mkpath(versionDir);

    const QString jsonPath = versionDir + QStringLiteral("/") + versionId + QStringLiteral(".json");
    QJsonDocument doc(versionJson);
    QFile jsonFile(jsonPath);
    if (jsonFile.open(QIODevice::WriteOnly)) {
        jsonFile.write(doc.toJson(QJsonDocument::Indented));
        jsonFile.close();
        emit logMessage(QStringLiteral("[盘古] 版本 JSON 下载完成 %1").arg(jsonPath));
    } else {
        emit logMessage(QStringLiteral("[盘古] [警告] 无法保存版本清单: %1").arg(jsonPath));
    }

    // --- Step 2-4: assets index 双源竞速（与版本 JSON 同款）---
    // 并行化：libs/jar 不依赖 index → 阶段 A 立即启动夸父；
    // index 完成后阶段 B 启动山海经补 assets（旧结构是 index 下完才统一启动两个引擎）。
    QJsonObject assetIdx = versionJson.value(QStringLiteral("assetIndex")).toObject();

    // ── 阶段 A：libs/jar 立即启动（assetObjects 空 → collectTasks 只产出 libs/jar）──
    // 2026-08-04：缓存预检（读盘 SHA1）移后台线程——几百个库文件全量读盘 hash
    // 会在主线程卡 UI（点击开始下载的卡顿主因）。collectTasks 仍主线程（纯 JSON
    // 遍历，快）；addFile 排队 + start 在主线程，但 addFile 不再重复读盘。
    {
        m_assetObjects.clear();
        QVector<DownloadTask> libTasks;
        collectTasks(versionJson, versionId, m_assetObjects, libTasks);
        m_totalFiles.storeRelaxed(libTasks.size());
        qint64 totalEstimate = 0;
        for (const auto& t : libTasks) totalEstimate += t.totalBytes;
        m_totalBytes.storeRelaxed(totalEstimate);

        const bool preferOfficial = (m_downloadCfg.fileSource == DownloadSourcePolicy::PreferOfficial ||
                                     m_downloadCfg.fileSource == DownloadSourcePolicy::AutoSwitch);

        // ── 后台预检缓存：读盘 SHA1 在 worker 线程，命中者 notifyCacheHit 计入完成 ──
        // 结果经 invokeMethod 回主线程，避免跨线程调 QObject/emit
        struct LibPrecheck {
            QString savePath;
            QString name;
            QStringList sources;
            qint64 totalBytes;
            QByteArray sha1;
        };
        QVector<LibPrecheck> prechecks;
        for (const auto& t : libTasks) {
            const int cat = m_fileCategory.value(t.savePath, -1);
            if (cat == 2) continue;   // 防御：空 assetObjects 不应有 assets 任务
            LibPrecheck p;
            p.savePath = t.savePath;
            p.name = t.name;
            p.totalBytes = t.totalBytes;
            p.sha1 = t.sha1.toUtf8();
            if (preferOfficial) {
                // 严格分组：官方源全部在前（组内按 t.url/mirrors 顺序），
                // 镜像源全部在后——下载器按“首选组内哈希分流”需要组连续。
                // t.url 若为官方并入官方组，否则并入镜像组。
                bool mojangAdded = false;
                for (const auto& m : t.mirrors) {
                    if (!mojangAdded && (m.contains("mojang.com") || m.contains("minecraft.net"))) {
                        p.sources.append(m); mojangAdded = true;
                    }
                }
                if (t.url.contains("mojang.com") || t.url.contains("minecraft.net")) {
                    if (!mojangAdded) {
                        p.sources.append(t.url);
                    } else if (!p.sources.contains(t.url)) {
                        p.sources.append(t.url);
                    }
                }
                for (const auto& m : t.mirrors) {
                    if (!m.contains("mojang.com") && !m.contains("minecraft.net"))
                        p.sources.append(m);
                }
                if (!t.url.contains("mojang.com") && !t.url.contains("minecraft.net"))
                    p.sources.append(t.url);
            } else {
                // PreferMirror：严格分组——镜像源全部在前（含 t.url 若镜像），
                // 官方源全部在后（组连续，供下载器“首选组内哈希分流”）
                const bool urlIsOfficial = t.url.contains("mojang.com") || t.url.contains("minecraft.net");
                if (!urlIsOfficial)
                    p.sources.append(t.url);
                for (const auto& m : t.mirrors) {
                    if (!m.contains("mojang.com") && !m.contains("minecraft.net")
                        && !p.sources.contains(m))
                        p.sources.append(m);
                }
                if (urlIsOfficial && !p.sources.contains(t.url))
                    p.sources.append(t.url);
                for (const auto& m : t.mirrors) {
                    if ((m.contains("mojang.com") || m.contains("minecraft.net"))
                        && !p.sources.contains(m))
                        p.sources.append(m);
                }
            }
            prechecks.append(p);
        }
        const bool hasLibTasks = !prechecks.isEmpty();

        QtConcurrent::run([this, prechecks, hasLibTasks]() {
            // worker 线程：逐个读盘 SHA1 判定缓存命中（不碰任何 QObject 成员）
            QVector<LibPrecheck> toDownload;
            QVector<QPair<QString, qint64>> cacheHits;   // (path, size)
            // 大小降序：大文件先下（前期吃满带宽），小文件后批量收尾
            // —— 避免 JSON 顺序导致的小文件集中在后期、每文件 TLS 建连开销
            //    使后期速度暴跌（用户实测：下载支持库后期 <1MB/s）
            QVector<LibPrecheck> sorted = prechecks;
            std::sort(sorted.begin(), sorted.end(),
                      [](const LibPrecheck& a, const LibPrecheck& b) {
                          return a.totalBytes > b.totalBytes;
                      });
            for (const auto& p : sorted) {
                bool hit = false;
                if (!p.sha1.isEmpty()) {
                    QFileInfo fi(p.savePath);
                    if (fi.exists() && fi.size() > 0) {
                        QFile f(p.savePath);
                        if (f.open(QIODevice::ReadOnly)) {
                            QCryptographicHash hash(QCryptographicHash::Sha1);
                            hash.addData(&f);
                            f.close();
                            if (hash.result() == p.sha1) {
                                hit = true;
                                cacheHits.append({p.savePath, fi.size()});
                            }
                        }
                    }
                }
                if (!hit)
                    toDownload.append(p);
            }

            QMetaObject::invokeMethod(this, [this, toDownload, cacheHits, hasLibTasks]() {
                // 命中者计入完成（不读盘不排队）
                for (const auto& ch : cacheHits)
                    m_downloader->notifyCacheHit(ch.first, ch.second);

                // 未命中者分流：大文件（>1MB）→ 夸父（分片加速）；
                // 小文件（≤1MB）→ 山海经（独立并发引擎，双引擎并行）。
                // 修复：56 个小文件瞬间占满夸父并发 → 大文件单连接饿死
                // （fastutil 22MB 龟速 23 秒）；小文件交给山海经后夸父专心分片。
                QVector<AssetDownloader::AssetTask> smallTasks;
                QVector<LibPrecheck> bigTasks;
                for (const auto& p : toDownload) {
                    if (p.totalBytes > 0 && p.totalBytes <= 1LL * 1024 * 1024) {
                        AssetDownloader::AssetTask at;
                        at.savePath = p.savePath;
                        at.sha1 = QString::fromUtf8(p.sha1);
                        at.mirrors = p.sources;
                        at.size = p.totalBytes;
                        smallTasks.append(at);
                    } else {
                        bigTasks.append(p);
                    }
                }

                // 大文件 → 夸父
                bool anyBig = false;
                for (const auto& p : bigTasks) {
                    m_downloader->addFile(p.savePath, p.name, p.sources,
                                          p.totalBytes, p.sha1, false, true);
                    anyBig = true;
                }

                // 小文件 → 山海经（立即启动，不等 assets index）
                bool anySmall = !smallTasks.isEmpty();
                if (anySmall) {
                    emit logMessage(QStringLiteral("[盘古] 小库文件交山海经引擎 (%1 个)").arg(smallTasks.size()));
                    if (m_assetDownloader->isRunning()) {
                        // 阶段 B 可能已先启动（index 快于预检）→ 追加
                        m_assetDownloader->appendTasks(smallTasks);
                    } else {
                        m_assetDownloader->startDownload(smallTasks, m_maxWorkers);
                    }
                }

                const bool anyQueued = anyBig || anySmall;
                m_libTasksDone = !anyBig;   // 夸父侧 libs 完成（山海经侧由 allFinished 计数）
                if (anyBig) {
                    emit logMessage(QStringLiteral("[盘古] 开始下载库文件 (%1 个)").arg(bigTasks.size()));
                    m_downloader->start();
                } else if (hasLibTasks && !anySmall) {
                    // 全部缓存命中且无小文件：start() 空任务立即 allFinished
                    m_downloader->start();
                }
            });
        });
    }

    // ── 阶段 B：assets index 双源竞速下载 → 完成后启动山海经（assets）──
    auto startAssets = [this, versionJson, versionId, assetIdx]() {
        if (m_state == Cancelled) return;   // 取消后竞速完成不再启动下载（防 cancel 失效）
        // 不重置 m_categoryTotalBytes[1]（libs 总量）——阶段 A 已统计，
        // 阶段 B 的 collectTasks 只有 assets 任务会把它清 0 → 支持库进度
        // 分母丢失（用户实测：支持库字节 0/0KB，进度失真）
        if (!assetIdx.isEmpty()) {
            m_assetObjects = parseAssetIndex(assetIdx);
        }
        QVector<DownloadTask> tasks;
        collectTasks(versionJson, versionId, m_assetObjects, tasks);
        if (tasks.isEmpty()) {
            // assets 无任务：山海经若还在跑阶段 A 的小库文件，等它 allFinished；
            // 没在跑则直接完成（m_assetTasksDone 由 allFinished 统一置位）
            if (!m_assetDownloader->isRunning()) {
                m_assetTasksDone = true;
                checkBothDownloadersDone();
            }
            return;
        }

        const bool preferOfficial = (m_downloadCfg.fileSource == DownloadSourcePolicy::PreferOfficial ||
                                     m_downloadCfg.fileSource == DownloadSourcePolicy::AutoSwitch);
        QVector<AssetDownloader::AssetTask> assetTasks;
        bool hasAssetTasks = false;
        qint64 assetsBytes = 0;
        for (const auto& t : tasks) {
            const int cat = m_fileCategory.value(t.savePath, -1);
            if (cat != 2) continue;   // 只取 assets（libs 已在阶段 A 启动）
            hasAssetTasks = true;
            assetsBytes += t.totalBytes;
            QStringList mirrors;
            if (preferOfficial) {
                // 严格分组：官方源全部在前，镜像源全部在后
                //（下载器按“首选组内哈希分流”需要组连续）
                bool mojangAdded = false;
                for (const auto& m : t.mirrors) {
                    if (!mojangAdded && (m.contains("mojang.com") || m.contains("minecraft.net"))) {
                        mirrors.append(m); mojangAdded = true;
                    }
                }
                if (t.url.contains("mojang.com") || t.url.contains("minecraft.net")) {
                    if (!mojangAdded)
                        mirrors.append(t.url);
                    else if (!mirrors.contains(t.url))
                        mirrors.append(t.url);
                }
                for (const auto& m : t.mirrors) {
                    if (!m.contains("mojang.com") && !m.contains("minecraft.net"))
                        mirrors.append(m);
                }
                if (!t.url.contains("mojang.com") && !t.url.contains("minecraft.net"))
                    mirrors.append(t.url);
            } else {
                // PreferMirror：严格分组——镜像源全部在前（含 t.url 若镜像），官方源全部在后
                const bool urlIsOfficial = t.url.contains("mojang.com") || t.url.contains("minecraft.net");
                if (!urlIsOfficial)
                    mirrors.append(t.url);
                for (const auto& m : t.mirrors) {
                    if (!m.contains("mojang.com") && !m.contains("minecraft.net")
                        && !mirrors.contains(m))
                        mirrors.append(m);
                }
                if (urlIsOfficial && !mirrors.contains(t.url))
                    mirrors.append(t.url);
                for (const auto& m : t.mirrors) {
                    if ((m.contains("mojang.com") || m.contains("minecraft.net"))
                        && !mirrors.contains(m))
                        mirrors.append(m);
                }
            }
            AssetDownloader::AssetTask at;
            at.savePath = t.savePath;
            at.sha1 = t.sha1;
            at.mirrors = mirrors;
            at.size = t.totalBytes;
            assetTasks.append(at);
        }
        m_totalFiles.fetchAndAddRelaxed(assetTasks.size());
        m_totalBytes.fetchAndAddRelaxed(assetsBytes);
        m_assetTasksDone = !hasAssetTasks && !m_assetDownloader->isRunning();
        if (hasAssetTasks) {
            emit logMessage(QStringLiteral("[盘古] 开始下载资源文件 (%1 个)").arg(assetTasks.size()));
            if (m_assetDownloader->isRunning()) {
                // 山海经正在下小库文件（阶段 A 分流）→ 追加 assets 任务并行
                m_assetDownloader->appendTasks(assetTasks);
            } else {
                m_assetDownloader->startDownload(assetTasks, m_maxWorkers);
            }
        }
        checkBothDownloadersDone();
    };

    if (assetIdx.isEmpty()) { startAssets(); return; }

    const QString idxUrl = assetIdx.value(QStringLiteral("url")).toString();
    const QString idxId = assetIdx.value(QStringLiteral("id")).toString(QStringLiteral("legacy"));
    const QString idxPath = m_minecraftDir + QStringLiteral("/assets/indexes/") + idxId + QStringLiteral(".json");

    if (idxUrl.isEmpty() || QFileInfo::exists(idxPath)) { startAssets(); return; }

    emit logMessage(QStringLiteral("[盘古] 正在下载资源索引（双源竞速）..."));
    QDir().mkpath(QFileInfo(idxPath).absolutePath());
    downloadAssetIndexRace(idxUrl, idxPath,
        [this, startAssets]() {
            QMetaObject::invokeMethod(this, startAssets, Qt::QueuedConnection);
        });
}

// ═══════════════════════════════════════════════════════════
// assets index 双源竞速（镜像 BMCLAPI + 官方 piston-meta 同时发，先成功者赢）
// ═══════════════════════════════════════════════════════════

void VersionDownloader::downloadAssetIndexRace(const QString& idxUrl, const QString& idxPath,
                                               std::function<void()> done)
{
    QString official = idxUrl;
    QString mirror = idxUrl;
    mirror.replace(QStringLiteral("piston-meta.mojang.com"),
                   QStringLiteral("bmclapi2.bangbang93.com"));

    auto won = std::make_shared<bool>(false);
    auto pending = std::make_shared<int>(mirror == official ? 1 : 2);

    auto launch = [this, won, pending, done, idxPath](const QString& url, const QString& label) {
        auto* nam = new QNetworkAccessManager(this);
        const QUrl reqUrl(url);
        QNetworkRequest req(reqUrl);
        req.setRawHeader("User-Agent", "ShadowLauncher/1.0");
        req.setTransferTimeout(kRaceTimeoutMs);
        QNetworkReply* reply = nam->get(req);

        connect(reply, &QNetworkReply::finished, this,
                [this, won, pending, done, idxPath, url, label, reply, nam]() {
            reply->deleteLater();
            nam->deleteLater();
            if (*won) return;   // 另一源已胜出（旧轮慢请求不干扰）

            const bool ok = (reply->error() == QNetworkReply::NoError)
                && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200;
            if (ok) {
                const QByteArray data = reply->readAll();
                if (!data.isEmpty()) {
                    *won = true;
                    QDir().mkpath(QFileInfo(idxPath).absolutePath());
                    QFile f(idxPath);
                    if (f.open(QIODevice::WriteOnly)) {
                        f.write(data);
                        f.close();
                        emit logMessage(QStringLiteral("[盘古] 资源索引竞速胜出 源=%1").arg(label));
                        done();
                        return;
                    }
                }
            } else {
                qCWarning(logDownload) << QStringLiteral("[盘古] 资源索引源失败 %1: %2")
                    .arg(label, reply->errorString());
            }
            if (--(*pending) <= 0) {
                *won = true;
                emit logMessage(QStringLiteral("[盘古] 资源索引下载失败（双源均失败），按无资源继续"));
                done();   // 继续流程（assets 任务为空）
            }
        });
    };

    if (mirror != official) launch(mirror, QStringLiteral("镜像"));
    launch(official, QStringLiteral("官方"));
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
    if (m_state == Idle || m_state == Done || m_state == Failed) return;
    m_state = Cancelled;
    if (m_downloader) m_downloader->cancel();
    if (m_assetDownloader) m_assetDownloader->cancel();
    if (m_verifyWorkerObj) {
        m_verifyWorkerObj->cancel();
        if (m_verifyThread) {
            m_verifyThread->quit();
            m_verifyThread->wait(3000);
        }
    }
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

qint64 VersionDownloader::cachedBytes() const
{
    qint64 bytes = 0;
    if (m_assetDownloader)
        bytes += m_assetDownloader->cachedBytes();
    if (m_downloader)
        bytes += m_downloader->cachedBytes();
    // 修复：此前漏加 FileDownloader(库/jar) 的缓存字节 → 上层 netDb=db-cache 虚高
    // → 缓存命中文件被计入网络速度，造成卡片速度虚高残留
    return bytes;
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
// Cross-downloader completion: both FileDownloader (libraries)
// and AssetDownloader (assets, HTTP/2) must finish before
// proceeding to integrity verification.
// ═══════════════════════════════════════════════════════════

void VersionDownloader::checkBothDownloadersDone()
{

    if (!m_libTasksDone || !m_assetTasksDone)
        return;
    if (m_state == Cancelled) {
        emit downloadFinished(false, tr("下载已取消"));
        return;
    }

    // Both downloaders complete — continue to the original
    // onAllFinishedV2 logic (verification, mirror fallback, etc.)
    int libFailed = m_downloader ? m_downloader->failedFiles() : 0;
    int failedCount = libFailed;

    QStringList failedPaths;
    if (failedCount > 0) {
        emit logMessage(QStringLiteral("[盘古] [警告] 库文件下载: %1 个文件下载失败").arg(failedCount));
    }

    // Mirror fallback: calculate fail rate against library tasks only.
    // Asset failures are handled individually by AssetDownloader's per-file retry.
    int libTotalFiles = m_downloader ? m_downloader->totalFiles() : 0;
    const double failRate = libTotalFiles > 0
        ? static_cast<double>(failedCount) / libTotalFiles : 0.0;

    if (failedCount > 0
        && m_fallbackIndex + 1 < m_fallbackChain.size()
        && failRate >= kFallbackThreshold) {
        const auto& next = m_fallbackChain[m_fallbackIndex + 1];
        emit logMessage(QStringLiteral("[盘古] [重试] %1%% 文件下载失败, 切换到 %2 重试...")
                            .arg(static_cast<int>(failRate * 100))
                            .arg(next.name));
        retryWithNextMirror();
        return;
    }

    // --- Integrity verification ---
    m_state = Verifying;
    m_downloadFailedCount = failedCount;
    emit stateChanged();
    emit logMessage(QStringLiteral("[盘古] 正在进行完整性校验..."));

    QVector<VerifyItem> items = collectVerifyItems(m_currentVersionJson, m_currentVersionId);
    startAsyncVerify(items);
}

// ═══════════════════════════════════════════════════════════
// allFinished (FileDownloader v8) → library tasks done
// ═══════════════════════════════════════════════════════════

void VersionDownloader::onAllFinishedV2()
{
    // FileDownloader finished — mark lib tasks done and check both
    m_libTasksDone = true;
    checkBothDownloadersDone();
}

// ═══════════════════════════════════════════════════════════
// allFinished → verify → emit downloadFinished
// ═══════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════
// Asset index download (single-file, blocking via QEventLoop)
// ═══════════════════════════════════════════════════════════

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

        // ── Primary URL ──
        // BMCLAPI etc: use /version/ endpoint (works via mirror jarHost)
        // Mojang 官方: launcher.mojang.com is DEAD (2023 retired), use origUrl (piston-data) instead
        const QString primaryUrl = (m_mirror.name == tr("Mojang 官方"))
            ? origUrl
            : jarUrl;

        DownloadTask jarTask;
        jarTask.name      = versionId + QStringLiteral(".jar");
        jarTask.url       = primaryUrl;
        jarTask.savePath  = versionDir + QStringLiteral("/") + versionId + QStringLiteral(".jar");
        jarTask.sha1      = client.value(QStringLiteral("sha1")).toString();
        jarTask.totalBytes = static_cast<qint64>(client.value(QStringLiteral("size")).toDouble());

        // Build fallback mirrors: BMCLAPI /version/ first, Mojang direct as last resort
        QStringList jarMirrors;
        // For non-Mojang mirrors: primary is BMCLAPI /version/, add dead launcher.mojang as 3rd+ resort only
        // For Mojang mirror: primary is piston-data, skip jarUrl (launcher.mojang is dead)
        if (m_mirror.name != tr("Mojang 官方")) {
            jarMirrors << jarUrl;
        }
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
    // 依赖库分析统计已在前面输出
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
        int cat = -1;
        if (t.savePath.contains(QStringLiteral("/versions/")) || t.savePath.contains(QStringLiteral("/libraries/"))
            || t.savePath.contains(QStringLiteral("/maven/")))
            cat = 1;
        else if (t.savePath.contains(QStringLiteral("/assets/")))
            cat = 2;
        if (cat >= 0) {
            m_categoryTotalBytes[cat] += t.totalBytes;
            m_fileCategory[t.savePath] = cat;
        }
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

    // Create fresh FileDownloader (old one is in Done/Failed state, can't reuse)
    if (m_downloader) {
        m_downloader->deleteLater();
        m_downloader = nullptr;
    }
    m_downloader = new ShadowDownloader::FileDownloader(this);
    m_downloader->setMaxThreads(m_maxWorkers);
    m_downloader->setSpeedLimitMB(m_downloadCfg.speedLimitMB);
    m_downloader->setMinecraftDir(m_minecraftDir);
    m_downloader->setCacheFallbackDir(m_cacheFallbackDir);
    connect(m_downloader, &ShadowDownloader::FileDownloader::progressChanged,
            this, [this](int completed, int total, qint64 rx, qint64 totalBytes) {
        int assetCompleted = m_assetDownloader ? m_assetDownloader->completedFiles() : 0;
        int assetTotal = m_assetDownloader ? m_assetDownloader->totalFiles() : 0;
        qint64 assetBytes = m_assetDownloader ? m_assetDownloader->downloadedBytes() : 0;
        qint64 assetTotalBytes = m_assetDownloader ? m_assetDownloader->totalBytes() : 0;
        m_completedFiles.storeRelaxed(completed + assetCompleted);
        m_totalFiles.storeRelaxed(total + assetTotal);
        m_downloadedBytes.storeRelaxed(rx + assetBytes);
        m_totalBytes.storeRelaxed(totalBytes + assetTotalBytes);
        emit progressChanged(completed + assetCompleted, total + assetTotal,
                             rx + assetBytes, totalBytes + assetTotalBytes);
    });
    connect(m_downloader, &ShadowDownloader::FileDownloader::logMessage,
            this, &VersionDownloader::logMessage);
    connect(m_downloader, &ShadowDownloader::FileDownloader::allFinished,
            this, &VersionDownloader::onAllFinishedV2,
            Qt::QueuedConnection);

    // Re-collect tasks with the new mirror
    QVector<DownloadTask> tasks;
    collectTasks(m_currentVersionJson, m_currentVersionId, m_assetObjects, tasks);

    // Reset tracking — only libraries/jar go back to FileDownloader
    m_libTasksDone = false;

    // Filter to only library/jar tasks (category 1) for FileDownloader retry
    QVector<DownloadTask> libTasks;
    bool hasLibTasks = false;
    for (const auto& t : tasks) {
        int cat = m_fileCategory.value(t.savePath, -1);
        if (cat != 2) {  // not asset → library/jar
            libTasks.append(t);
            hasLibTasks = true;
        }
    }

    m_totalFiles.storeRelaxed(libTasks.size() + (m_assetDownloader ? m_assetDownloader->totalFiles() : 0));

    if (!hasLibTasks) {
        m_state = Running;
        checkBothDownloadersDone();
        return;
    }

    qint64 totalEstimate = 0;
    for (const auto& t : libTasks) totalEstimate += t.totalBytes;

    m_state = Running;
    emit stateChanged();
    emit logMessage(QStringLiteral("[盘古] 开始下载版本 %1 | 镜像=%2").arg(m_currentVersionId, next.name));
    emit logMessage(QStringLiteral("[盘古] 已切换到 %1 (%2/%3)，重新下载 %4 个库文件")
                        .arg(next.name)
                        .arg(m_fallbackIndex + 1)
                        .arg(m_fallbackChain.size())
                        .arg(libTasks.size()));

    // Feed library/jar tasks to FileDownloader
    for (const auto& t : libTasks) {
        QStringList sources;
        sources.append(t.url);
        for (const auto& m : t.mirrors) sources.append(m);
        m_downloader->addFile(t.savePath, t.name, sources,
                              t.totalBytes, t.sha1.toUtf8(), false);
    }
    m_downloader->start();

    // AssetDownloader is already running (or completed) — no need to restart
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
        qCInfo(logVersion) << QStringLiteral("[盘古] 收集到 %1 个文件待校验").arg(totalItems);
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
        emit logMessage(QStringLiteral("[盘古] [警告] 完整性检查: %1 个文件缺失").arg(missedLabels.size()));
        for (int i = 0; i < qMin(missedLabels.size(), 10); ++i)
            emit logMessage(QStringLiteral("[盘古]   缺失: %1").arg(missedLabels[i]));
        if (missedLabels.size() > 10)
            emit logMessage(QStringLiteral("[盘古]   ... 共 %1 个").arg(missedLabels.size()));

        emit downloadFailedFiles(missedLabels);

        // ── Mirror fallback: missing files → retry with next mirror ──
        if (m_fallbackIndex + 1 < m_fallbackChain.size()) {
            const auto& next = m_fallbackChain[m_fallbackIndex + 1];
            emit logMessage(QStringLiteral("[盘古] [重试] 完整性校验未通过 (%1 缺失), 切换到 %2 重试...")
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
        emit logMessage(QStringLiteral("[盘古] [警告] %1 个文件下载失败，但已通过完整性校验").arg(m_downloadFailedCount));
    }

    m_state = Done;
    emit stateChanged();
    emit logMessage(QStringLiteral("[盘古] 版本下载完成 %1").arg(m_currentVersionId));
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
 
