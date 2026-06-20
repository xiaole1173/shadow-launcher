#pragma once

// ── Microsoft OAuth2 Authorization Code Flow for Minecraft ──
// Steps:
//   1. Open browser → login.live.com/oauth20_authorize.srf
//   2. User signs in → browser redirects with ?code=***
//   3. User pastes code/URL → we exchange code for token
//   4. Xbox Live auth → XSTS → Minecraft token → Profile

#include <QObject>
#include <QString>

namespace ShadowLauncher {

class MicrosoftAuth : public QObject {
    Q_OBJECT
public:
    explicit MicrosoftAuth(QObject* parent = nullptr);

    // Start login — build auth URL and open browser
    Q_INVOKABLE void startLogin(const QString& clientId);

    // User submitted the auth code (or full redirect URL containing ?code=...)
    Q_INVOKABLE void submitCode(const QString& codeOrUrl);

    // Cancel
    Q_INVOKABLE void cancelLogin();

    bool isBusy() const { return m_busy; }

signals:
    void loginProgress(const QString& step, const QString& detail);
    void authUrlReady(const QString& url);  // QML opens this in browser
    void loginSuccess(const QString& minecraftToken, const QString& username,
                      const QString& uuid, const QString& refreshToken);
    void loginFailed(const QString& error);

private:
    bool m_busy = false;
    QString m_clientId;
    QString m_msAccessToken;
    QString m_msRefreshToken;
    QString m_msMcToken;

    void exchangeCode(const QString& code);
    void authenticateXbl(const QString& accessToken);
    void authenticateXsts(const QString& xblToken);
    void authenticateMc(const QString& xstsToken, const QString& uhs);
    void fetchMcProfile(const QString& mcAccessToken);
};

} // namespace ShadowLauncher
