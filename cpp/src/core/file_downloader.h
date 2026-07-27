// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
//
// Per-file download engine (v9).
//
// Architecture (主流启动器-ref):
//   - Per-file thread chain: files > 1MB are split dynamically at 40% of
//     the largest unfinished piece (主流启动器-style).
//   - Shared QNetworkAccessManager with HTTP/1.1 connection pool (255/host).
//   - DNS resolution with IP reliability scoring (round-robin across IPs).
//   - Multi-source round-robin: different threads of the same file use
//     different mirror URLs to distribute server load.
//   - Adaptive timeout: starts at 15 s, grows with per-source fail count.
//   - Speed floor: weighted average x 85%, updated every 100 ms.
//   - Speed limit throttle: per-worker sleep to cap aggregate throughput.

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
    int id = 0;
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
    int sourceId = 0;           // index into file->orderedSources
    QString sourceUrl;
    QString tempPath;
    qint64 lastReceiveTime = 0;
    int state = 0;              // 0=waiting,1=connecting,2=downloading,3=finished,4=failed
    int timeoutMs = 15000;      // adaptive per-source timeout

    qint64 downloadUndone() const { return qMax(0LL, downloadEnd - downloadStart - downloadDone); }
};

// ── Per-file download state ──
struct FileDownload {
    QString localPath;
    QString localName;
    qint64 fileSize = -2;       // -2=unknown, -1=cannot determine
    bool isUnknownSize = false;
    bool isNoSplit = false;     // files < 1MB, single range
    int state = 0;              // 0=waiting,1=connecting,2=downloading,3=merging,4=finished,5=failed
    QStringList sourceUrls;     // original source URLs
    QList<DownloadSource> orderedSources;  // with fail tracking
    QList<std::shared_ptr<DownloadThread>> threads;
    QByteArray expectedSha1;
    bool needsJarStrip = false;

    // Connect timing (主流启动器 ConnectAverage)
    int connectCount = 0;
    qint64 connectTotalMs = 0;
    int connectAverageMs() const { return connectCount > 0 ? (int)(connectTotalMs / connectCount) : -1; }

    std::shared_ptr<DownloadThread> findMaxUndonePiece() const;
    int nextSourceId(bool preferDifferent = false) const;

    qint64 totalDone() const {
        qint64 sum = 0;
        for (auto& t : threads) sum += t->downloadDone;
        return sum;
    }
};

// ── DNS resolver (主流启动器-style IP reliability) ──
struct DnsRecord {
    QString host;
    QString ip;
    double reliability = 0.0;   // -1 to +0.5
};

class DnsResolver {
public:
    /// Resolve host to the best IP (or return raw host if resolution skipped).
    static QString resolve(const QUrl& url, QString& outIp, int& outFamily);
    /// Record success/failure for an IP.
    static void recordReliability(const QString& ip, double score);
    /// Get best IPs for a host, sorted by reliability.
    static QStringList getCandidateIps(const QString& host);

private:
    static QMutex s_mutex;
    static QMap<QString, double> s_ipReliability; // ip → score
    static QMap<QString, qint64> s_dnsFailureTime; // host → last fail epoch ms
    static constexpr qint64 kDnsFailCooldownMs = 60000;
};

// ── Stats (thread-safe atomics) ──
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
    void setSpeedLimitMB(double mb) {
        m_speedLimitBps.storeRelaxed(mb > 0 ? static_cast<qint64>(mb * 1024 * 1024) : -1);
    }

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

    // Note: QNetworkAccessManager is NOT thread-safe in Qt 6.
    // Each thread creates its own QNAM with per-request HTTP/1.1 config.

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
    std::atomic<bool> m_finishedGuard{false};
    int m_bmclapiThisTick = 0;
    static constexpr int kMaxBmclapiPerTick = 4;
    static constexpr qint64 kMinSpeedFloorBps = 256 * 1024;

    // Speed limit throttle state
    QElapsedTimer m_throttleTimer;
    qint64 m_throttleBytes = 0;

    // Timers
    QTimer* m_managerTimer = nullptr;
    QTimer* m_speedTimer2 = nullptr;
    void managerTick();
    void speedTick();

    static QString formatSize(qint64 bytes);
    static qint64 getElapsedMs();
};

} // namespace ShadowDownloader
