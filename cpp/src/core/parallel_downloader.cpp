// Shadow Launcher — Parallel download engine
// Concurrent multi-file downloader with SHA1 verification,
// mirror racing, pause/resume/cancel support.

#include "parallel_downloader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QEventLoop>
#include <QTimer>
#include <QThread>

namespace ShadowLauncher {

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
    // Signal threads to stop
    m_cancelled = true;
    m_state = Cancelled;
    {
        QMutexLocker lock(&m_mutex);
        m_paused = false;
        m_pauseCond.wakeAll();
    }

    // Join all workers with generous timeout
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
    // Tasks can only be added before start()
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
// Lifecycle
// ═══════════════════════════════════════════════════════════

void ParallelDownloader::start()
{
    if (m_state == Running || m_state == Paused) return;

    // Clean up any completed tasks from a previous run
    {
        QMutexLocker lock(&m_mutex);
        m_tasks.erase(
            std::remove_if(m_tasks.begin(), m_tasks.end(),
                           [](const DownloadTask& t) { return t.completed; }),
            m_tasks.end());
    }

    // Reset all state
    m_cancelled = false;
    m_paused = false;
    m_completedFiles.storeRelaxed(0);
    m_failedCount.storeRelaxed(0);
    m_downloadedBytes.storeRelaxed(0);
    m_nextTask.storeRelaxed(0);
    m_state = Running;

    // Estimate total bytes from task size hints
    qint64 totalEstimate = 0;
    {
        QMutexLocker lock(&m_mutex);
        for (const auto& t : m_tasks) {
            totalEstimate += t.totalBytes;
        }
    }
    m_totalBytes.storeRelaxed(static_cast<int>(totalEstimate));

    const int totalFiles = m_tasks.size();

    emit logMessage(QString::fromUtf8("准备下载 %1 个文件, 预估 %2")
                        .arg(totalFiles)
                        .arg(formatSize(totalEstimate)));
    emit stateChanged();

    if (totalFiles == 0) {
        m_state = Done;
        emit allFinished(true, 0);
        emit stateChanged();
        return;
    }

    // Spawn worker threads (one fewer than tasks if tasks < maxWorkers)
    const int workers = qMin(m_maxWorkers, totalFiles);
    for (int i = 0; i < workers; ++i) {
        m_activeWorkers.ref();
        QThread* worker = QThread::create([this]() { runWorker(); });
        m_workers.append(worker);
        worker->start();
    }
}

void ParallelDownloader::pause()
{
    if (m_state != Running) return;
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
    emit logMessage(QString::fromUtf8("▶ 下载已恢复"));
    emit stateChanged();
}

void ParallelDownloader::cancel()
{
    if (m_state != Running && m_state != Paused) return;
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
    // m_tasks is only written before start(); safe to read without lock
    // during the run phase, but take the lock for strict correctness.
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

        // --- Dequeue next task index (atomic) ---
        int idx = m_nextTask.fetchAndAddRelaxed(1);

        // Guard: size may have been read before lock in a later iteration
        int taskCount;
        {
            QMutexLocker lock(&m_mutex);
            taskCount = m_tasks.size();
        }
        if (idx >= taskCount) break;

        // --- Copy task under lock ---
        DownloadTask task;
        {
            QMutexLocker lock(&m_mutex);
            if (idx >= m_tasks.size()) break;   // double-check
            task = m_tasks[idx];
        }

        // --- Download ---
        bool ok = downloadSingleFile(task);

        // Update global counters
        m_completedFiles.fetchAndAddRelaxed(1);
        if (!ok) {
            m_failedCount.fetchAndAddRelaxed(1);
        }

        // Per-file completion signal (thread-safe cross-thread emission)
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

    // --- Last-worker-exit detection ---
    if (m_activeWorkers.deref() == 0) {
        if (!m_cancelled) {
            const int failed = m_failedCount.loadRelaxed();
            const bool success = (failed == 0);
            m_state = Done;

            emit allFinished(success, failed);
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
// Single-file download with mirror fallback
// ═══════════════════════════════════════════════════════════

bool ParallelDownloader::downloadSingleFile(const DownloadTask& task)
{
    // ------------------------------------------------------------------
    // Early-exit: local file already exists with matching SHA1
    // ------------------------------------------------------------------
    if (QFileInfo::exists(task.savePath) && !task.sha1.isEmpty()) {
        if (verifySha1File(task.savePath, task.sha1)) {
            const qint64 fileSize = QFileInfo(task.savePath).size();
            m_downloadedBytes.fetchAndAddRelaxed(static_cast<int>(fileSize));
            emit fileProgress(task.name, fileSize, fileSize);
            emit logMessage(QString::fromUtf8("⊘ 跳过(已存在): %1").arg(task.name));
            return true;
        }
        // SHA1 mismatch — remove the stale file
        QFile::remove(task.savePath);
        emit logMessage(QString::fromUtf8("⚠ SHA1不匹配,重新下载: %1").arg(task.name));
    } else if (QFileInfo::exists(task.savePath) && task.sha1.isEmpty()) {
        // No SHA1 constraint but file exists — assume complete
        const qint64 fileSize = QFileInfo(task.savePath).size();
        m_downloadedBytes.fetchAndAddRelaxed(static_cast<int>(fileSize));
        emit fileProgress(task.name, fileSize, fileSize);
        emit logMessage(QString::fromUtf8("⊘ 跳过(已存在): %1").arg(task.name));
        return true;
    }

    // Ensure target directory exists
    QDir().mkpath(QFileInfo(task.savePath).absolutePath());

    // ------------------------------------------------------------------
    // Build URL list: primary URL + mirrors (deduplicated, order-preserving)
    // ------------------------------------------------------------------
    QStringList urls;
    urls.append(task.url);
    for (const QString& m : task.mirrors) {
        if (!urls.contains(m)) {
            urls.append(m);
        }
    }

    // ------------------------------------------------------------------
    // Phase 1: Mirror race — try first 2 URLs in parallel
    // ------------------------------------------------------------------
    if (urls.size() >= 2) {
        emit logMessage(QString::fromUtf8("  ⚡ 镜像竞速: %1").arg(task.name));
        if (downloadMirrorRace(urls.mid(0, 2), task.savePath, task.sha1)) {
            const qint64 fileSize = QFileInfo(task.savePath).size();
            m_downloadedBytes.fetchAndAddRelaxed(static_cast<int>(fileSize));
            emit fileProgress(task.name, fileSize, fileSize);
            return true;
        }
        urls = urls.mid(2);   // both failed — try the rest
    }

    // ------------------------------------------------------------------
    // Phase 2: Sequential fallback for remaining URLs
    // ------------------------------------------------------------------
    for (const QString& url : urls) {
        if (m_cancelled) return false;

        emit logMessage(QString::fromUtf8("  → 尝试: %1").arg(url));

        const QString tmpPath = task.savePath + QStringLiteral(".tmp");
        QFile::remove(tmpPath);

        qint64 bytesReceived = 0;
        const bool ok = downloadSingleUrl(url, tmpPath, bytesReceived);

        if (ok && QFileInfo::exists(tmpPath)) {
            // SHA1 verification
            if (!task.sha1.isEmpty()) {
                if (!verifySha1File(tmpPath, task.sha1)) {
                    QFile::remove(tmpPath);
                    emit logMessage(QString::fromUtf8("  ✗ SHA1不匹配: %1").arg(url));
                    continue;
                }
            }

            // Atomic rename tmp → final
            QFile::remove(task.savePath);
            if (QFile::rename(tmpPath, task.savePath)) {
                const qint64 fileSize = QFileInfo(task.savePath).size();
                m_downloadedBytes.fetchAndAddRelaxed(static_cast<int>(fileSize));
                emit fileProgress(task.name, fileSize, fileSize);
                return true;
            }
        }

        QFile::remove(tmpPath);   // failed download cleanup
    }

    return false;
}

// ═══════════════════════════════════════════════════════════
// Single-URL download (blocking — runs inside a worker thread)
// ═══════════════════════════════════════════════════════════

bool ParallelDownloader::downloadSingleUrl(const QString& url,
                                            const QString& destPath,
                                            qint64& bytesReceived)
{
    QNetworkAccessManager mgr;          // safe: created in the calling thread
    QUrl qurl(url);
    QNetworkRequest request(qurl);
    request.setRawHeader("User-Agent", "ShadowLauncher/1.0");
    request.setRawHeader("Connection", "Keep-Alive");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);
    request.setTransferTimeout(60000);  // 60s total transfer timeout

    QNetworkReply* reply = mgr.get(request);
    if (!reply) return false;

    QFile file(destPath);
    if (!file.open(QIODevice::WriteOnly)) {
        reply->deleteLater();
        return false;
    }

    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);

    // Periodic cancel/pause poll (runs in this worker thread)
    QTimer cancelTimer;
    connect(&cancelTimer, &QTimer::timeout, [&]() {
        if (m_cancelled) {
            reply->abort();
            loop.quit();
            return;
        }
        // Honour pause during download
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
        timeoutTimer.start(30000);        // reset stall-watchdog on every chunk
    });

    // Download progress (omit context object so it fires on worker thread directly)
    connect(reply, &QNetworkReply::downloadProgress,
            [&](qint64 received, qint64 /*total*/) {
        bytesReceived = received;
    });

    // Completion handlers
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&timeoutTimer, &QTimer::timeout, &loop, [&]() {
        reply->abort();
        loop.quit();
    });

    timeoutTimer.start(30000);
    loop.exec();               // block until download finishes, times out, or cancelled

    cancelTimer.stop();
    file.close();

    // --- Check result ---
    const int statusCode =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError netErr = reply->error();
    reply->deleteLater();

    if (netErr != QNetworkReply::NoError || statusCode >= 400) {
        QFile::remove(destPath);
        return false;
    }

    return true;
}

// ═══════════════════════════════════════════════════════════
// Mirror race: first-2-URLs parallel, first success wins
// ═══════════════════════════════════════════════════════════

bool ParallelDownloader::downloadMirrorRace(const QStringList& urls,
                                             const QString& destPath,
                                             const QString& sha1)
{
    if (urls.size() < 2) return false;

    // Shared state between the two racing threads
    struct RaceState {
        QAtomicInt winner{-1};       // -1 = no winner declared yet, 0/1 = winner index
    };
    RaceState state;
    const QString tmpPath0 = destPath + QStringLiteral(".par.0");
    const QString tmpPath1 = destPath + QStringLiteral(".par.1");

    // Each racing thread runs this lambda
    auto tryUrl = [&](const QString& url, const QString& tmpPath, int idx) {
        // Someone already won — bail out early
        if (state.winner.loadAcquire() >= 0) return;

        QNetworkAccessManager mgr;           // created in this racing thread
        QUrl qurl(url);
        QNetworkRequest request(qurl);
        request.setRawHeader("User-Agent", "ShadowLauncher/1.0");
        request.setRawHeader("Connection", "Keep-Alive");
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);
        request.setTransferTimeout(60000);  // 60s total transfer timeout

        QNetworkReply* reply = mgr.get(request);
        if (!reply) return;

        QFile file(tmpPath);
        if (!file.open(QIODevice::WriteOnly)) {
            reply->deleteLater();
            return;
        }

        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);

