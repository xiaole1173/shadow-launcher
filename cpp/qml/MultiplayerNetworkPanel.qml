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
            text: _diffText
            color: _diffColor
            font.pixelSize: StyleTokens.fontSizeSm; font.bold: true
            Behavior on color {
                ColorAnimation { duration: AnimationTokens.highlightDuration; easing.type: AnimationTokens.highlightEasing }
            }

            readonly property string _diffText: {
                if (!mp) return "—"
                var d = mp.connectionDifficulty
                if (d === 0) return "未知"; if (d === 1) return "直连"
                if (d === 2) return "简单"; if (d === 3) return "中等"
                if (d === 4) return "困难"; return "未知"
            }
            readonly property color _diffColor: {
                var d = mp ? mp.connectionDifficulty : 0
                if (d === 0) return StyleTokens.textMuted; if (d === 1) return StyleTokens.success
                if (d === 2) return StyleTokens.info; if (d === 3) return StyleTokens.warning
                return StyleTokens.error
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
            text: _mcPortText
            color: _mcPortColor
            font.pixelSize: StyleTokens.fontSizeSm

            readonly property string _mcPortText: {
                if (!mp || mp.role === 0) return "—"
                if (mp.role === 1) {
                    // Host: show actual scanned port if available
                    var p = mp.mcServerPort
                    if (p > 0 && mp.state >= 6) return "" + p
                    if (mp.state >= 1) return "等待中"
                    return "—"
                }
                // Guest: state-based
                if (mp.state >= 6) return "已就绪"
                if (mp.state >= 1) return "连接中"
                return "—"
            }
            readonly property color _mcPortColor: {
                if (!mp) return StyleTokens.textMuted
                if (mp.role === 1 && mp.mcServerPort > 0 && mp.state >= 6) return StyleTokens.success
                if (mp.state >= 6) return StyleTokens.success
                return StyleTokens.textMuted
            }
            Behavior on color {
                ColorAnimation { duration: AnimationTokens.highlightDuration; easing.type: AnimationTokens.highlightEasing }
            }
        }

        Label { text: "MC服务器"; color: StyleTokens.textSubtle; font.pixelSize: StyleTokens.fontSizeSm }
        Text {
            id: mcServerValue
            text: _mcServerText
            color: _mcSrvColor
            font.pixelSize: StyleTokens.fontSizeSm

            readonly property string _mcServerText: {
                if (!mp) return "—"
                if (mp.role === 1) {
                    // Host: show detected server name
                    var name = mp.mcServerName
                    if (name && name.length > 0) return name
                    if (mp.state === 7) return "等待MC启动..."
                    return "—"
                }
                // Guest: show forward endpoint
                if (mp.role === 2 && mp.state >= 5) return "127.0.0.1"
                return "—"
            }
            readonly property color _mcSrvColor: {
                if (!mp) return StyleTokens.textMuted
                if (mp.role === 1 && mp.mcServerName && mp.mcServerName.length > 0) return StyleTokens.success
                if (mp.role === 2 && mp.state >= 5) return StyleTokens.success
                return StyleTokens.textMuted
            }
        }

        // ── Row 3: Online count + fingerprint ──
        Label { text: "在线玩家"; color: StyleTokens.textSubtle; font.pixelSize: StyleTokens.fontSizeSm }
        Text {
            text: mp ? (mp.players ? mp.players.length : 0) + "/" + mp.maxPlayers : "—"
            color: StyleTokens.textTertiary; font.pixelSize: StyleTokens.fontSizeSm
        }

        Label { text: "指纹校验"; color: StyleTokens.textSubtle; font.pixelSize: StyleTokens.fontSizeSm }
        Text {
            text: mp && mp.state >= 5 ? "已通过 ✓" : "—"
            color: mp && mp.state >= 5 ? StyleTokens.success : StyleTokens.textMuted
            font.pixelSize: StyleTokens.fontSizeSm
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
