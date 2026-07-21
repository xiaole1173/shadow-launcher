// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "yggdrasil_auth.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QLoggingCategory>
#include <QRandomGenerator>

Q_LOGGING_CATEGORY(logYggAuth, "shadow.yggdrasil.auth")

namespace ShadowLauncher {

// ── YggdrasilProfile ──

QJsonObject YggdrasilProfile::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("id")] = id;
    obj[QStringLiteral("name")] = name;
    return obj;
}

YggdrasilProfile YggdrasilProfile::fromJson(const QJsonObject &obj)
{
    YggdrasilProfile p;
    p.id = obj.value(QStringLiteral("id")).toString();
    p.name = obj.value(QStringLiteral("name")).toString();
    return p;
}

// ── YggdrasilSession ──

void YggdrasilSession::clear()
{
    apiRoot.clear();
    email.clear();
    accessToken.clear();
    clientToken.clear();
    profiles.clear();
    selectedProfileIndex = 0;
    username.clear();
    uuid.clear();
}

// ── YggdrasilAuth ──

YggdrasilAuth::YggdrasilAuth(QNetworkAccessManager *nam)
    : m_nam(nam)
{
    Q_ASSERT(m_nam);
}

QString YggdrasilAuth::generateClientToken()
{
    // 生成一个随机的 UUID v4
    quint64 r1 = QRandomGenerator::global()->generate64();
    quint64 r2 = QRandomGenerator::global()->generate64();
    return QStringLiteral("%1-%2-%3-%4-%5")
        .arg(r1 & 0xFFFFFFFF, 8, 16, QLatin1Char('0'))
        .arg((r1 >> 32) & 0xFFFF, 4, 16, QLatin1Char('0'))
        .arg(((r1 >> 48) & 0x0FFF) | 0x4000, 4, 16, QLatin1Char('0'))
        .arg((r2 & 0x3FFF) | 0x8000, 4, 16, QLatin1Char('0'))
        .arg((r2 >> 16) & 0xFFFFFFFFFFFF, 12, 16, QLatin1Char('0'));
}

QNetworkReply* YggdrasilAuth::doPost(const QString &url, const QJsonObject &body)
{
    QUrl qurl(url);
    QNetworkRequest req(qurl);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json; charset=utf-8"));
    req.setRawHeader("User-Agent", "ShadowLauncher/1.0");
    QByteArray data = QJsonDocument(body).toJson(QJsonDocument::Compact);
    return m_nam->post(req, data);
}

QNetworkReply* YggdrasilAuth::doGet(const QString &url)
{
    QUrl qurl(url);
    QNetworkRequest req(qurl);
    req.setRawHeader("User-Agent", "ShadowLauncher/1.0");
    return m_nam->get(req);
}

// ── 请求 ──

QNetworkReply* YggdrasilAuth::fetchMeta(const QString &apiRoot)
{
    return doGet(apiRoot);
}

QNetworkReply* YggdrasilAuth::authenticate(const QString &apiRoot,
                                            const QString &username,
                                            const QString &password,
                                            const QString &clientToken)
{
    QJsonObject payload;
    QJsonObject agent;
    agent[QStringLiteral("name")] = QStringLiteral("Minecraft");
    agent[QStringLiteral("version")] = 1;
    payload[QStringLiteral("agent")] = agent;
    payload[QStringLiteral("username")] = username;
    payload[QStringLiteral("password")] = password;
    payload[QStringLiteral("clientToken")] = clientToken;
    payload[QStringLiteral("requestUser")] = true;

    QString url = apiRoot;
    if (!url.endsWith(QLatin1Char('/')))
        url += QLatin1Char('/');
    url += QStringLiteral("authserver/authenticate");

    return doPost(url, payload);
}

QNetworkReply* YggdrasilAuth::refresh(const QString &apiRoot,
                                       const QString &accessToken,
                                       const QString &clientToken)
{
    QJsonObject payload;
    payload[QStringLiteral("accessToken")] = accessToken;
    payload[QStringLiteral("clientToken")] = clientToken;
    payload[QStringLiteral("requestUser")] = true;

    QString url = apiRoot;
    if (!url.endsWith(QLatin1Char('/')))
        url += QLatin1Char('/');
    url += QStringLiteral("authserver/refresh");

    return doPost(url, payload);
}

QNetworkReply* YggdrasilAuth::validate(const QString &apiRoot,
                                        const QString &accessToken)
{
    QJsonObject payload;
    payload[QStringLiteral("accessToken")] = accessToken;

    QString url = apiRoot;
    if (!url.endsWith(QLatin1Char('/')))
        url += QLatin1Char('/');
    url += QStringLiteral("authserver/validate");

    return doPost(url, payload);
}

QNetworkReply* YggdrasilAuth::signout(const QString &apiRoot,
                                       const QString &username,
                                       const QString &password)
{
    QJsonObject payload;
    payload[QStringLiteral("username")] = username;
    payload[QStringLiteral("password")] = password;

    QString url = apiRoot;
    if (!url.endsWith(QLatin1Char('/')))
        url += QLatin1Char('/');
    url += QStringLiteral("authserver/signout");

    return doPost(url, payload);
}

