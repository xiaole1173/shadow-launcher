// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
//
// Asset-dedicated download engine v2 (performance-tuned 2026-07-29).
//
// Performance fixes:
//   - Async SHA1 pre-check (IO pool instead of blocking main thread)
//   - Async DNS resolution (QHostInfo::lookupHost instead of fromName)
//   - Fixed cache-hit speed spike corrupting speed floor
//   - Reduced default concurrency (burst 12, max 32, per-host 8)
//   - Added detailed logging for diagnostics
//
// Design:
//   1. Init: async DNS pre-resolution, brief size-based cache check
//   2. Burst: fire ~12 requests immediately
//   3. Accelerate: every 50ms, check throughput. If speed < floor → add 4 more.
//      Speed floor starts at 256KB/s, rises to 85% of weighted peak.
//   4. Steady: when speed >= floor, stop adding. Replace finished requests.
//   5. Cooldown: if speed drops sharply, reduce inflight to avoid congestion.

#include "asset_downloader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QUrl>
#include <QLoggingCategory>
#include <QRunnable>
#include <QNetworkRequest>
#include <QHttp1Configuration>
#include <QDateTime>
#include <QPointer>
#include <QHostInfo>

Q_LOGGING_CATEGORY(logAsset, "ShadowDownloader.Asset")

namespace {
    QString fmtSize(qint64 bytes) {
        if (bytes < 1024)       return QStringLiteral("%1 B").arg(bytes);
        if (bytes < 1024*1024)  return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
        return QStringLiteral("%1 MB").arg(bytes / (1024.0*1024.0), 0, 'f', 1);
    }
} // anonymous

// ═════════════════════════════════════════════════════════════════════════════
// Construction / Destruction
// ═════════════════════════════════════════════════════════════════════════════

AssetDownloader::AssetDownloader(QObject* parent)
    : QObject(parent)
{
    // Acceleration timer — fires frequently during the accelerate phase
    m_accelTimer = new QTimer(this);
    m_accelTimer->setSingleShot(false);
    m_accelTimer->setInterval(kAccelIntervalMs);
    connect(m_accelTimer, &QTimer::timeout, this, &AssetDownloader::accelTick);

    // I/O thread pool: 4 workers for SHA1 + disk writes
    m_ioPool.setMaxThreadCount(kMaxIOWorkers);

    setupNam();
}

AssetDownloader::~AssetDownloader()
{
    cancel();
}

// ═════════════════════════════════════════════════════════════════════════════
// Network manager setup
// ═════════════════════════════════════════════════════════════════════════════

void AssetDownloader::setupNam()
{
    m_accelTimer->stop();
    m_burstSent = 0;
    m_targetInflight = 0;
    m_phase = PhaseInit;

    // Two QNAMs for HTTP/1.1 — each with 255 connections per host max.
    // Per-request we set Http2AllowedAttribute=false to prevent ALPN
    // from negotiating HTTP/2, which causes RST_STREAM issues on BMCLAPI.
    for (int i = 0; i < 2; ++i) {
        if (m_nam[i]) {
            m_nam[i]->disconnect();
            m_nam[i]->deleteLater();
            m_nam[i] = nullptr;
        }
        m_nam[i] = new QNetworkAccessManager(this);
    }

    qCInfo(logAsset) << QStringLiteral("[资源] 2× QNAM 就绪");
}

// ═════════════════════════════════════════════════════════════════════════════
// Entry point
// ═════════════════════════════════════════════════════════════════════════════

