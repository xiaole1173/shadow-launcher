// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs

Rectangle {
    id: root
    anchors.fill: parent
    color: "transparent"
    visible: false
    z: 250

    property var toastManager: null
    property var appWindow: null

    // Internal state
    property bool _importing: false
    property string _statusText: ""
    property real _progress: 0.0
    property string _currentFile: ""
    property var _modItems: []
    property bool _hasResult: false
    property string _resultName: ""
    property string _resultVersionId: ""

    function show() {
        if (backend && backend.modpackImporter) {
            root.visible = true
        } else {
            if (toastManager) toastManager.show(qsTr("后端未就绪"), "error")
        }
    }

    function hide() {
        root.visible = false
    }

    // ── Backend signal bridge ──
    Connections {
        target: backend && backend.modpackImporter ? backend.modpackImporter : null
        enabled: root.visible

        function onBusyChanged() {
            root._importing = backend.modpackImporter.busy
        }
        function onProgressChanged() {
            root._statusText = backend.modpackImporter.statusText || ""
            root._progress = backend.modpackImporter.progress || 0.0
            root._currentFile = backend.modpackImporter.currentFile || ""
        }
        function onModListChanged() {
            root._modItems = backend.modpackImporter.modItems || []
        }
        function onHasResultChanged() {
            root._hasResult = backend.modpackImporter.hasResult
            root._resultName = backend.modpackImporter.resultName
            root._resultVersionId = backend.modpackImporter.resultVersionId
        }
        function onImportFinished(success, versionName, error) {
            if (success) {
                if (toastManager) toastManager.show(qsTr("整合包「%1」导入成功").arg(versionName), "success")
            } else {
                if (toastManager) toastManager.show(qsTr("导入失败: %1").arg(error), "error")
            }
        }
    }

    // ── File dialog ──
    FileDialog {
        id: importFileDialog
        title: qsTr("选择整合包文件")
        fileMode: FileDialog.OpenFile
        currentFolder: backend ? "file:///" + backend.gameDir.replace(/\\/g, "/") : ""
        nameFilters: [
            qsTr("整合包文件 (*.zip *.mrpack)"),
            qsTr("所有文件 (*)")
        ]
        onAccepted: {
            var path = importFileDialog.selectedFile.toString()
            if (path.startsWith("file:///"))
                path = path.substring(8)
            if (backend && backend.modpackImporter) {
                backend.modpackImporter.startImport(path)
            }
        }
    }

    // ── Dim overlay ──
    Rectangle {
        anchors.fill: parent
        color: "#80000000"
        opacity: root.visible ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
        MouseArea {
            anchors.fill: parent
            onClicked: { if (!root._importing) hide() }
        }
    }

    // ── Card ──
    Rectangle {
        id: card
        anchors.centerIn: parent
        width: 460
        height: Math.min(Math.max(320, root._importing || root._hasResult ? 400 : 300), parent.height - 80)
        radius: 16
        color: "#141820"
        border.color: "#2a2f3a"
        border.width: 1
        clip: true

        scale: root.visible ? 1.0 : 0.9
        opacity: root.visible ? 1 : 0
        Behavior on scale { NumberAnimation { duration: 300; easing.type: Easing.OutBack } }
        Behavior on opacity { NumberAnimation { duration: 200 } }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // ── Header ──
            Item {
                Layout.fillWidth: true
                height: 48
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 1
                    color: "#1e2230"
                }

                Text {
                    anchors.left: parent.left; anchors.leftMargin: 20
                    anchors.verticalCenter: parent.verticalCenter
                    text: root._hasResult ? qsTr("导入完成") : qsTr("导入整合包")
                    color: "#e0e4f0"
                    font.pixelSize: 15
                    font.bold: true
                }

                // Close button
                Rectangle {
                    anchors.right: parent.right; anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    width: 28; height: 28; radius: 6
                    color: closeBtn.containsMouse ? "#2a2f3a" : "transparent"
                    visible: !root._importing
                    Text {
                        anchors.centerIn: parent; text: "\u00D7"; color: "#8890a0"; font.pixelSize: 14
                    }
                    MouseArea {
                        id: closeBtn
                        anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: root.hide()
                    }
                }
            }

            // ── Body ──
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                // ──[State 1] Initial: file picker ──
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 12
                    visible: !root._importing && !root._hasResult && !root._statusText.length

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("选择整合包文件")
                        color: "#b0b8d0"
                        font.pixelSize: 14
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("支持 Modrinth (.mrpack) 和 CurseForge (.zip) 格式")
                        color: "#687080"
                        font.pixelSize: 11
                    }

                    // File picker area
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 80
                        radius: 8; color: "#1a1f2a"; border.color: "#2a2f3a"; border.width: 1
                        ColumnLayout {
                            anchors.centerIn: parent; spacing: 6
                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: qsTr("点击下方按钮选择文件或在文件夹图标处拖放")
                                color: "#687080"; font.pixelSize: 11
                            }
                            Rectangle {
                                Layout.alignment: Qt.AlignHCenter
                                width: 180; height: 36; radius: 8
                                color: pickArea.containsMouse ? "#2a2f3a" : "#1e2230"
                                border.color: "#3a3f4a"; border.width: 1
                                Text {
                                    anchors.centerIn: parent
                                    text: qsTr("选择文件"); color: "#8890e0"; font.pixelSize: 13
                                }
                                MouseArea {
                                    id: pickArea
                                    anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                    onClicked: importFileDialog.open()
                                }
                            }
                        }
                    }
                }

                // ──[State 2] Importing: progress ──
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 10
                    visible: root._importing || (!root._hasResult && root._statusText.length > 0 && !root._importing)

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: root._statusText || qsTr("准备中...")
                        color: "#c0c8e0"; font.pixelSize: 14
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: root._currentFile
                        color: "#687080"; font.pixelSize: 10
                        visible: root._currentFile.length > 0
                    }

                    // Progress bar
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 6
                        radius: 3; color: "#1e2230"
                        Rectangle {
                            width: parent.width * Math.max(0.02, root._progress)
                            height: parent.height; radius: 3
                            color: "#8890e0"
                            Behavior on width { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                        }
                    }

                    // Mod list
                    Item {
                        Layout.fillWidth: true; Layout.fillHeight: true
                        clip: true
                        visible: root._modItems.length > 0

                        ListView {
                            anchors.fill: parent
                            model: root._modItems
                            spacing: 2
                            delegate: RowLayout {
                                width: ListView.view.width
                                height: 22
                                spacing: 6

                                Text {
                                    text: modelData.status === "done" ? "完成" :
                                          modelData.status === "fail" ? "失败" : "等待"
                                    color: modelData.status === "done" ? "#60b060" :
                                           modelData.status === "fail" ? "#c06060" : "#687080"
                                    font.pixelSize: 12
                                }
                                Text {
                                    text: modelData.name || ""
                                    color: modelData.status === "fail" ? "#c06060" : "#a0a8b8"
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }

                    // Cancel button
                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        width: 100; height: 30; radius: 8
                        color: cancelBtn.containsMouse ? "#2a1520" : "#1e1820"
                        border.color: "#3a1520"; border.width: 1
                        visible: root._importing
                        Text {
                            anchors.centerIn: parent
                            text: qsTr("取消"); color: "#c06060"; font.pixelSize: 12
                        }
                        MouseArea {
                            id: cancelBtn
                            anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (backend && backend.modpackImporter) {
                                    backend.modpackImporter.cancelImport()
                                }
                            }
                        }
                    }
                }

                // ──[State 3] Finished ──
                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 10
                    visible: root._hasResult && !root._importing

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "\u2605"
                        font.pixelSize: 36
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("整合包「%1」导入成功").arg(root._resultName)
                        color: "#c0c8e0"; font.pixelSize: 14; font.bold: true
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: root._resultVersionId
                        color: "#687080"; font.pixelSize: 11
                    }

                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        width: 100; height: 32; radius: 8
                        color: finishBtn.containsMouse ? "#2a2f3a" : "#1e2230"
                        Text {
                            anchors.centerIn: parent
                            text: qsTr("关闭"); color: "#8890e0"; font.pixelSize: 13
                        }
                        MouseArea {
                            id: finishBtn
                            anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (backend && backend.modpackImporter) {
                                    backend.modpackImporter.dismissResult()
                                }
                                root.hide()
                            }
                        }
                    }
                }
            }
        }
    }
}
