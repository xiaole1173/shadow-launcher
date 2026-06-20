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
    if (skin.isNull()) { qCWarning(logAccount)<<"renderHead3D: load fail"<<skinPath; return {}; }

    constexpr int  SS = 4;
    constexpr int  SZ = 64 * SS;
    constexpr float HW = 4.0f;
    constexpr float DEG = 0.01745329252f;
    const float yr = 18.0f*DEG, pr = 0.0f;
    const float cosYf=cosf(yr),sinYf=sinf(yr), cosPf=cosf(pr),sinPf=sinf(pr);
    const float SC = (float)SZ / 12.0f;
    const float OFF = (SZ - SC*8.0f) * 0.5f;
    // Diffuse light: upper-front-right
    const float Lx=-0.4f, Ly=-0.6f, Lz=1.0f, Llen=sqrtf(Lx*Lx+Ly*Ly+Lz*Lz);

    QImage out(SZ, SZ, QImage::Format_ARGB32_Premultiplied);
    out.fill(0);

    int tW = skin.width(), tH = skin.height();
    float dV = (float)tH;
    // 64x32 also has hat in right-half (cols 32-63), same UV offset +32
    bool hat = (tH >= 32);
    float pad = 0.30f;

    auto prj = [&](float x,float y,float z, float&sx,float&sy,float&dep){
        float rx=x*cosYf-z*sinYf, rz=x*sinYf+z*cosYf, ry2=y*cosPf-rz*sinPf;
        dep=y*sinPf+rz*cosPf;
        sx=(rx+HW)*SC+OFF; sy=(HW-ry2)*SC+OFF;
    };

    struct Tri { float ax,ay,bx,by,cx,cy, ua,va,ub,vb,uc,vc, dep, br; bool ht; };
    Tri tri[24]; int tc=0;

    // Compute face brightness from rotated normal · light direction
    auto faceBright = [&](float nx,float ny,float nz)->float {
        float rnx=nx*cosYf-nz*sinYf, rnz=nx*sinYf+nz*cosYf, rny2=ny*cosPf-rnz*sinPf;
        float d = (rnx*Lx+rny2*Ly+rnz*Lz)/Llen;
        float b = 0.4f + 0.6f * fmaxf(0.0f, d);
        return b > 1.0f ? 1.0f : b;
    };
    auto emitQuad = [&](const float cx[4],const float cy[4],const float cz[4], int u0,int v0,int u1,int v1, bool isHat, float br)
    {
        float sx[4],sy[4],sd[4];
        for(int i=0;i<4;++i) prj(cx[i],cy[i],cz[i],sx[i],sy[i],sd[i]);
        float us[4]={(float)u0/tW,(float)u1/tW,(float)u1/tW,(float)u0/tW};
        float vs[4]={(float)v1/dV,(float)v1/dV,(float)v0/dV,(float)v0/dV};
        if(tc+2>24)return;
        tri[tc]={sx[0],sy[0],sx[1],sy[1],sx[2],sy[2],us[0],vs[0],us[1],vs[1],us[2],vs[2],(sd[0]+sd[1]+sd[2])/3.f,br,isHat};tc++;
        tri[tc]={sx[0],sy[0],sx[2],sy[2],sx[3],sy[3],us[0],vs[0],us[2],vs[2],us[3],vs[3],(sd[0]+sd[2]+sd[3])/3.f,br,isHat};tc++;
    };

    // Head inner faces
    {const float X[4]={-HW,HW,HW,-HW},Y[4]={-HW,-HW,-HW,-HW},Z[4]={HW,HW,-HW,-HW};emitQuad(X,Y,Z,8,0,16,8,false,faceBright(0,-1,0));}
    {const float X[4]={HW,HW,HW,HW},Y[4]={-HW,-HW,HW,HW},Z[4]={HW,-HW,-HW,HW};emitQuad(X,Y,Z,0,8,8,16,false,faceBright(1,0,0));}
    {const float X[4]={-HW,HW,HW,-HW},Y[4]={-HW,-HW,HW,HW},Z[4]={HW,HW,HW,HW};emitQuad(X,Y,Z,8,8,16,16,false,faceBright(0,0,1));}
    // Hat outer faces — works for both 64x32 and 64x64 (alpha<64 filter)
    if(hat){
        {const float X[4]={-HW,HW,HW,-HW},Y[4]={-HW-pad,-HW-pad,-HW-pad,-HW-pad},Z[4]={HW,HW,-HW,-HW};emitQuad(X,Y,Z,40,0,48,8,true,faceBright(0,-1,0));}
        {const float X[4]={HW+pad,HW+pad,HW+pad,HW+pad},Y[4]={-HW,-HW,HW,HW},Z[4]={HW,-HW,-HW,HW};emitQuad(X,Y,Z,32,8,40,16,true,faceBright(1,0,0));}
        {const float X[4]={-HW,HW,HW,-HW},Y[4]={-HW,-HW,HW,HW},Z[4]={HW+pad,HW+pad,HW+pad,HW+pad};emitQuad(X,Y,Z,40,8,48,16,true,faceBright(0,0,1));}
    }

    std::sort(tri,tri+tc,[](const Tri&a,const Tri&b){return a.dep<b.dep;});

    for(int ti=0;ti<tc;++ti){
        auto& t=tri[ti];
        float dn=(t.by-t.cy)*(t.ax-t.cx)+(t.cx-t.bx)*(t.ay-t.cy);
        if(fabsf(dn)<1e-6f)continue;
        int mx=(int)std::max(0.f,std::min({t.ax,t.bx,t.cx}));
        int Mx=(int)std::min((float)SZ-1,std::max({t.ax,t.bx,t.cx}));
        int my=(int)std::max(0.f,std::min({t.ay,t.by,t.cy}));
        int My=(int)std::min((float)SZ-1,std::max({t.ay,t.by,t.cy}));
        for(int y=my;y<=My;++y) for(int x=mx;x<=Mx;++x){
            float px=(float)x+.5f,py=(float)y+.5f;
            float w0=((t.by-t.cy)*(px-t.cx)+(t.cx-t.bx)*(py-t.cy))/dn;
            if(w0<-1e-4f)continue;
            float w1=((t.cy-t.ay)*(px-t.cx)+(t.ax-t.cx)*(py-t.cy))/dn;
            if(w1<-1e-4f)continue;
            float w2=1-w0-w1;if(w2<-1e-4f)continue;
            float u=w0*t.ua+w1*t.ub+w2*t.uc;u=u<0?0:u>1?1:u;
            float v=w0*t.va+w1*t.vb+w2*t.vc;v=v<0?0:v>1?1:v;
            QRgb sc=skin.pixel((int)(u*tW)%tW,(int)(v*dV)%(int)dV);
            if(t.ht){if(qAlpha(sc)<64)continue;}
            int rr=(int)(qRed(sc)*t.br); if(rr>255)rr=255;
            int gg=(int)(qGreen(sc)*t.br); if(gg>255)gg=255;
            int bb=(int)(qBlue(sc)*t.br); if(bb>255)bb=255;
            out.setPixel(x,y,qRgba(rr,gg,bb,qAlpha(sc)));
        }
    }

    QImage final = out.scaled(64, 64, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    QString hp=skinPath.left(skinPath.length()-4)+QStringLiteral("_head.png");
    if(final.save(hp,"PNG")){qCInfo(logAccount)<<"3D head:"<<hp;return hp;}
    qCWarning(logAccount)<<"renderHead3D: save fail"<<hp;
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
