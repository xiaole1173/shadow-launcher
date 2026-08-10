// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
//
// Asset-dedicated download engine v2 — speed-adaptive, multi-connection.
//
// Design:
//   - 2 persistent QNAMs (HTTP/1.1, 255 conn/host) avoid HTTP/2 stream limits
//   - Speed-based adaptive concurrency: monitor throughput, adjust inflight count
//   - Three-phase acceleration: burst → accelerate → steady
//   - SHA1+disk I/O offloaded to QThreadPool (non-blocking main thread)
//   - Per-host health tracking with consecutive-failure detection
//   - DNS pre-resolution with IP reliability scoring
//   - Sliding-window speed floor (85 % of weighted peak) prevents over-threading

#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QAtomicInt>
#include <QAtomicInteger>
#include <QElapsedTimer>
#include <QStringList>
#include <QVector>
#include <QQueue>
#include <QTimer>
#include <QThreadPool>
#include <QMutex>
#include <QMap>
#include <QSet>
#include <QHostInfo>

#include "utils/types.h"

// ────────────────────────────────────────────────────────────────────────────
class AssetDownloader : public QObject {
    Q_OBJECT
    Q_PROPERTY(int completedFiles READ completedFiles NOTIFY progressChanged)
    Q_PROPERTY(int totalFiles READ totalFiles NOTIFY progressChanged)
    Q_PROPERTY(qint64 downloadedBytes READ downloadedBytes NOTIFY progressChanged)
    Q_PROPERTY(qint64 totalBytes READ totalBytes NOTIFY progressChanged)

public:
    struct AssetTask {
        QString savePath;
        QString sha1;
        QStringList mirrors;   // download URLs, ordered by priority
        qint64 size = 0;
    };

    explicit AssetDownloader(QObject* parent = nullptr);
    ~AssetDownloader() override;

    void setMaxConcurrent(int n) { m_maxConcurrent = qBound(4, n, 256); }
    void setSpeedLimitMB(double mbps) { m_speedLimitMB = mbps; }
    /// 工作目录（merged 场景为 UUID 临时目录）——用于缓存 fallback 相对路径推导（2026-08-05）
    void setMinecraftDir(const QString& dir) { m_minecraftDir = dir; }
    /// If a file is not found in the working dir (tempDir), check this fallback
    /// dir (gameDir) for a matching SHA1 and copy it locally instead of re-downloading.
    void setFallbackCacheDir(const QString& dir) { m_fallbackCacheDir = dir; }

    void startDownload(const QVector<AssetTask>& tasks, int maxConcurrent = 16);
    /// 运行中追加任务（m_pendingQueue 追加，dispatch 循环会消费）。
    /// 用于：阶段 A 小库文件先启动，阶段 B assets 就绪后追加同引擎并行。
    void appendTasks(const QVector<AssetTask>& tasks);
    void cancel();
    bool isRunning() const { return m_state == Running; }

    // --- Stats ---
    int completedFiles() const { return m_completedFiles.loadRelaxed(); }
    int totalFiles() const { return m_totalFiles.loadRelaxed(); }
    qint64 downloadedBytes() const { return m_downloadedBytes.loadRelaxed(); }
    qint64 totalBytes() const { return m_totalBytes.loadRelaxed(); }
    /// Bytes from cache hits (excluded from speed calculation).
    qint64 cachedBytes() const { return m_cacheBytes.loadRelaxed(); }
    /// Network-only bytes (total - cache).
    qint64 networkBytes() const { return m_downloadedBytes.loadRelaxed() - m_cacheBytes.loadRelaxed(); }
    double currentSpeedMBps() const { return m_emaMbps; }

signals:
    void progressChanged(int completedFiles, int totalFiles,
                         qint64 downloadedBytes, qint64 totalBytes);
    void fileCompleted(const QString& fileName, bool success);
    void fileProgress(const QString& url, const QString& fileName,
                      qint64 received, qint64 total,
                      const QString& savePath);
    void allFinished(bool success, int failedCount, const QStringList& failedFiles);
    void logMessage(const QString& msg);

private slots:
    void onReplyFinished(QNetworkReply* reply);

private:
    // ── Lifecycle ──
    void setupNam();
    void fireNext();
    void finishDownload(const AssetTask& task, bool success);
    void checkAllFinished();

