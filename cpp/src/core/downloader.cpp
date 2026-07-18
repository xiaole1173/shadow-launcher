// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
// Shadow Launcher — Single-file Downloader implementation
// Phase 2.3

#include "downloader.h"
#include "../utils/logger.h"

#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QNetworkRequest>
#include <QUrl>

namespace ShadowLauncher {

// ────────────────────────────────────────────────────────────
//  Construction / Destruction
// ────────────────────────────────────────────────────────────

Downloader::Downloader(QObject* parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
    , m_timeoutTimer(new QTimer(this))
{
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, &Downloader::onTimeout);
}

Downloader::~Downloader()
{
    cancel();
}

// ────────────────────────────────────────────────────────────
//  Public API
// ────────────────────────────────────────────────────────────

void Downloader::start(const DownloadTask& task)
{
    // Only one download at a time
    if (m_downloading) {
        return;
    }

    m_task        = task;
    m_retryCount  = 0;
    m_cancelled   = false;
    m_downloading = true;

    qCInfo(logDownload) << QStringLiteral("下载任务已创建 文件=%1").arg(task.name)
                        << "url:" << task.url
                        << "sha1:" << (task.sha1.isEmpty() ? QStringLiteral("(none)") : task.sha1.left(16) + QStringLiteral("..."));

    doStart();
}

void Downloader::cancel()
{
    m_cancelled   = true;
    m_retryCount  = m_maxRetries;   // prevent any pending retry from restarting
    m_downloading = false;

    // Cooperative cancellation: do not abort the reply.
    // Let onFinished() fire naturally; it calls the single cleanup() path.
    // This avoids the race between abort-triggered callbacks and manual cleanup.
}

bool Downloader::isDownloading() const
{
    return m_downloading;
}

// ────────────────────────────────────────────────────────────
//  Internal: start the actual HTTP request
// ────────────────────────────────────────────────────────────

void Downloader::doStart()
{
    // Guard: cancel() may have been called while we were waiting for retry timer
    if (m_cancelled) {
        m_downloading = false;
        return;
    }

    // Ensure target directory exists
    const QFileInfo fi(m_task.savePath);
    QDir().mkpath(fi.absolutePath());

    // --- Early-exit: file already exists and SHA1 matches ---
    if (fi.exists() && !m_task.sha1.isEmpty()) {
        if (verifySha1(m_task.savePath, m_task.sha1)) {
            emit downloadStarted(m_task.name);
            emit downloadFinished(m_task.name, true, QString());
            m_downloading = false;
            return;
        }
        // SHA1 mismatch → remove stale file and re-download
        QFile::remove(m_task.savePath);
    }

    emit downloadStarted(m_task.name);

    // Build request
    QNetworkRequest request(QUrl(m_task.url));
    request.setRawHeader("User-Agent", "ShadowLauncher/1.0");
    // Follow redirects (including https→http, which is the safest default)
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    m_reply = m_manager->get(request);

    // Wire signals
    connect(m_reply, &QNetworkReply::readyRead,
            this, &Downloader::onReadyRead);
    connect(m_reply, &QNetworkReply::finished,
            this, &Downloader::onFinished);
    connect(m_reply, &QNetworkReply::downloadProgress,
            this, &Downloader::onDownloadProgress);
    connect(m_reply, &QNetworkReply::errorOccurred,
            this, &Downloader::onNetworkError);

    // Open temporary file (WriteOnly truncates any stale .tmp from a previous attempt)
    const QString tmpPath = m_task.savePath + QStringLiteral(".tmp");
    m_file = new QFile(tmpPath);
    if (!m_file->open(QIODevice::WriteOnly)) {
        emit downloadFinished(m_task.name, false,
                              QStringLiteral("Cannot open file: %1").arg(tmpPath));
        cleanup();
        m_downloading = false;
        return;
    }

    // Arm 30 s timeout (reset on each readyRead)
    m_timeoutTimer->start(30000);
}

// ────────────────────────────────────────────────────────────
//  Slots
// ────────────────────────────────────────────────────────────

void Downloader::onReadyRead()
{
    if (m_cancelled || !m_file || !m_reply) return;

    m_file->write(m_reply->readAll());
    // Reset timeout on every chunk received
    m_timeoutTimer->start(30000);
}

void Downloader::onFinished()
{
    // Network errors are handled by onNetworkError
    if (!m_reply || m_reply->error() != QNetworkReply::NoError) {
        return;
    }

    m_timeoutTimer->stop();

    if (m_cancelled) {
        cleanup();
        // m_downloading already set to false in cancel()
        return;
    }

    // Flush any remaining bytes
    if (m_file) {
        m_file->write(m_reply->readAll());
        m_file->close();
    }

    // Check HTTP status (e.g. 404, 500 — not a network error but still a failure)
    const int statusCode =
        m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (statusCode >= 400) {
        const QString reason =
            m_reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
        const QString errorMsg =
            QStringLiteral("HTTP %1: %2").arg(statusCode).arg(reason);

        if (m_retryCount < m_maxRetries) {
            ++m_retryCount;
            cleanup();
            QTimer::singleShot(m_retryIntervalMs, this, &Downloader::doStart);
            return;
        }

        qCCritical(logDownload) << QStringLiteral("下载失败 文件=%1 错误=%2").arg(m_task.name, errorMsg);
        emit downloadFinished(m_task.name, false, errorMsg);
        cleanup();
        m_downloading = false;
        return;
    }

    // Atomically rename .tmp → final path
    const QString tmpPath = m_task.savePath + QStringLiteral(".tmp");
    if (QFile::exists(m_task.savePath)) {
        QFile::remove(m_task.savePath);
    }
    if (!QFile::rename(tmpPath, m_task.savePath)) {
        emit downloadFinished(m_task.name, false,
                              QStringLiteral("Failed to rename temp file: %1 → %2")
                                  .arg(tmpPath, m_task.savePath));
        cleanup();
        m_downloading = false;
        return;
    }

    // SHA1 verification (only when expected hash is provided)
    if (!m_task.sha1.isEmpty()) {
        if (!verifySha1(m_task.savePath, m_task.sha1)) {
            emit downloadFinished(m_task.name, false,
                                  QStringLiteral("SHA1 verification failed"));
            cleanup();
            m_downloading = false;
            return;
        }
    }

    qCInfo(logDownload) << QStringLiteral("下载完成 文件=%1").arg(m_task.name);
    // Success
    emit downloadFinished(m_task.name, true, QString());
    cleanup();
    m_downloading = false;
}

void Downloader::onNetworkError(QNetworkReply::NetworkError /*error*/)
{
    if (m_cancelled) return;

    const QString errorMsg = m_reply ? m_reply->errorString()
                                     : QStringLiteral("Unknown network error");

    if (m_retryCount < m_maxRetries) {
        ++m_retryCount;
        cleanup();
        QTimer::singleShot(m_retryIntervalMs, this, &Downloader::doStart);
        return;
    }

    qCCritical(logDownload) << QStringLiteral("下载失败 文件=%1 错误=%2").arg(m_task.name, errorMsg);
    emit downloadFinished(m_task.name, false, errorMsg);
    cleanup();
    m_downloading = false;
}

void Downloader::onDownloadProgress(qint64 received, qint64 total)
{
    if (m_cancelled) return;
    emit downloadProgress(m_task.name, received, total);

    // Log at 20% milestones
    if (total > 0) {
        static int lastMilestone = 0;
        int pct = static_cast<int>(received * 100 / total);
        int milestone = (pct / 20) * 20; // 0, 20, 40, 60, 80
        if (milestone > lastMilestone && milestone > 0) {
            lastMilestone = milestone;
            qCInfo(logDownload) << QStringLiteral("下载进度 文件=%1").arg(m_task.name)
                                << milestone << "% (" << received / 1048576
                                << "/" << total / 1048576 << "MB)";
        }
        if (pct == 0) lastMilestone = 0;
        if (pct >= 100) lastMilestone = 0;
    }
}

void Downloader::onTimeout()
{
    if (m_cancelled) return;

    if (m_reply) {
        m_reply->abort();   // triggers onNetworkError → retry logic
    }

    // If abort didn't trigger a retry (e.g. reply already gone), handle here
    if (m_retryCount < m_maxRetries) {
        ++m_retryCount;
        cleanup();
        QTimer::singleShot(m_retryIntervalMs, this, &Downloader::doStart);
        return;
    }

    emit downloadFinished(m_task.name, false,
                          QStringLiteral("Download timed out after 30 s"));
    cleanup();
    m_downloading = false;
}

// ────────────────────────────────────────────────────────────
//  Helpers
// ────────────────────────────────────────────────────────────

void Downloader::cleanup()
{
    // Error suppression: each operation wrapped independently
    // so a failure in one doesn't prevent cleanup of the rest.
    // Equivalent to VB.NET's "On Error Resume Next".

    try { m_timeoutTimer->stop(); } catch (...) {}

    try {
        if (m_reply) {
            m_reply->disconnect();
            m_reply->abort();       // safe even if already finished
            m_reply->deleteLater();
            m_reply = nullptr;
        }
    } catch (...) {}

    try {
        if (m_file) {
            if (m_file->isOpen()) {
                m_file->close();
            }
            m_file->deleteLater();
            m_file = nullptr;
        }
    } catch (...) {}
}

bool Downloader::verifySha1(const QString& filePath, const QString& expected)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QCryptographicHash hash(QCryptographicHash::Sha1);
    if (!hash.addData(&file)) {
        file.close();
        return false;
    }
    file.close();

    const QString actual = QString::fromLatin1(hash.result().toHex());
    return actual.compare(expected, Qt::CaseInsensitive) == 0;
}

} // namespace ShadowLauncher
