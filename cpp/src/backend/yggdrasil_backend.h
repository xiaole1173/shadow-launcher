// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QJsonObject>
#include <QNetworkAccessManager>

#include "core/yggdrasil_auth.h"

namespace ShadowLauncher {

/// Yggdrasil 外置登录后端 — 给 QML 消费的 QObject 封装
class YggdrasilBackend : public QObject {
    Q_OBJECT

    // ══ QML 可读属性 ══
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY stateChanged)
    Q_PROPERTY(QString apiRoot READ apiRoot NOTIFY stateChanged)
    Q_PROPERTY(QString email READ email NOTIFY stateChanged)
    Q_PROPERTY(QString username READ username NOTIFY stateChanged)
    Q_PROPERTY(QString uuid READ uuid NOTIFY stateChanged)
    Q_PROPERTY(QString accessToken READ accessToken NOTIFY stateChanged)
    Q_PROPERTY(QString clientToken READ clientToken NOTIFY stateChanged)
    Q_PROPERTY(QString metaServerName READ metaServerName NOTIFY metaReady)
    Q_PROPERTY(QString registerUrl READ registerUrl NOTIFY metaReady)
    Q_PROPERTY(QVariantList profiles READ profiles NOTIFY profilesChanged)
    Q_PROPERTY(int profileIndex READ profileIndex NOTIFY profilesChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(bool hadSavedSession READ hadSavedSession CONSTANT)
    Q_PROPERTY(QString serverAddress READ serverAddress WRITE setServerAddress NOTIFY serverAddressChanged)
    Q_PROPERTY(bool serverAddressValid READ serverAddressValid NOTIFY serverAddressChanged)
    Q_PROPERTY(QString serverName READ serverName WRITE setServerName NOTIFY serverNameChanged)
    Q_PROPERTY(bool autoJoinServer READ autoJoinServer WRITE setAutoJoinServer NOTIFY autoJoinServerChanged)

public:
    explicit YggdrasilBackend(QObject *parent = nullptr);

    // ── Getters ──
    bool loggedIn() const { return m_session.isValid(); }
    QString apiRoot() const { return m_session.apiRoot; }
    QString email() const { return m_session.email; }
    QString username() const { return m_session.username; }
    QString uuid() const { return m_session.uuid; }
    QString accessToken() const { return m_session.accessToken; }
    QString clientToken() const { return m_session.clientToken; }
    QString metaServerName() const { return m_meta.serverName; }
    QString registerUrl() const { return m_meta.registerUrl; }
    QVariantList profiles() const;
    int profileIndex() const { return m_session.selectedProfileIndex; }
    QString statusMessage() const { return m_statusMessage; }

    Q_INVOKABLE void fetchMeta(const QString &apiRoot);
    Q_INVOKABLE void login(const QString &apiRoot, const QString &email, const QString &password);
    Q_INVOKABLE void refreshToken();
    Q_INVOKABLE void logout();
    Q_INVOKABLE void selectProfile(int index);

    // 持久化 session
    Q_INVOKABLE void saveSession();
    Q_INVOKABLE void loadSession();
    Q_INVOKABLE void deleteSavedSession();
    bool hadSavedSession() const { return m_hadSavedSession; }
    QString sessionFilePath() const;

    // ── 服务器设置 ──
    static bool isValidServerAddress(const QString &addr);
    QString serverAddress() const;
    void setServerAddress(const QString &addr);
    bool serverAddressValid() const { return isValidServerAddress(serverAddress()); }
    QString serverName() const;
    void setServerName(const QString &name);
    bool autoJoinServer() const;
    void setAutoJoinServer(bool enabled);

signals:
    void metaReady();
    void metaFailed(const QString &error);
    void loginSuccess();
    void loginFailed(const QString &error);
    void stateChanged();
    void profilesChanged();
    void statusMessageChanged();
    void serverAddressChanged();
    void serverNameChanged();
    void autoJoinServerChanged();

private slots:
    void onMetaReply();
    void onAuthenticateReply();
    void onRefreshReply();
    void onLogoutReply();

private:
    void setStatus(const QString &msg);

    QNetworkAccessManager *m_nam = nullptr;
    YggdrasilAuth m_auth;
    YggdrasilSession m_session;
    YggdrasilMeta m_meta;
    QString m_statusMessage;
    QString m_pendingApiRoot;   // 登录过程中记录的 API Root
    QString m_pendingEmail;     // 登录过程中记录邮箱
    QString m_pendingPassword;  // 登录过程中记录密码（仅用于 signout）
    bool m_loggingOut = false;
    bool m_hadSavedSession = false;  // session.json 是否存在过
};

} // namespace ShadowLauncher
