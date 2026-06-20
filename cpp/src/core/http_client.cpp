// Shadow Launcher — HTTP client implementation (Qt6::Network backend)
// cpr/libcurl backend will be added later via #ifdef
//
// Design rules:
//   1. Qt Network is async → never blocks the UI
//   2. Proxy/UA config shared between all network layers
//   3. DownloadQueue limits concurrency to 2~4

#include "http_client.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QNetworkProxy>
#include <QNetworkDiskCache>
#include <QStandardPaths>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QUrl>
#include <QPointer>
#include <QElapsedTimer>
#include <QDebug>

#include <memory>

namespace ShadowLauncher {

// ============================================================
// QueueItem — holds one enqueued download task
// ============================================================

struct QueueItem {
    QString url;
    QString savePath;
    std::function<void(qint64, qint64)> progress;
    std::function<void(bool, const QString&)> done;
};

// ============================================================
// HttpClient implementation
// ============================================================

HttpClient::HttpClient()
{
    m_manager = new QNetworkAccessManager(this);
    m_manager->setTransferTimeout(m_config.totalTimeoutMs);

    // Persist HTTP cache (webp icons, API responses) to disk
    auto* diskCache = new QNetworkDiskCache(this);
    QString cachePath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                        + QStringLiteral("/httpcache");
    diskCache->setCacheDirectory(cachePath);
    diskCache->setMaximumCacheSize(128 * 1024 * 1024);  // 128 MB
    m_manager->setCache(diskCache);
}

HttpClient::~HttpClient()
{
    // m_manager is parented to this, auto-clean
}

HttpClient& HttpClient::instance()
{
    static HttpClient inst;
    return inst;
}

// --------------- proxy ---------------

void HttpClient::setProxy(const QString& host, int port,
                          const QString& user, const QString& pass)
{
    m_config.proxyHost = host.toStdString();
    m_config.proxyPort = port;
    m_config.proxyUser = user.toStdString();
    m_config.proxyPass = pass.toStdString();

    if (!host.isEmpty() && port > 0) {
        QNetworkProxy proxy(QNetworkProxy::HttpProxy, host,
                            static_cast<quint16>(port));
        if (!user.isEmpty()) {
            proxy.setUser(user);
            proxy.setPassword(pass);
        }
        m_manager->setProxy(proxy);
    } else {
        m_manager->setProxy(QNetworkProxy::NoProxy);
    }

    emit proxyChanged();
}

// --------------- user-agent ---------------

void HttpClient::setUserAgent(const QString& ua)
{
    m_config.userAgent = ua.toStdString();
}

// --------------- helpers ---------------

static QNetworkRequest buildRequest(const NetworkConfig& cfg, const QUrl& url,
                                     bool useCache = true)
{
    QNetworkRequest req(url);
    req.setRawHeader("User-Agent",
                     QString::fromStdString(cfg.userAgent).toUtf8());
    req.setTransferTimeout(cfg.totalTimeoutMs);
    if (!useCache)
        req.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         QNetworkRequest::AlwaysNetwork);
    return req;
}

// --------------- GET ---------------

void HttpClient::get(const QString& url,
                     std::function<void(int, const QByteArray&)> callback,
                     std::function<void(const QString&)> onError)
{
    QNetworkRequest req = buildRequest(m_config, QUrl(url));
    QNetworkReply* reply = m_manager->get(req);

    auto timer = std::make_shared<QElapsedTimer>();
    timer->start();

    // Handle both success and error in finished (avoids double-callback)
    QObject::connect(reply, &QNetworkReply::finished, this,
        [reply, callback = std::move(callback), onError = std::move(onError), timer, url]() {
            qint64 elapsed = timer->elapsed();
            if (reply->error() != QNetworkReply::NoError) {
                qDebug().noquote() << QStringLiteral("[NET] GET %1 → ERROR (%2ms): %3")
                                 .arg(url, QString::number(elapsed), reply->errorString());
                if (onError)
                    onError(reply->errorString());
            } else {
                const int status = reply->attribute(
                    QNetworkRequest::HttpStatusCodeAttribute).toInt();
                const QByteArray body = reply->readAll();
                qDebug().noquote() << QStringLiteral("[NET] GET %1 → %2 (%3ms, %4B)")
                                 .arg(url, QString::number(status),
                                      QString::number(elapsed),
                                      QString::number(body.size()));
                if (callback)
                    callback(status, body);
            }
            reply->deleteLater();
        });
}

// --------------- download (stream to file) ---------------

