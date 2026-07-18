// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
// Shadow Launcher — Parallel download engine v6
// global chunk pool + speed floor + configurable source policy:
//  - All files pre-split into chunks → atomic counter distributes work
//  - Source order configurable (mirror-first / official-first / auto-switch)
//  - Speed floor: sticky high-water mark raised to 85% of observed speed,
//    prevents over-threading by stopping worker spawn when saturated
//  - Adaptive worker count starts at 16, ramps up/down based on throughput
//  - Small files: single chunk → direct download to final path
//  - Large files: HTTP Range chunks → merged on completion
//  - Version-level concurrency queue (max 3)

#include "parallel_downloader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QCryptographicHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QHttp1Configuration>
#include <QUrl>
#include <QEventLoop>
#include <QTimer>
#include <QThread>
#include <QProcess>
#include <QStandardPaths>

namespace ShadowLauncher {

// Static queue members
QMutex ParallelDownloader::s_queueMutex;
QQueue<ParallelDownloader*> ParallelDownloader::s_versionQueue;
int ParallelDownloader::s_activeVersionCount = 0;

// ═══════════════════════════════════════════════════════════
// Construction / Destruction
// ═══════════════════════════════════════════════════════════

ParallelDownloader::ParallelDownloader(const DownloadConfig& config, QObject* parent)
    : QObject(parent)
{
    m_userMaxWorkers = qBound(1, config.maxWorkers, kMaxWorkers);
    m_fileSource = config.fileSource;
    m_speedLimitMB = config.speedLimitMB;
}

ParallelDownloader::~ParallelDownloader()
{
    m_cancelled = true;
    m_state = Cancelled;
    {
        QMutexLocker lock(&m_mutex);
        m_paused = false;
        m_pauseCond.wakeAll();
    }

    for (QThread* w : m_workers) {
        if (w->isRunning()) {
            if (!w->wait(35000)) {
                w->terminate();
                w->wait(2000);
            }
        }
        delete w;
    }
    m_workers.clear();
}

// ═══════════════════════════════════════════════════════════
// Task management
// ═══════════════════════════════════════════════════════════

void ParallelDownloader::addTask(const DownloadTask& task)
{
    QMutexLocker lock(&m_mutex);
    if (m_state != Idle) return;
    m_tasks.append(task);
}

void ParallelDownloader::addTasks(const QVector<DownloadTask>& tasks)
{
    QMutexLocker lock(&m_mutex);
    if (m_state != Idle) return;
    m_tasks.append(tasks);
}

// ═══════════════════════════════════════════════════════════
// Source ordering
// ═══════════════════════════════════════════════════════════

bool ParallelDownloader::isMojangUrl(const QString& url) const
{
    return url.contains(QStringLiteral("mojang.com"), Qt::CaseInsensitive)
        || url.contains(QStringLiteral("minecraft.net"), Qt::CaseInsensitive);
}

QStringList ParallelDownloader::buildOrderedSources(const QString& primaryUrl,
                                                     const QStringList& fallbackUrls) const
{
    QStringList ordered;

    // Identify which is Mojang and which is BMCLAPI
    bool primaryIsMojang = isMojangUrl(primaryUrl);
    QStringList nonMojangMirrors;
    QString mojangMirror;
    for (const QString& m : fallbackUrls) {
        if (isMojangUrl(m))
            mojangMirror = m;
        else
            nonMojangMirrors.append(m);
    }

    switch (m_fileSource) {
    case DownloadSourcePolicy::PreferOfficial:
    case DownloadSourcePolicy::AutoSwitch:
        // Mojang first, then BMCLAPI, then other mirrors
        if (primaryIsMojang)
            ordered.append(primaryUrl);
        else if (!mojangMirror.isEmpty())
            ordered.append(mojangMirror);
        if (!primaryIsMojang)
            ordered.append(primaryUrl);
        ordered.append(nonMojangMirrors);
        if (primaryIsMojang && !mojangMirror.isEmpty())
            ordered.append(mojangMirror);
        break;

    case DownloadSourcePolicy::PreferMirror:
    default:
        // BMCLAPI first, then other mirrors, Mojang last
        ordered.append(primaryUrl);
        ordered.append(nonMojangMirrors);
        if (!mojangMirror.isEmpty())
            ordered.append(mojangMirror);
        else if (primaryIsMojang)
            ; // primary already Mojang, skip duplicate
        break;
    }

    // Deduplicate while preserving order
    QStringList deduped;
    for (const QString& u : ordered) {
        if (!deduped.contains(u))
            deduped.append(u);
    }
    return deduped;
}

// ═══════════════════════════════════════════════════════════
// Chunking logic: split file >1MB into chunks across mirrors
// ═══════════════════════════════════════════════════════════

QVector<ParallelDownloader::ChunkInfo>
ParallelDownloader::splitIntoChunks(const DownloadTask& task) const
{
    QVector<ChunkInfo> chunks;

    // Small files: single chunk, no splitting needed
    if (task.totalBytes <= kChunkThreshold) {
        ChunkInfo ci;
        ci.index     = 0;
        ci.start     = 0;
        ci.end       = -1;
        ci.mirrorUrl = task.url;
        ci.completed = false;
        chunks.append(ci);
        return chunks;
    }

    // Large file: split into ~2MB chunks, max kMaxChunks
    // All chunks use the primary URL (typically BMCLAPI) via HTTP Range.
    // Mirror diversity is handled by the fallback — if chunks fail,
    // we retry sequentially with all mirrors.
    const qint64 totalSize = task.totalBytes;
    const int numChunks = qBound(1,
                                 (int)(totalSize / kChunkSizeHint),
                                 kMaxChunks);
    const qint64 chunkSize = totalSize / numChunks;

    for (int i = 0; i < numChunks; ++i) {
        const qint64 start = i * chunkSize;
        qint64 end = (i == numChunks - 1) ? -1 : (start + chunkSize - 1);

        ChunkInfo ci;
        ci.index     = i;
        ci.start     = start;
        ci.end       = end;
        ci.mirrorUrl = task.url;  // all chunks from primary URL for max speed
        ci.completed = false;
        chunks.append(ci);
    }

    return chunks;
}


void ParallelDownloader::start()
{
    if (m_state == Running || m_state == Paused) return;

    // Clean completed tasks from previous runs
    {
        QMutexLocker lock(&m_mutex);
        m_tasks.erase(
            std::remove_if(m_tasks.begin(), m_tasks.end(),
                           [](const DownloadTask& t) { return t.completed; }),
            m_tasks.end());
    }

    // Reset state
    m_cancelled = false;
    m_paused = false;
    m_completedFiles.storeRelaxed(0);
    m_failedCount.storeRelaxed(0);
    m_downloadedBytes.storeRelaxed(0);
    m_nextTask.storeRelaxed(0);
    m_failedFiles.clear();

    // ── Enhanced: pre-split all files into global chunk pool ──
    m_chunkPool.clear();
    m_fileTrackers.clear();

    qint64 totalEstimate = 0;
    {
        QMutexLocker lock(&m_mutex);
        const int fileCount = m_tasks.size();
        m_fileTrackers.resize(fileCount);

        for (int fi = 0; fi < fileCount; ++fi) {
            const DownloadTask& task = m_tasks[fi];
            totalEstimate += task.totalBytes;

            FilePoolTracker& tracker = m_fileTrackers[fi];
            tracker.finalPath = task.savePath;
            tracker.sha1 = task.sha1;

            // Split file into chunks (always returns ≥1)
            QVector<ChunkInfo> chunks = splitIntoChunks(task);
            const int nChunks = chunks.size();
            tracker.remainingChunks.storeRelaxed(nChunks);

            if (nChunks == 1) {
                PoolChunk pc;
                pc.fileIndex = fi;
                pc.chunkIndex = 0;
                pc.chunkTotal = 1;
                pc.start = chunks[0].start;
                pc.end = chunks[0].end;
                pc.url = task.url;
                pc.chunkPath = task.savePath;
                pc.mirrors = task.mirrors;
                pc.totalBytes = task.totalBytes;
                m_chunkPool.append(pc);
            } else {
                for (const auto& ci : chunks) {
                    PoolChunk pc;
                    pc.fileIndex = fi;
                    pc.chunkIndex = ci.index;
                    pc.chunkTotal = nChunks;
                    pc.start = ci.start;
                    pc.end = ci.end;
                    pc.url = task.url;
                    pc.chunkPath = task.savePath + QStringLiteral(".chunk.")
                                   + QString::number(ci.index);
                    pc.mirrors = task.mirrors;
                    pc.totalBytes = task.totalBytes;
                    m_chunkPool.append(pc);
                }
            }
        }
    }

    m_totalBytes.storeRelaxed(totalEstimate);
    m_state = Running;

    emit logMessage(QString::fromUtf8("准备下载 %1 个文件, 预估 %2, 共 %3 块 (全局池, 上限 %4 workers)")
                        .arg(m_tasks.size())
                        .arg(formatSize(totalEstimate))
                        .arg(m_chunkPool.size())
                        .arg(m_userMaxWorkers));
    emit stateChanged();

    if (m_chunkPool.isEmpty()) {
        m_state = Done;
        emit allFinished(true, 0, QStringList());
        emit stateChanged();
        return;
    }

    // Spawn workers — start with conservative count (v6 adaptive)
    const int initialWorkers = qMin(kInitialWorkers, (int)m_chunkPool.size());
    m_maxWorkers.storeRelaxed(initialWorkers);
    for (int i = 0; i < initialWorkers; ++i) {
        m_activeWorkers.ref();
        QThread* worker = QThread::create([this]() { runWorker(); });
        m_workers.append(worker);
        worker->start();
    }

    // Start throughput timer
    m_throughputTimer.start();
    m_windowBytes.storeRelaxed(0);
    m_lastSpeedBytes = m_downloadedBytes.loadRelaxed();
    m_emaMbps = 0.0;
    m_peakMbps = 0.0;
    m_speedFloorBps.storeRelaxed(kMinSpeedFloorBps);
    m_bmclapiUnreachable.storeRelaxed(0);

    // Speed limit throttle
    m_throttleTimer.start();
    m_throttleBytes = 0;

    // Start periodic progress emission (200ms throttle)
    if (!m_progressTimer) {
        m_progressTimer = new QTimer(this);
        connect(m_progressTimer, &QTimer::timeout, this, [this]() {
            if (m_state != Running) return;
            int cf = m_completedFiles.loadRelaxed();
            int tf = m_tasks.size();
            qint64 db = m_downloadedBytes.loadRelaxed();
            qint64 tb = m_totalBytes.loadRelaxed();
            emit progressChanged(cf, tf, db, tb);

            // ── Adaptive concurrency: every kConcurrencyIntervalMs ──
            if (++m_concurrencyTick >= kConcurrencyIntervalMs / 200) {
                m_concurrencyTick = 0;
                adjustConcurrency();
            }
        });
    }
    m_progressTimer->start(200);
}

void ParallelDownloader::pause()
{
    if (m_state != Running) return;
    if (m_progressTimer) m_progressTimer->stop();
    m_state = Paused;
    {
        QMutexLocker lock(&m_mutex);
        m_paused = true;
    }
    emit logMessage(QString::fromUtf8("[暂停] 下载已暂停"));
    emit stateChanged();
}

void ParallelDownloader::resume()
{
    if (m_state != Paused) return;
    m_state = Running;
    {
        QMutexLocker lock(&m_mutex);
        m_paused = false;
        m_pauseCond.wakeAll();
    }
    if (m_progressTimer) m_progressTimer->start(200);
    // Reset throughput window to avoid misreading pause time as slowness
    m_throughputTimer.restart();
    m_windowBytes.storeRelaxed(0);
    m_lastSpeedBytes = m_downloadedBytes.loadRelaxed();
    m_throttleTimer.restart();
    m_throttleBytes = 0;
    emit logMessage(QString::fromUtf8("▶ 下载已恢复"));
    emit stateChanged();
}

void ParallelDownloader::cancel()
{
    if (m_state != Running && m_state != Paused) return;
    if (m_progressTimer) m_progressTimer->stop();
    m_state = Cancelled;
    m_cancelled = true;
    {
        QMutexLocker lock(&m_mutex);
        m_paused = false;
        m_pauseCond.wakeAll();
    }
    emit logMessage(QString::fromUtf8("[失败] 下载已取消"));
    emit stateChanged();
}

// ═══════════════════════════════════════════════════════════
// State queries
// ═══════════════════════════════════════════════════════════

int ParallelDownloader::totalFiles() const
{
    QMutexLocker lock(&const_cast<QMutex&>(m_mutex));
    return m_tasks.size();
}

int ParallelDownloader::completedFiles() const
{
    return m_completedFiles.loadRelaxed();
}

qint64 ParallelDownloader::totalBytes() const
{
    return m_totalBytes.loadRelaxed();
}

qint64 ParallelDownloader::downloadedBytes() const
{
    return m_downloadedBytes.loadRelaxed();
}

QString ParallelDownloader::stateStr() const
{
    switch (m_state) {
    case Idle:      return QStringLiteral("idle");
    case Running:   return QStringLiteral("running");
    case Paused:    return QStringLiteral("paused");
    case Cancelled: return QStringLiteral("cancelled");
    case Done:      return QStringLiteral("done");
    }
    return QStringLiteral("idle");
}

bool ParallelDownloader::isRunning() const
{
    return m_state == Running;
}

// ═══════════════════════════════════════════════════════════
// Version-level concurrency queue (static)
// ═══════════════════════════════════════════════════════════

bool ParallelDownloader::enqueueVersionDownload(ParallelDownloader* instance)
{
    QMutexLocker lock(&s_queueMutex);
    if (s_activeVersionCount < kMaxConcurrentVersions) {
        s_activeVersionCount++;
        instance->m_queuedAndReady = true;
        return true;
    }
    s_versionQueue.enqueue(instance);
    instance->m_queuedAndReady = false;
    return false;
}

void ParallelDownloader::dequeueVersionDownload(ParallelDownloader* instance)
{
    QMutexLocker lock(&s_queueMutex);
    Q_UNUSED(instance);
    s_activeVersionCount = qMax(0, s_activeVersionCount - 1);
}

void ParallelDownloader::processNextQueued()
{
    QMutexLocker lock(&s_queueMutex);
    if (s_versionQueue.isEmpty())
        return;
    if (s_activeVersionCount >= kMaxConcurrentVersions)
        return;

    ParallelDownloader* next = s_versionQueue.dequeue();
    s_activeVersionCount++;
    next->m_queuedAndReady = true;

    QMetaObject::invokeMethod(next, [next]() {
        if (next->m_queuedAndReady && next->m_state == Idle) {
            next->start();
        }
    }, Qt::QueuedConnection);
}

// ═══════════════════════════════════════════════════════════
// Worker thread entry point
// ═══════════════════════════════════════════════════════════

void ParallelDownloader::runWorker()
{
    // ── One QNAM per worker, created ONCE — avoids per-file SSL/socket init ──
    // Per-host connection limit set to 255 via QNetworkRequest::setHttp1Configuration
    // in downloadSingleUrl() (Qt 6.8 only supports it on request, not manager).
    QScopedPointer<QNetworkAccessManager> mgr(new QNetworkAccessManager());

    while (!m_cancelled) {
        // --- Pause checkpoint ---
        {
            QMutexLocker lock(&m_mutex);
            while (m_paused && !m_cancelled)
                m_pauseCond.wait(&m_mutex);
        }
        if (m_cancelled) break;

        // --- Voluntarily exit if we're excess workers (v6 adaptive) ---
        if (m_activeWorkers.loadRelaxed() > m_maxWorkers.loadRelaxed()) {
            m_activeWorkers.deref();
            return;
        }

        // --- Dequeue next CHUNK from global pool ---
        // v7: wrap-around scan — keeps all workers busy at the tail.
        // When the linear counter exhausts, reset and re-scan from the start.
        int idx;
        for (int loopGuard = 0; loopGuard < 3; ++loopGuard) {
            idx = m_nextTask.fetchAndAddRelaxed(1);
            if (idx < m_chunkPool.size() && !m_chunkPool[idx].completed)
                goto got_chunk;
            // Counter wrapped — reset and retry
            if (idx >= m_chunkPool.size()) {
                if (m_completedFiles.loadRelaxed() + m_failedCount.loadRelaxed() >= (int)m_fileTrackers.size())
                    goto exit_worker;
                m_nextTask.storeRelaxed(0);
            }
        }
        // Final pass: scan from 0
        for (int i = 0; i < m_chunkPool.size(); ++i) {
            if (!m_chunkPool[i].completed) {
                idx = i;
                goto got_chunk;
            }
        }
        break; // truly done
    got_chunk:
        const PoolChunk& chunk = m_chunkPool[idx];

        // Ensure chunk directory exists
        QDir().mkpath(QFileInfo(chunk.chunkPath).absolutePath());

        // --- Download single chunk ---
        qint64 bytesReceived = 0;
        bool ok = downloadPoolChunk(mgr.data(), chunk, bytesReceived);

        // Mark chunk done
        m_chunkPool[idx].completed = ok;

        if (ok) {
            if (m_fileTrackers[chunk.fileIndex].remainingChunks.deref() == 0)
                tryMergeAndFinalize(chunk.fileIndex);
        } else {
            FilePoolTracker& tracker = m_fileTrackers[chunk.fileIndex];
            if (tracker.fileFailed.testAndSetRelaxed(0, 1)) {
                m_failedCount.fetchAndAddRelaxed(1);
                {
                    QMutexLocker lock(&m_mutex);
                    const QString& name = (chunk.fileIndex < m_tasks.size())
                        ? m_tasks[chunk.fileIndex].name : QString();
                    m_failedFiles.append(name);
                }
                emit fileCompleted(
                    m_tasks[chunk.fileIndex].name, false);
                emit logMessage(QString::fromUtf8("  [失败] 下载失败: %1")
                                    .arg(m_tasks[chunk.fileIndex].name));
                tracker.finalPath.clear();
            }
        }

        // ── Speed limit throttle (v6) ──
        if (m_speedLimitMB > 0 && ok) {
            m_throttleBytes += bytesReceived;
            qint64 elapsed = m_throttleTimer.elapsed();
            if (elapsed >= 1000) {
                double currentMbps = m_throttleBytes / (1024.0 * 1024.0) / (elapsed / 1000.0);
                if (currentMbps > m_speedLimitMB) {
                    // Sleep to bring average down to limit
                    qint64 targetMs = static_cast<qint64>(
                        m_throttleBytes / (1024.0 * 1024.0) / m_speedLimitMB * 1000);
                    qint64 sleepMs = targetMs - elapsed;
                    if (sleepMs > 5)
                        QThread::msleep(qMin(sleepMs, (qint64)500));
                }
                m_throttleTimer.restart();
                m_throttleBytes = 0;
            }
        }
    }

exit_worker:

    // Last-worker-exit
    if (m_activeWorkers.deref() == 0) {
        if (m_progressTimer) m_progressTimer->stop();

        const int failed = m_failedCount.loadRelaxed();

        if (!m_cancelled) {
            const bool success = (failed == 0);
            m_state = Done;

            dequeueVersionDownload(this);
            processNextQueued();

            emit allFinished(success, failed, m_failedFiles);
            emit stateChanged();
            emit logMessage(success
                ? QString::fromUtf8("[完成] 全部完成")
                : QString::fromUtf8("[警告] %1/%2 失败")
                      .arg(failed)
                      .arg(m_tasks.size()));
        } else {
            m_state = Cancelled;

            dequeueVersionDownload(this);
            processNextQueued();

            emit logMessage(QString::fromUtf8("[失败] 下载已取消"));
            emit allFinished(false, failed, m_failedFiles);
            emit stateChanged();
        }
    }
}

// ═══════════════════════════════════════════════════════════
// Shared per-chunk download constants
// ═══════════════════════════════════════════════════════════

static constexpr int kDownloadTimeoutMs = 30000;  // per-attempt timeout
static constexpr int kMaxRetries = 3;             // max attempts per URL
static const int kRetryDelaysMs[kMaxRetries] = {500, 1500, 3000}; // progressive backoff

// ═══════════════════════════════════════════════════════════
// Enhanced: download a single chunk from global pool
// ═══════════════════════════════════════════════════════════

bool ParallelDownloader::downloadPoolChunk(QNetworkAccessManager* mgr,
                                           const PoolChunk& chunk,
                                           qint64& bytesReceived)
{
    bytesReceived = 0;

    // ── Build ordered source list based on policy ──
    QStringList orderedSources = buildOrderedSources(chunk.url, chunk.mirrors);
    if (orderedSources.isEmpty()) {
        // Fallback: try primary then mirrors (legacy behavior)
        orderedSources.append(chunk.url);
        orderedSources.append(chunk.mirrors);
    }

    // ── v6: BMCLAPI dead detection applies in PreferMirror mode ──
    bool skipBmclapi = false;
    if (m_bmclapiUnreachable.loadRelaxed()
        && m_fileSource == DownloadSourcePolicy::PreferMirror) {
        skipBmclapi = true;
    }

    // ── v6: AutoSwitch — if Mojang is confirmed slow, skip it ──
    bool skipMojang = false;
    if (m_fileSource == DownloadSourcePolicy::AutoSwitch && m_mojangSlow.loadRelaxed()) {
        skipMojang = true;
    }

    // Try each source in order
    bool ok = false;
    int sourceIdx = 0;
    for (const QString& sourceUrl : orderedSources) {
        if (m_cancelled) return false;

        // Skip BMCLAPI if dead
        if (skipBmclapi && !isMojangUrl(sourceUrl)) {
            sourceIdx++;
            continue;
        }

        // Skip Mojang if marked slow (AutoSwitch mode)
        if (skipMojang && isMojangUrl(sourceUrl)) {
            sourceIdx++;
            continue;
        }

        if (sourceIdx > 0) {
            QFile::remove(chunk.chunkPath);
            emit logMessage(QString::fromUtf8("  ↻ chunk[%1/%2] 源[%3/%4]: %5")
                                .arg(chunk.fileIndex).arg(chunk.chunkIndex)
                                .arg(sourceIdx + 1).arg(orderedSources.size())
                                .arg(sourceUrl));
            bytesReceived = 0;
        }

        ok = downloadSingleUrl(mgr, sourceUrl, chunk.chunkPath,
                                chunk.start, chunk.end, bytesReceived);
        sourceIdx++;

        if (ok) break;
    }

    if (ok) {
        // ── Track bytes for throughput (all sources count) ──
        m_windowBytes.fetchAndAddRelaxed(bytesReceived);

        // ── BMCLAPI death detection / recovery ──
        if (sourceIdx == 1 && !isMojangUrl(orderedSources.first())) {
            m_consecutiveBmclapiFails.storeRelaxed(0);
            if (m_bmclapiUnreachable.loadRelaxed()) {
                m_bmclapiUnreachable.storeRelaxed(0);
                emit logMessage(QString::fromUtf8("[切换] BMCLAPI 已恢复, 重新启用镜像"));
            }
        }

        // ── Mojang slow detection (AutoSwitch mode) ──
        if (m_fileSource == DownloadSourcePolicy::AutoSwitch) {
            if (sourceIdx == 1 && isMojangUrl(orderedSources.first())) {
                // Mojang worked first try — reset counter, recover if was marked slow
                m_consecutiveMojangSlow.storeRelaxed(0);
                if (m_mojangSlow.loadRelaxed()) {
                    m_mojangSlow.storeRelaxed(0);
                    emit logMessage(QString::fromUtf8("[切换] 官方源速度恢复, 已切回官方源"));
                }
            } else if (sourceIdx > 1) {
                // Mojang failed or was skipped — we fell back; increment counter
                int cnt = m_consecutiveMojangSlow.fetchAndAddRelaxed(1) + 1;
                if (cnt >= kMojangSlowThreshold && !m_mojangSlow.loadRelaxed()
                    && m_mojangSlow.testAndSetRelaxed(0, 1)) {
                    emit logMessage(QString::fromUtf8("[警告] 官方源速度过慢, 已切换镜像源"));
                }
            }
        }

        const int done = m_completedFiles.loadRelaxed();
        const qint64 dl = m_downloadedBytes.loadRelaxed();
        const qint64 tot = m_totalBytes.loadRelaxed();
        emit progressChanged(done, (int)m_tasks.size(), dl, tot);
    } else {
        emit logMessage(QString::fromUtf8("  [失败] chunk[%1/%2] 下载失败(所有源均失败)")
                            .arg(chunk.fileIndex).arg(chunk.chunkIndex));
    }

    return ok;
}

void ParallelDownloader::tryMergeAndFinalize(int fileIndex)
{
    if (fileIndex < 0 || fileIndex >= m_fileTrackers.size())
        return;

    FilePoolTracker& tracker = m_fileTrackers[fileIndex];
    if (tracker.finalPath.isEmpty()) return;  // already finalized
    if (tracker.fileFailed.loadRelaxed() != 0) {
        tracker.finalPath.clear();  // already failed by another worker
        return;
    }

    // ── All chunks done — merge ──
    const QString& finalPath = tracker.finalPath;

    // For single-chunk files: chunkPath IS the finalPath (no merge needed)
    if (QFileInfo::exists(finalPath)) {
        const bool sha1Ok = tracker.sha1.isEmpty() || verifySha1File(finalPath, tracker.sha1);
        const QString& taskName = (fileIndex < m_tasks.size())
                                      ? m_tasks[fileIndex].name : finalPath;
        if (sha1Ok) {
            // Strip JAR signatures to prevent SecurityException (NeoForge / signed JARs)
            if (finalPath.endsWith(QString::fromUtf8(".jar"), Qt::CaseInsensitive))
                stripJarSignatures(finalPath);

            const qint64 fileSize = QFileInfo(finalPath).size();
            m_completedFiles.fetchAndAddRelaxed(1);
            emit fileCompleted(taskName, true);
            emit logMessage(QString::fromUtf8("  [完成] 完成: %1").arg(taskName));
            emit fileProgress(QString(), taskName, fileSize, fileSize, finalPath);
            tracker.finalPath.clear();
            return;
        }
        // SHA1 mismatch — fail
        QFile::remove(finalPath);
        m_failedCount.fetchAndAddRelaxed(1);
        {
            QMutexLocker lock(&m_mutex);
            m_failedFiles.append(taskName);
        }
        emit fileCompleted(taskName, false);
        emit logMessage(QString::fromUtf8("  [失败] SHA1校验失败: %1").arg(finalPath));
        tracker.finalPath.clear();
        return;
    }

    // Multi-chunk merge
    QFile out(finalPath);
    bool mergeOk = false;
    if (out.open(QIODevice::WriteOnly)) {
        mergeOk = true;
        int chunksSeen = 0;
        for (const auto& pc : m_chunkPool) {
            if (pc.fileIndex != fileIndex || !pc.completed) continue;

            QFile in(pc.chunkPath);
            if (!in.open(QIODevice::ReadOnly)) {
                mergeOk = false;
                break;
            }

            QByteArray buf;
            buf.resize(65536);
            qint64 bytesRead;
            while ((bytesRead = in.read(buf.data(), buf.size())) > 0) {
                if (out.write(buf.constData(), bytesRead) != bytesRead) {
                    mergeOk = false;
                    break;
                }
            }
            in.close();
            if (!mergeOk) break;
            chunksSeen++;

            QFile::remove(pc.chunkPath);
        }
        out.close();

        if (!mergeOk || chunksSeen == 0) {
            QFile::remove(finalPath);
        }
    }

    const QString& taskName = (fileIndex < m_tasks.size())
                                  ? m_tasks[fileIndex].name : finalPath;
    if (mergeOk) {
        if (!tracker.sha1.isEmpty() && !verifySha1File(finalPath, tracker.sha1)) {
            mergeOk = false;
            QFile::remove(finalPath);
        }
    }

    if (mergeOk) {
        if (finalPath.endsWith(QString::fromUtf8(".jar"), Qt::CaseInsensitive))
            stripJarSignatures(finalPath);

        const qint64 fileSize = QFileInfo(finalPath).size();
        m_completedFiles.fetchAndAddRelaxed(1);
        emit fileCompleted(taskName, true);
        emit fileProgress(QString(), taskName, fileSize, fileSize, finalPath);
    } else {
        m_failedCount.fetchAndAddRelaxed(1);
        {
            QMutexLocker lock(&m_mutex);
            m_failedFiles.append(taskName);
        }
        emit fileCompleted(taskName, false);
        emit logMessage(QString::fromUtf8("  [失败] 合并失败: %1").arg(finalPath));
    }

    tracker.finalPath.clear();
}

// ═══════════════════════════════════════════════════════════
// JAR Signature Stripping (Windows-only — uses PowerShell + .NET)
// ═══════════════════════════════════════════════════════════

void ParallelDownloader::stripJarSignatures(const QString& jarPath)
{
    const QString psScript = QStringLiteral(
        "Add-Type -AssemblyName System.IO.Compression.FileSystem;"
        "$p='%1';$t=\"$p.tmp\";"
        "$s=[IO.Compression.ZipFile]::OpenRead($p);"
        "$d=[IO.Compression.ZipFile]::Open($t,[IO.Compression.ZipArchiveMode]::Create);"
        "$n=0;"
        "foreach($e in $s.Entries){"
            "if($e.Name -match '\\.(SF|RSA|DSA)$' -and $e.FullName.Replace('/','\\').StartsWith('META-INF\\')){"
                "$n++"
            "}else{"
                "$de=$d.CreateEntry($e.FullName);"
                "$de.LastWriteTime=$e.LastWriteTime;"
                "$si=$e.Open();$di=$de.Open();"
                "$si.CopyTo($di);"
                "$di.Close();$si.Close()"
            "}"
        "}"
        "$s.Dispose();$d.Dispose();"
        "[IO.File]::Delete($p);"
        "[IO.File]::Move($t,$p);"
        "Write-Output \"STRIPPED:$n\""
    ).arg(jarPath);

    QProcess proc;
    proc.start(QStringLiteral("powershell.exe"),
               {QStringLiteral("-NoProfile"),
                QStringLiteral("-Command"), psScript});

    if (!proc.waitForFinished(60000)) {
        emit logMessage(QString::fromUtf8("  [警告] JAR 签名剥离超时(60s): %1").arg(jarPath));
        return;
    }

    if (proc.exitCode() != 0) {
        const QString errMsg = QString::fromUtf8(proc.readAllStandardError());
        emit logMessage(QString::fromUtf8("  [警告] JAR 签名剥离失败(exit=%1): %2")
                            .arg(proc.exitCode()).arg(errMsg));
        return;
    }

    const QString output = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    if (output.startsWith(QStringLiteral("STRIPPED:"))) {
        int stripped = output.mid(9).toInt();
        if (stripped > 0) {
            qint64 afterSize = QFileInfo(jarPath).size();
            emit logMessage(QString::fromUtf8("  [完成] 剥离 %1 个签名 → %2")
                                .arg(stripped).arg(formatSize(afterSize)));
        }
    }
}

// ═══════════════════════════════════════════════════════════
// Adaptive Concurrency (v6 — adaptive speed floor)
//
// The principle: "speed floor" is a sticky high-water mark.
//   floor = max(256KB/s, observed_speed * 0.85)
// As speed increases, the floor rises. When current speed
// reaches the floor, we're saturated → stop adding workers.
// This prevents the TCP congestion collapse that naive
// "always add threads" approaches cause.
//
// Sources (BMCLAPI or Mojang) are treated identically for
// throughput tracking — the floor applies to all sources.
// ═══════════════════════════════════════════════════════════

void ParallelDownloader::adjustConcurrency()
{
    const qint64 elapsed = m_throughputTimer.elapsed();
    if (elapsed < 200) return;

    // ── Use m_downloadedBytes delta (continuously updated by downloadProgress signals) ──
    // m_windowBytes was only updated at chunk COMPLETION, causing huge spikes + 0 gaps.
    // m_downloadedBytes catches in-progress bytes, matching actual network speed.
    qint64 now = m_downloadedBytes.loadRelaxed();
    qint64 bytes = now - m_lastSpeedBytes;
    m_lastSpeedBytes = now;
    m_throughputTimer.restart();

    // ── Record instant speed ──
    qint64 actualBps = bytes * 1000 / elapsed;  // B/s
    {
        QMutexLocker lock(&m_speedMutex);
        m_speedRecords.prepend(actualBps);
        if (m_speedRecords.size() > kMaxSpeedRecords)
            m_speedRecords.removeLast();
    }

    // ── Display speed: recency-weighted average ──
    qint64 weightedSum = 0;
    int weightDiv = 0;
    {
        QMutexLocker lock(&m_speedMutex);
        int weight = m_speedRecords.size();
        for (auto rec : m_speedRecords) {
            weightedSum += rec * weight;
            weightDiv += weight;
            weight--;
        }
    }
    qint64 currentBps = (weightDiv > 0) ? (weightedSum / weightDiv) : actualBps;
    double displayMbps = currentBps / (1024.0 * 1024.0);

    // Update EMA for legacy/log
    m_emaMbps = m_emaMbps * 0.5 + displayMbps * 0.5;

    // ── Speed floor: weighted speed × 85% ──
    qint64 floorLimit = static_cast<qint64>(currentBps * 0.85);
    qint64 currentFloor = m_speedFloorBps.loadRelaxed();
    if (currentBps >= kMinSpeedFloorBps && floorLimit > currentFloor) {
        m_speedFloorBps.storeRelaxed(floorLimit);
        emit logMessage(QString::fromUtf8("[统计] 速度下限提升: %1 → %2")
                            .arg(formatSize(currentFloor) + QStringLiteral("/s"))
                            .arg(formatSize(floorLimit) + QStringLiteral("/s")));
    }

    int currentWorkers = m_maxWorkers.loadRelaxed();

    // ── Saturation check ──
    if (currentBps >= m_speedFloorBps.loadRelaxed()) {
        if (displayMbps > m_peakMbps) m_peakMbps = displayMbps;
        return;
    }

    // ── Not saturated → add workers (up to user ceiling) ──
    int effectiveMax = qMin(m_userMaxWorkers, kMaxWorkers);
    if (currentWorkers < effectiveMax) {
        int toSpawn = qMin(8, effectiveMax - currentWorkers);
        m_maxWorkers.storeRelaxed(currentWorkers + toSpawn);
        for (int i = 0; i < toSpawn; ++i) {
            m_activeWorkers.ref();
            QThread* worker = QThread::create([this]() { runWorker(); });
            m_workers.append(worker);
            worker->start();
        }
        if (displayMbps > m_peakMbps) m_peakMbps = displayMbps;
        emit logMessage(QString::fromUtf8("[优化] 提速: +%1 workers → %2 (%3 MB/s, 下限 %4)")
                            .arg(toSpawn).arg(m_maxWorkers.loadRelaxed())
                            .arg(QString::number(displayMbps, 'f', 1))
                            .arg(QString::number(m_speedFloorBps.loadRelaxed() / (1024.0 * 1024.0), 'f', 2) + QStringLiteral(" MB/s")));
    }
}

// ═══════════════════════════════════════════════════════════

bool ParallelDownloader::downloadSingleUrl(QNetworkAccessManager* mgr, const QString& url,
                                            const QString& destPath,
                                            qint64 rangeStart,
                                            qint64 rangeEnd,
                                            qint64& bytesReceived)
{
    bytesReceived = 0;

    for (int attempt = 1; attempt <= kMaxRetries; ++attempt) {
        if (m_cancelled) return false;

        // Pause checkpoint
        {
            QMutexLocker lock(&m_mutex);
            while (m_paused && !m_cancelled)
                m_pauseCond.wait(&m_mutex);
        }
        if (m_cancelled) return false;

        // --- Core download attempt ---
        QUrl qurl(url);
        QNetworkRequest request(qurl);
        {
            QHttp1Configuration h1cfg;
            h1cfg.setNumberOfConnectionsPerHost(255);
            request.setHttp1Configuration(h1cfg);
        }
        request.setRawHeader("User-Agent", "ShadowLauncher/1.0");
        request.setRawHeader("Connection", "Keep-Alive");
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);
        request.setTransferTimeout(kDownloadTimeoutMs);

        // HTTP Range header for chunked download
        if (rangeStart > 0 || rangeEnd >= 0) {
            QByteArray rangeHeader = QStringLiteral("bytes=%1-")
                                         .arg(rangeStart).toUtf8();
            if (rangeEnd >= 0)
                rangeHeader += QString::number(rangeEnd).toUtf8();
            request.setRawHeader("Range", rangeHeader);
        }

        QNetworkReply* reply = mgr->get(request);
        if (!reply) {
            if (attempt < kMaxRetries)
                QThread::msleep(kRetryDelaysMs[attempt - 1]);
            continue;
        }

        QFile file(destPath);
        if (!file.open(QIODevice::WriteOnly)) {
            reply->deleteLater();
            if (attempt < kMaxRetries)
                QThread::msleep(kRetryDelaysMs[attempt - 1]);
            continue;
        }

        QEventLoop loop;
        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);

