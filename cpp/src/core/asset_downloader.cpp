// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
//
// Asset-dedicated download engine v2.
//
// Key improvements over v1:
//   - 2 × HTTP/1.1 QNAM (255 conn/host each) → avoids HTTP/2 RST_STREAM issues
//   - Three-phase acceleration (burst → accelerate → steady)
//   - Speed-based adaptive concurrency with sliding-window speed floor
//   - SHA1+disk I/O offloaded to QThreadPool
//   - Per-host health tracking + DNS IP reliability scoring
//
// Principle ("phased acceleration" inspired by 主流启动器):
//   1. Burst: fire ~20 requests immediately (bypass SHA1 precheck for speed)
//   2. Accelerate: every 50ms, check throughput. If speed < floor → add 8 more.
//      Speed floor starts at 256KB/s, rises to 85% of weighted peak.
//   3. Steady: when speed >= floor, stop adding. Replace finished requests.
//   4. Cooldown: if speed drops sharply, reduce inflight to avoid congestion.

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
    m_phase = PhaseBurst;

    for (int i = 0; i < 2; ++i) {
        if (m_nam[i]) {
            m_nam[i]->disconnect();
            m_nam[i]->deleteLater();
            m_nam[i] = nullptr;
        }
        m_nam[i] = new QNetworkAccessManager(this);
        // HTTP/1.1 with 255 max connections per host — avoids HTTP/2
        // stream-multiplexing issues that BMCLAPI triggers (RST_STREAM).
        // Two QNAMs × 255 = 510 concurrent connections max.
        // HTTP/1.1 connections — we set per-request config in fireNext()
        // because QNetworkAccessManager::setHttp1Configuration may not be
        // available across all Qt 6 minor versions. Per-request is fine.
    }

    qCInfo(logAsset) << "AssetDownloader v2: 2× HTTP/1.1 QNAMs (255 conn/host)";
}

// ═════════════════════════════════════════════════════════════════════════════
// Entry point
// ═════════════════════════════════════════════════════════════════════════════

void AssetDownloader::startDownload(const QVector<AssetTask>& tasks, int maxConcurrent)
{
    if (m_state == Running) {
        qCWarning(logAsset) << "already running, ignoring startDownload()";
        return;
    }

    m_maxConcurrent = qBound(16, maxConcurrent, 256);
    m_targetInflight = 0;
    m_phase = PhaseBurst;
    m_burstSent = 0;
    m_cacheHitCount = 0;

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

    // Pre-resolve DNS for all unique hosts
    QSet<QString> hosts;
    for (const auto& t : tasks) {
        for (const auto& m : t.mirrors)
            hosts.insert(extractHost(m));
    }
    for (const auto& h : hosts)
        resolveHost(h);

    m_state = Running;
    m_lastProgressEmit.start();
    m_speedTimer.start();
    // 初始化 lastSampleBytes 为当前已加载的缓存字节，避免首次测速将
    // 所有 cache hit 的文件大小算作瞬时速度
    m_lastSampleBytes.storeRelaxed(m_downloadedBytes.loadRelaxed());

    emit logMessage(QString("AssetDownloader v2: %1 files (%2), %3 hosts")
                        .arg(tasks.size()).arg(fmtSize(totalEst)).arg(hosts.size()));
    emit progressChanged(0, tasks.size(), 0, totalEst);

    // ── Phase 1: Burst — fire kBurstSize immediately ──
    int burst = qMin(kBurstSize, m_maxConcurrent);
    for (int i = 0; i < burst && !m_pendingQueue.isEmpty(); ++i) {
        fireNext();
        m_burstSent++;
    }

    if (m_inFlight.size() < m_maxConcurrent && !m_pendingQueue.isEmpty())
        m_accelTimer->start();  // enter Phase 2: accelerate
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

    for (auto* reply : toAbort) {
        reply->abort();
        reply->deleteLater();
    }

    m_ioPool.clear();

    emit logMessage("AssetDownloader: cancelled");
    emit allFinished(false, m_failedCount, m_failedFiles);
}

// ═════════════════════════════════════════════════════════════════════════════
// Fire next pending request
// ═════════════════════════════════════════════════════════════════════════════

