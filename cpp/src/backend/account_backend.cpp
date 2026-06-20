#include "account_backend.h"
#include "../utils/logger.h"
#include <cmath>
#include <vector>
#include <algorithm>
#include <cstring>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPainter>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>

#include "core/http_client.h"

namespace ShadowLauncher {

// ────────────────────────────────────────────────────────────
// Constructor
// ────────────────────────────────────────────────────────────

AccountBackend::AccountBackend(QObject *parent)
    : QObject(parent)
{
    m_dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                + QStringLiteral("/shadow");

    // Ensure skin cache directory exists
    QDir().mkpath(m_dataDir + QStringLiteral("/assets/skins"));
    qCInfo(logAccount) << "AccountBackend constructed";

    loadOfflineHistory();
    loadMicrosoftSession();

    // ── MicrosoftAuth ──
    m_msAuth = new MicrosoftAuth(this);
    connect(m_msAuth, &MicrosoftAuth::loginProgress, this, [this](const QString& step, const QString& detail) {
        m_msStatus = step + QStringLiteral(": ") + detail;
        emit microsoftLoginProgress(step, detail);
    });
    connect(m_msAuth, &MicrosoftAuth::loginSuccess, this, [this](const QString& mcToken, const QString& username, const QString& uuid, const QString& refreshToken) {
        m_msMcToken = mcToken;
        m_msRefreshToken = refreshToken;
        m_username = username;
        m_uuid = uuid;
        m_loggedIn = true;
        m_isOnline = true;
        m_msStatus.clear();
        qCInfo(logAccount) << "Microsoft login SUCCESS:" << username << uuid;
        saveMicrosoftSession();
        emit microsoftLoginSuccess(username, uuid);
        emit accountChanged();
        emit logMessage(QStringLiteral("正版登录成功: %1").arg(username));
        downloadSkin(username);
    });
    connect(m_msAuth, &MicrosoftAuth::loginFailed, this, [this](const QString& error) {
        m_msStatus.clear();
        qCWarning(logAccount) << "Microsoft login FAILED:" << error;
        emit microsoftLoginFailed(error);
        emit microsoftLoginBusyChanged();
    });
}

// ────────────────────────────────────────────────────────────
// Offline Login
// ────────────────────────────────────────────────────────────

void AccountBackend::offlineLogin(const QString &username)
{
    QString name = username.trimmed();
    if (name.isEmpty()) {
        emit logMessage(QStringLiteral("用户名不能为空"));
        return;
    }

    // Preserve MS session username/uuid if already logged in online
    bool hadMsSession = m_loggedIn && m_isOnline && !m_msMcToken.isEmpty();
    if (!hadMsSession) {
        m_username = name;
        m_uuid = generateOfflineUuid(name);
    }
    m_loggedIn = true;
    m_isOnline = hadMsSession;  // don't flip online→offline if MS session exists

    // Track offline username history (most recent first, max 20)
    m_offlineUsernames.removeAll(name);
    m_offlineUsernames.prepend(name);
    if (m_offlineUsernames.size() > 20) {
        m_offlineUsernames = m_offlineUsernames.mid(0, 20);
    }
    saveOfflineHistory();

    emit accountChanged();
    emit offlineHistoryChanged();
    qCInfo(logAccount) << "Offline login:" << name << "UUID:" << m_uuid;
    emit logMessage(QStringLiteral("离线登录: %1").arg(name));

    downloadSkin(name);
}

// ────────────────────────────────────────────────────────────
// Logout
// ────────────────────────────────────────────────────────────

void AccountBackend::logout()
{
    m_username.clear();
    m_uuid.clear();
    m_skinPath.clear();
    m_loggedIn = false;
    m_isOnline = false;

    emit accountChanged();
    qCInfo(logAccount) << "Logged out";
    emit logMessage(QStringLiteral("已登出"));
}

// ────────────────────────────────────────────────────────────
// Microsoft Login
// ────────────────────────────────────────────────────────────

void AccountBackend::microsoftLogin()
{
    if (!m_msAuth) return;
    if (m_msAuth->isBusy()) {
        emit logMessage(QStringLiteral("Microsoft 登录已在进行中"));
        return;
    }
    emit microsoftLoginBusyChanged();
    m_msAuth->startLogin(QStringLiteral("1167b841-0421-4bfa-9ca2-3ab67e136f9f"));
}

void AccountBackend::cancelMicrosoftLogin()
{
    if (!m_msAuth) return;
    m_msAuth->cancelLogin();
    emit microsoftLoginBusyChanged();
}

// ────────────────────────────────────────────────────────────
// Skin Cache Path
// ────────────────────────────────────────────────────────────

// ────────────────────────────────────────────────────────────
// Generate Offline UUID  (Java Edition compatible)
// ────────────────────────────────────────────────────────────

QString AccountBackend::generateOfflineUuid(const QString &username) const
{
    // Java: UUID.nameUUIDFromBytes(("OfflinePlayer:" + name).getBytes(UTF_8))
    QByteArray input = QStringLiteral("OfflinePlayer:").toUtf8()
                     + username.toUtf8();
    QByteArray hash = QCryptographicHash::hash(input, QCryptographicHash::Md5);

    // RFC 4122: set version 3 (MD5-based) and variant 10xx
    char *data = hash.data();
    data[6] = static_cast<char>((data[6] & 0x0f) | 0x30);   // version 3
    data[8] = static_cast<char>((data[8] & 0x3f) | 0x80);   // variant 10xx

    // Format as 8-4-4-4-12 hex
    QString hex = hash.toHex();
    return QStringLiteral("%1-%2-%3-%4-%5")
        .arg(QStringView(hex).mid(0, 8),
             QStringView(hex).mid(8, 4),
             QStringView(hex).mid(12, 4),
             QStringView(hex).mid(16, 4),
             QStringView(hex).mid(20, 12));
}

// ────────────────────────────────────────────────────────────
// Skin Cache Path
// ────────────────────────────────────────────────────────────

QString AccountBackend::skinCachePath(const QString &username) const
{
    QByteArray nameHash = QCryptographicHash::hash(
        username.toLower().toUtf8(), QCryptographicHash::Md5);
    QString hex = QString::fromLatin1(nameHash.toHex());
    return m_dataDir + QStringLiteral("/assets/skins/") + hex
           + QStringLiteral(".png");
}

// ────────────────────────────────────────────────────────────
// Face Cropping — extract 8×8 head from skin atlas, alpha-blend hat
// ────────────────────────────────────────────────────────────

QString AccountBackend::renderHead3D(const QString& skinPath)
{
    QImage skin(skinPath);
    if (skin.isNull()) {
        qCWarning(logAccount) << "renderHead3D: cannot load" << skinPath;
        return {};
    }

    constexpr int    SZ   = 64;
    constexpr float  HALF = SZ / 2.0f;
    constexpr float  SCC  = HALF * 0.22f;

    QImage result(SZ, SZ, QImage::Format_ARGB32_Premultiplied);
    result.fill(0);

    float sw = (float)skin.width(), sh = (float)skin.height();
    float divV = (sh <= 32.0f) ? 32.0f : 64.0f;

    float yr = 30.0f * 3.1415926535f / 180.0f;
    float pr = -15.0f * 3.1415926535f / 180.0f;
    float cosY = cosf(yr), sinY = sinf(yr);
    float cosP = cosf(pr), sinP = sinf(pr);

    auto rotate = [&](float x, float y, float z, float& ox, float& oy, float& oz) {
        float rx = x * cosY - z * sinY;
        float rz = x * sinY + z * cosY;
        ox = rx;
        oy = y * cosP - rz * sinP;
        oz = y * sinP + rz * cosP;
    };

    struct Tri { float ax,ay,bx,by,cx,cy, u0,v0,u1,v1,u2,v2, depth; bool hat; };
    std::vector<Tri> tris; tris.reserve(72);

    struct FaceUV { int u0,v0,u1,v1; };
    static const FaceUV faceUVs[6] = {
        { 8,8,16,16}, {24,8,32,16}, { 8,0,16, 8},
        {16,0,24, 8}, { 0,8, 8,16}, {16,8,24,16},
    };

    auto emitTri = [&](float ax,float ay,float bx,float by,float cx,float cy,
        float ua,float va,float ub,float vb,float uc,float vc,float depth,bool hat)
    {
        float cp = (bx-ax)*(cy-ay) - (by-ay)*(cx-ax);
        if (cp <= 0) return;
        tris.push_back({ax,ay,bx,by,cx,cy, ua,va,ub,vb,uc,vc, depth, hat});
    };

    for (int hp = 0; hp < 2; ++hp) {
        float s = hp ? 1.07f : 1.0f, uOff = hp ? 32.0f : 0;
        float C[8][3] = {{-s,-s,+s},{+s,-s,+s},{+s,+s,+s},{-s,+s,+s},{+s,-s,-s},{-s,-s,-s},{-s,+s,-s},{+s,+s,-s}};
        int F[6][4] = {{0,1,2,3},{4,5,6,7},{3,2,7,6},{5,4,1,0},{1,4,7,2},{5,0,3,6}};
        for (int fi = 0; fi < 6; ++fi) {
            FaceUV fv = faceUVs[fi];
            float UB[4] = {(fv.u1+uOff)/sw,(fv.u0+uOff)/sw,(fv.u0+uOff)/sw,(fv.u1+uOff)/sw};
            float VB[4] = {fv.v1/divV,fv.v1/divV,fv.v0/divV,fv.v0/divV};
            float sx[4],sy[4],sd[4];
            for (int i = 0; i < 4; ++i) {
                float rx,ry,rz;
                rotate(C[F[fi][i]][0],C[F[fi][i]][1],C[F[fi][i]][2],rx,ry,rz);
                float zz = rz+10; if(zz<0.1f)zz=0.1f;
                sx[i]=(rx/zz)*10*SCC+HALF; sy[i]=(-ry/zz)*10*SCC+HALF; sd[i]=zz;
            }
            float d = (sd[0]+sd[2])*.5f;
            emitTri(sx[0],sy[0],sx[1],sy[1],sx[2],sy[2],UB[0],VB[0],UB[1],VB[1],UB[2],VB[2],d,(bool)hp);
            emitTri(sx[0],sy[0],sx[2],sy[2],sx[3],sy[3],UB[0],VB[0],UB[2],VB[2],UB[3],VB[3],d,(bool)hp);
        }
    }

    std::sort(tris.begin(),tris.end(),[](auto&a,auto&b){return a.depth>b.depth;});

    for (auto& t : tris) {
        int mx = qMax(0, (int)std::min({t.ax,t.bx,t.cx}));
        int Mx = qMin(SZ-1,(int)std::max({t.ax,t.bx,t.cx}));
        int my = qMax(0, (int)std::min({t.ay,t.by,t.cy}));
        int My = qMin(SZ-1,(int)std::max({t.ay,t.by,t.cy}));
        float denom = (t.by-t.cy)*(t.ax-t.cx)+(t.cx-t.bx)*(t.ay-t.cy);
        if(fabsf(denom)<1e-6f)continue;
        for (int y=my;y<=My;++y){for(int x=mx;x<=Mx;++x){
            float px=(float)x+.5f,py=(float)y+.5f;
            float w0=((t.by-t.cy)*(px-t.cx)+(t.cx-t.bx)*(py-t.cy))/denom;
            if(w0<-1e-4f)continue;
            float w1=((t.cy-t.ay)*(px-t.cx)+(t.ax-t.cx)*(py-t.cy))/denom;
            if(w1<-1e-4f)continue;
            float w2=1-w0-w1; if(w2<-1e-4f)continue;
            float u=w0*t.u0+w1*t.u1+w2*t.u2; if(u<0)u=0;if(u>1)u=1;
            float v=w0*t.v0+w1*t.v1+w2*t.v2; if(v<0)v=0;if(v>1)v=1;
            int six=(int)(u*sw)%(int)sw, siy=(int)(v*divV)%(int)divV;
            QRgb col=skin.pixel(six,siy);
            if(t.hat){if(qAlpha(col)<64)continue; result.setPixel(x,y,col);}
            else result.setPixel(x,y,col);
        }}
    }

    QString hp = skinPath.left(skinPath.length()-4) + QStringLiteral("_head.png");
    if (result.save(hp, "PNG")) { qCInfo(logAccount)<<"3D head:"<<hp; return hp; }
    qCWarning(logAccount)<<"renderHead3D: save failed"<<hp;
    return {};
}


QString AccountBackend::toImageUrl(const QString& filePath)
{
    if (filePath.isEmpty()) return {};
    if (filePath.startsWith(QStringLiteral("qrc:"))) return filePath;
    return QUrl::fromLocalFile(filePath).toString();
}

// ────────────────────────────────────────────────────────────
// Download Skin
// ────────────────────────────────────────────────────────────

void AccountBackend::downloadSkin(const QString &username)
{
    // ── Always try Mojang API first if we have a valid mcToken ──
    if (!m_msMcToken.isEmpty()) {
        downloadOnlineSkin();
        return;
    }

    QString cachePath = skinCachePath(username);

    // ── Check cache first ──
    QFileInfo cacheInfo(cachePath);
    if (cacheInfo.exists() && cacheInfo.size() > 0) {
        // Check for existing face crop
        QString facePath = cachePath.left(cachePath.length() - 4) + QStringLiteral("_head.png");
        if (!QFileInfo::exists(facePath)) {
            facePath = renderHead3D(cachePath);
        }
        m_skinPath = toImageUrl(facePath.isEmpty() ? cachePath : facePath);
        qCInfo(logAccount) << "Skin from cache, face:" << m_skinPath;
        emit skinReady();
        return;
    }

    // ── Offline / no token: use placeholder ──
    setFallbackSkin();
    qCInfo(logAccount) << "Skin: placeholder for" << username;
    emit skinReady();
}

// ────────────────────────────────────────────────────────────
// Online Skin (Mojang API)
// ────────────────────────────────────────────────────────────

void AccountBackend::downloadOnlineSkin()
{
    qCInfo(logAccount) << "Skin: fetching from Mojang API...";
    QNetworkRequest req(QUrl(QStringLiteral("https://api.minecraftservices.com/minecraft/profile")));
    req.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(m_msMcToken).toUtf8());

