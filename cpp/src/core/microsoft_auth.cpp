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
#include <QTcpServer>
#include <QTcpSocket>
#include <QRandomGenerator>
#include <QTimer>
#include <QEventLoop>
#include <QNetworkAccessManager>

#ifdef Q_OS_WIN
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

static QByteArray httpPostSync(const QString& url, const QByteArray& body, int* outStatus)
{
    QByteArray result;
    *outStatus = 0;

    QUrl qurl(url);
    QString host = qurl.host();
    int port = qurl.port(443);
    bool https = qurl.scheme() == QStringLiteral("https");
    QString path = qurl.path();
    if (qurl.hasQuery()) path += QLatin1Char('?') + qurl.query();

    HINTERNET hSession = WinHttpOpen(L"ShadowLauncher/1.0",
                                      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME,
                                      WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return result;

    HINTERNET hConnect = WinHttpConnect(hSession,
                                         reinterpret_cast<LPCWSTR>(host.utf16()),
                                         port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return result; }

    LPCWSTR types[2] = { L"text/*", nullptr };
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST",
                                             reinterpret_cast<LPCWSTR>(path.utf16()),
                                             nullptr, WINHTTP_NO_REFERER,
                                             types,
                                             https ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return result; }

    LPCWSTR headers = L"Content-Type: application/x-www-form-urlencoded\r\n";
    if (!WinHttpSendRequest(hRequest, headers, static_cast<DWORD>(wcslen(headers)),
                            const_cast<char*>(body.data()), static_cast<DWORD>(body.size()),
                            static_cast<DWORD>(body.size()), 0)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return result;
    }

    if (WinHttpReceiveResponse(hRequest, nullptr)) {
        DWORD dwSize = sizeof(DWORD);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, outStatus, &dwSize, WINHTTP_NO_HEADER_INDEX);

        DWORD bytesRead = 0;
        QByteArray buf(4096, Qt::Uninitialized);
        while (WinHttpReadData(hRequest, buf.data(), static_cast<DWORD>(buf.size()), &bytesRead) && bytesRead > 0) {
            result.append(buf.constData(), static_cast<int>(bytesRead));
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
}
#endif

namespace ShadowLauncher {

static const char* kAuthUrl = "https://login.live.com/oauth20_authorize.srf";
static const char* kTokenUrl = "https://login.live.com/oauth20_token.srf";
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

    // Pick a local port and start HTTP server to capture the OAuth callback
    int port = 58000 + (QRandomGenerator::global()->bounded(2000)); // 58000-59999
    m_localServer = new QTcpServer(this);

    connect(m_localServer, &QTcpServer::newConnection, this, [this]() {
        while (m_localServer->hasPendingConnections()) {
            auto* sock = m_localServer->nextPendingConnection();
            sock->waitForReadyRead(2000);
            QByteArray data = sock->readAll();
            sock->close();
            sock->deleteLater();

            qCInfo(logApp) << "[MSA] Received callback:" << data.left(300);

            // Extract GET /?code=*** HTTP/1.1
            int codeIdx = data.indexOf(QByteArrayLiteral("code="));
            if (codeIdx < 0) {
                qCWarning(logApp) << "[MSA] No code in callback";
                m_localServer->close();
                m_localServer->deleteLater();
                m_localServer = nullptr;
                m_busy = false;
                emit loginFailed(QStringLiteral("浏览器回调中未找到验证码"));
                return;
            }

            // Extract code value
            int valStart = codeIdx + 5;
            int valEnd = data.indexOf(' ', valStart);
            if (valEnd < 0) valEnd = data.indexOf('&', valStart);
            if (valEnd < 0) valEnd = data.size();
            QString code = QString::fromUtf8(data.mid(valStart, valEnd - valStart));

            qCInfo(logApp) << "[MSA] Extracted code:" << code;

            m_localServer->close();
            m_localServer->deleteLater();
            m_localServer = nullptr;

            emit loginProgress(QStringLiteral("验证中"), QStringLiteral("正在换取访问令牌..."));
            exchangeCode(code);  // sync call — WinHTTP doesn't need event loop
        }
    });

    if (!m_localServer->listen(QHostAddress::LocalHost, port)) {
        qCWarning(logApp) << "[MSA] Local server failed to listen on port" << port;
        m_busy = false;
        emit loginFailed(QStringLiteral("本地回调服务启动失败"));
        return;
    }

    QString redirectUri = QStringLiteral("http://localhost:%1").arg(port);
    m_localRedirectUri = redirectUri;
    qCInfo(logApp) << "[MSA] Local server listening on" << redirectUri;

    // Build the authorization URL with localhost redirect
    QString url = QStringLiteral("%1"
        "?client_id=%2"
        "&response_type=code"
        "&redirect_uri=%3"
        "&scope=XboxLive.signin%20offline_access")
        .arg(QString::fromLatin1(kAuthUrl),
             clientId,
             redirectUri);

    qCInfo(logApp) << "[MSA] Auth URL:" << url;
    emit loginProgress(QStringLiteral("浏览器登录"), QStringLiteral("请在浏览器中登录 Microsoft 账号"));
    emit authUrlReady(url);

    QDesktopServices::openUrl(QUrl(url));
}

void MicrosoftAuth::cancelLogin()
{
    if (m_localServer) {
        m_localServer->close();
        m_localServer->deleteLater();
        m_localServer = nullptr;
    }
    m_busy = false;
    emit loginFailed(QStringLiteral("用户取消登录"));
}

void MicrosoftAuth::submitCode(const QString&)
{
    // No longer needed — code captured by local server
}

// ═══════════════════════════════════════════════════
// Exchange code for token
// ═══════════════════════════════════════════════════
void MicrosoftAuth::exchangeCode(const QString& code)
{
    qCInfo(logApp) << "[MSA] exchangeCode starting, redirectUri:" << m_localRedirectUri;
    QString redirectUri = m_localRedirectUri;

    QUrlQuery postData;
    postData.addQueryItem(QStringLiteral("client_id"), m_clientId);
    postData.addQueryItem(QStringLiteral("code"), code);
    postData.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("authorization_code"));
    postData.addQueryItem(QStringLiteral("redirect_uri"), redirectUri);

    QByteArray body = postData.toString(QUrl::FullyEncoded).toUtf8();
    qCInfo(logApp) << "[MSA] Token POST body len:" << body.size();

    // Use WinHTTP to avoid Qt network stack crash
    QByteArray raw;
    int status = 0;

    // Try WinHTTP first, fallback to Qt
#ifdef Q_OS_WIN
    raw = httpPostSync(QString::fromLatin1(kTokenUrl), body, &status);
#endif

    if (raw.isEmpty()) {
        // Fallback: use local event loop with plain QNetworkAccessManager
        qCInfo(logApp) << "[MSA] WinHTTP failed, trying Qt fallback...";
        QNetworkAccessManager mgr;
        QNetworkRequest req(QUrl(QString::fromLatin1(kTokenUrl)));
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
        req.setTransferTimeout(15000);
        QNetworkReply* reply = mgr.post(req, body);
        QEventLoop loop;
        QTimer::singleShot(15000, &loop, &QEventLoop::quit);
        if (reply) {
            QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            loop.exec();
            raw = reply->readAll();
            status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            reply->deleteLater();
        }
    }

    qCInfo(logApp) << "[MSA] Token response HTTP" << status << "body:" << raw.left(500);

    QJsonDocument doc = QJsonDocument::fromJson(raw);
    QJsonObject obj = doc.object();

    if (status < 200 || status >= 300 || !obj[QStringLiteral("access_token")].isString()) {
        QString err = obj[QStringLiteral("error")].toString(QString::fromLatin1("unknown"));
        QString desc = obj[QStringLiteral("error_description")].toString(QString::fromLatin1(raw).left(200));
        qCWarning(logApp) << "[MSA] Token exchange FAILED:" << err << desc;
        m_busy = false;
        emit loginFailed(QStringLiteral("令牌交换失败: %1").arg(desc.isEmpty() ? err : desc));
        return;
    }

    m_msAccessToken = obj[QStringLiteral("access_token")].toString();
    m_msRefreshToken = obj[QStringLiteral("refresh_token")].toString();

    if (m_msAccessToken.isEmpty()) {
        qCWarning(logApp) << "[MSA] No access_token in response:" << raw.left(500);
        m_busy = false;
        emit loginFailed(QStringLiteral("未获取到访问令牌"));
        return;
    }

    qCInfo(logApp) << "[MSA] Token obtained!";
    emit loginProgress(QStringLiteral("Xbox 验证"), QStringLiteral("正在验证 Xbox Live..."));
    authenticateXbl(m_msAccessToken);
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
