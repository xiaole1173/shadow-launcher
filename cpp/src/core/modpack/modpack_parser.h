// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
//
// modpack_parser.h — 双平台整合包解析层（参照 主流启动器 ModModpack.vb）。
//
//   CurseForge: manifest.json（name / version / minecraft.version+modLoaders /
//               files[projectID,fileID,required] / overrides）
//   Modrinth:   modrinth.index.json（name / versionId / dependencies /
//               files[path,downloads,fileSize,hashes.sha1,env] / overrides 目录）
//
// 输出统一 ModpackMeta，含本地覆写目录清单（overrideDirs）与远端下载任务列表。
// 解析只读 zip 中央目录 + 清单文件，不做整包解压，轻量且可取消。

#pragma once

#include <QString>

#include <atomic>

#include "modpack_common.h"

namespace ShadowLauncher {

class ModpackParser {
public:
    // 格式探测：优先 modrinth.index.json，其次 manifest.json（主流启动器 同顺序）
    static ModpackFormat detectFormat(const QString& zipPath);

    // 解析清单并填充 meta。失败返回 false，error 携带可读原因。
    static bool parse(const QString& zipPath, ModpackFormat format,
                      ModpackMeta& meta, QString& error);
};

} // namespace ShadowLauncher
