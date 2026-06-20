// Microsoft OAuth2 Device Code Flow — full implementation
#include "microsoft_auth.h"
#include "http_client.h"
#include "../utils/logger.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QDesktopServices>
#include <QUrl>

namespace ShadowLauncher {

static const char* kDeviceCodeUrl = "https://login.microsoftonline.com/common/oauth2/v2.0/devicecode";
static const char* kTokenUrl = "https://login.microsoftonline.com/common/oauth2/v2.0/token";
static const char* kXblAuthUrl = "https://user.auth.xboxlive.com/user/authenticate";
static const char* kXstsAuthUrl = "https://xsts.auth.xboxlive.com/xsts/authorize";
static const char* kMcAuthUrl = "https://api.minecraftservices.com/authentication/login_with_xbox";
static const char* kMcProfileUrl = "https://api.minecraftservices.com/minecraft/profile";

MicrosoftAuth::MicrosoftAuth(QObject* parent)
    : QObject(parent)
{
    m_pollTimer = new QTimer(this);
    m_pollTimer->setSingleShot(false);
    connect(m_pollTimer, &QTimer::timeout, this, &MicrosoftAuth::pollToken);
}

void MicrosoftAuth::startLogin(const QString& clientId)
{
    if (m_state != State::Idle) {
        emit loginFailed(QStringLiteral("登录已在进行中"));
        return;
    }
    m_clientId = clientId;
    m_pollCount = 0;
    requestDeviceCode(clientId);
}

void MicrosoftAuth::cancelLogin()
{
    m_pollTimer->stop();
    m_state = State::Idle;
    emit loginFailed(QStringLiteral("用户取消登录"));
}

// ═══════════════════════════════════════════════════
// Step 1: Request device code
// ═══════════════════════════════════════════════════
void MicrosoftAuth::requestDeviceCode(const QString& clientId)
{
    emit loginProgress(QStringLiteral("设备码"), QStringLiteral("正在请求登录验证码..."));

    QUrlQuery postData;
    postData.addQueryItem(QStringLiteral("client_id"), clientId);
    postData.addQueryItem(QStringLiteral("scope"), QStringLiteral("XboxLive.signin offline_access"));

    QByteArray body = postData.toString(QUrl::FullyEncoded).toUtf8();

    // We need to send a POST with form data. HttpClient::get is GET only.
    // Use a manual QNetworkAccessManager POST via a helper.
    QNetworkRequest req(QUrl(QString::fromLatin1(kDeviceCodeUrl)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));

    auto* reply = HttpClient::instance().post(req, body);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(logApp) << "[MSA] Device code request failed:" << reply->errorString();
            emit loginFailed(QStringLiteral("无法连接 Microsoft 服务器: %1").arg(reply->errorString()));
            return;
        }
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject obj = doc.object();

        if (obj.contains(QStringLiteral("error"))) {
            QString err = obj[QStringLiteral("error")].toString();
            QString desc = obj[QStringLiteral("error_description")].toString();
            qCWarning(logApp) << "[MSA] Device code error:" << err << desc;
            emit loginFailed(QStringLiteral("Microsoft 认证错误: %1").arg(desc));
            return;
        }

        m_deviceCode = obj[QStringLiteral("device_code")].toString();
        QString userCode = obj[QStringLiteral("user_code")].toString();
        QString verificationUrl = obj[QStringLiteral("verification_uri")].toString();
        m_pollInterval = obj[QStringLiteral("interval")].toInt(5);
        int expiresIn = obj[QStringLiteral("expires_in")].toInt(900);

        qCInfo(logApp) << "[MSA] Device code obtained. user_code=" << userCode
                        << "interval=" << m_pollInterval << "expires=" << expiresIn;

        // Notify QML: show the code to the user
        emit userCodeReady(userCode, verificationUrl);

        // Auto-open browser
        QString otcUrl = verificationUrl + QStringLiteral("?otc=") + userCode;
        QDesktopServices::openUrl(QUrl(otcUrl));
        qCInfo(logApp) << "[MSA] Opening browser:" << otcUrl;

        // Start polling
        m_state = State::PollingDeviceCode;
        m_pollTimer->start(m_pollInterval * 1000);
        emit loginProgress(QStringLiteral("等待验证"),
                           QStringLiteral("请在浏览器中输入验证码 %1").arg(userCode));
    });
}

