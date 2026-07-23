// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "yggdrasil_skin_fetcher.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QUrl>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QLoggingCategory>
#include <QImage>
#include <QPainter>
#include "account_backend.h"

Q_LOGGING_CATEGORY(logYggSkin, "shadow.yggdrasil.skin")

namespace ShadowLauncher {

// ── 辅助：文件路径转 QML 可用的 URL ──
static QString toImageUrl(const QString& filePath)
{
    if (filePath.isEmpty()) return {};
    return QUrl::fromLocalFile(filePath).toString();
}

// ── 缓存目录 ──

QString YggdrasilSkinFetcher::cacheDir()
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                   + QStringLiteral("/yggdrasil/skins");
    QDir().mkpath(path);
    return path;
}

// ── 构造 ──

YggdrasilSkinFetcher::YggdrasilSkinFetcher(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

// ── PCL 方案: McSkinGetAddress + McSkinDownload ──

void YggdrasilSkinFetcher::fetchSkin(const QString &apiRoot, const QString &uuid)
{
    if (apiRoot.isEmpty() || uuid.isEmpty()) {
        m_skinPath.clear();
        m_skinVariant = 0;
        emit skinChanged();
        emit skinFailed(QStringLiteral("无法获取皮肤：参数不完整"));
        return;
    }

    // PCL McSkinGetAddress: GET {apiRoot}/sessionserver/session/minecraft/profile/{uuid}
    // (variant 信息在 profile 返回中，所以先不查缓存，等 onProfileReply 里再查): GET {apiRoot}/sessionserver/session/minecraft/profile/{uuid}
    m_pendingUuid = uuid;
    m_pendingVariant = 0;

    QString urlStr = apiRoot;
    if (!urlStr.endsWith(QLatin1Char('/')))
        urlStr.append(QLatin1Char('/'));
    urlStr += QStringLiteral("sessionserver/session/minecraft/profile/");
    urlStr += uuid;
    urlStr += QStringLiteral("?unsigned=true");

    QNetworkRequest req{QUrl(urlStr)};
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("ShadowLauncher/1.0"));
    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, &YggdrasilSkinFetcher::onProfileReply);

    qCDebug(logYggSkin) << "Fetching skin profile:" << urlStr;
}

void YggdrasilSkinFetcher::onProfileReply()
{
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit skinFailed(QStringLiteral("获取皮肤信息失败: ") + reply->errorString());
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject root = doc.object();
    QJsonArray props = root.value(QStringLiteral("properties")).toArray();

    // PCL: 找 name == "textures" → value (base64)
    QString texturesBase64;
    for (const QJsonValue &v : props) {
        QJsonObject prop = v.toObject();
        if (prop.value(QStringLiteral("name")).toString() == QStringLiteral("textures")) {
            texturesBase64 = prop.value(QStringLiteral("value")).toString();
            break;
        }
    }

    if (texturesBase64.isEmpty()) {
        emit skinFailed(QStringLiteral("皮肤数据为空"));
        return;
    }

    // PCL: Base64 解码 → JSON
    QByteArray decoded = QByteArray::fromBase64(texturesBase64.toUtf8());
    QJsonObject texturesObj = QJsonDocument::fromJson(decoded).object();
    QJsonObject textures = texturesObj.value(QStringLiteral("textures")).toObject();

    // 提取 SKIN.url + metadata.model
    QString skinUrl;
    int variant = 0;
    QJsonObject skin = textures.value(QStringLiteral("SKIN")).toObject();
    if (!skin.isEmpty()) {
        skinUrl = skin.value(QStringLiteral("url")).toString();
        QJsonObject meta = skin.value(QStringLiteral("metadata")).toObject();
        if (meta.value(QStringLiteral("model")).toString() == QStringLiteral("slim"))
            variant = 1;
    }

    if (skinUrl.isEmpty()) {
        emit skinFailed(QStringLiteral("用户未设置自定义皮肤"));
        return;
    }

    // 再查一次缓存（第一次查时 variant 还未解析）
    QString cachedPath = cacheDir() + QStringLiteral("/") + m_pendingUuid + QStringLiteral(".png");
    if (QFile::exists(cachedPath)) {
        QString headPath = cachedPath.left(cachedPath.length() - 4) + QStringLiteral("_head.png");
        if (!QFile::exists(headPath))
            headPath = AccountBackend::renderHead3D(cachedPath);
        if (headPath.isEmpty())
            headPath = cachedPath;  // fallback: 用完整皮肤
        m_skinPath = toImageUrl(headPath);
        m_skinVariant = variant;
        emit skinChanged();
        emit skinReady();
        qCDebug(logYggSkin) << "Cache hit (post-parse):" << cachedPath;
        return;
    }

    // PCL McSkinDownload: 下载皮肤 PNG
    m_pendingVariant = variant;
    QNetworkRequest skinReq{QUrl(skinUrl)};
    skinReq.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("ShadowLauncher/1.0"));
    QNetworkReply *dlReply = m_nam->get(skinReq);
    connect(dlReply, &QNetworkReply::finished, this, &YggdrasilSkinFetcher::onDownloadReply);
}

void YggdrasilSkinFetcher::onDownloadReply()
{
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit skinFailed(QStringLiteral("下载皮肤失败: ") + reply->errorString());
        return;
    }

    QByteArray pngData = reply->readAll();

    // PCL: 下载到 .downloading → 重命名（防写一半进程崩）
    QString cachedPath = cacheDir() + QStringLiteral("/") + m_pendingUuid + QStringLiteral(".png");
    QString tmpPath = cachedPath + QStringLiteral(".downloading");

    QFile file(tmpPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(pngData);
        file.close();
        QFile::remove(cachedPath);
        QFile::rename(tmpPath, cachedPath);
        // 生成头部裁剪
        QString headPath = AccountBackend::renderHead3D(cachedPath);
        if (!headPath.isEmpty())
            m_skinPath = toImageUrl(headPath);
        else
            m_skinPath = toImageUrl(cachedPath);  // fallback
        m_skinVariant = m_pendingVariant;
        qCDebug(logYggSkin) << "Skin cached:" << cachedPath;
    } else {
        emit skinFailed(QStringLiteral("保存皮肤缓存失败"));
        return;
    }

    emit skinChanged();
    emit skinReady();
}

} // namespace ShadowLauncher