void AssetDownloader::startDownload(const QVector<AssetTask>& tasks, int maxConcurrent)
{
    if (m_state == Running) {
        qCWarning(logAsset) << QStringLiteral("[资源] 已在运行，忽略重复的 startDownload()");
        return;
    }

    m_maxConcurrent = qBound(4, maxConcurrent, 256);
    m_targetInflight = 0;
    m_phase = PhaseInit;
    m_burstSent = 0;
    m_cacheHitCount = 0;
    m_preCheckQueued = 0;
    m_cacheBytes.storeRelaxed(0);
    m_dnsPendingCount = 0;
    m_dnsAllResolved = false;
    m_pendingPreCheck.clear();
    m_dnsToResolve.clear();
    m_hostStats.clear();  // Clear stale degradation from previous runs

    m_pendingQueue.clear();
    m_inFlight.clear();
    m_failedFiles.clear();
    m_failedCount = 0;
    m_completedFiles.storeRelaxed(0);
    m_downloadedBytes.storeRelaxed(0);
    m_totalTaskCount = tasks.size();

    qint64 totalEst = 0;
    for (const auto& t : tasks)
        totalEst += t.size;
    m_totalBytes.storeRelaxed(totalEst);
    m_totalFiles.storeRelaxed(tasks.size());

    for (const auto& t : tasks)
        m_pendingQueue.enqueue(t);

    m_state = Running;
    m_lastProgressEmit.start();
    m_downloadTimer.start();
    m_speedLogTimer.start();

    // Log the first few task's mirror sources to verify which source is being used
    if (!tasks.isEmpty()) {
        const auto& first = tasks.first();
        QString sampleUrls;
        for (int i = 0; i < qMin(first.mirrors.size(), 4); ++i) {
            if (i > 0) sampleUrls += QStringLiteral(" | ");
            sampleUrls += first.mirrors[i];
        }
        emit logMessage(QStringLiteral("[资源] 开始下载资源文件 共 %1 个 (%2)")
                            .arg(tasks.size()).arg(fmtSize(totalEst)));
        emit logMessage(QStringLiteral("[资源]   镜像源示例: %1").arg(sampleUrls));
    } else {
        emit logMessage(QStringLiteral("[资源] 开始下载资源文件 共 0 个"));
    }
    emit progressChanged(0, tasks.size(), 0, totalEst);

    // ════════════════════════════════════════════════════════════════
    // Phase Init: async DNS + pre-check already-cached files
    // ════════════════════════════════════════════════════════════════
    // Collect unique hosts for async DNS resolution
    QSet<QString> hosts;
    for (const auto& t : tasks) {
        for (const auto& m : t.mirrors)
            hosts.insert(extractHost(m));
    }
    // Store them for async resolution
    for (const auto& h : hosts) {
        DnsPending dp;
        dp.host = h;
        dp.refCount = 0;
        m_dnsToResolve.insert(h, dp);
    }
    m_dnsPendingCount = m_dnsToResolve.size();

    if (!m_dnsToResolve.isEmpty()) {
        emit logMessage(QStringLiteral("[资源] 正在解析 %1 个域名...").arg(m_dnsToResolve.size()));
        startAsyncDns();
    } else {
        m_dnsAllResolved = true;
    }

    // Pre-scan for cache hits: quick file-exists + size check (no SHA1 I/O).
    // Dispatch async SHA1 pre-check for files that pass the fast check.
    int fastCacheHits = 0;
    QQueue<AssetTask> downloadQueue;
    while (!m_pendingQueue.isEmpty()) {
        AssetTask task = m_pendingQueue.dequeue();
        if (!task.sha1.isEmpty()) {
            // Check 1: working dir (tempDir)
            QFileInfo fi(task.savePath);
            if (fi.exists() && fi.size() > 0) {
                if (task.size <= 0 || fi.size() == task.size) {
                    enqueuePreCheck(task, task.savePath);
                    fastCacheHits++;
                    continue;
                }
            }
            // Check 2: fallback cache dir (gameDir) — avoid re-downloading
            if (!m_fallbackCacheDir.isEmpty()) {
                const QString fallbackPath = m_fallbackCacheDir + QStringLiteral("/assets/objects/")
                    + task.sha1.left(2) + QStringLiteral("/") + task.sha1;
                QFileInfo ffi(fallbackPath);
                if (ffi.exists() && ffi.size() > 0) {
                    if (task.size <= 0 || ffi.size() == task.size) {
                        enqueuePreCheck(task, fallbackPath);
                        fastCacheHits++;
                        continue;
                    }
                }
            }
        }
        // Needs download
        downloadQueue.enqueue(task);
    }
    m_pendingQueue = downloadQueue;

    if (fastCacheHits > 0) {
        emit logMessage(QStringLiteral("[资源] 预检查: %1 个文件进入异步 SHA1 校验").arg(fastCacheHits));
    }

    // ── Phase 1: Burst — fire kBurstSize immediately ──
    // (Only for files that definitely need downloading, pre-checks run in parallel)
    m_phase = PhaseBurst;
    int burst = qMin(kBurstSize, m_pendingQueue.size());
    burst = qMin(burst, m_maxConcurrent);
    if (burst > 0) {
        emit logMessage(QStringLiteral("[资源] 突发请求: 发送 %1 个请求 (待下载=%2, 预检查=%3)")
                            .arg(burst).arg(m_pendingQueue.size()).arg(m_pendingPreCheck.size()));
        for (int i = 0; i < burst && !m_pendingQueue.isEmpty(); ++i) {
            fireNext();
            m_burstSent++;
        }
    }

    // Initialize speed baseline AFTER counting pre-check cache hits
    resetSpeedBaseline();

    if (m_inFlight.size() < m_maxConcurrent && (!m_pendingQueue.isEmpty() || !m_pendingPreCheck.isEmpty()))
        m_accelTimer->start();

    logState("startDownload complete");
}

// ═════════════════════════════════════════════════════════════════════════════
// Cancel
// ═════════════════════════════════════════════════════════════════════════════

void AssetDownloader::cancel()
{
    if (m_state != Running) return;
    m_state = Cancelled;
    m_accelTimer->stop();

    QList<QNetworkReply*> toAbort;
    for (auto it = m_inFlight.cbegin(); it != m_inFlight.cend(); ++it)
        toAbort.append(it.key());
    m_inFlight.clear();
    m_pendingQueue.clear();
    m_pendingPreCheck.clear();

    for (auto* reply : toAbort) {
        reply->abort();
        reply->deleteLater();
    }

    m_ioPool.clear();

    emit logMessage(QStringLiteral("[资源] 下载已取消"));
    logState("cancelled");
    emit allFinished(false, m_failedCount, m_failedFiles);
}

// ═════════════════════════════════════════════════════════════════════════════
// Fire next pending request
// ═════════════════════════════════════════════════════════════════════════════

