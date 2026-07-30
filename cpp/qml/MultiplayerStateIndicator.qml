// SPDX-License-Identifier: AGPL-3.0-or-later
// 联机状态指示面板 — 显示连接状态、角色、进度动效
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    implicitHeight: col.implicitHeight + 24
    color: StyleTokens.bgPrimary
    border.color: StyleTokens.bgElevated; border.width: 1
    radius: StyleTokens.radiusLg

    property var mp: null

    // Panel enter/exit animation
    visible: mp && mp.state !== 0 && mp.role !== 0
    opacity: visible ? 1 : 0
    Behavior on opacity {
        NumberAnimation { duration: AnimationTokens.panelEnterDuration; easing.type: AnimationTokens.panelEnterEasing }
    }

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

            // Role badge with scale animation on change
            Rectangle {
                implicitWidth: roleText.implicitWidth + 8
                implicitHeight: 20
                radius: StyleTokens.radiusSm
                color: mp && mp.role === 1 ? "#20f59e0b" : "#204ade80"

                Text {
                    id: roleText
                    anchors.centerIn: parent
                    text: {
                        if (!mp) return ""
                        if (mp.role === 1) return "房主"
                        if (mp.role === 2) return "访客"
                        return ""
                    }
                    font.pixelSize: StyleTokens.fontSizeXs
                    color: mp && mp.role === 1 ? "#f59e0b" : "#4ade80"
                    font.bold: true
                }
            }

            // State text with smooth change
            Text {
                Layout.fillWidth: true
                text: mp ? mp.stateText : ""
                font.pixelSize: StyleTokens.fontSizeMd
                color: StyleTokens.textTertiary
                elide: Text.ElideRight
                Behavior on text {
                    SequentialAnimation {
                        NumberAnimation { target: slideAnim; property: "animOpacity"; to: 0; duration: 80 }
                        NumberAnimation { target: slideAnim; property: "animOpacity"; to: 1; duration: 120 }
                    }
                }
                Rectangle { id: slideAnim; visible: false; property real animOpacity: 1 }
            }
        }

        // ── Progress bar with smooth width animation ──
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 3
            radius: 2
            color: StyleTokens.bgElevated
            // Show progress bar during initialization phases: CreatingRoom(1) through Connecting(4), plus WaitingForMcServer(7)
            visible: mp && ((mp.state >= 1 && mp.state <= 4) || mp.state === 7)

            Rectangle {
                id: progressBar
                width: parent.width * progress
                height: parent.height
                radius: 2
                color: {
                    if (!mp) return StyleTokens.info
                    if (mp.state === 1 || mp.state === 2) return StyleTokens.info
                    if (mp.state === 3) return StyleTokens.warning
                    if (mp.state === 4) return StyleTokens.info
                    return StyleTokens.info
                }
                property real progress: 0.3
                Behavior on width {
                    NumberAnimation { duration: AnimationTokens.progressDuration; easing.type: Easing.InOutSine }
                }

                SequentialAnimation on progress {
                    loops: Animation.Infinite
                    running: mp && mp.state >= 1 && mp.state <= 4
                    NumberAnimation { from: 0.15; to: 0.85; duration: 2200; easing.type: Easing.InOutSine }
                    NumberAnimation { from: 0.85; to: 0.15; duration: 2200; easing.type: Easing.InOutSine }
                }
            }
        }
    }
}
