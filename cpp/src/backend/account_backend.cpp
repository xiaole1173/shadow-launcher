// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "account_backend.h"
#include "../utils/logger.h"
#include "../utils/token_crypto.h"
#include <cmath>
#include <vector>
#include <QCoreApplication>
#include <algorithm>
#include <cstring>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPainter>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>
#include <QDateTime>

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
    qCInfo(logAccount) << QStringLiteral("账号模块已初始化");

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
    connect(m_msAuth, &MicrosoftAuth::loginSuccess, this, [this](const QString& mcToken, const QString& username, const QString& uuid, const QString& refreshToken, int expiresIn) {
        m_msMcToken = mcToken;
        // Only update refreshToken if non-empty AND not during background refresh
        // (refreshMicrosoftToken already stored the new token before calling refreshMcChain)
        if (!m_refreshingToken && !refreshToken.isEmpty()) {
            m_msRefreshToken = refreshToken;
        }
        m_username = username;
        m_uuid = uuid;
        m_loggedIn = true;
        m_isOnline = true;
        m_msStatus.clear();
        m_msTokenObtainedAt = QDateTime::currentSecsSinceEpoch();
        m_msTokenExpiresIn = expiresIn;  // from Mojang API response
        qCInfo(logAccount) << QStringLiteral("正版登录成功: ") << username << uuid
                           << "refreshToken:" << (refreshToken.isEmpty() ? QStringLiteral("EMPTY!") : QStringLiteral("present"));
        saveMicrosoftSession();
        startBackgroundRefresh();
        if (m_refreshingToken) {
            m_refreshingToken = false;
            emit tokenRefreshed(true);
        } else {
            emit microsoftLoginSuccess(username, uuid);
        }
        emit accountChanged();
        emit logMessage(tr("正版登录成功: %1").arg(username));
        downloadSkin(username);
    });
    connect(m_msAuth, &MicrosoftAuth::loginFailed, this, [this](const QString& error) {
        m_msStatus.clear();
        if (m_refreshingToken) {
            m_refreshingToken = false;
            emit tokenRefreshed(false);
            // LaunchBackend needs tokenRefreshFailed to decide retry vs abort.
            // loginFailed from the chain (XBL/XSTS/MC/Profile) is NOT token expiry,
            // it is a transient error that should be retried, not blocked.
            emit tokenRefreshFailed(false, error);
        }
        qCWarning(logAccount) << QStringLiteral("Microsoft登录失败 错误=%1").arg(error);
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
        emit logMessage(tr("用户名不能为空"));
        return;
    }

    // Offline identity — COMPLETELY separate from premium (m_username/m_uuid untouched)
    m_offlineUsername = name;
    m_offlineUuid = generateOfflineUuid(name);
    m_isOnline = false;
    m_loggedIn = true;

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

    qCInfo(logAccount) << QStringLiteral("离线登录: ") << name << QStringLiteral(" UUID:") << m_offlineUuid;
    emit logMessage(tr("离线登录: %1").arg(name));

    // Skin handled by updateOfflineSkin() above — no downloadSkin() call here
}

// ────────────────────────────────────────────────────────────
// Cape Selection — calls Mojang API to switch cape
// ────────────────────────────────────────────────────────────