void HttpClient::download(const QString& url, const QString& savePath,
                          std::function<void(qint64, qint64)> progress,
                          std::function<void(bool, const QString&)> done)
{
    const QString tmpPath = savePath + QStringLiteral(".tmp");

    // Ensure parent directory exists
    const QFileInfo fi(savePath);
    QDir().mkpath(fi.absolutePath());

    // Remove stale .tmp
    QFile::remove(tmpPath);

    QNetworkRequest req = buildRequest(m_config, QUrl(url));
    QNetworkReply* reply = m_manager->get(req);

    // --- open output file ---
    auto* file = new QFile(tmpPath);
    if (!file->open(QIODevice::WriteOnly)) {
        reply->abort();
        reply->deleteLater();
        const QString err = QStringLiteral("Cannot open file: %1").arg(tmpPath);
        delete file;
        if (done) {
            done(false, err);
        }
        return;
    }

    // --- incremental write ---
    QObject::connect(reply, &QNetworkReply::readyRead, this,
        [reply, file]() {
            file->write(reply->readAll());
        });

    // --- progress ---
    if (progress) {
        QObject::connect(reply, &QNetworkReply::downloadProgress, this,
            [progress = std::move(progress)](qint64 received, qint64 total) {
                progress(received, total);
            });
    }

    // --- completion ---
    QObject::connect(reply, &QNetworkReply::finished, this,
        [reply, file, savePath, tmpPath, done = std::move(done)]() {
            file->close();
            delete file;

            const int status = reply->attribute(
                QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const bool networkOk = (reply->error() == QNetworkReply::NoError);
            const QString errorStr = reply->errorString();

            reply->deleteLater();

            if (networkOk && status == 200) {
                // Atomic rename: remove target, rename tmp → target
                QFile::remove(savePath);
                if (QFile::rename(tmpPath, savePath)) {
                    if (done) done(true, {});
                } else {
                    QFile::remove(tmpPath);
                    if (done)
                        done(false,
                             QStringLiteral("Failed to rename: %1").arg(tmpPath));
                }
            } else {
                QFile::remove(tmpPath);
                if (done) {
                    if (!networkOk)
                        done(false, errorStr);
                    else
                        done(false, QStringLiteral("HTTP %1").arg(status));
                }
            }
        });
}

QNetworkReply* HttpClient::downloadWithReply(const QString& url, const QString& savePath,
                  std::function<void(qint64, qint64)> progress,
                  std::function<void(bool, const QString&)> done,
                  qint64 resumeFrom)
{
    const QString tmpPath = savePath + QStringLiteral(".tmp");
    const QFileInfo fi(savePath);
    QDir().mkpath(fi.absolutePath());

    // Only remove stale tmp on fresh download; keep on resume
    if (resumeFrom <= 0) QFile::remove(tmpPath);

    QNetworkRequest req = buildRequest(m_config, QUrl(url), false);
    if (resumeFrom > 0) {
        req.setRawHeader("Range", QStringLiteral("bytes=%1-").arg(resumeFrom).toUtf8());
    }
    QNetworkReply* reply = m_manager->get(req);

    auto openMode = (resumeFrom > 0) ? QIODevice::Append : QIODevice::WriteOnly;
    auto* file = new QFile(tmpPath);
    if (!file->open(openMode)) {
        reply->abort();
        reply->deleteLater();
        delete file;
        if (done) done(false, QStringLiteral("Cannot open file: %1").arg(tmpPath));
        return nullptr;
    }

    QObject::connect(reply, &QNetworkReply::readyRead, this,
        [reply, file]() { file->write(reply->readAll()); });

    if (progress) {
        QObject::connect(reply, &QNetworkReply::downloadProgress, this,
            [progress = std::move(progress), resumeFrom](qint64 received, qint64 total) {
                progress(resumeFrom + received, resumeFrom > 0 ? (total > 0 ? resumeFrom + total : -1) : total);
            });
    }

    QObject::connect(reply, &QNetworkReply::finished, this,
        [this, reply, file, tmpPath, savePath, done = std::move(done)]() {
            file->close();
            reply->deleteLater();
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const bool networkOk = (reply->error() == QNetworkReply::NoError);
            const QString errorStr = reply->errorString();
            // HTTP 206 Partial Content is success for Range requests
            const bool httpOk = networkOk && (status == 200 || status == 206);
            if (httpOk) {
                QFile::remove(savePath);
                if (QFile::rename(tmpPath, savePath)) {
                    delete file;
                    if (done) done(true, QString());
                } else {
                    QFile::remove(tmpPath);
                    delete file;
                    if (done) done(false, QStringLiteral("Unable to rename downloaded file"));
                }
            } else {
                QFile::remove(tmpPath);
                delete file;
                if (done) done(false, networkOk
                    ? QStringLiteral("HTTP %1").arg(status)
                    : errorStr);
            }
        });

    QObject::connect(reply, &QNetworkReply::errorOccurred, this,
        [reply, file, tmpPath, done](QNetworkReply::NetworkError) {
            Q_UNUSED(reply)
            file->close();
            file->deleteLater();
            QFile::remove(tmpPath);
            if (done) {
                const QString err = QStringLiteral("Network error: %1").arg(reply->errorString());
                done(false, err);
            }
        });

    return reply;
}

// ═══════════════════════════════════════════════════
// POST with raw body
// ═══════════════════════════════════════════════════
QNetworkReply* HttpClient::post(const QNetworkRequest& request, const QByteArray& body)
{
    QNetworkRequest req = request;
    req.setRawHeader("User-Agent",
                     QString::fromStdString(m_config.userAgent).toUtf8());
    req.setTransferTimeout(m_config.totalTimeoutMs);
    return m_manager->post(req, body);
}

// ═══════════════════════════════════════════════════
// GET with custom request
// ═══════════════════════════════════════════════════
QNetworkReply* HttpClient::getRaw(const QNetworkRequest& request)
{
    QNetworkRequest req = request;
    req.setRawHeader("User-Agent",
                     QString::fromStdString(m_config.userAgent).toUtf8());
    req.setTransferTimeout(m_config.totalTimeoutMs);
    return m_manager->get(req);
}

void HttpClient::abortDownload(QNetworkReply* reply)
{
    if (reply) reply->abort();
}

// ============================================================
// DownloadQueue implementation
// ============================================================

DownloadQueue::DownloadQueue(int maxConcurrent, QObject* parent)
    : QObject(parent)
    , m_maxConcurrent(maxConcurrent)
    , m_active(0)
    , m_completed(0)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(100);
    QObject::connect(m_timer, &QTimer::timeout, this, &DownloadQueue::scheduleNext);
}