    QNetworkReply* reply = HttpClient::instance().getRaw(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(logAccount) << "Mojang profile fetch failed:" << reply->errorString();
            setFallbackSkin();
            emit skinReady();
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject profile = doc.object();

        // Update UUID and username from Mojang profile (fixes offline UUID leak)
        QString profileUuid = profile[QStringLiteral("id")].toString();
        QString profileName = profile[QStringLiteral("name")].toString();
        if (!profileUuid.isEmpty() && !profileName.isEmpty()) {
            bool changed = (m_uuid != profileUuid || m_username != profileName);
            m_uuid = profileUuid;
            m_username = profileName;
            if (changed) {
                saveMicrosoftSession();
                emit accountChanged();
                qCInfo(logAccount) << "UUID corrected from Mojang:" << profileUuid;
            }
        }

        QJsonArray skins = profile[QStringLiteral("skins")].toArray();

        QString skinUrl;
        for (const QJsonValue& sv : skins) {
            QJsonObject s = sv.toObject();
            if (s[QStringLiteral("state")].toString() == QStringLiteral("ACTIVE")) {
                skinUrl = s[QStringLiteral("url")].toString();
                break;
            }
        }

        if (skinUrl.isEmpty()) {
            qCInfo(logAccount) << "No active skin in Mojang profile, using default";
            setFallbackSkin();
            emit skinReady();
            return;
        }

        qCInfo(logAccount) << "Downloading skin from Mojang CDN:" << skinUrl;
        QNetworkRequest skinReq(skinUrl);
        QNetworkReply* skinReply = HttpClient::instance().getRaw(skinReq);
        connect(skinReply, &QNetworkReply::finished, this, [this, skinReply]() {
            skinReply->deleteLater();
            if (skinReply->error() != QNetworkReply::NoError) {
                qCWarning(logAccount) << "Skin texture download failed:" << skinReply->errorString();
                setFallbackSkin();
            } else {
                QString cachePath = skinCachePath(m_username);
                QFileInfo fi(cachePath);
                QDir().mkpath(fi.absolutePath());
                QFile file(cachePath);
                if (file.open(QIODevice::WriteOnly)) {
                    file.write(skinReply->readAll());
                    file.close();
                    QString facePath = renderHead3D(cachePath);
                    m_skinPath = toImageUrl(facePath.isEmpty() ? cachePath : facePath);
                    qCInfo(logAccount) << "Online skin + face:" << m_skinPath;
                }
            }
            emit skinReady();
        });
    });
}