    // ── Speed-adaptive scheduler (replaces old rampTick) ──
    void accelTick();                      // called by m_accelTimer
    void watchdogTick();                   // called by m_watchdogTimer（2026-08-10）
    void schedulePhase();
    int  currentInflight() const;
    bool shouldThrottleSpeed() const;
    /// 缓存预检查（startDownload 与 appendTasks 共用，2026-08-05）：
    /// savePath 或 fallback 存在且大小匹配 → enqueuePreCheck，返回 true
    bool enqueueCachePreCheck(const AssetTask& task);

    // ── Per-host health ──
    struct HostStats {
        int  activeRequests = 0;
        int  consecutiveFails = 0;
        int  totalFails = 0;
        int  totalSuccess = 0;
        qint64 avgFirstByteMs = 15000;     // running average, start conservative
        bool  degraded = false;            // skip if too many fails
        qint64 degradedAtMs = 0;           // 降级时间戳（2026-08-05：超过 kDegradeRetryMs 自动恢复，防永久卡死）
        int  dynamicLimit = 4;             // current dynamic per-host limit
    };
    QString extractHost(const QString& url) const;
    bool hostCanAccept(const QString& host) const;
    void recordHostResult(const QString& host, bool ok, qint64 firstByteMs);
    /// Per-host connection limit — dynamically adjusted.
    /// BMCLAPI: separate limit; others use host-specific dynamic limit.
    int  getHostLimit(const QString& host) const;
    void adjustHostLimits();              // called periodically to tune limits

    // ── 全源不可用冷却（2026-08-05）──
    // 所有镜像 degraded/限流时：fireNext 每 50ms 扫全队列无意义（锁竞争 + 无日志），
    // 改为冷却 kAllBlockedRetryMs 后重试；持续不可用超过 kAllBlockedFailMs 判定失败收尾。
    static constexpr qint64 kDegradeRetryMs = 30000;   // degraded 自动恢复周期（30s）
    static constexpr qint64 kAllBlockedRetryMs = 5000; // 全拒冷却后重试（5s）
    static constexpr qint64 kAllBlockedFailMs = 120000; // 全拒持续 120s → 剩余任务失败收尾
    qint64 m_blockedUntilMs = 0;           // 全拒冷却截止时间
    qint64 m_allBlockedSinceMs = 0;        // 首次全拒时间（持续超时用）
    qint64 m_lastBlockedLogMs = 0;         // 全拒日志节流

    // ── DNS + IP reliability ──
    struct IPInfo {
        QStringList addresses;             // resolved IPs
        QMap<QString, double> reliability; // IP → score (-1..+0.5)
        qint64 lastResolveMs = 0;
    };

    // ── Speed monitoring ──
    void sampleSpeed();
    void updateSpeedFloor();
    /// Reset speed baseline after burst cache-hits, so initial reading is correct
    void resetSpeedBaseline();

    // ── I/O offload (thread pool) ──
    struct IOTask {
        AssetTask task;
        QByteArray data;
    };
    void enqueueIO(const AssetTask& task, const QByteArray& data);

    // ── Cache fallback ──
    QString m_minecraftDir;        // 工作目录（tempDir for merged；2026-08-05）

    // ── Async SHA1 pre-check (offloaded to IO pool, avoids main-thread blocking) ──
    /// checkPath: path to verify SHA1 at (default: task.savePath).
    /// If checkPath != task.savePath and SHA1 matches, copies checkPath → savePath.
    void enqueuePreCheck(const AssetTask& task, const QString& checkPath = QString());
    struct PreCheckPending {
        AssetTask task;
        qint64 enqueueAtMs = 0;
    };
    QMap<QString, PreCheckPending> m_pendingPreCheck;  // sha1 → task
    void onPreCheckResult(const AssetTask& task, bool sha1Match);

    // ── Async DNS resolution ──
    struct DnsPending {
        QString host;
        int refCount = 0;
    };
    QMap<QString, DnsPending> m_dnsToResolve;  // host → pending state
    int  m_dnsPendingCount = 0;
    bool m_dnsAllResolved = false;
    void startAsyncDns();
    void checkAllDnsResolved();

    // ── State ──
    enum State { Idle, Running, Cancelled, Done };
    State m_state = Idle;
    int   m_totalTaskCount = 0;

