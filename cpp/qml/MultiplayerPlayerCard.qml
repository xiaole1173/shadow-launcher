// SPDX-License-Identifier: AGPL-3.0-or-later
// 联机玩家卡片 — 展示单个玩家信息 + 入场/退场/延迟动画
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

    // Guard: no data yet → hide the whole card (avoids reading undefined fields)
    visible: playerData !== undefined

    // Entrance: fade + scale with staggered delay per index
    opacity: 0
    scale: 0.88
    Component.onCompleted: {
        staggerTimer.start()
    }

    Timer {
        id: staggerTimer
        interval: 40 * root.entryIndex
        repeat: false
        onTriggered: {
            root.opacity = 1
            root.scale = 1
        }
    }

    Behavior on opacity {
        NumberAnimation { duration: AnimationTokens.listItemEnterDuration; easing.type: AnimationTokens.listItemEnterEasing }
    }
    Behavior on scale {
        NumberAnimation { duration: 350; easing.type: AnimationTokens.listItemScaleEnterEasing }
    }

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
                // 名字下方显示厂商（如 "shadow"），与主流启动器显示访客条目一致。
                // 不再显示 IP：游戏内"对局域网开放"会自动发现服务器，无需手动填 IP；
                // 且旧实现里房主填虚拟 IP、访客填 127.0.0.1/内网 IP，语义混乱、无参考价值。
                text: playerData.vendor || ""
                color: StyleTokens.textSubtle
                font.pixelSize: StyleTokens.fontSizeXs
                font.family: StyleTokens.fontFamilyMono
                elide: Text.ElideRight
                visible: text !== ""
            }
        }

        // Latency badge with color transition
        Rectangle {
            visible: playerData.latency !== undefined && playerData.latency >= 0
            implicitWidth: latLabel.implicitWidth + 14
            implicitHeight: 22
            radius: StyleTokens.radiusMd
            // Direct bindings — re-evaluate when playerData updates
            color: (playerData.latency || 0) < 50 ? "#2060c060"
                : (playerData.latency || 0) < 150 ? "#20f59e0b"
                : "#20ef4444"
            Behavior on color {
                ColorAnimation { duration: AnimationTokens.dataFlushDuration; easing.type: AnimationTokens.dataFlushEasing }
            }

            Text {
                id: latLabel
                anchors.centerIn: parent
                text: (playerData.latency || 0) + "ms"
                font.pixelSize: StyleTokens.fontSizeSm
                color: (playerData.latency || 0) < 50 ? "#60c060"
                    : (playerData.latency || 0) < 150 ? "#f59e0b"
                    : StyleTokens.error
                Behavior on color {
                    ColorAnimation { duration: AnimationTokens.dataFlushDuration; easing.type: AnimationTokens.dataFlushEasing }
                }
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