void AccountBackend::setFallbackSkin()
{
    QString placeholder = m_dataDir + QStringLiteral("/assets/steve.png");
    if (QFileInfo::exists(placeholder)) {
        m_skinPath = toImageUrl(placeholder);
    }
    // Also try qrc fallback
    if (m_skinPath.isEmpty()) {
        m_skinPath = QStringLiteral("qrc:/ShadowLauncher/assets/steve.png");
    }
}

// ────────────────────────────────────────────────────────────
// Offline History Persistence
// ────────────────────────────────────────────────────────────

void AccountBackend::loadOfflineHistory()
{
    m_offlineUsernames.clear();

    QString path = m_dataDir + QStringLiteral("/offline_history.json");
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return;

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray())
        return;

    const QJsonArray arr = doc.array();
    m_offlineUsernames.reserve(arr.size());
    for (const QJsonValue &val : arr) {
        if (val.isString())
            m_offlineUsernames.append(val.toString());
    }
}

void AccountBackend::saveOfflineHistory()
{
    QJsonArray arr;
    for (const QString &name : m_offlineUsernames)
        arr.append(name);

    QString path = m_dataDir + QStringLiteral("/offline_history.json");
    QFileInfo fi(path);
    QDir().mkpath(fi.absolutePath());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;

    QJsonDocument doc(arr);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
}

