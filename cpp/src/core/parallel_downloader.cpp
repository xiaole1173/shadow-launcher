// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow
// Shadow Launcher — Parallel download engine v3
// Chunked multi-source segmented download with:
//  - HTTP Range requests across mirrors for large files (>1MB)
//  - Direct single-URL for small files
//  - Checkpoint resume via .download_progress.json
//  - Pause/resume preserving .tmp chunks
//  - Version-level concurrency queue (max 3)

#include "parallel_downloader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QEventLoop>
#include <QTimer>
#include <QThread>

namespace ShadowLauncher {

// Static queue members
QMutex ParallelDownloader::s_queueMutex;
QQueue<ParallelDownloader*> ParallelDownloader::s_versionQueue;
int ParallelDownloader::s_activeVersionCount = 0;

// ═══════════════════════════════════════════════════════════
// Construction / Destruction
// ═══════════════════════════════════════════════════════════

ParallelDownloader::ParallelDownloader(int maxWorkers, QObject* parent)
    : QObject(parent)
    , m_maxWorkers(qBound(1, maxWorkers, 32))
{
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
// Checkpoint dir
// ═══════════════════════════════════════════════════════════

void ParallelDownloader::setCheckpointDir(const QString& dir)
{
    m_checkpointDir = dir;
    if (!dir.isEmpty())
        QDir().mkpath(dir);
}

QString ParallelDownloader::checkpointDir() const
{
    return m_checkpointDir;
}

bool ParallelDownloader::hasResumeCheckpoint(const QString& taskName) const
{
    return QFileInfo::exists(checkpointPath(taskName));
}

// ═══════════════════════════════════════════════════════════
// Checkpoint helpers
// ═══════════════════════════════════════════════════════════

QString ParallelDownloader::checkpointPath(const QString& taskName) const
{
    if (m_checkpointDir.isEmpty())
        return QString();
    return m_checkpointDir + QStringLiteral("/") + taskName
           + QStringLiteral(".checkpoint.json");
}

void ParallelDownloader::saveCheckpoint(const QString& taskName,
                                         const QVector<ChunkInfo>& chunks)
{
    if (m_checkpointDir.isEmpty()) return;

    QJsonArray arr;
    for (const auto& c : chunks) {
        QJsonObject obj;
        obj[QStringLiteral("index")]     = c.index;
        obj[QStringLiteral("start")]     = c.start;
        obj[QStringLiteral("end")]       = c.end;
        obj[QStringLiteral("mirrorUrl")] = c.mirrorUrl;
        obj[QStringLiteral("completed")] = c.completed;
        arr.append(obj);
    }

    QJsonObject root;
    root[QStringLiteral("chunks")] = arr;

    QFile f(checkpointPath(taskName));
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        f.close();
    }
}

QVector<ParallelDownloader::ChunkInfo>
ParallelDownloader::loadCheckpoint(const QString& taskName) const
{
    QVector<ChunkInfo> result;
    QString path = checkpointPath(taskName);
    if (path.isEmpty() || !QFileInfo::exists(path))
        return result;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return result;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();

    if (!doc.isObject()) return result;
    QJsonArray arr = doc.object().value(QStringLiteral("chunks")).toArray();
    for (const auto& val : arr) {
        QJsonObject obj = val.toObject();
        ChunkInfo ci;
        ci.index     = obj.value(QStringLiteral("index")).toInt();
        ci.start     = static_cast<qint64>(obj.value(QStringLiteral("start")).toDouble());
        ci.end       = static_cast<qint64>(obj.value(QStringLiteral("end")).toDouble());
        ci.mirrorUrl = obj.value(QStringLiteral("mirrorUrl")).toString();
        ci.completed = obj.value(QStringLiteral("completed")).toBool();
        result.append(ci);
    }
    return result;
}

void ParallelDownloader::clearCheckpoint(const QString& taskName)
{
    QFile::remove(checkpointPath(taskName));
}

// ═══════════════════════════════════════════════════════════
// Chunking logic: split file >1MB into chunks across mirrors
// ═══════════════════════════════════════════════════════════

QVector<ParallelDownloader::ChunkInfo>
ParallelDownloader::splitIntoChunks(const DownloadTask& task) const
{
    QVector<ChunkInfo> chunks;

    // Small files: one chunk with primary URL (use first mirror=mojang if available)
    if (task.totalBytes <= kChunkThreshold) {
        ChunkInfo ci;
        ci.index       = 0;
        ci.start       = 0;
        ci.end         = -1;  // entire file
        ci.mirrorUrl   = task.url;
        ci.completed   = false;
        chunks.append(ci);
        return chunks;
    }

    // Large file: split into N chunks across all available URLs
    // Build all available URLs (primary + mirrors)
    QStringList allUrls;
    allUrls.append(task.url);
    for (const QString& m : task.mirrors) {
        if (!allUrls.contains(m))
            allUrls.append(m);
    }

    if (allUrls.isEmpty()) {
        // Fallback: single URL
        ChunkInfo ci;
        ci.index       = 0;
        ci.start       = 0;
        ci.end         = -1;
        ci.mirrorUrl   = task.url;
        ci.completed   = false;
        chunks.append(ci);
        return chunks;
    }

    // Use number of sources as chunk count (max 8 chunks)
    const int numChunks = qMin(allUrls.size(), 8);
    const qint64 totalSize = (task.totalBytes > 0)
                             ? task.totalBytes
                             : kChunkThreshold * 4; // fallback estimate
    const qint64 chunkSize = qMax(totalSize / numChunks, (qint64)(64 * 1024)); // min 64KB per chunk

    // Distribute work: most traffic to BMCLAPI-like (first in list),
    // but spread across all mirrors.
    // Rotate through URLs so each chunk gets a different mirror.
    for (int i = 0; i < numChunks; ++i) {
        const qint64 start = i * chunkSize;
        qint64 end = (i == numChunks - 1) ? -1 : (start + chunkSize - 1);

        // Pick mirror: cycle through sources, preferring BMCLAPI-like for odd chunks
        int urlIdx;
        if (i % 2 == 0) {
            // First mirror gets more load (BMCLAPI)
            urlIdx = 0;
        } else {
            // Rotate through remaining mirrors
            urlIdx = qMin(i % (allUrls.size() - 1) + 1, allUrls.size() - 1);
        }

        // Clamp
        if (urlIdx >= allUrls.size()) urlIdx = 0;

        ChunkInfo ci;
        ci.index       = i;
        ci.start       = start;
        ci.end         = end;
        ci.mirrorUrl   = allUrls[urlIdx];
        ci.completed   = false;
        chunks.append(ci);
    }

    return chunks;
}

// ═══════════════════════════════════════════════════════════
// Merge .tmp chunks → final file
// ═══════════════════════════════════════════════════════════

bool ParallelDownloader::mergeChunks(const QString& finalPath,
                                      const QVector<ChunkInfo>& chunks)
{
    QFile out(finalPath);
    if (!out.open(QIODevice::WriteOnly)) {
        return false;
    }

    for (const auto& ci : chunks) {
        if (!ci.completed) {
            out.close();
            QFile::remove(finalPath);
            return false;
        }

        QString chunkPath = finalPath + QStringLiteral(".chunk.")
                            + QString::number(ci.index);
        QFile in(chunkPath);
        if (!in.open(QIODevice::ReadOnly)) {
            out.close();
            QFile::remove(finalPath);
            return false;
        }

        // Stream copy in 64KB blocks to avoid memory blowout on large files
        QByteArray buf;
        buf.resize(65536);
        qint64 bytesRead;
        while ((bytesRead = in.read(buf.data(), buf.size())) > 0) {
            if (out.write(buf.constData(), bytesRead) != bytesRead) {
                in.close();
                out.close();
                QFile::remove(finalPath);
                return false;
            }
        }
        in.close();

        // Clean up chunk tmp
        QFile::remove(chunkPath);
    }

    out.close();
    return true;
}

// ═══════════════════════════════════════════════════════════
// Lifecycle
// ═══════════════════════════════════════════════════════════

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
    m_state = Running;