void AssetDownloader::fireNext()
{
    if (m_state != Running || m_pendingQueue.isEmpty()) return;

    // Scan the queue to find a file whose host can accept.
    // Previously we only checked the first file — if it was blocked
    // by per-host limits, it would be prepended and block ALL files
    // behind it, causing the pipeline to stall at ~90% completion.
    // Now we move blocked files to the back of the queue and try
    // the next file, ensuring dispatch doesn't stall.
    AssetTask dispatchTask;
    int selectedMirror = -1;
    int totalTried = 0;
    while (totalTried < m_pendingQueue.size()) {
        dispatchTask = m_pendingQueue.dequeue();
        totalTried++;

        if (dispatchTask.mirrors.isEmpty()) {
            QTimer::singleShot(0, this, [this, dispatchTask]() {
                finishDownload(dispatchTask, false);
            });
            continue;
        }

        // Pick source: skip degraded hosts, respect per-host limits
        selectedMirror = -1;
        for (int i = 0; i < dispatchTask.mirrors.size(); ++i) {
            QString host = extractHost(dispatchTask.mirrors[i]);
            if (hostCanAccept(host)) {
                selectedMirror = i;
                break;
            }
        }
        if (selectedMirror < 0) {
            // All mirrors at capacity — move to back and try next file
            m_pendingQueue.enqueue(dispatchTask);
            continue;
        }
        break;  // found a dispatchable file
    }

    if (selectedMirror < 0) {
        // All files tried, none dispatchable — let accelTick retry later
        if (!m_accelTimer->isActive())
            m_accelTimer->start();
        return;
    }

    // ── Dispatch the found file ──
    QDir().mkpath(QFileInfo(dispatchTask.savePath).absolutePath());
    const QString& url = dispatchTask.mirrors[selectedMirror];
    // ── Source selection log (throttled: every 5s or on fallback) ──
    {
        static int logCounter = 0;
        if (++logCounter % 100 == 1 || selectedMirror > 0) {
            QString host = extractHost(url);
            QString srcLabel = selectedMirror == 0
                ? QStringLiteral("primary")
                : QStringLiteral("fallback[%1]").arg(selectedMirror);
            qCInfo(logAsset) << QStringLiteral("  [资源] 源=%1 主机=%2 SHA1=%3...").arg(srcLabel, host, dispatchTask.sha1.left(12));
        }
    }
    QUrl qurl(url);
    QNetworkRequest req(qurl);
    req.setRawHeader("User-Agent", "ShadowLauncher/1.0");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    // Disable HTTP/2 — BMCLAPI and Mojang CDN send RST_STREAM under
    // concurrent HTTP/2 streams. HTTP/1.1 with multiple connections is
    // more reliable for asset downloads.
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    // Per-request HTTP/1.1 config: 255 connections per host per QNAM
    {
        QHttp1Configuration h1cfg;
        h1cfg.setNumberOfConnectionsPerHost(128);
        req.setHttp1Configuration(h1cfg);
    }
    // Per-source adaptive timeout:
    //   min(avg_first_byte × 3, 30s)
    // This avoids long waits on dead hosts while allowing slow-but-alive ones.
    {
        QString host = extractHost(url);
        QMutexLocker lock(&m_hostMutex);
        auto it = m_hostStats.find(host);
        qint64 timeoutMs = (it != m_hostStats.end())
            ? qMin(it->avgFirstByteMs * 3, 30000LL)
            : 15000;
        req.setTransferTimeout(static_cast<int>(timeoutMs));
        it->activeRequests++;
    }

    // Round-robin across 2 QNAMs
    int namIdx = m_namRoundRobin;
    m_namRoundRobin = (m_namRoundRobin + 1) & 1;

    QNetworkReply* reply = m_nam[namIdx]->get(req);

    InFlight ift;
    ift.task = dispatchTask;
    ift.mirrorIndex = selectedMirror;
    ift.namIndex = namIdx;
    ift.startMs = QDateTime::currentMSecsSinceEpoch();
    m_inFlight.insert(reply, ift);

    // 增量跟踪下载字节，避免在 onReplyFinished 中整批累加导致测速尖峰
    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, reply](qint64 received, qint64) {
        auto it = m_inFlight.find(reply);
        if (it == m_inFlight.end()) return;
        qint64 delta = received - it->progressBytes;
        if (delta > 0) {
            m_downloadedBytes.fetchAndAddRelaxed(delta);
            it->progressBytes = received;
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onReplyFinished(reply);
    });
}

// ═════════════════════════════════════════════════════════════════════════════
// Reply handler
// ═════════════════════════════════════════════════════════════════════════════

void AssetDownloader::onReplyFinished(QNetworkReply* reply)
{
    // Defer deletion so we can read reply data first
    reply->deleteLater();

    auto it = m_inFlight.find(reply);
    if (it == m_inFlight.end()) return;

    InFlight ift = it.value();
    m_inFlight.erase(it);

    // Decrement active-request count for the host
    {
        QString host = extractHost(ift.task.mirrors.value(ift.mirrorIndex));
        QMutexLocker lock(&m_hostMutex);
        auto hit = m_hostStats.find(host);
        if (hit != m_hostStats.end())
            hit->activeRequests--;
    }

    if (m_state == Cancelled) {
        checkAllFinished();
        return;
    }

    const AssetTask& task = ift.task;
    qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - ift.startMs;

    if (reply->error() != QNetworkReply::NoError) {
        QString host = extractHost(ift.task.mirrors.value(ift.mirrorIndex));
        recordHostResult(host, false, elapsed);

        int nextIdx = ift.mirrorIndex + 1;
        if (nextIdx < task.mirrors.size()) {
            // Retry with next mirror
            AssetTask retryTask = task;
            retryTask.mirrors = task.mirrors.mid(nextIdx);
            m_pendingQueue.prepend(retryTask);
            fireNext();
        } else {
            qCWarning(logAsset) << QStringLiteral("  [资源] 下载失败 %1 所有镜像已耗尽 (%2)").arg(task.sha1, reply->errorString());
            finishDownload(task, false);
        }
        return;
    }

    QByteArray data = reply->readAll();

    // DNS IP reliability reward
    {
        QString url = ift.task.mirrors.value(ift.mirrorIndex);
        QString host = extractHost(url);
        // We connected successfully — reward the IP we used
        QMutexLocker lock(&m_dnsMutex);
        auto ipIt = m_dnsCache.find(host);
        if (ipIt != m_dnsCache.end() && !ipIt->addresses.isEmpty()) {
            // The QNAM chose an IP; we approximate by rewarding all resolved IPs
            // equally since Qt doesn't expose which IP was actually connected.
            for (const QString& ip : ipIt->addresses)
                ipIt->reliability[ip] = qBound(-1.0,
                    ipIt->reliability.value(ip, 0.0) * 0.5 + 0.3, 0.5);
        }
    }

    // SHA1 verification — must pass before any I/O or counting.
    // This is the definitive SHA1 check. The I/O worker only writes to disk.
    if (!task.sha1.isEmpty()) {
        QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex();
        if (hash != task.sha1) {
            int nextIdx = ift.mirrorIndex + 1;
            if (nextIdx < task.mirrors.size()) {
                qCWarning(logAsset) << QStringLiteral("  [资源] SHA1 校验失败 %1，切换到镜像[%2]").arg(task.sha1).arg(nextIdx);
                QString host = extractHost(ift.task.mirrors.value(ift.mirrorIndex));
                recordHostResult(host, false, elapsed);

                AssetTask retryTask = task;
                retryTask.mirrors = task.mirrors.mid(nextIdx);
                m_pendingQueue.prepend(retryTask);
                fireNext();
            } else {
                qCWarning(logAsset) << QStringLiteral("  [资源] SHA1 校验失败 %1（所有镜像均不匹配）").arg(task.sha1);
                finishDownload(task, false);
            }
            return;
        }
    }

    // 字节已在 downloadProgress 增量累加，这里不再重复累加。
    // onReplyFinished 只负责 I/O（写盘、SHA1 校验）。

    // I/O offload: SHA1 + disk write in thread pool
    enqueueIO(task, data);
}

