// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
// Shadow Launcher — HTTP client wrapper
// Ensures cpr and Qt Network share proxy / UA / thread safety

#pragma once

#include <QObject>
#include <QThread>
#include <QNetworkProxy>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QList>
#include <string>
#include <functional>
#include <memory>

namespace ShadowLauncher {

// ============================================================
// Shared config: all network layers must use these
// ============================================================

struct NetworkConfig {
    std::string userAgent = "ShadowLauncher/1.0";
    std::string proxyHost;       // empty = no proxy
    int proxyPort = 0;
    std::string proxyUser;
    std::string proxyPass;
    int connectTimeoutMs = 10000;
    int totalTimeoutMs = 10000;
};

// ============================================================
// HttpClient — singleton network layer
// ============================================================

class HttpClient : public QObject {
    Q_OBJECT
public:
    static HttpClient& instance();

    void setProxy(const QString& host, int port,
                  const QString& user = {}, const QString& pass = {});
    void setUserAgent(const QString& ua);
    NetworkConfig config() const { return m_config; }

    // All HTTP calls run on worker thread via QtConcurrent
    void get(const QString& url,
             std::function<void(int status, const QByteArray& body)> callback,
             std::function<void(const QString& error)> onError = nullptr);

    void getWithFallback(const QString& url,
             std::function<void(int status, const QByteArray& body)> callback,
             std::function<void(const QString& error)> onError = nullptr);

    void download(const QString& url, const QString& savePath,
                  std::function<void(qint64 received, qint64 total)> progress,
                  std::function<void(bool ok, const QString& error)> done);

    void downloadWithFallback(const QString& url, const QString& savePath,
                  std::function<void(qint64 received, qint64 total)> progress,
                  std::function<void(bool ok, const QString& error)> done);

    // Returns QNetworkReply* for abort/pause support
    // resumeFrom: bytes already downloaded (-1 = fresh, >=0 = append to existing file)
    QNetworkReply* downloadWithReply(const QString& url, const QString& savePath,
                  std::function<void(qint64 received, qint64 total)> progress,
                  std::function<void(bool ok, const QString& error)> done,
                  qint64 resumeFrom = -1);

    // POST with raw body (returns QNetworkReply* for async handling)
    QNetworkReply* post(const QNetworkRequest& request, const QByteArray& body);

    // GET with custom request (returns QNetworkReply* for async handling)
    QNetworkReply* getRaw(const QNetworkRequest& request);

    // PUT with custom request + body (returns QNetworkReply* for async handling)
    QNetworkReply* put(const QNetworkRequest& request, const QByteArray& body);

    // DELETE with custom request (returns QNetworkReply* for async handling)
    QNetworkReply* deleteResource(const QNetworkRequest& request);

    void abortDownload(QNetworkReply* reply);

    QNetworkAccessManager* manager() const { return m_manager; }

signals:
    void proxyChanged();

private:
    HttpClient();
    ~HttpClient() override;

    NetworkConfig m_config;
    QNetworkAccessManager* m_manager = nullptr;
};

// ============================================================
// DownloadQueue — concurrent download scheduler
// ============================================================

struct QueueItem;

class DownloadQueue : public QObject {
    Q_OBJECT
public:
    explicit DownloadQueue(int maxConcurrent = 3, QObject* parent = nullptr);
    ~DownloadQueue() override;

    void enqueue(const QString& url, const QString& savePath,
                 std::function<void(qint64, qint64)> progress,
                 std::function<void(bool, const QString&)> done);
    void cancelAll();
    int pending() const;

signals:
    void allCompleted();
    void progressChanged(int completed, int total);

private:
    void scheduleNext();

    int m_maxConcurrent;
    int m_active = 0;
    int m_completed = 0;
    QTimer* m_timer = nullptr;
    QList<std::shared_ptr<QueueItem>> m_queue;
};

} // namespace ShadowLauncher