// ═══════════════════════════════════════════════════
// Step 2/3: Poll for token
// ═══════════════════════════════════════════════════
void MicrosoftAuth::pollToken()
{
    m_pollCount++;
    if (m_pollCount > kMaxPollCount) {
        m_pollTimer->stop();
        m_state = State::Idle;
        emit loginFailed(QStringLiteral("验证超时，请重新登录"));
        return;
    }

    emit loginProgress(QStringLiteral("等待验证"),
                       QStringLiteral("等待你在浏览器中完成登录... (%1/%2)")
                           .arg(m_pollCount).arg(kMaxPollCount));

    QUrlQuery postData;
    postData.addQueryItem(QStringLiteral("grant_type"),
                          QStringLiteral("urn:ietf:params:oauth:grant-type:device_code"));
    postData.addQueryItem(QStringLiteral("client_id"), m_clientId);
    postData.addQueryItem(QStringLiteral("device_code"), m_deviceCode);

    QByteArray body = postData.toString(QUrl::FullyEncoded).toUtf8();

    QNetworkRequest req(QUrl(QString::fromLatin1(kTokenUrl)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));

    auto* reply = HttpClient::instance().post(req, body);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError &&
            reply->error() != QNetworkReply::ContentAccessDenied &&
            reply->error() != QNetworkReply::AuthenticationRequiredError) {
            qCWarning(logApp) << "[MSA] Token poll network error:" << reply->errorString();
            // Keep polling on network errors
            return;
        }
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject obj = doc.object();

        if (obj.contains(QStringLiteral("access_token"))) {
            // Success!
            m_pollTimer->stop();
            m_state = State::Idle;
            m_msAccessToken = obj[QStringLiteral("access_token")].toString();
            m_msRefreshToken = obj[QStringLiteral("refresh_token")].toString();
            int expiresIn = obj[QStringLiteral("expires_in")].toInt();
            qCInfo(logApp) << "[MSA] Token obtained! expires_in=" << expiresIn;

            emit loginProgress(QStringLiteral("Xbox 验证"), QStringLiteral("正在验证 Xbox Live..."));

            // Proceed to Xbox Live auth
            authenticateXbl(m_msAccessToken);
            return;
        }

        QString error = obj[QStringLiteral("error")].toString();
        if (error == QStringLiteral("authorization_pending")) {
            // Normal — user hasn't completed yet, keep polling
            return;
        }
        if (error == QStringLiteral("authorization_declined")) {
            m_pollTimer->stop();
            m_state = State::Idle;
            emit loginFailed(QStringLiteral("用户在浏览器中拒绝了授权"));
            return;
        }
        if (error == QStringLiteral("expired_token")) {
            m_pollTimer->stop();
            m_state = State::Idle;
            emit loginFailed(QStringLiteral("验证码已过期，请重新登录"));
            return;
        }
        if (error == QStringLiteral("bad_verification_code")) {
            m_pollTimer->stop();
            m_state = State::Idle;
            emit loginFailed(QStringLiteral("内部错误：验证码无效"));
            return;
        }

        // Unknown error — stop and report
        m_pollTimer->stop();
        m_state = State::Idle;
        QString desc = obj[QStringLiteral("error_description")].toString();
        qCWarning(logApp) << "[MSA] Token poll error:" << error << desc;
        emit loginFailed(QStringLiteral("登录失败: %1").arg(desc.isEmpty() ? error : desc));
    });
}

// ═══════════════════════════════════════════════════
// Step 4: Xbox Live authenticate
// ═══════════════════════════════════════════════════
void MicrosoftAuth::authenticateXbl(const QString& accessToken)
{
    QJsonObject props;
    props[QStringLiteral("AuthMethod")] = QStringLiteral("RPS");
    props[QStringLiteral("SiteName")] = QStringLiteral("user.auth.xboxlive.com");
    props[QStringLiteral("RpsTicket")] = QStringLiteral("d=%1").arg(accessToken);

    QJsonObject body;
    body[QStringLiteral("Properties")] = props;
    body[QStringLiteral("RelyingParty")] = QStringLiteral("http://auth.xboxlive.com");
    body[QStringLiteral("TokenType")] = QStringLiteral("JWT");

    QNetworkRequest req(QUrl(QString::fromLatin1(kXblAuthUrl)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("Accept", "application/json");

    QByteArray json = QJsonDocument(body).toJson(QJsonDocument::Compact);
    auto* reply = HttpClient::instance().post(req, json);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            QString errBody = reply->readAll();
            qCWarning(logApp) << "[MSA] XBL auth failed:" << reply->errorString()
                             << "body:" << errBody.left(200);
            emit loginFailed(QStringLiteral("Xbox Live 验证失败: %1").arg(reply->errorString()));
            return;
        }
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject obj = doc.object();

        QString xblToken = obj[QStringLiteral("Token")].toString();
        QString uhs = obj[QStringLiteral("DisplayClaims")].toObject()
                        [QStringLiteral("xui")].toArray()[0].toObject()
                        [QStringLiteral("uhs")].toString();

        if (xblToken.isEmpty()) {
            qCWarning(logApp) << "[MSA] XBL auth: no token in response";
            emit loginFailed(QStringLiteral("Xbox Live 返回数据异常"));
            return;
        }

        qCInfo(logApp) << "[MSA] XBL token obtained. uhs=" << uhs;
        emit loginProgress(QStringLiteral("XSTS 验证"), QStringLiteral("正在获取 Minecraft 权限..."));

        authenticateXsts(xblToken);
    });
}

