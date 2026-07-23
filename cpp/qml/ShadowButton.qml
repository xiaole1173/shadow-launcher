// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic

// ═══ 弹性按钮 — hover 放大, press 缩小 ═══

Button {
    id: shadowBtn
    property color accentColor: StyleTokens.accent
    property color textColor: StyleTokens.textInverse

    // ── 联机页所需属性 ──
    property bool bold: false
    property int btnRadius: StyleTokens.radiusMd
    property bool outlined: false
    property real hoverScale: 1.04
    property real pressScale: 0.94
    property real hoverFillAlpha: 0.10
    property real btnWidth: 0
    // ── 图标 ──
    property string iconSource: ""
    property int iconSize: 16

    flat: true
    font.pixelSize: StyleTokens.fontSizeMd
    font.bold: shadowBtn.bold
    hoverEnabled: true

    // 弹性动画
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

    contentItem: Item {
        implicitWidth: row.implicitWidth; implicitHeight: row.implicitHeight

        Row {
            id: row
            anchors.centerIn: parent
            spacing: shadowBtn.iconSource.length > 0 ? 6 : 0

            Image {
                anchors.verticalCenter: parent.verticalCenter
                source: shadowBtn.iconSource
                visible: shadowBtn.iconSource.length > 0
                width: shadowBtn.iconSize; height: shadowBtn.iconSize
                sourceSize.width: shadowBtn.iconSize * 2; sourceSize.height: shadowBtn.iconSize * 2
                fillMode: Image.PreserveAspectFit
            }

            Text {
                text: shadowBtn.text
                font: shadowBtn.font
                color: !shadowBtn.enabled ? StyleTokens.textMuted : shadowBtn.textColor
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
            }
        }
    }

    background: Rectangle {
        radius: shadowBtn.btnRadius
        opacity: shadowBtn.enabled ? 1.0 : 0.5

        color: {
            var a = shadowBtn.accentColor
            if (!shadowBtn.enabled)
                return Qt.rgba(a.r, a.g, a.b, 0.4)
            return shadowBtn.hovered ? Qt.lighter(a, 1.08) : a
        }

        border.color: {
            var a = shadowBtn.accentColor
            if (!shadowBtn.enabled) return Qt.rgba(a.r, a.g, a.b, 0.25)
            return shadowBtn.hovered ? Qt.lighter(a, 1.2) : a
        }
        border.width: 1

        Behavior on color { ColorAnimation { duration: 180; easing.type: Easing.OutCubic } }
        Behavior on border.color { ColorAnimation { duration: 200; easing.type: Easing.OutCubic } }
    }
}