        // Periodic cancel/pause poll
        QTimer cancelTimer;
        connect(&cancelTimer, &QTimer::timeout, [&]() {
            if (m_cancelled) {
                loop.exit();
                return;
            }
            if (m_paused) {
                QMutexLocker lock(&m_mutex);
                while (m_paused && !m_cancelled)
                    m_pauseCond.wait(&m_mutex);
            }
        });
        cancelTimer.start(200);

        // Data arrival
        connect(reply, &QNetworkReply::readyRead, [&]() {
            file.write(reply->readAll());
            timeoutTimer.start(kDownloadTimeoutMs);
        });

        // Progress tracking — also feed m_downloadedBytes for speed display
        connect(reply, &QNetworkReply::downloadProgress,
                [&](qint64 received, qint64 /*total*/) {
            qint64 delta = received - bytesReceived;
            if (delta > 0) {
                m_downloadedBytes.fetchAndAddRelaxed(delta);
            }
            bytesReceived = received;
        });

        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        connect(&timeoutTimer, &QTimer::timeout, &loop, [&]() {
            reply->abort();
            loop.quit();
        });

        timeoutTimer.start(kDownloadTimeoutMs);
        loop.exec();

        cancelTimer.stop();
        file.close();

        if (m_cancelled) {
            reply->deleteLater();
            return false;
        }

