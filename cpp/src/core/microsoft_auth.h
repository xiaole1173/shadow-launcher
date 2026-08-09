// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

// ── Microsoft OAuth2 Authorization Code Flow for Minecraft ──
// Uses Microsoft Identity Platform v2 endpoint (consumers tenant)
//
// Mode A: localhost callback — system browser → localhost → code extraction
// Mode B: embedded login — standalone wv2login.exe (Edge WebView2 in a
//         separate process; never runs WebView2 inside this process, so
//         closing the login window can never crash the launcher)

#include <QObject>
#include <QString>
#include <QPointer>

class QTcpServer;
class QProcess;

namespace ShadowLauncher {

class MicrosoftAuth : public QObject {
    Q_OBJECT
public:
    explicit MicrosoftAuth(QObject* parent = nullptr);
    ~MicrosoftAuth();

    Q_INVOKABLE void startLogin(const QString& clientId);
    Q_INVOKABLE void startEmbeddedLogin(const QString& clientId);
    Q_INVOKABLE void cancelLogin();
    Q_INVOKABLE void refreshMcChain(const QString& msAccessToken);
    bool isBusy() const { return m_busy; }

signals:
    void loginProgress(const QString& step, const QString& detail);
    void loginSuccess(const QString& minecraftToken, const QString& username,
                      const QString& uuid, const QString& refreshToken, int expiresIn = 86400);
    void loginFailed(const QString& error);

    // Internal — progress step signals (used with QueuedConnection)
    void _internalCodeReady(const QString& code, const QString& redirectUri);
    void _internalTokenReady(const QString& accessToken);

private:
    bool m_busy = false;
    QString m_clientId;
    QString m_localRedirectUri;
    QString m_msAccessToken;
    QString m_msRefreshToken;
    QString m_msMcToken;
    int m_msTokenExpiresIn = 86400;
    QTcpServer* m_localServer = nullptr;
    QProcess* m_wv2Process = nullptr;
    bool m_embeddedMode = false;
    bool m_codeCaptured = false;

    void startWv2LoginProcess(const QString& clientId);
    Q_INVOKABLE void exchangeCode(const QString& code, const QString& redirectUri);
    void authenticateXbl(const QString& accessToken);
    void authenticateXsts(const QString& xblToken);
    void authenticateMc(const QString& xstsToken, const QString& uhs);
    void fetchMcProfile(const QString& mcAccessToken);
};

} // namespace ShadowLauncher
