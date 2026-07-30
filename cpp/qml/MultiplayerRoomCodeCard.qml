// SPDX-License-Identifier: AGPL-3.0-or-later
// 联机房间码卡片 — 房主模式 + 复制动效 + 难度徽章
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    implicitHeight: 72
    color: "transparent"
    border.color: StyleTokens.bgElevated; border.width: 1
    radius: StyleTokens.radiusLg

    property var mp: null
    property var toastManager: null

    visible: mp && mp.role === 1 && mp.roomCode !== ""
    opacity: visible ? 1 : 0
    Behavior on opacity {
        NumberAnimation { duration: AnimationTokens.panelEnterDuration; easing.type: AnimationTokens.panelEnterEasing }
    }

    // Copy feedback overlay
    Rectangle {
        id: copyFlash
        anchors.fill: parent
        radius: StyleTokens.radiusLg
        color: StyleTokens.success
        opacity: 0
        Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutQuad } }
    }

    RowLayout {
        anchors.fill: parent; anchors.margins: 16; spacing: 12

        ColumnLayout {
            spacing: 2
            Text {
                text: "房间码（分享给你的好友）"
                font.pixelSize: StyleTokens.fontSizeSm
                color: StyleTokens.textSubtle
            }
            Text {
                text: mp ? mp.roomCode : ""
                font.pixelSize: StyleTokens.fontSizeXl
                font.bold: true
                color: StyleTokens.accentLight
                font.family: StyleTokens.fontFamilyMono
                Behavior on text {
                    SequentialAnimation {
                        NumberAnimation { target: codePulse; property: "opacity"; to: 0.3; duration: 100 }
                        NumberAnimation { target: codePulse; property: "opacity"; to: 0; duration: 300 }
                    }
                }
                Rectangle { id: codePulse; visible: false; property real opacity: 0 }
            }
        }

        Item { Layout.fillWidth: true }

        // Connection difficulty badge
        Rectangle {
            id: diffBadge
            visible: mp && mp.connectionDifficulty !== 0
            implicitWidth: diffLabel.implicitWidth + 12
            implicitHeight: 24
            radius: StyleTokens.radiusSm
            color: _diffBg
            Behavior on color {
                ColorAnimation { duration: AnimationTokens.highlightDuration; easing.type: AnimationTokens.highlightEasing }
            }

            readonly property color _diffBg: {
                if (!mp) return "transparent"
                var d = mp.connectionDifficulty
                if (d === 1) return "#1a4ade80"
                if (d === 2) return "#1a3b82f6"
                if (d === 3) return "#1af59e0b"
                return "#1aef4444"
            }

            Text {
                id: diffLabel
                anchors.centerIn: parent
                text: {
                    if (!mp) return ""
                    var d = mp.connectionDifficulty
                    if (d === 1) return "直连"
                    if (d === 2) return "简单"
                    if (d === 3) return "中等"
                    return "困难"
                }
                font.pixelSize: StyleTokens.fontSizeXs
                color: _diffText
                Behavior on color {
                    ColorAnimation { duration: AnimationTokens.highlightDuration; easing.type: AnimationTokens.highlightEasing }
                }
                readonly property color _diffText: {
                    if (!mp) return "transparent"
                    var d = mp.connectionDifficulty
                    if (d === 1) return "#4ade80"
                    if (d === 2) return "#60a0f0"
                    if (d === 3) return "#f59e0b"
                    return "#ef4444"
                }
            }
        }

        ShadowButton {
            implicitWidth: 90; implicitHeight: 34
            text: "复制"
            btnRadius: StyleTokens.radiusMd
            font.pixelSize: StyleTokens.fontSizeMd
            iconSource: "icons/lucide/copy.svg"
            onClicked: {
                if (mp) {
                    mp.copyRoomCode()
                    // Copy feedback flash
                    copyFlash.opacity = 0.15
                    flashTimer.start()
                    if (root.toastManager)
                        root.toastManager.show("房间码已复制到剪贴板")
                }
            }
        }
    }

    Timer {
        id: flashTimer
        interval: 300
        onTriggered: copyFlash.opacity = 0
    }
}
