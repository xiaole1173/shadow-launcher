// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
//
// modpack_common.h — 双平台整合包统一数据结构（解析层对外输出契约）。
//
// 参考 主流启动器 ModModpack.vb 的流程架构：无论 CurseForge (.zip+manifest.json)
// 还是 Modrinth (.mrpack + modrinth.index.json)，解析后统一收敛为
// ModpackMeta，上层业务无需关心整合包来源。

#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

namespace ShadowLauncher {

// ── 整合包来源格式 ──
enum class ModpackFormat {
    Unknown = 0,
    CurseForge,   // zip + manifest.json
    Modrinth      // mrpack + modrinth.index.json
};

// ── 单个远端模组/资源文件 ──
struct ModpackRemoteFile {
    QString source;          // "curseforge" | "modrinth"（诊断用）
    int projectId = 0;       // CurseForge: projectID
    int fileId = 0;          // CurseForge: fileID
    QString displayName;     // 界面展示名（CF API displayName / Modrinth path basename）
    QString fileName;        // 落盘文件名（CF API fileName / Modrinth path basename）
    QString relPath;         // 相对游戏目录的目标路径，如 "mods/jei.jar"、"config/xx.toml"
    QString category;        // "mods" | "resourcepacks" | "shaderpacks" | "other"（CF 按 modules 判定）
    QString downloadUrl;     // 首选下载地址（CF API 返回的签名直链 / Modrinth downloads[0]）
    QStringList fallbackUrls; // 备用下载地址（Modrinth downloads[1..n]）
    QByteArray sha1;         // 期望 SHA1（Modrinth index.json；CF API 不提供则留空）
    qint64 size = 0;         // 期望大小（CF API fileLength；Modrinth fileSize；0 = 未知）
    bool required = true;    // CF required / Modrinth env.client
    int index = 0;           // 稳定序号（UI 列表定位用）

    // 界面状态（由下载层更新，经 modItemsChanged 上抛）
    QString status = QStringLiteral("pending");  // pending / downloading / done / fail / skipped
    QString error;
};

// ── 统一解析输出结构 ──
struct ModpackMeta {
    ModpackFormat format = ModpackFormat::Unknown;
    QString name;            // 整合包名称
    QString versionId;       // 整合包版本号
    QString summary;         // 简介（Modrinth）
    QString mcVersion;       // Minecraft 版本
    QString loaderType;      // "forge" | "neoforge" | "fabric" | ""（"" = 纯原版）
    QString loaderVersion;   // 加载器版本
    QStringList overrideDirs; // 需解压到游戏目录的覆写目录（CF: ["overrides"]；Modrinth: ["overrides","client-overrides"]；"." 表示整包根）
    QList<ModpackRemoteFile> files;   // 远端下载任务列表
    QString iconPath;        // 版本图标路径（预留，当前为空）

    int fileCount() const { return files.size(); }
};

// ── 工具：加载器标识解析（主流启动器: "forge-47.2.0" / "fabric-loader-0.15.11" / "neoforge-20.4.237"）──
// 返回 true 表示成功识别；false 表示不支持/过老的加载器（error 携带原因）。
bool parseLoaderId(const QString& rawId, QString* loaderType, QString* loaderVersion, QString* error);

// ── 工具：目录名安全化（版本文件夹名必须可作路径、可作版本 id）──
QString sanitizeVersionName(const QString& raw);

// ── 工具：ZIP 条目 / 整合包内相对路径安全净化 ──
// 拒绝绝对路径、".." 穿越、盘符、Windows 非法字符；返回净化后的相对路径，非法返回空。
QString sanitizeRelPath(const QString& entryName);

} // namespace ShadowLauncher
