// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
// Shadow Launcher — 资源拉取引擎（司南）
#include "resource_fetch_engine.h"
#include "engine_identity.h"
#include "../utils/logger.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPixmap>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QCryptographicHash>
#include <QDateTime>
#include <QTimer>
#include <QUrl>

namespace ShadowLauncher {

ResourceFetchEngine::ResourceFetchEngine(const QString& cacheRoot, QObject* parent)
    : QObject(parent)
    , m_cacheRoot(cacheRoot)
{
    QDir().mkpath(m_cacheRoot + "/icons");
    QDir().mkpath(m_cacheRoot + "/thumbs");
    m_nam.setTransferTimeout(20000);
    qCInfo(logDownload) << engineBanner(kEngineId);
    qCInfo(logDownload) << engineTag(kEngineId) << QStringLiteral("缓存根目录: %1").arg(m_cacheRoot);
}

// ═══════════════════════════════ API JSON ═══════════════════════════════

void ResourceFetchEngine::getJson(const QString& url, bool cacheable,
                                  JsonDone done, JsonFail fail)
{
    if (cacheable) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        auto it = m_jsonCache.constFind(url);
        if (it != m_jsonCache.constEnd() && (now - it->ts) < kJsonTtlMs) {
            qCInfo(logDownload) << engineTag(kEngineId)
                                << QStringLiteral("搜索缓存命中(url 前80字符) %1").arg(url.left(80));
            if (done) done(200, it->body);
            return;
        }
    }
    m_apiQueue.enqueue({url, cacheable, std::move(done), std::move(fail), 0});
    pumpApi();
}

void ResourceFetchEngine::pumpApi()
{
    while (m_apiActive < kApiConcurrent && !m_apiQueue.isEmpty()) {
        ApiReq req = m_apiQueue.dequeue();
        startApiRequest(std::move(req));
    }
}

void ResourceFetchEngine::startApiRequest(ApiReq req)
{
    ++m_apiActive;
    const QUrl qurl(req.url);
    QNetworkRequest request(qurl);
    request.setRawHeader("User-Agent", "ShadowLauncher");
    request.setTransferTimeout(15000);
    QNetworkReply* reply = m_nam.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, req, reply]() {
        reply->deleteLater();
        --m_apiActive;
        const QByteArray body = reply->readAll();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() == QNetworkReply::NoError && status == 200) {
            if (req.cacheable) {
                m_jsonCache.insert(req.url, {body, QDateTime::currentMSecsSinceEpoch()});
                m_jsonOrder.removeAll(req.url);
                m_jsonOrder.append(req.url);
                while (m_jsonOrder.size() > kJsonMax) {
                    const QString ev = m_jsonOrder.takeFirst();
                    m_jsonCache.remove(ev);
                }
            }
            if (req.done) req.done(status, body);
        } else if (req.retries < kApiRetryMax) {
            qCWarning(logDownload) << engineTag(kEngineId)
                                   << QStringLiteral("API 失败(status=%1)，重试 %2：%3")
                                          .arg(status).arg(req.url.left(60), reply->errorString());
            m_apiQueue.enqueue({req.url, req.cacheable, req.done, req.fail, req.retries + 1});
        } else {
            if (req.fail) req.fail(reply->errorString());
        }
        pumpApi();
    });
}

// ═══════════════════════════════ 图标 ═══════════════════════════════

QString ResourceFetchEngine::iconLocalPath(const QString& url, bool preferThumb)
{
    if (url.isEmpty())
        return {};
    if (preferThumb) {
        const QString t = thumbFile(url);
        if (QFileInfo::exists(t))
            return fileUrl(t);
    }
    const QString o = iconFile(url);
    if (QFileInfo::exists(o))
        return fileUrl(o);

    // 未命中 → 排队下载（去重：已在队列/下载中则跳过）
    if (!m_iconActive.contains(url) && !m_iconQueued.contains(url)) {
        m_iconQueued.insert(url);
        m_iconQueue.enqueue({url, 0});
        pumpIcons();
    }
    return {};
}

