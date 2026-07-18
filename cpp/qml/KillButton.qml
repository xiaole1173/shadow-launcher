// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic

// ═══ 强制退出按钮 — 圆形 [电源] 图标 hover 展开为圆角长方形 ═══

Rectangle {
    id: killButton
    width: normalSize
    height: normalSize
    radius: height / 2
    visible: false

    property var backend: null
    property int normalSize: 44
    property int expandedWidth: 220
    property int btnHeight: 44
    property bool expanded: false
    property color bgColor: "#c0101114"
    property color borderColor: "#78787887"
    property color textColor: "#b4b4c3"
    property color expandedBgColor: "#f01e1f26"
    property color expandedBorderColor: "#e74c3c"
    property color expandedTextColor: "#e74c3c"

    Behavior on width {
        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
    }
    Behavior on radius {
        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
    }

    Text {
        id: killText
        anchors.centerIn: parent
        text: expanded ? "[优化] 强制结束游戏进程" : "[电源]"
        font.pixelSize: expanded ? 12 : 16
        font.bold: expanded
        color: expanded ? expandedTextColor : textColor
        Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
    }

    color: expanded ? expandedBgColor : bgColor
    border.color: expanded ? expandedBorderColor : borderColor
    border.width: 1.5

    Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
    Behavior on border.color { ColorAnimation { duration: 200 } }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor

        onEntered: {
            killButton.expanded = true
            killButton.width = killButton.expandedWidth
        }
        onExited: {
            killButton.expanded = false
            killButton.width = killButton.normalSize
        }
        onClicked: {
            if (backend) backend.killGameProcess()
            // 缩放消失动画
            killDisappear.start()
        }
    }

    // 点击后缩放消失
    SequentialAnimation {
        id: killDisappear
        PropertyAnimation {
            target: killButton
            property: "scale"
            from: 1.0
            to: 0.0
            duration: 300
            easing.type: Easing.InBack
        }
        PropertyAnimation {
            target: killButton
            property: "opacity"
            from: 1.0
            to: 0.0
            duration: 300
        }
        onFinished: {
            killButton.visible = false
            killButton.scale = 1.0
            killButton.opacity = 1.0
        }
    }
}