// ────────────────────────────────────────────────────────────
// Microsoft Session Persistence
// ────────────────────────────────────────────────────────────

void AccountBackend::saveMicrosoftSession()
{
    QJsonObject obj;
    obj[QStringLiteral("username")] = m_username;
    obj[QStringLiteral("uuid")] = m_uuid;
    obj[QStringLiteral("mcToken")] = m_msMcToken;
    obj[QStringLiteral("refreshToken")] = m_msRefreshToken;

    QString path = m_dataDir + QStringLiteral("/microsoft_session.json");
    QFileInfo fi(path);
    QDir().mkpath(fi.absolutePath());

    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
        file.close();
        qCInfo(logAccount) << "Microsoft session saved:" << path;
    }
}

void AccountBackend::loadMicrosoftSession()
{
    QString path = m_dataDir + QStringLiteral("/microsoft_session.json");
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    QJsonObject obj = doc.object();
    m_username = obj[QStringLiteral("username")].toString();
    m_uuid = obj[QStringLiteral("uuid")].toString();
    m_msMcToken = obj[QStringLiteral("mcToken")].toString();
    m_msRefreshToken = obj[QStringLiteral("refreshToken")].toString();

    if (!m_msRefreshToken.isEmpty()) {
        qCInfo(logAccount) << "Found saved Microsoft session:" << m_username;
        m_loggedIn = true;
        m_isOnline = true;
        downloadSkin(m_username);
        QTimer::singleShot(500, this, [this]() { refreshMicrosoftToken(); });
    }
}

