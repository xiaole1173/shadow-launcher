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
    // 外部目录只读模式：绝不向导入的 .minecraft 写入版本隔离配置（不破坏原结构）
    if (m_folderLayout != MinecraftLayout::Unknown) return;

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
    // 外部目录：按布局判断（2026-08-19）—— 不依赖本启动器全局隔离开关。
    // 若用户全局开关为关 + 外部目录实际是隔离形态，此前会误判为"非隔离"，
    // 导致加载器安装（OptiFine 等）把 mod 写到根目录 mods/ 而非
    // versions/<id>/mods → 游戏（从版本目录运行）读不到该 mod。
    // 规则：游戏数据目录位于版本目录内（含 legacy game/ 子目录）→ 视为隔离。
    if (m_folderLayout != MinecraftLayout::Unknown) {
        const QString gd = QDir::toNativeSeparators(QDir::cleanPath(getVersionGameDir(versionId)));
        const QString verDir = QDir::toNativeSeparators(
            QDir::cleanPath(m_gameDir + QStringLiteral("/versions/") + versionId));
        const QString gameSub = QDir::toNativeSeparators(
            QDir::cleanPath(verDir + QStringLiteral("/game")));
        return !gd.isEmpty()
            && (gd == verDir || gd == gameSub || gd.startsWith(verDir + QLatin1Char('\\')));
    }

    // If global isolation is enabled, all versions are considered isolated
    if (m_enabled) return true;

    // Otherwise check the .isolated marker file
    const QString marker = m_gameDir + QStringLiteral("/versions/")
                           + versionId + QStringLiteral("/.isolated");
    return QFileInfo::exists(marker);
}

void VersionIsolation::markForeignIsolated(const QString& versionId)
{
    // 仅外部模式：共享目录内新装版本打 .isolated 标记 → resolveVersionGameDir
    // 优先识别为隔离（游戏数据落 versions/<id>，不污染共享根目录）。
    // 该标记是本启动器的辅助文件，不影响原启动器对目录的解析。
    if (m_folderLayout == MinecraftLayout::Unknown || versionId.isEmpty()) return;

    const QString verDir = m_gameDir + QStringLiteral("/versions/") + versionId;
    QDir().mkpath(verDir);
    const QString marker = verDir + QStringLiteral("/.isolated");
    QFile f(marker);
    if (!QFileInfo::exists(marker)) {
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write("isolated");
            f.close();
        }
    }
}

// ── 检查目录是否非空（排除旧代码 mkpath 产物）──
static bool isDirNonEmpty(const QString& path)
{
    QDir dir(path);
    return dir.exists()
        && !dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty();
}

// ============================================================
// Get version game directory
// ============================================================

QString VersionIsolation::getVersionGameDir(const QString& versionId) const
{
    if (m_gameDir.isEmpty()) return {};

    // 外部目录：布局感知解析（版本隔离/非隔离均正确），不强制本启动器的隔离形态
    if (m_folderLayout != MinecraftLayout::Unknown) {
        return resolveVersionGameDir(m_gameDir, versionId, m_folderLayout);
    }

    if (m_enabled) {
        const QString verDir = m_gameDir + QStringLiteral("/versions/")
                               + versionId;
        const QString gameDir = verDir + QStringLiteral("/game");

        // 返回实际有内容的位置（2026-08-07 修正）：绝不 mkpath 创建空 game/——
        // 旧版本/多数安装是散文件布局（数据在版本文件夹根目录），强制指向新建的
        // 空 game/ 会让 MC 在空目录首次启动 → Narrator 界面 + config 全空壳
        // （fml.toml "is not correct. Correcting"）+ 存档丢失感。
        // 规则：game/ 非空 → game/；否则 → 版本文件夹根（散文件布局）。
        if (isDirNonEmpty(gameDir))
            return gameDir;
        return verDir;
    }

    // Shared mode: all versions use the base game directory
    return m_gameDir;
}

// ============================================================
// Migrate version to isolated
// ============================================================

bool VersionIsolation::migrateToIsolated(const QString& versionId)
{
    // 外部目录只读模式：不允许迁移（会创建 game/ 与 .isolated 标记，破坏原结构）
    if (m_folderLayout != MinecraftLayout::Unknown) {
        return false;
    }

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
