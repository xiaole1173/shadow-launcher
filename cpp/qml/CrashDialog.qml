import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Crash dialog — shown when Minecraft exits abnormally and a crash report is found
Popup {
    id: dialog
    modal: true
    anchors.centerIn: Overlay.overlay
    width: 440; height: Math.min(implicitHeight + 40, 580)
    closePolicy: Popup.CloseOnEscape
    padding: 0

    property var crashData: ({})  // {type, reason, description, suspectedMods, filePath, timestamp}

    onCrashDataChanged: {
        if (crashData && crashData.isValid) {
            console.log("[crash] dialog opened type=", crashData.type, "reason=", crashData.reason)
            open()
        }
    }

    background: Rectangle {
        radius: 12
        color: "#141820"
        border.color: "#2a1a20"
        border.width: 1
    }

    contentItem: ColumnLayout {
        spacing: 12
        anchors.fill: parent
        anchors.margins: 20

        // ── Header ──
        RowLayout {
            spacing: 10
            Rectangle {
                width: 36; height: 36; radius: 18
                color: crashData.type === "jvm" ? "#382020" : "#382828"
                Text { anchors.centerIn: parent; text: "⚠"; font.pixelSize: 18 }
            }
            ColumnLayout { spacing: 2
                Text {
                    text: crashData.type === "jvm" ? "JVM 崩溃" : "Minecraft 崩溃"
                    font.pixelSize: 16; font.bold: true; color: "#e8a0a0"
                }
                Text {
                    text: crashData.timestamp
                        ? new Date(crashData.timestamp).toLocaleString(Qt.locale(), "yyyy-MM-dd hh:mm:ss")
                        : ""
                    font.pixelSize: 10; color: "#706060"
                    visible: !!crashData.timestamp
                }
            }
        }

        // ── Body ──
        ColumnLayout { spacing: 8; Layout.fillWidth: true
            // Crash reason
            Text {
                Layout.fillWidth: true; font.pixelSize: 12; color: "#d0a0a0"
                text: crashData.reason || "未知原因"
                wrapMode: Text.Wrap
            }
            // Crash description
            Rectangle {
                Layout.fillWidth: true; Layout.preferredHeight: 60
                radius: 6; color: "#0e1018"; border.color: "#2a1a24"; clip: true
                visible: !!crashData.description
                Flickable {
                    anchors.fill: parent; anchors.margins: 10
                    contentWidth: descLabel.implicitWidth; contentHeight: descLabel.implicitHeight
                    Text {
                        id: descLabel; font.pixelSize: 11; color: "#908088"
                        text: crashData.description || ""; wrapMode: Text.Wrap; width: parent.width
                    }
                }
            }
            // Suspected mods
            ColumnLayout {
                visible: crashData.suspectedMods && crashData.suspectedMods.length > 0
                spacing: 4; Layout.fillWidth: true
                Text {
                    text: "⚠ 可疑模组:"
                    font.pixelSize: 11; color: "#a08080"; font.bold: true
                }
                Repeater {
                    model: crashData.suspectedMods || []
                    delegate: Rectangle {
                        width: parent ? parent.width : 200; height: 22; radius: 4
                        color: "#1a1420"; border.color: "#382838"
                        Text {
                            anchors.centerIn: parent; text: modelData
                            font.pixelSize: 10; color: "#c8a0c8"
                        }
                    }
                }
            }
        }

        // ── Buttons ──
        RowLayout {
            spacing: 8; Layout.fillWidth: true
            // Open crash report
            Rectangle {
                Layout.fillWidth: true; height: 32; radius: 6
                color: openHov.hovered ? "#202840" : "#151828"
                border.color: "#405068"
                Text {
                    anchors.centerIn: parent; text: "📄 打开崩溃报告"
                    font.pixelSize: 12; color: "#a0b0d0"
                }
                MouseArea {
                    id: openHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (crashData.filePath) {
                            Qt.openUrlExternally("file:///" + crashData.filePath)
                            console.log("[crash] opened crash report:", crashData.filePath)
                        }
                    }
                }
            }
            // Dismiss
            Rectangle {
                width: 80; height: 32; radius: 6
                color: closeHov.hovered ? "#382020" : "#201818"
                border.color: "#503030"
                Text {
                    anchors.centerIn: parent; text: "关闭"
                    font.pixelSize: 12; color: "#c0a0a0"
                }
                MouseArea {
                    id: closeHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: { dialog.close(); console.log("[crash] dialog dismissed") }
                }
            }
        }
    }
}
