// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

#include <QObject>
#include <QVector>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QAtomicInt>
#include <QMap>
#include <QStringList>
#include <QQueue>
#include <QElapsedTimer>
#include <deque>
#include <functional>

#include "utils/types.h"

class QNetworkAccessManager;

namespace ShadowLauncher {

// DownloadSourcePolicy and DownloadConfig → see utils/types.h

/// Multi-file parallel downloader with global chunk pool.
///
/// Design (v6 — configurable source + adaptive speed floor):
///  - All files pre-split into chunks → single atomic counter distributes work
///  - Source order configurable per user settings
///  - Enhanced "speed floor" prevents over-threading (gradually raised to 85% of peak)
///  - Adaptive worker count: starts at 16, ramps up/down based on throughput
///  - Small files (<=1MB): single chunk → downloaded directly to final path
///  - Large files (>1MB): split into ~2MB HTTP Range chunks → merged on completion
///  - BMCLAPI dead detection: after 3 immediate failures, skip to Mojang
///  - Version-level concurrency queue (max 3 concurrent versions)

class ParallelDownloader : public QObject {
    Q_OBJECT
    Q_PROPERTY(int totalFiles READ totalFiles NOTIFY progressChanged)
    Q_PROPERTY(int completedFiles READ completedFiles NOTIFY progressChanged)
    Q_PROPERTY(qint64 totalBytes READ totalBytes NOTIFY progressChanged)
    Q_PROPERTY(qint64 downloadedBytes READ downloadedBytes NOTIFY progressChanged)
    Q_PROPERTY(QString state READ stateStr NOTIFY stateChanged)

public:
    /// @param config  User download configuration (workers, speed limit, source policy)
    /// @param parent  QObject parent
    explicit ParallelDownloader(const DownloadConfig& config = DownloadConfig(),
                                QObject* parent = nullptr);
    ~ParallelDownloader() override;

    // --- Task management ---
    void addTask(const DownloadTask& task);
    void addTasks(const QVector<DownloadTask>& tasks);

    // --- Lifecycle ---
    void start();
    void pause();
    void resume();
    void cancel();

    // --- State queries ---
    int totalFiles() const;
    int completedFiles() const;
    qint64 totalBytes() const;
    qint64 downloadedBytes() const;
    QString stateStr() const;
    bool isRunning() const;

    /// Runtime concurrency override (for settings changes that don't warrant recreating)
    void setUserMaxWorkers(int n) { m_userMaxWorkers = qBound(1, n, 128); }

signals:
    void progressChanged(int completedFiles, int totalFiles,
                         qint64 downloadedBytes, qint64 totalBytes);
    void fileProgress(const QString& url, const QString& fileName, qint64 received, qint64 total, const QString& savePath = QString());
    void fileCompleted(const QString& fileName, bool success);
    /// @param failedFiles  List of file names that failed after all mirror retries
    void allFinished(bool success, int failedCount, const QStringList& failedFiles);
    void logMessage(const QString& msg);
    void stateChanged();

private:
    // --- Chunked download logic ---
    struct ChunkInfo {
        int index;
        qint64 start;
        qint64 end;         // inclusive, or -1 for last
        QString mirrorUrl;  // which mirror serves this chunk
        bool completed = false;
    };

    // ── global chunk pool (v4) ──
    struct PoolChunk {
        int fileIndex;       // index into m_tasks (parent file)
        int chunkIndex;      // which chunk of this file
        int chunkTotal;      // total chunks for parent file
        qint64 start;
        qint64 end;          // inclusive, -1 for last
        QString url;         // primary download URL
        QString chunkPath;   // where to save this chunk (finalPath + ".chunk.N")
        QStringList mirrors; // fallback URLs
        qint64 totalBytes;   // total file bytes (for progress)
        bool completed = false;
    };

    struct FilePoolTracker {
        QString finalPath;
        QString sha1;
        QAtomicInt remainingChunks{0};
        QAtomicInt fileFailed{0};   // 0=pending, 1=any chunk failed → skip merge
    };

    // --- Worker entry point ---
    void runWorker();

    // --- Worker-local download loop (receives persistent QNAM) ---
    bool downloadSingleUrl(QNetworkAccessManager* mgr,
                           const QString& url, const QString& destPath,
                           qint64 rangeStart, qint64 rangeEnd,
                           qint64& bytesReceived);

    // ── chunk pool download (v5) ──
    bool downloadPoolChunk(QNetworkAccessManager* mgr, const PoolChunk& chunk,
                           qint64& bytesReceived);
    void tryMergeAndFinalize(int fileIndex);

    // ── JAR signature stripping (SecurityException fix) ──
    void stripJarSignatures(const QString& jarPath);

    // ── Adaptive concurrency (v6 — adaptive speed floor) ──
    void adjustConcurrency();

    // ── Source ordering ──
    QStringList buildOrderedSources(const QString& primaryUrl,
                                    const QStringList& fallbackUrls) const;
    bool isMojangUrl(const QString& url) const;

    // --- Chunk a file across mirrors ---
    QVector<ChunkInfo> splitIntoChunks(const DownloadTask& task) const;
    // --- SHA1 helpers ---
    static QString sha1Hex(const QString& filePath);
    static bool verifySha1File(const QString& filePath,
                               const QString& expected);
    static QString formatSize(qint64 bytes);