// ═════════════════════════════════════════════════════════════════════════════
// Speed-adaptive scheduler (replaces the old rampTick)
// ═════════════════════════════════════════════════════════════════════════════

void AssetDownloader::accelTick()
{
    if (m_state != Running) {
        m_accelTimer->stop();
        return;
    }

    // Wait for DNS to finish before accelerating
    if (!m_dnsAllResolved) {
        return;
    }

    // Sample speed every tick
    sampleSpeed();
    updateSpeedFloor();

    // Log speed once per second regardless of dispatch state
    logSpeed();

    // Speed limit gate: if user set a limit and we're at/above it, don't add more
    if (m_speedLimitMB > 0.0 && m_emaMbps >= m_speedLimitMB) {
        return;
    }

    schedulePhase();

    int inflight = currentInflight();
    int target = m_targetInflight;

    // Keep timer running even when full — logSpeed() + adjustHostLimits()
    // need to fire periodically to increase per-host limits.
    // Only stop when there's genuinely no work left.
    if (m_pendingQueue.isEmpty() && m_pendingPreCheck.isEmpty()) {
        m_accelTimer->stop();
        return;
    }
    if (inflight >= target) {
        return;  // at capacity, but keep timer for limit adjustment
    }

    // Fire more requests up to target
    int toSend = qMin(target - inflight, kAccelStep);
    for (int i = 0; i < toSend && !m_pendingQueue.isEmpty(); ++i)
        fireNext();

    // accelTick: debug-level (every 50ms would flood log file)
    // Enable with: setFilterRules(\"ShadowDownloader.Asset.debug=true\")
    // and remove the 'if (type == QtDebugMsg) return;' in shadowMessageHandler
    qCDebug(logAsset) << QStringLiteral("  [资源] 加速: phase=%1 inflight=%2 target=%3 floor=%4/s pending=%5 preCheck=%6")
        .arg(m_phase).arg(inflight).arg(target)
        .arg(fmtSize(m_speedFloorBps.loadRelaxed()))
        .arg(m_pendingQueue.size())
        .arg(m_pendingPreCheck.size());
}

void AssetDownloader::schedulePhase()
{
    qint64 floor = m_speedFloorBps.loadRelaxed();
    qint64 speed = 0;
    {
        QMutexLocker lock(&m_speedMutex);
        if (!m_speedRecords.isEmpty())
            speed = m_speedRecords.first();
    }

    int inflight = currentInflight();
    int pending = m_pendingQueue.size();
    int preChecking = m_pendingPreCheck.size();
    bool hasWork = (pending > 0 || preChecking > 0);

    switch (m_phase) {

    case PhaseInit:
        // Should not normally reach here; handled in startDownload
        m_phase = PhaseBurst;
        m_targetInflight = qMin(kBurstSize, m_maxConcurrent);
        break;

    case PhaseBurst:
        // After burst sent, immediately go to accelerate
        m_phase = PhaseAccelerate;
        m_targetInflight = qMin(m_burstSent + kAccelStep, m_maxConcurrent);
        break;

    case PhaseAccelerate:
        if (inflight >= m_maxConcurrent || !hasWork) {
            m_phase = PhaseSteady;
            m_targetInflight = qMin(inflight, m_maxConcurrent);
        } else if (speed >= floor && inflight > 0) {
            // Saturated — stop adding, let speed floor catch up
            m_phase = PhaseSteady;
            m_targetInflight = inflight;
        } else {
            // Speed < floor — add more (but keep some headroom for I/O)
            m_targetInflight = qMin(inflight + kAccelStep, m_maxConcurrent);
        }
        break;

    case PhaseSteady:
        if (!hasWork) {
            m_targetInflight = inflight;
        } else if (speed < floor && inflight < m_maxConcurrent && pending > 0) {
            // Speed dropped below floor — try adding more
            m_phase = PhaseAccelerate;
            m_targetInflight = qMin(inflight + kAccelStep, m_maxConcurrent);
        } else if (inflight > 0 && speed == 0 && inflight >= 4) {
            // Complete stall — reduce inflight to avoid congestion collapse
            m_phase = PhaseCooldown;
            m_targetInflight = qMax(4, inflight / 2);
        } else if (pending > 0) {
            // Steady state: replace finished requests
            m_targetInflight = qMin(m_maxConcurrent, inflight + 1);
        }
        break;

    case PhaseCooldown:
        if (speed > 0) {
            m_phase = PhaseAccelerate;
            m_targetInflight = qMin(inflight + kAccelStep, m_maxConcurrent);
        } else if (inflight <= 4 || !hasWork) {
            m_phase = PhaseSteady;
            m_targetInflight = qMin(8, m_maxConcurrent);
        }
        break;
    }
}

