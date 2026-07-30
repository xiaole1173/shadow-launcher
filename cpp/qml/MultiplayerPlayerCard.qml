// SPDX-License-Identifier: AGPL-3.0-or-later
// 联机玩家卡片 — 展示单个玩家信息（名字、延迟、角色、IP）
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    implicitHeight: 52
    color: StyleTokens.surfaceOverlay
    radius: StyleTokens.radiusLg
    border.color: StyleTokens.accentSubtle
    border.width: 1

    property var playerData: ({})
    property int entryIndex: 0

    // Entrance animation
    opacity: 0
    scale: 0.9
    Component.onCompleted: { root.opacity = 1; root.scale = 1 }
    Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
    Behavior on scale { NumberAnimation { duration: 350; easing.type: Easing.OutBack } }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14; anchors.rightMargin: 14
        spacing: 10

        // Player name + machine_id
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1
            Text {
                Layout.fillWidth: true
                text: playerData.name || playerData.machine_id || "Unknown"
                color: StyleTokens.textSecondary
                font.pixelSize: StyleTokens.fontSizeMd
                font.weight: Font.Medium
                elide: Text.ElideRight
            }
            Text {
                text: {
                    var ip = playerData.ip || ""
                    return ip !== "" ? ip : (playerData.hostname || "")
                }
                color: StyleTokens.textSubtle
                font.pixelSize: StyleTokens.fontSizeXs
                font.family: StyleTokens.fontFamilyMono
                elide: Text.ElideRight
                visible: text !== ""
            }
        }

        // Latency badge
        Rectangle {
            visible: playerData.latency !== undefined && playerData.latency >= 0
            implicitWidth: latLabel.implicitWidth + 14
            implicitHeight: 22
            radius: StyleTokens.radiusMd
            color: {
                var lat = playerData.latency || 0
                if (lat < 50) return "#2060c060"
                if (lat < 150) return "#20f59e0b"
                return "#20ef4444"
            }
            Text {
                id: latLabel
                anchors.centerIn: parent
                text: (playerData.latency || 0) + "ms"
                font.pixelSize: StyleTokens.fontSizeSm
                color: {
                    var lat = playerData.latency || 0
                    if (lat < 50) return "#60c060"
                    if (lat < 150) return "#f59e0b"
                    return StyleTokens.error
                }
            }
        }

        // Vendor badge
        Rectangle {
            visible: playerData.vendor && playerData.vendor !== ""
            implicitWidth: vendorLabel.implicitWidth + 10
            implicitHeight: 20
            radius: StyleTokens.radiusSm
            color: "#103b82f6"
            Text {
                id: vendorLabel
                anchors.centerIn: parent
                text: playerData.vendor
                font.pixelSize: StyleTokens.fontSizeXs
                color: "#80a0e0"
            }
        }

        // Role badge
        Rectangle {
            implicitWidth: roleLabel.implicitWidth + 14
            implicitHeight: 22
            radius: StyleTokens.radiusMd
            color: playerData.kind === "HOST" ? "#20f59e0b" :
                   playerData.kind === "LOCAL" ? "#204ade80" : "#203b82f6"
            Text {
                id: roleLabel
                anchors.centerIn: parent
                text: playerData.kind === "HOST" ? "房主" :
                      playerData.kind === "LOCAL" ? "本地" : "玩家"
                font.pixelSize: StyleTokens.fontSizeSm
                color: playerData.kind === "HOST" ? "#f59e0b" :
                       playerData.kind === "LOCAL" ? "#4ade80" : "#60a0f0"
            }
        }

        // Status dot
        MultiplayerStateDot {
            state: playerData.kind === "HOST" ? 6 : 5
            role: playerData.kind === "HOST" ? 1 : 2
        }
    }
}