void AccountBackend::refreshMicrosoftToken()
{
    if (m_msRefreshToken.isEmpty()) return;
    qCInfo(logAccount) << "Attempting Microsoft token refresh...";

    QUrlQuery postData;
    postData.addQueryItem(QStringLiteral("client_id"), QStringLiteral("1167b841-0421-4bfa-9ca2-3ab67e136f9f"));
    postData.addQueryItem(QStringLiteral("refresh_token"), m_msRefreshToken);
    postData.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
    postData.addQueryItem(QStringLiteral("redirect_uri"), QStringLiteral("http://localhost"));

    QByteArray body = postData.toString(QUrl::FullyEncoded).toUtf8();

    QNetworkRequest req(QUrl(QStringLiteral("https://login.live.com/oauth20_token.srf")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    req.setTransferTimeout(10000);

    QNetworkReply* reply = HttpClient::instance().post(req, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        QByteArray raw = reply->readAll();
        QJsonObject obj = QJsonDocument::fromJson(raw).object();

        if (reply->error() != QNetworkReply::NoError || !obj[QStringLiteral("access_token")].isString()) {
            qCInfo(logAccount) << "Microsoft token refresh failed (need re-login):" << raw.left(200);
            return;
        }

        m_msRefreshToken = obj[QStringLiteral("refresh_token")].toString(m_msRefreshToken);
        qCInfo(logAccount) << "Microsoft token refreshed!";

        // Auto-login with refreshed token
        m_loggedIn = true;
        m_isOnline = true;
        saveMicrosoftSession();
        emit accountChanged();
        emit logMessage(QStringLiteral("已自动登录: %1").arg(m_username));
        downloadSkin(m_username);
    });
}

} // namespace ShadowLauncher
