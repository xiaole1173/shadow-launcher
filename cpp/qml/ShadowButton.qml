// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic

// ═══ 弹性按钮 — 实心 + 描边双模式，细节交互动画 ═══
// 使用方式:
//   ShadowButton { text: "创建房间"; accentColor: StyleTokens.accent }
//   ShadowButton { text: "返回"; outlined: true; accentColor: StyleTokens.textTertiary; btnRadius: StyleTokens.radiusLg }

Button {
    id: shadowBtn

    // ── 样式属性 ──
    property color accentColor: StyleTokens.accent       // 主色 (实心=背景色, 描边=边框+文字色)
    property color textColor: StyleTokens.textInverse    // 文字色 (仅实心模式)

    property bool outlined: false        // 描边模式: 透明背景+色边框
    property bool bold: false            // 加粗文字
    property int btnRadius: StyleTokens.radiusMd  // 圆角
    property real btnWidth: 0            // 0=自适应, >0 固定宽度

    // ── 交互属性 ──
    property real hoverScale: 1.04
    property real pressScale: 0.94

    // ── 按钮基础 ──
    flat: true
    font.pixelSize: StyleTokens.fontSizeMd
    font.bold: shadowBtn.bold
    hoverEnabled: true
    cursorShape: Qt.PointingHandCursor

    implicitWidth: btnWidth > 0 ? btnWidth : undefined

    // ── 弹性缩放 ──
    scale: 1.0
    Behavior on scale {
        NumberAnimation {
            duration: 120
            easing.type: Easing.OutBack
            easing.overshoot: 0.15
        }
    }

    onHoveredChanged: updateScale()
    onPressedChanged: updateScale()

    function updateScale() {
        if (pressed) {
            scale = pressScale
        } else if (hovered) {
            scale = hoverScale
        } else {
            scale = 1.0
        }
    }

    // ── 文字内容 ──
    contentItem: Text {
        text: shadowBtn.text
        font: shadowBtn.font
        color: {
            if (!shadowBtn.enabled) return StyleTokens.textMuted
            if (shadowBtn.outlined) return shadowBtn.accentColor
            return shadowBtn.textColor
        }
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
    }

    // ── 背景（实心/描边+悬停填色）──
    background: Rectangle {
        id: bgRect
        radius: shadowBtn.btnRadius
        opacity: shadowBtn.enabled ? 1.0 : 0.5

        // 实心：accentColor 背景 → hover 微亮
        // 描边：透明背景 → hover 半透明填色
        color: {
            if (!shadowBtn.enabled) return shadowBtn.outlined ? "transparent" : Qt.rgba(bgRect._c.r, bgRect._c.g, bgRect._c.b, 0.4)
            if (shadowBtn.outlined) {
                if (shadowBtn.hovered) {
                    return Qt.rgba(bgRect._c.r, bgRect._c.g, bgRect._c.b, 0.10)
                }
                return "transparent"
            }
            return shadowBtn.hovered
                ? Qt.lighter(shadowBtn.accentColor, 1.08)
                : shadowBtn.accentColor
        }

        border.color: {
            if (!shadowBtn.enabled) return Qt.rgba(bgRect._c.r, bgRect._c.g, bgRect._c.b, 0.25)
            if (shadowBtn.outlined) {
                if (shadowBtn.hovered) return Qt.lighter(shadowBtn.accentColor, 1.15)
                return shadowBtn.accentColor
            }
            if (shadowBtn.hovered) return Qt.lighter(shadowBtn.accentColor, 1.2)
            return shadowBtn.accentColor
        }
        border.width: shadowBtn.outlined ? 1.5 : 1

        // 辅助：从 accentColor 提取 rgb 分量（减少 Qt.lighter 调用）
        readonly property color _c: shadowBtn.accentColor

        Behavior on color { ColorAnimation { duration: 180; easing.type: Easing.OutCubic } }
        Behavior on border.color { ColorAnimation { duration: 200; easing.type: Easing.OutCubic } }
        Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
    }
}
