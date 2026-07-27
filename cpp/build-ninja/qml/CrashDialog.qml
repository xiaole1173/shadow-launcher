// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
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
        radius: StyleTokens.radiusXl
        color: StyleTokens.surfaceOverlay
        border.color: StyleTokens.surfaceLight
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
                Text { anchors.centerIn: parent; text: "[警告]"; font.pixelSize: StyleTokens.fontSizeXl }
            }
            ColumnLayout { spacing: 2
                Text {
                    text: crashData.type === "jvm" ? "JVM 崩溃" : "Minecraft 崩溃"
                    font.pixelSize: StyleTokens.fontSizeLg; font.bold: true; color: "#e8a0a0"
                }
                Text {
                    text: crashData.timestamp
                        ? new Date(crashData.timestamp).toLocaleString(Qt.locale(), "yyyy-MM-dd hh:mm:ss")
                        : ""
                    font.pixelSize: StyleTokens.fontSizeXs; color: "#706060"
                    visible: !!crashData.timestamp
                }
            }
        }

        // ── Body ──
        ColumnLayout { spacing: 8; Layout.fillWidth: true
            // Crash reason
            Text {
                Layout.fillWidth: true; font.pixelSize: StyleTokens.fontSizeSm; color: "#d0a0a0"
                text: crashData.reason || "未知原因"
                wrapMode: Text.Wrap
            }
            // Crash description
            Rectangle {
                Layout.fillWidth: true; Layout.preferredHeight: 60
                radius: StyleTokens.radiusMd; color: "#0e1018"; border.color: "#2a1a24"; clip: true
                visible: !!crashData.description
                Flickable {
                    anchors.fill: parent; anchors.margins: 10
                    contentWidth: descLabel.implicitWidth; contentHeight: descLabel.implicitHeight
                    Text {
                        id: descLabel; font.pixelSize: StyleTokens.fontSizeSm; color: "#908088"
                        text: crashData.description || ""; wrapMode: Text.Wrap; width: parent.width
                    }
                }
            }
            // Suspected mods
            ColumnLayout {
                visible: !!crashData && crashData.suspectedMods && crashData.suspectedMods.length > 0
                spacing: 4; Layout.fillWidth: true
                Text {
                    text: qsTr("[警告] 可疑模组:")
                    font.pixelSize: StyleTokens.fontSizeSm; color: "#a08080"; font.bold: true
                }
                Repeater {
                    model: crashData.suspectedMods || []
                    delegate: Rectangle {
                        width: parent ? parent.width : 200; height: 22; radius: StyleTokens.radiusSm
                        color: "#1a1420"; border.color: StyleTokens.bgHover
                        Text {
                            anchors.centerIn: parent; text: modelData
                            font.pixelSize: StyleTokens.fontSizeXs; color: "#c8a0c8"
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
                Layout.fillWidth: true; height: 32; radius: StyleTokens.radiusMd
                color: openHov.hovered ? "#202840" : "#151828"
                border.color: StyleTokens.textMuted
                Text {
                    anchors.centerIn: parent; text: qsTr("打开崩溃报告")
                    font.pixelSize: StyleTokens.fontSizeSm; color: "#a0b0d0"
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
                width: 80; height: 32; radius: StyleTokens.radiusMd
                color: closeHov.hovered ? "#382020" : "#201818"
                border.color: "#503030"
                Text {
                    anchors.centerIn: parent; text: qsTr("关闭")
                    font.pixelSize: StyleTokens.fontSizeSm; color: "#c0a0a0"
                }
                MouseArea {
                    id: closeHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: { dialog.close(); console.log("[crash] dialog dismissed") }
                }
            }
        }
    }
}
