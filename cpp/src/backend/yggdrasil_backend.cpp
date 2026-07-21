// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "yggdrasil_backend.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QEventLoop>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logYggBackend, "shadow.yggdrasil.backend")

namespace ShadowLauncher {

static QString yggdrasilDataDir()
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                   + QStringLiteral("/yggdrasil");
    QDir().mkpath(path);
    return path;
}

YggdrasilBackend::YggdrasilBackend(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_auth(m_nam)
{
    loadSession();
}

QVariantList YggdrasilBackend::profiles() const
{
    QVariantList list;
    for (const auto &p : m_session.profiles) {
        QVariantMap m;
        m[QStringLiteral("id")] = p.id;
        m[QStringLiteral("name")] = p.name;
        list.append(m);
    }
    return list;
}

void YggdrasilBackend::setStatus(const QString &msg)
{
    if (m_statusMessage != msg) {
        m_statusMessage = msg;
        emit statusMessageChanged();
    }
}

void YggdrasilBackend::fetchMeta(const QString &apiRoot)
{
    if (apiRoot.trimmed().isEmpty()) {
        emit metaFailed(QStringLiteral("认证服务器地址不能为空"));
        return;
    }

    setStatus(QStringLiteral("正在获取服务器信息..."));
    QNetworkReply *reply = m_auth.fetchMeta(apiRoot.trimmed());
    connect(reply, &QNetworkReply::finished, this, &YggdrasilBackend::onMetaReply);
}

void YggdrasilBackend::login(const QString &apiRoot, const QString &email, const QString &password)
{
    if (apiRoot.trimmed().isEmpty() || email.trimmed().isEmpty() || password.isEmpty()) {
        emit loginFailed(QStringLiteral("请填写完整的登录信息"));
        return;
    }

    m_pendingApiRoot = apiRoot.trimmed();
    m_pendingEmail = email.trimmed();
    m_pendingPassword = password;  // 暂存，登出时可能用

    setStatus(QStringLiteral("正在登录..."));
    qCDebug(logYggBackend) << "Starting yggdrasil login for" << m_pendingEmail << "at" << apiRoot;

    QNetworkReply *reply = m_auth.authenticate(
        apiRoot.trimmed(), email.trimmed(), password,
        YggdrasilAuth::generateClientToken());

    connect(reply, &QNetworkReply::finished, this, &YggdrasilBackend::onAuthenticateReply);
}

void YggdrasilBackend::refreshToken()
{
    if (!m_session.isValid() || m_session.accessToken.isEmpty()) return;

    setStatus(QStringLiteral("正在刷新令牌..."));
    QNetworkReply *reply = m_auth.refresh(
        m_session.apiRoot, m_session.accessToken, m_session.clientToken);
    connect(reply, &QNetworkReply::finished, this, &YggdrasilBackend::onRefreshReply);
}

void YggdrasilBackend::logout()
{
    if (!m_session.isValid()) return;

    m_loggingOut = true;
    setStatus(QStringLiteral("正在登出..."));

    // 尝试 invalidate 服务端令牌（不需要等结果，本地直接清）
    if (!m_session.accessToken.isEmpty()) {
        QNetworkReply *reply = m_auth.invalidate(
            m_session.apiRoot, m_session.accessToken, m_session.clientToken);
        connect(reply, &QNetworkReply::finished, this, &YggdrasilBackend::onLogoutReply);
    }

    // 不管 invalidate 是否成功，本地先清理
    m_session.clear();
    m_meta = YggdrasilMeta();
    m_pendingEmail.clear();
    m_pendingPassword.clear();
    m_loggingOut = false;
    setStatus(QString());
    deleteSavedSession();
    emit stateChanged();
    emit profilesChanged();
}

void YggdrasilBackend::selectProfile(int index)
{
    if (index < 0 || index >= m_session.profiles.size()) return;
    m_session.selectedProfileIndex = index;
    m_session.username = m_session.profiles[index].name;
    m_session.uuid = m_session.profiles[index].id;
    emit profilesChanged();
    emit stateChanged();
    saveSession();
}

QString YggdrasilBackend::sessionFilePath() const
{
    return yggdrasilDataDir() + QStringLiteral("/session.json");
}

void YggdrasilBackend::saveSession()
{
    QJsonObject obj;
    obj[QStringLiteral("apiRoot")]         = m_session.apiRoot;
    obj[QStringLiteral("email")]           = m_session.email;
    obj[QStringLiteral("accessToken")]     = m_session.accessToken;
    obj[QStringLiteral("clientToken")]     = m_session.clientToken;
    obj[QStringLiteral("username")]        = m_session.username;
    obj[QStringLiteral("uuid")]            = m_session.uuid;
    obj[QStringLiteral("profileIndex")]    = m_session.selectedProfileIndex;

    QJsonArray profiles;
    for (const auto &p : m_session.profiles) {
        profiles.append(p.toJson());
    }
    obj[QStringLiteral("profiles")] = profiles;

    // 也存 meta 信息
    QJsonObject metaObj;
    metaObj[QStringLiteral("serverName")] = m_meta.serverName;
    metaObj[QStringLiteral("registerUrl")] = m_meta.registerUrl;
    obj[QStringLiteral("meta")] = metaObj;

    QJsonDocument doc(obj);
    QFile file(sessionFilePath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
        qCDebug(logYggBackend) << "Saved yggdrasil session";
    }
}

void YggdrasilBackend::loadSession()
{
    QFile file(sessionFilePath());
    m_hadSavedSession = file.exists();
    if (!file.open(QIODevice::ReadOnly)) return;

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) return;

    QJsonObject root = doc.object();
    m_session.apiRoot          = root.value(QStringLiteral("apiRoot")).toString();
    m_session.email            = root.value(QStringLiteral("email")).toString();
    m_session.accessToken      = root.value(QStringLiteral("accessToken")).toString();
    m_session.clientToken      = root.value(QStringLiteral("clientToken")).toString();
    m_session.username         = root.value(QStringLiteral("username")).toString();
    m_session.uuid             = root.value(QStringLiteral("uuid")).toString();
    m_session.selectedProfileIndex = root.value(QStringLiteral("profileIndex")).toInt();

    QJsonArray profilesArr = root.value(QStringLiteral("profiles")).toArray();
    m_session.profiles.clear();
    for (const QJsonValue &v : profilesArr) {
        m_session.profiles.append(YggdrasilProfile::fromJson(v.toObject()));
    }

    QJsonObject metaObj = root.value(QStringLiteral("meta")).toObject();
    m_meta.serverName = metaObj.value(QStringLiteral("serverName")).toString();
    m_meta.registerUrl = metaObj.value(QStringLiteral("registerUrl")).toString();

    if (m_session.isValid()) {
        qCDebug(logYggBackend) << "Loaded yggdrasil session for" << m_session.username;
        emit stateChanged();
        if (!m_session.profiles.isEmpty())
            emit profilesChanged();

        // 启动时验证 token 有效性，无效则尝试刷新
        QNetworkReply *valReply = m_auth.validate(m_session.apiRoot, m_session.accessToken);
        QEventLoop loop;
        QObject::connect(valReply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (valReply->error() != QNetworkReply::NoError) {
            // token 可能过期，尝试刷新
            qCDebug(logYggBackend) << "Token validation failed, attempting refresh...";
            QNetworkReply *refReply = m_auth.refresh(
                m_session.apiRoot, m_session.accessToken, m_session.clientToken);
            QEventLoop loop2;
            QObject::connect(refReply, &QNetworkReply::finished, &loop2, &QEventLoop::quit);
            loop2.exec();

            if (refReply->error() == QNetworkReply::NoError) {
                QByteArray refData = refReply->readAll();
                QString refError;
                YggdrasilSession refreshed = YggdrasilAuth::parseRefresh(
                    refData, m_session.apiRoot, m_session.email, refError);
                if (refError.isEmpty()) {
                    // 保留 profiles 和 index
                    refreshed.profiles = m_session.profiles;
                    refreshed.selectedProfileIndex = m_session.selectedProfileIndex;
                    m_session = refreshed;
                    saveSession();
                    qCDebug(logYggBackend) << "Token refreshed successfully";
                    emit stateChanged();
                }
            } else {
                qCWarning(logYggBackend) << "Token refresh failed, clearing session";
                m_session.clear();
                deleteSavedSession();
                emit stateChanged();
            }
            refReply->deleteLater();
        }
        valReply->deleteLater();
    }
}

void YggdrasilBackend::deleteSavedSession()
{
    QFile::remove(sessionFilePath());
}

// ── Slots (网络回复处理) ──

void YggdrasilBackend::onMetaReply()
{
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();

    if (reply->error() == QNetworkReply::OperationCanceledError) return;

    if (reply->error() != QNetworkReply::NoError) {
        QString err = reply->errorString();
        setStatus(QString());
        emit metaFailed(QStringLiteral("获取服务器信息失败: ") + err);
        return;
    }

    QByteArray data = reply->readAll();
    QString errorOut;
    YggdrasilMeta meta = YggdrasilAuth::parseMeta(data, errorOut);

    if (!errorOut.isEmpty()) {
        setStatus(QString());
        emit metaFailed(errorOut);
        return;
    }

    m_meta = meta;
    setStatus(QStringLiteral("已连接: ") + meta.serverName);
    emit metaReady();
}

void YggdrasilBackend::onAuthenticateReply()
{
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();

    if (reply->error() == QNetworkReply::OperationCanceledError) {
        setStatus(QString());
        emit loginFailed(QStringLiteral("登录已取消"));
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        // 尝试从响应体解析结构化错误信息
        QByteArray body = reply->readAll();
        QString errorOut;
        YggdrasilSession tmp = YggdrasilAuth::parseAuthenticate(body, m_pendingApiRoot, m_pendingEmail, errorOut);
        setStatus(QString());
        if (!errorOut.isEmpty())
            emit loginFailed(errorOut);
        else
            emit loginFailed(QStringLiteral("网络错误: ") + reply->errorString());
        return;
    }

    QByteArray data = reply->readAll();
    QString errorOut;
    YggdrasilSession session = YggdrasilAuth::parseAuthenticate(
        data, m_pendingEmail, m_pendingEmail, errorOut);

    if (!errorOut.isEmpty()) {
        setStatus(QString());
        emit loginFailed(errorOut);
        return;
    }

    // 成功！
    m_session = session;
    m_pendingPassword.clear();
    setStatus(QString());
    saveSession();

    qCDebug(logYggBackend) << "Yggdrasil login success:" << m_session.username;
    emit loginSuccess();
    emit stateChanged();
    if (!m_session.profiles.isEmpty())
        emit profilesChanged();
}

void YggdrasilBackend::onRefreshReply()
{
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();

    if (reply->error() == QNetworkReply::OperationCanceledError) return;

    if (reply->error() != QNetworkReply::NoError) {
        qCWarning(logYggBackend) << "Yggdrasil token refresh failed:" << reply->errorString();
        // 刷新失败不破坏已有 session，下次启动时再试
        setStatus(QString());
        return;
    }

    QByteArray data = reply->readAll();
    QString errorOut;
    YggdrasilSession session = YggdrasilAuth::parseRefresh(
        data, m_session.apiRoot, m_session.email, errorOut);

    if (!errorOut.isEmpty()) {
        qCWarning(logYggBackend) << "Yggdrasil token refresh parse failed:" << errorOut;
        setStatus(QString());
        return;
    }

    // 保留 profiles 和 index
    session.profiles = m_session.profiles;
    session.selectedProfileIndex = m_session.selectedProfileIndex;
    m_session = session;
    saveSession();
    emit stateChanged();
    setStatus(QString());
    qCDebug(logYggBackend) << "Yggdrasil token refreshed";
}

void YggdrasilBackend::onLogoutReply()
{
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();
    // 登出不需要额外处理，已经在 logout() 中清理了
}

} // namespace ShadowLauncher