DownloadQueue::~DownloadQueue()
{
    cancelAll();
}

// --------------- enqueue ---------------

void DownloadQueue::enqueue(const QString& url, const QString& savePath,
                            std::function<void(qint64, qint64)> progress,
                            std::function<void(bool, const QString&)> done)
{
    auto item = std::make_shared<QueueItem>();
    item->url      = url;
    item->savePath = savePath;
    item->progress = std::move(progress);
    item->done     = std::move(done);

    m_queue.append(std::move(item));
    scheduleNext();
}

// --------------- cancel all ---------------

void DownloadQueue::cancelAll()
{
    m_queue.clear();
    m_timer->stop();
    // Note: active downloads continue running; their doneCb checks
    // QPointer which becomes null if this is destroyed
}

// --------------- pending count ---------------

int DownloadQueue::pending() const
{
    return m_queue.size() + m_active;
}

// --------------- internal scheduler ---------------

void DownloadQueue::scheduleNext()
{
    // Keep starting tasks while we have capacity and items waiting
    while (m_active < m_maxConcurrent && !m_queue.isEmpty()) {
        auto item = m_queue.takeFirst();
        m_active++;

        QPointer<DownloadQueue> self(this);

        // --- progress forwarder ---
        auto progressCb = [item](qint64 received, qint64 total) {
            if (item->progress)
                item->progress(received, total);
        };

        // --- done wrapper (includes queue management) ---
        auto doneCb = [this, self, item](bool ok, const QString& err) {
            if (!self)
                return;

            m_active--;
            m_completed++;

            // Forward to original caller
            if (item->done)
                item->done(ok, err);

            // Emit progress update (completed, total)
            const int total = m_completed + pending();
            emit progressChanged(m_completed, total);

            // Try to start more
            scheduleNext();

            // If everything is done, emit allCompleted
            if (m_active == 0 && m_queue.isEmpty()) {
                m_timer->stop();
                emit allCompleted();
            }
        };

        HttpClient::instance().download(item->url, item->savePath,
                                        std::move(progressCb),
                                        std::move(doneCb));
    }

    // If items remain but no capacity, start periodic re-check
    if (!m_queue.isEmpty() && !m_timer->isActive()) {
        m_timer->start();
    }
}

} // namespace ShadowLauncher
