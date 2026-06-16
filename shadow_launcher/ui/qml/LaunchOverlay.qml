import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: overlay
    color: "#0c0f16"

    // Flip-page animation on appearance
    property bool flipped: false
    transform: Rotation {
        id: flipRotation
        origin.x: overlay.width / 2
        origin.y: overlay.height / 2
        axis { x: 0; y: 1; z: 0 }
        angle: flipped ? 0 : 90
        Behavior on angle { NumberAnimation { duration: 500; easing.type: Easing.OutBack } }
    }
    opacity: flipped ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }

    onVisibleChanged: {
        if (visible) flipped = true
        else flipped = false
    }

    // Progress state
    property int progressValue: backend ? (backend.launchProgress || 0) : 0
    property string statusText: backend ? (backend.launchStatus || "准备启动...") : ""
    property string versionId: backend ? (backend.launchVersion || "") : ""
    property string username: backend ? (backend.launchUsername || "") : ""
    property int memory: backend ? (backend.launchMemory || 0) : 0

    // Track progress from backend
    Connections {
        target: backend
        enabled: backend !== null
        function onLaunchProgressChanged(pct, status) {
            progressValue = pct; statusText = status
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 20
        width: Math.min(parent.width * 0.55, 420)

        // Header
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "正在启动 Minecraft"
            font.pixelSize: 20
            font.bold: true
            color: "#d0d4e0"
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: versionId ? ("版本 " + versionId) : ""
            color: "#505468"
            font.pixelSize: 12
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: username ? ("玩家: " + username + "  |  内存: " + memory + " MB") : ""
            color: "#606478"
            font.pixelSize: 11
        }

        // Progress bar
        Rectangle {
            Layout.fillWidth: true
            height: 6
            radius: 3
            color: "#1a1f2a"

            Rectangle {
                height: 6; radius: 3
                color: "#3a4eb8"
                width: parent.width * (progressValue / 100)
                Behavior on width { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
            }
        }

        // Percentage
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: progressValue + "%"
            font.pixelSize: 24
            font.bold: true
            color: "#d0d4e0"
        }

        // Status text
        Text {
            id: statusLabel
            Layout.alignment: Qt.AlignHCenter
            text: statusText
            color: "#9094a8"
            font.pixelSize: 12
        }

        // Cancel button at bottom
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
