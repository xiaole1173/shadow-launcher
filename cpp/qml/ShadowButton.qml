// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic

// ═══ 弹性按钮 — 实心 + 描边双模式，细节交互动画 ═══
Button {
    id: shadowBtn

    // ── 样式属性 ──
    property color accentColor: StyleTokens.accent
    property color textColor: StyleTokens.textInverse

    property bool outlined: false
    property bool bold: false
    property int btnRadius: StyleTokens.radiusMd
    property real btnWidth: 0
    property real hoverFillAlpha: 0.10

    // ── 交互属性 ──
    property real hoverScale: 1.04
    property real pressScale: 0.94

    // ── 按钮基础 ──
    flat: true
    font.pixelSize: StyleTokens.fontSizeMd
    font.bold: bold
    hoverEnabled: true
    cursorShape: Qt.PointingHandCursor

    // ── 弹性缩放 ──
    scale: 1.0
    Behavior on scale {
        NumberAnimation { duration: 120; easing.type: Easing.OutBack; easing.overshoot: 0.15 }
    }

    onHoveredChanged: updateScale()
    onPressedChanged: updateScale()

    function updateScale() {
        if (pressed) scale = pressScale
        else if (hovered) scale = hoverScale
        else scale = 1.0
    }

    // ── 文字内容 ──
    contentItem: Text {
        text: shadowBtn.text
        font: shadowBtn.font
        color: !shadowBtn.enabled ? StyleTokens.textMuted
            : shadowBtn.outlined && shadowBtn.hovered && shadowBtn.hoverFillAlpha > 0.3
                ? StyleTokens.textInverse
                : shadowBtn.outlined ? shadowBtn.accentColor : shadowBtn.textColor
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
    }

    // ── 背景（实心/描边+悬停填色）──
    background: Rectangle {
        id: bgRect
        radius: shadowBtn.btnRadius
        opacity: shadowBtn.enabled ? 1.0 : 0.5

        // 实心：accentColor → hover 微亮
        // 描边：透明 → hover 半透明填色
        color: {
            var c = shadowBtn.accentColor
            if (!shadowBtn.enabled)
                return shadowBtn.outlined ? "transparent" : Qt.rgba(c.r, c.g, c.b, 0.4)
            if (shadowBtn.outlined)
                return shadowBtn.hovered ? Qt.rgba(c.r, c.g, c.b, shadowBtn.hoverFillAlpha) : "transparent"
            return shadowBtn.hovered ? Qt.lighter(c, 1.08) : c
        }

        border.color: {
            var c = shadowBtn.accentColor
            if (!shadowBtn.enabled) return Qt.rgba(c.r, c.g, c.b, 0.25)
            if (shadowBtn.outlined)
                return shadowBtn.hovered ? Qt.lighter(c, 1.15) : c
            return shadowBtn.hovered ? Qt.lighter(c, 1.2) : c
        }
        border.width: shadowBtn.outlined ? 1.5 : 1

        Behavior on color { ColorAnimation { duration: 180; easing.type: Easing.OutCubic } }
        Behavior on border.color { ColorAnimation { duration: 200; easing.type: Easing.OutCubic } }
        Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
    }
}
