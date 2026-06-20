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

    // Pre-render Steve and Alex offline heads (once, persistent)
    initOfflineHeads();

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

    // Preserve MS identity (username/UUID) for premium mode if MS session exists
    bool hasMsSession = !m_msMcToken.isEmpty();
    if (!hasMsSession) {
        m_username = name;
        m_uuid = generateOfflineUuid(name);
    }
    // m_isOnline = false ensures offline auth, but MS identity is preserved for premium
    m_loggedIn = true;
    m_isOnline = false;

    // Track offline username history (most recent first, max 20)
    m_offlineUsernames.removeAll(name);
    m_offlineUsernames.prepend(name);
    if (m_offlineUsernames.size() > 20) {
        m_offlineUsernames = m_offlineUsernames.mid(0, 20);
    }
    saveOfflineHistory();

    emit accountChanged();
    emit offlineHistoryChanged();
    // Skin: always use offline head render (Steve/Alex by UUID hash)
    // Writes to m_offlineSkinPath (dedicated, never touches m_skinPath)
    updateOfflineSkin(name);

    qCInfo(logAccount) << "Offline login:" << name << "UUID:" << m_uuid;
    emit logMessage(QStringLiteral("离线登录: %1").arg(name));

    // Skin handled by updateOfflineSkin() above — no downloadSkin() call here
}

void AccountBackend::setOnlineMode()
{
    // Restore online status when premium tab is active and MS session exists
    if (!m_msRefreshToken.isEmpty() || !m_msMcToken.isEmpty()) {
        m_isOnline = true;
        m_loggedIn = true;
    }
}

// ────────────────────────────────────────────────────────────
// Update Offline Skin (real-time avatar preview by UUID hash)
// ────────────────────────────────────────────────────────────

void AccountBackend::updateOfflineSkin(const QString &username)
{
    QString name = username.trimmed();

    // Default to Steve model when name is empty
    QString modelName = QStringLiteral("steve");

    if (!name.isEmpty()) {
        // Generate offline UUID → hash LSB determines Steve/Alex
        QByteArray input = QStringLiteral("OfflinePlayer:").toUtf8() + name.toUtf8();
        QByteArray hash = QCryptographicHash::hash(input, QCryptographicHash::Md5);
        modelName = (static_cast<unsigned char>(hash[0]) & 1) == 1
                        ? QStringLiteral("alex") : QStringLiteral("steve");
        qCInfo(logAccount) << "Offline skin preview:" << name << "→" << modelName
                           << "(hash[0]&1=" << (hash[0] & 1) << ")";
    }

    QString headPath = m_dataDir + QStringLiteral("/assets/skins/") + modelName + QStringLiteral("_head.png");

    if (!QFileInfo::exists(headPath)) {
        initOfflineHeads();
        if (!QFileInfo::exists(headPath)) {
            qCWarning(logAccount) << "Offline head still missing:" << headPath;
            m_offlineSkinPath.clear();
            emit offlineSkinReady();
            return;
        }
    }

    // Dedicated offline path — never touches m_skinPath (premium)
    m_offlineSkinPath = toImageUrl(headPath);
    emit offlineSkinReady();
}

// ────────────────────────────────────────────────────────────
// Init Offline Heads — pre-render Steve & Alex heads once
// ────────────────────────────────────────────────────────────

void AccountBackend::initOfflineHeads()
{
    const QString skinsDir = QStringLiteral("D:/MC_Project/shadow-launcher/skins");
    QStringList models = {QStringLiteral("steve"), QStringLiteral("alex")};

    for (const QString& model : models) {
        QString headPath = m_dataDir + QStringLiteral("/assets/skins/") + model + QStringLiteral("_head.png");
        if (QFileInfo::exists(headPath)) {
            qCInfo(logAccount) << "Offline head already cached:" << model;
            continue;
        }

        QString skinSource = skinsDir + QStringLiteral("/") + model + QStringLiteral(".png");
        if (!QFileInfo::exists(skinSource)) {
            qCWarning(logAccount) << "Skin source not found:" << skinSource;
            continue;
        }

        QString rendered = renderHead3D(skinSource);
        if (rendered.isEmpty()) {
            qCWarning(logAccount) << "Failed to render head for:" << model;
            continue;
        }

        if (QFile::rename(rendered, headPath)) {
            qCInfo(logAccount) << "Offline head rendered + saved:" << headPath;
        } else {
            // fallback: copy + delete
            QFile::copy(rendered, headPath);
            QFile::remove(rendered);
            qCInfo(logAccount) << "Offline head copied:" << headPath;
        }
    }
}

// ────────────────────────────────────────────────────────────
// Logout
// ────────────────────────────────────────────────────────────

