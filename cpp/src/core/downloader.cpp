// Shadow Launcher 鈥?Single-file Downloader implementation
// Phase 2.3

#include "downloader.h"

#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QNetworkRequest>
#include <QUrl>

namespace ShadowLauncher {

// 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
//  Construction / Destruction
// 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€

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

// 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
//  Public API
// 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€

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

    doStart();
}

void Downloader::cancel()
{
    m_cancelled   = true;
    m_retryCount  = m_maxRetries;   // prevent any pending retry from restarting
    m_downloading = false;

    if (m_reply) {
        m_reply->abort();
    }
    cleanup();
}

bool Downloader::isDownloading() const
{
    return m_downloading;
}

// 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
//  Internal: start the actual HTTP request
// 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€

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
        // SHA1 mismatch 鈫?remove stale file and re-download
        QFile::remove(m_task.savePath);
    }

    emit downloadStarted(m_task.name);

    // Build request
    QNetworkRequest request(QUrl(m_task.url));
    request.setRawHeader("User-Agent", "ShadowLauncher/1.0");
    // Follow redirects (including https鈫抙ttp, which is the safest default)
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

    // Arm 30鈥痵 timeout (reset on each readyRead)
    m_timeoutTimer->start(30000);
}

// 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
//  Slots
// 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€

void Downloader::onReadyRead()
{
    if (!m_file || !m_reply) return;

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

    // Check HTTP status (e.g. 404, 500 鈥?not a network error but still a failure)
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

        emit downloadFinished(m_task.name, false, errorMsg);
        cleanup();
        m_downloading = false;
        return;
    }

    // Atomically rename .tmp 鈫?final path
    const QString tmpPath = m_task.savePath + QStringLiteral(".tmp");
    if (QFile::exists(m_task.savePath)) {
        QFile::remove(m_task.savePath);
    }
    if (!QFile::rename(tmpPath, m_task.savePath)) {
        emit downloadFinished(m_task.name, false,
                              QStringLiteral("Failed to rename temp file: %1 鈫?%2")
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

    emit downloadFinished(m_task.name, false, errorMsg);
    cleanup();
    m_downloading = false;
}

void Downloader::onDownloadProgress(qint64 received, qint64 total)
{
    if (m_cancelled) return;
    emit downloadProgress(m_task.name, received, total);
}

void Downloader::onTimeout()
{
    if (m_reply) {
        m_reply->abort();   // triggers onNetworkError 鈫?retry logic
    }

    // If abort didn't trigger a retry (e.g. reply already gone), handle here
    if (m_retryCount < m_maxRetries) {
        ++m_retryCount;
        cleanup();
        QTimer::singleShot(m_retryIntervalMs, this, &Downloader::doStart);
        return;
    }

    emit downloadFinished(m_task.name, false,
                          QStringLiteral("Download timed out after 30鈥痵"));
    cleanup();
    m_downloading = false;
}

// 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
//  Helpers
// 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€

void Downloader::cleanup()
{
    m_timeoutTimer->stop();

    if (m_reply) {
        m_reply->disconnect();
        m_reply->abort();       // safe even if already finished
        m_reply->deleteLater();
        m_reply = nullptr;
    }

    if (m_file) {
        if (m_file->isOpen()) {
            m_file->close();
        }
        m_file->deleteLater();
        m_file = nullptr;
    }
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

// Qt MOC 鈥?must be included at end of the .cpp when the header lives in the same TU
