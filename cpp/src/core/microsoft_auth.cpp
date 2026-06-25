#include "microsoft_auth.h"
#include "http_client.h"
#include "../utils/logger.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QUrl>
#include <QUrlQuery>
#include <QWebEngineView>
#include <QTimer>
#include <QWindow>
#include <QWidget>
#include <QDebug>

namespace ShadowLauncher {

using namespace Qt::StringLiterals;

// ── Constants ──

static const char* kAuthUrl = "https://login.microsoftonline.com/consumers/oauth2/v2.0/authorize";
static const char* kTokenUrl = "https://login.microsoftonline.com/consumers/oauth2/v2.0/token";
static const char* kNativeRedirectUri = "https://login.microsoftonline.com/common/oauth2/nativeclient";

static const QString kClientId = u"1167b841-0421-4bfa-9ca2-3ab67e136f9f"_s;

static QString createState() {
    QByteArray bytes;
    bytes.resize(32);
    for (int i = 0; i < 32; ++i) bytes[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    return QString::fromLatin1(bytes.toHex());
}

// ── Constructor / Destructor ──

MicrosoftAuth::MicrosoftAuth(QObject* parent) : QObject(parent) {}
MicrosoftAuth::~MicrosoftAuth() { cancelLogin(); }

// ── cancelLogin ──

void MicrosoftAuth::cancelLogin() {
    if (m_localServer) { m_localServer->close(); m_localServer->deleteLater(); m_localServer = nullptr; }
    if (m_embeddedWindow) { m_embeddedWindow->close(); m_embeddedWindow->deleteLater(); }
    m_embeddedMode = false;
    m_busy = false;
    m_codeCaptured = false;
}

// ═══════════════════════════════════════════════════
// Localhost callback login (Mode A — default)
// ═══════════════════════════════════════════════════

void MicrosoftAuth::startLogin(const QString& clientId) {
    if (m_busy) return;
    m_busy = true;
    m_embeddedMode = false;
    m_clientId = clientId.isEmpty() ? kClientId : clientId;

    // Create local HTTP server
    m_localServer = new QTcpServer(this);
    int port = 0;
    for (int p : {49152, 49153, 49154, 49155, 49156}) {
        if (m_localServer->listen(QHostAddress::LocalHost, p)) { port = p; break; }
    }
    if (port == 0) {
        emit loginFailed(tr("无法启动本地回调服务器"));
        m_busy = false;
        return;
    }
    m_localRedirectUri = QStringLiteral("http://localhost:%1").arg(port);

    connect(m_localServer, &QTcpServer::newConnection, this, [this]() {
        auto* socket = m_localServer->nextPendingConnection();
        socket->waitForReadyRead(500);
        QByteArray data = socket->readAll();
        QString request = QString::fromUtf8(data);
        socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n"
            "<html><body><h2>登录成功</h2><p>请返回启动器</p></body></html>");
        socket->close();
        socket->deleteLater();
        m_localServer->close();

        int codeIdx = request.indexOf("code=");
        if (codeIdx == -1) { emit loginFailed(tr("浏览器回调中未找到验证码")); m_busy = false; return; }
        QString code = request.mid(codeIdx + 5);
        int ampIdx = code.indexOf('&');
        if (ampIdx != -1) code = code.left(ampIdx);
        int spaceIdx = code.indexOf(' ');
        if (spaceIdx != -1) code = code.left(spaceIdx);
        code = QUrl::fromPercentEncoding(code.toUtf8());
        emit loginProgress(tr("获取授权码"), code.left(15) + u"..."_s);
        exchangeCode(code, m_localRedirectUri);
    });

    QString state = createState();
    QUrl url(kAuthUrl);
    QUrlQuery q;
    q.addQueryItem("client_id", m_clientId);
    q.addQueryItem("response_type", "code");
    q.addQueryItem("redirect_uri", m_localRedirectUri);
    q.addQueryItem("scope", "XboxLive.signin offline_access");
    q.addQueryItem("state", state);
    url.setQuery(q);

    emit loginProgress(tr("打开浏览器"), url.toString().left(80));
    QDesktopServices::openUrl(url);
}

// ═══════════════════════════════════════════════════
// Embedded login (Mode B — QWebEngineView)
// ═══════════════════════════════════════════════════

void MicrosoftAuth::startEmbeddedLogin(const QString& clientId) {
    if (m_busy) { emit loginFailed(tr("登录已在进行中")); return; }
    m_busy = true;
    m_embeddedMode = true;
    m_clientId = clientId.isEmpty() ? kClientId : clientId;
    QString redirect = u"https://login.microsoftonline.com/common/oauth2/nativeclient"_s;

    qCInfo(logApp) << "[WebEngine] Creating embedded login window...";

    // Create browser window
    auto* window = new QWidget(nullptr, Qt::WindowStaysOnTopHint | Qt::Dialog);
    window->setWindowTitle(tr("Microsoft 登录"));
    window->resize(480, 650);
    window->setAttribute(Qt::WA_DeleteOnClose);

    auto* webView = new QWebEngineView(window);
    webView->setGeometry(0, 0, 480, 650);
    webView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_embeddedWindow = window;
    m_webView = webView;

    // URL interception
    connect(webView, &QWebEngineView::urlChanged, this, [this, redirect](const QUrl& url) {
        QString urlStr = url.toString();
        if (urlStr.startsWith(redirect)) {
            // Extract code
            QUrlQuery query(url);
            QString code = query.queryItemValue("code");
            if (!code.isEmpty()) {
                qCInfo(logApp) << "[WebEngine] Code captured:" << code.left(15) << "...";
                m_codeCaptured = true;
                if (m_embeddedWindow) m_embeddedWindow->close();
                exchangeCode(code, redirect);
                return;
            }
        }
    });

    // Close event
    connect(window, &QWidget::destroyed, this, [this]() {
        if (m_embeddedMode && m_busy && !m_codeCaptured) {
            qCInfo(logApp) << "[WebEngine] Window closed by user";
            m_embeddedMode = false;
            m_busy = false;
            emit loginFailed(tr("登录被用户手动取消"));
        }
    });

    QString state = createState();
    QUrl url(kAuthUrl);
    QUrlQuery q;
    q.addQueryItem("client_id", m_clientId);
    q.addQueryItem("response_type", "code");
    q.addQueryItem("redirect_uri", redirect);
    q.addQueryItem("scope", "XboxLive.signin offline_access");
    q.addQueryItem("state", state);
    url.setQuery(q);

    emit loginProgress(tr("正在加载登录页面"), QString());

    qCInfo(logApp) << "[WebEngine] Loading auth URL..." << url.toString().left(100);
    webView->load(url);
    window->show();
    webView->show();

    // Notify QML: waiting for user to complete login in browser window
    QTimer::singleShot(500, this, [this]() {
        if (m_busy && m_embeddedMode)
            emit loginProgress(tr("请在浏览器窗口中登录"), QString());
    });
}

// ═══════════════════════════════════════════════════
// Token exchange + MC auth (shared between both modes)
// ═══════════════════════════════════════════════════

void MicrosoftAuth::exchangeCode(const QString& code, const QString& redirectUri) {
    emit loginProgress(tr("换取访问令牌"), QString());

    QUrlQuery params;
    params.addQueryItem("client_id", m_clientId);
    params.addQueryItem("code", code);
    params.addQueryItem("redirect_uri", redirectUri);
    params.addQueryItem("grant_type", "authorization_code");

    QNetworkRequest req(QUrl(QString::fromLatin1(kTokenUrl)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    auto* reply = HttpClient::instance().post(req, params.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray data = reply->readAll();
        if (status != 200) {
            emit loginFailed(QStringLiteral("Token exchange failed: %1").arg(status));
            m_busy = false; return;
        }
        QJsonObject resp = QJsonDocument::fromJson(data).object();
        if (resp.contains("error")) {
            emit loginFailed(resp["error_description"].toString());
            m_busy = false; return;
        }
        m_msAccessToken = resp["access_token"].toString();
        m_msRefreshToken = resp["refresh_token"].toString();
        authenticateXbl(m_msAccessToken);
    });
}

void MicrosoftAuth::authenticateXbl(const QString& accessToken) {
    emit loginProgress(tr("Xbox Live 认证"), QString());
    QJsonObject body;
    QJsonObject props;
    props["AuthMethod"] = "RPS";
    props["SiteName"] = "user.auth.xboxlive.com";
    props["RpsTicket"] = QStringLiteral("d=%1").arg(accessToken);
    body["Properties"] = props;
    body["RelyingParty"] = "http://auth.xboxlive.com";
    body["TokenType"] = "JWT";

    QNetworkRequest req(QUrl(QString::fromLatin1("https://user.auth.xboxlive.com/user/authenticate")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    auto* reply = HttpClient::instance().post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        QJsonObject resp = QJsonDocument::fromJson(reply->readAll()).object();
        QString token = resp["Token"].toString();
        if (token.isEmpty()) { emit loginFailed(tr("XBL 令牌为空")); m_busy = false; return; }
        authenticateXsts(token);
    });
}

void MicrosoftAuth::authenticateXsts(const QString& xblToken) {
    emit loginProgress(tr("XSTS 认证"), QString());
    QJsonObject body;
    QJsonObject props;
    props["SandboxId"] = "RETAIL";
    props["UserTokens"] = QJsonArray{xblToken};
    body["Properties"] = props;
    body["RelyingParty"] = "rp://api.minecraftservices.com/";
    body["TokenType"] = "JWT";

    QNetworkRequest req(QUrl(QString::fromLatin1("https://xsts.auth.xboxlive.com/xsts/authorize")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    auto* reply = HttpClient::instance().post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        QJsonObject resp = QJsonDocument::fromJson(reply->readAll()).object();
        if (resp.contains("XErr")) {
            emit loginFailed(tr("XSTS 错误: %1").arg(resp["XErr"].toInt()));
            m_busy = false; return;
        }
        QString token = resp["Token"].toString();
        QString uhs = resp["DisplayClaims"].toObject()["xui"].toArray()[0].toObject()["uhs"].toString();
        if (token.isEmpty() || uhs.isEmpty()) { emit loginFailed(tr("XSTS 响应无效")); m_busy = false; return; }
        authenticateMc(token, uhs);
    });
}

void MicrosoftAuth::authenticateMc(const QString& xstsToken, const QString& uhs) {
    emit loginProgress(tr("Minecraft 认证"), QString());
    QJsonObject body;
    body["identityToken"] = QStringLiteral("XBL3.0 x=%1;%2").arg(uhs, xstsToken);

    QNetworkRequest req(QUrl(QString::fromLatin1("https://api.minecraftservices.com/authentication/login_with_xbox")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    auto* reply = HttpClient::instance().post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        QJsonObject resp = QJsonDocument::fromJson(reply->readAll()).object();
        m_msMcToken = resp["access_token"].toString();
        if (m_msMcToken.isEmpty()) { emit loginFailed(tr("MC 令牌为空")); m_busy = false; return; }
        m_msTokenExpiresIn = resp["expires_in"].toInt(86400);
        fetchMcProfile(m_msMcToken);
    });
}

void MicrosoftAuth::fetchMcProfile(const QString& mcAccessToken) {
    emit loginProgress(tr("获权个人信息"), QString());

    QNetworkRequest req(QUrl(QString::fromLatin1("https://api.minecraftservices.com/minecraft/profile")));
    req.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(mcAccessToken).toUtf8());

    auto* reply = HttpClient::instance().getRaw(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        m_busy = false;
        QJsonObject resp = QJsonDocument::fromJson(reply->readAll()).object();
        QString name = resp["name"].toString();
        QString uuid = resp["id"].toString();
        if (name.isEmpty() || uuid.isEmpty()) { emit loginFailed(tr("未找到 Minecraft 档案")); return; }
        qCInfo(logApp) << "[MSA] Login success:" << name;
        emit loginSuccess(m_msMcToken, name, uuid, m_msRefreshToken, m_msTokenExpiresIn);
    });
}

void MicrosoftAuth::refreshMcChain(const QString& msAccessToken) {
    authenticateXbl(msAccessToken);
}

} // namespace ShadowLauncher
