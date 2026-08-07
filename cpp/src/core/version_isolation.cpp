// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "version_isolation.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include "../utils/logger.h"
#include <QJsonObject>

namespace ShadowLauncher {

// ============================================================
// Constructor
// ============================================================

VersionIsolation::VersionIsolation(QObject* parent)
    : QObject(parent)
    , m_enabled(true)
{
}

// ============================================================
// Game directory
// ============================================================

void VersionIsolation::setGameDir(const QString& dir)
{
    if (m_gameDir != dir) {
        m_gameDir = dir;
        loadConfig();
    }
}

// ============================================================
// Config path
// ============================================================

QString VersionIsolation::configPath() const
{
    return m_gameDir + QStringLiteral("/config/version_isolation.json");
}

// ============================================================
// Load / Save config
// ============================================================

void VersionIsolation::loadConfig()
{
    if (m_gameDir.isEmpty()) return;

    const QString path = configPath();
    QFile file(path);
    if (!file.exists()) {
        // Defaults
        m_enabled = true;
        m_isolatedVersions.clear();
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseErr);
    file.close();

    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        // Corrupted config, use defaults
        m_enabled = true;
        m_isolatedVersions.clear();
        return;
    }

    QJsonObject root = doc.object();
    m_enabled = root.value(QStringLiteral("enabled")).toBool(true);

    m_isolatedVersions.clear();
    const QJsonArray arr = root.value(QStringLiteral("isolated_versions")).toArray();
    for (const auto& v : arr) {
        if (v.isString()) {
            m_isolatedVersions.append(v.toString());
        }
    }
}

void VersionIsolation::saveConfig()
{
    if (m_gameDir.isEmpty()) return;

    QDir().mkpath(m_gameDir + QStringLiteral("/config"));

    QJsonObject root;
    root[QStringLiteral("enabled")] = m_enabled;

    QJsonArray arr;
    for (const QString& vid : m_isolatedVersions) {
        arr.append(vid);
    }
    root[QStringLiteral("isolated_versions")] = arr;

    QJsonDocument doc(root);
    QFile file(configPath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
    }
}

// ============================================================
// Properties
// ============================================================

bool VersionIsolation::isEnabled() const
{
    return m_enabled;
}

void VersionIsolation::setEnabled(bool enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;
        saveConfig();
        emit isolationChanged();
    }
}

// ============================================================
// Isolated versions
// ============================================================

bool VersionIsolation::isVersionIsolated(const QString& versionId) const
{
    // If global isolation is enabled, all versions are considered isolated
    if (m_enabled) return true;

    // Otherwise check the .isolated marker file
    const QString marker = m_gameDir + QStringLiteral("/versions/")
                           + versionId + QStringLiteral("/.isolated");
    return QFileInfo::exists(marker);
}

// ── 检查目录是否非空（排除旧代码 mkpath 产物）──
// ============================================================
// Get version game directory
// ============================================================

QString VersionIsolation::getVersionGameDir(const QString& versionId) const
{
    if (m_gameDir.isEmpty()) return {};

    if (m_enabled) {
        const QString verDir = m_gameDir + QStringLiteral("/versions/")
                               + versionId;
        const QString gameDir = verDir + QStringLiteral("/game");

        // 隔离模式统一返回 game/（不存在则创建）——消除"降级到版本根目录"分支：
        // 降级会让 launcher 的 .minecraft junction 逻辑 cdUp 过头，junction 建到
        // versions/.minecraft → 版本目录（2026-08-07 实测 1.4.7 启动时 versions 根
        // 出现 <JUNCTION> .minecraft → versions/1.4.7-forge-... 且 MC 找不到主类崩溃）
        QDir().mkpath(gameDir);
        return gameDir;
    }

    // Shared mode: all versions use the base game directory
    return m_gameDir;
}

// ============================================================
// Migrate version to isolated
// ============================================================

bool VersionIsolation::migrateToIsolated(const QString& versionId)
{
    const QString verDir = m_gameDir + QStringLiteral("/versions/") + versionId;
    const QString jsonFile = verDir + QStringLiteral("/") + versionId + QStringLiteral(".json");

    if (!QFileInfo::exists(jsonFile)) {
        return false;
    }

    // Create the game/ subdirectory
    const QString gameDir = verDir + QStringLiteral("/game");
    QDir().mkpath(gameDir);

    // Create .isolated marker
    const QString marker = verDir + QStringLiteral("/.isolated");
    if (!QFileInfo::exists(marker)) {
        QFile f(marker);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write("isolated");
            f.close();
        }
    }

    // Update config
    if (!m_isolatedVersions.contains(versionId)) {
        m_isolatedVersions.append(versionId);
        saveConfig();
    }

    return true;
}

} // namespace ShadowLauncher
