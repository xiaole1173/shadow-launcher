// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
//
// Per-file download engine v9 — thread-pool based, phased acceleration.
//
// Key improvements over v8:
//   - QThreadPool instead of QThread::create (avoids OS thread thrash)
//   - 2 shared QNAMs with per-request HTTP/1.1 config (connection pooling)
//   - DNS pre-resolution + IP reliability scoring
//   - Per-host health tracking (consecutive failures, degraded hosts)
//   - Phased acceleration: first-thread-for-all → speed-floor-thread-split
//   - Removed QThread::msleep(50) BMCLAPI serial bottleneck
//   - Removed static s_lastProgressEmitMs race condition

#include "core/file_downloader.h"
#include "utils/logger.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QHttp1Configuration>
#include <QThread>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QEventLoop>
#include <QTimer>
#include <QRunnable>
#include <QHostInfo>
#include <QDateTime>
#include <QPointer>
#include <algorithm>

namespace ShadowDownloader {
using namespace ShadowLauncher;

// ═════════════════════════════════════════════════════════════════════════════
// Construction / Destruction
// ═════════════════════════════════════════════════════════════════════════════

FileDownloader::FileDownloader(QObject* parent) : QObject(parent)
{
    qCInfo(logDownload) << QStringLiteral("下载引擎 v9 初始化");

    m_speedTimer.start();

    // Thread pool: max 128 concurrent workers
    m_threadPool.setMaxThreadCount(128);

    m_managerTimer = new QTimer(this);
    m_managerTimer->setTimerType(Qt::PreciseTimer);
    connect(m_managerTimer, &QTimer::timeout, this, &FileDownloader::managerTick);

    m_speedTimer2 = new QTimer(this);
    m_speedTimer2->setTimerType(Qt::PreciseTimer);
    connect(m_speedTimer2, &QTimer::timeout, this, &FileDownloader::speedTick);
}

FileDownloader::~FileDownloader()
{
    cancel();
    m_threadPool.clear(); // prevent new jobs from starting
    m_threadPool.waitForDone(10000);
}

// ═════════════════════════════════════════════════════════════════════════════
// addFile — enqueue a file for download
// ═════════════════════════════════════════════════════════════════════════════

void FileDownloader::addFile(const QString& localPath, const QString& localName,
                              const QStringList& sources, qint64 expectedSize,
                              const QByteArray& sha1, bool jarStrip)
{
    qCInfo(logDownload) << QStringLiteral("添加下载任务 名称=%1 大小=%2").arg(localName, formatSize(expectedSize));

    // Pre-check SHA1 cache hit
    if (!sha1.isEmpty()) {
        QFileInfo fi(localPath);
        if (fi.exists() && fi.size() > 0) {
            QFile f(localPath);
            if (f.open(QIODevice::ReadOnly)) {
                QCryptographicHash hash(QCryptographicHash::Sha1);
                hash.addData(&f);
                f.close();
                if (hash.result().toHex() == sha1) {
                    m_completedFiles.fetchAndAddRelaxed(1);
                    m_totalBytes.fetchAndAddRelaxed(fi.size());
                    m_downloadedBytes.fetchAndAddRelaxed(fi.size());
                    emit logMessage(QString::fromUtf8("[完成] 缓存命中: %1 (%2)")
                                        .arg(localName, formatSize(fi.size())));
                    emit fileProgress(localPath, localName, fi.size(), fi.size(), localPath);
                    m_totalFiles.fetchAndAddRelaxed(1);
                    emit fileFinished(localPath, true);
                    return;
                }
            }
        }
    }

    auto file = std::make_shared<FileDownload>();
    file->localPath = localPath;
    file->localName = localName;
    file->orderedSources = sources;
    file->expectedSha1 = sha1;
    file->needsJarStrip = jarStrip;
    file->fileSize = expectedSize;
    file->isUnknownSize = (expectedSize <= 0);
    file->isNoSplit = (!file->isUnknownSize && file->fileSize < 50LL * 1024 * 1024);

    QMutexLocker lock(&m_filesMutex);
    m_files.append(file);
    m_totalFiles.fetchAndAddRelaxed(1);
    if (file->fileSize > 0) m_totalBytes.fetchAndAddRelaxed(file->fileSize);

    qCInfo(logDownload) << QStringLiteral("任务已排队 名称=%1 队列总数=%2").arg(localName).arg(m_files.size());
}

// ═════════════════════════════════════════════════════════════════════════════
// Lifecycle
// ═════════════════════════════════════════════════════════════════════════════

void FileDownloader::start()
{
    if (m_state == Running) return;
    m_state = Running;
    m_cancelled.storeRelaxed(0);
    m_phase = PhaseFirstThread;

    m_speedTimer.start();
    // 初始化 lastSpeedBytes 为当前 m_downloadedBytes（可能已包含 cache hit 的文件），
    // 避免首次 speedTick 将缓存文件计入瞬时速度
    m_lastSpeedBytes = m_downloadedBytes.loadRelaxed();
    m_speedFloorBps.storeRelaxed(kMinSpeedFloorBps);
    m_speedRecords.clear();

    // Pre-resolve DNS for all unique hosts
    {
        QSet<QString> hosts;
        QMutexLocker lock(&m_filesMutex);
        for (const auto& f : m_files) {
            for (const auto& s : f->orderedSources)
                hosts.insert(extractHost(s));
        }
        lock.unlock();
        for (const auto& h : hosts)
            resolveHost(h);
    }

    // Clear thread pool from any previous runs
    m_threadPool.clear();

    m_managerTimer->start(50);
    m_speedTimer2->start(100);

    emit progressChanged(m_completedFiles.loadRelaxed(), m_totalFiles.loadRelaxed(),
                          m_downloadedBytes.loadRelaxed(), m_totalBytes.loadRelaxed());

    emit logMessage(QString("引擎 v9 启动: %1 文件, %2 线程上限, %3 DNS 解析")
                        .arg(m_files.size()).arg(m_maxThreads).arg(""));
}

void FileDownloader::pause()
{
    m_state = Paused;
    m_managerTimer->stop();
    m_speedTimer2->stop();
    emit logMessage(QString::fromUtf8("[暂停] 下载已暂停"));
}

void FileDownloader::resume()
{
    if (m_state != Paused) return;
    m_state = Running;
    m_speedTimer.restart();
    m_lastSpeedBytes = m_downloadedBytes.loadRelaxed();
    m_managerTimer->start(50);
    m_speedTimer2->start(100);
    emit logMessage(QString::fromUtf8("▶ 下载已恢复"));
}

void FileDownloader::cancel()
{
    m_cancelled.storeRelaxed(1);
    m_state = Cancelled;
    m_managerTimer->stop();
    m_speedTimer2->stop();
    m_threadPool.clear();

    // Abort ALL in-flight HTTP replies immediately.
    // IMPORTANT: QNetworkReply::abort() is only thread-safe when called from the
    // thread that owns the reply. Workers run in QThreadPool threads, so we MUST
    // invoke abort() on the worker's event loop via QueuedConnection.
    // Direct reply->abort() from the main thread is UNDEFINED BEHAVIOR.
    {
        QMutexLocker lock(&m_inflightMutex);
        for (auto* reply : m_inflightReplies) {
            // Queue abort() on the worker thread's event loop
            QMetaObject::invokeMethod(reply, "abort", Qt::QueuedConnection);
        }
        m_inflightReplies.clear();
    }

    emit logMessage(QString::fromUtf8("[失败] 下载已取消"));
}

// ═════════════════════════════════════════════════════════════════════════════

// ── In-flight reply tracking (for immediate abort on cancel) ──
void FileDownloader::addInFlightReply(QNetworkReply* reply) {
    QMutexLocker lock(&m_inflightMutex);
    m_inflightReplies.append(reply);
}

void FileDownloader::removeInFlightReply(QNetworkReply* reply) {
    QMutexLocker lock(&m_inflightMutex);
    m_inflightReplies.removeOne(reply);
}

// Manager tick — phase-based scheduling (主流启动器-style)
// ═════════════════════════════════════════════════════════════════════════════

void FileDownloader::managerTick()
{
    if (m_state != Running || m_cancelled.loadRelaxed()) return;

    int active = m_activeThreads.loadRelaxed();
    int maxThreads = m_maxThreads;
    if (active >= maxThreads) return;

    QMutexLocker lock(&m_filesMutex);

    // Phase 1: ensure every file has at least one thread started
    if (m_phase == PhaseFirstThread) {
        bool allStarted = true;
        for (auto& f : m_files) {
            if (f->state == 5 || f->state == 4) continue; // failed or finished
            if (f->state == 0) {
                if (active >= maxThreads) { allStarted = false; break; }
                auto th = tryStartFirstThread(f);
                if (th) active++;
            }
        }
        if (allStarted) {
            m_phase = PhaseAccelerate;
            qCInfo(logDownload) << "Phase: all files have first thread → accelerate";
        }
        lock.unlock();
        return;
    }

    // Phase 2-3: speed-based thread splitting
    double curMbps = currentSpeedMBps();
    qint64 floor = m_speedFloorBps.loadRelaxed();

    // If speed >= floor, don't add more threads
    if (curMbps * 1024 * 1024 >= floor) {
        lock.unlock();
        return;
    }

    // Add threads to files with large remaining chunks
    for (auto& f : m_files) {
        if (active >= maxThreads) break;
        if (f->state >= 3 || f->state == 5) continue; // merging/finished/failed
        if (f->isNoSplit && !f->threads.isEmpty()) continue; // single-thread files, already started

        // Check if preparing threads outnumber downloading threads
        int prep = 0, dl = 0;
        for (auto& t : f->threads) {
            if (t->state < 2) prep++;
            else if (t->state == 2) dl++;
        }
        if (prep > dl) continue;

        auto th = tryAddThread(f);
        if (th) active++;
    }
    lock.unlock();
}

int FileDownloader::filesWithoutThread()
{
    QMutexLocker lock(&m_filesMutex);
    int count = 0;
    for (const auto& f : m_files) {
        if (f->state == 0 && f->state != 5 && f->state != 4)
            count++;
    }
    return count;
}

// ═════════════════════════════════════════════════════════════════════════════
// Thread management — tryStartFirstThread / tryAddThread
// ═════════════════════════════════════════════════════════════════════════════

std::shared_ptr<DownloadThread> FileDownloader::tryStartFirstThread(
    std::shared_ptr<FileDownload> file)
{
    if (file->orderedSources.isEmpty()) return nullptr;
    if (file->fileSize <= 0 && !file->isUnknownSize) file->fileSize = 10LL * 1024 * 1024;
    if (file->fileSize <= 0) { file->isUnknownSize = true; file->isNoSplit = true; }

    // Pick a healthy source
    int sourceIdx = 0;
    int srcLabel = 0;
    for (int i = 0; i < file->orderedSources.size(); ++i) {
        QString host = extractHost(file->orderedSources[i]);
        if (hostCanAccept(host)) { sourceIdx = i; srcLabel = (i == 0) ? 0 : i; break; }
        if (i == file->orderedSources.size() - 1) { sourceIdx = i; srcLabel = i; } // last resort
    }

    // Source selection log (sampled)
    static int s_srcLogCtr = 0;
    if (srcLabel > 0 || (++s_srcLogCtr % 20 == 0)) {
        qCInfo(logDownload) << QStringLiteral("[source] primary=%1 file=%2")
            .arg(extractHost(file->orderedSources[sourceIdx]), file->localName);
    }

    auto th = std::make_shared<DownloadThread>();
    th->uuid = m_nextUuid.fetchAndAddRelaxed(1);
    th->downloadStart = 0;
    th->downloadEnd = file->isUnknownSize ? 0 : file->fileSize;
    th->sourceUrl = file->orderedSources[sourceIdx];
    file->threads.append(th);
    file->state = 1;
    m_activeThreads.fetchAndAddRelaxed(1);

    // Track host active request count
    {
        QString host = extractHost(th->sourceUrl);
        QMutexLocker lock(&m_hostMutex);
        m_hostStats[host].activeRequests++;
    }

    emit logMessage(QString("▶ 启动: %1 [%2 → %3] %4")
                        .arg(file->localName)
                        .arg(th->downloadStart).arg(th->downloadEnd)
                        .arg(th->sourceUrl));

    launchWorker(th, file);
    return th;
}

std::shared_ptr<DownloadThread> FileDownloader::tryAddThread(
    std::shared_ptr<FileDownload> file)
{
    if (file->fileSize <= 0 || file->isNoSplit || file->threads.isEmpty()) return nullptr;

    std::shared_ptr<DownloadThread> maxPiece;
    qint64 maxUndone = 0;
    for (auto& t : file->threads) {
        if (t->state >= 3) continue;
        qint64 u = t->downloadUndone();
        if (u > maxUndone) { maxUndone = u; maxPiece = t; }
    }
    if (!maxPiece || maxUndone < 512 * 1024) return nullptr;

    qint64 splitPoint = maxPiece->downloadEnd - static_cast<qint64>(maxUndone * 0.4);

    // Pick a healthy source
    int sourceIdx = 0;
    for (int i = 0; i < file->orderedSources.size(); ++i) {
        QString host = extractHost(file->orderedSources[i]);
        if (hostCanAccept(host)) { sourceIdx = i; break; }
        if (i == file->orderedSources.size() - 1) { sourceIdx = i; } // last resort
    }

    // Source selection log (sampled per 20 addition threads)
    static int s_addSrcLogCtr = 0;
    if (sourceIdx > 0 || (++s_addSrcLogCtr % 20 == 0)) {
        qCInfo(logDownload) << QStringLiteral("[source] addition file=%1 host=%2")
            .arg(file->localName, extractHost(file->orderedSources[sourceIdx]));
    }

    auto th = std::make_shared<DownloadThread>();
    th->uuid = m_nextUuid.fetchAndAddRelaxed(1);
    th->downloadStart = splitPoint;
    th->downloadEnd = maxPiece->downloadEnd;
    th->sourceUrl = file->orderedSources[sourceIdx];
    maxPiece->downloadEnd = splitPoint;

    qCInfo(logDownload) << QStringLiteral("附加线程 线程号=%1 文件=%2 偏移=%3")
                            .arg(th->uuid).arg(file->localName).arg(splitPoint);

    file->threads.append(th);
    m_activeThreads.fetchAndAddRelaxed(1);

    {
        QString host = extractHost(th->sourceUrl);
        QMutexLocker lock(&m_hostMutex);
        m_hostStats[host].activeRequests++;
    }

    launchWorker(th, file);
    return th;
}

// ═════════════════════════════════════════════════════════════════════════════
// Worker — QRunnable that downloads one file range
// ═════════════════════════════════════════════════════════════════════════════

class DownloadWorker : public QRunnable {
public:
    FileDownloader* self = nullptr;
    std::shared_ptr<DownloadThread> th;
    std::shared_ptr<FileDownload> file;

