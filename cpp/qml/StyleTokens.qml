// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
pragma Singleton
import QtQuick 2.15

// ═══════════════════════════════════════
// StyleTokens — 统一设计令牌（颜色、间距、圆角、字号）
//
// 所有 QML 文件应从本文件引用样式值，而非硬编码。
//
// 用法:
//   color: StyleTokens.bgCard
//   radius: StyleTokens.radiusMd
//   font.pixelSize: StyleTokens.fontSizeMd
// ═══════════════════════════════════════

QtObject {
    id: root

    // ──────────────────────────────────────
    // [色彩] 色彩系统
    // ──────────────────────────────────────

    // 背景色
    readonly property color bgPrimary:     "#0c0f16"
    readonly property color bgSecondary:   "#11141c"
    readonly property color bgCard:        "#1a1f2e"
    readonly property color bgElevated:    "#1e2230"
    readonly property color bgHover:       "#2a3040"
    readonly property color bgInput:       "#1a1f2a"

    // 前景/表面色
    readonly property color surfaceOverlay: "#141820"
    readonly property color surfaceLight:  "#1a1d24"

    // 文字色
    readonly property color textPrimary:   "#e8ecf8"
    readonly property color textSecondary: "#e4e8f2"
    readonly property color textTertiary:  "#9094a8"
    readonly property color textMuted:     "#505468"
    readonly property color textInverse:   "#ffffff"

    // 主色调 (蓝色系)
    readonly property color accent:        "#3b82f6"
    readonly property color accentHover:   "#5068c8"
    readonly property color accentLight:   "#6080e8"
    readonly property color accentVivid:   "#4a8fe7"
    readonly property color accentLink:    "#8aaeff"
    readonly property color accentSubtle:  "#1a2848"

    // 状态色
    readonly property color success:       "#4bc870"
    readonly property color successBg:     "#1a3a1a"
    readonly property color warning:       "#ff9800"
    readonly property color warningBg:     "#3a3000"
    readonly property color error:         "#ef4444"
    readonly property color errorBg:       "#3a1a1a"
    readonly property color errorLight:    "#e06060"

    // 信息色
    readonly property color info:          "#5098e8"
    readonly property color infoBg:        "#1a3a5c"

    // 边框色
    readonly property color border:        "#1e2230"
    readonly property color borderLight:   "#2a3040"
    readonly property color borderFocus:   "#5068c8"

    // ──────────────────────────────────────
    // [间距] 间距体系 (8px 网格)
    // ──────────────────────────────────────

    readonly property int spacingXs:   4
    readonly property int spacingSm:   8
    readonly property int spacingMd:  12
    readonly property int spacingLg:  16
    readonly property int spacingXl:  24
    readonly property int spacing2xl: 32
    readonly property int spacing3xl: 40

    // ──────────────────────────────────────
    // [圆角] 圆角
    // ──────────────────────────────────────

    readonly property int radiusXs:   2
    readonly property int radiusSm:   4
    readonly property int radiusMd:   6
    readonly property int radiusLg:   8
    readonly property int radiusXl:  12
    readonly property int radiusFull: 9999

    // ──────────────────────────────────────
    // [字号] 字号
    // ──────────────────────────────────────

    readonly property int fontSizeXs:   10
    readonly property int fontSizeSm:   11
    readonly property int fontSizeMd:   13
    readonly property int fontSizeLg:   15
    readonly property int fontSizeXl:   18
    readonly property int fontSize2xl:  24

    // ──────────────────────────────────────
    // [下载] 容器 & 布局
    // ──────────────────────────────────────

    readonly property int sidebarWidth: 220
    readonly property int headerHeight: 48
    readonly property int cardMinHeight: 120

    // ──────────────────────────────────────
    // [阴影] 阴影 (QML 不支持复杂阴影, 用于参考)
    // ──────────────────────────────────────

    readonly property color shadowColor:  "#0a0a12"
    readonly property real shadowOpacity: 0.3
}