void AssetDownloader::fireNext()
{
    if (m_state != Running || m_pendingQueue.isEmpty()) return;

    AssetTask task = m_pendingQueue.dequeue();

    if (task.mirrors.isEmpty()) {
        QTimer::singleShot(0, this, [this, task]() {
            finishDownload(task, false);
        });
        return;
    }

    // SHA1 pre-check: skip if file exists with matching hash
    if (!task.sha1.isEmpty()) {
        QFileInfo fi(task.savePath);
        if (fi.exists() && fi.size() > 0) {
            // Quick size-match check first
            if (task.size > 0 && fi.size() != task.size)
                goto do_download;

            QFile f(task.savePath);
            if (f.open(QIODevice::ReadOnly)) {
                QCryptographicHash hash(QCryptographicHash::Sha1);
                hash.addData(&f);
                f.close();
                if (hash.result().toHex() == task.sha1) {
                    // Cache hit — count bytes immediately for smooth progress
                    m_cacheHitCount++;
                    m_downloadedBytes.fetchAndAddRelaxed(task.size);
                    QTimer::singleShot(0, this, [this, task]() {
                        finishDownload(task, true);
                    });
                    return;
                }
            }
        }
    }

do_download:
    // Ensure parent dir exists
    QDir().mkpath(QFileInfo(task.savePath).absolutePath());

    // Pick source: skip degraded hosts
    const QStringList& mirrors = task.mirrors;
    int selectedMirror = 0;
    for (int i = 0; i < mirrors.size(); ++i) {
        QString host = extractHost(mirrors[i]);
        if (hostCanAccept(host)) {
            selectedMirror = i;
            break;
        }
        // Last resort: use it anyway
        if (i == mirrors.size() - 1)
            selectedMirror = i;
    }

    const QString& url = mirrors[selectedMirror];
    QUrl qurl(url);
    QNetworkRequest req(qurl);
    req.setRawHeader("User-Agent", "ShadowLauncher/1.0");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    // HTTP/1.1: allow up to 255 connections per host per manager (×2 = 510)
    {
        QHttp1Configuration h1cfg;
        h1cfg.setNumberOfConnectionsPerHost(255);
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
    ift.task = task;
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
            qCWarning(logAsset) << "  [fail]" << task.sha1
                << "all mirrors exhausted (" << reply->errorString() << ")";
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
                qCWarning(logAsset) << "  [sha1]" << task.sha1
                    << "mismatch, switching to mirror[" << nextIdx << "]";
                QString host = extractHost(ift.task.mirrors.value(ift.mirrorIndex));
                recordHostResult(host, false, elapsed);

                AssetTask retryTask = task;
                retryTask.mirrors = task.mirrors.mid(nextIdx);
                m_pendingQueue.prepend(retryTask);
                fireNext();
            } else {
                qCWarning(logAsset) << "  [fail]" << task.sha1
                    << "SHA1 mismatch (all mirrors)";
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

    // Sample speed every tick
    sampleSpeed();
    updateSpeedFloor();

    // Speed limit gate: if user set a limit and we're at/above it, don't add more
    if (m_speedLimitMB > 0.0 && m_emaMbps >= m_speedLimitMB) {
        return;
    }

    schedulePhase();

    int inflight = currentInflight();
    int target = m_targetInflight;

    if (inflight >= target || m_pendingQueue.isEmpty()) {
        if (inflight >= m_maxConcurrent || m_pendingQueue.isEmpty())
            m_accelTimer->stop();  // either fully loaded or no more work
        return;
    }

    // Fire more requests up to target
    int toSend = qMin(target - inflight, kAccelStep);
    for (int i = 0; i < toSend && !m_pendingQueue.isEmpty(); ++i)
        fireNext();

    qCDebug(logAsset) << "  accel: phase=" << m_phase
        << "inflight=" << inflight << "target=" << target
        << "floor=" << fmtSize(m_speedFloorBps.loadRelaxed()) << "/s";
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

    switch (m_phase) {

    case PhaseBurst:
        // After burst sent, immediately go to accelerate
        m_phase = PhaseAccelerate;
        m_targetInflight = qMin(kBurstSize + kAccelStep, m_maxConcurrent);
        break;

    case PhaseAccelerate:
        if (inflight >= m_maxConcurrent || pending == 0) {
            m_phase = PhaseSteady;
            m_targetInflight = m_maxConcurrent;
        } else if (speed >= floor) {
            // Saturated — stop adding, let speed floor catch up
            m_phase = PhaseSteady;
            m_targetInflight = inflight;
        } else {
            // Speed < floor — add more
            m_targetInflight = qMin(inflight + kAccelStep, m_maxConcurrent);
        }
        break;

    case PhaseSteady:
        if (pending == 0) {
            // No more work — wind down
            m_targetInflight = inflight;
        } else if (speed < floor && inflight < m_maxConcurrent) {
            // Speed dropped below floor — try adding more
            m_phase = PhaseAccelerate;
            m_targetInflight = qMin(inflight + kAccelStep, m_maxConcurrent);
        } else if (inflight > 0 && speed == 0 && inflight >= 4) {
            // Complete stall — reduce inflight to avoid congestion collapse
            m_phase = PhaseCooldown;
            m_targetInflight = qMax(4, inflight / 2);
        } else {
            // Steady state: replace finished requests
            m_targetInflight = qMin(m_maxConcurrent, inflight + (pending > 0 ? 1 : 0));
        }
        break;

    case PhaseCooldown:
        if (speed > 0) {
            m_phase = PhaseAccelerate;
            m_targetInflight = qMin(inflight + kAccelStep, m_maxConcurrent);
        } else if (inflight <= 4 || pending == 0) {
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

    qint64 now = m_downloadedBytes.loadRelaxed();
    qint64 last = m_lastSampleBytes.loadRelaxed();
    qint64 bytes = now - last;
    m_lastSampleBytes.storeRelaxed(now);
    m_speedTimer.restart();

    qint64 bps = bytes * 1000 / qMax(elapsed, 1LL);

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

    // Floor = 85% of weighted average
    qint64 newFloor = static_cast<qint64>(avgBps * 0.85);
    qint64 currentFloor = m_speedFloorBps.loadRelaxed();
    if (newFloor > currentFloor) {
        m_speedFloorBps.storeRelaxed(newFloor);
        qCDebug(logAsset) << "  floor ↑" << fmtSize(currentFloor) << "/s →"
                          << fmtSize(newFloor) << "/s";
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

    fireNext();
    checkAllFinished();
}

// ═════════════════════════════════════════════════════════════════════════════
// Check completion
// ═════════════════════════════════════════════════════════════════════════════

void AssetDownloader::checkAllFinished()
{
    if (m_state != Running && m_state != Cancelled) return;
    if (!m_pendingQueue.isEmpty() || !m_inFlight.isEmpty()) return;

    m_accelTimer->stop();
    m_state = (m_state == Cancelled) ? Cancelled : Done;

    emit progressChanged(m_completedFiles.loadRelaxed(),
                         m_totalFiles.loadRelaxed(),
                         m_downloadedBytes.loadRelaxed(),
                         m_totalBytes.loadRelaxed());

    if (m_state == Cancelled) {
        emit logMessage("AssetDownloader: cancelled");
        emit allFinished(false, m_failedCount, m_failedFiles);
    } else {
        bool ok = (m_failedCount == 0);
        emit logMessage(QString("AssetDownloader v2: %1/%2 done, %3 cache hits, "
                                "%4 failed, peak %5 MB/s, floor %6/s")
                            .arg(m_totalTaskCount - m_failedCount)
                            .arg(m_totalTaskCount)
                            .arg(m_cacheHitCount)
                            .arg(m_failedCount)
                            .arg(m_emaMbps, 0, 'f', 1)
                            .arg(fmtSize(m_speedFloorBps.loadRelaxed())));
        emit allFinished(ok, m_failedCount, m_failedFiles);
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

bool AssetDownloader::hostCanAccept(const QString& host) const
{
    QMutexLocker lock(&m_hostMutex);
    auto it = m_hostStats.find(host);
    if (it == m_hostStats.end()) return true;  // unknown = accept
    // degraded is set by recordHostResult() — pure read here
    if (it->degraded) return false;
    if (it->activeRequests >= kMaxPerHost) return false;
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
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// DNS + IP reliability
// ═════════════════════════════════════════════════════════════════════════════

QStringList AssetDownloader::resolveHost(const QString& host)
{
    QMutexLocker lock(&m_dnsMutex);
    auto it = m_dnsCache.find(host);
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    if (it != m_dnsCache.end()) {
        // Check cache expiry
        if (now - it->lastResolveMs < kDnsCacheMs)
            return it->addresses;
        // If last resolve failed and backoff hasn't expired, skip
        if (it->addresses.isEmpty() && now - it->lastResolveMs < kDnsFailureBackoffMs)
            return {};
    }

    // Perform DNS resolution (synchronous but fast for cached/nearby records)
    QHostInfo info = QHostInfo::fromName(host);
    IPInfo& ipi = m_dnsCache[host];
    ipi.lastResolveMs = now;

    if (info.error() != QHostInfo::NoError) {
        qCWarning(logAsset) << "  [dns] resolve failed:" << host << info.errorString();
        ipi.addresses.clear();
        return {};
    }

    // Sort: prefer IPv4 over IPv6 (IPv6 often has worse routing in China)
    QStringList ipv4, ipv6;
    for (const auto& addr : info.addresses()) {
        if (addr.protocol() == QAbstractSocket::IPv4Protocol)
            ipv4.append(addr.toString());
        else
            ipv6.append(addr.toString());
    }

    // Mix: put higher-reliability IPs first
    auto sortByReliability = [&](QStringList& list) {
        std::sort(list.begin(), list.end(), [&](const QString& a, const QString& b) {
            return ipi.reliability.value(a, 0.0) > ipi.reliability.value(b, 0.0);
        });
    };
    sortByReliability(ipv4);
    sortByReliability(ipv6);

    ipi.addresses = ipv4 + ipv6;
    if (ipi.addresses.isEmpty()) {
        qCWarning(logAsset) << "  [dns] no addresses for:" << host;
    } else {
        qCDebug(logAsset) << "  [dns]" << host << "→" << ipi.addresses;
    }
    return ipi.addresses;
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
                qCDebug(logAsset) << "  [io-ok]" << task.sha1 << fmtSize(data.size());
                writeOk = true;
            } else {
                qCWarning(logAsset) << "  [io-write]" << task.sha1 << f.errorString();
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
