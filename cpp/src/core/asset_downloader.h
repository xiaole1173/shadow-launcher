// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
//
// Lightweight asset-dedicated downloader.
//
// Design:
//   - Single shared QNetworkAccessManager with HTTP/2
//   - Async (non-blocking) request model on the main thread
//   - Fires up to maxConcurrent requests simultaneously
//   - All requests to the same host share one HTTP/2 connection → multiplexed

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

#include "utils/types.h"

// ────────────────────────────────────────────────────────────────────────────
/// Lightweight, async, single-threaded downloader optimized for asset objects.
///
/// All I/O runs on the main thread via async QNetworkReply signals.
/// HTTP/2 is configured on the shared QNAM so many small requests are
/// multiplexed over one TCP+TLS connection.
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

    /// Set how many concurrent downloads to allow (default: 32).
    void setMaxConcurrent(int n) { m_maxConcurrent = qBound(1, n, 256); }

    /// Add a batch of asset tasks and start downloading.
    /// @param tasks  List of asset files to download.
    /// @param maxConcurrent  Max concurrent HTTP requests (default 32).
    void startDownload(const QVector<AssetTask>& tasks, int maxConcurrent = 32);

    void cancel();
    bool isRunning() const { return m_state == Running; }

    // --- Stats ---
    int completedFiles() const { return m_completedFiles.loadRelaxed(); }
    int totalFiles() const { return m_totalFiles.loadRelaxed(); }
    qint64 downloadedBytes() const { return m_downloadedBytes.loadRelaxed(); }
    qint64 totalBytes() const { return m_totalBytes.loadRelaxed(); }

signals:
    void progressChanged(int completedFiles, int totalFiles,
                         qint64 downloadedBytes, qint64 totalBytes);
    void fileCompleted(const QString& fileName, bool success);
    void allFinished(bool success, int failedCount, const QStringList& failedFiles);
    void logMessage(const QString& msg);

private slots:
    void onReplyFinished(QNetworkReply* reply);

private:
    void setupNam();
    void fireNext();
    void finishDownload(const AssetTask& task, bool success);
    void checkAllFinished();
    static QString sha1HexOf(const QByteArray& data);

    enum State { Idle, Running, Cancelled, Done };
    State m_state = Idle;

    // HTTP/2-enabled, single, persistent manager
    QNetworkAccessManager* m_nam = nullptr;

    // Queue of pending tasks
    QQueue<AssetTask> m_pendingQueue;

    // Accumulated stats
    int m_totalTaskCount = 0;
    QAtomicInt m_completedFiles{0};
    QAtomicInt m_totalFiles{0};
    QAtomicInteger<qint64> m_totalBytes{0};
    QAtomicInteger<qint64> m_downloadedBytes{0};
    int m_failedCount = 0;
    QStringList m_failedFiles;

    // In-flight tracking (to know which replies belong to which task)
    struct InFlight {
        AssetTask task;
        int mirrorIndex = 0;  // which mirror we're currently trying (index into task.mirrors)
    };
    QMap<QNetworkReply*, InFlight> m_inFlight;
    int m_maxConcurrent = 32;

    // Throttle progress signals (avoid flooding QML)
    QElapsedTimer m_lastProgressEmit;
    static constexpr int kProgressThrottleMs = 150;
};
