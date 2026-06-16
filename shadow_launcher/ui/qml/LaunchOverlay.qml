import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: overlay
    color: "#0c0f16"

    // ── Flip-page animation ──
    property bool flipped: false
    property bool closing: false

    transform: Rotation {
        id: flipRotation
        origin.x: overlay.width / 2
        origin.y: overlay.height / 2
        axis { x: 0; y: 1; z: 0 }
        angle: flipped ? 0 : 90
        Behavior on angle {
            NumberAnimation {
                id: flipAnim
                duration: 500; easing.type: Easing.OutBack
                onStopped: {
                    if (closing) {
                        overlay.visible = false
                        overlay.closing = false
                    }
                }
            }
        }
    }

    opacity: flipped ? 1 : 0
    Behavior on opacity {
        NumberAnimation {
            id: fadeAnim
            duration: 300; easing.type: Easing.OutCubic
        }
    }

    // Activate: visible first, then flip in
    onVisibleChanged: {
        if (visible) {
            flipped = true
            closing = false
        }
    }

    // Close gracefully: flip out first, then hide
    function closeOverlay() {
        closing = true
        flipped = false
        // If already not flipped, hide immediately
        if (flipAnim && !flipAnim.running) {
            overlay.visible = false
            closing = false
        }
    }

    // Auto-close when backend stops launching
    Connections {
        target: backend
        enabled: backend !== null
        function onLaunchingChanged() {
            if (backend && !backend.launching && overlay.visible) {
                closeOverlay()
            }
        }
    }

    // ── Progress state from backend ──
    property int progressValue: 0
    property string statusText: "准备启动..."
    property string versionId: ""
    property string username: ""
    property int memory: 0

    Connections {
        target: backend
        enabled: backend !== null
        function onLaunchProgressChanged(pct, status) {
            progressValue = pct; statusText = status
            versionId = backend.launchVersion || ""
            username = backend.launchUsername || ""
            memory = backend.launchMemory || 0
        }
    }

    Component.onCompleted: {
        if (backend) {
            versionId = backend.launchVersion || ""
            username = backend.launchUsername || ""
            memory = backend.launchMemory || 0
        }
    }

    // ── UI ──
    ColumnLayout {
        anchors.centerIn: parent
        spacing: 20
        width: Math.min(parent.width * 0.55, 420)

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "正在启动 Minecraft"
            font.pixelSize: 20; font.bold: true; color: "#d0d4e0"
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: versionId ? ("版本 " + versionId) : ""
            color: "#505468"; font.pixelSize: 12
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: username ? ("玩家: " + username + "  |  内存: " + memory + " MB") : ""
            color: "#606478"; font.pixelSize: 11
        }

        // Progress bar
        Rectangle {
            Layout.fillWidth: true; height: 6; radius: 3
            color: "#1a1f2a"
            Rectangle {
                height: 6; radius: 3; color: "#3a4eb8"
                width: parent.width * (progressValue / 100)
                Behavior on width { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
            }
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: progressValue + "%"
            font.pixelSize: 24; font.bold: true; color: "#d0d4e0"
        }

        Text {
            id: statusLabel
            Layout.alignment: Qt.AlignHCenter
            text: statusText; color: "#9094a8"; font.pixelSize: 12
        }

        Item { Layout.preferredHeight: 10 }

        // Cancel button
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            width: 120; height: 34; radius: 6
            color: "transparent"; border.color: "#402428"
            Text { anchors.centerIn: parent; text: "取消启动"; font.pixelSize: 12; color: "#c05050" }
            MouseArea {
                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                onClicked: { if (backend) backend.cancelLaunch() }
            }
        }
    }
}
