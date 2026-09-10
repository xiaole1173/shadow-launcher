// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick

/*!
  ToastStyleSuccess — 绿色下载完成通知样式
  供 LaunchOverlay 及类似组件引用，可单独配置颜色
 */
QtObject {
    id: root

    // ── 背景与边框 ──
    property color bgColor:          StyleTokens.successBg
    property color borderColor:      StyleTokens.successBg
    property color leftAccentColor:  StyleTokens.success

    // ── 文字颜色 ──
    property color textColor:        StyleTokens.success
    property color subtextColor:     StyleTokens.success

    // ── 图标 ──
    property string iconSource:      ""
    property int    iconSize:        18

    // ── 圆角 ──
    property int    radius:          8
}
