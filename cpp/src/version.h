// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 Shadow
#pragma once

// ── 唯一版本号来源（2026-08-15 从 CMakeLists compile definition 移出）──
// 背景：SHADOW_DISPLAY_VERSION 原来定义在 CMakeLists target_compile_definitions，
// 改版本号会触发 vcxproj 预处理宏变化 → MSBuild 全量重编整个项目（实测每次
// 切版本编译数分钟，严重拖慢更新验证）。
// 现在只需改本文件并重编引用它的少数文件（main_release/main/app_backend）。
#define SHADOW_DISPLAY_VERSION "v1.0.0"