void ResourceFetchEngine::prefetchIcons(const QStringList& urls)
{
    for (const QString& u : urls)
        iconLocalPath(u, true);
}

void ResourceFetchEngine::pumpIcons()
{
    while (m_iconActive.size() < kIconConcurrent && !m_iconQueue.isEmpty()) {
        IconReq req = m_iconQueue.dequeue();
        m_iconQueued.remove(req.url);
        startIconDownload(req.url);
    }
}

void ResourceFetchEngine::startIconDownload(const QString& url)
{
    m_iconActive.insert(url);
    const QUrl qurl(url);
    QNetworkRequest request(qurl);
    request.setRawHeader("User-Agent", "ShadowLauncher");
    request.setTransferTimeout(10000); // 镜像 TTFB 3~8s，10s 超时快速失败重试，避免堵住队列
    QNetworkReply* reply = m_nam.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, url, reply]() {
        reply->deleteLater();
        const QByteArray data = reply->readAll();
        const bool ok = (reply->error() == QNetworkReply::NoError) && !data.isEmpty();
        onIconData(url, data, ok, reply->errorString());
        pumpIcons();
    });
}

void ResourceFetchEngine::onIconData(const QString& url, const QByteArray& data,
                                     bool ok, const QString& err)
{
    m_iconActive.remove(url);
    if (!ok) {
        int& r = m_iconRetries[url];
        if (r < kIconRetryMax) {
            ++r;
            qCWarning(logDownload) << engineTag(kEngineId)
                                   << QStringLiteral("图标下载失败(第%1次) %2 → 稍后重试")
                                          .arg(r).arg(url.left(60));
            QTimer::singleShot(800 * r, this, [this, url, r]() {
                if (!m_iconActive.contains(url)) {
                    m_iconQueued.insert(url);
                    m_iconQueue.enqueue({url, r});
                    pumpIcons();
                }
            });
        } else {
            qCWarning(logDownload) << engineTag(kEngineId)
                                   << QStringLiteral("图标放弃(重试%1次耗尽) %2：%3")
                                          .arg(r).arg(url.left(60), err);
        }
        return;
    }

    // 写原图
    const QString of = iconFile(url);
    {
        QFile f(of);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(data);
            f.close();
        }
    }
    // 生成缩略图（失败不影响原图）
    makeThumbnail(url, data);

    QString lp = iconFile(url);
    if (QFileInfo::exists(thumbFile(url)))
        lp = thumbFile(url);
    qCInfo(logDownload) << engineTag(kEngineId)
                        << QStringLiteral("图标就绪 %1 (%2 KB)").arg(url.left(60)).arg(data.size() / 1024);
    emit iconReady(url, fileUrl(lp));
}

void ResourceFetchEngine::makeThumbnail(const QString& url, const QByteArray& data)
{
    QImage img;
    if (!img.loadFromData(data))
        return;
    const QImage thumb = img.scaled(kThumbSize, kThumbSize,
                                    Qt::KeepAspectRatio, Qt::SmoothTransformation);
    const QString tf = thumbFile(url);
    QFile f(tf);
    if (f.open(QIODevice::WriteOnly)) {
        thumb.save(&f, "PNG");
        f.close();
    }
}

// ═══════════════════════════════ 路径/工具 ═══════════════════════════════

QString ResourceFetchEngine::iconFile(const QString& url) const
{
    return m_cacheRoot + "/icons/" + hashUrl(url) + ".png";
}

QString ResourceFetchEngine::thumbFile(const QString& url) const
{
    return m_cacheRoot + "/thumbs/" + hashUrl(url) + "_" + QString::number(kThumbSize) + ".png";
}

QString ResourceFetchEngine::hashUrl(const QString& url)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(url.toUtf8(), QCryptographicHash::Sha1).toHex()).left(16);
}

QString ResourceFetchEngine::fileUrl(const QString& path)
{
    return QUrl::fromLocalFile(path).toString();
}

} // namespace ShadowLauncher
