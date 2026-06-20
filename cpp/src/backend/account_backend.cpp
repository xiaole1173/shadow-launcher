#include "account_backend.h"
#include "../utils/logger.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

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

    // ── MicrosoftAuth ──
    m_msAuth = new MicrosoftAuth(this);
    connect(m_msAuth, &MicrosoftAuth::loginProgress, this, [this](const QString& step, const QString& detail) {
        m_msStatus = step + QStringLiteral(": ") + detail;
        emit microsoftLoginProgress(step, detail);
    });
    connect(m_msAuth, &MicrosoftAuth::authUrlReady, this, [this](const QString& url) {
        m_msAuthUrl = url;
        emit microsoftAuthUrlReady(url);
        emit logMessage(QStringLiteral("[MSA] 浏览器已打开，请登录后复制地址栏 URL 粘贴"));
    });
    connect(m_msAuth, &MicrosoftAuth::loginSuccess, this, [this](const QString& mcToken, const QString& username, const QString& uuid, const QString& refreshToken) {
        m_msMcToken = mcToken;
        m_msRefreshToken = refreshToken;
        m_username = username;
        m_uuid = uuid;
        m_loggedIn = true;
        m_isOnline = true;
        m_msStatus.clear();
        m_msAuthUrl.clear();
        qCInfo(logAccount) << "Microsoft login SUCCESS:" << username << uuid;
        emit microsoftLoginSuccess(username, uuid);
        emit accountChanged();
        emit logMessage(QStringLiteral("正版登录成功: %1").arg(username));
    });
    connect(m_msAuth, &MicrosoftAuth::loginFailed, this, [this](const QString& error) {
        m_msStatus.clear();
        m_msAuthUrl.clear();
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

    // Generate offline-mode UUID (Java-compatible)
    m_username = name;
    m_uuid = generateOfflineUuid(name);
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

void AccountBackend::microsoftSubmitCode(const QString& code)
{
    if (!m_msAuth) return;
    m_msAuth->submitCode(code);
}

void AccountBackend::cancelMicrosoftLogin()
{
    if (!m_msAuth) return;
    m_msAuth->cancelLogin();
    emit microsoftLoginBusyChanged();
}

// ────────────────────────────────────────────────────────────
// Skin URL (NameMC face API)
// ────────────────────────────────────────────────────────────

QString AccountBackend::getSkinUrl(const QString &username) const
{
    QString name = username.isEmpty() ? m_username : username;
    if (name.isEmpty())
        return {};
    return QStringLiteral("https://api.namemc.com/skin/%1/face").arg(name);
}

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
// Download Skin
// ────────────────────────────────────────────────────────────

void AccountBackend::downloadSkin(const QString &username)
{
    QString cachePath = skinCachePath(username);

    // ── Check cache first ──
    QFileInfo cacheInfo(cachePath);
    if (cacheInfo.exists() && cacheInfo.size() > 0) {
        m_skinPath = cachePath;
        qCInfo(logAccount) << "Skin loaded from cache:" << cachePath;
        emit skinReady();
        return;
    }

    // ── Offline mode: use placeholder (Steve skin) ──
    if (!m_isOnline) {
        QString placeholder = m_dataDir + QStringLiteral("/assets/steve.png");
        if (QFileInfo::exists(placeholder)) {
            m_skinPath = placeholder;
        }
        qCInfo(logAccount) << "Skin: offline placeholder for" << username;
        emit skinReady();
        return;
    }

    // ── Online mode: fetch from NameMC API ──
    QString url = getSkinUrl(username);
    HttpClient::instance().get(url,
        // success callback
        [this, username, cachePath](int status, const QByteArray &body) {
            if (status == 200 && !body.isEmpty()) {
                // Ensure parent directory exists
                QFileInfo fi(cachePath);
                QDir().mkpath(fi.absolutePath());

                QFile file(cachePath);
                if (file.open(QIODevice::WriteOnly)) {
                    file.write(body);
                    file.close();
                    m_skinPath = cachePath;
                }
                qCInfo(logAccount) << "Skin downloaded:" << username
                                   << "status:" << status;
            } else {
                // Fallback to placeholder
                QString placeholder = m_dataDir + QStringLiteral("/assets/steve.png");
                if (QFileInfo::exists(placeholder)) {
                    m_skinPath = placeholder;
                }
            }
            emit skinReady();
        },
        // error callback
        [this, username](const QString &error) {
            qCWarning(logAccount) << "Skin download failed for" << username << ":" << error;
            QString placeholder = m_dataDir + QStringLiteral("/assets/steve.png");
            if (QFileInfo::exists(placeholder)) {
                m_skinPath = placeholder;
            }
            emit skinReady();
        }
    );
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

} // namespace ShadowLauncher