// ═══════════════════════════════════════════════════
// Step 5: XSTS authorize for Minecraft
// ═══════════════════════════════════════════════════
void MicrosoftAuth::authenticateXsts(const QString& xblToken)
{
    QJsonObject props;
    props[QStringLiteral("SandboxId")] = QStringLiteral("RETAIL");
    QJsonArray userTokens;
    userTokens.append(xblToken);
    props[QStringLiteral("UserTokens")] = userTokens;

    QJsonObject body;
    body[QStringLiteral("Properties")] = props;
    body[QStringLiteral("RelyingParty")] = QStringLiteral("rp://api.minecraftservices.com/");
    body[QStringLiteral("TokenType")] = QStringLiteral("JWT");

    QNetworkRequest req(QUrl(QString::fromLatin1(kXstsAuthUrl)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("Accept", "application/json");

    QByteArray json = QJsonDocument(body).toJson(QJsonDocument::Compact);
    auto* reply = HttpClient::instance().post(req, json);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        QByteArray raw = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(raw);
        QJsonObject obj = doc.object();

        if (reply->error() != QNetworkReply::NoError || obj.contains(QStringLiteral("error"))) {
            QString err = obj[QStringLiteral("error")].toString();
            QString errMsg = obj[QStringLiteral("errorMessage")].toString();
            qCWarning(logApp) << "[MSA] XSTS failed:" << reply->errorString()
                             << "XErr:" << err << "msg:" << errMsg;

            // Special: 2148916233 = user doesn't own Minecraft
            if (err == QStringLiteral("2148916233")) {
                emit loginFailed(QStringLiteral("该 Microsoft 账号没有 Minecraft Java 版"));
                return;
            }
            emit loginFailed(QStringLiteral("Minecraft 授权失败: %1").arg(errMsg.isEmpty() ? err : errMsg));
            return;
        }

        QString xstsToken = obj[QStringLiteral("Token")].toString();
        QString uhs = obj[QStringLiteral("DisplayClaims")].toObject()
                        [QStringLiteral("xui")].toArray()[0].toObject()
                        [QStringLiteral("uhs")].toString();

        if (xstsToken.isEmpty()) {
            qCWarning(logApp) << "[MSA] XSTS: no token";
            emit loginFailed(QStringLiteral("XSTS 返回数据异常"));
            return;
        }

        qCInfo(logApp) << "[MSA] XSTS token obtained";
        emit loginProgress(QStringLiteral("Minecraft 认证"),
                           QStringLiteral("正在获取 Minecraft 登录凭证..."));

        authenticateMc(xstsToken, uhs);
    });
}

