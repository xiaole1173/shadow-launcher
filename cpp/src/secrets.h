// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
//
// secrets.h — 发布构建的脱敏占位符（入库版本）。
//
// 本文件被 git 跟踪，所有值均为占位符，可安全上传到公开仓库。
// 真实值放在同目录 secrets_local.h（.gitignore 忽略，仅存在于开发者机器），
// 构建时若存在 secrets_local.h 则优先使用其中定义（见下方 #if __has_include）。
//
// 注入真实值的方式：
//   1. 复制本文件为 secrets_local.h
//   2. 在 secrets_local.h 中定义 SHADOW_AZURE_CLIENT_ID / SHADOW_GITEE_OWNER /
//      SHADOW_GITEE_REPO 等宏（覆盖本文件的占位符默认值）
//   3. 重新编译
//
// 生成脚本：tools/gen_secrets_local.py（本地用，不入库）

#pragma once

// ── 微软登录 Azure App ID（客户端公开标识；无 client secret）──
#ifndef SHADOW_AZURE_CLIENT_ID
#define SHADOW_AZURE_CLIENT_ID "YOUR_AZURE_CLIENT_ID"
#endif

// ── 更新服务器仓库（Gitee API）──
#ifndef SHADOW_GITEE_OWNER
#define SHADOW_GITEE_OWNER "YOUR_GITEE_OWNER"
#endif
#ifndef SHADOW_GITEE_REPO
#define SHADOW_GITEE_REPO "YOUR_GITEE_REPO"
#endif

// ── 自定义更新服务器 API（2026-08-15 迁移）──
// 指向自建服务器的 latest.json（字段兼容 Gitee release 格式）。
// 非空时优先使用，替代 Gitee API；为空则回退 setRepo(Gitee)。
#ifndef SHADOW_UPDATE_API_URL
#define SHADOW_UPDATE_API_URL ""
#endif

// 本地注入：若 secrets_local.h 存在（git 忽略），其 #define 覆盖上述占位符
#if __has_include("secrets_local.h")
#include "secrets_local.h"
#endif
