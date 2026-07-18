// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
// Shadow Launcher — Single-file Downloader with SHA1, resume, retry
// Phase 2.3: QNetworkAccessManager-based download with progress signals
#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QFile>

#include "utils/types.h"

namespace ShadowLauncher {

class Downloader : public QObject {
    Q_OBJECT

public:
    explicit Downloader(QObject* parent = nullptr);
    ~Downloader() override;

    /// Start downloading a single file. Only one download at a time.
    void start(const DownloadTask& task);
    /// Cancel the current download (no signal emitted).
    void cancel();
    /// Returns true while a download is in progress or retrying.
    bool isDownloading() const;

signals:
    /// Emitted repeatedly during download with byte-level progress.
    void downloadProgress(const QString& name, qint64 received, qint64 total);
    /// Emitted once when download completes (success or final failure).
    void downloadFinished(const QString& name, bool success, const QString& errorMsg);
    /// Emitted when the actual network request begins.
    void downloadStarted(const QString& name);

private slots:
    void onReadyRead();
    void onFinished();
    void onNetworkError(QNetworkReply::NetworkError error);
    void onDownloadProgress(qint64 received, qint64 total);
    void onTimeout();

private:
    void doStart();
    void cleanup();
    bool verifySha1(const QString& filePath, const QString& expected);

    QNetworkAccessManager* m_manager = nullptr;
    QNetworkReply*          m_reply = nullptr;
    QFile*                  m_file = nullptr;
    QTimer*                 m_timeoutTimer = nullptr;

    DownloadTask m_task;
    int          m_retryCount = 0;
    int          m_maxRetries = 3;
    int          m_retryIntervalMs = 2000;   // 2 s between retries
    bool         m_cancelled = false;
    bool         m_downloading = false;
};

} // namespace ShadowLauncher