void AccountBackend::logout()
{
    m_username.clear();
    m_uuid.clear();
    m_skinPath.clear();
    m_capePath.clear();
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

QString AccountBackend::capeCachePath(const QString &username) const
{
    QByteArray nameHash = QCryptographicHash::hash(
        username.toLower().toUtf8(), QCryptographicHash::Md5);
    QString hex = QString::fromLatin1(nameHash.toHex());
    return m_dataDir + QStringLiteral("/assets/capes/") + hex
           + QStringLiteral(".png");
}

// ────────────────────────────────────────────────────────────
// Face Cropping — extract 8×8 head from skin atlas, alpha-blend hat
// ────────────────────────────────────────────────────────────

QString AccountBackend::renderHead3D(const QString& skinPath)
{
    // PCL-style: hat layer RENDERED LARGER than face layer (56 vs 48 in 64 grid)
    // Hat pixels = 7px width, Face pixels = 6px width → hat wraps around as "head套"
    QImage skin(skinPath);
    if (skin.isNull()) { qCWarning(logAccount)<<"renderHead3D: load fail"<<skinPath; return {}; }

    // ── Format detection ──
    const int skinW = skin.width();
    const int skinH = skin.height();
    const char* formatTag = "unknown";
    if (skinW == 64 && skinH == 32) formatTag = "64x32 (legacy)";
    else if (skinW == 64 && skinH == 64) formatTag = "64x64 (modern)";
    else formatTag = "custom";
    qCInfo(logAccount) << "renderHead3D: skin format" << formatTag << skinW << "x" << skinH;

    constexpr int FACE_X=8, FACE_Y=8, FACE_W=8, FACE_H=8;
    constexpr int HAT_X=40, HAT_Y=8, HAT_W=8, HAT_H=8;
    constexpr int CANVAS = 128;
    // PCL ratios: face 48/64 = 3/4, hat 56/64 = 7/8
    constexpr int FACE_SZ = CANVAS * 3 / 4;   // 96
    constexpr int HAT_SZ  = CANVAS * 7 / 8;   // 112

    bool hasHat = (skinW >= 64 && skinH >= 32);
    qCInfo(logAccount) << "renderHead3D: hasHat=" << hasHat << ", format=" << formatTag;

    // 1. Face layer: 8×8 → scaled to FACE_SZ, centered in canvas
    QImage face = skin.copy(FACE_X, FACE_Y, FACE_W, FACE_H);
    QImage faceScaled = face.scaled(FACE_SZ, FACE_SZ, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    faceScaled = faceScaled.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    QImage out(CANVAS, CANVAS, QImage::Format_ARGB32_Premultiplied);
    out.fill(0);
    int faceX = (CANVAS - FACE_SZ) / 2;
    int faceY = (CANVAS - FACE_SZ) / 2;
    for (int y = 0; y < FACE_SZ; ++y) {
        QRgb* dst = (QRgb*)out.scanLine(faceY + y) + faceX;
        const QRgb* src = (const QRgb*)faceScaled.constScanLine(y);
        for (int x = 0; x < FACE_SZ; ++x) dst[x] = src[x];
    }

    // 2. Hat layer: 8×8 → scaled to HAT_SZ (LARGER than face), centered on top
    if (hasHat) {
        QImage hatSrc = skin.copy(HAT_X, HAT_Y, HAT_W, HAT_H);
        QImage hat = hatSrc.scaled(HAT_SZ, HAT_SZ, Qt::IgnoreAspectRatio, Qt::FastTransformation);
        hat = hat.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        int hatX = (CANVAS - HAT_SZ) / 2;
        int hatY = (CANVAS - HAT_SZ) / 2;
        for (int y = 0; y < HAT_SZ; ++y) {
            QRgb* dst = (QRgb*)out.scanLine(hatY + y) + hatX;
            const QRgb* hatLine = (const QRgb*)hat.constScanLine(y);
            for (int x = 0; x < HAT_SZ; ++x) {
                int a = qAlpha(hatLine[x]);
                if (a >= 64) dst[x] = hatLine[x];
            }
        }
    }

    QString hp = skinPath.left(skinPath.length() - 4) + QStringLiteral("_head.png");
    if (out.save(hp, "PNG")) {
        qCInfo(logAccount) << "PCL-2D head:" << hp;
        return hp;
    }
    qCWarning(logAccount) << "renderHead3D: save fail" << hp;
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

        // ── Cape extraction ──
        QJsonArray capes = profile[QStringLiteral("capes")].toArray();
        QString capeUrl;
        for (const QJsonValue& cv : capes) {
            QJsonObject c = cv.toObject();
            if (c[QStringLiteral("state")].toString() == QStringLiteral("ACTIVE")) {
                capeUrl = c[QStringLiteral("url")].toString();
                qCInfo(logAccount) << "Found cape:" << c[QStringLiteral("alias")].toString() << capeUrl;
                break;
            }
        }

        // ── Download cape if available ──
        if (!capeUrl.isEmpty()) {
            QDir().mkpath(m_dataDir + QStringLiteral("/assets/capes"));
            QNetworkRequest capeReq(capeUrl);
            QNetworkReply* capeReply = HttpClient::instance().getRaw(capeReq);
            connect(capeReply, &QNetworkReply::finished, this, [this, capeReply]() {
                capeReply->deleteLater();
                if (capeReply->error() == QNetworkReply::NoError) {
                    QString cachePath = capeCachePath(m_username);
                    QFile file(cachePath);
                    if (file.open(QIODevice::WriteOnly)) {
                        file.write(capeReply->readAll());
                        file.close();
                        m_capePath = toImageUrl(cachePath);
                        qCInfo(logAccount) << "Cape cached:" << cachePath;
                        emit capeReady();
                    }
                } else {
                    qCWarning(logAccount) << "Cape download failed:" << capeReply->errorString();
                }
            });
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
                emit skinReady();
                return;
            }
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
            emit skinReady();
        });
    });
}

void AccountBackend::setFallbackSkin()
{
    // Use rendered offline Steve head (always exists after initOfflineHeads)
    QString headPath = m_dataDir + QStringLiteral("/assets/skins/steve_head.png");
    if (QFileInfo::exists(headPath)) {
        m_skinPath = toImageUrl(headPath);
    } else {
        // Last resort: qrc placeholder
        m_skinPath = QStringLiteral("qrc:/ShadowLauncher/assets/steve.png");
    }
    m_capePath.clear();
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

        // Auto-login with refreshed token (skin already downloaded from loadMicrosoftSession)
        m_loggedIn = true;
        m_isOnline = true;
        saveMicrosoftSession();
        emit accountChanged();
        emit logMessage(QStringLiteral("已自动登录: %1").arg(m_username));
    });
}

} // namespace ShadowLauncher
