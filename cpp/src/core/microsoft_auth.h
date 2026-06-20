#pragma once

// ── Microsoft OAuth2 Device Code Flow for Minecraft ──
// Steps:
//   1. Request device code
//   2. User opens browser → enters code → signs in
//   3. Poll for token (every 5s until user completes or expires)
//   4. Xbox Live auth → XSTS → Minecraft token → Profile

#include <QObject>
#include <QString>
#include <QTimer>
#include <QJsonObject>

namespace ShadowLauncher {

class MicrosoftAuth : public QObject {
    Q_OBJECT
public:
    explicit MicrosoftAuth(QObject* parent = nullptr);

    // Start login flow (clientId is the Azure app ID)
    Q_INVOKABLE void startLogin(const QString& clientId);

    // Cancel ongoing login
    Q_INVOKABLE void cancelLogin();

    // Refresh token (silent, using stored refresh_token)
    Q_INVOKABLE void refreshToken(const QString& clientId, const QString& refreshToken);

    // Whether a login flow is in progress
    Q_INVOKABLE bool isBusy() const { return m_state != State::Idle; }

signals:
    // Step progress (for QML status display)
    void loginProgress(const QString& step, const QString& detail);

    // User needs to open browser + enter code
    void userCodeReady(const QString& userCode, const QString& verificationUrl);

    // Login complete
    void loginSuccess(const QString& minecraftToken, const QString& username,
                      const QString& uuid, const QString& refreshToken);

    // Login failed
    void loginFailed(const QString& error);

private slots:
    void pollToken();
    void onStep3TokenResponse(int status, const QByteArray& body);
    void onXblAuthResponse(int status, const QByteArray& body);
    void onXstsAuthResponse(int status, const QByteArray& body);
    void onMcAuthResponse(int status, const QByteArray& body);
    void onMcProfileResponse(int status, const QByteArray& body);

private:
    enum class State { Idle, PollingDeviceCode };
    State m_state = State::Idle;

    QString m_clientId;
    QString m_deviceCode;
    QString m_msAccessToken;
    QString m_msRefreshToken;
    int m_pollInterval = 5;  // seconds
    int m_pollCount = 0;
    static const int kMaxPollCount = 24;  // 2 minutes max

    QTimer* m_pollTimer = nullptr;

    void requestDeviceCode(const QString& clientId);
    void authenticateXbl(const QString& accessToken);
    void authenticateXsts(const QString& xblToken);
    void authenticateMc(const QString& xstsToken, const QString& uhs);
    void fetchMcProfile(const QString& mcAccessToken);
};

} // namespace ShadowLauncher