    // Estimate total bytes
    qint64 totalEstimate = 0;
    {
        QMutexLocker lock(&m_mutex);
        for (const auto& t : m_tasks)
            totalEstimate += t.totalBytes;
    }
    m_totalBytes.storeRelaxed(totalEstimate);

    emit logMessage(QString::fromUtf8("准备下载 %1 个文件, 预估 %2")
                        .arg(m_tasks.size())
                        .arg(formatSize(totalEstimate)));
    emit stateChanged();

    if (m_tasks.isEmpty()) {
        m_state = Done;
        emit allFinished(true, 0, QStringList());
        emit stateChanged();
        return;
    }

    // Spawn workers
    const int workers = qMin(m_maxWorkers, (int)m_tasks.size());
    for (int i = 0; i < workers; ++i) {
        m_activeWorkers.ref();
        QThread* worker = QThread::create([this]() { runWorker(); });
        m_workers.append(worker);
        worker->start();
    }

    // Start periodic progress emission (200ms throttle, Layer ① anti-jitter)
    if (!m_progressTimer) {
        m_progressTimer = new QTimer(this);
        connect(m_progressTimer, &QTimer::timeout, this, [this]() {
            if (m_state != Running) return;
            emit progressChanged(m_completedFiles.loadRelaxed(),
                                 m_tasks.size(),
                                 m_downloadedBytes.loadRelaxed(),
                                 m_totalBytes.loadRelaxed());
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
    emit logMessage(QString::fromUtf8("⏸ 下载已暂停"));
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
    emit logMessage(QString::fromUtf8("✕ 下载已取消"));
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
        // Slot available — run immediately
        s_activeVersionCount++;
        instance->m_queuedAndReady = true;
        return true;
    }
    // Queue full — append to queue
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

    // Wake the instance's start logic (the instance itself needs to react)
    // Since we're in a static context, send a meta-call via QMetaObject
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
    while (!m_cancelled) {
        // --- Pause checkpoint ---
        {
            QMutexLocker lock(&m_mutex);
            while (m_paused && !m_cancelled) {
                m_pauseCond.wait(&m_mutex);
            }
        }
        if (m_cancelled) break;

        // --- Dequeue next task index ---
        int idx = m_nextTask.fetchAndAddRelaxed(1);

        int taskCount;
        {
            QMutexLocker lock(&m_mutex);
            taskCount = m_tasks.size();
        }
        if (idx >= taskCount) break;

        // --- Copy task ---
        DownloadTask task;
        {
            QMutexLocker lock(&m_mutex);
            if (idx >= m_tasks.size()) break;
            task = m_tasks[idx];
        }

        // --- Download using chunked segmented approach ---
        bool ok = downloadSingleFile(task);

        // Update counters
        m_completedFiles.fetchAndAddRelaxed(1);
        if (!ok) {
            m_failedCount.fetchAndAddRelaxed(1);
            // Thread-safe append to failed files list
            {
                QMutexLocker lock(&m_mutex);
                m_failedFiles.append(task.name);
            }
        }

        emit fileCompleted(task.name, ok);

        // Aggregate progress
        const int completed  = m_completedFiles.loadRelaxed();
        const qint64 dlBytes = m_downloadedBytes.loadRelaxed();
        const qint64 totBytes = m_totalBytes.loadRelaxed();

        int total;
        {
            QMutexLocker lock(&m_mutex);
            total = m_tasks.size();
        }
        emit progressChanged(completed, total, dlBytes, totBytes);
    }

    // Last-worker-exit
    if (m_activeWorkers.deref() == 0) {
        // Stop periodic progress timer
        if (m_progressTimer) m_progressTimer->stop();

        if (!m_cancelled) {
            const int failed = m_failedCount.loadRelaxed();
            const bool success = (failed == 0);
            m_state = Done;

            // Notify version-level queue that we're done
            dequeueVersionDownload(this);
            processNextQueued();

            emit allFinished(success, failed, m_failedFiles);
            emit stateChanged();
            emit logMessage(success
                ? QString::fromUtf8("✅ 全部完成")
                : QString::fromUtf8("⚠ %1/%2 失败")
                      .arg(failed)
                      .arg(m_tasks.size()));
        }
    }
}

// ═══════════════════════════════════════════════════════════
// Single-file download — chunked multi-source segmented
// ═══════════════════════════════════════════════════════════

bool ParallelDownloader::downloadSingleFile(const DownloadTask& task)
{
    const QString taskName = task.name;
    const QString finalPath = task.savePath;

    // ------------------------------------------------------------------
    // Early-exit: local file exists with matching SHA1
    // ------------------------------------------------------------------
    if (QFileInfo::exists(finalPath) && !task.sha1.isEmpty()) {
        if (verifySha1File(finalPath, task.sha1)) {
            const qint64 fileSize = QFileInfo(finalPath).size();
            m_downloadedBytes.fetchAndAddRelaxed(fileSize);
            emit fileProgress(task.url, taskName, fileSize, fileSize);
            emit logMessage(QString::fromUtf8("⊘ 跳过(已存在SHA1): %1").arg(taskName));
            return true;
        }
        // SHA1 mismatch
        QFile::remove(finalPath);
        emit logMessage(QString::fromUtf8("⚠ SHA1不匹配,重新下载: %1").arg(taskName));
    } else if (QFileInfo::exists(finalPath) && task.sha1.isEmpty()) {
        // No SHA1, file exists — assume complete
        const qint64 fileSize = QFileInfo(finalPath).size();
        m_downloadedBytes.fetchAndAddRelaxed(fileSize);
        emit fileProgress(task.url, taskName, fileSize, fileSize);
        emit logMessage(QString::fromUtf8("⊘ 跳过(已存在): %1").arg(taskName));
        return true;
    }

    // Ensure target directory exists
    QDir().mkpath(QFileInfo(finalPath).absolutePath());

    // ------------------------------------------------------------------
    // Get or create chunk plan
    // ------------------------------------------------------------------
    QVector<ChunkInfo> chunks;

    if (hasResumeCheckpoint(taskName)) {
        // Resume from checkpoint — reload chunk state
        chunks = loadCheckpoint(taskName);
        if (chunks.isEmpty()) {
            // Corrupted checkpoint — start fresh
            QFile::remove(checkpointPath(taskName));
        }
        emit logMessage(QString::fromUtf8("  ↻ 断点续传: %1").arg(taskName));
    }

    if (chunks.isEmpty()) {
        // Fresh download — build chunk plan
        chunks = splitIntoChunks(task);
        saveCheckpoint(taskName, chunks);
    }

    // ------------------------------------------------------------------
    // Download each chunk (skip completed ones)
    // ------------------------------------------------------------------
    bool allChunksOk = true;
    for (int i = 0; i < chunks.size(); ++i) {
        if (m_cancelled) return false;

        // Pause checkpoint
        {
            QMutexLocker lock(&m_mutex);
            while (m_paused && !m_cancelled)
                m_pauseCond.wait(&m_mutex);
        }
        if (m_cancelled) return false;

        ChunkInfo& ci = chunks[i];

        // Skip already-completed chunks (resume support)
        if (ci.completed)
            continue;

        QString chunkPath = finalPath + QStringLiteral(".chunk.")
                            + QString::number(ci.index);

        emit logMessage(QString::fromUtf8("  → 块%1/%2 [%3-%4]: %5")
                            .arg(ci.index + 1)
                            .arg(chunks.size())
                            .arg(ci.start)
                            .arg(ci.end < 0 ? QStringLiteral("END") : QString::number(ci.end))
                            .arg(ci.mirrorUrl));

        qint64 bytesReceived = 0;
        bool ok = downloadSingleUrl(ci.mirrorUrl, chunkPath,
                                      ci.start, ci.end,
                                      bytesReceived);

        // Mirror fallback: if primary URL fails, try mirrors from task
        int mirrorIdx = 0;
        while (!ok && mirrorIdx < task.mirrors.size()) {
            const QString& mirrorUrl = task.mirrors[mirrorIdx];
            // Skip if same as already-tried primary URL
            if (mirrorUrl != ci.mirrorUrl) {
                QFile::remove(chunkPath); // clean failed partial
                emit logMessage(QString::fromUtf8("  ↻ 回退镜像[%1]: %2")
                                    .arg(mirrorIdx + 1)
                                    .arg(mirrorUrl));
                bytesReceived = 0;
                ok = downloadSingleUrl(mirrorUrl, chunkPath,
                                        ci.start, ci.end,
                                        bytesReceived);
            }
            mirrorIdx++;
        }

        if (ok && QFileInfo::exists(chunkPath)) {
            ci.completed = true;
            m_downloadedBytes.fetchAndAddRelaxed(bytesReceived);

            // Update checkpoint after each chunk
            saveCheckpoint(taskName, chunks);
        } else {
            allChunksOk = false;
            QFile::remove(chunkPath);
            emit logMessage(QString::fromUtf8("  ✗ 块%1 下载失败(所有镜像均失败): %2")
                                .arg(ci.index + 1)
                                .arg(ci.mirrorUrl));
            // Don't break — try remaining chunks from different mirrors
        }
    }

    if (!allChunksOk) {
        // Some chunks failed; keep checkpoint for resume
        emit logMessage(QString::fromUtf8("  ✗ 部分块失败: %1").arg(taskName));
        // Remove any orphan chunk tmp files
        for (const auto& ci : chunks) {
            if (!ci.completed) {
                QString cp = finalPath + QStringLiteral(".chunk.") + QString::number(ci.index);
                QFile::remove(cp);
            }
        }
        return false;
    }

    // ------------------------------------------------------------------
    // All chunks complete — merge
    // ------------------------------------------------------------------
    emit logMessage(QString::fromUtf8("  🔗 合并块: %1").arg(taskName));

    QFile::remove(finalPath); // clean slate
    if (!mergeChunks(finalPath, chunks)) {
        emit logMessage(QString::fromUtf8("  ✗ 合并失败: %1").arg(taskName));
        return false;
    }

    // ------------------------------------------------------------------
    // SHA1 verification
    // ------------------------------------------------------------------
    if (!task.sha1.isEmpty()) {
        if (!verifySha1File(finalPath, task.sha1)) {
            QFile::remove(finalPath);
            emit logMessage(QString::fromUtf8("  ✗ SHA1不匹配: %1").arg(taskName));
            // Keep checkpoint for resume? No — SHA1 mismatch means data is bad.
            clearCheckpoint(taskName);
            return false;
        }
    }

    // Success — clear checkpoint, report progress
    clearCheckpoint(taskName);

    const qint64 fileSize = QFileInfo(finalPath).size();
    m_downloadedBytes.fetchAndAddRelaxed(fileSize);
    emit fileProgress(task.url, taskName, fileSize, fileSize);
    emit logMessage(QString::fromUtf8("  ✓ 完成: %1").arg(taskName));
    return true;
}

// ═══════════════════════════════════════════════════════════
// Single-URL download with HTTP Range support + retry
// ═══════════════════════════════════════════════════════════

static constexpr int kDownloadTimeoutMs = 30000;  // per-attempt timeout
static constexpr int kMaxRetries = 3;             // max attempts per URL
static const int kRetryDelaysMs[kMaxRetries] = {500, 1500, 3000}; // progressive backoff

bool ParallelDownloader::downloadSingleUrl(const QString& url,
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
        QNetworkAccessManager mgr;
        QUrl qurl(url);
        QNetworkRequest request(qurl);
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

        QNetworkReply* reply = mgr.get(request);
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
                reply->abort();
                loop.quit();
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

        // Progress tracking
        connect(reply, &QNetworkReply::downloadProgress,
                [&](qint64 received, qint64 /*total*/) {
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