        connect(reply, &QNetworkReply::readyRead, [&]() {
            file.write(reply->readAll());
        });
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        connect(&timer, &QTimer::timeout, &loop, [&]() {
            reply->abort();
            loop.quit();
        });

        timer.start(30000);
        loop.exec();

        file.close();

        const int status =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QNetworkReply::NetworkError netErr = reply->error();
        reply->deleteLater();

        if (status == 200 && netErr == QNetworkReply::NoError) {
            // Atomically claim the winner slot
            if (state.winner.testAndSetOrdered(-1, idx)) {
                return;   // tmp file left on disk for caller to rename
            }
        }

        // Lost the race or failed — clean up our tmp
        QFile::remove(tmpPath);
    };

    // Spawn two racing threads
    QThread* t0 = QThread::create([&]() { tryUrl(urls[0], tmpPath0, 0); });
    QThread* t1 = QThread::create([&]() { tryUrl(urls[1], tmpPath1, 1); });

    t0->start();
    t1->start();

    // Wait for both (each has a 30 s timeout internally, so 35 s is safe)
    const bool t0done = t0->wait(35000);
    const bool t1done = t1->wait(35000);
    if (!t0done) t0->terminate();
    if (!t1done) t1->terminate();
    t0->deleteLater();
    t1->deleteLater();

    // --- Determine outcome ---
    const int w = state.winner.loadAcquire();
    if (w < 0) {
        // Both failed — clean up both temp files
        QFile::remove(tmpPath0);
        QFile::remove(tmpPath1);
        return false;
    }

    const QString winnerTmp = (w == 0) ? tmpPath0 : tmpPath1;
    const QString loserTmp  = (w == 0) ? tmpPath1 : tmpPath0;

    // SHA1 verification on winner
    if (!sha1.isEmpty() && !verifySha1File(winnerTmp, sha1)) {
        QFile::remove(winnerTmp);
        QFile::remove(loserTmp);
        return false;
    }

    // Atomic rename winner → final destination
    QFile::remove(destPath);
    if (!QFile::rename(winnerTmp, destPath)) {
        QFile::remove(winnerTmp);
        QFile::remove(loserTmp);
        return false;
    }

    // Clean up loser tmp
    QFile::remove(loserTmp);
    return true;
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
    // Mojang-provided hashes are lowercase; case-insensitive compare for safety
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
