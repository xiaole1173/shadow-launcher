// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    width: parent ? parent.width : 800
    height: parent ? parent.height : 600
    color: StyleTokens.bgSecondary
    id: root

    property var backend: null
    property var mainWindow: null
    property var versionIds: []
    signal goBack()
    signal versionSelected(string versionId)

    // Page enter
    opacity: 0
    y: 10
    Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeInDuration; easing.type: AnimationTokens.itemFadeInEasing } }
    Behavior on y { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
    Component.onCompleted: { opacity = 1; y = 0 }

    property string selectedVersionId: ""
    property string activeFilter: "all"
    // ToastManager 引用 — 由父组件传入
    property var toastManager: null

    // ═══ TOP BAR ═══
    RowLayout {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 44
        spacing: 8

        Rectangle {
            width: backText.implicitWidth + 20; height: 32; radius: StyleTokens.radiusMd
            color: backArea.containsMouse ? "#1A222D" : "transparent"
            Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
            Text {
                id: backText
                anchors.centerIn: parent
                text: qsTr("← 启动")
                color: backArea.containsMouse ? "#3B82F6" : "#B4BAC6"
                font.pixelSize: StyleTokens.fontSizeMd
                Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
            }
            MouseArea {
                id: backArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.goBack()
            }
        }

        // 刷新按钮 — 紧挨"← 启动"  [DIAGNOSTIC: pink bg for visibility]
        Rectangle {
            width: refreshLabel.implicitWidth + 24; height: 32; radius: StyleTokens.radiusMd
            color: refreshHover.hovered ? "#3B82F6" : "#2a3040"
            border.color: refreshHover.hovered ? "#60A5FA" : "#3a4050"
            border.width: 1
            scale: refreshMa.pressed ? 0.88 : (refreshHover.hovered ? 1.08 : 1.0)
            Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
            Behavior on border.color { ColorAnimation { duration: 150 } }
            Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }
            Text {
                id: refreshLabel
                anchors.centerIn: parent
                text: qsTr("⟳ 刷新")
                color: StyleTokens.textSecondary
                font.pixelSize: StyleTokens.fontSizeSm; font.weight: Font.Medium
            }
            HoverHandler { id: refreshHover }
            ToolTip { visible: refreshHover.hovered; text: qsTr("刷新版本列表"); delay: 500 }
            MouseArea {
                id: refreshMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (backend) {
                        toastManager.show("正在刷新...")
                        if (mainWindow) mainWindow.pageLoading = true
                        versionModel.clear()
                        backend.refreshInstalled()
                        backend.refreshVersionList()
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("版本选择")
            color: StyleTokens.textPrimary
            font.pixelSize: StyleTokens.fontSizeXl
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
        }
    }

    // ═══ TWO PANELS ═══
    RowLayout {
        anchors.top: parent.top
        anchors.topMargin: 52
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        spacing: 0

        // ─── LEFT: Game folders ───
        Rectangle {
            Layout.preferredWidth: 220
            Layout.fillHeight: true
            color: StyleTokens.bgPrimary

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 6

                Text {
                    text: qsTr("游戏文件夹")
                    color: "#7E8596"
                    font.pixelSize: StyleTokens.fontSizeSm
                    font.bold: true
                    Layout.bottomMargin: 4
                }

                // Game directories list
                ListView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 160
                    model: [
                        { name: "默认 (.minecraft)", path: backend ? backend.gameDir : "" },
                    ]
                    spacing: 2

                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 44
                        color: "transparent"
                        radius: StyleTokens.radiusMd
                        ColumnLayout {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 8
                            spacing: 0
                            Text {
                                text: modelData.name
                                color: "#B4BAC6"
                                font.pixelSize: StyleTokens.fontSizeMd
                            }
                            Text {
                                text: modelData.path
                                color: StyleTokens.textMuted
                                font.pixelSize: StyleTokens.fontSizeXs
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                        }
                    }
                }

                // Add folder button
                Rectangle {
                    Layout.fillWidth: true
                    height: 36; radius: StyleTokens.radiusLg
                    color: "transparent"
                    border.color: addArea.containsMouse ? "#3B82F6" : "#2A2F3A"
                    border.width: 1
                    scale: addArea.containsMouse ? 1.03 : 1.0
                    Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }
                    Behavior on border.color { ColorAnimation { duration: 200 } }
                    Text {
                        anchors.centerIn: parent
                        text: qsTr("[添加] 添加文件夹")
                        color: addArea.containsMouse ? "#3B82F6" : "#B4BAC6"
                        font.pixelSize: StyleTokens.fontSizeMd
                        Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                    }
                    MouseArea {
                        id: addArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (toastManager) toastManager.show("添加文件夹功能开发中...")
                        }
                    }
                }

                // Import modpack button
                Rectangle {
                    Layout.fillWidth: true
                    height: 36; radius: StyleTokens.radiusLg
                    color: "transparent"
                    border.color: importArea.containsMouse ? "#3B82F6" : "#2A2F3A"
                    border.width: 1
                    scale: importArea.containsMouse ? 1.03 : 1.0
                    Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }
                    Behavior on border.color { ColorAnimation { duration: 200 } }
                    Text {
                        anchors.centerIn: parent
                        text: qsTr("[下载] 导入整合包")
                        color: importArea.containsMouse ? "#3B82F6" : "#B4BAC6"
                        font.pixelSize: StyleTokens.fontSizeMd
                        Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                    }
                    MouseArea {
                        id: importArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            // Show modpack import overlay
                            var importOverlay = mainWindow ? mainWindow.modpackImportOverlay : null
                            if (importOverlay) {
                                importOverlay.show()
                            } else if (toastManager) {
                                toastManager.show("无法打开导入面板", "error")
                            }
                        }
                    }
                }
            }
        }

        // ─── RIGHT: Version list ───
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Filter pills
            RowLayout {
                id: filterPills
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 12
                height: 36
                spacing: 8

                Repeater {
                    model: ["全部", "正式版", "预览版", "原始版本"]
                    Rectangle {
                        height: 30; radius: 15
                        width: pillText.implicitWidth + 24
                        color: activeFilter === modelData ? "#3B82F6" : "#1A1D24"
                        border.color: activeFilter === modelData ? "#3B82F6" : "#2A2F3A"
                        border.width: 1
                        scale: pillMouse.containsMouse ? 1.04 : 1.0
                        Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }
                        Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                        Behavior on border.color { ColorAnimation { duration: 200 } }
                        Text {
                            id: pillText
                            anchors.centerIn: parent
                            text: modelData
                            color: activeFilter === modelData ? "#FFFFFF" : "#B4BAC6"
                            font.pixelSize: StyleTokens.fontSizeSm
                            Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                        }
                        MouseArea {
                            id: pillMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                activeFilter = modelData
                                rebuildVersionList()
                            }
                        }
                    }
                }
            }

            // Version list — populated in batches for instant page display
            ListModel { id: versionModel }
            property bool _listReady: false
            property var _pendingVersions: []
            property int _batchPos: 0

            function rebuildVersionList() {
                versionModel.clear()
                _listReady = false
                _pendingVersions = []
                _batchPos = 0

                let ids = backend && backend.installedVersions ? backend.installedVersions : []
                for (let i = 0; i < ids.length; i++) {
                    let v = ids[i] || ""
                    let type = "release"
                    if (v.indexOf("pre") >= 0 || v.indexOf("rc") >= 0 || v.indexOf("w") >= 0) type = "snapshot"
                    else if (v.indexOf("alpha") >= 0 || v.indexOf("beta") >= 0 || v.indexOf("inf") >= 0 || v.indexOf("rd") >= 0) type = "old"

                    if (activeFilter === "all" ||
                        (activeFilter === "正式版" && type === "release") ||
                        (activeFilter === "预览版" && type === "snapshot") ||
                        (activeFilter === "原始版本" && type === "old")) {
                        _pendingVersions.push({ versionId: v, vtype: type })
                    }
                }
                versionBatchTimer.start()
            }

            Timer {
                id: versionBatchTimer
                interval: 1  // yield to UI between batches
                repeat: true
                property int batchSize: 50
                onTriggered: {
                    var end = Math.min(_batchPos + batchSize, _pendingVersions.length)
                    for (var i = _batchPos; i < end; i++) {
                        versionModel.append(_pendingVersions[i])
                    }
                    _batchPos = end
                    if (_batchPos >= _pendingVersions.length) {
                        stop()
                        _listReady = true
                        _pendingVersions = []
                        if (mainWindow) mainWindow.pageLoading = false
                    }
                }
            }

            Connections {
                target: backend
                enabled: backend !== null
                function onVersionListReady() {
                    rebuildVersionList()
                    if (toastManager) toastManager.show("版本列表已刷新")
                }
            }

            Component.onCompleted: rebuildVersionList()

            ScrollView {
                anchors.top: filterPills.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: selectBtn.top
                anchors.topMargin: 8
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                anchors.bottomMargin: 8
                clip: true

                ListView {
                    anchors.fill: parent
                    model: versionModel
                    spacing: 4
                    clip: true

                    delegate: Rectangle {
                        implicitHeight: 48
                        width: ListView.view.width
                        radius: StyleTokens.radiusMd
                        color: cardArea.containsMouse ? "#1E212A" : "transparent"
                        border.color: selectedVersionId === model.versionId ? "#3B82F6" : "transparent"
                        border.width: 1
                        opacity: 0
                        scale: cardArea.containsMouse ? 1.01 : 1.0
                        Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeInDuration; easing.type: AnimationTokens.itemFadeInEasing } }
                        Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }
                        Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                        Behavior on border.color { ColorAnimation { duration: 200 } }
                        Component.onCompleted: { opacity = 1 }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 8

                            Text {
                                text: model.versionId
                                color: StyleTokens.textPrimary
                                font.pixelSize: StyleTokens.fontSizeMd
                                font.bold: true
                                Layout.preferredWidth: 140
                            }

                            Rectangle {
                                radius: StyleTokens.radiusLg
                                height: 20
                                implicitWidth: typeTag.implicitWidth + 12
                                color: model.vtype === "release" ? "#10B981" : (model.vtype === "snapshot" ? "#F59E0B" : "#7E8596")
                                Text {
                                    id: typeTag
                                    anchors.centerIn: parent
                                    text: model.vtype === "release" ? "正式版" : (model.vtype === "snapshot" ? "快照" : "旧版")
                                    color: StyleTokens.textInverse
                                    font.pixelSize: StyleTokens.fontSizeSm
                                }
                            }
                        }

                        MouseArea {
                            id: cardArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: { selectedVersionId = model.versionId }
                        }
                    }
                }
            }

            // Select button
            Rectangle {
                id: selectBtn
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 12
                height: 42
                radius: StyleTokens.radiusLg
                color: selectArea.containsMouse ? "#2563EB" : (selectedVersionId !== "" ? "#3B82F6" : "#2A2F3A")
                opacity: selectedVersionId !== "" ? 1.0 : 0.5
                scale: selectArea.containsMouse ? 1.03 : 1.0
                Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }
                Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeOutDuration; easing.type: AnimationTokens.itemFadeOutEasing } }

                Text {
                    anchors.centerIn: parent
                    text: qsTr("选择此版本")
                    color: StyleTokens.textInverse
                    font.pixelSize: StyleTokens.fontSizeMd
                    font.bold: true
                }

                MouseArea {
                    id: selectArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: selectedVersionId !== "" ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: {
                        if (selectedVersionId !== "") root.versionSelected(selectedVersionId)
                    }
                }
            }

        }
    }
}
