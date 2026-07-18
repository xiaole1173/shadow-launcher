// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
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
#include "utils/logger.h"
#include <QNetworkProxy>
#include <QNetworkDiskCache>
#include <QStandardPaths>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QUrl>
#include <QPointer>
#include <QElapsedTimer>
#include <QDateTime>
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
                                     bool useCache = true, int timeoutMs = -1)
{
    QNetworkRequest req(url);
    req.setRawHeader("User-Agent",
                     QString::fromStdString(cfg.userAgent).toUtf8());
    req.setTransferTimeout(timeoutMs > 0 ? timeoutMs : cfg.totalTimeoutMs);
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
                qCInfo(logDownload).noquote() << QStringLiteral("[NET] GET %1 → ERROR (%2ms): %3")
                                 .arg(url, QString::number(elapsed), reply->errorString());
                if (onError)
                    onError(reply->errorString());
            } else {
                const int status = reply->attribute(
                    QNetworkRequest::HttpStatusCodeAttribute).toInt();
                const QByteArray body = reply->readAll();
                qCInfo(logDownload).noquote() << QStringLiteral("[NET] GET %1 → %2 (%3ms, %4B)")
                                 .arg(url, QString::number(status),
                                      QString::number(elapsed),
                                      QString::number(body.size()));
                if (callback)
                    callback(status, body);
            }
            reply->deleteLater();
        });
}

// --------------- URL mirror mapping ---------------

static QString mirrorUrl(const QString& url)
{
    // piston-meta.mojang.com → BMCLAPI
    if (url.contains(QStringLiteral("piston-meta.mojang.com"))) {
        QString m = url;
        m.replace(QStringLiteral("piston-meta.mojang.com"),
                  QStringLiteral("bmclapi2.bangbang93.com"));
        return m;
    }
    // launchermeta.mojang.com → BMCLAPI
    if (url.contains(QStringLiteral("launchermeta.mojang.com"))) {
        QString m = url;
        m.replace(QStringLiteral("launchermeta.mojang.com"),
                  QStringLiteral("bmclapi2.bangbang93.com"));
        return m;
    }
    // libraries.minecraft.net → BMCLAPI libraries
    if (url.contains(QStringLiteral("libraries.minecraft.net"))) {
        QString m = url;
        m.replace(QStringLiteral("https://libraries.minecraft.net/"),
                  QStringLiteral("https://bmclapi2.bangbang93.com/libraries/"));
        return m;
    }
    // resources.download.minecraft.net → BMCLAPI assets
    if (url.contains(QStringLiteral("resources.download.minecraft.net"))) {
        QString m = url;
        m.replace(QStringLiteral("resources.download.minecraft.net"),
                  QStringLiteral("bmclapi2.bangbang93.com/assets"));
        return m;
    }
    return {};
}

// --------------- downloadImpl (internal — mirror-first, single attempt) ---------------

static void downloadImpl(QNetworkAccessManager* mgr, const NetworkConfig& cfg,
                         const QString& url, const QString& savePath,
                         std::function<void(qint64, qint64)> progress,
                         std::function<void(bool, const QString&)> done,
                         int timeoutMs)
{
    const QString tmpPath = savePath + QStringLiteral(".tmp");
    QFileInfo fi(savePath);
    QDir().mkpath(fi.absolutePath());
    QFile::remove(tmpPath);

    QString mirror = mirrorUrl(url);
    QString primaryUrl = mirror.isEmpty() ? url : mirror;

    QNetworkRequest req = buildRequest(cfg, QUrl(primaryUrl), true, timeoutMs);
    QNetworkReply* reply = mgr->get(req);

    auto* file = new QFile(tmpPath);
    if (!file->open(QIODevice::WriteOnly)) {
        reply->abort();
        reply->deleteLater();
        delete file;
        if (done) done(false, QStringLiteral("Cannot open file: %1").arg(tmpPath));
        return;
    }

    QObject::connect(reply, &QNetworkReply::readyRead, mgr,
        [reply, file]() { file->write(reply->readAll()); });

    auto sharedProg = std::make_shared<std::function<void(qint64,qint64)>>(std::move(progress));
    auto sharedDone = std::make_shared<std::function<void(bool,const QString&)>>(std::move(done));

    if (*sharedProg) {
        QObject::connect(reply, &QNetworkReply::downloadProgress, mgr,
            [sharedProg](qint64 recv, qint64 total) { (*sharedProg)(recv, total); });
    }

    QObject::connect(reply, &QNetworkReply::finished, mgr,
        [mgr, primaryUrl, mirror, url, reply, file, savePath, tmpPath,
         sharedProg, sharedDone, cfg, timeoutMs]() mutable {
            file->close();
            delete file;

            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const bool ok = (reply->error() == QNetworkReply::NoError);
            const QString errStr = reply->errorString();
            reply->deleteLater();

            if (ok && status == 200) {
                QFile::remove(savePath);
                if (QFile::rename(tmpPath, savePath)) {
                    if (*sharedDone) (*sharedDone)(true, {});
                } else {
                    QFile::remove(tmpPath);
                    if (*sharedDone) (*sharedDone)(false, QStringLiteral("Rename failed: %1").arg(tmpPath));
                }
            } else if (!mirror.isEmpty() && primaryUrl == mirror) {
                qCInfo(logDownload) << "[NET] Mirror failed:" << errStr << "→ official:" << url;
                QFile::remove(tmpPath);
                downloadImpl(mgr, cfg, url, savePath,
                             sharedProg ? *sharedProg : nullptr,
                             sharedDone ? *sharedDone : nullptr,
                             timeoutMs);
            } else {
                QFile::remove(tmpPath);
                if (*sharedDone) {
                    (*sharedDone)(false, ok ? QStringLiteral("HTTP %1").arg(status) : errStr);
                }
            }
        });
}

