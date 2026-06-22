#pragma once

// ── Microsoft OAuth2 Authorization Code Flow for Minecraft ──
// Step 1: startLogin() → local HTTP server on 127.0.0.1:PORT → browser
// Step 2: browser callback → _internalCodeReady (QueuedConnection) → exchangeCode (WinHTTP)
// Step 3-7: XBL → XSTS → MC auth → Profile (async via HttpClient)

#include <QObject>
#include <QString>

class QTcpServer;

namespace ShadowLauncher {

class MicrosoftAuth : public QObject {
    Q_OBJECT
public:
    explicit MicrosoftAuth(QObject* parent = nullptr);

    Q_INVOKABLE void startLogin(const QString& clientId);
    Q_INVOKABLE void cancelLogin();
    Q_INVOKABLE void refreshMcChain(const QString& msAccessToken);
    bool isBusy() const { return m_busy; }

signals:
    void loginProgress(const QString& step, const QString& detail);
    void loginSuccess(const QString& minecraftToken, const QString& username,
                      const QString& uuid, const QString& refreshToken);
    void loginFailed(const QString& error);

    // Internal — progress step signals (used with QueuedConnection)
    void _internalCodeReady(const QString& code, const QString& redirectUri);
    void _internalTokenReady(const QString& accessToken);

private:
    bool m_busy = false;
    bool m_pendingCodeReady = false;
    QString m_clientId;
    QString m_localRedirectUri;
    QString m_msAccessToken;
    QString m_msRefreshToken;
    QString m_msMcToken;
    QTcpServer* m_localServer = nullptr;

    Q_INVOKABLE void exchangeCode(const QString& code, const QString& redirectUri);
    void authenticateXbl(const QString& accessToken);
    void authenticateXsts(const QString& xblToken);
    void authenticateMc(const QString& xstsToken, const QString& uhs);
    void fetchMcProfile(const QString& mcAccessToken);
};

} // namespace ShadowLauncher
