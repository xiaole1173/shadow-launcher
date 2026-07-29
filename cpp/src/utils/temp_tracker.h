// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
//
// TempTracker — 记录和清理下载安装遗留的临时目录。
//
// 当启动器在下载过程中被强行关闭时，%TEMP%/shadow-merged-* 不会被清理。
// TempTracker 通过在固定位置记录 tracking 文件，在下次启动时自动清理残留。
//
// 设计：
//   1. createMergedContext 创建 tempDir 时 → record()
//   2. destroyMergedContext 成功删除 tempDir 后 → forget()
//   3. 启动器启动时 → cleanupOrphans() 遍历残留的 tracking 文件并清理

#pragma once

#include <QString>

class TempTracker {
public:
    /// tracking 文件存放目录: %TEMP%/shadow-tracker/
    static QString trackerDir();

    /// 记录一个临时目录路径。写入 UUID 命名的 tracking 文件。
    static void record(const QString& tempDir);

    /// 移除 tracking 记录（临时目录已正常清理）。
    static void forget(const QString& tempDir);

    /// 扫描所有残留的 tracking 文件，删除对应的临时目录，再删掉 tracking 文件。
    /// 应在启动器初始化阶段的早期调用。
    static void cleanupOrphans();
};