    // ── Phase management ──
    enum Phase { PhaseInit, PhaseBurst, PhaseAccelerate, PhaseSteady, PhaseCooldown };
    Phase m_phase = PhaseInit;
    QTimer* m_accelTimer = nullptr;        // fires every 50ms during accel
    QTimer* m_watchdogTimer = nullptr;     // 挂起看门狗 1s（独立于 accelTick：队列空时 accelTick 会停）
    static constexpr int kAccelIntervalMs = 50;
    static constexpr int kWatchdogIntervalMs = 1000;
    static constexpr qint64 kInFlightStallMs = 30000;  // in-flight 无字节进展 30s → abort 换源

    // ── Concurrency ──
    int  m_maxConcurrent = 16;
    int  m_targetInflight = 0;             // computed by schedulePhase()
    int  m_burstSent = 0;                  // count of initial burst requests
    int  m_preCheckQueued = 0;             // count of async SHA1 pre-checks in flight
    static constexpr int kBurstSize = 4;   // immediate burst (reduced from 12, servers throttle >4)
    static constexpr int kAccelStep = 2;   // add per tick during accelerate (reduced from 4)
    static constexpr int kMaxPerHost = 4;  // max concurrent per host (reduced from 8)

    // ── Network managers (2 × HTTP/1.1 to avoid HTTP/2 stream limits) ──
    QNetworkAccessManager* m_nam[2] = {nullptr, nullptr};
    int  m_namRoundRobin = 0;              // simple round-robin

    // ── Task queues ──
    QQueue<AssetTask> m_pendingQueue;
    struct InFlight {
        AssetTask task;
        int  mirrorIndex = 0;
        int  namIndex = 0;                 // which QNAM handled it
        qint64 startMs = 0;                // for timing
        qint64 progressBytes = 0;          // bytes tracked via downloadProgress (增量)
        qint64 lastProgressMs = 0;         // 最近一次字节进展时刻（挂起看门狗用，2026-08-10）
        bool  watchdogAborted = false;     // 看门狗已 abort（跳过 host 失败记录，防误伤源）
    };
    QMap<QNetworkReply*, InFlight> m_inFlight;
    int  m_failedCount = 0;
    QStringList m_failedFiles;
    int  m_cacheHitCount = 0;

    // ── Stats ──
    QAtomicInt m_completedFiles{0};
    QAtomicInt m_totalFiles{0};
    QAtomicInteger<qint64> m_totalBytes{0};
    QAtomicInteger<qint64> m_downloadedBytes{0};
    QAtomicInteger<qint64> m_lastSampleBytes{0};
    QAtomicInteger<qint64> m_cacheBytes{0};  // bytes from cache hits (excluded from speed calc)
    QString m_fallbackCacheDir;            // gameDir to check for existing files before downloading
    QElapsedTimer m_speedTimer;

    // ── Speed floor (adaptive, 85 % of weighted peak) ──
    static constexpr int   kMaxSpeedRecords = 20;
    static constexpr qint64 kMinSpeedFloorBps = 256LL * 1024;
    mutable QMutex m_speedMutex;
    QList<qint64> m_speedRecords;          // B/s, newest first
    QAtomicInteger<qint64> m_speedFloorBps{kMinSpeedFloorBps};
    double m_emaMbps = 0.0;
    double m_speedLimitMB = -1.0;

    // ── Per-host tracking ──
    mutable QMutex m_hostMutex;
    QMap<QString, HostStats> m_hostStats;

    // ── DNS / IP reliability (built-up async from lookupHost results) ──
    mutable QMutex m_dnsMutex;
    QMap<QString, IPInfo> m_dnsCache;
    static constexpr qint64 kDnsCacheMs = 300000;     // 5 min

    // ── I/O thread pool ──
    QThreadPool m_ioPool;
    static constexpr int  kMaxIOWorkers = 4;

    // ── Throttle progress signals ──
    QElapsedTimer m_lastProgressEmit;
    static constexpr int kProgressThrottleMs = 150;

    // ── Detailed logging ──
    QElapsedTimer m_downloadTimer;
    void logState(const char* event);
    void logSpeed();  // rate-limited speed snapshot (1s interval)
    QElapsedTimer m_speedLogTimer;
};
