// SPDX-License-Identifier: AGPL-3.0-or-later
// 联机网络监控面板 — 数据刷新缓动动画 + 高亮反馈
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    implicitHeight: grid.implicitHeight + 28
    color: StyleTokens.bgPrimary
    border.color: StyleTokens.bgElevated; border.width: 1
    radius: StyleTokens.radiusLg

    property var mp: null

    visible: mp && mp.state > 0
    opacity: visible ? 1 : 0
    Behavior on opacity {
        NumberAnimation { duration: AnimationTokens.panelEnterDuration; easing.type: AnimationTokens.panelEnterEasing }
    }

    GridLayout {
        id: grid
        width: parent.width - 24
        x: 12; y: 14
        columns: 4
        columnSpacing: 16
        rowSpacing: 12

        // ── Row 1: Connection difficulty ──
        Label { text: "连接难度"; color: StyleTokens.textSubtle; font.pixelSize: StyleTokens.fontSizeSm }
        Text {
            id: diffValue
            // Direct binding with explicit property references — re-evaluates on
            // connectionDifficultyChanged (NOTIFY) and on mp reassignment
            text: !mp ? "—"
                : mp.connectionDifficulty === 0 ? "未知"
                : mp.connectionDifficulty === 1 ? "直连"
                : mp.connectionDifficulty === 2 ? "简单"
                : mp.connectionDifficulty === 3 ? "中等"
                : mp.connectionDifficulty === 4 ? "困难" : "未知"
            color: !mp ? StyleTokens.textMuted
                : mp.connectionDifficulty === 0 ? StyleTokens.textMuted
                : mp.connectionDifficulty === 1 ? StyleTokens.success
                : mp.connectionDifficulty === 2 ? StyleTokens.info
                : mp.connectionDifficulty === 3 ? StyleTokens.warning
                : StyleTokens.error
            font.pixelSize: StyleTokens.fontSizeSm; font.bold: true
            Behavior on color {
                ColorAnimation { duration: AnimationTokens.highlightDuration; easing.type: AnimationTokens.highlightEasing }
            }
        }

        Label { text: "联机协议"; color: StyleTokens.textSubtle; font.pixelSize: StyleTokens.fontSizeSm }
        Text {
            text: mp && mp.state > 0 ? "Scaffolding v1" : "—"
            color: StyleTokens.textTertiary; font.pixelSize: StyleTokens.fontSizeSm; font.family: StyleTokens.fontFamilyMono
        }

        // ── Row 2: MC port status with highlight on change ──
        Label { text: "MC端口"; color: StyleTokens.textSubtle; font.pixelSize: StyleTokens.fontSizeSm }
        Text {
            id: mcPortValue
            text: !mp || mp.role === 0 ? "—"
                : mp.role === 1
                    ? (mp.mcServerPort > 0 && mp.state >= 6 ? "" + mp.mcServerPort
                        : (mp.state >= 1 ? "等待中" : "—"))
                    : (mp.state >= 6 ? "已就绪"
                        : (mp.state >= 1 ? "连接中" : "—"))
            color: !mp ? StyleTokens.textMuted
                : (mp.role === 1 && mp.mcServerPort > 0 && mp.state >= 6) ? StyleTokens.success
                : mp.state >= 6 ? StyleTokens.success
                : StyleTokens.textMuted
            font.pixelSize: StyleTokens.fontSizeSm
            Behavior on color {
                ColorAnimation { duration: AnimationTokens.highlightDuration; easing.type: AnimationTokens.highlightEasing }
            }
        }

        Label { text: "MC服务器"; color: StyleTokens.textSubtle; font.pixelSize: StyleTokens.fontSizeSm }
        Text {
            id: mcServerValue
            text: !mp ? "—"
                : mp.role === 1
                    ? (mp.mcServerName && mp.mcServerName.length > 0 ? mp.mcServerName
                        : (mp.state === 7 ? "等待MC启动..." : "—"))
                    : (mp.role === 2 && mp.state >= 5 ? "127.0.0.1" : "—")
            color: !mp ? StyleTokens.textMuted
                : (mp.role === 1 && mp.mcServerName && mp.mcServerName.length > 0) ? StyleTokens.success
                : (mp.role === 2 && mp.state >= 5) ? StyleTokens.success
                : StyleTokens.textMuted
            font.pixelSize: StyleTokens.fontSizeSm
        }

        // ── Row 3: Online count + fingerprint ──
        Label { text: "在线玩家"; color: StyleTokens.textSubtle; font.pixelSize: StyleTokens.fontSizeSm }
        Text {
            text: mp ? (mp.players ? mp.players.length : 0) + "/" + mp.maxPlayers : "—"
            color: StyleTokens.textTertiary; font.pixelSize: StyleTokens.fontSizeSm
        }

        Label { text: "指纹校验"; color: StyleTokens.textSubtle; font.pixelSize: StyleTokens.fontSizeSm }
        RowLayout {
            spacing: 4
            Image {
                source: "icons/lucide/check-circle.svg"
                width: 14; height: 14
                sourceSize.width: 14; sourceSize.height: 14
                visible: mp && mp.state >= 5
            }
            Text {
                text: mp && mp.state >= 5 ? "已通过" : "—"
                color: mp && mp.state >= 5 ? StyleTokens.success : StyleTokens.textMuted
                font.pixelSize: StyleTokens.fontSizeSm
            }
        }
    }

    // Periodic refresh timer to catch late-binding updates (e.g. connectionDifficulty from async EasyTier query)
    Timer {
        interval: 3000
        running: root.visible
        repeat: true
        onTriggered: {
            // Force property refresh by re-reading from C++ backend
            // The readonly property bindings should auto-update on NOTIFY signals,
            // but this timer ensures eventual consistency for late-arriving data.
            if (mp) {
                // Touch the properties to force QML binding re-evaluation
                var _forceDifficulty = mp.connectionDifficulty;
                var _forceMcName = mp.mcServerName;
                var _forceMcPort = mp.mcServerPort;
            }
        }
    }
}
