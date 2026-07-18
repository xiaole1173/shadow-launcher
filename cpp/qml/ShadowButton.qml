// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic

// ═══ 弹性按钮 — hover 放大, press 缩小 ═══

Button {
    id: shadowBtn
    property string accentColor: "#4a9eff"
    property string textColor: "#ffffff"

    flat: true
    font.pixelSize: StyleTokens.fontSizeMd
    font.bold: false

    // 弹性动画
    scale: 1.0
    Behavior on scale {
        NumberAnimation { duration: 120; easing.type: Easing.OutBack; easing.overshoot: 0.15 }
    }

    onHoveredChanged: {
        if (hovered) {
            shadowBtn.scale = 1.04
        } else if (!pressed) {
            shadowBtn.scale = 1.0
        }
    }

    onPressedChanged: {
        if (pressed) {
            shadowBtn.scale = 0.94
        } else if (hovered) {
            shadowBtn.scale = 1.04
        } else {
            shadowBtn.scale = 1.0
        }
    }

    contentItem: Text {
        text: shadowBtn.text
        font: shadowBtn.font
        color: shadowBtn.enabled ? shadowBtn.textColor : "#555760"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
    }

    background: Rectangle {
        radius: StyleTokens.radiusMd
        color: shadowBtn.hovered && shadowBtn.enabled ? Qt.lighter(shadowBtn.accentColor, 1.08) : shadowBtn.accentColor
        border.color: shadowBtn.hovered && shadowBtn.enabled ? Qt.lighter(shadowBtn.accentColor, 1.2) : shadowBtn.accentColor
        border.width: 1
        opacity: shadowBtn.enabled ? 1.0 : 0.5

        Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
        Behavior on border.color { ColorAnimation { duration: 200 } }
    }
}