QNetworkReply* YggdrasilAuth::invalidate(const QString &apiRoot,
                                          const QString &accessToken,
                                          const QString &clientToken)
{
    QJsonObject payload;
    payload[QStringLiteral("accessToken")] = accessToken;
    payload[QStringLiteral("clientToken")] = clientToken;

    QString url = apiRoot;
    if (!url.endsWith(QLatin1Char('/')))
        url += QLatin1Char('/');
    url += QStringLiteral("authserver/invalidate");

    return doPost(url, payload);
}

// ── 响应解析 ──

YggdrasilMeta YggdrasilAuth::parseMeta(const QByteArray &data, QString &errorOut)
{
    YggdrasilMeta meta;
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        errorOut = QStringLiteral("元数据 JSON 解析失败: ") + err.errorString();
        return meta;
    }

    QJsonObject root = doc.object();
    QJsonObject metaObj = root.value(QStringLiteral("meta")).toObject();
    meta.serverName = metaObj.value(QStringLiteral("serverName")).toString();
    meta.registerUrl = metaObj.value(QStringLiteral("registerUrl")).toString();
    return meta;
}

YggdrasilSession YggdrasilAuth::parseAuthenticate(const QByteArray &data,
                                                    const QString &apiRoot,
                                                    const QString &emailInput,
                                                    QString &errorOut)
{
    YggdrasilSession session;
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        errorOut = QStringLiteral("登录响应 JSON 解析失败: ") + err.errorString();
        return session;
    }

    QJsonObject root = doc.object();

    // 错误检查
    if (root.contains(QStringLiteral("error"))) {
        QString errType = root.value(QStringLiteral("error")).toString();
        QString errMsg = root.value(QStringLiteral("errorMessage")).toString();
        QString cause = root.value(QStringLiteral("cause")).toString();
        errorOut = QStringLiteral("[%1] %2").arg(errType, errMsg);
        if (!cause.isEmpty())
            errorOut += QStringLiteral(" (%1)").arg(cause);
        return session;
    }

    session.apiRoot = apiRoot;
    session.email = emailInput;
    session.accessToken = root.value(QStringLiteral("accessToken")).toString();
    session.clientToken = root.value(QStringLiteral("clientToken")).toString();

    // 角色列表
    QJsonArray profilesArr = root.value(QStringLiteral("availableProfiles")).toArray();
    for (const QJsonValue &v : profilesArr) {
        QJsonObject po = v.toObject();
        YggdrasilProfile p;
        p.id = po.value(QStringLiteral("id")).toString();
        p.name = po.value(QStringLiteral("name")).toString();
        session.profiles.append(p);
    }

    // 选中的角色
    QJsonObject selected = root.value(QStringLiteral("selectedProfile")).toObject();
    session.uuid = selected.value(QStringLiteral("id")).toString();
    session.username = selected.value(QStringLiteral("name")).toString();

    // 如果没有 selectedProfile 但有 profiles，默认选第一个
    if (session.username.isEmpty() && !session.profiles.isEmpty()) {
        session.username = session.profiles[0].name;
        session.uuid = session.profiles[0].id;
        session.selectedProfileIndex = 0;
    }

    if (session.accessToken.isEmpty()) {
        errorOut = QStringLiteral("返回数据中没有 accessToken");
        session.clear();
    }

    return session;
}

bool YggdrasilAuth::parseValidate(const QByteArray &data, QString &errorOut)
{
    // validate 返回 204 No Content 表示有效
    Q_UNUSED(data);
    Q_UNUSED(errorOut);
    return !data.isEmpty(); // 空数据 = 204 = 有效; 有内容可能是错误
}

YggdrasilSession YggdrasilAuth::parseRefresh(const QByteArray &data,
                                               const QString &apiRoot,
                                               const QString &email,
                                               QString &errorOut)
{
    YggdrasilSession session;
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        errorOut = QStringLiteral("刷新响应 JSON 解析失败: ") + err.errorString();
        return session;
    }

    QJsonObject root = doc.object();
    if (root.contains(QStringLiteral("error"))) {
        QString errType = root.value(QStringLiteral("error")).toString();
        QString errMsg = root.value(QStringLiteral("errorMessage")).toString();
        errorOut = QStringLiteral("[%1] %2").arg(errType, errMsg);
        return session;
    }

    session.apiRoot = apiRoot;
    session.email = email;
    session.accessToken = root.value(QStringLiteral("accessToken")).toString();
    session.clientToken = root.value(QStringLiteral("clientToken")).toString();

    QJsonObject selected = root.value(QStringLiteral("selectedProfile")).toObject();
    session.uuid = selected.value(QStringLiteral("id")).toString();
    session.username = selected.value(QStringLiteral("name")).toString();

    if (session.accessToken.isEmpty()) {
        errorOut = QStringLiteral("刷新返回数据中没有 accessToken");
    }

    return session;
}

} // namespace ShadowLauncher
