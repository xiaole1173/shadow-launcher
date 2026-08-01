// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
// Shadow Launcher — 资源拉取引擎（司南）
#include "resource_fetch_engine.h"
#include "engine_identity.h"
#include "../utils/logger.h"

#include <webp/decode.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
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

    // 清理历史坏缓存：此前版本可能把 webp/错误页等内容原样存成 .png，QML 解码失败会卡占位图
    // 用 PNG 魔数判断（比 QImageReader::canRead 可靠，canRead 对 RIFF/webp 可能误判）
    int cleaned = 0;
    const auto iconFiles = QDir(m_cacheRoot + "/icons")
                               .entryInfoList({QStringLiteral("*.png")}, QDir::Files);
    for (const QFileInfo& fi : iconFiles) {
        QFile f(fi.absoluteFilePath());
        if (!f.open(QIODevice::ReadOnly)) { continue; }
        const QByteArray head = f.read(8);
        f.close();
        const bool isPng = (head.size() >= 8 && head[0] == '\x89' && head[1] == 'P'
                            && head[2] == 'N' && head[3] == 'G');
        if (!isPng) {
            QFile::remove(fi.absoluteFilePath());
            ++cleaned;
        }
    }
    if (cleaned > 0)
        qCWarning(logDownload) << engineTag(kEngineId)
                               << QStringLiteral("清理 %1 个历史坏图标缓存").arg(cleaned);
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
    const QUrl qurl(downloadUrl(url)); // 规范化（修复 Modrinth API 偶发的 //data 双斜杠）
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
    // 网络失败或极小文件（镜像错误页特征）→ 走重试
    if (!ok || data.size() < 256) {
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

    // 解码验证：先 Qt 后 libwebp（Modrinth 图标常见 webp，Qt 无插件）；都解不了（错误页等）→ 丢弃
    QImage img;
    bool decoded = img.loadFromData(data);
    if (!decoded) {
        int w = 0, h = 0;
        uint8_t* rgba = WebPDecodeRGBA(
            reinterpret_cast<const uint8_t*>(data.constData()), data.size(), &w, &h);
        if (rgba && w > 0 && h > 0) {
            img = QImage(rgba, w, h, QImage::Format_RGBA8888,
                         [](void* p) { WebPFree(p); }, rgba);
            decoded = true;
        } else {
            WebPFree(rgba);
        }
    }
    if (!decoded) {
        qCWarning(logDownload) << engineTag(kEngineId)
                               << QStringLiteral("图标格式无法解码，丢弃 %1 (size=%2)")
                                      .arg(url.left(60)).arg(data.size());
        return;
    }

    // 统一转 PNG 存原图（QML 必能解码）
    if (!img.save(iconFile(url), "PNG")) {
        qCWarning(logDownload) << engineTag(kEngineId)
                               << QStringLiteral("图标写盘失败 %1").arg(url.left(60));
        return;
    }
    // 生成 88px 缩略图
    makeThumbnail(url, img);

    QString lp = iconFile(url);
    if (QFileInfo::exists(thumbFile(url)))
        lp = thumbFile(url);
    qCInfo(logDownload) << engineTag(kEngineId)
                        << QStringLiteral("图标就绪 %1 (%2 KB)").arg(url.left(60)).arg(data.size() / 1024);
    emit iconReady(url, fileUrl(lp));
}

void ResourceFetchEngine::makeThumbnail(const QString& url, const QImage& img)
{
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

QString ResourceFetchEngine::downloadUrl(const QString& url)
{
    QString u = url;
    const int schemeEnd = u.indexOf(QStringLiteral("://"));
    if (schemeEnd >= 0) {
        int slash = u.indexOf(QLatin1Char('/'), schemeEnd + 3);
        while (slash >= 0 && slash + 1 < u.size() && u.at(slash + 1) == QLatin1Char('/'))
            u.remove(slash, 1);
    }
    return u;
}

} // namespace ShadowLauncher