void AccountBackend::selectCape(const QString &alias)
{
    if (m_msMcToken.isEmpty()) {
        emit capeChangeFinished(false, QStringLiteral("未登录正版账号"));
        return;
    }

    if (alias.isEmpty()) {
        // ── Disable cape ──
        qCInfo(logAccount) << QStringLiteral("卸下披风 (DELETE)");
        QNetworkRequest req(QUrl(QStringLiteral("https://api.minecraftservices.com/minecraft/profile/capes/active")));
        req.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(m_msMcToken).toUtf8());
        QNetworkReply* reply = HttpClient::instance().deleteResource(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            if (reply->error() == QNetworkReply::NoError) {
                m_capePath.clear();
                m_activeCapeAlias.clear();
                for (CapeInfo& ci : m_capes) ci.active = false;
                qCInfo(logAccount) << QStringLiteral("披风已卸下");
                emit capesReady();
                emit capeReady();
                emit capeChangeFinished(true, QString());
            } else {
                QString err = reply->errorString();
                qCWarning(logAccount) << QStringLiteral("卸下披风失败 错误=%1").arg(err);
                emit capeChangeFinished(false, err);
            }
        });
    } else {
        // ── Switch to a specific cape ──
        QString capeId;
        for (const CapeInfo& ci : m_capes) {
            if (ci.alias == alias) {
                capeId = ci.capeId;
                break;
            }
        }
        if (capeId.isEmpty()) {
            qCWarning(logAccount) << QStringLiteral("披风ID未找到 alias=%1").arg(alias);
            emit capeChangeFinished(false, QStringLiteral("披风ID未找到"));
            return;
        }

        qCInfo(logAccount) << QStringLiteral("切换披风 alias=%1 capeId=%2").arg(alias, capeId);
        QNetworkRequest req(QUrl(QStringLiteral("https://api.minecraftservices.com/minecraft/profile/capes/active")));
        req.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(m_msMcToken).toUtf8());
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

        QJsonObject body;
        body[QStringLiteral("capeId")] = capeId;
        QNetworkReply* reply = HttpClient::instance().put(req, QJsonDocument(body).toJson());
        connect(reply, &QNetworkReply::finished, this, [this, reply, alias]() {
            reply->deleteLater();
            if (reply->error() == QNetworkReply::NoError) {
                m_activeCapeAlias = alias;
                m_capePath.clear();
                for (CapeInfo& ci : m_capes) {
                    ci.active = (ci.alias == alias);
                    if (ci.active) m_capePath = ci.localPath;
                }
                qCInfo(logAccount) << QStringLiteral("披风切换成功 alias=%1").arg(alias);
                emit capesReady();
                emit capeReady();
                emit capeChangeFinished(true, QString());
            } else {
                QString err = reply->errorString();
                qCWarning(logAccount) << QStringLiteral("披风切换失败 错误=%1").arg(err);
                emit capeChangeFinished(false, err);
            }
        });
    }
}

// ────────────────────────────────────────────────────────────
// Upload Skin to Mojang API
// ────────────────────────────────────────────────────────────

void AccountBackend::uploadSkin(const QString &filePath, const QString &variant)
{
    if (m_msMcToken.isEmpty()) {
        emit skinUploadFinished(false, QStringLiteral("未登录正版账号"));
        return;
    }

    QFileInfo fi(filePath);
    if (!fi.exists()) {
        emit skinUploadFinished(false, QStringLiteral("皮肤文件不存在"));
        return;
    }

    qCInfo(logAccount) << QStringLiteral("上传皮肤 variant=%1 path=%2").arg(variant, filePath);

    QHttpMultiPart* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    // Variant field
    QHttpPart variantPart;
    variantPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                          QStringLiteral("form-data; name=\"variant\""));
    variantPart.setBody(variant.toUtf8());
    multiPart->append(variantPart);

    // File field
    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentTypeHeader,
                       QStringLiteral("image/png"));
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QStringLiteral("form-data; name=\"file\"; filename=\"skin.png\""));
    QFile* file = new QFile(filePath);
    if (!file->open(QIODevice::ReadOnly)) {
        delete multiPart;
        delete file;
        emit skinUploadFinished(false, QStringLiteral("无法读取皮肤文件"));
        return;
    }
    filePart.setBodyDevice(file);
    file->setParent(multiPart);  // multiPart owns file now
    multiPart->append(filePart);

    QNetworkRequest req(QUrl(QStringLiteral("https://api.minecraftservices.com/minecraft/profile/skins")));
    req.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(m_msMcToken).toUtf8());
    QNetworkReply* reply =
        HttpClient::instance().manager() ?
            HttpClient::instance().manager()->post(req, multiPart) : nullptr;

    if (!reply) {
        delete multiPart;
        emit skinUploadFinished(false, QStringLiteral("网络请求失败"));
        return;
    }
    multiPart->setParent(reply);  // reply owns multiPart

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            qCInfo(logAccount) << QStringLiteral("皮肤上传成功");
            // Refresh profile to get new skin
            downloadSkin(m_username);
            emit skinUploadFinished(true, QString());
        } else {
            QString err = reply->errorString();
            qCWarning(logAccount) << QStringLiteral("皮肤上传失败 错误=%1").arg(err);
            emit skinUploadFinished(false, err);
        }
    });
}

// ────────────────────────────────────────────────────────────
// Refresh Skin (re-download/regenerate from current account)
// ────────────────────────────────────────────────────────────