int AssetDownloader::currentInflight() const
{
    return m_inFlight.size();
}

// ═════════════════════════════════════════════════════════════════════════════
// Speed monitoring
// ═════════════════════════════════════════════════════════════════════════════

void AssetDownloader::sampleSpeed()
{
    qint64 elapsed = m_speedTimer.elapsed();
    if (elapsed < 50) return;

    // Exclude cache-hit bytes from speed calculation
    qint64 now = m_downloadedBytes.loadRelaxed();
    qint64 cacheNow = m_cacheBytes.loadRelaxed();
    qint64 base = m_lastSampleBytes.loadRelaxed();
    qint64 netBytes = (now - cacheNow) - base;

    if (netBytes < 0) {
        // Safety: if cache count caught up after reset baseline, re-sync
        m_lastSampleBytes.storeRelaxed(now - cacheNow);
        netBytes = 0;
    }

    m_lastSampleBytes.storeRelaxed(now - cacheNow);
    m_speedTimer.restart();

    qint64 bps = netBytes * 1000 / qMax(elapsed, 1LL);

    {
        QMutexLocker lock(&m_speedMutex);
        m_speedRecords.prepend(bps);
        if (m_speedRecords.size() > kMaxSpeedRecords)
            m_speedRecords.removeLast();
    }

    // Update EMA
    double mbps = bps / (1024.0 * 1024.0);
    m_emaMbps = m_emaMbps * 0.5 + mbps * 0.5;
}

