// SPDX-License-Identifier: AGPL-3.0-or-later
// 联机状态指示面板 — 显示连接状态、角色、房间码、难度
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    implicitHeight: col.implicitHeight + 24
    color: StyleTokens.bgPrimary
    border.color: StyleTokens.bgElevated; border.width: 1
    radius: StyleTokens.radiusLg
    visible: mp && mp.state !== 0 && mp.role !== 0
    opacity: visible ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }

    property var mp: null

    ColumnLayout {
        id: col
        width: parent.width - 24
        x: 12; y: 12
        spacing: 8

        // ── Top row: dot + role badge + state text ──
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            MultiplayerStateDot {
                state: mp ? mp.state : 0
                role: mp ? mp.role : 0
            }

            Text {
                text: {
                    if (!mp) return ""
                    if (mp.role === 1) return "房主模式"
                    if (mp.role === 2) return "访客模式"
                    return ""
                }
                font.pixelSize: StyleTokens.fontSizeSm
                color: mp && mp.role === 1 ? StyleTokens.warning : StyleTokens.info
                font.bold: true
            }

            Text {
                Layout.fillWidth: true
                text: mp ? mp.stateText : ""
                font.pixelSize: StyleTokens.fontSizeMd
                color: StyleTokens.textTertiary
                elide: Text.ElideRight
            }
        }

        // ── Progress bar for transient states ──
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 3
            radius: 2
            color: StyleTokens.bgElevated
            visible: mp && mp.state >= 1 && mp.state <= 4

            Rectangle {
                id: progressBar
                width: parent.width * progress
                height: parent.height
                radius: 2
                color: {
                    if (!mp) return StyleTokens.info
                    if (mp.state === 1) return StyleTokens.info      // CreatingRoom
                    if (mp.state === 2) return StyleTokens.info      // JoiningNetwork
                    if (mp.state === 3) return StyleTokens.warning   // Discovering
                    if (mp.state === 4) return StyleTokens.info      // Connecting
                    return StyleTokens.info
                }
                property real progress: 0.3

                SequentialAnimation on progress {
                    loops: Animation.Infinite
                    running: mp && mp.state >= 1 && mp.state <= 4
                    NumberAnimation { from: 0.15; to: 0.85; duration: 2000; easing.type: Easing.InOutSine }
                    NumberAnimation { from: 0.85; to: 0.15; duration: 2000; easing.type: Easing.InOutSine }
                }
            }
        }
    }
}