void AccountBackend::refreshSkin()
{
    if (m_loggedIn && m_isOnline && !m_username.isEmpty()) {
        qCInfo(logApp) << QStringLiteral("正版皮肤刷新");
        emit logMessage(tr("正在刷新皮肤..."));
        downloadOnlineSkin(true);
    } else if (!m_offlineUsername.isEmpty()) {
        qCInfo(logApp) << QStringLiteral("离线皮肤刷新");
        emit logMessage(tr("正在刷新离线头像..."));
        updateOfflineSkin(m_offlineUsername);
    } else {
        emit logMessage(tr("未登录，无需刷新"));
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
        // Minecraft Java: UUID.hashCode() & 1 determines Alex (slim) vs Steve (wide)
        // hashCode = XOR of 4× 32-bit halves: hash[0..3]^hash[4..7]^hash[8..11]^hash[12..15]
        // Simplified to byte-level: bit0 = (hash[3]^hash[7]^hash[11]^hash[15]) & 1
        int parity = (hash[3] & 1) ^ (hash[7] & 1) ^ (hash[11] & 1) ^ (hash[15] & 1);
        modelName = parity ? QStringLiteral("alex") : QStringLiteral("steve");
        qCInfo(logAccount) << QStringLiteral("离线皮肤预览: ") << name << QStringLiteral(" -> ") << modelName
                           << "(parity=" << parity << ")";
    }

    QString headPath = m_dataDir + QStringLiteral("/assets/skins/") + modelName + QStringLiteral("_head.png");

    if (!QFileInfo::exists(headPath)) {
        initOfflineHeads();
        if (!QFileInfo::exists(headPath)) {
            qCWarning(logAccount) << QStringLiteral("离线头像仍缺失 路径=%1").arg(headPath);
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
    const QString skinsDir = QCoreApplication::applicationDirPath() + QStringLiteral("/skins");
    QStringList models = {QStringLiteral("steve"), QStringLiteral("alex")};

    for (const QString& model : models) {
        QString headPath = m_dataDir + QStringLiteral("/assets/skins/") + model + QStringLiteral("_head.png");
        if (QFileInfo::exists(headPath)) {
            qCInfo(logAccount) << QStringLiteral("离线头像已缓存: ") << model;
            continue;
        }

        QString skinSource = skinsDir + QStringLiteral("/") + model + QStringLiteral(".png");
        if (!QFileInfo::exists(skinSource)) {
            qCWarning(logAccount) << QStringLiteral("皮肤源文件不存在 路径=%1").arg(skinSource);
            continue;
        }

        QString rendered = renderHead3D(skinSource);
        if (rendered.isEmpty()) {
            qCWarning(logAccount) << QStringLiteral("头像渲染失败 模型=%1").arg(model);
            continue;
        }

        if (QFile::rename(rendered, headPath)) {
            qCInfo(logAccount) << QStringLiteral("离线头像已渲染: ") << headPath;
        } else {
            // fallback: copy + delete
            QFile::copy(rendered, headPath);
            QFile::remove(rendered);
            qCInfo(logAccount) << QStringLiteral("离线头像已复制: ") << headPath;
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
    m_offlineUsername.clear();
    m_offlineUuid.clear();
    m_skinPath.clear();
    m_offlineSkinPath.clear();
    m_capePath.clear();
    m_skinVariant.clear();
    m_capes.clear();
    m_activeCapeAlias.clear();
    m_loggedIn = false;
    m_isOnline = false;

    // Clear MS token state and persisted session
    m_msRefreshToken.clear();
    m_msMcToken.clear();
    m_msTokenObtainedAt = 0;
    m_msTokenExpiresIn = 0;
    deleteMicrosoftSession();

    // Stop background refresh timer
    if (m_refreshTimer) {
        m_refreshTimer->stop();
        delete m_refreshTimer;
        m_refreshTimer = nullptr;
    }

    emit accountChanged();
    qCInfo(logAccount) << QStringLiteral("已登出");
    emit logMessage(tr("已登出"));
}

// ────────────────────────────────────────────────────────────
// Microsoft Login
// ────────────────────────────────────────────────────────────

void AccountBackend::setEmbeddedLoginEnabled(bool v)
{
    if (m_embeddedLoginEnabled != v) {
        m_embeddedLoginEnabled = v;
        emit embeddedLoginChanged();
        qCInfo(logAccount) << QStringLiteral("内嵌登录 %1").arg(v ? QStringLiteral("已启用") : QStringLiteral("已禁用"));
    }
}

void AccountBackend::microsoftLogin()
{
    if (!m_msAuth) return;
    if (m_msAuth->isBusy()) {
        emit logMessage(tr("Microsoft 登录已在进行中"));
        return;
    }
    emit microsoftLoginBusyChanged();
        const QString clientId = QStringLiteral("YOUR_CLIENT_ID_HERE");
    if (m_embeddedLoginEnabled) {
        qCInfo(logAccount) << QStringLiteral("使用内嵌浏览器登录");
        m_msAuth->startEmbeddedLogin(clientId);
    } else {
        qCInfo(logAccount) << QStringLiteral("使用本地回调登录");
        m_msAuth->startLogin(clientId);
    }
}

void AccountBackend::cancelMicrosoftLogin()
{
    if (!m_msAuth) return;
    m_msAuth->cancelLogin();
    emit microsoftLoginBusyChanged();
}

bool AccountBackend::shouldRefresh() const
{
    if (m_msRefreshToken.isEmpty()) return false;
    if (m_msTokenExpiresIn <= 0 || m_msTokenObtainedAt <= 0) return true;
    qint64 now = QDateTime::currentSecsSinceEpoch();
    qint64 expiresAt = m_msTokenObtainedAt + m_msTokenExpiresIn;
    return (now + 3600) >= expiresAt;
}

void AccountBackend::startBackgroundRefresh()
{
    if (m_refreshTimer) return;
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(3600000);
    connect(m_refreshTimer, &QTimer::timeout, this, [this]() {
        if (shouldRefresh()) {
            qCInfo(logAccount) << QStringLiteral("后台刷新已触发");
            m_refreshingToken = true;
            refreshMicrosoftToken();
        }
    });
    m_refreshTimer->start();
    qCInfo(logAccount) << QStringLiteral("后台令牌刷新已启动");
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

QVariantList AccountBackend::capes() const
{
    QVariantList list;
    for (const CapeInfo& ci : m_capes) {
        QVariantMap m;
        m[QStringLiteral("alias")] = ci.alias;
        m[QStringLiteral("capeId")] = ci.capeId;
        m[QStringLiteral("localPath")] = ci.localPath;
        m[QStringLiteral("active")] = ci.active;
        list.append(m);
    }
    return list;
}

// ────────────────────────────────────────────────────────────
// Face Cropping — extract 8×8 head from skin atlas, alpha-blend hat
// ────────────────────────────────────────────────────────────

QString AccountBackend::renderHead3D(const QString& skinPath)
{
    // Hat layer rendered larger than face layer (56 vs 48 in 64 grid)
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
    qCInfo(logAccount) << QStringLiteral("皮肤格式: ") << formatTag << skinW << "x" << skinH;

    constexpr int FACE_X=8, FACE_Y=8, FACE_W=8, FACE_H=8;
    constexpr int HAT_X=40, HAT_Y=8, HAT_W=8, HAT_H=8;
    constexpr int CANVAS = 128;
    // Ratios: face 48/64 = 3/4, hat 56/64 = 7/8
    constexpr int FACE_SZ = CANVAS * 3 / 4;   // 96
    constexpr int HAT_SZ  = CANVAS * 7 / 8;   // 112

    bool hasHat = (skinW >= 64 && skinH >= 32);
    qCInfo(logAccount) << QStringLiteral("皮肤帽子: ") << hasHat << QStringLiteral(" 格式:") << formatTag;

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
        qCInfo(logAccount) << QStringLiteral("头像: ") << hp;
        return hp;
    }
    qCWarning(logAccount) << QStringLiteral("renderHead3D 保存失败 路径=%1").arg(hp);
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
        qCInfo(logAccount) << QStringLiteral("开始从Mojang获取皮肤和披风 用户=%1").arg(username);
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
        qCInfo(logAccount) << QStringLiteral("皮肤缓存: ") << m_skinPath;
        emit skinReady();
        return;
    }

    // ── Offline / no token: use placeholder ──
    setFallbackSkin();
    qCInfo(logAccount) << QStringLiteral("皮肤占位: ") << username;
    emit skinReady();
}

// ────────────────────────────────────────────────────────────
// Online Skin (Mojang API)
// ────────────────────────────────────────────────────────────

void AccountBackend::downloadOnlineSkin(bool forceRefresh)
{
    // Check skin cache
    bool skinCached = false;
    QString cachePath = skinCachePath(m_username);
    QString facePath = cachePath + QStringLiteral("_head.png");
    if (QFileInfo::exists(facePath)) {
        m_skinPath = toImageUrl(facePath);
        skinCached = true;
        qCInfo(logAccount) << QStringLiteral("皮肤缓存命中(已渲染) path=%1").arg(m_skinPath);
        if (!forceRefresh)
            emit skinReady();
    } else if (QFileInfo::exists(cachePath)) {
        // Raw skin exists but no rendered face — render and use
        facePath = renderHead3D(cachePath);
        m_skinPath = toImageUrl(facePath.isEmpty() ? cachePath : facePath);
        skinCached = true;
        qCInfo(logAccount) << QStringLiteral("皮肤缓存命中(原始) path=%1").arg(m_skinPath);
        if (!forceRefresh)
            emit skinReady();
    }

    // When force-refreshing, always download skin from network (ignore cache)
    if (forceRefresh) {
        skinCached = false;
        qCInfo(logAccount) << QStringLiteral("强制刷新: 将重新下载皮肤纹理");
    }

    // Always call Mojang Profile API — even if skin is cached,
    // we need cape data which is only available from the live API.
    // In the response handler, we'll skip skin texture download if skinCached is true.

    // No cache — fetch from Mojang API
    qCInfo(logAccount) << QStringLiteral("正在调用Mojang Profile API...");
    QNetworkRequest req(QUrl(QStringLiteral("https://api.minecraftservices.com/minecraft/profile")));
    req.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(m_msMcToken).toUtf8());

    QNetworkReply* reply = HttpClient::instance().getRaw(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, skinCached]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(logAccount) << QStringLiteral("Mojang档案获取失败 错误=%1").arg(reply->errorString());
            m_capes.clear();
            emit capesReady();
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
                qCInfo(logAccount) << QStringLiteral("UUID已修正: ") << profileUuid;
            }
        }

        QJsonArray skins = profile[QStringLiteral("skins")].toArray();
        QJsonArray capes = profile[QStringLiteral("capes")].toArray();
        qCInfo(logAccount) << QStringLiteral("Mojang Profile API响应 皮肤数=%1 披风数=%2")
                                   .arg(skins.size()).arg(capes.size());

        QString skinUrl;
        QString skinVariant;
        for (const QJsonValue& sv : skins) {
            QJsonObject s = sv.toObject();
            if (s[QStringLiteral("state")].toString() == QStringLiteral("ACTIVE")) {
                skinUrl = s[QStringLiteral("url")].toString();
                skinVariant = s[QStringLiteral("variant")].toString();
                if (m_skinVariant != skinVariant) {
                    m_skinVariant = skinVariant;
                    emit skinVariantChanged();
                }
                qCInfo(logAccount) << QStringLiteral("从Mojang获取到皮肤 variant=%1 url=%2")
                                           .arg(skinVariant, skinUrl);
                break;
            }
        }

        // ── Cape extraction (all capes, regardless of state) ──
        m_capes.clear();
        m_activeCapeAlias.clear();
        QStringList capeUrls;
        QStringList capeAliases;
        QStringList capeIds;
        QList<bool> capeActiveFlags;
        for (const QJsonValue& cv : capes) {
            QJsonObject c = cv.toObject();
            QString alias = c[QStringLiteral("alias")].toString();
            QString state = c[QStringLiteral("state")].toString();
            QString url = c[QStringLiteral("url")].toString();
            QString id = c[QStringLiteral("id")].toString();
            bool active = (state == QStringLiteral("ACTIVE"));
            qCInfo(logAccount) << QStringLiteral("披风条目 alias=%1 state=%2 id=%3 url=%4")
                                       .arg(alias, state, id, url);
            capeUrls.append(url);
            capeAliases.append(alias);
            capeIds.append(id);
            capeActiveFlags.append(active);
            if (active) {
                m_activeCapeAlias = alias;
            }
        }

        if (capeUrls.isEmpty()) {
            qCInfo(logAccount) << QStringLiteral("Mojang档案中无披风");
            emit capesReady();
        } else {
            qCInfo(logAccount) << QStringLiteral("从Mojang获取到披风 总数=%1 ACTIVE=%2")
                                       .arg(capeUrls.size()).arg(m_activeCapeAlias);
        }

        // ── Download ALL capes ──
        if (!capeUrls.isEmpty()) {
            QDir().mkpath(m_dataDir + QStringLiteral("/assets/capes"));
            for (int i = 0; i < capeUrls.size(); ++i) {
                const QString& url = capeUrls[i];
                const QString& alias = capeAliases[i];
                const QString& id = capeIds[i];
                bool active = capeActiveFlags[i];
                QNetworkRequest capeReq(url);
                QNetworkReply* capeReply = HttpClient::instance().getRaw(capeReq);
                connect(capeReply, &QNetworkReply::finished, this, [this, capeReply, alias, id, active]() {
                    capeReply->deleteLater();
                    if (capeReply->error() == QNetworkReply::NoError) {
                        QString cachePath = capeCachePath(m_username + QStringLiteral("_") + alias);
                        QFile file(cachePath);
                        if (file.open(QIODevice::WriteOnly)) {
                            file.write(capeReply->readAll());
                            file.close();
                            QString localUrl = toImageUrl(cachePath);

                            // Add to cape list
                            CapeInfo info;
                            info.alias = alias;
                            info.capeId = id;
                            info.localPath = localUrl;
                            info.active = active;
                            m_capes.append(info);

                            // Primary (active) cape sets m_capePath for avatar
                            if (active) {
                                m_capePath = localUrl;
                            }

                            qint64 capeSize = file.size();
                            qCInfo(logAccount) << QStringLiteral("披风下载完成 alias=%1 active=%2 大小=%3字节 路径=%4")
                                                       .arg(alias).arg(active).arg(capeSize).arg(cachePath);
                            emit capesReady();
                            if (active) emit capeReady();
                        }
                    } else {
                        qCWarning(logAccount) << QStringLiteral("披风下载失败 alias=%1 错误=%2")
                                                      .arg(alias, capeReply->errorString());
                        // Still add to list with empty path
                        CapeInfo info;
                        info.alias = alias;
                        info.capeId = id;
                        info.localPath.clear();
                        info.active = active;
                        m_capes.append(info);
                        emit capesReady();
                    }
                });
            }
        }

        if (!skinCached) {
            if (skinUrl.isEmpty()) {
                qCInfo(logAccount) << QStringLiteral("无Mojang皮肤,使用默认");
                setFallbackSkin();
                emit skinReady();
            } else {
                qCInfo(logAccount) << QStringLiteral("下载Mojang皮肤: ") << skinUrl;
                QNetworkRequest skinReq(skinUrl);
                QNetworkReply* skinReply = HttpClient::instance().getRaw(skinReq);
                connect(skinReply, &QNetworkReply::finished, this, [this, skinReply, skinVariant]() {
                    skinReply->deleteLater();
                    if (skinReply->error() != QNetworkReply::NoError) {
                        qCWarning(logAccount) << QStringLiteral("皮肤纹理下载失败 错误=%1").arg(skinReply->errorString());
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
                        QString freshFace = facePath.isEmpty() ? cachePath : facePath;
                        // Append cache buster for QML Image to re-read on same path
                        m_skinPath = toImageUrl(freshFace)
                            + QStringLiteral("?t=%1").arg(QDateTime::currentMSecsSinceEpoch());
                        qint64 skinSize = file.size();
                        qCInfo(logAccount) << QStringLiteral("皮肤下载完成 variant=%1 大小=%2字节 路径=%3")
                                                   .arg(skinVariant, QString::number(skinSize), m_skinPath);
                    }
                    emit skinReady();
                });
            }
        } else {
            qCInfo(logAccount) << QStringLiteral("皮肤已缓存,跳过纹理下载");
        }
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

void AccountBackend::removeOfflineUsername(const QString &username)
{
    m_offlineUsernames.removeAll(username);
    saveOfflineHistory();
    emit offlineHistoryChanged();
}

// ────────────────────────────────────────────────────────────
// Microsoft Session Persistence
// ────────────────────────────────────────────────────────────

void AccountBackend::saveMicrosoftSession()
{
    QJsonObject obj;
    obj[QStringLiteral("username")] = m_username;
    obj[QStringLiteral("uuid")] = TokenCrypto::encrypt(m_uuid);
    obj[QStringLiteral("mcToken")] = TokenCrypto::encrypt(m_msMcToken);
    obj[QStringLiteral("refreshToken")] = TokenCrypto::encrypt(m_msRefreshToken);
    obj[QStringLiteral("tokenObtainedAt")] = TokenCrypto::encrypt(QString::number(m_msTokenObtainedAt));
    obj[QStringLiteral("tokenExpiresIn")] = TokenCrypto::encrypt(QString::number(m_msTokenExpiresIn));
    obj[QStringLiteral("encrypted")] = true;  // mark for migration detection

    QString path = m_dataDir + QStringLiteral("/microsoft_session.json");
    QFileInfo fi(path);
    QDir().mkpath(fi.absolutePath());

    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
        file.close();
        qCInfo(logAccount) << QStringLiteral("微软会话已保存: ") << path;
    }
}

