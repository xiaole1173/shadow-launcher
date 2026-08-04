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
#include "../utils/hash_utils.h"
#include "core/engine_identity.h"
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
    qCInfo(logDownload) << QStringLiteral("[夸父] 下载引擎 v9 初始化");

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
                              const QByteArray& sha1, bool jarStrip, bool skipCacheCheck)
{
    qCInfo(logDownload) << QStringLiteral("[夸父] 添加下载任务 名称=%1 大小=%2").arg(localName, formatSize(expectedSize));

    // Pre-check SHA1 cache hit in working dir (tempDir for merged installs)
    // skipCacheCheck=true：调用方已后台预检过缓存（未命中），跳过重复读盘 SHA1
    if (!sha1.isEmpty() && !skipCacheCheck) {
        QFileInfo fi(localPath);
        if (fi.exists() && fi.size() > 0) {
            QFile f(localPath);
            if (f.open(QIODevice::ReadOnly)) {
                QCryptographicHash hash(QCryptographicHash::Sha1);
                hash.addData(&f);
                f.close();
                if (hash.result().toHex() == sha1) {
                    // 缓存命中：本地文件 SHA1 完全匹配 → 直接复用，跳过网络请求。
                    // 计数延迟到 start() 统一入账（m_cacheHits）——否则 addFile 阶段
                    // completed 提前涨高，进度条从高完成度开始；全命中时 completed==total
                    // 但无 worker → allFinished 永不触发 → 下载流程卡死（问题 1/2 根因）。
                    m_cacheHits.fetchAndAddRelaxed(1);
                    m_cacheBytes.fetchAndAddRelaxed(fi.size());
                    emit logMessage(QString::fromUtf8("[夸父] 缓存命中｜文件名:%1，直接复用本地文件，跳过网络请求")
                                        .arg(localName));
                    qCInfo(logDownload) << QStringLiteral("[夸父] 缓存命中｜文件名:%1，直接复用本地文件，跳过网络请求")
                                           .arg(localName);
                    // 不发 fileProgress：缓存命中不是网络下载，不应驱动上层 catBytesDl
                    // （否则缓存全命中时 catBytesDl 立即满 → 卡片提前绿色完成态）
                    emit fileFinished(localPath, true);
                    return;
                }
            }
        }

        // Fallback cache check: if file not in working dir, check gameDir cache
        if (!m_cacheFallbackDir.isEmpty() && !m_minecraftDir.isEmpty()
            && localPath.startsWith(m_minecraftDir)) {
            QString relative = localPath.mid(m_minecraftDir.length());
            QString fallbackPath = m_cacheFallbackDir + relative;
            QFileInfo ffi(fallbackPath);
            if (ffi.exists() && ffi.size() > 0) {
                QFile f(fallbackPath);
                if (f.open(QIODevice::ReadOnly)) {
                    QCryptographicHash hash(QCryptographicHash::Sha1);
                    hash.addData(&f);
                    f.close();
                    if (hash.result().toHex() == sha1) {
                        // Cache hit from gameDir! Copy to working dir.
                        // 计数延迟到 start() 统一入账（m_cacheHits），同工作目录缓存命中
                        QDir().mkpath(QFileInfo(localPath).absolutePath());
                        if (QFile::copy(fallbackPath, localPath)) {
                            m_cacheHits.fetchAndAddRelaxed(1);
                            m_cacheBytes.fetchAndAddRelaxed(ffi.size());
                            emit logMessage(QString::fromUtf8("[夸父] 缓存命中｜文件名:%1，直接复用本地文件，跳过网络请求")
                                                .arg(localName));
                            qCInfo(logDownload) << QStringLiteral("[夸父] 缓存命中｜文件名:%1，直接复用本地文件，跳过网络请求")
                                                   .arg(localName);
                            // 不发 fileProgress（同工作目录缓存命中）：避免驱动上层 catBytesDl 提前满
                            emit fileFinished(localPath, true);
                            return;
                        } else {
                            qCWarning(logDownload) << QStringLiteral("[夸父] [缓存] 复制失败 %1 → %2")
                                .arg(fallbackPath).arg(localPath);
                        }
                    }
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
    file->isNoSplit = (!file->isUnknownSize
                       && file->fileSize < (m_modpackMode ? 1LL : 50LL) * 1024 * 1024);

    QMutexLocker lock(&m_filesMutex);
    m_files.append(file);
    m_totalFiles.fetchAndAddRelaxed(1);
    if (file->fileSize > 0) m_totalBytes.fetchAndAddRelaxed(file->fileSize);

    qCInfo(logDownload) << QStringLiteral("[夸父] 任务已排队 名称=%1 队列总数=%2").arg(localName).arg(m_files.size());
}

void FileDownloader::notifyCacheHit(const QString& localPath, qint64 size)
{
    // 调用方已后台预检确认 SHA1 命中：直接计入完成（completed/total/cacheHits），
    // 不读盘不排队——避免主线程批量读盘 hash 卡 UI。
    // 发 fileProgress 让上层分类字节（catBytesDl）计入缓存命中——否则子步骤进度
    // （如“下载支持库”）不含缓存文件，进度到 42% 突然跳完成。
    m_cacheHits.fetchAndAddRelaxed(1);
    m_cacheBytes.fetchAndAddRelaxed(size);
    const QString name = localPath.section(QLatin1Char('/'), -1);
    emit fileProgress(localPath, name, size, size, localPath);
    emit fileFinished(localPath, true);
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
    // 初始化 lastSpeedBytes 为当前网络字节：m_downloadedBytes 仅累加真实网络 IO
    // （缓存命中分支已剔除字节累加），避免首次 speedTick 将缓存文件计入瞬时速度
    m_lastSpeedBytes = m_downloadedBytes.loadRelaxed();
    m_speedFloorBps.storeRelaxed(kMinSpeedFloorBps);
    m_speedRecords.clear();

    // ── 缓存命中统一入账（addFile 阶段只累计 m_cacheHits，start 时再补计数）──
    // 修复：之前 addFile 阶段就 completed++/total++，进度条从高完成度开始；
    // 缓存全命中时 completed==total 但无 worker → allFinished 永不触发 → 下载卡死。
    {
        const int hits = m_cacheHits.fetchAndStoreRelaxed(0);
        if (hits > 0) {
            m_completedFiles.fetchAndAddRelaxed(hits);
            m_totalFiles.fetchAndAddRelaxed(hits);
            // 缓存字节并入总字节（进度条字节显示完整）
            const qint64 cb = m_cacheBytes.fetchAndStoreRelaxed(0);
            m_totalBytes.fetchAndAddRelaxed(cb);
        }
    }

    // ── 无网络任务（全缓存命中）：无 worker 可跑，立即完成 ──
    if (m_files.isEmpty()) {
        qCInfo(logDownload) << QStringLiteral("[夸父] 无待下载文件（全部缓存命中或空任务），直接完成");
        emit logMessage(QStringLiteral("[夸父] 无待下载文件，直接完成"));
        m_state = Idle;
        const int done = m_completedFiles.loadRelaxed();
        const int total = m_totalFiles.loadRelaxed();
        emit progressChanged(done, total,
                              m_downloadedBytes.loadRelaxed(),
                              m_totalBytes.loadRelaxed());
        emit allFinished();
        return;
    }

    // Clear thread pool from any previous runs
    m_threadPool.clear();

    m_managerTimer->start(kManagerTickMs);
    m_speedTimer2->start(kSpeedTickMs);

    emit progressChanged(m_completedFiles.loadRelaxed(), m_totalFiles.loadRelaxed(),
                          m_downloadedBytes.loadRelaxed(), m_totalBytes.loadRelaxed());

    qCInfo(logDownload) << engineBanner("kuafu");
    emit logMessage(engineBanner("kuafu"));
    emit logMessage(QStringLiteral("[夸父] 引擎启动 文件数=%1 最大线程=%2").arg(m_files.size()).arg(m_maxThreads));
}

void FileDownloader::pause()
{
    m_state = Paused;
    m_managerTimer->stop();
    m_speedTimer2->stop();
    emit logMessage(QString::fromUtf8("[夸父] [暂停] 下载已暂停"));
}

void FileDownloader::resume()
{
    if (m_state != Paused) return;
    m_state = Running;
    m_speedTimer.restart();
    m_lastSpeedBytes = m_downloadedBytes.loadRelaxed();   // 网络字节口径（缓存已剔除）
    m_managerTimer->start(kManagerTickMs);
    m_speedTimer2->start(kSpeedTickMs);
    emit logMessage(QStringLiteral("[夸父] 下载已恢复"));
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

    // 清理分片临时文件（.shadow_temp/dl_*.tmp）
    {
        QMutexLocker lock(&m_filesMutex);
        for (auto& f : m_files) {
            for (auto& t : f->threads) {
                if (!t->tempPath.isEmpty()) {
                    QFile::remove(t->tempPath);
                    t->tempPath.clear();
                }
            }
        }
    }

    emit logMessage(QString::fromUtf8("[夸父] [失败] 下载已取消"));
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

// Manager tick — 分阶段调度
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
                if (th) {
                    active++;
                } else if (m_modpackMode) {
                    // 模组专项：host 暂时满/降级被拒 → 文件保持 state 0，
                    // 强制留在 Phase1 下轮重试——否则 allStarted=true 进入 Phase2 后
                    // state 0 文件永不被调度（Phase2 只给已有线程的文件加片）
                    allStarted = false;
                }
            }
        }
        if (allStarted) {
            m_phase = PhaseAccelerate;
            qCInfo(logDownload) << QStringLiteral("[夸父] [调度] 所有文件已启动首线程，进入加速阶段");
        }
        lock.unlock();
        return;
    }

    // Phase 2-3: speed-based thread splitting
    // ── 速度门限（主流启动器语义，2026-08-04 恢复）──
    // 全局下载速度 ≥256KB/s 就不追加分片线程：并发受控，避免把 CDN
    // 打到限流（实测 14 个大文件全开分片 → 官方 CDN 限流 → 集体超时）。
    // 速度不足时才加分片加速——主流启动器 NetTaskSpeedLimitLow=256KB/s。
    if (m_emaMbps * 1024 * 1024 >= kSpeedLimitLowBps)
        return;

    // Add threads to files with large remaining chunks
    for (auto& f : m_files) {
        if (active >= maxThreads) break;
        if (f->state >= 3 || f->state == 5) continue; // merging/finished/failed
        if (m_modpackMode && f->threads.isEmpty() && f->state == 0) {
            // 模组专项双保险：Phase1 已退出但仍有 state 0 文件（极端时序）→ 补启动首线程
            auto th = tryStartFirstThread(f);
            if (th) { active++; continue; }
        }
        // 小文件补启动：Phase1 因线程上限未启动的文件在此补首线程（主流启动器：
        // 等待文件循环持续启动直到线程满；这里每 tick 继续补）。
        if (f->threads.isEmpty() && f->state == 0) {
            auto th = tryStartFirstThread(f);
            if (th) { active++; }
            continue;
        }
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
    // ── 主流启动器 GetSource 语义（2026-08-04）：从源 0 开始顺序找第一个可用源。
    // 源列表 = [首选组(官方或镜像按设置)..., 备选组...]——首选组用完/被禁用
    // 才轮到备选组；不做多源混合分流（哈希 65/35 已废除）。
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
        qCInfo(logDownload) << QStringLiteral("[夸父] [调度] 首源选择 host=%1 file=%2")
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

    emit logMessage(QStringLiteral("[夸父] 启动线程 文件=%1 范围=[%2-%3] 源=%4")
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
    // ── 主流启动器语义：顺序 fallback（源 0 起）+ 镜像禁止分片 ──
    // BMCLAPI 等镜像：主流启动器 明确不分片（TryBeginThread 对 bmclapi 返回 Nothing），
    // 镜像单连接下载 + 限速请求频率（防镜像限流）。分片仅限官方源。
    {
        const QString firstUrl = file->orderedSources.isEmpty()
            ? QString() : file->orderedSources.first();
        if (isMirrorUrl(firstUrl) || file->orderedSources.isEmpty())
            return nullptr;
    }
    int sourceIdx = 0;
    for (int i = 0; i < file->orderedSources.size(); ++i) {
        QString host = extractHost(file->orderedSources[i]);
        if (hostCanAccept(host)) { sourceIdx = i; break; }
        if (i == file->orderedSources.size() - 1) { sourceIdx = i; } // last resort
    }

    // Source selection log (sampled per 20 addition threads)
    static int s_addSrcLogCtr = 0;
    if (sourceIdx > 0 || (++s_addSrcLogCtr % 20 == 0)) {
        qCInfo(logDownload) << QStringLiteral("[夸父] [调度] 附加源选择 file=%1 host=%2")
            .arg(file->localName, extractHost(file->orderedSources[sourceIdx]));
    }

    auto th = std::make_shared<DownloadThread>();
    th->uuid = m_nextUuid.fetchAndAddRelaxed(1);
    th->downloadStart = splitPoint;
    th->downloadEnd = maxPiece->downloadEnd;
    th->sourceUrl = file->orderedSources[sourceIdx];
    maxPiece->downloadEnd = splitPoint;

    qCInfo(logDownload) << QStringLiteral("[夸父] 附加线程 线程号=%1 文件=%2 偏移=%3")
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

    // URL 必须是最后一个 arg：QString::arg 对替换文本中的 %N 递归处理，
    // URL 含 %2B 等编码时会被后续 .arg() 破坏（2026-08-02 实测日志错乱）
    qCInfo(logDownload) << QStringLiteral("[夸父] 开始下载 文件=%2 偏移=%3 URL=%1")
                            .arg(file->localName).arg(th->downloadStart).arg(th->sourceUrl);

    if (!ShadowLauncher::suppressUrlLog())
        emit logMessage(QStringLiteral("[夸父] 开始下载 %1 源=%2").arg(file->localName).arg(th->sourceUrl));

    // Per-worker QNAM — each worker thread creates its own.
    // QNetworkAccessManager is reentrant but NOT thread-safe:
    // a single instance MUST NOT be used from multiple threads.
    QNetworkAccessManager mgr;

    bool sourceOk = false;
    bool modRetriedOnce = false;   // 模组专项：全部源耗尽后重置源列表整体重试一轮（PCL Retried 同款）
    for (int sourceIdx = 0; sourceIdx < file->orderedSources.size() && !sourceOk; ++sourceIdx) {
        if (m_cancelled.loadRelaxed()) goto cleanup;

        QString url = file->orderedSources[sourceIdx];
        th->sourceUrl = url;

        if (sourceIdx > 0)
            qCInfo(logDownload) << QStringLiteral("[夸父] 切换到镜像%1 URL=%2").arg(sourceIdx + 1).arg(url);

        sourceOk = false;
        qint64 startTimeMs = getElapsedMs();
        // 镜像限速（主流启动器语义）：BMCLAPI 等镜像每线程请求间隔 100ms，
        // 防镜像源高频请求限流（403/429）。主流启动器: TryBeginThread 对 bmclapi
        // sleep 100ms。
        if (sourceIdx > 0 && isMirrorUrl(url))
            QThread::msleep(100);
        // 模组专项：重置重试轮每源仅 1 次尝试（PCL「逐个重新尝试下载」语义）
        const int attemptLimit = modRetriedOnce ? 1 : 6;
        for (int attempt = 0; attempt < attemptLimit && !sourceOk; ++attempt) {
            if (m_cancelled.loadRelaxed()) goto cleanup;

            int timeoutMs;
            // ── 自适应超时（主流启动器语义）──
            // 主流启动器: Timeout = min(max(ConnectAverage,15000)×(1+FailCount), 30000)
            // 基础 12s（官方 CDN 正常 1-2s 首字节），连续失败次数越多超时越长
            // （上限 30s）——避免死等也避免误杀慢速但存活的连接。
            // setTransferTimeout 是“无数据超时”，持续下载的文件不受影响。
            const bool isOfficialUrl = url.contains("mojang.com") || url.contains("minecraft.net");
            {
                QString host = extractHost(url);
                int fails = 0;
                {
                    QMutexLocker hlock(&m_hostMutex);
                    auto it = m_hostStats.find(host);
                    if (it != m_hostStats.end())
                        fails = it->consecutiveFails;
                }
                const qint64 base = isOfficialUrl ? 12000 : 15000;
                timeoutMs = static_cast<int>(qMin(base * (1 + qMin(fails, 3)), 30000LL));
            }
            // 官方源（mojang.com/minecraft.net）只试 2 次就切镜像：
            // 实测 3 个大文件同时压官方必超时，快速切 BMCLAPI 反而秒下。
            if (isOfficialUrl && attempt >= 2 && (getElapsedMs() - startTimeMs) < 20000)
                break;
            if (attempt >= 2 && file->expectedSha1.isEmpty()
                && (getElapsedMs() - startTimeMs) < 5500) break;
            if (attempt >= kMaxChunkAttempts && (getElapsedMs() - startTimeMs) < 5500) break;
            if (attempt > 0) QThread::msleep(kRetryBackoffMs);

            QNetworkRequest req{QUrl(url)};
            req.setRawHeader("User-Agent", "ShadowLauncher/1.0");
            if (m_modpackMode) {
                // 模组专项：禁 H2（MCIM 镜像对 QNAM H2 连接不稳定，实测 Connection closed）；
                // 请求原始编码（Qt QNAM 无自动 gzip 解压，发 gzip 头会致 SHA1 不符）
                req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
                req.setRawHeader("Accept-Encoding", "identity");
            }
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
            if (m_modpackMode) {
                // 模组专项：空闲无数据超时——每次收到数据包重置，连续 30s 无数据才断；
                // 慢速持续传输的大文件不被整体超时误杀（PCL「无数据才超时」语义）
                timeout.start(kChunkTimeoutMs);
                connect(reply, &QNetworkReply::readyRead, &timeout,
                        [&timeout]() { timeout.start(kChunkTimeoutMs); });
            } else {
                timeout.start(timeoutMs);
            }

            qint64 lastProgressEmitMs = getElapsedMs();

            connect(reply, &QNetworkReply::downloadProgress,
                    [&](qint64 received, qint64 total) {
                qint64 delta = received - th->downloadDone;
                if (delta > 0) {
                    // 首包数据到达 → 线程进入 downloading（state 2）。
                    // 分片调度（managerTick Phase2）以 state<2=prep / state==2=dl 判断
                    // 是否可加片——6129062 基线仅模组模式置 state=2，MC 模式 prep>dl 恒成立
                    // 导致 MC >50MB 文件分片永不触发（分片机制对 MC 是死代码）。
                    // 豁免保留：放开为通用；模组路径行为与基线一致（基线模组本就置 state=2）。
                    if (th->state == 1) th->state = 2;
                    m_downloadedBytes.fetchAndAddRelaxed(delta);
                    th->downloadDone = received;
                }
                th->lastReceiveTime = getElapsedMs();
                qint64 now = getElapsedMs();
                if (now - lastProgressEmitMs >= kProgressEmitThrottleMs) {
                    lastProgressEmitMs = now;
                    emit progressChanged(m_completedFiles.loadRelaxed(), m_totalFiles.loadRelaxed(),
                                          m_downloadedBytes.loadRelaxed(), m_totalBytes.loadRelaxed());
                    // 与 progressChanged 同节奏节流：高并发（模组 12 路 + MC 64 路）下
                    // 每个数据包都跨线程 emit 会灌爆主线程事件队列 → UI 卡死无响应。
                    emit fileProgress(th->sourceUrl, file->localName, received, total, file->localPath);
                }
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
                qCWarning(logDownload) << QStringLiteral("[夸父] 请求失败 URL=%1 错误=%2 重试=%3")
                    .arg(url, timedOut ? QStringLiteral("超时") : reply->errorString())
                    .arg(attempt + 1);
                reply->abort();
                reply->deleteLater();
                if (m_modpackMode) {
                    // 模组专项：网络错误（超时/连接关闭/4xx/5xx）立即放弃当前源换下一个（PCL 禁源策略）
                    th->downloadDone = 0;
                    break;
                }
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
                qCWarning(logDownload) << QStringLiteral("[夸父] 下载数据不完整 URL=%1 预期=%2(Content-Length) 实际=%3")
                    .arg(url).arg(expectedSize).arg(data.size());
                sourceOk = false;
                reply->deleteLater();
                th->downloadDone = 0;
                continue;
            }
            reply->deleteLater();

            // Determine file size on first thread
            // 始终以实际 Content-Length（或收到的数据大小）为准，
            // 修正 addFile 时传入的 expectedSize 可能偏小的问题。
            // 2026-08-02 实测修复：仅响应完整（data.size() >= contentLen）时才更新
            // fileSize/isNoSplit——Modrinth 高峰下首线程可能拿到 404/截断响应
            // （contentLen 偏小），旧逻辑据此把 isNoSplit 翻转为 true → 分片作废
            // + 全量 SHA1 风暴 → 必失败。
            if (th->downloadStart == 0 && data.size() > 0
                && (contentLen <= 0 || data.size() >= contentLen)) {
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
                    file->isNoSplit = (actualSize < (m_modpackMode ? 1LL : 50LL) * 1024 * 1024);
                    th->downloadEnd = actualSize;
                }
            }

            // SHA1 verification for full-download files（只对非分片，放锁外避免长持锁）
            bool isFullDownload = file->isNoSplit || file->isUnknownSize;
            if (isFullDownload && !file->expectedSha1.isEmpty()) {
                const QString dlHash = sha1Hex(data);
                if (dlHash != QString::fromLatin1(file->expectedSha1)) {
                    qCWarning(logDownload) << QStringLiteral("[夸父] SHA1不匹配 URL=%1 预期=%2 实际=%3 (第%4次)")
                        .arg(url).arg(QString::fromLatin1(file->expectedSha1)).arg(dlHash)
                        .arg(attempt + 1);
                    sourceOk = false;
                    th->downloadDone = 0;   // 防续传错位：重试必须从头（2026-08-02 实测修复）
                    if (attempt >= 5) break;
                    continue;
                }
            }

            // ── 截断 + 写盘 + state 更新（与主线程切分互斥）──
            // 竞态修复（2026-08-02 本地 20MB 分片实测发现）：managerTick（主线程）在锁内
            // 切分下载中线程的 downloadEnd，worker 线程此前无锁读 downloadEnd 截断写盘 →
            // 切分与写盘交错时 tempPath 数据基于旧 range，合并后文件缺块/错位。
            // 现整体加 m_filesMutex：锁内读最新 downloadEnd 截断 + 写盘 + state=3。
            {
                QMutexLocker lock(&m_filesMutex);
                if (!file->isUnknownSize && !file->isNoSplit) {
                    // ── Truncate data if thread was split mid-download ──
                    // When tryAddThread splits a running thread, the in-flight HTTP request
                    // may return more data than this thread's current (reduced) range.
                    // Clip to the allocated range to avoid overlapping temp files during merge.
                    // 注意：截取方向取决于服务器行为（2026-08-02 实测修复）——
                    //   206 正确分片响应：data 从 0 开始（分片内容）→ left(threadRange)
                    //   200 忽略 Range 全量：data 从 0 开始（全文件）→ mid(downloadStart, threadRange)
                    // 旧代码一律 mid(downloadStart, …) 对 206 响应取错位置 → temp 空/错位 → 合并缺块。
                    qint64 threadRange = th->downloadEnd - th->downloadStart;
                    if (threadRange > 0 && data.size() > threadRange) {
                        qint64 excess = data.size() - threadRange;
                        const int respStatus = reply->attribute(
                            QNetworkRequest::HttpStatusCodeAttribute).toInt();
                        qCInfo(logDownload) << QStringLiteral("[夸父] 截断多余数据 文件=%1 起始=%2 预期=%3 实际=%4 超额=%5 状态=%6")
                            .arg(file->localName).arg(th->downloadStart).arg(threadRange).arg(data.size()).arg(excess).arg(respStatus);
                        data = (respStatus == 206)
                            ? data.left(threadRange)
                            : data.mid(th->downloadStart, threadRange);
                        // Adjust byte counters: the excess was already counted via downloadProgress
                        th->downloadDone = threadRange;
                        m_downloadedBytes.fetchAndAddRelaxed(-excess);
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
            }
            goto worker_done;
        }

        // 模组专项：全部源耗尽后，重置源列表整体重试一轮（PCL Retried 同款）。
        // 第二轮每源仅 1 次尝试（attemptLimit 已按 modRetriedOnce 收窄）。
        if (!sourceOk && m_modpackMode && !modRetriedOnce) {
            modRetriedOnce = true;
            sourceIdx = -1;   // for 循环 ++ 后回到 0，重走全部源
            qCInfo(logDownload) << QStringLiteral("[夸父] 全部源失败，重置源列表整体重试一轮: %1")
                                   .arg(file->localName);
            continue;
        }
    }

    // Last resort retry（模组专项已重置重试一轮覆盖，跳过原兜底）
    if (!sourceOk && !m_modpackMode && !file->expectedSha1.isEmpty() && file->orderedSources.size() > 0) {
        const QString url = file->orderedSources[0];
        qCInfo(logDownload) << QStringLiteral("[夸父] 最终兜底重试 URL=%1").arg(url);
        for (int attempt = 0; attempt < 3 && !sourceOk; ++attempt) {
            if (m_cancelled.loadRelaxed()) goto cleanup;
            if (attempt > 0) QThread::msleep(kRetryBackoffMs);

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
            if (m_modpackMode) {
                timeout.start(kChunkTimeoutMs);
                connect(reply, &QNetworkReply::readyRead, &timeout,
                        [&timeout]() { timeout.start(kChunkTimeoutMs); });
            } else {
                int timeoutMs = (attempt == 0) ? kFirstAttemptTimeoutMs : kChunkTimeoutMs;
                timeout.start(timeoutMs);
            }

            connect(reply, &QNetworkReply::downloadProgress,
                    [&](qint64 received, qint64 total) {
                qint64 delta = received - th->downloadDone;
                if (delta > 0) {
                    m_downloadedBytes.fetchAndAddRelaxed(delta);
                    th->downloadDone = received;
                }
                th->lastReceiveTime = getElapsedMs();
                qint64 now = getElapsedMs();
                if (now - lastProgressEmitMs >= kProgressEmitThrottleMs) {
                    lastProgressEmitMs = now;
                    emit progressChanged(m_completedFiles.loadRelaxed(), m_totalFiles.loadRelaxed(),
                                          m_downloadedBytes.loadRelaxed(), m_totalBytes.loadRelaxed());
                    // 同主请求：150ms 节流，避免跨线程信号风暴
                    emit fileProgress(th->sourceUrl, file->localName, received, total, file->localPath);
                }
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
                    qCInfo(logDownload) << QStringLiteral("[夸父] 截断多余数据(重试) 文件=%1 起始=%2 预期=%3 实际=%4 超额=%5")
                        .arg(file->localName).arg(th->downloadStart).arg(threadRange).arg(data.size()).arg(excess);
                    data = data.mid(th->downloadStart, threadRange);
                    th->downloadDone = threadRange;
                    m_downloadedBytes.fetchAndAddRelaxed(-excess);
                }
            }

            bool isFullDownload = file->isNoSplit || file->isUnknownSize;
            if (isFullDownload && !file->expectedSha1.isEmpty()) {
                const QString dlHash = sha1Hex(data);
                if (dlHash == QString::fromLatin1(file->expectedSha1)) {
                    qCInfo(logDownload) << QStringLiteral("[夸父] 最终兜底 SHA1匹配 URL=%1").arg(url);
                } else {
                    if (file->fileSize > 0 && data.size() >= file->fileSize) {
                        qCInfo(logDownload) << QStringLiteral("[夸父] 最终兜底 大小匹配，信任数据 URL=%1").arg(url);
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
    emit logMessage(QStringLiteral("[夸父] 所有源均失败 %1").arg(file->localName));

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

    qCInfo(logDownload) << QStringLiteral("[夸父] 线程完成 文件=%1 字节=%2")
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
        // 模组专项：失败分片重下（PCL「丢弃失败分片重下」语义）——
        // 24 路并发下单片网络抖动不应导致整个大文件报废：
        // 失败分片（state=4 且未重试过）重置后重新下载其 range，
        // allDone 会因 state=0 重新计算，重试完成后再进入合并/终判。
        if (m_modpackMode && anyFailed) {
            bool launchedRetry = false;
            {
                QMutexLocker lock(&m_filesMutex);
                for (auto& t : file->threads) {
                    if (t->state == 4 && !t->retried) {
                        t->retried = true;
                        t->state = 0;
                        t->downloadDone = 0;
                        if (!t->tempPath.isEmpty()) QFile::remove(t->tempPath);
                        t->tempPath.clear();
                        launchedRetry = true;
                        m_activeThreads.fetchAndAddRelaxed(1);
                        launchWorker(t, file);   // 重新下载该分片 range（源会重新选择）
                    }
                }
            }
            if (launchedRetry) {
                qCInfo(logDownload) << QStringLiteral("[夸父] 分片失败重下 %1").arg(file->localName);
                return;   // 等重试分片完成
            }
        }

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
                self->logMessage(QString::fromUtf8("[夸父] [失败] 下载失败: %1 (所有源均失败)").arg(file->localName));
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
                emit logMessage(QString("[夸父] SHA1校验失败: %1").arg(file->localName));
                return false;
            }
        }
        return true;
    }

    qCInfo(logDownload) << QStringLiteral("[夸父] 开始合并文件 路径=%1 分段数=%2")
        .arg(file->localPath).arg(file->threads.size());

    QDir().mkpath(QFileInfo(file->localPath).absolutePath());
    QFile out(file->localPath);
    if (!out.open(QIODevice::WriteOnly)) {
        emit logMessage(QString("[夸父] 无法写入: %1").arg(file->localPath));
        return false;
    }

    // 按偏移排序合并——不能用 uuid（创建顺序）：tryAddThread 从"最大未完成片"切分，
    // 创建顺序 ≠ 偏移顺序，按 uuid 合并会乱序损坏文件（此路径 MC 下载从未触发，
    // 只有 modpackMode 1MB 阈值下真实大文件才暴露，2026-08-02 本地 20MB 实测发现）
    auto sorted = file->threads;
    std::sort(sorted.begin(), sorted.end(), [](auto& a, auto& b) { return a->downloadStart < b->downloadStart; });

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
            emit logMessage(QString("[夸父] SHA1合并校验失败: %1").arg(file->localName));
            return false;
        }
    }

    qCInfo(logDownload) << QStringLiteral("[夸父] 合并完成 文件=%1 SHA1校验=通过").arg(file->localName);
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

    // 速度口径：仅真实网络收发字节。缓存命中分支不累加 m_downloadedBytes（也不累加
    // m_cacheBytes），本地文件读取/缓存标记数据不参与速度统计；m_cacheBytes 保持 0。
    qint64 now = m_downloadedBytes.loadRelaxed();
    if (now < 0) now = 0;   // 分片重下/截断修正时瞬时回退 → 钳零
    qint64 bytes = now - m_lastSpeedBytes;
    if (bytes < 0) bytes = 0;   // 分片重下/截断修正时 m_downloadedBytes 瞬时回退 → 钳零，避免负速度
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

    // 展示速度 = 滑动窗口线性加权值（30 条 × 100ms ≈ 3s 窗口）：
    // 停流时窗口内连续 0 采样 → 数值 ~3s 内自然滑落归零（PCL 同款语义），
    // 不再叠加 EMA 混合（0.5/0.5 在 100ms 节拍下衰减过陡、观感像跳变）。
    m_emaMbps = currentBps / (1024.0 * 1024.0);

    // ── Speed floor: up on growth, decay on stagnation ──
    qint64 floorLimit = static_cast<qint64>(currentBps * kFloorRatio);
    qint64 currentFloor = m_speedFloorBps.loadRelaxed();
    const qint64 nowMs = getElapsedMs();
    if (currentBps >= kMinSpeedFloorBps && floorLimit > currentFloor) {
        m_speedFloorBps.storeRelaxed(floorLimit);
        m_lastFloorIncreaseMs = nowMs;
        // 只在跨过整 MB 时打日志（避免每次微增刷屏）
        if (floorLimit / (1024 * 1024) > currentFloor / (1024 * 1024)) {
            qCInfo(logDownload) << QStringLiteral("[夸父] 速度下限 %1 M").arg(floorLimit / (1024.0 * 1024.0), 0, 'f', 2);
        }
    } else if (nowMs - m_lastFloorIncreaseMs > kFloorDecayMs && currentFloor > kMinSpeedFloorBps) {
        // 5s without growth: decay floor by 50%
        qint64 newFloor = qMax(kMinSpeedFloorBps, currentFloor / 2);
        m_speedFloorBps.storeRelaxed(newFloor);
        m_lastFloorIncreaseMs = nowMs;
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
        emit logMessage(QString::fromUtf8("[夸父] [完成] 下载完成: %1/%2 文件, %3, 速度 %4 MB/s")
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

bool FileDownloader::isMirrorUrl(const QString& url)
{
    // 镜像判定（主流启动器语义）：BMCLAPI 及各类镜像站不分片、限速请求
    const QString u = url.toLower();
    return u.contains(QStringLiteral("bmclapi"))
        || u.contains(QStringLiteral("bangbang93"))
        || u.contains(QStringLiteral("mcimirror"))
        || u.contains(QStringLiteral("mcbbs"))
        || u.contains(QStringLiteral("tuna"))
        || u.contains(QStringLiteral("ustc"))
        || u.contains(QStringLiteral("aliyun"))
        || u.contains(QStringLiteral("tencent"))
        || u.contains(QStringLiteral("huawei"));
}


bool FileDownloader::hostCanAccept(const QString& host) const
{
    QMutexLocker lock(&m_hostMutex);
    auto it = m_hostStats.find(host);
    if (it == m_hostStats.end()) return true;
    // 模组专项：单镜像 host 场景跳过 degraded 拦截——短时连续失败若标记降级，
    // 会导致后续所有文件无法启动、整队列卡死（实测 129 文件尾部大文件全部拒启）；
    // MC 多源场景保留 degraded 源隔离（降级源不参与新连接）
    if (it->degraded && !m_modpackMode) return false;
    // per-host 上限跟随用户设置的最大线程数（m_maxThreads 已由用户配置流入）；
    // 模组专项：MCIM 镜像实测 16+ 会限流饿死连接，固定 12 温和稳定
    const int limit = m_modpackMode ? 12 : qMax(1, m_maxThreads);
    if (it->activeRequests >= limit) return false;
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
