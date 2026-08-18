// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
// ────────────────────────────────────────────────────────────────
// 外部 .minecraft 文件夹识别与游戏目录解析（纯逻辑，不写盘）
//
// 背景（2026-08-18）：为兼容 PCL2 / HMCL / 官方启动器生成的 .minecraft
// 文件夹，需要同时正确处理「版本隔离」与「非版本隔离」两种形态，并保证
// 导入后绝不破坏原有目录结构。本模块只做只读探测与目录解析：
//   - probeMinecraftFolder()：识别有效性/布局形态/来源启动器/版本清单
//   - resolveVersionGameDir()：对每个版本解析其游戏数据实际所在目录
// 本模块不创建、不删除、不移动任何文件。
#pragma once

#include <QString>
#include <QStringList>

namespace ShadowLauncher {

// ── 目录布局形态（版本隔离状态）──
enum class MinecraftLayout {
    Unknown = 0,   // 无效目录 / 未探测
    Isolated,      // 版本隔离：根目录无游戏数据，各版本数据在 versions/<id>/
    Shared,        // 非隔离：根目录持有游戏数据，所有版本共享
    Mixed,         // 混合：根目录有数据，同时部分版本目录自带数据（PCL2 逐版本隔离）
};

// ── 来源启动器（尽力识别，标记缺失不判定为失败）──
enum class LauncherSource {
    Unknown = 0,
    Shadow,        // 本启动器（根 config/version_isolation.json）
    Pcl2,          // PCL2（根 PCL.ini 或 versions/<id>/PCL/）
    Hmcl,          // HMCL（versions/<id>/.hmcl/）
    Official,      // 官方启动器（launcher_profiles.json 且无其它标记）
};

// ── 一次探测的完整结果 ──
struct MinecraftFolderInfo {
    bool valid = false;                 // 是否为有效 .minecraft（有 versions/ 且含版本 json）
    MinecraftLayout layout = MinecraftLayout::Unknown;
    LauncherSource launcher = LauncherSource::Unknown;
    QString root;                       // 规范化后的根目录（末尾无分隔符）
    QStringList versionIds;             // 可识别的版本目录名（含 .json 或 .jar）
    bool rootHasGameData = false;       // 根目录是否持有游戏数据（→ 共享/非隔离倾向）
    int isolatedVersionCount = 0;       // 版本目录自带游戏数据的数量
    int totalVersionCount = 0;          // versions/ 下含 json/jar 的目录数
    QStringList notes;                  // 人类可读备注（QML 提示用）
};

// ════════════════════════════════════════════════════════════════
// 只读探测
// ════════════════════════════════════════════════════════════════

/// 根目录是否持有「强」游戏数据标记（用于判定共享/非隔离）。
/// 刻意排除 resourcepacks/、resources/、config/ —— PCL2 隔离模式下根目录
/// 仍会存在 resourcepacks/、resources/（全局资源），config/ 可能来自本启动器
/// 的辅助文件，均不足以证明共享布局。
bool hasRootGameData(const QString& dir);

/// 版本目录是否自带游戏数据 / 隔离意图（含 PCL/ 与 .hmcl/ 标记）。
bool hasVersionGameData(const QString& verDir);

/// 列出 versions/ 下可识别为版本的目录名（目录内含 .json 或 .jar）。
/// 不创建任何目录。
QStringList listVersionIds(const QString& root);

/// 识别来源启动器。
LauncherSource detectLauncherSource(const QString& root, const QStringList& versionIds);

/// 判定目录布局形态。
MinecraftLayout detectLayout(const QString& root, const QStringList& versionIds);

/// 完整探测（只读）。非 .minecraft 目录返回 valid=false 与备注。
MinecraftFolderInfo probeMinecraftFolder(const QString& root);

// ════════════════════════════════════════════════════════════════
// 游戏目录解析（核心）
// ════════════════════════════════════════════════════════════════

/// 解析某版本的实际游戏数据目录。规则（2026-08-18 与真实 PCL2/HMCL/Shadow
/// 目录逐一核对）：
///   1) versions/<id>/game 非空     → 该目录（Shadow 标准隔离 game/ 布局）
///   2) versions/<id>/ 自带游戏数据 → 该版本目录（散文件隔离：PCL2/HMCL/Shadow 散装）
///   3) 版本目录为空：
///        Shared/Mixed 布局 → 根目录（非隔离共享）
///        Isolated 布局     → 版本目录（隔离：即使未启动，数据也将落在版本目录）
QString resolveVersionGameDir(const QString& root, const QString& versionId,
                              MinecraftLayout layout);

// ════════════════════════════════════════════════════════════════
// 显示名
// ════════════════════════════════════════════════════════════════

QString layoutDisplayName(MinecraftLayout layout);
QString launcherDisplayName(LauncherSource source);

} // namespace ShadowLauncher