void AccountBackend::deleteMicrosoftSession()
{
    QString path = m_dataDir + QStringLiteral("/microsoft_session.json");
    QFile::remove(path);
    qCInfo(logAccount) << QStringLiteral("微软会话已删除: ") << path;
}

// Helper: read a field from JSON, auto-decrypt if encrypted
static QString readSecureField(const QJsonObject &obj, const QString &key, bool isEncrypted)
{
    QString val = obj[key].toString();
    if (val.isEmpty()) return {};
    if (isEncrypted || TokenCrypto::looksEncrypted(val)) {
        QString dec = TokenCrypto::decrypt(val);
        if (!dec.isEmpty()) return dec;
        // Decrypt failed — fall through to plaintext for migration
        qCInfo(logAccount) << QStringLiteral("解密失败,尝试明文: ") << key;
    }
    return val;
}

void AccountBackend::loadMicrosoftSession()
{
    QString path = m_dataDir + QStringLiteral("/microsoft_session.json");
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    QJsonObject obj = doc.object();
    bool isEncrypted = obj[QStringLiteral("encrypted")].toBool(false);

    m_username = obj[QStringLiteral("username")].toString();
    m_uuid = readSecureField(obj, QStringLiteral("uuid"), isEncrypted);
    m_msMcToken = readSecureField(obj, QStringLiteral("mcToken"), isEncrypted);
    m_msRefreshToken = readSecureField(obj, QStringLiteral("refreshToken"), isEncrypted);
    m_msTokenObtainedAt = readSecureField(obj, QStringLiteral("tokenObtainedAt"), isEncrypted).toLongLong();
    m_msTokenExpiresIn = readSecureField(obj, QStringLiteral("tokenExpiresIn"), isEncrypted).toLongLong();

    // Migration: if old plaintext file was loaded, re-save as encrypted
    if (!isEncrypted && !m_msRefreshToken.isEmpty()) {
        qCInfo(logAccount) << QStringLiteral("迁移明文会话至加密存储");
        saveMicrosoftSession();
    }

    qint64 now = QDateTime::currentSecsSinceEpoch();
    bool knownExpired = (m_msTokenExpiresIn > 0) && (m_msTokenObtainedAt > 0)
                        && (now > m_msTokenObtainedAt + m_msTokenExpiresIn);

    // Session loaded
    // Strategy: trust timestamps for display, verify on pre-launch
    if (!m_msRefreshToken.isEmpty() && !knownExpired) {
        qCInfo(logAccount) << QStringLiteral("找到微软会话: ") << m_username;
        m_loggedIn = true;
        m_isOnline = true;
        downloadSkin(m_username);
        emit accountChanged();
    } else if (m_msRefreshToken.isEmpty() && !m_msMcToken.isEmpty()) {
        // MC token without refresh token = incomplete session, can't refresh
        qCInfo(logAccount) << QStringLiteral("会话无刷新令牌,已清除");
        m_msMcToken.clear();
        m_username.clear();
        m_uuid.clear();
        // Do NOT save — don't overwrite file with empty data
    } else if (m_msRefreshToken.isEmpty() && m_msMcToken.isEmpty()) {
        // Empty session — corrupted or first run, silently skip
        qCInfo(logAccount) << QStringLiteral("会话为空,重新开始");
    } else if (knownExpired) {
        if (!m_msRefreshToken.isEmpty()) {
            // MC token expired, but we have a refresh token (valid up to 90 days)
            qCInfo(logAccount) << QStringLiteral("MC令牌过期,使用刷新令牌...");
            m_loggedIn = true;
            m_isOnline = true;
            downloadSkin(m_username);
            emit accountChanged();
            refreshMicrosoftToken();
            startBackgroundRefresh();
        } else {
            // Both tokens expired — truly need re-login
            qCInfo(logAccount) << QStringLiteral("微软会话已过期,已清除");
            m_msMcToken.clear();
            m_msRefreshToken.clear();
            m_msTokenObtainedAt = 0;
            m_msTokenExpiresIn = 0;
            m_username.clear();
            m_uuid.clear();
            m_loggedIn = false;
            m_isOnline = false;
            saveMicrosoftSession();
            emit accountChanged();
        }
    }
}

