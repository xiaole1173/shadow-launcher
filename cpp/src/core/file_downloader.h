// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
//
// Per-file download engine (v8).
//
// Architecture:
//   - NO pre-created chunk pool
//   - Each file manages its own thread chain
//   - Manager scans files every 50ms, starts/expands threads as needed
//   - Threads split remaining work dynamically (40% of largest unfinished piece)
//   - Speed floor: weighted average × 85%

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
#include <memory>
#include <QTimer>

class QThread;

namespace ShadowDownloader {

// ── Download source: one URL with fail-tracking ──
struct DownloadSource {
    QString url;
    int failCount = 0;
    bool isDead = false;
};

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

    qint64 downloadUndone() const { return downloadEnd - downloadStart - downloadDone; }
};

// ── Per-file download state ──
struct FileDownload {
    QString localPath;
    QString localName;
    qint64 fileSize = -2;      // -2=unknown, -1=cannot determine
    bool isUnknownSize = false;
    bool isNoSplit = false;    // files < 1MB, single range
    int state = 0;             // 0=waiting,1=connecting,2=downloading,3=merging,4=finished,5=failed
    QStringList orderedSources;
    QList<std::shared_ptr<DownloadThread>> threads;
    QByteArray expectedSha1;
    bool needsJarStrip = false;

    qint64 totalDone() const {
        qint64 sum = 0;
        for (auto& t : threads) sum += t->downloadDone;
        return sum;
    }
};

// ── Stats ──
struct DownloadStats {
    QAtomicInt totalFiles{0};
    QAtomicInt completedFiles{0};
    QAtomicInt failedFiles{0};
    QAtomicInteger<qint64> downloadedBytes{0};
    QAtomicInteger<qint64> totalBytes{0};
    QAtomicInt activeThreads{0};
    int maxThreads = 64;
};

// ── Main downloader ──
class FileDownloader : public QObject {
    Q_OBJECT
public:
    explicit FileDownloader(QObject* parent = nullptr);
    ~FileDownloader() override;

    void addFile(const QString& localPath, const QString& localName,
                 const QStringList& sources, qint64 expectedSize = -1,
                 const QByteArray& sha1 = QByteArray(),
                 bool jarStrip = false);
    void start();
    void pause();
    void resume();
    void cancel();

    void setMaxThreads(int n) { m_maxThreads = qBound(1, n, 128); }
    int maxThreads() const { return m_maxThreads; }
    void setSpeedLimitMB(double mb) { m_speedLimitBps.storeRelaxed(static_cast<qint64>(mb * 1024 * 1024)); }

    int completedFiles() const { return m_completedFiles.loadRelaxed(); }
    int totalFiles() const { return m_totalFiles.loadRelaxed(); }
    int failedFiles() const { return m_failedFiles.loadRelaxed(); }
    qint64 downloadedBytes() const { return m_downloadedBytes.loadRelaxed(); }
    qint64 totalBytes() const { return m_totalBytes.loadRelaxed(); }
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

    // Thread limit
    int m_maxThreads = 64;
    QAtomicInteger<qint64> m_speedLimitBps{-1};

    // File tracking
    QList<std::shared_ptr<FileDownload>> m_files;
    QMutex m_filesMutex;
    QAtomicInt m_totalFiles{0};
    QAtomicInt m_completedFiles{0};
    QAtomicInt m_failedFiles{0};
    QAtomicInteger<qint64> m_downloadedBytes{0};
    QAtomicInteger<qint64> m_totalBytes{0};
    QAtomicInt m_activeThreads{0};
    QAtomicInt m_nextUuid{0};

    std::shared_ptr<DownloadThread> tryStartFirstThread(std::shared_ptr<FileDownload> file);
    std::shared_ptr<DownloadThread> tryAddThread(std::shared_ptr<FileDownload> file);
    void runDownloadThread(std::shared_ptr<DownloadThread> th, std::shared_ptr<FileDownload> file);
    bool mergeFile(std::shared_ptr<FileDownload> file);
    void updateStats();

    // Speed tracking
    static constexpr int kMaxSpeedRecords = 30;
    QList<qint64> m_speedRecords;
    QElapsedTimer m_speedTimer;
    qint64 m_lastSpeedBytes = 0;
    mutable QMutex m_speedMutex;
    double m_emaMbps = 0.0;
    QAtomicInteger<qint64> m_speedFloorBps{256 * 1024};
    static constexpr qint64 kMinSpeedFloorBps = 256 * 1024;

    // Timers
    QTimer* m_managerTimer = nullptr;
    QTimer* m_speedTimer2 = nullptr;
    void managerTick();
    void speedTick();

    static QString formatSize(qint64 bytes);
    static qint64 getElapsedMs();
};

} // namespace ShadowDownloader
