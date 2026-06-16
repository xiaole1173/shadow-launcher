import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: overlay
    color: "#0c0f16"

    // Simple fade animation — Loader 直接加载/卸载，QML animate 自动产生
    opacity: 1
    Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }

    Component.onCompleted: overlay.opacity = 0; overlay.opacity = 1  // trigger fade-in
    Component.onDestruction: overlay.opacity = 0  // fade-out (may not render due to Loader)

    // ── Progress state from backend ──
    property int progressValue: 0
    property string statusText: "准备启动..."
    property string versionId: backend ? (backend.launchVersion || "") : ""
    property string username: backend ? (backend.launchUsername || "") : ""
    property int memory: backend ? (backend.launchMemory || 0) : 0

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
