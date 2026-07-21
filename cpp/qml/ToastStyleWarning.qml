// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick

/*!
  ToastStyleWarning — 黄色下载中通知样式
  供 LaunchOverlay 及类似组件引用，可单独配置颜色
 */
QtObject {
    id: root

    // ── 背景与边框 ──
    property color bgColor:          "#1a1800"
    property color borderColor:      "#5a5010"
    property color leftAccentColor:  "#d4a020"

    // ── 文字颜色 ──
    property color textColor:        "#e8d870"
    property color subtextColor:     "#c0b050"

    // ── 图标 ──
    property string iconSource:      ""   // 预留 SVG 图标路径
    property int    iconSize:        18

    // ── 圆角 ──
    property int    radius:          8
}
