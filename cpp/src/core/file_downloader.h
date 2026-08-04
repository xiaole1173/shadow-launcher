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

#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QElapsedTimer>
#include <QAtomicInt>
#include <QAtomicInteger>
#include <QMutex>
#include <QList>
#include <QString>
#include <QMap>
#include <QSet>
#include <QThreadPool>
#include <memory>
#include <QTimer>
#include <QNetworkReply>

class QHostInfo;

namespace ShadowDownloader {

// ── Per-thread download state ──
struct DownloadThread {
    int uuid = 0;
    qint64 downloadStart = 0;
    qint64 downloadEnd = 0;
    qint64 downloadDone = 0;
    QString sourceUrl;
    QString tempPath;
    qint64 lastReceiveTime = 0;
    int state = 0;  // 0=waiting,1=connecting,2=downloading,3=finished,4=failed
    bool retried = false;   // 分片失败已重试（模组专项：丢弃失败分片重下）

    qint64 downloadUndone() const { return downloadEnd - downloadStart - downloadDone; }
};

// ── Per-file download state ──
struct FileDownload {
    QString localPath;
    QString localName;
    qint64 fileSize = -2;      // -2=unknown, -1=cannot determine
    bool isUnknownSize = false;
    bool isNoSplit = false;    // files < 50MB, single range
    int state = 0;             // 0=waiting,1=connecting,2=downloading,3=merging,4=finished,5=failed
    QStringList orderedSources;
    QVector<int> sourceFailCounts;   // per-source 失败计数（主流启动器 NetSource.FailCount 语义）
    bool retriedOnce = false;        // 全源失败后已重置重试一轮（主流启动器 Retried 标志）
    QList<std::shared_ptr<DownloadThread>> threads;
    QByteArray expectedSha1;
    bool needsJarStrip = false;

    qint64 totalDone() const {
        qint64 sum = 0;
        for (auto& t : threads) sum += t->downloadDone;
        return sum;
    }
};

// ── Main downloader ──
// Forward declaration for QRunnable friend
class DownloadWorker;

class FileDownloader : public QObject {
    Q_OBJECT
    friend class DownloadWorker;
public:
    explicit FileDownloader(QObject* parent = nullptr);
    ~FileDownloader() override;

    void addFile(const QString& localPath, const QString& localName,
                 const QStringList& sources, qint64 expectedSize = -1,
                 const QByteArray& sha1 = QByteArray(),
                 bool jarStrip = false,
                 bool skipCacheCheck = false);
    /// 调用方已后台预检确认文件 SHA1 命中：直接计入完成，不读盘不排队。
    void notifyCacheHit(const QString& localPath, qint64 size);
    void start();
    void pause();
    void resume();
    void cancel();
    void addInFlightReply(QNetworkReply* reply);
    void removeInFlightReply(QNetworkReply* reply);

    void setMaxThreads(int n) { m_maxThreads = qBound(1, n, 128); }
    int maxThreads() const { return m_maxThreads; }
    void setSpeedLimitMB(double mb) { m_speedLimitBps.storeRelaxed(static_cast<qint64>(mb * 1024 * 1024)); }
    /// 模组下载专项模式（ModpackDownloader 实例开启；MC 下载实例保持默认关闭）：
    /// 1) 禁用 HTTP/2（MCIM 镜像 H2 连接不稳，Connection closed 断连）
    /// 2) 分片阈值 50MB → 1MB（PCL 同款，突破单连接限速）
    /// 3) 空闲无数据超时（30s 无数据才断，慢速大文件不被整体超时误杀）
    /// 4) 网络错误立即换源，全部源耗尽后重置源列表整体重试一轮（PCL 失败策略）
    /// 5) Accept-Encoding: identity（Qt 无自动解压，防服务器 gzip 导致 SHA1 不符）
    /// 默认关闭 ⇒ MC 下载行为与既有完全一致。
    void setModpackMode(bool on) { m_modpackMode = on; }
    bool modpackMode() const { return m_modpackMode; }
    /// Set the working Minecraft directory (used for cache fallback path computation).
    void setMinecraftDir(const QString& dir) { m_minecraftDir = dir; }
    /// If a file is not found in the working dir, check this fallback dir for
    /// a matching SHA1 and copy locally instead of re-downloading.
    void setCacheFallbackDir(const QString& dir) { m_cacheFallbackDir = dir; }

    int completedFiles() const { return m_completedFiles.loadRelaxed(); }
    int totalFiles() const { return m_totalFiles.loadRelaxed(); }
    int failedFiles() const { return m_failedFiles.loadRelaxed(); }
    qint64 downloadedBytes() const { return m_downloadedBytes.loadRelaxed(); }
    qint64 totalBytes() const { return m_totalBytes.loadRelaxed(); }
    /// Bytes from cache hits. 缓存命中分支不再累加（保持 0），仅供上层口径兼容。
    qint64 cachedBytes() const { return m_cacheBytes.loadRelaxed(); }
    /// Network-only bytes：m_downloadedBytes 已只含真实网络字节。
    qint64 networkBytes() const { return m_downloadedBytes.loadRelaxed(); }
    int activeThreads() const { return m_activeThreads.loadRelaxed(); }

    double currentSpeedMBps() const;
    bool isRunning() const { return m_state == Running; }

signals:
    void progressChanged(int completedFiles, int totalFiles,
                          qint64 downloadedBytes, qint64 totalBytes);
    void fileProgress(const QString& url, const QString& fileName,
                      qint64 received, qint64 total,
                      const QString& savePath = QString());
    void fileFinished(const QString& localPath, bool success);
    void allFinished();
    void logMessage(const QString& msg);

private:
    enum State { Idle, Running, Paused, Cancelled };
    State m_state = Idle;
    QAtomicInt m_cancelled{0};

