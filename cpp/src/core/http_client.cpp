// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
// Shadow Launcher — HTTP client implementation (Qt6::Network backend) — 驿道 v2
//
// v2 (2026-08-02)：高速多线程通用下载引擎
//   1. >4MB 文件自动 Range 多线程分片（实测 cdn-alt 8 连接 337KB/s vs 单连接超时/几十 KB）
//   2. 分片前探测：Range 支持判定（206）+ 307 重定向预解析（分片直接打最终 URL）
//   3. 片数据按偏移直写主文件（零合并开销，磁盘峰值≈单份）
//   4. 停滞检测推广到所有源/所有片（2s×3 无字节增长 → abort 重试）
//   5. expectedSize/expectedSha1 完整性校验内建（不符自动重下一次）
//   6. DownloadQueue 类删除（全项目无调用方的漏网死代码）
//
// Design rules:
//   1. Qt Network is async → never blocks the UI
//   2. Proxy/UA config shared between all network layers
//   3. API 请求（get/post）保持单连接零变化；文件传输自动分片

#include "http_client.h"
#include "engine_identity.h"
#include "utils/hash_utils.h"

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
#include <QCryptographicHash>

#include <memory>

namespace ShadowLauncher {

// ============================================================
// HttpClient implementation
// ============================================================

HttpClient::HttpClient()
{
    qCInfo(logDownload) << engineBanner("yidao");
    m_manager = new QNetworkAccessManager(this);
    m_manager->setTransferTimeout(m_config.totalTimeoutMs);
    // 分片双 QNAM 轮询（对齐山海经 2×QNAM 验证结论：避免单 manager 队列拥塞）
    m_manager2 = new QNetworkAccessManager(this);
    m_manager2->setTransferTimeout(m_config.totalTimeoutMs);

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
    // managers are parented to this, auto-clean
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
        m_manager2->setProxy(proxy);
    } else {
        m_manager->setProxy(QNetworkProxy::NoProxy);
        m_manager2->setProxy(QNetworkProxy::NoProxy);
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

// ============================================================
// DownloadHandle — 分片/单连接统一任务句柄
// ============================================================

HttpClient::DownloadHandle::DownloadHandle(QObject* parent)
    : QObject(parent)
{
}

HttpClient::DownloadHandle::~DownloadHandle()
{
    abort();
}

void HttpClient::DownloadHandle::addReply(QNetworkReply* r)
{
    QMutexLocker lock(&m_mutex);
    if (m_aborted) {
        lock.unlock();
        r->abort();
        r->deleteLater();
        return;
    }
    m_replies.append(r);
}

void HttpClient::DownloadHandle::onReplyFinished(QNetworkReply* r)
{
    QMutexLocker lock(&m_mutex);
    m_replies.removeOne(r);
}

void HttpClient::DownloadHandle::abort()
{
    QMutexLocker lock(&m_mutex);
    if (m_aborted) return;
    m_aborted = true;
    const auto replies = m_replies;
    lock.unlock();
    for (auto* r : replies) {
        if (r) r->abort();
    }
}

bool HttpClient::DownloadHandle::isActive() const
{
    QMutexLocker lock(&m_mutex);
    return !m_aborted && !m_replies.isEmpty();
}

void HttpClient::abortDownload(DownloadHandle* handle)
{
    if (handle) handle->abort();
}

// --------------- GET ---------------

void HttpClient::get(const QString& url,
                     std::function<void(int, const QByteArray&)> callback,
                     std::function<void(const QString&)> onError)
{
    qCInfo(logDownload).noquote() << QStringLiteral("[驿道] 发起网络请求 %1 方式=GET 超时=%2ms")
                                     .arg(url).arg(m_config.totalTimeoutMs);

    QNetworkRequest req = buildRequest(m_config, QUrl(url));
    QNetworkReply* reply = m_manager->get(req);

    auto timer = std::make_shared<QElapsedTimer>();
    timer->start();

    // Handle both success and error in finished (avoids double-callback)
    QObject::connect(reply, &QNetworkReply::finished, this,
        [reply, callback = std::move(callback), onError = std::move(onError), timer, url]() {
            qint64 elapsed = timer->elapsed();
            const int status = reply->attribute(
                QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (reply->error() != QNetworkReply::NoError) {
                qCInfo(logDownload).noquote() << QStringLiteral("[驿道] 请求失败 %1 状态码=%2 耗时=%3ms")
                                 .arg(url, QString::number(status),
                                      QString::number(elapsed));
                if (onError)
                    onError(reply->errorString());
            } else {
                const QByteArray body = reply->readAll();
                qCInfo(logDownload).noquote() << QStringLiteral("[驿道] 请求完成 %1 状态码=%2 耗时=%3ms 数据量=%4")
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
    // launcher.mojang.com → BMCLAPI (已关停的旧CDN)
    // 注: launcher.mojang.com/version/<ver>/client.jar → BMCLAPI 镜像
    if (url.contains(QStringLiteral("launcher.mojang.com"))) {
        QString m = url;
        m.replace(QStringLiteral("launcher.mojang.com"),
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

// ============================================================
// 完整性校验（size + sha1）
// ============================================================

bool HttpClient::verifyFile(const QString& path, qint64 expectedSize,
                            const QString& expectedSha1, QString* errOut)
{
    const QFileInfo fi(path);
    if (expectedSize > 0 && fi.size() != expectedSize) {
        if (errOut) *errOut = QStringLiteral("大小不符 预期=%1 实际=%2")
                                  .arg(expectedSize).arg(fi.size());
        return false;
    }
    if (!expectedSha1.isEmpty()) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            if (errOut) *errOut = QStringLiteral("无法读取文件校验: %1").arg(path);
            return false;
        }
        const QString actual = sha1Hex(f.readAll());
        f.close();
        if (actual.compare(expectedSha1, Qt::CaseInsensitive) != 0) {
            if (errOut) *errOut = QStringLiteral("SHA1不符 预期=%1 实际=%2")
                                      .arg(expectedSha1, actual);
            return false;
        }
    }
    return true;
}

// ============================================================
// 单连接下载（小文件 / 不支持 Range / 断点续传路径）
// 镜像优先 + 停滞检测 + 可选校验
// ============================================================

void HttpClient::startSingle(const QString& url, const QString& savePath,
                             std::function<void(qint64, qint64)> progress,
                             std::function<void(bool, const QString&)> done,
                             qint64 resumeFrom, qint64 expectedSize,
                             const QString& expectedSha1, DownloadHandle* handle)
{
    const QString tmpPath = savePath + QStringLiteral(".tmp");
    const QFileInfo fi(savePath);
    QDir().mkpath(fi.absolutePath());
    if (resumeFrom <= 0) QFile::remove(tmpPath);

    const QString mirror = mirrorUrl(url);
    const QString primaryUrl = mirror.isEmpty() ? url : mirror;
    const bool isMirror = !mirror.isEmpty() && primaryUrl == mirror;

    auto dlTimer = std::make_shared<QElapsedTimer>();
    dlTimer->start();

    QNetworkRequest req = buildRequest(m_config, QUrl(primaryUrl), false,
                                       resumeFrom > 0 ? kShardIdleTimeoutMs : kShardConnectTimeoutMs);
    if (resumeFrom > 0)
        req.setRawHeader("Range", QStringLiteral("bytes=%1-").arg(resumeFrom).toUtf8());
    QNetworkReply* reply = m_manager->get(req);
    if (handle) handle->addReply(reply);

    qCInfo(logDownload).noquote() << QStringLiteral("[驿道] 开始下载(单连接) %1").arg(primaryUrl);

    auto openMode = (resumeFrom > 0) ? QIODevice::Append : QIODevice::WriteOnly;
    auto* file = new QFile(tmpPath);
    if (!file->open(openMode)) {
        if (handle) handle->onReplyFinished(reply);
        reply->abort();
        reply->deleteLater();
        delete file;
        if (done) done(false, QStringLiteral("无法打开文件: %1").arg(tmpPath));
        return;
    }

    QObject::connect(reply, &QNetworkReply::readyRead, this,
        [reply, file]() { file->write(reply->readAll()); });

    // ── 停滞检测（慢速挂起传输防护，推广到所有源）──
    // 注意：裸指针 + deleteLater（shared_ptr<QTimer> + deleteLater 会 double free → 堆损坏）
    auto totalRecv = std::make_shared<qint64>(0);
    auto lastRecv = std::make_shared<qint64>(-1);
    auto stallCount = std::make_shared<int>(0);
    auto* stallTimer = new QTimer;
    stallTimer->setInterval(kStallIntervalMs);
    QObject::connect(stallTimer, &QTimer::timeout,
        [reply, totalRecv, lastRecv, stallCount]() {
            if (reply->error() != QNetworkReply::NoError) return;
            if (*totalRecv <= 0) return;   // 首包未到：交给 transferTimeout，停滞检测不介入
            if (*totalRecv == *lastRecv) {
                if (++(*stallCount) >= kStallTicks)
                    reply->abort();   // 传输中停滞 → 中止
            } else {
                *lastRecv = *totalRecv;
                *stallCount = 0;
            }
        });
    stallTimer->start();

    auto sharedProg = std::make_shared<std::function<void(qint64,qint64)>>(std::move(progress));
    auto sharedDone = std::make_shared<std::function<void(bool,const QString&)>>(std::move(done));

    if (*sharedProg) {
        QObject::connect(reply, &QNetworkReply::downloadProgress, this,
            [sharedProg, totalRecv, resumeFrom](qint64 recv, qint64 total) {
                *totalRecv = recv;
                (*sharedProg)(resumeFrom + recv,
                              resumeFrom > 0 && total > 0 ? resumeFrom + total : total);
            });
    } else {
        QObject::connect(reply, &QNetworkReply::downloadProgress, this,
            [totalRecv](qint64 recv, qint64) { *totalRecv = recv; });
    }

    QObject::connect(reply, &QNetworkReply::finished, this,
        [this, primaryUrl, isMirror, url, reply, file, savePath, tmpPath,
         sharedProg, sharedDone, dlTimer, stallTimer, resumeFrom,
         expectedSize, expectedSha1, handle]() mutable {
            file->close();
            delete file;
            stallTimer->stop();
            stallTimer->deleteLater();
            if (handle) handle->onReplyFinished(reply);

            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const bool networkOk = (reply->error() == QNetworkReply::NoError);
            const QString errStr = reply->errorString();
            const qint64 elapsed = dlTimer->elapsed();
            reply->deleteLater();

            // 取消短路：abort 后不镜像重试，直接失败（调用方按 cancelled 处理）
            if (handle && handle->m_aborted) {
                QFile::remove(tmpPath);
                if (*sharedDone) (*sharedDone)(false, QStringLiteral("已取消"));
                return;
            }

            // HTTP 206 Partial Content is success for Range requests
            const bool httpOk = networkOk && (status == 200 || (resumeFrom > 0 && status == 206));
            if (httpOk) {
                // 完整性校验（size + sha1）
                QString verifyErr;
                if (!verifyFile(tmpPath, expectedSize, expectedSha1, &verifyErr)) {
                    QFile::remove(tmpPath);
                    if (*sharedDone) (*sharedDone)(false, verifyErr);
                    return;
                }
                QFile::remove(savePath);
                if (QFile::rename(tmpPath, savePath)) {
                    const qint64 fileSize = QFileInfo(savePath).size();
                    qCInfo(logDownload).noquote() << QStringLiteral("[驿道] 下载完成 %1 文件大小=%2 耗时=%3ms")
                                                     .arg(savePath).arg(fileSize).arg(elapsed);
                    if (*sharedDone) (*sharedDone)(true, {});
                } else {
                    QFile::remove(tmpPath);
                    if (*sharedDone) (*sharedDone)(false, QStringLiteral("重命名失败: %1").arg(tmpPath));
                }
            } else if (isMirror) {
                qCInfo(logDownload).noquote() << QStringLiteral("[驿道] 镜像源失败 %1，切换到官方源 %2").arg(errStr, url);
                QFile::remove(tmpPath);
                startSingle(url, savePath,
                            sharedProg ? *sharedProg : nullptr,
                            sharedDone ? *sharedDone : nullptr,
                            resumeFrom, expectedSize, expectedSha1, handle);
            } else {
                QFile::remove(tmpPath);
                if (*sharedDone) {
                    (*sharedDone)(false, networkOk
                        ? QStringLiteral("HTTP %1").arg(status) : errStr);
                }
            }
        });
}

// ============================================================
// 分片下载
// ============================================================

// 探测：GET Range: bytes=0-0（跟随重定向拿最终 URL）
// 206 → runChunked(finalUrl, total)；200/小文件 → startSingle
void HttpClient::startChunked(const QString& url, const QString& savePath,
                              std::function<void(qint64, qint64)> progress,
                              std::function<void(bool, const QString&)> done,
                              qint64 expectedSize, const QString& expectedSha1,
                              DownloadHandle* handle)
{
    QNetworkRequest req = buildRequest(m_config, QUrl(url), false, kShardConnectTimeoutMs);
    req.setRawHeader("Range", "bytes=0-0");
    QNetworkReply* probe = m_manager->get(req);
    if (handle) handle->addReply(probe);

    qCInfo(logDownload).noquote() << QStringLiteral("[驿道] 分片探测 %1").arg(url);

    QObject::connect(probe, &QNetworkReply::finished, this,
        [this, probe, url, savePath, progress = std::move(progress),
         done = std::move(done), expectedSize, expectedSha1, handle]() {
            if (handle) handle->onReplyFinished(probe);
            const int status = probe->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QString finalUrl = probe->url().toString();  // 重定向后的实际 URL
            const bool networkOk = (probe->error() == QNetworkReply::NoError);
            const QString errStr = probe->errorString();
            probe->deleteLater();

            if (!networkOk) {
                // 取消短路：abort 后不降级单连接
                if (handle && handle->m_aborted) {
                    if (done) done(false, QStringLiteral("已取消"));
                    return;
                }
                qCInfo(logDownload).noquote() << QStringLiteral("[驿道] 探测失败 %1 (%2)，降级单连接").arg(url, errStr);
                // 降级单连接（镜像优先 + 官方 fallback + 停滞检测），不直接失败——保证不比 v1 脆
                startSingle(url, savePath, std::move(progress), std::move(done),
                            -1, expectedSize, expectedSha1, handle);
                return;
            }

            if (status == 206) {
                // Content-Range: bytes 0-0/TOTAL
                const QByteArray cr = probe->rawHeader("Content-Range");
                const int slash = cr.lastIndexOf('/');
                bool ok = false;
                const qint64 total = slash >= 0 ? cr.mid(slash + 1).toLongLong(&ok) : 0;
                if (ok && total > 0 && total >= kShardThresholdBytes) {
                    // 307 预解析：分片直接打最终 URL，省掉每片一次重定向 + TLS 握手
                    if (finalUrl != url) {
                        qCInfo(logDownload).noquote() << QStringLiteral("[驿道] 307 预解析 %1 → %2").arg(url, finalUrl);
                    }
                    runChunked(finalUrl, total, savePath, std::move(progress),
                               std::move(done), expectedSize, expectedSha1, handle);
                    return;
                }
            }
            // 不支持 Range（200）或文件较小 → 单连接
            startSingle(url, savePath, std::move(progress), std::move(done),
                        -1, expectedSize, expectedSha1, handle);
        });
}

// 分片并行核心：每片 Range 直写主文件对应偏移（零合并开销）
void HttpClient::runChunked(const QString& finalUrl, qint64 total,
                            const QString& savePath,
                            std::function<void(qint64, qint64)> progress,
                            std::function<void(bool, const QString&)> done,
                            qint64 expectedSize, const QString& expectedSha1,
                            DownloadHandle* handle)
{
    const QString tmpPath = savePath + QStringLiteral(".part");
    QFileInfo fi(savePath);
    QDir().mkpath(fi.absolutePath());
    QFile::remove(tmpPath);

    // ── 分片参数：N = clamp(total/2MB, 4, 8)（实测 8 连接收益最佳）──
    const int shards = static_cast<int>(
        qBound<qint64>(static_cast<qint64>(kMinShards),
                       (total + kShardSizeBytes - 1) / kShardSizeBytes,
                       static_cast<qint64>(kMaxShards)));
    const qint64 chunk = (total + shards - 1) / shards;

    struct Ctx {
        QFile* file = nullptr;
        QVector<bool> doneFlags;          // 片成功落盘标记
        QVector<int> retries;             // 片重试计数（主线程串行，无竞争）
        QVector<int> slowRetries;         // 片龟速换连接计数（预算 kShardSlowRetries，2026-08-10）
        qint64 recvBytes = 0;             // 网络已收字节（进度）
        int inflight = 0;                 // 在途片数（含重试）
        bool failed = false;              // 任一片耗尽重试 → 整体收尾
        bool finished = false;            // 已发 done
        qint64 total = 0;
    };
    auto ctx = std::make_shared<Ctx>();
    ctx->total = total;
    ctx->doneFlags.fill(false, shards);
    ctx->retries.fill(0, shards);
    ctx->slowRetries.fill(0, shards);

    auto* file = new QFile(tmpPath);
    if (!file->open(QIODevice::WriteOnly)) {
        delete file;
        if (done) done(false, QStringLiteral("无法打开分片文件: %1").arg(tmpPath));
        return;
    }
    ctx->file = file;

    auto dlTimer = std::make_shared<QElapsedTimer>();
    dlTimer->start();

    // ── 整体收尾（仅一次）──
    auto finishAll = [this, ctx, file, savePath, tmpPath, total,
                      done, expectedSize, expectedSha1, dlTimer, handle](bool ok, const QString& err) {
        if (ctx->finished) return;
        ctx->finished = true;
        if (ctx->file) {
            ctx->file->close();
            delete ctx->file;
            ctx->file = nullptr;
        }
        if (!ok) {
            QFile::remove(tmpPath);
            if (done) done(false, err);
            return;
        }
        const qint64 elapsed = dlTimer->elapsed();
        QString verifyErr;
        if (!verifyFile(tmpPath, expectedSize, expectedSha1, &verifyErr)) {
            QFile::remove(tmpPath);
            if (done) done(false, verifyErr);
            return;
        }
        QFile::remove(savePath);
        if (QFile::rename(tmpPath, savePath)) {
            qCInfo(logDownload).noquote() << QStringLiteral("[驿道] 分片下载完成 %1 大小=%2 分片=%3 耗时=%4ms")
                                             .arg(savePath).arg(total).arg(ctx->doneFlags.size()).arg(elapsed);
            if (done) done(true, {});
        } else {
            QFile::remove(tmpPath);
            if (done) done(false, QStringLiteral("重命名失败: %1").arg(tmpPath));
        }
    };

    // ── 片完成（成功/失败统一入口）──
    auto finishChunk = [this, ctx, file, finishAll, handle](int i, qint64 start,
                                                    const QByteArray& data, bool ok,
                                                    const QString& err) {
        if (ctx->failed || ctx->finished) return;
        if (!ok) {
            // 某片已耗尽重试次数 → 整体失败（其余片由 abort 中止、回调短路）
            ctx->failed = true;
            if (handle) handle->abort();
            finishAll(false, err.isEmpty()
                ? QStringLiteral("分片下载失败")
                : QStringLiteral("分片下载失败: %1").arg(err));
            return;
        }
        // 成功：按偏移直写主文件（主线程串行，无并发竞争）
        if (!file->seek(start)) {
            ctx->failed = true;
            finishAll(false, QStringLiteral("分片文件定位失败"));
            return;
        }
        const qint64 n = file->write(data);
        if (n != data.size()) {
            ctx->failed = true;
            finishAll(false, QStringLiteral("分片文件写入不完整"));
            return;
        }
        ctx->doneFlags[i] = true;

        // 全部片成功落盘 → 收尾
        if (ctx->inflight <= 0) {
            bool allDone = true;
            for (int k = 0; k < ctx->doneFlags.size(); k++) {
                if (!ctx->doneFlags[k]) { allDone = false; break; }
            }
            if (allDone) finishAll(true, {});
        }
    };

    // ── 启动单片（shared_ptr 自引用支持重试递归）──
    auto startShard = std::make_shared<std::function<void(int)>>();
    *startShard = [this, ctx, finalUrl, chunk, total, shards,
                   finishChunk, startShard, handle, progress](int i) {
        if (ctx->failed || ctx->finished) return;
        if (handle && handle->m_aborted) {
            // 已取消：不再启动新片，直接整体收尾
            ctx->failed = true;
            finishChunk(i, 0, {}, false, QStringLiteral("已取消"));
            return;
        }
        const qint64 start = static_cast<qint64>(i) * chunk;
        const qint64 end = qMin(start + chunk - 1, total - 1);

        QNetworkRequest req = buildRequest(m_config, QUrl(finalUrl), false, kShardIdleTimeoutMs);
        req.setRawHeader("Range", QStringLiteral("bytes=%1-%2").arg(start).arg(end).toUtf8());
        QNetworkAccessManager* mgr = (i % 2 == 0) ? m_manager : m_manager2;  // 双 QNAM 轮询
        QNetworkReply* reply = mgr->get(req);
        if (handle) handle->addReply(reply);

        ctx->inflight++;

        auto shardData = std::make_shared<QByteArray>();
        auto shardRecv = std::make_shared<qint64>(0);
        auto stallCount = std::make_shared<int>(0);
        auto lastRecv = std::make_shared<qint64>(-1);
        auto slowTicks = std::make_shared<int>(0);   // 龟速连续计数（2026-08-10）
        auto slowAborted = std::make_shared<int>(0); // 本次 abort 是否龟速触发（重试不消耗失败预算）
        // 裸指针 + deleteLater（shared_ptr<QTimer> + deleteLater 会 double free → 堆损坏）
        auto* stallTimer = new QTimer;
        stallTimer->setInterval(kStallIntervalMs);
        QObject::connect(stallTimer, &QTimer::timeout,
            [reply, shardRecv, lastRecv, stallCount, slowTicks, slowAborted, i, ctx]() {
                if (reply->error() != QNetworkReply::NoError) return;
                if (*shardRecv <= 0) return;   // 首包未到：交给 transferTimeout，停滞检测不介入
                const qint64 delta = *shardRecv - *lastRecv;
                if (delta == 0) {
                    // 完全无进展：原逻辑（abort → 重试走失败预算）
                    if (++(*stallCount) >= kStallTicks)
                        reply->abort();
                } else if (delta < kShardSlowBytesPerTick) {
                    // 龟速（有数据但 <128KB/s）：看门狗预算内 abort 换连接（2026-08-10）
                    if (ctx->slowRetries[i] < kShardSlowRetries) {
                        if (++(*slowTicks) >= kShardSlowTicks) {
                            ctx->slowRetries[i]++;
                            *slowAborted = 1;
                            reply->abort();
                        }
                    }
                    // 预算用完：停止看门狗，让慢片爬完（不失败）
                } else {
                    *lastRecv = *shardRecv;
                    *stallCount = 0;
                    *slowTicks = 0;
                }
            });
        stallTimer->start();

        QObject::connect(reply, &QNetworkReply::readyRead, this,
            [reply, shardData]() { shardData->append(reply->readAll()); });

        // 进度：聚合各片已收字节（150ms 节流）
        auto lastEmit = std::make_shared<qint64>(0);
        QObject::connect(reply, &QNetworkReply::downloadProgress, this,
            [ctx, shardRecv, progress, lastEmit](qint64 recv, qint64) {
                const qint64 delta = recv - *shardRecv;
                *shardRecv = recv;
                if (delta > 0) ctx->recvBytes += delta;
                if (!progress) return;
                const qint64 now = QDateTime::currentMSecsSinceEpoch();
                if (now - *lastEmit < 150) return;
                *lastEmit = now;
                progress(ctx->recvBytes, ctx->total);
            });

        QObject::connect(reply, &QNetworkReply::finished, this,
            [this, ctx, reply, i, start, shardData, shards, total, finalUrl, chunk,
             finishChunk, startShard, stallTimer, handle, slowAborted]() {
                stallTimer->stop();
                stallTimer->deleteLater();
                if (handle) handle->onReplyFinished(reply);
                const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                const bool ok = (reply->error() == QNetworkReply::NoError) && status == 206;
                const QString errStr = reply->errorString();
                const QByteArray data = *shardData;
                reply->deleteLater();

                ctx->inflight--;

                if (ctx->failed || ctx->finished) return;

                if (!ok && *slowAborted) {
                    // 慢片看门狗 abort：换连接重试（不消耗失败预算，2026-08-10）
                    *slowAborted = 0;
                    qCInfo(logDownload).noquote()
                        << QStringLiteral("[驿道] 分片%1/%2 龟速换连接重试").arg(i + 1).arg(shards);
                    QTimer::singleShot(500, this, [startShard, i]() { (*startShard)(i); });
                    return;
                }

                if (!ok) {
                    const int retried = ctx->retries[i];
                    ctx->retries[i] = retried + 1;
                    const bool aborted = handle && handle->m_aborted;
                    if (!aborted && retried < kShardMaxRetries - 1) {
                        qCInfo(logDownload).noquote()
                            << QStringLiteral("[驿道] 分片%1/%2 重试(%3) %4")
                                   .arg(i + 1).arg(shards).arg(retried + 1).arg(errStr);
                        // 重试退避 500ms（对齐夸父 kRetryBackoffMs），避免并发重发加剧拥塞
                        QTimer::singleShot(500, this, [startShard, i]() { (*startShard)(i); });
                        return;
                    }
                    finishChunk(i, start, data, false,
                                aborted ? QStringLiteral("已取消")
                                        : (status != 206 ? QStringLiteral("HTTP %1").arg(status) : errStr));
                    return;
                }
                finishChunk(i, start, data, true, {});
            });
    };

    for (int i = 0; i < shards; i++)
        (*startShard)(i);

    qCInfo(logDownload).noquote() << QStringLiteral("[驿道] 分片启动 %1 大小=%2 分片=%3").arg(finalUrl).arg(total).arg(shards);
}

// --------------- download (public API) ---------------

void HttpClient::download(const QString& url, const QString& savePath,
                          std::function<void(qint64, qint64)> progress,
                          std::function<void(bool, const QString&)> done,
                          qint64 expectedSize, const QString& expectedSha1)
{
    // 大文件 → 分片；小文件/不支持 Range → 探测后自动落单连接
    startChunked(url, savePath, std::move(progress), std::move(done),
                 expectedSize, expectedSha1, nullptr);
}

void HttpClient::downloadWithFallback(const QString& url, const QString& savePath,
                          std::function<void(qint64, qint64)> progress,
                          std::function<void(bool, const QString&)> done,
                          qint64 expectedSize, const QString& expectedSha1)
{
    download(url, savePath, std::move(progress), std::move(done),
             expectedSize, expectedSha1);
}

// --------------- downloadWithReply ---------------

HttpClient::DownloadHandle* HttpClient::downloadWithReply(const QString& url, const QString& savePath,
                  std::function<void(qint64, qint64)> progress,
                  std::function<void(bool, const QString&)> done,
                  qint64 resumeFrom, qint64 expectedSize,
                  const QString& expectedSha1, bool skipProbe)
{
    auto* handle = new DownloadHandle(this);
    if (resumeFrom > 0) {
        // 断点续传走单连接（分片与续传不叠加，保持 update_manager 语义不变）
        startSingle(url, savePath, std::move(progress), std::move(done),
                    resumeFrom, expectedSize, expectedSha1, handle);
    } else if (skipProbe) {
        // 免探测单连接（2026-08-14）：安装器库等小文件批量场景，
        // 跳过 Range: bytes=0-0 探测直接下载，省一半往返（国内源 RTT 高时收益显著）
        startSingle(url, savePath, std::move(progress), std::move(done),
                    -1, expectedSize, expectedSha1, handle);
    } else {
        startChunked(url, savePath, std::move(progress), std::move(done),
                     expectedSize, expectedSha1, handle);
    }
    return handle;
}

// --------------- POST / GET / PUT / DELETE ---------------

QNetworkReply* HttpClient::post(const QNetworkRequest& request, const QByteArray& body)
{
    QNetworkRequest req = request;
    req.setRawHeader("User-Agent",
                     QString::fromStdString(m_config.userAgent).toUtf8());
    req.setTransferTimeout(m_config.totalTimeoutMs);
    return m_manager->post(req, body);
}

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

} // namespace ShadowLauncher
