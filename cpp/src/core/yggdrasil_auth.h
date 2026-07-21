// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace ShadowLauncher {

/// Yggdrasil 认证协议中一个角色
struct YggdrasilProfile {
    QString id;   // UUID with dashes
    QString name; // Player name

    [[nodiscard]] QJsonObject toJson() const;
    static YggdrasilProfile fromJson(const QJsonObject &obj);
};

/// 认证服务器元数据
struct YggdrasilMeta {
    QString serverName;
    QString registerUrl;

    [[nodiscard]] bool isValid() const { return !serverName.isEmpty(); }
};

/// 完整认证 session
struct YggdrasilSession {
    QString apiRoot;
    QString email;
    QString accessToken;
    QString clientToken;
    QList<YggdrasilProfile> profiles;
    int selectedProfileIndex = 0;
    QString username;  // selectedProfile.name
    QString uuid;      // selectedProfile.id

    [[nodiscard]] bool isValid() const {
        return !apiRoot.isEmpty() && !accessToken.isEmpty() && !username.isEmpty();
    }
    void clear();
};

/// Yggdrasil 认证协议（纯网络逻辑，非 QObject）
class YggdrasilAuth {
public:
    explicit YggdrasilAuth(QNetworkAccessManager *nam);

    // ── 请求 ──
    QNetworkReply* fetchMeta(const QString &apiRoot);
    QNetworkReply* authenticate(const QString &apiRoot,
                                const QString &username,
                                const QString &password,
                                const QString &clientToken);
    QNetworkReply* refresh(const QString &apiRoot,
                           const QString &accessToken,
                           const QString &clientToken);
    QNetworkReply* validate(const QString &apiRoot,
                            const QString &accessToken);
    QNetworkReply* signout(const QString &apiRoot,
                           const QString &username,
                           const QString &password);
    QNetworkReply* invalidate(const QString &apiRoot,
                              const QString &accessToken,
                              const QString &clientToken);

    // ── 响应解析 ──
    static YggdrasilMeta parseMeta(const QByteArray &data, QString &errorOut);
    static YggdrasilSession parseAuthenticate(const QByteArray &data,
                                               const QString &apiRoot,
                                               const QString &emailInput,
                                               QString &errorOut);
    static bool parseValidate(const QByteArray &data, QString &errorOut);
    static YggdrasilSession parseRefresh(const QByteArray &data,
                                          const QString &apiRoot,
                                          const QString &email,
                                          QString &errorOut);

    // ── 工具 ──
    static QString generateClientToken();

private:
    QNetworkAccessManager *m_nam;

    QNetworkReply* doPost(const QString &url, const QJsonObject &body);
    QNetworkReply* doGet(const QString &url);
};

} // namespace ShadowLauncher