// ═══════════════════════════════════════════════════
// Step 6: Authenticate with Minecraft
// ═══════════════════════════════════════════════════
void MicrosoftAuth::authenticateMc(const QString& xstsToken, const QString& uhs)
{
    Q_UNUSED(uhs)
    QJsonObject body;
    body[QStringLiteral("identityToken")] = QStringLiteral("XBL3.0 x=%1;%2")
        .arg(uhs, xstsToken);

    QNetworkRequest req(QUrl(QString::fromLatin1(kMcAuthUrl)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("Accept", "application/json");

    QByteArray json = QJsonDocument(body).toJson(QJsonDocument::Compact);
    auto* reply = HttpClient::instance().post(req, json);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        // Note: even on success status might be 200, but we check body
        QByteArray raw = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(raw);
        QJsonObject obj = doc.object();

        if (reply->error() != QNetworkReply::NoError || obj.contains(QStringLiteral("error"))) {
            QString err = obj[QStringLiteral("error")].toString();
            QString errMsg = obj[QStringLiteral("errorMessage")].toString();
            qCWarning(logApp) << "[MSA] MC auth failed:" << reply->errorString()
                             << "err:" << err << "msg:" << errMsg;
            emit loginFailed(QStringLiteral("Minecraft 认证失败: %1")
                                 .arg(errMsg.isEmpty() ? reply->errorString() : errMsg));
            return;
        }

        QString mcAccessToken = obj[QStringLiteral("access_token")].toString();
        if (mcAccessToken.isEmpty()) {
            qCWarning(logApp) << "[MSA] MC auth: no access_token";
            emit loginFailed(QStringLiteral("Minecraft 返回数据异常"));
            return;
        }

        qCInfo(logApp) << "[MSA] MC token obtained!";
        emit loginProgress(QStringLiteral("获取档案"), QStringLiteral("正在获取 Minecraft 档案..."));

        fetchMcProfile(mcAccessToken);
    });
}

// ═══════════════════════════════════════════════════
// Step 7: Get Minecraft profile
// ═══════════════════════════════════════════════════
void MicrosoftAuth::fetchMcProfile(const QString& mcAccessToken)
{
    QNetworkRequest req(QUrl(QString::fromLatin1(kMcProfileUrl)));
    req.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(mcAccessToken).toUtf8());

    auto* reply = HttpClient::instance().getRaw(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply, mcAccessToken]() {
        reply->deleteLater();
        QByteArray raw = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(raw);
        QJsonObject obj = doc.object();

        if (reply->error() != QNetworkReply::NoError || obj.contains(QStringLiteral("error"))) {
            QString err = obj[QStringLiteral("error")].toString();
            QString errMsg = obj[QStringLiteral("errorMessage")].toString();
            qCWarning(logApp) << "[MSA] Profile fetch failed:" << reply->errorString()
                             << "err:" << err << "msg:" << errMsg;
            emit loginFailed(QStringLiteral("获取档案失败: %1")
                                 .arg(errMsg.isEmpty() ? reply->errorString() : errMsg));
            return;
        }

        QString uuid = obj[QStringLiteral("id")].toString();
        QString username = obj[QStringLiteral("name")].toString();

        if (uuid.isEmpty() || username.isEmpty()) {
            qCWarning(logApp) << "[MSA] Profile: empty uuid/name";
            emit loginFailed(QStringLiteral("该账号未创建 Minecraft 档案"));
            return;
        }

        qCInfo(logApp) << "[MSA] Login SUCCESS! username=" << username << "uuid=" << uuid;
        emit loginSuccess(mcAccessToken, username, uuid, m_msRefreshToken);
    });
}

// ═══════════════════════════════════════════════════
// Token refresh
// ═══════════════════════════════════════════════════
void MicrosoftAuth::refreshToken(const QString& clientId, const QString& refreshToken)
{
    if (m_state != State::Idle) {
        emit loginFailed(QStringLiteral("操作已在进行中"));
        return;
    }
    m_clientId = clientId;

    emit loginProgress(QStringLiteral("刷新令牌"), QStringLiteral("正在静默刷新登录..."));

    QUrlQuery postData;
    postData.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
    postData.addQueryItem(QStringLiteral("client_id"), clientId);
    postData.addQueryItem(QStringLiteral("refresh_token"), refreshToken);

    QByteArray body = postData.toString(QUrl::FullyEncoded).toUtf8();

    QNetworkRequest req(QUrl(QString::fromLatin1(kTokenUrl)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));

    auto* reply = HttpClient::instance().post(req, body);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject obj = doc.object();

        if (obj.contains(QStringLiteral("access_token"))) {
            m_msAccessToken = obj[QStringLiteral("access_token")].toString();
            m_msRefreshToken = obj[QStringLiteral("refresh_token")].toString();
            qCInfo(logApp) << "[MSA] Token refreshed!";

            emit loginProgress(QStringLiteral("Xbox 验证"), QStringLiteral("刷新完成，正在验证..."));
            authenticateXbl(m_msAccessToken);
            return;
        }

        QString error = obj[QStringLiteral("error")].toString();
        QString desc = obj[QStringLiteral("error_description")].toString();
        qCWarning(logApp) << "[MSA] Token refresh failed:" << error << desc;
        emit loginFailed(QStringLiteral("令牌刷新失败，请重新登录"));
    });
}

} // namespace ShadowLauncher
