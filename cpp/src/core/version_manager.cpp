// Shadow Launcher 鈥?Version Manager
// Phase 2: Core 鈥?Fetches Minecraft version manifest from Mojang API

#include "version_manager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QEventLoop>
#include <QNetworkRequest>
#include <QUrl>
#include <QDateTime>
#include <future>

// nlohmann/json 鈥?parse Mojang official JSON (pure C++ parsing, avoid QJsonDocument perf issues with large files)
#include "../third_party/nlohmann/json.hpp"

namespace ShadowLauncher {

// ============================================================
// Constants
// ============================================================

static const char* PRIMARY_URL = "https://bmclapi2.bangbang93.com/mc/game/version_manifest.json";
static const char* FALLBACK_URL = "https://launchermeta.mojang.com/mc/game/version_manifest.json";

static const char* CACHE_FILENAME = "versions.json";

static const char* USER_AGENT = "ShadowLauncher/1.0";

// ============================================================
// Construction / Destruction
// ============================================================

VersionManager::VersionManager(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

VersionManager::~VersionManager() = default;

// ============================================================
// Network request (async)
// ============================================================

void VersionManager::fetchVersions()
{
    // 0) If already loaded (cache or previous fetch), emit immediately — no re-parse
    if (!m_versions.isEmpty()) {
        emit versionsReady(m_versions);
        return;
    }

    // 1) Try cache in background thread — 1000+ version JSON parsing blocks main thread
    m_cacheLoadFuture = std::async(std::launch::async, [this]() {
        // Do all heavy work (file I/O + JSON parse) on background thread
        QVector<McVersion> parsed;
        const QString path = cacheFilePath();
        if (!path.isEmpty()) {
            QFile file(path);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                const QByteArray data = file.readAll();
                file.close();
                parsed = parseManifest(data);
            }
        }
        if (!parsed.isEmpty()) {
            // Assign m_versions on main thread to avoid data race
            QMetaObject::invokeMethod(this, [this, parsed]() {
                m_versions = parsed;
                emit versionsReady(m_versions);
            }, Qt::QueuedConnection);
            return;
        }
        // Cache miss — trigger network fetch on main thread
        QMetaObject::invokeMethod(this, [this]() {
            doNetworkFetch();
        }, Qt::QueuedConnection);
    });
}

void VersionManager::doNetworkFetch()
{
    // 2) Request primary source (BMCLAPI) with Mojang fallback
    QUrl primaryUrl(PRIMARY_URL);
    QNetworkRequest request(primaryUrl);
    request.setRawHeader("User-Agent", QString::fromLatin1(USER_AGENT).toUtf8());
    request.setTransferTimeout(8000);  // 8s timeout for primary

    QNetworkReply* reply = m_nam->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        // Check if primary succeeded
        if (reply->error() == QNetworkReply::NoError) {
            const QByteArray data = reply->readAll();
            QVector<McVersion> parsed = parseManifest(data);
            if (!parsed.isEmpty()) {
                m_versions = parsed;
                saveToCache();
                emit versionsReady(m_versions);
                return;
            }
        }

        // Primary failed — fallback to Mojang official
        QUrl fallbackUrl(FALLBACK_URL);
        QNetworkRequest mirrorReq(fallbackUrl);
        mirrorReq.setRawHeader("User-Agent", QString::fromLatin1(USER_AGENT).toUtf8());
        mirrorReq.setTransferTimeout(15000);

        QNetworkReply* mirrorReply = m_nam->get(mirrorReq);
        connect(mirrorReply, &QNetworkReply::finished, this, [this, mirrorReply]() {
            mirrorReply->deleteLater();
            if (mirrorReply->error() != QNetworkReply::NoError) {
                emit fetchError(mirrorReply->errorString());
                return;
            }
            const QByteArray data = mirrorReply->readAll();
            QVector<McVersion> parsed = parseManifest(data);
            if (parsed.isEmpty()) {
                emit fetchError(QStringLiteral("Failed to parse version manifest"));
                return;
            }
            m_versions = parsed;
            saveToCache();
            emit versionsReady(m_versions);
        });
    });
}

// ============================================================
// Sync fetch
// ============================================================

QVector<McVersion> VersionManager::fetchVersionsSync()
{
    // Check cache first
    if (!m_versions.isEmpty())
        return m_versions;

    if (loadFromCache())
        return m_versions;

    QVector<McVersion> result;

    QNetworkAccessManager nam;
    QUrl syncUrl(PRIMARY_URL);
    QNetworkRequest request(syncUrl);
    request.setRawHeader("User-Agent", QString::fromLatin1(USER_AGENT).toUtf8());
    request.setTransferTimeout(15000);

    QNetworkReply* reply = nam.get(request);

    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();  // Block until done

    if (reply->error() == QNetworkReply::NoError) {
        result = parseManifest(reply->readAll());
        m_versions = result;
        saveToCache();
    }

    reply->deleteLater();
    return result;
}

