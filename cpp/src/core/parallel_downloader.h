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
#include <functional>

#include "utils/types.h"

namespace ShadowLauncher {

/// Multi-file parallel downloader with chunked multi-source segmented download,
/// pause/resume, checkpoint resume, and version-level concurrency queue.
///
/// Key design changes from v2 (mirror-race):
///  - Large files (>1MB) are split into chunks assigned to different mirrors (BMCLAPI, Mojang, MCBBS)
///  - Small files (<=1MB) go directly to Mojang official first
///  - Workers download chunks to .tmp files, then merge at completion
///  - Checkpoint .download_progress.json enables resume across crashes
///  - Version-level queue (max 3 concurrent versions) managed via static counters

class ParallelDownloader : public QObject {
    Q_OBJECT
    Q_PROPERTY(int totalFiles READ totalFiles NOTIFY progressChanged)
    Q_PROPERTY(int completedFiles READ completedFiles NOTIFY progressChanged)
    Q_PROPERTY(qint64 totalBytes READ totalBytes NOTIFY progressChanged)
    Q_PROPERTY(qint64 downloadedBytes READ downloadedBytes NOTIFY progressChanged)
    Q_PROPERTY(QString state READ stateStr NOTIFY stateChanged)

public:
    /// @param maxWorkers  Maximum concurrent download threads (default 8)
    /// @param parent      QObject parent
    explicit ParallelDownloader(int maxWorkers = 8, QObject* parent = nullptr);
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

    /// Set a progress directory for checkpoint files (for resume support)
    void setCheckpointDir(const QString& dir);
    QString checkpointDir() const;

    /// Check for existing resume checkpoints
    bool hasResumeCheckpoint(const QString& taskName) const;

signals:
    void progressChanged(int completedFiles, int totalFiles,
                         qint64 downloadedBytes, qint64 totalBytes);
    void fileProgress(const QString& url, const QString& fileName, qint64 received, qint64 total);
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

    // --- Worker entry point ---
    void runWorker();

    // --- Single-file download dispatcher (decides chunked vs. direct) ---
    bool downloadSingleFile(const DownloadTask& task);

    // --- Direct download (small files, or single chunk) ---
    bool downloadSingleUrl(const QString& url, const QString& destPath,
                           qint64 rangeStart, qint64 rangeEnd,
                           qint64& bytesReceived);

    // --- Chunk a file across mirrors ---
    QVector<ChunkInfo> splitIntoChunks(const DownloadTask& task) const;

    // --- Merge .tmp chunks into final file ---
    bool mergeChunks(const QString& finalPath,
                     const QVector<ChunkInfo>& chunks);

    // --- Checkpoint helpers ---
    void saveCheckpoint(const QString& taskName,
                        const QVector<ChunkInfo>& chunks);
    QVector<ChunkInfo> loadCheckpoint(const QString& taskName) const;
    QString checkpointPath(const QString& taskName) const;
    void clearCheckpoint(const QString& taskName);

    // --- SHA1 helpers ---
    static QString sha1Hex(const QString& filePath);
    static bool verifySha1File(const QString& filePath,
                               const QString& expected);
    static QString formatSize(qint64 bytes);

    // --- Internal state ---
    QVector<DownloadTask> m_tasks;
    int m_maxWorkers;

    // Atomic counters
    QAtomicInt m_completedFiles{0};
    QAtomicInteger<qint64> m_totalBytes{0};
    QAtomicInteger<qint64> m_downloadedBytes{0};
    QAtomicInt m_failedCount{0};
    QAtomicInt m_activeWorkers{0};
    QAtomicInt m_nextTask{0};

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

    // Checkpoint directory for resume info
    QString m_checkpointDir;

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