// --------------- downloadRetry (3-attempt: 10s → 30s → 4s) ---------------

static void downloadRetry(QNetworkAccessManager* mgr, const NetworkConfig& cfg,
                          const QString& url, const QString& savePath,
                          std::function<void(qint64,qint64)> progress,
                          std::function<void(bool,const QString&)> done,
                          int attempt, qint64 startTimeMs)
{
    if (attempt >= 3) {
        if (done) done(false, QStringLiteral("Download failed after 3 attempts: %1").arg(url));
        return;
    }

    int timeoutMs;
    switch (attempt) {
        case 0: timeoutMs = 10000; break;
        case 1: timeoutMs = 30000; break;
        default: timeoutMs = 4000; break;
    }

    if (attempt >= 2) {
        qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - startTimeMs;
        if (elapsed < 5500) {
            if (done) done(false, QStringLiteral("Download failed too fast to retry: %1").arg(url));
            return;
        }
    }

    int delay = (attempt > 0) ? 500 : 0;

    auto doReq = [=]() {
        downloadImpl(mgr, cfg, url, savePath, progress,
                     [=](bool ok, const QString& err) {
            if (ok) { if (done) done(true, {}); }
            else downloadRetry(mgr, cfg, url, savePath, progress, done, attempt + 1, startTimeMs);
        }, timeoutMs);
    };

    if (delay > 0)
        QTimer::singleShot(delay, mgr, doReq);
    else
        doReq();
}

// --------------- download (public API — retry wrapper) ---------------

void HttpClient::download(const QString& url, const QString& savePath,
                          std::function<void(qint64, qint64)> progress,
                          std::function<void(bool, const QString&)> done)
{
    downloadRetry(m_manager, m_config, url, savePath,
                  std::move(progress), std::move(done),
                  0, QDateTime::currentMSecsSinceEpoch());
}
// --------------- GET with mirror fallback ---------------

void HttpClient::getWithFallback(const QString& url,
                     std::function<void(int, const QByteArray&)> callback,
                     std::function<void(const QString&)> onError)
{
    QString mirror = mirrorUrl(url);
    if (mirror.isEmpty()) {
        get(url, std::move(callback), std::move(onError));
        return;
    }

    // Race: mirror and official in parallel, take first success
    auto doneCount = std::make_shared<QAtomicInt>(0);
    auto hasWinner = std::make_shared<QAtomicInt>(0);
    auto cb = std::make_shared<std::function<void(int,const QByteArray&)>>(std::move(callback));
    auto eb = std::make_shared<std::function<void(const QString&)>>(std::move(onError));
    static const int kTotal = 2;

    auto tryGet = [this, doneCount, hasWinner, cb, eb]
        (const QString& targetUrl) {
            get(targetUrl,
                [hasWinner, targetUrl, cb](int status, const QByteArray& body) {
                    if (hasWinner->testAndSetRelaxed(0, 1)) {
                        qCInfo(logDownload) << "[NET] GET race winner:" << targetUrl
                                 << "(HTTP" << status << ")";
                        if (*cb) (*cb)(status, body);
                    }
                },
                [doneCount, hasWinner, eb](const QString& err) {
                    int prev = doneCount->fetchAndAddRelaxed(1);
                    if (prev + 1 >= kTotal && hasWinner->loadRelaxed() == 0) {
                        if (*eb) (*eb)(err);
                    }
                });
        };

    tryGet(mirror);
    tryGet(url);
}

// --------------- downloadWithFallback (delegates to download which has mirror built-in) ---------------

void HttpClient::downloadWithFallback(const QString& url, const QString& savePath,
                          std::function<void(qint64, qint64)> progress,
                          std::function<void(bool, const QString&)> done)
{
    download(url, savePath, std::move(progress), std::move(done));
}

// --------------- downloadWithReply ---------------

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

QNetworkReply* HttpClient::put(const QNetworkRequest& request, const QByteArray& body)
{
    QNetworkRequest req = request;
    req.setRawHeader("User-Agent",
                     QString::fromStdString(m_config.userAgent).toUtf8());
    req.setTransferTimeout(m_config.totalTimeoutMs);
    return m_manager->put(req, body);
}

QNetworkReply* HttpClient::deleteResource(const QNetworkRequest& request)
{
    QNetworkRequest req = request;
    req.setRawHeader("User-Agent",
                     QString::fromStdString(m_config.userAgent).toUtf8());
    req.setTransferTimeout(m_config.totalTimeoutMs);
    return m_manager->deleteResource(req);
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

        HttpClient::instance().downloadWithFallback(item->url, item->savePath,
                                        std::move(progressCb),
                                        std::move(doneCb));
    }

    // If items remain but no capacity, start periodic re-check
    if (!m_queue.isEmpty() && !m_timer->isActive()) {
        m_timer->start();
    }
}

} // namespace ShadowLauncher