void AccountBackend::refreshMicrosoftToken()
{
    if (m_msRefreshToken.isEmpty()) return;
    if (m_refreshingToken) {
        qCInfo(logAccount) << QStringLiteral("令牌刷新中,跳过重复请求");
        return;
    }
    qCInfo(logAccount) << QStringLiteral("正在刷新微软令牌...");
    m_refreshingToken = true;

    QUrlQuery postData;
        postData.addQueryItem(QStringLiteral("client_id"), QStringLiteral("YOUR_CLIENT_ID_HERE"));
    postData.addQueryItem(QStringLiteral("refresh_token"), m_msRefreshToken);
    postData.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
    postData.addQueryItem(QStringLiteral("redirect_uri"), QStringLiteral("http://localhost"));

    QByteArray body = postData.toString(QUrl::FullyEncoded).toUtf8();

    QNetworkRequest req(QUrl(QStringLiteral("https://login.microsoftonline.com/consumers/oauth2/v2.0/token")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    req.setTransferTimeout(10000);

    QNetworkReply* reply = HttpClient::instance().post(req, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        QByteArray raw = reply->readAll();
        QJsonObject obj = QJsonDocument::fromJson(raw).object();

        if (reply->error() != QNetworkReply::NoError || !obj[QStringLiteral("access_token")].isString()) {
            m_refreshingToken = false;

            // Distinguish token-expired from network/server errors
            bool tokenExpired = false;
            QString reason;

            if (!obj.isEmpty()) {
                QString err = obj[QStringLiteral("error")].toString();
                if (err == QStringLiteral("invalid_grant")) {
                    tokenExpired = true;
                    reason = tr("令牌已过期");
                }
            }

            if (!tokenExpired) {
                QNetworkReply::NetworkError netErr = reply->error();
                if (netErr == QNetworkReply::TimeoutError ||
                    netErr == QNetworkReply::ConnectionRefusedError ||
                    netErr == QNetworkReply::HostNotFoundError ||
                    netErr == QNetworkReply::TemporaryNetworkFailureError) {
                    reason = tr("网络异常，令牌刷新失败");
                } else if (netErr != QNetworkReply::NoError) {
                    int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                    if (httpStatus >= 500) {
                        reason = tr("服务器错误 (%1)，令牌刷新失败").arg(httpStatus);
                    } else {
                        reason = tr("令牌刷新失败");
                    }
                }
            }

            if (reason.isEmpty()) reason = tr("令牌刷新失败");
            qCWarning(logAccount) << QStringLiteral("Microsoft Token刷新失败 原因=%1 响应=%2").arg(reason, raw.left(200));

            emit tokenRefreshed(false);
            emit tokenRefreshFailed(tokenExpired, reason);

            if (tokenExpired) {
                // Only prompt re-login when token is genuinely expired
                emit logMessage(tr("正版登录已过期，请重新登录"));
            } else {
                emit logMessage(tr("令牌刷新暂时失败（%1），将在下次启动时重试").arg(reason));
            }
            return;
        }

        m_msRefreshToken = obj[QStringLiteral("refresh_token")].toString(m_msRefreshToken);
        QString newAccessToken = obj[QStringLiteral("access_token")].toString();
        qCInfo(logAccount) << QStringLiteral("微软令牌已刷新,获取MC令牌...");

        // Continue the chain: MS access_token → XBL → XSTS → Minecraft token
        // m_refreshingToken already true from function entry
        // Don't set m_loggedIn here — wait for MC chain to complete via loginSuccess/loginFailed
        m_msAuth->refreshMcChain(newAccessToken);
    });
}

} // namespace ShadowLauncher
