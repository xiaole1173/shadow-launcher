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
    /// If a file is not found in the working dir (tempDir), check this fallback
    /// dir (gameDir) for a matching SHA1 and copy it locally instead of re-downloading.
    void setFallbackCacheDir(const QString& dir) { m_fallbackCacheDir = dir; }

    void startDownload(const QVector<AssetTask>& tasks, int maxConcurrent = 16);
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
    static QString sha1HexOf(const QByteArray& data);

    // ── Speed-adaptive scheduler (replaces old rampTick) ──
    void accelTick();                      // called by m_accelTimer
    void schedulePhase();
    int  currentInflight() const;
    bool shouldThrottleSpeed() const;

    // ── Per-host health ──
    struct HostStats {
        int  activeRequests = 0;
        int  consecutiveFails = 0;
        int  totalFails = 0;
        int  totalSuccess = 0;
        qint64 avgFirstByteMs = 15000;     // running average, start conservative
        bool  degraded = false;            // skip if too many fails
        int  dynamicLimit = 4;             // current dynamic per-host limit
    };
    QString extractHost(const QString& url) const;
    HostStats& hostStats(const QString& host);
    bool hostCanAccept(const QString& host) const;
    void recordHostResult(const QString& host, bool ok, qint64 firstByteMs);
    /// Per-host connection limit — dynamically adjusted.
    /// BMCLAPI: separate limit; others use host-specific dynamic limit.
    int  getHostLimit(const QString& host) const;
    void adjustHostLimits();              // called periodically to tune limits

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
    static constexpr int kAccelIntervalMs = 50;

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