        const int statusCode =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QNetworkReply::NetworkError netErr = reply->error();
        reply->deleteLater();

        // Accept 200 (full file) or 206 (partial content for Range requests)
        bool ok = false;
        if (rangeStart > 0 || rangeEnd >= 0) {
            ok = (statusCode == 206 || statusCode == 200)
                 && netErr == QNetworkReply::NoError;
        } else {
            ok = (statusCode == 200)
                 && netErr == QNetworkReply::NoError;
        }

        if (ok) return true;

        // Clean up failed attempt
        QFile::remove(destPath);

        // Don't retry on 404 (file genuinely missing on this server)
        if (statusCode == 404) return false;

        // Retry with backoff
        if (attempt < kMaxRetries) {
            bytesReceived = 0;
            QThread::msleep(kRetryDelaysMs[attempt - 1]);
        }
    }

    bytesReceived = 0;
    return false;
}

// ═══════════════════════════════════════════════════════════
// SHA1 helpers
// ═══════════════════════════════════════════════════════════

QString ParallelDownloader::sha1Hex(const QString& filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(&f);
    return QString::fromLatin1(hash.result().toHex());
}

bool ParallelDownloader::verifySha1File(const QString& filePath,
                                         const QString& expected)
{
    const QString actual = sha1Hex(filePath);
    if (actual.isEmpty()) return false;
    return actual.compare(expected, Qt::CaseInsensitive) == 0;
}

// ═══════════════════════════════════════════════════════════
// Formatting
// ═══════════════════════════════════════════════════════════

QString ParallelDownloader::formatSize(qint64 bytes)
{
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    return QStringLiteral("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
}

} // namespace ShadowLauncher
