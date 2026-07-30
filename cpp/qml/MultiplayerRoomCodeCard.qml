// SPDX-License-Identifier: AGPL-3.0-or-later
// 联机房间码卡片 — 房主模式下显示房间码 + 复制按钮
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    implicitHeight: 72
    color: "transparent"
    border.color: StyleTokens.bgElevated; border.width: 1
    radius: StyleTokens.radiusLg
    visible: mp && mp.role === 1 && mp.roomCode !== ""
    opacity: visible ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }

    property var mp: null
    property var toastManager: null

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
            color: {
                if (!mp) return "transparent"
                var d = mp.connectionDifficulty
                if (d === 1) return "#1a4ade80"     // Easiest
                if (d === 2) return "#1a3b82f6"     // Simple
                if (d === 3) return "#1af59e0b"     // Medium
                return "#1aef4444"                   // Tough
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
                color: {
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
                    if (root.toastManager)
                        root.toastManager.show("房间码已复制到剪贴板")
                }
            }
        }
    }
}
