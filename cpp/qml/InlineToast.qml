// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick

// ═══ 通用内联通知栏 — Toast 同款设计 ═══
// 用法: <InlineToast type="warning" message="错误信息" />
// type: "info"(蓝色) | "warning"(红色) | "success"(绿色) | "error"(深红)
// 特性: 左侧色条 + 弹性右滑入场 + 高度自适应
Item {
    id: root
    property string type: "info"
    property string message: ""

    clip: true
    implicitHeight: message ? bar.implicitHeight + 8 : 0
    Behavior on implicitHeight {
        NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
    }

    // ── Color palette ──
    readonly property var _colors: ({
        "info":    { bg: "#1a3a5c", bar: "#5098e8", text: "#c8d8f0" },
        "warning": { bg: "#3a1518", bar: "#e85050", text: "#f0c8c8" },
        "success": { bg: "#1a3a1a", bar: "#40b840", text: "#c8f0c8" },
        "error":   { bg: "#3a1518", bar: "#e85050", text: "#f0c8c8" }
    })

    Rectangle {
        id: bar
        width: parent.width
        opacity: root.message ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 200 } }
        radius: StyleTokens.radiusSm
        color: _colors[type]?.bg || _colors.info.bg
        implicitHeight: label.implicitHeight + 20

        // ── Left accent bar ──
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 3
            color: _colors[type]?.bar || _colors.info.bar
            radius: StyleTokens.radiusXs
        }

        // ── Message ──
        Text {
            id: label
            anchors.left: parent.left; anchors.leftMargin: 10
            anchors.right: parent.right; anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            text: root.message
            font.pixelSize: StyleTokens.fontSizeSm
            color: _colors[type]?.text || _colors.info.text
            wrapMode: Text.WordWrap
        }

        // ── Elastic slide from right (Toast style) ──
        transform: Translate {
            x: root.message ? 0 : bar.width
            Behavior on x { NumberAnimation { duration: 350; easing.type: Easing.OutBack } }
        }
    }
}
