// Microsoft OAuth2 Authorization Code Flow for Minecraft
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

static const char* kAuthUrl = "https://login.live.com/oauth20_authorize.srf";
static const char* kTokenUrl = "https://login.live.com/oauth20_token.srf";
static const char* kRedirectUri = "https://login.live.com/oauth20_desktop.srf";
static const char* kXblAuthUrl = "https://user.auth.xboxlive.com/user/authenticate";
static const char* kXstsAuthUrl = "https://xsts.auth.xboxlive.com/xsts/authorize";
static const char* kMcAuthUrl = "https://api.minecraftservices.com/authentication/login_with_xbox";
static const char* kMcProfileUrl = "https://api.minecraftservices.com/minecraft/profile";

MicrosoftAuth::MicrosoftAuth(QObject* parent) : QObject(parent) {}

void MicrosoftAuth::startLogin(const QString& clientId)
{
    if (m_busy) { emit loginFailed(QStringLiteral("登录已在进行中")); return; }
    m_clientId = clientId;
    m_busy = true;

    // Build the authorization URL
    QString url = QStringLiteral("%1"
        "?client_id=%2"
        "&response_type=code"
        "&redirect_uri=%3"
        "&scope=XboxLive.signin%20offline_access")
        .arg(QString::fromLatin1(kAuthUrl),
             clientId,
             QString::fromLatin1(kRedirectUri));

    qCInfo(logApp) << "[MSA] Auth URL:" << url;
    emit loginProgress(QStringLiteral("浏览器登录"), QStringLiteral("请在浏览器中登录 Microsoft 账号"));
    emit authUrlReady(url);

    // Auto-open browser
    QDesktopServices::openUrl(QUrl(url));
}

void MicrosoftAuth::cancelLogin()
{
    m_busy = false;
    emit loginFailed(QStringLiteral("用户取消登录"));
}

void MicrosoftAuth::submitCode(const QString& codeOrUrl)
{
    if (!m_busy) {
        emit loginFailed(QStringLiteral("没有进行中的登录"));
        return;
    }

    // Extract code from URL or use as-is
    QString code = codeOrUrl;
    if (codeOrUrl.contains(QStringLiteral("?code="))) {
        QUrl url(codeOrUrl);
        QUrlQuery query(url);
        code = query.queryItemValue(QStringLiteral("code"));
    } else if (codeOrUrl.contains(QStringLiteral("&code="))) {
        // Handle fragments: extract ?code= or &code=
        int idx = codeOrUrl.indexOf(QStringLiteral("code="));
        int end = codeOrUrl.indexOf(QLatin1Char('&'), idx + 5);
        code = (end > 0) ? codeOrUrl.mid(idx + 5, end - idx - 5)
                         : codeOrUrl.mid(idx + 5);
    }

    if (code.isEmpty()) {
        emit loginFailed(QStringLiteral("未能从输入中提取验证码，请复制浏览器地址栏完整 URL"));
        return;
    }

    qCInfo(logApp) << "[MSA] Got auth code, exchanging...";
    emit loginProgress(QStringLiteral("验证中"), QStringLiteral("正在换取访问令牌..."));
    exchangeCode(code);
}

// ═══════════════════════════════════════════════════
// Exchange code for token
// ═══════════════════════════════════════════════════
void MicrosoftAuth::exchangeCode(const QString& code)
{
    QUrlQuery postData;
    postData.addQueryItem(QStringLiteral("client_id"), m_clientId);
    postData.addQueryItem(QStringLiteral("code"), code);
    postData.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("authorization_code"));
    postData.addQueryItem(QStringLiteral("redirect_uri"), QString::fromLatin1(kRedirectUri));

    QByteArray body = postData.toString(QUrl::FullyEncoded).toUtf8();

    QNetworkRequest req(QUrl(QString::fromLatin1(kTokenUrl)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));

    auto* reply = HttpClient::instance().post(req, body);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject obj = doc.object();

        if (reply->error() != QNetworkReply::NoError || obj.contains(QStringLiteral("error"))) {
            QString err = obj[QStringLiteral("error")].toString();
            QString desc = obj[QStringLiteral("error_description")].toString();
            qCWarning(logApp) << "[MSA] Token exchange failed:" << reply->errorString()
                             << "body:" << data.left(500);
            m_busy = false;
            emit loginFailed(QStringLiteral("令牌交换失败: %1").arg(desc.isEmpty() ? err : desc));
            return;
        }

        m_msAccessToken = obj[QStringLiteral("access_token")].toString();
        m_msRefreshToken = obj[QStringLiteral("refresh_token")].toString();

        if (m_msAccessToken.isEmpty()) {
            qCWarning(logApp) << "[MSA] No access_token in response:" << data.left(500);
            m_busy = false;
            emit loginFailed(QStringLiteral("未获取到访问令牌"));
            return;
        }

        qCInfo(logApp) << "[MSA] Token obtained!";
        emit loginProgress(QStringLiteral("Xbox 验证"), QStringLiteral("正在验证 Xbox Live..."));
        authenticateXbl(m_msAccessToken);
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
        QByteArray raw = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(raw);
        QJsonObject obj = doc.object();

        if (reply->error() != QNetworkReply::NoError || !obj[QStringLiteral("Token")].isString()) {
            qCWarning(logApp) << "[MSA] XBL auth failed:" << reply->errorString() << raw.left(200);
            m_busy = false;
            emit loginFailed(QStringLiteral("Xbox Live 验证失败"));
            return;
        }

        QString xblToken = obj[QStringLiteral("Token")].toString();
        qCInfo(logApp) << "[MSA] XBL token obtained";
        emit loginProgress(QStringLiteral("XSTS"), QStringLiteral("正在获取 Minecraft 权限..."));
        authenticateXsts(xblToken);
    });
}

