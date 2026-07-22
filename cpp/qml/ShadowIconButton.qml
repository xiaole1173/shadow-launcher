// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls.Basic

// ═══ ShadowIconButton — 统一纯图标按钮 ═══
//
// 基准样式：MainWindow 窗口标题栏图标按钮
// - 28×28 透明底，hover 着色 + 弹性缩放
// - close（红） / normal（灰/主题色）两种变体
//
// 用法：
//   ShadowIconButton { icon: "\u2715"; type: "close"; onClicked: ... }
//   ShadowIconButton { icon: "\u2014"; type: "normal"; onClicked: ... }
//   ShadowIconButton { icon: "+"; onClicked: ... }
//   ShadowIconButton { source: "icons/lucide/refresh-cw.svg"; onClicked: ... }
//
// ──────────────────────────────────────────────

Rectangle {
    id: root

    // ── Size (default 28×28) ──
    implicitWidth: 28
    implicitHeight: 28
    width: 28
    height: 28

    // ── Public API ──

    // Icon text character (e.g. "\u2715" for ✕, "\u2014" for —, "+", "↻")
    property string icon: ""

    // Optional SVG source (takes priority when non-empty)
    property string source: ""

    // Variant: "normal" | "close" | "accent"
    //   normal — gray hover (#252a35)
    //   close  — red hover (#c05050 / errorLight)
    //   accent — accent color hover
    property string type: "normal"

    // Custom hover color override (if set, overrides type behavior)
    property color hoverColor: "transparent"
    property color pressColor: "transparent"
    property color defaultColor: "transparent"

    // Custom icon text color
    property color iconColor: "#505568"
    property color iconHoverColor: "#d0d4e0"

    // Custom icon size (0 = auto based on size)
    property int iconPixelSize: 0

    // SVG icon size
    property int sourceWidth: 14
    property int sourceHeight: 14

    // ── Signals ──
    signal clicked()

    // ── Internal state ──
    property bool _hovered: false
    property bool _pressed: false

    // ── Visual ──
    radius: StyleTokens.radiusLg
    color: {
        if (root.hoverColor.a > 0) {
            return _pressed ? root.pressColor : (_hovered ? root.hoverColor : root.defaultColor)
        }
        // Auto based on type
        if (!_hovered && !_pressed) return root.defaultColor.a > 0 ? root.defaultColor : "transparent"
        if (_pressed) {
            return root.type === "close" ? StyleTokens.errorLight :
                   root.type === "accent" ? Qt.darker(StyleTokens.accent, 1.15) : "#3a4050"
        }
        // Hovered
        return root.type === "close" ? "#c05050" :
               root.type === "accent" ? StyleTokens.accentHover : "#252a35"
    }

    scale: _pressed ? 0.85 : (_hovered ? 1.12 : 1.0)

    Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

    // ── Content ──
    Image {
        anchors.centerIn: parent
        source: root.source
        width: root.sourceWidth
        height: root.sourceHeight
        visible: root.source !== ""
    }

    Text {
        anchors.centerIn: parent
        text: root.icon
        color: _hovered || _pressed ? root.iconHoverColor : root.iconColor
        font.pixelSize: root.iconPixelSize > 0 ? root.iconPixelSize :
                        root.width <= 24 ? StyleTokens.fontSizeXs :
                        root.width <= 28 ? StyleTokens.fontSizeSm : StyleTokens.fontSizeMd
        font.weight: Font.Bold
        visible: root.source === ""
    }

    // ── Mouse area ──
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor

        onEntered: root._hovered = true
        onExited: { root._hovered = false; root._pressed = false }
        onPressed: root._pressed = true
        onReleased: { root._pressed = false; root.clicked() }
    }
}
