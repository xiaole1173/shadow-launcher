// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "minecraft_layout.h"

#include <QDir>
#include <QFileInfo>

namespace ShadowLauncher {

namespace {

// ── 根目录「强」游戏数据标记（共享/非隔离的判据）──
// 刻意排除 resourcepacks/resources/config，见头文件注释。
// 2026-08-18：也排除 logs/ —— 本启动器启动时会在 .minecraft 根目录写 logs/
// （launcher.cpp mkpath(gameDir/logs)），纯隔离目录的根也会出现 logs/，
// 含了会把严格版本隔离的默认目录误判为「混合/共享」。
const char* const kRootMarkers[] = {
    "options.txt", "optionsof.txt",
    "saves", "mods",
    "crash-reports", "screenshots",
    "servers.dat", "usercache.json", "usernamecache.json", "eula.txt",
};

// ── 版本目录游戏数据 / 隔离意图标记（宽集）──
const char* const kVersionMarkers[] = {
    "options.txt", "optionsof.txt",
    "saves", "mods", "config",
    "resourcepacks", "shaderpacks", "resources",
    "logs", "crash-reports", "screenshots",
    "scripts", "structures", "kubejs", "global_packs", "datapacks",
    "server-resource-packs",
    "servers.dat", "server.properties", "eula.txt",
    "usercache.json", "usernamecache.json",
    "PCL",      // PCL2 逐版本隔离标记
    ".hmcl",    // HMCL 逐版本配置标记
};

bool pathExists(const QString& p)
{
    return QFileInfo::exists(p);
}

} // namespace

// ════════════════════════════════════════════════════════════════
// 只读探测
// ════════════════════════════════════════════════════════════════

bool hasRootGameData(const QString& dir)
{
    if (dir.isEmpty()) return false;
    for (const char* marker : kRootMarkers) {
        if (pathExists(dir + QStringLiteral("/") + QLatin1String(marker)))
            return true;
    }
    return false;
}

bool hasVersionGameData(const QString& verDir)
{
    if (verDir.isEmpty()) return false;
    for (const char* marker : kVersionMarkers) {
        if (pathExists(verDir + QStringLiteral("/") + QLatin1String(marker)))
            return true;
    }
    return false;
}

QStringList listVersionIds(const QString& root)
{
    QStringList ids;
    const QDir versionsDir(root + QStringLiteral("/versions"));
    if (!versionsDir.exists()) return ids;

    const QFileInfoList entries = versionsDir.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable);
    for (const QFileInfo& fi : entries) {
        const QString dir = fi.absoluteFilePath();
        // 目录内含版本 json 或 jar 才算版本（过滤 PCL2/HMCL 的无关子目录）
        bool hasJson = false, hasJar = false;
        const QFileInfoList children = QDir(dir).entryInfoList(
            QDir::Files | QDir::NoDotAndDotDot);
        for (const QFileInfo& c : children) {
            const QString name = c.fileName();
            if (name.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) hasJson = true;
            else if (name.endsWith(QStringLiteral(".jar"), Qt::CaseInsensitive)) hasJar = true;
        }
        if (hasJson || hasJar)
            ids.append(fi.fileName());
    }
    return ids;
}

LauncherSource detectLauncherSource(const QString& root, const QStringList& versionIds)
{
    if (pathExists(root + QStringLiteral("/config/version_isolation.json")))
        return LauncherSource::Shadow;

    if (pathExists(root + QStringLiteral("/PCL.ini")))
        return LauncherSource::Pcl2;

    for (const QString& id : versionIds) {
        const QString verDir = root + QStringLiteral("/versions/") + id;
        if (QDir(verDir + QStringLiteral("/PCL")).exists())
            return LauncherSource::Pcl2;
    }
    for (const QString& id : versionIds) {
        const QString verDir = root + QStringLiteral("/versions/") + id;
        if (QDir(verDir + QStringLiteral("/.hmcl")).exists())
            return LauncherSource::Hmcl;
    }

    if (pathExists(root + QStringLiteral("/launcher_profiles.json")))
        return LauncherSource::Official;

    return LauncherSource::Unknown;
}

MinecraftLayout detectLayout(const QString& root, const QStringList& versionIds)
{
    const bool rootData = hasRootGameData(root);
    int isolated = 0;
    for (const QString& id : versionIds) {
        if (hasVersionGameData(root + QStringLiteral("/versions/") + id))
            ++isolated;
    }

    if (rootData && isolated > 0) return MinecraftLayout::Mixed;
    if (rootData)                  return MinecraftLayout::Shared;
    // 根目录无游戏数据：隔离倾向（含空目录——本启动器/PCL2/HMCL 隔离默认）。
    // 只要存在任一自带数据的版本，即确认隔离；否则按隔离默认处理（空目录）。
    return MinecraftLayout::Isolated;
}

