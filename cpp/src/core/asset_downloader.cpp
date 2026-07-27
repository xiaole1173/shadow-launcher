// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173

#include "asset_downloader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHttp2Configuration>
#include <QCryptographicHash>
#include <QUrl>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logAssetDownload, "ShadowDownloader.Asset")

namespace {
QString formatSize(qint64 bytes)
{
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    return QStringLiteral("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
}
} // anonymous namespace

// ═══════════════════════════════════════════════════════════
AssetDownloader::AssetDownloader(QObject* parent)
    : QObject(parent)
{
    setupNam();
}

AssetDownloader::~AssetDownloader()
{
    cancel();
}

// ═══════════════════════════════════════════════════════════
// Setup: create QNAM with HTTP/2, SChannel (Windows native)
// ═══════════════════════════════════════════════════════════
void AssetDownloader::setupNam()
{
    if (m_nam) {
        m_nam->disconnect();
        m_nam->deleteLater();
    }

    m_nam = new QNetworkAccessManager(this);

    // ── HTTP/2 — configured per-request. Enable ALPN negotiation.
    // Windows 10+ SChannel supports HTTP/2 natively via ALPN.
    qCInfo(logAssetDownload) << "QNAM created";
}

// ═══════════════════════════════════════════════════════════
// Entry point
// ═══════════════════════════════════════════════════════════
void AssetDownloader::startDownload(const QVector<AssetTask>& tasks, int maxConcurrent)
{
    if (m_state == Running) {
        qCWarning(logAssetDownload) << "already running, ignoring startDownload()";
        return;
    }

    m_maxConcurrent = qBound(1, maxConcurrent, 256);
    m_pendingQueue.clear();
    m_inFlight.clear();
    m_failedFiles.clear();
    m_failedCount = 0;
    m_completedFiles.storeRelaxed(0);
    m_downloadedBytes.storeRelaxed(0);
    m_totalTaskCount = tasks.size();

    // Pre-compute totals
    qint64 totalEst = 0;
    for (const auto& t : tasks)
        totalEst += t.size;
    m_totalBytes.storeRelaxed(totalEst);
    m_totalFiles.storeRelaxed(tasks.size());

    // Enqueue
    for (const auto& t : tasks)
        m_pendingQueue.enqueue(t);

    m_state = Running;
    m_lastProgressEmit.start();

    emit logMessage(QString("AssetDownloader: start %1 files (%2)")
                        .arg(tasks.size()).arg(formatSize(totalEst)));
    emit progressChanged(0, tasks.size(), 0, totalEst);

    // Fire initial batch
    for (int i = 0; i < m_maxConcurrent && !m_pendingQueue.isEmpty(); ++i)
        fireNext();
}

// ═══════════════════════════════════════════════════════════
// Cancel
// ═══════════════════════════════════════════════════════════
void AssetDownloader::cancel()
{
    if (m_state != Running) return;
    m_state = Cancelled;

    // Collect replies into a local list FIRST, then clear the map.
    // This avoids iterator invalidation when abort() fires finished()
    // synchronously and onReplyFinished tries to erase from m_inFlight.
    QList<QNetworkReply*> toAbort;
    for (auto it = m_inFlight.begin(); it != m_inFlight.end(); ++it)
        toAbort.append(it.key());

    m_inFlight.clear();
    m_pendingQueue.clear();

    // Now abort — onReplyFinished won't find anything in m_inFlight
    // and will return immediately.
    for (auto* reply : toAbort) {
        reply->abort();
        reply->deleteLater();
    }

    emit logMessage("AssetDownloader: cancelled");

    // VersionDownloader relies on allFinished to unblock
    emit allFinished(false, m_failedCount, m_failedFiles);
}

// ═══════════════════════════════════════════════════════════
// Fire the next pending request
// ═══════════════════════════════════════════════════════════
void AssetDownloader::fireNext()
{
    if (m_state != Running || m_pendingQueue.isEmpty()) return;

    AssetTask task = m_pendingQueue.dequeue();

    if (task.mirrors.isEmpty()) {
        finishDownload(task, false);
        return;
    }

    // SHA1 pre-check: skip if file already exists with matching hash
    if (!task.sha1.isEmpty()) {
        QFileInfo fi(task.savePath);
        if (fi.exists() && fi.size() > 0) {
            QFile f(task.savePath);
            if (f.open(QIODevice::ReadOnly)) {
                QCryptographicHash hash(QCryptographicHash::Sha1);
                hash.addData(&f);
                f.close();
                if (hash.result().toHex() == task.sha1) {
                    qCInfo(logAssetDownload) << "  [cache hit]" << task.sha1;
                    finishDownload(task, true);
                    return;
                }
            }
        }
    }

    // Ensure parent dir exists
    QDir().mkpath(QFileInfo(task.savePath).absolutePath());

    // Fire primary URL
    const QString& url = task.mirrors.first();
    QUrl qurl(url);
    QNetworkRequest req(qurl);
    req.setRawHeader("User-Agent", "ShadowLauncher/1.0");
    req.setTransferTimeout(30000);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);