    // --- Internal state ---
    QVector<DownloadTask> m_tasks;
    QAtomicInt m_maxWorkers{16};     // runtime worker count (auto-adjusted)
    int m_userMaxWorkers = 64;        // user-configured ceiling

    // ── global chunk pool (v5) ──
    QVector<PoolChunk> m_chunkPool;            // all chunks from all files, pre-built
    QVector<FilePoolTracker> m_fileTrackers;   // per-file merge tracking (index = fileIndex)

    // Atomic counters
    QAtomicInt m_completedFiles{0};
    QAtomicInteger<qint64> m_totalBytes{0};
    QAtomicInteger<qint64> m_downloadedBytes{0};
    QAtomicInt m_failedCount{0};
    QAtomicInt m_activeWorkers{0};
    QAtomicInt m_nextTask{0};  // for chunk pool: points into m_chunkPool

    enum State { Idle, Running, Paused, Cancelled, Done };
    State m_state = Idle;

    QMutex m_mutex;
    QWaitCondition m_pauseCond;
    bool m_cancelled = false;
    bool m_paused = false;

    // Worker thread handles
    QList<QThread*> m_workers;

    // Chunk size threshold (bytes) — files larger than this get chunked
    static constexpr qint64 kChunkThreshold = 1024 * 1024; // 1 MB
    static constexpr int kMaxChunks = 16;                  // max chunks per large file
    static constexpr qint64 kChunkSizeHint = 2 * 1024 * 1024; // ~2MB per chunk

    // ── Adaptive concurrency (v6) ──
    static constexpr int kInitialWorkers = 16;      // start conservative
    static constexpr int kMinWorkers = 4;
    static constexpr int kMaxWorkers = 128;          // hard ceiling (raised from 64)
    static constexpr int kConcurrencyIntervalMs = 200; // ≈100 ms effective
    int m_concurrencyTick = 0;
    QAtomicInteger<qint64> m_windowBytes{0};         // bytes in current throughput window
    QElapsedTimer m_throughputTimer;                  // window timer
    qint64 m_lastSpeedBytes{0};                       // last m_downloadedBytes checkpoint for speed calc
    double m_emaMbps = 0.0;                            // EMA-smoothed MB/s
    double m_peakMbps = 0.0;                           // highest observed MB/s

    // ── Speed records (v7) ──
    static constexpr int kMaxSpeedRecords = 30;
    mutable QMutex m_speedMutex;
    QList<qint64> m_speedRecords;                     // B/s, newest first (max 30)
    qint64 m_speedLastDone = 0;                       // bytes last record point

    // ── Enhanced speed floor (v6) ──
    // Sticky high-water mark that prevents over-threading.
    // Raised to 85% of observed speed, never below 256 KB/s.
    // When current speed ≥ floor: saturated → don't add workers.
    static constexpr qint64 kMinSpeedFloorBps = 256 * 1024;
    QAtomicInteger<qint64> m_speedFloorBps{kMinSpeedFloorBps};

    // ── Source policy ──
    DownloadSourcePolicy m_fileSource;
    double m_speedLimitMB = -1;  // per-instance speed cap (MB/s)

    // ── BMCLAPI dead detection ──
    QAtomicInt m_bmclapiUnreachable{0};              // 1=BMCLAPI dead, skip to Mojang
    QAtomicInt m_consecutiveBmclapiFails{0};          // consecutive BMCLAPI failures across workers
    QAtomicInt m_mojangSlow{0};                        // 1=Mojang too slow, switch to BMCLAPI (AutoSwitch only)
    QAtomicInt m_consecutiveMojangSlow{0};             // consecutive slow Mojang downloads
    static constexpr int kMojangSlowThreshold = 5;     // N slow downloads = mark slow
    static constexpr qint64 kMojangSlowSpeedBps = 512 * 1024; // 512 KB/s threshold

    // ── Speed limit throttle state ──
    QElapsedTimer m_throttleTimer;
    qint64 m_throttleBytes{0};

    // Failed file names collected during download (for reporting)
    QStringList m_failedFiles;

    // ------------------------------------------------------------------
    // --- Periodic progress timer (200ms throttle) ---
    QTimer* m_progressTimer = nullptr;

    // ------------------------------------------------------------------
    // Version-level concurrency: static queue across ParallelDownloader instances
    // ------------------------------------------------------------------
public:
    /// Maximum concurrent version downloads globally
    static constexpr int kMaxConcurrentVersions = 3;

    /// Register an instance with the version-level queue (returns false if queue full)
    static bool enqueueVersionDownload(ParallelDownloader* instance);
    /// Notify queue that this version's downloads are done
    static void dequeueVersionDownload(ParallelDownloader* instance);
    /// Start the next queued download if any slot is free
    static void processNextQueued();

private:
    // Per-instance "ready to run" flag — set after enqueue and slot is granted
    bool m_queuedAndReady = false;

    // Static queue management
    static QMutex s_queueMutex;
    static QQueue<ParallelDownloader*> s_versionQueue;
    static int s_activeVersionCount;
};

} // namespace ShadowLauncher