MinecraftFolderInfo probeMinecraftFolder(const QString& root)
{
    MinecraftFolderInfo info;
    if (root.trimmed().isEmpty()) {
        info.notes.append(QStringLiteral("未选择目录"));
        return info;
    }

    // 规范化：去除尾部分隔符
    QString canon = QDir::cleanPath(root);
    while (canon.endsWith(QLatin1Char('/')) || canon.endsWith(QLatin1Char('\\')))
        canon.chop(1);
    info.root = QDir::toNativeSeparators(canon);

    if (!QDir(info.root).exists()) {
        info.notes.append(QStringLiteral("目录不存在: ") + info.root);
        return info;
    }

    info.versionIds = listVersionIds(info.root);
    info.totalVersionCount = info.versionIds.size();

    // 有效性：必须存在 versions/ 且至少一个版本
    if (info.totalVersionCount == 0) {
        info.notes.append(QStringLiteral("未发现可识别的版本（versions/ 下没有 .json/.jar）"));
        return info;
    }

    info.rootHasGameData = hasRootGameData(info.root);
    info.isolatedVersionCount = 0;
    for (const QString& id : info.versionIds) {
        if (hasVersionGameData(info.root + QStringLiteral("/versions/") + id))
            ++info.isolatedVersionCount;
    }

    info.layout = detectLayout(info.root, info.versionIds);
    info.launcher = detectLauncherSource(info.root, info.versionIds);
    info.valid = true;

    // 备注（QML 展示用）
    info.notes.append(QStringLiteral("识别为: ") + layoutDisplayName(info.layout));
    if (info.launcher != LauncherSource::Unknown)
        info.notes.append(QStringLiteral("来源: ") + launcherDisplayName(info.launcher));
    if (info.rootHasGameData)
        info.notes.append(QStringLiteral("根目录持有游戏数据（共享/非隔离倾向）"));
    if (info.isolatedVersionCount > 0)
        info.notes.append(QStringLiteral("%1 个版本自带独立游戏数据")
                              .arg(info.isolatedVersionCount));

    return info;
}

// ════════════════════════════════════════════════════════════════
// 游戏目录解析（核心）
// ════════════════════════════════════════════════════════════════

QString resolveVersionGameDir(const QString& root, const QString& versionId,
                              MinecraftLayout layout)
{
    if (root.isEmpty() || versionId.isEmpty()) return {};

    const QString verDir = QDir::cleanPath(root + QStringLiteral("/versions/") + versionId);
    const QString gameSub = verDir + QStringLiteral("/game");

    // 1) Shadow 标准隔离 game/ 布局：game 非空 → 该目录
    QDir gd(gameSub);
    if (gd.exists() && !gd.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty())
        return QDir::toNativeSeparators(gameSub);

    // 2) 散文件隔离布局（PCL2/HMCL/Shadow 散装）：版本目录自带游戏数据 → 版本目录
    if (hasVersionGameData(verDir))
        return QDir::toNativeSeparators(verDir);

    // 2.5) 本启动器新装版本标记（2026-08-19）：外部共享目录内新装的版本带
    //      .isolated 标记 → 强制隔离（游戏数据落版本目录，不污染共享根目录）。
    //      已有共享版本无此标记，走规则 3 保持共享。
    if (QFileInfo::exists(verDir + QStringLiteral("/.isolated")))
        return QDir::toNativeSeparators(verDir);

    // 3) 版本目录为空：按文件夹整体形态决定
    if (layout == MinecraftLayout::Shared || layout == MinecraftLayout::Mixed)
        return QDir::toNativeSeparators(QDir::cleanPath(root));  // 非隔离 → 共享根目录

    // Isolated（含 Unknown 默认走隔离安全侧）→ 版本目录
    return QDir::toNativeSeparators(verDir);
}

// ════════════════════════════════════════════════════════════════
// 显示名
// ════════════════════════════════════════════════════════════════

QString layoutDisplayName(MinecraftLayout layout)
{
    switch (layout) {
    case MinecraftLayout::Isolated: return QStringLiteral("版本隔离");
    case MinecraftLayout::Shared:   return QStringLiteral("共享（非隔离）");
    case MinecraftLayout::Mixed:    return QStringLiteral("混合（部分版本隔离）");
    case MinecraftLayout::Unknown:
    default:                        return QStringLiteral("无法识别");
    }
}

QString launcherDisplayName(LauncherSource source)
{
    switch (source) {
    case LauncherSource::Shadow:   return QStringLiteral("Shadow Launcher");
    case LauncherSource::Pcl2:     return QStringLiteral("PCL2");
    case LauncherSource::Hmcl:     return QStringLiteral("HMCL");
    case LauncherSource::Official: return QStringLiteral("Minecraft 官方启动器");
    case LauncherSource::Unknown:
    default:                       return QStringLiteral("未知");
    }
}

} // namespace ShadowLauncher