    // ── Config ──
    int m_maxThreads = 12;
    QAtomicInteger<qint64> m_speedLimitBps{-1};
    bool m_modpackMode = false;   // 模组下载专项模式（默认关，MC 下载不受影响）

    // ── Cache fallback (gameDir cache for tempDir downloads) ──
    QString m_minecraftDir;       // working dir where files are downloaded to (tempDir for merged)
    QString m_cacheFallbackDir;   // real gameDir cache to check before downloading

    // ── Thread pool (replaces QThread::create) ──
    QThreadPool m_threadPool;

    // (Per-worker QNAM in runWorker — no shared QNAM to avoid thread-safety issues)

    // ── File tracking ──
    QList<std::shared_ptr<FileDownload>> m_files;
    QMutex m_filesMutex;
    QAtomicInt m_totalFiles{0};
    QAtomicInt m_completedFiles{0};
    QAtomicInt m_failedFiles{0};
    QAtomicInt m_cacheHits{0};   // 缓存命中数（addFile 阶段累计，start 时统一入账）
    QAtomicInteger<qint64> m_downloadedBytes{0};  // 仅真实网络收发字节（缓存命中不累加）
    QAtomicInteger<qint64> m_cacheBytes{0};  // 保留字段：缓存命中分支已不累加，恒为 0（不参与速度）
    QAtomicInteger<qint64> m_totalBytes{0};
    QAtomicInt m_activeThreads{0};
    QAtomicInt m_nextUuid{0};

    // ── In-flight reply tracking (for immediate abort on cancel) ──
    mutable QMutex m_inflightMutex;
    QList<QNetworkReply*> m_inflightReplies;

    // ── 分阶段调度 ──
    enum Phase { PhaseFirstThread, PhaseAccelerate, PhaseSteady };
    Phase m_phase = PhaseFirstThread;

    // ── Per-host health ──
    struct HostStats {
        int  activeRequests = 0;
        int  consecutiveFails = 0;
        int  totalFails = 0;
        int  totalSuccess = 0;
        bool degraded = false;
    };
    mutable QMutex m_hostMutex;
    QMap<QString, HostStats> m_hostStats;

    static QString extractHost(const QString& url);
    static bool isMirrorUrl(const QString& url);
    bool hostCanAccept(const QString& host) const;
    void recordHostResult(const QString& host, bool ok);

    // ── DNS + IP reliability ──
    struct IPInfo {
        QStringList addresses;
        QMap<QString, double> reliability;
        qint64 lastResolveMs = 0;
    };
    mutable QMutex m_dnsMutex;
    QMap<QString, IPInfo> m_dnsCache;
    static constexpr qint64 kDnsCacheMs = 300000;
    static constexpr qint64 kDnsFailureBackoffMs = 60000;

    // ── Worker management ──
    std::shared_ptr<DownloadThread> tryStartFirstThread(std::shared_ptr<FileDownload> file);
    std::shared_ptr<DownloadThread> tryAddThread(std::shared_ptr<FileDownload> file);
    void launchWorker(std::shared_ptr<DownloadThread> th, std::shared_ptr<FileDownload> file);
    void runWorker(std::shared_ptr<DownloadThread> th, std::shared_ptr<FileDownload> file);
    bool mergeFile(std::shared_ptr<FileDownload> file);
    void onWorkerDone(std::shared_ptr<FileDownload> file);
    void updateStats();

    // ── Speed tracking ──
    static constexpr int kMaxSpeedRecords = 30;
    // 主流启动器 速度门限（NetTaskSpeedLimitLow=256KB/s）——但 主流启动器 默认走镜像
    //（单连接快），官方源单连接仅 ~300KB/s 时 256KB/s 门限永远不触发分片
    // → 大文件单连接龟速（用户实测中期 <1MB/s）。调高到 4MB/s：
    // 低于期望速度就分片（官方多连接补偿单连接慢），速度到 4MB/s 自然停
    //（per-host/线程上限仍防爆）。
    static constexpr qint64 kSpeedLimitLowBps = 4LL * 1024 * 1024;
    QList<qint64> m_speedRecords;
    QElapsedTimer m_speedTimer;
    qint64 m_lastSpeedBytes = 0;
    mutable QMutex m_speedMutex;
    double m_emaMbps = 0.0;
    qint64 m_instantBps = 0;   // 最近一次差分速度（主流启动器 Speed 语义，200ms 采样）——速度门限用
    QAtomicInteger<qint64> m_speedFloorBps{256 * 1024};
    qint64 m_lastFloorIncreaseMs = 0;
    static constexpr qint64 kMinSpeedFloorBps = 256 * 1024;
    static constexpr double kFloorRatio = 0.85;        // 速度下限 = 加权均值 × kFloorRatio
    static constexpr qint64 kFloorDecayMs = 5000;      // 下限无增长衰减周期
    static constexpr int kManagerTickMs = 20;          // 调度轮询节拍（同主流启动器 20ms）
    static constexpr int kSpeedTickMs = 100;           // 速度采样节拍
    static constexpr int kMaxChunkAttempts = 5;        // 分片最大尝试次数（含首试）
    static constexpr int kRetryBackoffMs = 500;        // 分片重试退避
    static constexpr int kFirstAttemptTimeoutMs = 60000; // 分片首试超时
    static constexpr int kChunkTimeoutMs = 30000;      // 分片重试超时
    static constexpr int kProgressEmitThrottleMs = 150;  // 进度发射节流

    // ── Timers ──
    QTimer* m_managerTimer = nullptr;
    QTimer* m_speedTimer2 = nullptr;
    void managerTick();
    void speedTick();

    static QString formatSize(qint64 bytes);
    static qint64 getElapsedMs();
};

} // namespace ShadowDownloader