void AssetDownloader::updateSpeedFloor()
{
    // Weighted average: newer samples count more
    qint64 weightedSum = 0;
    int weightDiv = 0;
    {
        QMutexLocker lock(&m_speedMutex);
        int w = m_speedRecords.size();
        for (auto rec : m_speedRecords) {
            weightedSum += rec * w;
            weightDiv += w;
            w--;
        }
    }
    qint64 avgBps = (weightDiv > 0) ? (weightedSum / weightDiv) : 0;
    if (avgBps < kMinSpeedFloorBps) return;

    // Floor = 85% of weighted average (architecture unchanged)
    qint64 newFloor = static_cast<qint64>(avgBps * 0.85);
    qint64 currentFloor = m_speedFloorBps.loadRelaxed();
    if (newFloor > currentFloor) {
        m_speedFloorBps.storeRelaxed(newFloor);
        qCInfo(logAsset) << QStringLiteral("速度下限已提升到 %1 M").arg(newFloor / (1024.0 * 1024.0), 0, 'f', 2);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Finish tracking for one task
// ═════════════════════════════════════════════════════════════════════════════

void AssetDownloader::finishDownload(const AssetTask& task, bool success)
{
    // NOTE: downloadedBytes are counted in onReplyFinished() or the cache-hit
    // path in fireNext(). We do NOT count them here to avoid double-counting.
    if (success) {
        m_completedFiles.fetchAndAddRelaxed(1);
    } else {
        m_failedCount++;
        m_failedFiles.append(task.sha1);
        m_completedFiles.fetchAndAddRelaxed(1);
    }

    // Throttled progress emit
    qint64 now = m_lastProgressEmit.elapsed();
    if (now >= kProgressThrottleMs) {
        m_lastProgressEmit.start();
        emit progressChanged(m_completedFiles.loadRelaxed(),
                             m_totalFiles.loadRelaxed(),
                             m_downloadedBytes.loadRelaxed(),
                             m_totalBytes.loadRelaxed());
    }

    emit fileCompleted(task.sha1, success);

    if (!task.mirrors.isEmpty()) {
        emit fileProgress(task.mirrors.first(), task.sha1,
                          task.size, task.size, task.savePath);
    }

    // Adjust per-host limits on every file completion so the dynamic
    // limit can ramp up even when accelTick is busy dispatching.
    // (Was only called from logSpeed() which stops if accelTimer is off.)
    adjustHostLimits();

    // Don't fireNext directly if at capacity — let accelTick pace requests.
    // This prevents inflight from growing unboundedly when finishDownload
    // fires faster than the server can handle, which caused RST_STREAM on
    // BMCLAPI / Mojang CDN when 5000+ files were all dispatched at once.
    if (m_inFlight.size() < m_maxConcurrent) {
        fireNext();
    } else if (!m_accelTimer->isActive()) {
        m_accelTimer->start();  // re-activate to dispatch when slots free
    }
    checkAllFinished();
}

// ═════════════════════════════════════════════════════════════════════════════
// Check completion
// ═════════════════════════════════════════════════════════════════════════════

void AssetDownloader::checkAllFinished()
{
    if (m_state != Running && m_state != Cancelled) return;
    if (!m_pendingQueue.isEmpty() || !m_inFlight.isEmpty() || !m_pendingPreCheck.isEmpty()) return;
    if (m_preCheckQueued > 0) return;

    m_accelTimer->stop();
    m_state = (m_state == Cancelled) ? Cancelled : Done;

    emit progressChanged(m_completedFiles.loadRelaxed(),
                         m_totalFiles.loadRelaxed(),
                         m_downloadedBytes.loadRelaxed(),
                         m_totalBytes.loadRelaxed());

    if (m_state == Cancelled) {
        emit logMessage(QStringLiteral("[资源] 下载已取消"));
        emit allFinished(false, m_failedCount, m_failedFiles);
    } else {
        bool ok = (m_failedCount == 0);
        emit logMessage(QStringLiteral("[资源] 资源文件下载完成 总数=%1 缓存命中=%2 失败=%3 峰值=%4 MB/s")
                            .arg(m_totalTaskCount)
                            .arg(m_cacheHitCount)
                            .arg(m_failedCount)
                            .arg(m_emaMbps, 0, 'f', 1));
        emit allFinished(ok, m_failedCount, m_failedFiles);
        logState("all done");
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Dynamic per-host limit adjustment (TCP slow-start style)
// ═════════════════════════════════════════════════════════════════════════════

void AssetDownloader::adjustHostLimits()
{
    // Slight lock risk — keep it quick
    QMutexLocker lock(&m_hostMutex);
    for (auto it = m_hostStats.begin(); it != m_hostStats.end(); ++it) {
        if (it.key().contains(QStringLiteral("bmclapi")))
            continue;  // BMCLAPI uses fixed limit
        auto& st = it.value();
        if (st.consecutiveFails > 0)
            continue;  // recent failure — don't increase
        if (st.totalSuccess < 1)
            continue;  // need at least 1 success
        // If we consistently fill the current limit, try increasing
        if (st.activeRequests >= st.dynamicLimit * 0.8) {
            int oldLimit = st.dynamicLimit;
            st.dynamicLimit = qMin(st.dynamicLimit + 1, 32);
            // Per-host limit increase: debug-only (too verbose at info level)
            if (st.dynamicLimit != oldLimit)
                qCDebug(logAsset) << QStringLiteral("  [资源] 限制 %1 ↑ %2 → %3 (良好)").arg(it.key()).arg(oldLimit).arg(st.dynamicLimit);
        }
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Per-host health
// ═════════════════════════════════════════════════════════════════════════════

QString AssetDownloader::extractHost(const QString& url) const
{
    QUrl qurl(url);
    return qurl.host().toLower();
}

AssetDownloader::HostStats& AssetDownloader::hostStats(const QString& host)
{
    return m_hostStats[host];
}

int AssetDownloader::getHostLimit(const QString& host) const
{
    // BMCLAPI: fixed at 8 (known to handle this well)
    if (host.contains(QStringLiteral("bmclapi")))
        return 8;
    // Other hosts: use dynamically adjusted per-host limit
    QMutexLocker lock(&m_hostMutex);
    auto it = m_hostStats.find(host);
    if (it == m_hostStats.end()) return 4;  // start conservative for unknown hosts
    return qBound(2, it->dynamicLimit, 32);
}

bool AssetDownloader::hostCanAccept(const QString& host) const
{
    QMutexLocker lock(&m_hostMutex);
    auto it = m_hostStats.find(host);
    if (it == m_hostStats.end()) return true;  // unknown = accept
    if (it->degraded) return false;
    if (it->activeRequests >= getHostLimit(host)) return false;
    return true;
}

void AssetDownloader::recordHostResult(const QString& host, bool ok, qint64 firstByteMs)
{
    QMutexLocker lock(&m_hostMutex);
    auto& st = m_hostStats[host];
    if (ok) {
        st.consecutiveFails = 0;
        st.totalSuccess++;
        // Update running average (first-byte time)
        st.avgFirstByteMs = (st.avgFirstByteMs * 3 + firstByteMs) / 4;
        // Recover from degraded state after 3 consecutive successes
        if (st.degraded && st.consecutiveFails == 0 && st.totalSuccess >= 3)
            st.degraded = false;
    } else {
        st.consecutiveFails++;
        st.totalFails++;
        // Degrade after 3 consecutive failures
        if (st.consecutiveFails >= 3)
            st.degraded = true;
        // Increase timeout estimate (host is slow)
        st.avgFirstByteMs = qMin(st.avgFirstByteMs * 2, 30000LL);
        // Failure → reduce dynamic limit (TCP congestion control style)
        int oldLimit = st.dynamicLimit;
        st.dynamicLimit = qMax(2, st.dynamicLimit / 2);
        // Per-host limit decrease: debug-only
        if (st.dynamicLimit != oldLimit)
            qCDebug(logAsset) << QStringLiteral("  [资源] 限制 %1 ↓ %2 → %3 (失败)").arg(host).arg(oldLimit).arg(st.dynamicLimit);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Async DNS resolution — uses QHostInfo::lookupHost() instead of blocking fromName()
// ═════════════════════════════════════════════════════════════════════════════

void AssetDownloader::startAsyncDns()
{
    if (m_dnsToResolve.isEmpty()) {
        m_dnsAllResolved = true;
        return;
    }

    for (auto it = m_dnsToResolve.begin(); it != m_dnsToResolve.end(); ++it) {
        const QString& host = it.key();
        qint64 now = QDateTime::currentMSecsSinceEpoch();

        // Check cache
        {
            QMutexLocker lock(&m_dnsMutex);
            auto cacheIt = m_dnsCache.find(host);
            if (cacheIt != m_dnsCache.end()) {
                if (now - cacheIt->lastResolveMs < kDnsCacheMs) {
                    // Cache hit — count as resolved immediately
                    qCInfo(logAsset) << QStringLiteral("  [资源] DNS 缓存命中: %1 %2").arg(host, cacheIt->addresses.join(", "));
                    m_dnsPendingCount--;
                    checkAllDnsResolved();
                    continue;
                }
            }
        }

        // Async lookup
        QHostInfo::lookupHost(host, this, [this, host](const QHostInfo& info) {
            if (m_state != Running && m_state != Idle) return;

            QMutexLocker lock(&m_dnsMutex);
            IPInfo& ipi = m_dnsCache[host];
            ipi.lastResolveMs = QDateTime::currentMSecsSinceEpoch();

            if (info.error() != QHostInfo::NoError) {
                qCWarning(logAsset) << QStringLiteral("  [资源] DNS 解析失败: %1 (%2)").arg(host, info.errorString());
                ipi.addresses.clear();
            } else {
                QStringList ipv4, ipv6;
                for (const auto& addr : info.addresses()) {
                    if (addr.protocol() == QAbstractSocket::IPv4Protocol)
                        ipv4.append(addr.toString());
                    else
                        ipv6.append(addr.toString());
                }
                auto sortByReliability = [&](QStringList& list) {
                    std::sort(list.begin(), list.end(), [&](const QString& a, const QString& b) {
                        return ipi.reliability.value(a, 0.0) > ipi.reliability.value(b, 0.0);
                    });
                };
                sortByReliability(ipv4);
                sortByReliability(ipv6);
                ipi.addresses = ipv4 + ipv6;
                qCInfo(logAsset) << QStringLiteral("  [资源] DNS %1 → %2").arg(host, ipi.addresses.join(", "));
            }
            lock.unlock();

            m_dnsPendingCount--;
            checkAllDnsResolved();
        });
    }
}

void AssetDownloader::checkAllDnsResolved()
{
    if (!m_dnsAllResolved && m_dnsPendingCount <= 0) {
        m_dnsAllResolved = true;
        qCInfo(logAsset) << QStringLiteral("  [资源] DNS 所有主机解析完成");
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// I/O offload — SHA1 verification + disk write in thread pool
// ═════════════════════════════════════════════════════════════════════════════

class IOWorker : public QRunnable {
public:
    AssetDownloader::AssetTask task;
    QByteArray data;
    std::function<void(const AssetDownloader::AssetTask&, bool)> callback;

    void run() override {
        // SHA1 already verified in onReplyFinished().
        // This worker only writes verified data to disk.
        bool writeOk = false;
        {
            QFile f(task.savePath);
            if (f.open(QIODevice::WriteOnly)) {
                f.write(data);
                f.close();
                // Write results are logged via finishDownload signal; keep lightweight here.
        // qCDebug is available for per-file debugging.
                writeOk = true;
            } else {
                qCWarning(logAsset) << QStringLiteral("  [资源] 写入失败 %1: %2").arg(task.sha1, f.errorString());
            }
        }

        if (callback)
            callback(task, writeOk);
    }
};

void AssetDownloader::enqueueIO(const AssetTask& task, const QByteArray& data)
{
    auto* worker = new IOWorker();
    worker->task = task;
    worker->data = data;

    QPointer<AssetDownloader> self(this);
    worker->callback = [self](const AssetDownloader::AssetTask& t, bool ok) {
        if (self) {
            QMetaObject::invokeMethod(self, [self, t, ok]() {
                if (self->m_state == AssetDownloader::Cancelled) return;
                self->finishDownload(t, ok);
            });
        }
    };

    m_ioPool.start(worker);
}

// ═════════════════════════════════════════════════════════════════════════════
QString AssetDownloader::sha1HexOf(const QByteArray& data)
{
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex());
}

// ═════════════════════════════════════════════════════════════════════════════
// Async SHA1 pre-check — offloaded to IO thread pool to avoid blocking main thread.
// ═════════════════════════════════════════════════════════════════════════════

class PreCheckWorker : public QRunnable {
public:
    AssetDownloader::AssetTask task;
    QString checkPath;              // path to check SHA1 at; empty = use task.savePath
    bool copyOnHit = false;         // if SHA1 matches and copyOnHit, copy checkPath → task.savePath
    std::function<void(const AssetDownloader::AssetTask&, bool)> callback;

    void run() override {
        qint64 t0 = QDateTime::currentMSecsSinceEpoch();
        const QString targetPath = checkPath.isEmpty() ? task.savePath : checkPath;
        bool sha1Match = false;
        {
            QFile f(targetPath);
            if (f.open(QIODevice::ReadOnly)) {
                QCryptographicHash hash(QCryptographicHash::Sha1);
                hash.addData(&f);
                f.close();
                if (hash.result().toHex() == task.sha1) {
                    sha1Match = true;
                }
            }
        }
        // If hit came from a fallback cache path, copy to working dir
        if (sha1Match && copyOnHit && targetPath != task.savePath) {
            QDir().mkpath(QFileInfo(task.savePath).absolutePath());
            if (!QFile::copy(targetPath, task.savePath)) {
                // Copy can fail if destination already exists (duplicate SHA1 task
                // from a different asset index entry — same content, same hash).
                if (QFileInfo::exists(task.savePath)) {
                    // Verify existing file has the right content
                    QFile f(task.savePath);
                    if (f.open(QIODevice::ReadOnly)) {
                        QCryptographicHash h(QCryptographicHash::Sha1);
                        h.addData(&f);
                        f.close();
                        if (h.result().toHex() == task.sha1) {
                            sha1Match = true;  // already there, count as hit
                        } else {
                            qCWarning(logAsset) << QStringLiteral("  [资源] 缓存复制失败（冲突）: %1").arg(task.sha1.left(12));
                            sha1Match = false;
                        }
                    } else {
                        qCWarning(logAsset) << QStringLiteral("  [资源] 缓存复制失败（无法验证）: %1").arg(task.sha1.left(12));
                        sha1Match = false;
                    }
                } else {
                    qCWarning(logAsset) << QStringLiteral("  [资源] 缓存复制失败（目标不存在）: %1 %2").arg(task.sha1.left(12), targetPath);
                    sha1Match = false;
                }
            }
        }
        qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - t0;
        // Per-file pre-check result: debug-only (too verbose at info level when 5000+ files)
        if (elapsed > 50) {
            qCDebug(logAsset) << QStringLiteral("  [资源] 预检查 %1 %2 %3 用时 %4ms").arg(task.sha1, sha1Match ? QStringLiteral("命中") : QStringLiteral("不匹配"), fmtSize(task.size)).arg(elapsed);
        }
        if (callback)
            callback(task, sha1Match);
    }
};

void AssetDownloader::enqueuePreCheck(const AssetTask& task, const QString& checkPath)
{
    // NOTE: Deliberately NOT de-duplicating by SHA1 here. Multiple asset index entries
    // CAN share the same SHA1 (different virtual paths, identical content). The first
    // PreCheckWorker that hits copies the file to savePath; subsequent workers for the
    // same SHA1 will find the file already there and count as cache hit via the
    // copy-fallback logic below. Removing dedup avoids the silent-drop bug.

    PreCheckPending pcp;
    pcp.task = task;
    pcp.enqueueAtMs = QDateTime::currentMSecsSinceEpoch();
    m_pendingPreCheck.insert(task.sha1, pcp);
    m_preCheckQueued++;

    auto* worker = new PreCheckWorker();
    worker->task = task;
    worker->checkPath = checkPath.isEmpty() ? task.savePath : checkPath;
    worker->copyOnHit = !checkPath.isEmpty() && checkPath != task.savePath;

    QPointer<AssetDownloader> self(this);
    worker->callback = [self](const AssetDownloader::AssetTask& t, bool sha1Match) {
        if (self) {
            QMetaObject::invokeMethod(self, [self, t, sha1Match]() {
                if (self->m_state == AssetDownloader::Cancelled) return;
                self->onPreCheckResult(t, sha1Match);
            });
        }
    };

    m_ioPool.start(worker);
}

void AssetDownloader::onPreCheckResult(const AssetTask& task, bool sha1Match)
{
    m_pendingPreCheck.remove(task.sha1);
    m_preCheckQueued--;

    if (m_state != Running) return;

    if (sha1Match) {
        // Cache hit: count bytes, finish, and fire next
        m_cacheHitCount++;
        m_downloadedBytes.fetchAndAddRelaxed(task.size);
        m_cacheBytes.fetchAndAddRelaxed(task.size);
        finishDownload(task, true);
    } else {
        // Size matched but SHA1 didn't: must download
        m_pendingQueue.prepend(task);
        if (m_inFlight.size() < m_maxConcurrent) {
            fireNext();
        } else {
            // If all slots full, re-activate timer to dispatch when slots free up
            if (!m_accelTimer->isActive())
                m_accelTimer->start();
        }
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Reset speed baseline — called after burst is sent to avoid cache-hit spike.
// ═════════════════════════════════════════════════════════════════════════════

void AssetDownloader::resetSpeedBaseline()
{
    m_speedTimer.restart();
    // Subtract cache-hit bytes so the first speed reading reflects only network I/O
    m_lastSampleBytes.storeRelaxed(
        m_downloadedBytes.loadRelaxed() - m_cacheBytes.loadRelaxed());
    m_emaMbps = 0.0;
    {
        QMutexLocker lock(&m_speedMutex);
        m_speedRecords.clear();
    }
    m_speedFloorBps.storeRelaxed(kMinSpeedFloorBps);
}

// ═════════════════════════════════════════════════════════════════════════════
// Detailed logging
// ═════════════════════════════════════════════════════════════════════════════

void AssetDownloader::logState(const char* event)
{
    qint64 elapsed = m_downloadTimer.elapsed();
    int inflight = m_inFlight.size();
    int pending = m_pendingQueue.size();
    int preCheckLeft = m_pendingPreCheck.size();
    int done = m_completedFiles.loadRelaxed();
    int total = m_totalFiles.loadRelaxed();
    qint64 bytes = m_downloadedBytes.loadRelaxed();
    double speed = m_emaMbps;

    // State transitions always go to info-level (visible in log file)
    qCInfo(logAsset) << QStringLiteral("[资源] [状态] %1 用时=%2ms phase=%3 inflight=%4 pending=%5 预检查=%6 进度=%7/%8 已下载=%9 速度=%10 MB/s 阈值=%11/s 缓存命中=%12")
        .arg(QString::fromLatin1(event))
        .arg(elapsed).arg(m_phase).arg(inflight).arg(pending).arg(preCheckLeft)
        .arg(done).arg(total)
        .arg(fmtSize(bytes))
        .arg(QString::number(speed, 'f', 1))
        .arg(fmtSize(m_speedFloorBps.loadRelaxed()))
        .arg(m_cacheHitCount);
}

// ═════════════════════════════════════════════════════════════════════════════
// Periodic speed logging — 1-second interval, info-level (visible in log file)
// ═════════════════════════════════════════════════════════════════════════════

void AssetDownloader::logSpeed()
{
    // Rate-limited: log every 5 seconds instead of every second to reduce log volume
    if (m_speedLogTimer.elapsed() < 5000)
        return;
    m_speedLogTimer.restart();

    qint64 elapsed = m_downloadTimer.elapsed();
    qint64 bytes = m_downloadedBytes.loadRelaxed();
    qint64 cacheBytes = m_cacheBytes.loadRelaxed();
    int inflight = m_inFlight.size();
    int pending = m_pendingQueue.size();
    int done = m_completedFiles.loadRelaxed();
    int total = m_totalFiles.loadRelaxed();
    double speed = m_emaMbps;

    // Also compute a simple average-overall speed for comparison
    double avgMbps = (elapsed > 0 && bytes > 0)
        ? (bytes - cacheBytes) / (1024.0 * 1024.0) / (elapsed / 1000.0)
        : 0.0;

    qCInfo(logAsset) << QStringLiteral("[资源] [速度] EMA=%1 MB/s 平均=%2 MB/s 阈值=%3/s phase=%4 inflight=%5 pending=%6 进度=%7/%8 用时=%9s")
        .arg(QString::number(speed, 'f', 2))
        .arg(QString::number(avgMbps, 'f', 2))
        .arg(fmtSize(m_speedFloorBps.loadRelaxed()))
        .arg(m_phase).arg(inflight).arg(pending)
        .arg(done).arg(total)
        .arg(elapsed / 1000);

    // Adjust per-host limits periodically
    adjustHostLimits();
}