    // Configure HTTP/2 for multiplexed streaming
    QHttp2Configuration h2;
    h2.setSessionReceiveWindowSize(256 * 1024 * 1024);
    h2.setStreamReceiveWindowSize(16 * 1024 * 1024);
    h2.setHuffmanCompressionEnabled(true);
    req.setHttp2Configuration(h2);

    QNetworkReply* reply = m_nam->get(req);

    InFlight ift;
    ift.task = task;
    ift.mirrorIndex = 0;
    m_inFlight.insert(reply, ift);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onReplyFinished(reply);
    });
}

// ═══════════════════════════════════════════════════════════
// Reply handler
// ═══════════════════════════════════════════════════════════
void AssetDownloader::onReplyFinished(QNetworkReply* reply)
{
    reply->deleteLater();

    auto it = m_inFlight.find(reply);
    if (it == m_inFlight.end()) return;

    InFlight ift = it.value();
    m_inFlight.erase(it);

    if (m_state == Cancelled) {
        checkAllFinished();
        return;
    }

    const AssetTask& task = ift.task;

    if (reply->error() != QNetworkReply::NoError) {
        int nextIdx = ift.mirrorIndex + 1;
        if (nextIdx < task.mirrors.size()) {
            qCWarning(logAssetDownload)
                << "  [retry]" << task.sha1 << "failed(" << reply->errorString()
                << "), switching to mirror[" << nextIdx << "]";
            AssetTask retryTask = task;
            retryTask.mirrors = task.mirrors.mid(nextIdx);
            m_pendingQueue.prepend(retryTask);
            fireNext();
        } else {
            qCWarning(logAssetDownload)
                << "  [fail]" << task.sha1 << "all mirrors exhausted ("
                << reply->errorString() << ")";
            finishDownload(task, false);
        }
        return;
    }

    QByteArray data = reply->readAll();

    // SHA1 verification
    if (!task.sha1.isEmpty()) {
        QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex();
        if (hash != task.sha1) {
            int nextIdx = ift.mirrorIndex + 1;
            if (nextIdx < task.mirrors.size()) {
                qCWarning(logAssetDownload)
                    << "  [retry]" << task.sha1
                    << "SHA1 mismatch, switching to mirror[" << nextIdx << "]";
                AssetTask retryTask = task;
                retryTask.mirrors = task.mirrors.mid(nextIdx);
                m_pendingQueue.prepend(retryTask);
                fireNext();
            } else {
                qCWarning(logAssetDownload)
                    << "  [fail]" << task.sha1 << "SHA1 mismatch (all mirrors)";
                finishDownload(task, false);
            }
            return;
        }
    }

    // Write to disk
    QFile f(task.savePath);
    if (!f.open(QIODevice::WriteOnly)) {
        qCWarning(logAssetDownload)
            << "  [fail]" << task.sha1 << "cannot write:" << f.errorString();
        finishDownload(task, false);
        return;
    }
    f.write(data);
    f.close();

    qCInfo(logAssetDownload)
        << "  [done]" << task.sha1 << "(" << formatSize(data.size()) << ")";

    finishDownload(task, true);
}

// ═══════════════════════════════════════════════════════════
// Finish tracking for one task
// ═══════════════════════════════════════════════════════════
void AssetDownloader::finishDownload(const AssetTask& task, bool success)
{
    if (success) {
        m_completedFiles.fetchAndAddRelaxed(1);
        m_downloadedBytes.fetchAndAddRelaxed(task.size);
    } else {
        m_failedCount++;
        m_failedFiles.append(task.sha1);
        m_completedFiles.fetchAndAddRelaxed(1);
    }

    // Throttled progress emit
    qint64 now = m_lastProgressEmit.elapsed();
    if (now >= kProgressThrottleMs) {
        m_lastProgressEmit.start();
        emit progressChanged(m_completedFiles.loadRelaxed(),
                             m_totalFiles.loadRelaxed(),
                             m_downloadedBytes.loadRelaxed(),
                             m_totalBytes.loadRelaxed());
    }

    emit fileCompleted(task.sha1, success);

    fireNext();
    checkAllFinished();
}

// ═══════════════════════════════════════════════════════════
// Check completion
// ═══════════════════════════════════════════════════════════
void AssetDownloader::checkAllFinished()
{
    if (m_state != Running && m_state != Cancelled) return;

    if (!m_pendingQueue.isEmpty() || !m_inFlight.isEmpty())
        return;

    m_state = (m_state == Cancelled) ? Cancelled : Done;

    emit progressChanged(m_completedFiles.loadRelaxed(),
                         m_totalFiles.loadRelaxed(),
                         m_downloadedBytes.loadRelaxed(),
                         m_totalBytes.loadRelaxed());

    if (m_state == Cancelled) {
        emit logMessage("AssetDownloader: cancelled");
        emit allFinished(false, m_failedCount, m_failedFiles);
    } else {
        bool ok = (m_failedCount == 0);
        emit logMessage(QString("AssetDownloader: done - %1 succeeded, %2 failed")
                            .arg(m_totalTaskCount - m_failedCount).arg(m_failedCount));
        emit allFinished(ok, m_failedCount, m_failedFiles);
    }
}

// ═══════════════════════════════════════════════════════════
QString AssetDownloader::sha1HexOf(const QByteArray& data)
{
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex());
}