// ============================================================
// Mojang JSON parsing
// ============================================================

QVector<McVersion> VersionManager::parseManifest(const QByteArray& rawJson) const
{
    QVector<McVersion> out;

    try {
        nlohmann::json root = nlohmann::json::parse(
            rawJson.constData(),
            rawJson.constData() + rawJson.size()
        );

        // Mojang structure:
        // {
        //   "latest": {"release": "1.21.5", "snapshot": "25w16a"},
        //   "versions": [
        //     {"id": "1.21.5", "type": "release", "url": "...",
        //      "time": "2026-04-01T...", "releaseTime": "2026-04-01T..."}
        //   ]
        // }

        if (!root.contains("versions") || !root["versions"].is_array()) {
            return out;
        }

        const auto& versions = root["versions"];

        for (const auto& v : versions) {
            if (!v.contains("id") || !v.contains("type"))
                continue;

            McVersion mv;
            mv.id = QString::fromStdString(v["id"].get<std::string>());
            mv.type = QString::fromStdString(v["type"].get<std::string>());

            if (v.contains("url"))
                mv.url = QString::fromStdString(v["url"].get<std::string>());

            if (v.contains("releaseTime")) {
                const QString rt = QString::fromStdString(
                    v["releaseTime"].get<std::string>()
                );
                mv.releaseTime = QDateTime::fromString(rt, Qt::ISODate);
            }

            // Check if installed
            mv.installed = isInstalled(mv.id);
            if (mv.installed)
                mv.installPath = getVersionPath(mv.id);

            out.append(mv);
        }
    } catch (const nlohmann::json::exception&) {
        // JSON parse failed, return empty list
        return {};
    } catch (const std::exception&) {
        return {};
    }

    return out;
}

// ============================================================
// Filter by type
// ============================================================

QVector<McVersion> VersionManager::getByType(const QString& type) const
{
    QVector<McVersion> result;
    for (const auto& v : m_versions) {
        if (v.type == type)
            result.append(v);
    }
    return result;
}

// ============================================================
// Latest version
// ============================================================

McVersion VersionManager::getLatest(const QString& type) const
{
    // Find the one with the most recent releaseTime
    const McVersion* latest = nullptr;

    for (const auto& v : m_versions) {
        if (v.type != type)
            continue;
        if (!latest || v.releaseTime > latest->releaseTime)
            latest = &v;
    }

    if (latest)
        return *latest;

    // No match: return empty McVersion
    McVersion empty;
    empty.type = type;
    return empty;
}

// ============================================================
// Installation check
// ============================================================

bool VersionManager::isInstalled(const QString& versionId) const
{
    if (m_gameDir.isEmpty())
        return false;

    // Check if {gameDir}/versions/{versionId}/{versionId}.json exists
    const QString jsonPath = getVersionPath(versionId) +
                             QStringLiteral("/") +
                             versionId +
                             QStringLiteral(".json");

    return QFileInfo::exists(jsonPath);
}

// ============================================================
// Version path
// ============================================================

QString VersionManager::getVersionPath(const QString& versionId) const
{
    // {gameDir}/versions/{versionId}/
    return m_gameDir +
           QStringLiteral("/versions/") +
           versionId;
}

// ============================================================
// Cache I/O
// ============================================================

QString VersionManager::cacheFilePath() const
{
    if (m_dataDir.isEmpty())
        return {};

    return m_dataDir + QStringLiteral("/") + CACHE_FILENAME;
}

bool VersionManager::loadFromCache()
{
    const QString path = cacheFilePath();
    if (path.isEmpty())
        return false;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    const QByteArray data = file.readAll();
    file.close();

    QVector<McVersion> parsed = parseManifest(data);
    if (parsed.isEmpty())
        return false;

    m_versions = parsed;
    return true;
}

void VersionManager::saveToCache() const
{
    const QString path = cacheFilePath();
    if (path.isEmpty() || m_versions.isEmpty())
        return;

    // Ensure directory exists
    QFileInfo fi(path);
    QDir().mkpath(fi.absolutePath());

    // Build JSON
    nlohmann::json root;
    nlohmann::json versionsArray = nlohmann::json::array();

    for (const auto& v : m_versions) {
        nlohmann::json entry;
        entry["id"] = v.id.toStdString();
        entry["type"] = v.type.toStdString();
        entry["url"] = v.url.toStdString();
        if (v.releaseTime.isValid()) {
            entry["releaseTime"] = v.releaseTime.toString(Qt::ISODate).toStdString();
        }
        versionsArray.push_back(entry);
    }

    root["versions"] = versionsArray;

    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        const std::string dump = root.dump(2);  // 2-space indent
        file.write(dump.data(), static_cast<qint64>(dump.size()));
        file.close();
    }
}

} // namespace ShadowLauncher