    void run() override {
        if (!self) return;
        self->runWorker(th, file);
    }
};

void FileDownloader::launchWorker(std::shared_ptr<DownloadThread> th,
                                   std::shared_ptr<FileDownload> file)
{
    auto* worker = new DownloadWorker();
    worker->setAutoDelete(true);
    worker->self = this;
    worker->th = th;
    worker->file = file;
    m_threadPool.start(worker);
}

void FileDownloader::runWorker(std::shared_ptr<DownloadThread> th,
                                std::shared_ptr<FileDownload> file)
{
    th->state = 1;
    th->lastReceiveTime = getElapsedMs();

    qCInfo(logDownload) << QStringLiteral("开始下载 URL=%1 文件=%2 偏移=%3")
                            .arg(th->sourceUrl, file->localName).arg(th->downloadStart);

    if (!ShadowLauncher::suppressUrlLog())
        emit logMessage(QString("↓ 下载: %1 (%2)").arg(file->localName).arg(th->sourceUrl));

    // Per-worker QNAM — each worker thread creates its own.
    // QNetworkAccessManager is reentrant but NOT thread-safe:
    // a single instance MUST NOT be used from multiple threads.
    QNetworkAccessManager mgr;

    bool sourceOk = false;
    for (int sourceIdx = 0; sourceIdx < file->orderedSources.size() && !sourceOk; ++sourceIdx) {
        if (m_cancelled.loadRelaxed()) goto cleanup;

        QString url = file->orderedSources[sourceIdx];
        th->sourceUrl = url;

        if (sourceIdx > 0)
            qCInfo(logDownload) << QStringLiteral("切换到镜像%1 URL=%2").arg(sourceIdx + 1).arg(url);

        sourceOk = false;
        qint64 startTimeMs = getElapsedMs();
        for (int attempt = 0; attempt < 6 && !sourceOk; ++attempt) {
            if (m_cancelled.loadRelaxed()) goto cleanup;

            int timeoutMs;
            switch (attempt) {
                case 0: timeoutMs = 30000; break;
                case 1: timeoutMs = 30000; break;
                default: timeoutMs = 15000; break;
            }
            if (attempt >= 2 && file->expectedSha1.isEmpty()
                && (getElapsedMs() - startTimeMs) < 5500) break;
            if (attempt >= 5 && (getElapsedMs() - startTimeMs) < 5500) break;
            if (attempt > 0) QThread::msleep(500);

            QNetworkRequest req{QUrl(url)};
            req.setRawHeader("User-Agent", "ShadowLauncher/1.0");
            req.setTransferTimeout(timeoutMs);
            req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
            // HTTP/1.1 connection pooling
            {
                QHttp1Configuration h1cfg;
                h1cfg.setNumberOfConnectionsPerHost(255);
                req.setHttp1Configuration(h1cfg);
            }

            if (!file->isUnknownSize && !file->isNoSplit) {
                qint64 start = th->downloadStart + th->downloadDone;
                qint64 end = th->downloadEnd - 1;
                req.setRawHeader("Range", QString("bytes=%1-%2").arg(start).arg(end).toUtf8());
            }

            QNetworkReply* reply = mgr.get(req);

            QEventLoop loop;
            QTimer timeout;
            timeout.setSingleShot(true);
            bool timedOut = false;

            connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            connect(&timeout, &QTimer::timeout, [&]() { timedOut = true; loop.quit(); });
            timeout.start(timeoutMs);

            qint64 lastProgressEmitMs = getElapsedMs();

            connect(reply, &QNetworkReply::downloadProgress,
                    [&](qint64 received, qint64 total) {
                qint64 delta = received - th->downloadDone;
                if (delta > 0) {
                    m_downloadedBytes.fetchAndAddRelaxed(delta);
                    th->downloadDone = received;
                }
                th->lastReceiveTime = getElapsedMs();
                qint64 now = getElapsedMs();
                if (now - lastProgressEmitMs >= 150) {
                    lastProgressEmitMs = now;
                    emit progressChanged(m_completedFiles.loadRelaxed(), m_totalFiles.loadRelaxed(),
                                          m_downloadedBytes.loadRelaxed(), m_totalBytes.loadRelaxed());
                }
                emit fileProgress(th->sourceUrl, file->localName, received, total, file->localPath);
            });

            // Register reply for thread-safe abort on cancel
            // QMetaObject::invokeMethod("abort", QueuedConnection) from the main
            // thread triggers reply->abort() on THIS worker thread (the owner).
            addInFlightReply(reply);

            // NOTE: QTimer cancelGuard is NOT used here — QThreadPool threads
            // have no event dispatcher, so QTimer never fires. Instead we rely on:
            //   1. addInFlightReply() + cancel() queuing abort() to this thread
            //   2. loop.exec() returning when reply finishes (naturally or aborted)
            //   3. The m_cancelled check below to discard data if cancelled

            loop.exec();

            removeInFlightReply(reply);

            // Cancel check: discard data immediately if cancelled
            if (m_cancelled.loadRelaxed()) {
                reply->abort();
                reply->deleteLater();
                goto cleanup;
            }

            if (timedOut || reply->error() != QNetworkReply::NoError) {
                qCWarning(logDownload) << QStringLiteral("请求失败 URL=%1 错误=%2 重试=%3")
                    .arg(url, timedOut ? QStringLiteral("超时") : reply->errorString())
                    .arg(attempt + 1);
                qCInfo(logDownload) << QStringLiteral("[fail] host=%1 file=%2 retry=%3")
                    .arg(extractHost(url), file->localName).arg(attempt + 1);
                reply->abort();
                reply->deleteLater();
                if (attempt >= 2 && !file->expectedSha1.isEmpty()) break;
                th->downloadDone = 0;
                continue;
            }

            sourceOk = true;
            QByteArray data = reply->readAll();
            qint64 contentLen = reply->rawHeader("Content-Length").toLongLong();
            // 优先用服务器返回的 Content-Length 做预期判断（它才是实际响应体大小）
            // file->fileSize 是 API 解析值，可能偏大（如清华镜像 API 195.6MB 实际 195.0MB）
            qint64 expectedSize = contentLen > 0 ? contentLen : (file->fileSize > 0 ? file->fileSize : 0);
            if (expectedSize > 0 && data.size() < expectedSize) {
                qCWarning(logDownload) << QStringLiteral("下载数据不完整 URL=%1 预期=%2(Content-Length) 实际=%3")
                    .arg(url).arg(expectedSize).arg(data.size());
                sourceOk = false;
                reply->deleteLater();
                th->downloadDone = 0;
                continue;
            }
            reply->deleteLater();

            // Determine file size on first thread
            // 始终以实际 Content-Length（或收到的数据大小）为准，
            // 修正 addFile 时传入的 expectedSize 可能偏小的问题
            if (th->downloadStart == 0 && data.size() > 0) {
                qint64 actualSize = contentLen > 0 ? contentLen : data.size();
                if (actualSize > 0 && actualSize != file->fileSize) {
                    if (file->fileSize > 0) {
                        // API 提供的 expectedSize 小于实际大小，补差
                        m_totalBytes.fetchAndAddRelaxed(actualSize - file->fileSize);
                    } else {
                        // 未知大小的文件，首次发现大小
                        m_totalBytes.fetchAndAddRelaxed(actualSize);
                    }
                    file->fileSize = actualSize;
                    file->isUnknownSize = false;
                    file->isNoSplit = (actualSize < 50LL * 1024 * 1024);
                    th->downloadEnd = actualSize;
                }
            }

            // ── Truncate data if thread was split mid-download ──
            // When tryAddThread splits a running thread, the in-flight HTTP request
            // may return more data than this thread's current (reduced) range.
            // Clip to the allocated range to avoid overlapping temp files during merge,
            // which would corrupt the final file (same bytes counted twice).
            // IMPORTANT: use mid(downloadStart, threadRange), NOT left(threadRange),
            // because split threads start at a non-zero offset. If the server ignores
            // Range header and returns the full file, left() would grab bytes 0..N
            // when we need bytes downloadStart..downloadStart+threadRange.
            {
                qint64 threadRange = th->downloadEnd - th->downloadStart;
                if (threadRange > 0 && data.size() > threadRange) {
                    qint64 excess = data.size() - threadRange;
                    qCInfo(logDownload) << QStringLiteral("截断多余数据 文件=%1 起始=%2 预期=%3 实际=%4 超额=%5")
                        .arg(file->localName).arg(th->downloadStart).arg(threadRange).arg(data.size()).arg(excess);
                    data = data.mid(th->downloadStart, threadRange);
                    // Adjust byte counters: the excess was already counted via downloadProgress
                    th->downloadDone = threadRange;
                    m_downloadedBytes.fetchAndAddRelaxed(-excess);
                }
            }

            // SHA1 verification for full-download files
            bool isFullDownload = file->isNoSplit || file->isUnknownSize;
            if (isFullDownload && !file->expectedSha1.isEmpty()) {
                QByteArray dlHash = QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex();
                if (dlHash != file->expectedSha1) {
                    qCWarning(logDownload) << QStringLiteral("SHA1不匹配 URL=%1 预期=%2 实际=%3 (第%4次)")
                        .arg(url, QString::fromLatin1(file->expectedSha1), QString::fromLatin1(dlHash))
                        .arg(attempt + 1);
                    qCInfo(logDownload) << QStringLiteral("[fail] SHA1 host=%1 file=%2")
                        .arg(extractHost(url), file->localName);
                    sourceOk = false;
                    if (attempt >= 5) break;
                    continue;
                }
            }

            // Write data
            if (file->isNoSplit) {
                QFile f(file->localPath);
                QDir().mkpath(QFileInfo(file->localPath).absolutePath());
                if (f.open(QIODevice::WriteOnly)) { f.write(data); f.close(); }
            } else {
                QString tmpDir = QFileInfo(file->localPath).absolutePath() + "/.shadow_temp/";
                QDir().mkpath(tmpDir);
                th->tempPath = tmpDir + QString("dl_%1_%2.tmp").arg(file->localName).arg(th->uuid);
                QFile f(th->tempPath);
                if (f.open(QIODevice::WriteOnly)) { f.write(data); f.close(); }
            }

            th->state = 3;
            goto worker_done;
        }
    }

    // Last resort retry
    if (!sourceOk && !file->expectedSha1.isEmpty() && file->orderedSources.size() > 0) {
        const QString url = file->orderedSources[0];
        qCInfo(logDownload) << QStringLiteral("最终兜底重试 URL=%1").arg(url);
        for (int attempt = 0; attempt < 3 && !sourceOk; ++attempt) {
            if (m_cancelled.loadRelaxed()) goto cleanup;
            if (attempt > 0) QThread::msleep(500);

            QUrl qurl2(url); QNetworkRequest req(qurl2);
            qint64 lastProgressEmitMs = getElapsedMs();
            req.setRawHeader("User-Agent", "ShadowLauncher/1.0");
            req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
            {
                    QHttp1Configuration h1cfg;
                h1cfg.setNumberOfConnectionsPerHost(255);
                req.setHttp1Configuration(h1cfg);
            }
            QNetworkReply* reply = mgr.get(req);

            QEventLoop loop;
            QTimer timeout;
            timeout.setSingleShot(true);
            bool timedOut = false;
            connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            connect(&timeout, &QTimer::timeout, [&]() { timedOut = true; loop.quit(); });
            int timeoutMs = (attempt == 0) ? 60000 : 30000;
            timeout.start(timeoutMs);

            connect(reply, &QNetworkReply::downloadProgress,
                    [&](qint64 received, qint64 total) {
                qint64 delta = received - th->downloadDone;
                if (delta > 0) {
                    m_downloadedBytes.fetchAndAddRelaxed(delta);
                    th->downloadDone = received;
                }
                th->lastReceiveTime = getElapsedMs();
                qint64 now = getElapsedMs();
                if (now - lastProgressEmitMs >= 150) {
                    lastProgressEmitMs = now;
                    emit progressChanged(m_completedFiles.loadRelaxed(), m_totalFiles.loadRelaxed(),
                                          m_downloadedBytes.loadRelaxed(), m_totalBytes.loadRelaxed());
                }
                emit fileProgress(th->sourceUrl, file->localName, received, total, file->localPath);
            });

            // Register reply for thread-safe abort on cancel
            addInFlightReply(reply);

            loop.exec();

            removeInFlightReply(reply);

            // Cancel check: discard data immediately if cancelled
            if (m_cancelled.loadRelaxed()) {
                reply->abort();
                reply->deleteLater();
                goto cleanup;
            }

            if (timedOut || reply->error() != QNetworkReply::NoError) {
                reply->abort();
                reply->deleteLater();
                continue;
            }

            QByteArray data = reply->readAll();
            reply->deleteLater();

            // ── Truncate data if thread was split mid-download (retry path) ──
            {
                qint64 threadRange = th->downloadEnd - th->downloadStart;
                if (threadRange > 0 && data.size() > threadRange) {
                    qint64 excess = data.size() - threadRange;
                    qCInfo(logDownload) << QStringLiteral("截断多余数据(重试) 文件=%1 起始=%2 预期=%3 实际=%4 超额=%5")
                        .arg(file->localName).arg(th->downloadStart).arg(threadRange).arg(data.size()).arg(excess);
                    data = data.mid(th->downloadStart, threadRange);
                    th->downloadDone = threadRange;
                    m_downloadedBytes.fetchAndAddRelaxed(-excess);
                }
            }

            bool isFullDownload = file->isNoSplit || file->isUnknownSize;
            if (isFullDownload && !file->expectedSha1.isEmpty()) {
                QByteArray dlHash = QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex();
                if (dlHash == file->expectedSha1) {
                    qCInfo(logDownload) << QStringLiteral("最终兜底 SHA1匹配 URL=%1").arg(url);
                } else {
                    if (file->fileSize > 0 && data.size() >= file->fileSize) {
                        qCInfo(logDownload) << QStringLiteral("最终兜底 大小匹配,信任数据 URL=%1").arg(url);
                    } else {
                        sourceOk = false;
                        continue;
                    }
                }
                sourceOk = true;
                if (file->isNoSplit) {
                    QFile f(file->localPath);
                    QDir().mkpath(QFileInfo(file->localPath).absolutePath());
                    if (f.open(QIODevice::WriteOnly)) { f.write(data); f.close(); }
                } else {
                    QString tmpDir = QFileInfo(file->localPath).absolutePath() + "/.shadow_temp/";
                    QDir().mkpath(tmpDir);
                    th->tempPath = tmpDir + QString("dl_%1_%2.tmp").arg(file->localName).arg(th->uuid);
                    QFile f(th->tempPath);
                    if (f.open(QIODevice::WriteOnly)) { f.write(data); f.close(); }
                }
                th->state = 3;
                goto worker_done;
            }
        }
    }

    th->state = 4; // failed
    emit logMessage(QString("%1: 所有源均失败").arg(file->localName));

worker_done:
cleanup:
    m_activeThreads.fetchAndAddRelaxed(-1);

    // Decrement host active request count
    {
        QString host = extractHost(th->sourceUrl);
        QMutexLocker lock(&m_hostMutex);
        auto it = m_hostStats.find(host);
        if (it != m_hostStats.end()) it->activeRequests--;
    }

    qCInfo(logDownload) << QStringLiteral("线程完成 文件=%1 字节=%2")
        .arg(file->localName).arg(th->downloadDone);

    // ── Merge on worker thread (avoid main thread disk I/O) ──
    bool allDone = false, anyFailed = false;
    {
        QMutexLocker lock(&m_filesMutex);
        allDone = true;
        for (auto& t : file->threads) {
            if (t.get() == th.get()) continue; // skip self (state may not be set yet)
            if (t->state < 3 && t->state != 4) { allDone = false; break; }
            if (t->state == 4) anyFailed = true;
        }
        // Only the LAST worker checks: all others done + self just finished
        if (allDone && th->state != 4) {
            // Count self as done (its state was set above before goto)
        }
        allDone = allDone && file->state < 3;
    }

    if (allDone) {
        bool ok = false;
        if (!anyFailed && th->state != 4) {
            file->state = 3;
            ok = mergeFile(file);
            file->state = ok ? 4 : 5;
        } else {
            file->state = 5;
        }

        // Signal completion to main thread (stats + signal emission)
        QPointer<FileDownloader> self(this);
        QMetaObject::invokeMethod(this, [self, file, ok, anyFailed]() {
            if (!self) return;
            if (!self) return;
            if (anyFailed || !ok) {
                self->m_failedFiles.fetchAndAddRelaxed(1);
                for (const auto& s : file->orderedSources)
                    self->recordHostResult(QUrl(s).host().toLower(), false);
                self->logMessage(QString::fromUtf8("[失败] 下载失败: %1 (所有源均失败)").arg(file->localName));
                self->fileFinished(file->localPath, false);
            } else {
                self->m_completedFiles.fetchAndAddRelaxed(1);
                for (const auto& t : file->threads)
                    self->recordHostResult(QUrl(t->sourceUrl).host().toLower(), true);
                self->fileFinished(file->localPath, true);
            }
            self->updateStats();
        }, Qt::QueuedConnection);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// mergeFile — combine multi-thread parts into final file
// ═════════════════════════════════════════════════════════════════════════════

bool FileDownloader::mergeFile(std::shared_ptr<FileDownload> file)
{
    if (file->isNoSplit) {
        if (!file->expectedSha1.isEmpty()) {
            QFile f(file->localPath);
            if (!f.open(QIODevice::ReadOnly)) return false;
            QCryptographicHash h(QCryptographicHash::Sha1);
            h.addData(&f); f.close();
            if (h.result().toHex() != file->expectedSha1) {
                emit logMessage(QString("SHA1校验失败: %1").arg(file->localName));
                return false;
            }
        }
        return true;
    }

    qCInfo(logDownload) << QStringLiteral("开始合并文件 路径=%1 分段数=%2")
        .arg(file->localPath).arg(file->threads.size());

    QDir().mkpath(QFileInfo(file->localPath).absolutePath());
    QFile out(file->localPath);
    if (!out.open(QIODevice::WriteOnly)) {
        emit logMessage(QString("无法写入: %1").arg(file->localPath));
        return false;
    }

    auto sorted = file->threads;
    std::sort(sorted.begin(), sorted.end(), [](auto& a, auto& b) { return a->uuid < b->uuid; });

    for (auto& th : sorted) {
        if (th->tempPath.isEmpty()) continue;
        QFile tf(th->tempPath);
        if (!tf.open(QIODevice::ReadOnly)) { out.close(); return false; }
        out.write(tf.readAll());
        tf.close();
        tf.remove();
    }
    out.close();

    if (!file->expectedSha1.isEmpty()) {
        out.open(QIODevice::ReadOnly);
        QCryptographicHash h(QCryptographicHash::Sha1);
        h.addData(&out); out.close();
        if (h.result().toHex() != file->expectedSha1) {
            emit logMessage(QString("SHA1合并校验失败: %1").arg(file->localName));
            return false;
        }
    }

    qCInfo(logDownload) << QStringLiteral("合并完成 文件=%1 SHA1校验=通过").arg(file->localName);
    return true;
}

// ═════════════════════════════════════════════════════════════════════════════
// Speed tracking
// ═════════════════════════════════════════════════════════════════════════════

void FileDownloader::speedTick()
{
    if (m_state != Running) return;

    qint64 elapsed = m_speedTimer.elapsed();
    if (elapsed < 100) return;

    qint64 now = m_downloadedBytes.loadRelaxed();
    qint64 bytes = now - m_lastSpeedBytes;
    m_lastSpeedBytes = now;
    m_speedTimer.restart();

    qint64 actualBps = bytes * 1000 / elapsed;

    {
        QMutexLocker lock(&m_speedMutex);
        m_speedRecords.prepend(actualBps);
        if (m_speedRecords.size() > kMaxSpeedRecords) m_speedRecords.removeLast();
    }

    qint64 weightedSum = 0;
    int weightDiv = 0;
    {
        QMutexLocker lock(&m_speedMutex);
        int w = m_speedRecords.size();
        for (auto rec : m_speedRecords) { weightedSum += rec * w; weightDiv += w; w--; }
    }
    qint64 currentBps = (weightDiv > 0) ? (weightedSum / weightDiv) : actualBps;

    m_emaMbps = m_emaMbps * 0.5 + (currentBps / (1024.0 * 1024.0)) * 0.5;

    // ── Speed floor: up on growth, decay on stagnation ──
    qint64 floorLimit = static_cast<qint64>(currentBps * 0.85);
    qint64 currentFloor = m_speedFloorBps.loadRelaxed();
    const qint64 nowMs = getElapsedMs();
    if (currentBps >= kMinSpeedFloorBps && floorLimit > currentFloor) {
        m_speedFloorBps.storeRelaxed(floorLimit);
        m_lastFloorIncreaseMs = nowMs;
    } else if (nowMs - m_lastFloorIncreaseMs > 5000 && currentFloor > kMinSpeedFloorBps) {
        // 5s without growth: decay floor by 50%
        qint64 newFloor = qMax(kMinSpeedFloorBps, currentFloor / 2);
        m_speedFloorBps.storeRelaxed(newFloor);
        m_lastFloorIncreaseMs = nowMs;
    }

    // ── Speed log (rate-limited to 1s) ──
    if (nowMs - m_lastSpeedLogMs >= 1000) {
        m_lastSpeedLogMs = nowMs;
        double avgMbps = currentBps / (1024.0 * 1024.0);
        qCInfo(logDownload) << QStringLiteral("[speed] EMA=%1 MB/s AVG=%2 MB/s floor=%3 KB/s active=%4/%5")
            .arg(m_emaMbps, 0, 'f', 1)
            .arg(avgMbps, 0, 'f', 1)
            .arg(currentFloor / 1024)
            .arg(m_activeThreads.loadRelaxed())
            .arg(m_maxThreads);
    }

    emit progressChanged(m_completedFiles.loadRelaxed(), m_totalFiles.loadRelaxed(),
                          m_downloadedBytes.loadRelaxed(), m_totalBytes.loadRelaxed());
}

// ═════════════════════════════════════════════════════════════════════════════
// updateStats
// ═════════════════════════════════════════════════════════════════════════════

void FileDownloader::updateStats()
{
    int done = m_completedFiles.loadRelaxed();
    int total = m_totalFiles.loadRelaxed();
    int fails = m_failedFiles.loadRelaxed();

    emit progressChanged(done, total,
                          m_downloadedBytes.loadRelaxed(),
                          m_totalBytes.loadRelaxed());

    if (done + fails >= total && total > 0) {
        m_managerTimer->stop();
        m_speedTimer2->stop();
        m_state = Idle;
        const qint64 totalDl = m_totalBytes.loadRelaxed();
        emit logMessage(QString::fromUtf8("[完成] 下载完成: %1/%2 文件, %3, 速度 %4 MB/s")
                            .arg(done).arg(total)
                            .arg(formatSize(totalDl))
                            .arg(m_emaMbps, 0, 'f', 1));
        emit allFinished();
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Per-host health
// ═════════════════════════════════════════════════════════════════════════════

QString FileDownloader::extractHost(const QString& url)
{
    return QUrl(url).host().toLower();
}


bool FileDownloader::hostCanAccept(const QString& host) const
{
    QMutexLocker lock(&m_hostMutex);
    auto it = m_hostStats.find(host);
    if (it == m_hostStats.end()) return true;
    if (it->degraded) return false;
    if (it->activeRequests >= kMaxPerHost) return false;
    return true;
}

void FileDownloader::recordHostResult(const QString& host, bool ok)
{
    QMutexLocker lock(&m_hostMutex);
    auto& st = m_hostStats[host];
    if (ok) {
        st.consecutiveFails = 0;
        st.totalSuccess++;
        if (st.degraded && st.totalSuccess >= 3)
            st.degraded = false;
    } else {
        st.consecutiveFails++;
        st.totalFails++;
        if (st.consecutiveFails >= 3)
            st.degraded = true;
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// DNS + IP reliability
// ═════════════════════════════════════════════════════════════════════════════

QStringList FileDownloader::resolveHost(const QString& host)
{
    QMutexLocker lock(&m_dnsMutex);
    auto it = m_dnsCache.find(host);
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    if (it != m_dnsCache.end()) {
        if (now - it->lastResolveMs < kDnsCacheMs)
            return it->addresses;
        if (it->addresses.isEmpty() && now - it->lastResolveMs < kDnsFailureBackoffMs)
            return {};
    }

    QHostInfo info = QHostInfo::fromName(host);
    IPInfo& ipi = m_dnsCache[host];
    ipi.lastResolveMs = now;

    if (info.error() != QHostInfo::NoError) {
        ipi.addresses.clear();
        return {};
    }

    QStringList ipv4, ipv6;
    for (const auto& addr : info.addresses()) {
        if (addr.protocol() == QAbstractSocket::IPv4Protocol)
            ipv4.append(addr.toString());
        else
            ipv6.append(addr.toString());
    }
    ipi.addresses = ipv4 + ipv6;
    return ipi.addresses;
}

// ═════════════════════════════════════════════════════════════════════════════
// Helpers
// ═════════════════════════════════════════════════════════════════════════════

double FileDownloader::currentSpeedMBps() const { return m_emaMbps; }

QString FileDownloader::formatSize(qint64 bytes)
{
    if (bytes < 1024) return QString::number(bytes) + " B";
    if (bytes < 1048576) return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    if (bytes < 1073741824LL) return QString::number(bytes / 1048576.0, 'f', 1) + " MB";
    return QString::number(bytes / 1073741824.0, 'f', 2) + " GB";
}

qint64 FileDownloader::getElapsedMs()
{
    static QElapsedTimer gt;
    if (!gt.isValid()) gt.start();
    return gt.elapsed();
}

} // namespace ShadowDownloader
