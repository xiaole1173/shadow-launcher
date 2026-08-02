// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
// Shadow Launcher — Version Manager
// Phase 2: Core — Fetches Minecraft version manifest from Mojang API

#include "version_manager.h"
#include "../utils/logger.h"

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

// nlohmann/json — parse Mojang official JSON (pure C++ parsing, avoid QJsonDocument perf issues with large files)
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
    // 1) Try cache synchronously on calling thread (main thread during startup).
    //    Reading/parsing ~900 versions takes ~10ms — negligible vs the 9–18s
    //    queued-connection delay that background-thread + invokeMethod caused.
    qCInfo(logMgr) << QStringLiteral("[版本管理] 获取版本列表 优先使用缓存");
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
        qCInfo(logMgr) << QStringLiteral("[版本管理] 缓存命中 解析=%1个版本 路径=%2").arg(parsed.size()).arg(path);
        m_versions = parsed;
        emit versionsReady(m_versions);
        return;
    }

    // 2) Cache miss — fetch from network (must run on main thread for QNetworkAccessManager)
    qCInfo(logMgr) << QStringLiteral("[版本管理] 缓存未命中 发起网络请求");
    QMetaObject::invokeMethod(this, [this]() {
        doNetworkFetch();
    }, Qt::QueuedConnection);
}

void VersionManager::doNetworkFetch()
{
    // 按下载源策略选主源：官方优先时 launchermeta 在前，否则 BMCLAPI 在前（另一源兜底）
    const char* primary = m_preferOfficial ? FALLBACK_URL : PRIMARY_URL;
    const char* fallback = m_preferOfficial ? PRIMARY_URL : FALLBACK_URL;

    QUrl primaryUrl(primary);
    QNetworkRequest request(primaryUrl);
    request.setRawHeader("User-Agent", QString::fromLatin1(USER_AGENT).toUtf8());
    request.setTransferTimeout(8000);  // 8s timeout for primary

    qCInfo(logMgr) << QStringLiteral("[版本管理] 网络请求 主源=%1 兜底=%2").arg(primary, fallback);
    QNetworkReply* reply = m_nam->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply, fallback]() {
        reply->deleteLater();

        // Check if primary succeeded
        if (reply->error() == QNetworkReply::NoError) {
            const QByteArray data = reply->readAll();
            QVector<McVersion> parsed = parseManifest(data);
            if (!parsed.isEmpty()) {
                m_versions = parsed;
                saveToCache();
                qCInfo(logMgr) << QStringLiteral("[版本管理] 主源获取成功 版本数=%1").arg(parsed.size());
                emit versionsReady(m_versions);
                return;
            }
        }

        // Primary failed — fallback to the other source（按策略选定的兜底源）
        qCWarning(logMgr) << QStringLiteral("[版本管理] 主源失败 错误=%1 回退=%2").arg(reply->errorString(), QString::fromLatin1(fallback));
        QUrl fallbackUrl(fallback);
        QNetworkRequest mirrorReq(fallbackUrl);
        mirrorReq.setRawHeader("User-Agent", QString::fromLatin1(USER_AGENT).toUtf8());
        mirrorReq.setTransferTimeout(15000);

        qCInfo(logMgr) << QStringLiteral("[版本管理] 网络请求 回退源=%1").arg(QString::fromLatin1(fallback));
        QNetworkReply* mirrorReply = m_nam->get(mirrorReq);
        connect(mirrorReply, &QNetworkReply::finished, this, [this, mirrorReply]() {
            mirrorReply->deleteLater();
            if (mirrorReply->error() != QNetworkReply::NoError) {
                qCCritical(logMgr) << QStringLiteral("[版本管理] 回退源也失败 错误=%1").arg(mirrorReply->errorString());
                emit fetchError(mirrorReply->errorString());
                return;
            }
            const QByteArray data = mirrorReply->readAll();
            QVector<McVersion> parsed = parseManifest(data);
            if (parsed.isEmpty()) {
                qCCritical(logMgr) << QStringLiteral("[版本管理] 回退源返回无法解析的数据");
                emit fetchError(QStringLiteral("Failed to parse version manifest"));
                return;
            }
            m_versions = parsed;
            saveToCache();
            qCInfo(logMgr) << QStringLiteral("[版本管理] 回退源成功 版本数=%1").arg(parsed.size());
            emit versionsReady(m_versions);
        });
    });
}

// ============================================================
// Sync fetch
// ============================================================

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

// ============================================================
// Latest version
// ============================================================

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
    qCInfo(logMgr) << QStringLiteral("[版本管理] 缓存已加载 版本数=%1 路径=%2").arg(parsed.size()).arg(path);
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
        qCInfo(logMgr) << QStringLiteral("[版本管理] 缓存已保存 版本数=%1 路径=%2").arg(m_versions.size()).arg(path);
    }
}

} // namespace ShadowLauncher
