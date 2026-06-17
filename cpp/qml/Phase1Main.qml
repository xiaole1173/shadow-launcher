// Phase 1: Minimal test UI — verifies QML engine + build pipeline
import QtQuick
import QtQuick.Controls
import QtQuick.Window

Window {
    id: window
    width: 900
    height: 600
    visible: true
    color: "#1a1a2e"
    title: "Shadow Launcher (C++)"

    Column {
        anchors.centerIn: parent
        spacing: 20

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "🎮 Shadow Launcher"
            font.pixelSize: 32
            font.bold: true
            color: "#e0e0ff"
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Phase 1 — C++ Backend Ready"
            font.pixelSize: 18
            color: "#8888cc"
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 400
            height: 2
            color: "#4444aa"
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Qt " + dataDir
            font.pixelSize: 14
            color: "#6666aa"
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Status: ✅ QML Engine OK\n✅ CMake Build OK\n⬜ Backend (Phase 2–3)"
            font.pixelSize: 13
            color: "#44cc44"
            horizontalAlignment: Text.AlignHCenter
            lineHeight: 1.5
        }
    }
}