// ═══════════════════════════════════════════════════
// Step 5: XSTS
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

        if (reply->error() != QNetworkReply::NoError || obj.contains(QStringLiteral("XErr"))) {
            int xerr = obj[QStringLiteral("XErr")].toInt();
            qCWarning(logApp) << "[MSA] XSTS failed XErr:" << xerr;
            m_busy = false;
            if (xerr == 2148916233) {
                emit loginFailed(QStringLiteral("该账号没有 Minecraft Java 版"));
            } else {
                emit loginFailed(QStringLiteral("Minecraft 授权失败 (XErr=%1)").arg(xerr));
            }
            return;
        }

        QString xstsToken = obj[QStringLiteral("Token")].toString();
        QString uhs = obj[QStringLiteral("DisplayClaims")].toObject()
                        [QStringLiteral("xui")].toArray()[0].toObject()
                        [QStringLiteral("uhs")].toString();

        qCInfo(logApp) << "[MSA] XSTS token obtained";
        emit loginProgress(QStringLiteral("Minecraft"), QStringLiteral("正在登录 Minecraft..."));
        authenticateMc(xstsToken, uhs);
    });
}

// ═══════════════════════════════════════════════════
// Step 6: Minecraft auth
// ═══════════════════════════════════════════════════
void MicrosoftAuth::authenticateMc(const QString& xstsToken, const QString& uhs)
{
    QJsonObject body;
    body[QStringLiteral("identityToken")] = QStringLiteral("XBL3.0 x=%1;%2").arg(uhs, xstsToken);

    QNetworkRequest req(QUrl(QString::fromLatin1(kMcAuthUrl)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("Accept", "application/json");

    QByteArray json = QJsonDocument(body).toJson(QJsonDocument::Compact);
    auto* reply = HttpClient::instance().post(req, json);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        QByteArray raw = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(raw);
        QJsonObject obj = doc.object();

        if (reply->error() != QNetworkReply::NoError || !obj[QStringLiteral("access_token")].isString()) {
            qCWarning(logApp) << "[MSA] MC auth failed:" << reply->errorString() << raw.left(200);
            m_busy = false;
            emit loginFailed(QStringLiteral("Minecraft 认证失败"));
            return;
        }

        QString mcToken = obj[QStringLiteral("access_token")].toString();
        qCInfo(logApp) << "[MSA] MC token obtained!";
        emit loginProgress(QStringLiteral("档案"), QStringLiteral("正在获取 Minecraft 档案..."));
        fetchMcProfile(mcToken);
    });
}

// ═══════════════════════════════════════════════════
// Step 7: Profile
// ═══════════════════════════════════════════════════
void MicrosoftAuth::fetchMcProfile(const QString& mcToken)
{
    m_msMcToken = mcToken;
    QNetworkRequest req(QUrl(QString::fromLatin1(kMcProfileUrl)));
    req.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(mcToken).toUtf8());

    auto* reply = HttpClient::instance().getRaw(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        QByteArray raw = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(raw);
        QJsonObject obj = doc.object();
        m_busy = false;

        if (reply->error() != QNetworkReply::NoError || !obj[QStringLiteral("id")].isString()) {
            qCWarning(logApp) << "[MSA] Profile failed:" << reply->errorString() << raw.left(200);
            emit loginFailed(QStringLiteral("获取档案失败"));
            return;
        }

        QString uuid = obj[QStringLiteral("id")].toString();
        QString username = obj[QStringLiteral("name")].toString();

        qCInfo(logApp) << "[MSA] Login SUCCESS!" << username << uuid;
        emit loginSuccess(m_msMcToken, username, uuid, m_msRefreshToken);
    });
}

} // namespace ShadowLauncher
