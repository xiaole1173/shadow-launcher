// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    color: StyleTokens.bgSecondary
    id: root

    property var backend: null
    signal goBack()

    // ── Color Palette ──
    readonly property string colorBg:          "#1A1D24"
    readonly property string colorBorder:      "#2A2F3A"
    readonly property string colorPrimary:     "#F1F3F6"
    readonly property string colorSecondary:   "#B4BAC6"
    readonly property string colorTertiary:    "#7E8596"
    readonly property string colorQuaternary:  "#5A6173"
    readonly property string colorAccent:      "#3B82F6"
    readonly property string colorAccentHover: "#2563EB"
    readonly property string colorError:       "#EF4444"
    readonly property string colorSuccess:     "#10B981"
    readonly property string colorWarning:     "#F59E0B"
    readonly property int    radius: StyleTokens.radiusLg

    // ── State ──
    QtObject {
        id: d

        // Version isolation
        property bool versionIsolation: true

        // Launcher visibility: "keep" | "hideOnLaunch"
        property string launcherVisibility: "keep"

        // Process priority: "normal" | "high" | "realtime"
        property string processPriority: "normal"

        // Window size: "default" | "fullscreen" | "custom"
        property string windowSize: "default"
        property int customWidth: 1280
        property int customHeight: 720
    }

    // ── Page enter animation ──
    states: State {
        name: "visible"
        PropertyChanges { target: root; opacity: 1 }
        PropertyChanges { target: root; y: 0 }
    }

    transitions: Transition {
        NumberAnimation { properties: "opacity,y"; duration: 300; easing.type: Easing.OutCubic }
    }

    opacity: 0
    y: 20

    Component.onCompleted: state = "visible"

    // ═══════════════════════════════════════════════════════════
    //  LAYOUT
    // ═══════════════════════════════════════════════════════════
    Flickable {
        anchors.fill: parent
        contentHeight: contentColumn.implicitHeight + 40
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: contentColumn
            width: parent.width - 40
            x: 20
            spacing: 24

            // ── Top Bar ──
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                Layout.topMargin: 12

                Rectangle {
                    id: backButton
                    width: backLabel.implicitWidth + 24
                    height: 36
                    radius: root.radius
                    color: "transparent"
                    anchors.verticalCenter: parent.verticalCenter

                    Label {
                        id: backLabel
                        anchors.centerIn: parent
                        text: qsTr("← 设置")
                        color: root.colorTertiary
                        font.pixelSize: StyleTokens.fontSizeLg
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        hoverEnabled: true
                        onClicked: root.goBack()

                        Rectangle {
                            anchors.fill: parent
                            radius: root.radius
                            color: root.colorBorder
                            opacity: parent.containsMouse ? 0.3 : 0
                            Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeOutDuration; easing.type: AnimationTokens.itemFadeOutEasing } }
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    text: qsTr("启动设置")
                    color: root.colorPrimary
                    font.pixelSize: StyleTokens.fontSizeXl
                    font.bold: true
                }
            }

            // ── 版本隔离 ──
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                Label {
                    text: qsTr("版本隔离")
                    color: root.colorTertiary
                    font.pixelSize: StyleTokens.fontSizeSm
                    font.bold: true
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(56, isolationRow.implicitHeight + 24)
                    radius: root.radius
                    color: root.colorBg
                    border.color: root.colorBorder
                    border.width: 1

                    RowLayout {
                        id: isolationRow
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 12

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Label {
                                text: qsTr("各版本独立路径存放资源")
                                color: root.colorSecondary
                                font.pixelSize: StyleTokens.fontSizeMd
                            }
                            Label {
                                text: qsTr("每个游戏版本使用独立的 libraries 和 assets 文件夹")
                                color: root.colorTertiary
                                font.pixelSize: StyleTokens.fontSizeSm
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                        }

                        // Custom Switch
                        Rectangle {
                            id: versionSwitch
                            Layout.preferredWidth: 44
                            Layout.preferredHeight: 24
                            Layout.alignment: Qt.AlignVCenter
                            radius: StyleTokens.radiusXl
                            color: d.versionIsolation ? root.colorAccent : root.colorQuaternary

                            Rectangle {
                                width: 18
                                height: 18
                                radius: StyleTokens.radiusLg
                                color: root.colorPrimary
                                anchors.verticalCenter: parent.verticalCenter
                                x: d.versionIsolation ? parent.width - width - 3 : 3

                                Behavior on x { NumberAnimation { duration: 250; easing.type: Easing.InOutCubic } }
                            }

                            Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: d.versionIsolation = !d.versionIsolation
                            }
                        }
                    }
                }
            }

            // ── 启动器可见性 ──
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                Label {
                    text: qsTr("启动器可见性")
                    color: root.colorTertiary
                    font.pixelSize: StyleTokens.fontSizeSm
                    font.bold: true
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: visibilityRow.implicitHeight + 24
                    radius: root.radius
                    color: root.colorBg
                    border.color: root.colorBorder
                    border.width: 1

                    RowLayout {
                        id: visibilityRow
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8

                        // Option 1: 保持不变
                        Rectangle {
                            id: visKeep
                            Layout.fillWidth: true
                            Layout.preferredHeight: visKeepLabel.implicitHeight + 20
                            radius: root.radius
                            color: d.launcherVisibility === "keep" ? root.colorAccent : root.colorBg
                            border.color: d.launcherVisibility === "keep" ? root.colorAccent : root.colorBorder
                            border.width: 1

                            Label {
                                id: visKeepLabel
                                anchors.centerIn: parent
                                text: qsTr("保持不变")
                                color: d.launcherVisibility === "keep" ? "#FFFFFF" : root.colorSecondary
                                font.pixelSize: StyleTokens.fontSizeMd
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: d.launcherVisibility = "keep"
                            }

                            Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                        }

                        // Option 2: 启动后隐藏
                        Rectangle {
                            id: visHide
                            Layout.fillWidth: true
                            Layout.preferredHeight: visHideLabel.implicitHeight + 20
                            radius: root.radius
                            color: d.launcherVisibility === "hideOnLaunch" ? root.colorAccent : root.colorBg
                            border.color: d.launcherVisibility === "hideOnLaunch" ? root.colorAccent : root.colorBorder
                            border.width: 1

                            Label {
                                id: visHideLabel
                                anchors.centerIn: parent
                                text: qsTr("启动后隐藏，游戏退出后显示")
                                color: d.launcherVisibility === "hideOnLaunch" ? "#FFFFFF" : root.colorSecondary
                                font.pixelSize: StyleTokens.fontSizeMd
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: d.launcherVisibility = "hideOnLaunch"
                            }

                            Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                        }
                    }
                }
            }

            // ── 进程优先级 ──
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                Label {
                    text: qsTr("进程优先级")
                    color: root.colorTertiary
                    font.pixelSize: StyleTokens.fontSizeSm
                    font.bold: true
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: priorityRow.implicitHeight + 24
                    radius: root.radius
                    color: root.colorBg
                    border.color: root.colorBorder
                    border.width: 1

                    RowLayout {
                        id: priorityRow
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8

                        // Normal
                        Rectangle {
                            id: priNormal
                            Layout.fillWidth: true
                            Layout.preferredHeight: priNormalLabel.implicitHeight + 20
                            radius: root.radius
                            color: d.processPriority === "normal" ? root.colorAccent : root.colorBg
                            border.color: d.processPriority === "normal" ? root.colorAccent : root.colorBorder
                            border.width: 1

                            Label {
                                id: priNormalLabel
                                anchors.centerIn: parent
                                text: qsTr("正常")
                                color: d.processPriority === "normal" ? "#FFFFFF" : root.colorSecondary
                                font.pixelSize: StyleTokens.fontSizeMd
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: d.processPriority = "normal"
                            }

                            Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                        }

                        // High
                        Rectangle {
                            id: priHigh
                            Layout.fillWidth: true
                            Layout.preferredHeight: priHighLabel.implicitHeight + 20
                            radius: root.radius
                            color: d.processPriority === "high" ? root.colorAccent : root.colorBg
                            border.color: d.processPriority === "high" ? root.colorAccent : root.colorBorder
                            border.width: 1

                            Label {
                                id: priHighLabel
                                anchors.centerIn: parent
                                text: qsTr("高")
                                color: d.processPriority === "high" ? "#FFFFFF" : root.colorSecondary
                                font.pixelSize: StyleTokens.fontSizeMd
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: d.processPriority = "high"
                            }

                            Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                        }

                        // Realtime
                        Rectangle {
                            id: priRealtime
                            Layout.fillWidth: true
                            Layout.preferredHeight: priRealtimeLabel.implicitHeight + 20
                            radius: root.radius
                            color: d.processPriority === "realtime" ? root.colorAccent : root.colorBg
                            border.color: d.processPriority === "realtime" ? root.colorAccent : root.colorBorder
                            border.width: 1

                            Label {
                                id: priRealtimeLabel
                                anchors.centerIn: parent
                                text: qsTr("实时")
                                color: d.processPriority === "realtime" ? "#FFFFFF" : root.colorSecondary
                                font.pixelSize: StyleTokens.fontSizeMd
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: d.processPriority = "realtime"
                            }

                            Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                        }
                    }
                }
            }

            // ── 游戏窗口大小 ──
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                Label {
                    text: qsTr("游戏窗口大小")
                    color: root.colorTertiary
                    font.pixelSize: StyleTokens.fontSizeSm
                    font.bold: true
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: windowRow.implicitHeight + 24
                    radius: root.radius
                    color: root.colorBg
                    border.color: root.colorBorder
                    border.width: 1

                    RowLayout {
                        id: windowRow
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8

                        // Default
                        Rectangle {
                            id: winDefault
                            Layout.fillWidth: true
                            Layout.preferredHeight: winDefaultLabel.implicitHeight + 20
                            radius: root.radius
                            color: d.windowSize === "default" ? root.colorAccent : root.colorBg
                            border.color: d.windowSize === "default" ? root.colorAccent : root.colorBorder
                            border.width: 1

                            Label {
                                id: winDefaultLabel
                                anchors.centerIn: parent
                                text: qsTr("默认")
                                color: d.windowSize === "default" ? "#FFFFFF" : root.colorSecondary
                                font.pixelSize: StyleTokens.fontSizeMd
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: d.windowSize = "default"
                            }

                            Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                        }

                        // Fullscreen
                        Rectangle {
                            id: winFullscreen
                            Layout.fillWidth: true
                            Layout.preferredHeight: winFullscreenLabel.implicitHeight + 20
                            radius: root.radius
                            color: d.windowSize === "fullscreen" ? root.colorAccent : root.colorBg
                            border.color: d.windowSize === "fullscreen" ? root.colorAccent : root.colorBorder
                            border.width: 1

                            Label {
                                id: winFullscreenLabel
                                anchors.centerIn: parent
                                text: qsTr("全屏")
                                color: d.windowSize === "fullscreen" ? "#FFFFFF" : root.colorSecondary
                                font.pixelSize: StyleTokens.fontSizeMd
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: d.windowSize = "fullscreen"
                            }

                            Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                        }

                        // Custom resolution
                        Rectangle {
                            id: winCustom
                            Layout.fillWidth: true
                            Layout.preferredHeight: winCustomLabel.implicitHeight + 20
                            radius: root.radius
                            color: d.windowSize === "custom" ? root.colorAccent : root.colorBg
                            border.color: d.windowSize === "custom" ? root.colorAccent : root.colorBorder
                            border.width: 1

                            Label {
                                id: winCustomLabel
                                anchors.centerIn: parent
                                text: qsTr("指定分辨率")
                                color: d.windowSize === "custom" ? "#FFFFFF" : root.colorSecondary
                                font.pixelSize: StyleTokens.fontSizeMd
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: d.windowSize = "custom"
                            }

                            Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                        }
                    }
                }

                // Custom resolution inputs — animated reveal
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: d.windowSize === "custom" ? resolutionRow.implicitHeight + 16 : 0
                    Layout.topMargin: d.windowSize === "custom" ? 0 : -8
                    radius: root.radius
                    color: root.colorBg
                    border.color: root.colorBorder
                    border.width: d.windowSize === "custom" ? 1 : 0
                    clip: true
                    opacity: d.windowSize === "custom" ? 1 : 0

                    Behavior on Layout.preferredHeight { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
                    Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeInDuration; easing.type: AnimationTokens.itemFadeInEasing } }

                    RowLayout {
                        id: resolutionRow
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.margins: 12
                        spacing: 10

                        // Width spinbox
                        ColumnLayout {
                            spacing: 4
                            Label {
                                text: qsTr("宽度")
                                color: root.colorTertiary
                                font.pixelSize: StyleTokens.fontSizeSm
                            }

                            Rectangle {
                                Layout.preferredWidth: 120
                                Layout.preferredHeight: 36
                                radius: root.radius
                                color: root.colorBg
                                border.color: root.colorBorder
                                border.width: 1

                                RowLayout {
                                    anchors.fill: parent
                                    spacing: 0

                                    // Decrement
                                    Rectangle {
                                        Layout.preferredWidth: 30
                                        Layout.fillHeight: true
                                        color: "transparent"
                                        radius: root.radius

                                        Label {
                                            anchors.centerIn: parent
                                            text: "−"
                                            color: root.colorSecondary
                                            font.pixelSize: StyleTokens.fontSizeLg
                                            font.bold: true
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            hoverEnabled: true
                                            onClicked: {
                                                d.customWidth = Math.max(640, d.customWidth - 100)
                                            }

                                            Rectangle {
                                                anchors.fill: parent
                                                radius: root.radius
                                                color: root.colorBorder
                                                opacity: parent.containsMouse ? 0.4 : 0
                                                Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeOutDuration; easing.type: AnimationTokens.itemFadeOutEasing } }
                                            }
                                        }
                                    }

                                    // Value
                                    Label {
                                        Layout.fillWidth: true
                                        horizontalAlignment: Text.AlignHCenter
                                        text: d.customWidth
                                        color: root.colorPrimary
                                        font.pixelSize: StyleTokens.fontSizeMd
                                    }

                                    // Increment
                                    Rectangle {
                                        Layout.preferredWidth: 30
                                        Layout.fillHeight: true
                                        color: "transparent"
                                        radius: root.radius

                                        Label {
                                            anchors.centerIn: parent
                                            text: "+"
                                            color: root.colorSecondary
                                            font.pixelSize: StyleTokens.fontSizeLg
                                            font.bold: true
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            hoverEnabled: true
                                            onClicked: {
                                                d.customWidth = Math.min(7680, d.customWidth + 100)
                                            }

                                            Rectangle {
                                                anchors.fill: parent
                                                radius: root.radius
                                                color: root.colorBorder
                                                opacity: parent.containsMouse ? 0.4 : 0
                                                Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeOutDuration; easing.type: AnimationTokens.itemFadeOutEasing } }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Label {
                            text: "×"
                            color: root.colorQuaternary
                            font.pixelSize: StyleTokens.fontSizeXl
                            Layout.alignment: Qt.AlignVCenter
                            Layout.topMargin: 14
                        }

                        // Height spinbox
                        ColumnLayout {
                            spacing: 4
                            Label {
                                text: qsTr("高度")
                                color: root.colorTertiary
                                font.pixelSize: StyleTokens.fontSizeSm
                            }

                            Rectangle {
                                Layout.preferredWidth: 120
                                Layout.preferredHeight: 36
                                radius: root.radius
                                color: root.colorBg
                                border.color: root.colorBorder
                                border.width: 1

                                RowLayout {
                                    anchors.fill: parent
                                    spacing: 0

                                    // Decrement
                                    Rectangle {
                                        Layout.preferredWidth: 30
                                        Layout.fillHeight: true
                                        color: "transparent"
                                        radius: root.radius

                                        Label {
                                            anchors.centerIn: parent
                                            text: "−"
                                            color: root.colorSecondary
                                            font.pixelSize: StyleTokens.fontSizeLg
                                            font.bold: true
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            hoverEnabled: true
                                            onClicked: {
                                                d.customHeight = Math.max(360, d.customHeight - 100)
                                            }

                                            Rectangle {
                                                anchors.fill: parent
                                                radius: root.radius
                                                color: root.colorBorder
                                                opacity: parent.containsMouse ? 0.4 : 0
                                                Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeOutDuration; easing.type: AnimationTokens.itemFadeOutEasing } }
                                            }
                                        }
                                    }

                                    // Value
                                    Label {
                                        Layout.fillWidth: true
                                        horizontalAlignment: Text.AlignHCenter
                                        text: d.customHeight
                                        color: root.colorPrimary
                                        font.pixelSize: StyleTokens.fontSizeMd
                                    }

                                    // Increment
                                    Rectangle {
                                        Layout.preferredWidth: 30
                                        Layout.fillHeight: true
                                        color: "transparent"
                                        radius: root.radius

                                        Label {
                                            anchors.centerIn: parent
                                            text: "+"
                                            color: root.colorSecondary
                                            font.pixelSize: StyleTokens.fontSizeLg
                                            font.bold: true
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            hoverEnabled: true
                                            onClicked: {
                                                d.customHeight = Math.min(4320, d.customHeight + 100)
                                            }

                                            Rectangle {
                                                anchors.fill: parent
                                                radius: root.radius
                                                color: root.colorBorder
                                                opacity: parent.containsMouse ? 0.4 : 0
                                                Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeOutDuration; easing.type: AnimationTokens.itemFadeOutEasing } }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ── 下载设置 ──
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                property int _fileSource: root.backend ? root.backend.fileDownloadSource : 0
                property int _listSource: root.backend ? root.backend.listDownloadSource : 0
                property int _threads: root.backend ? root.backend.maxDownloadThreads : 64
                property double _speedLimit: root.backend ? root.backend.downloadSpeedLimitMB : -1

                function _fileSourceText() {
                    if (_fileSource === 0) return qsTr("尽量使用镜像源 (BMCLAPI)")
                    if (_fileSource === 1) return qsTr("尽量使用官方源")
                    return qsTr("速度过慢自动切换镜像源")
                }
                function _listSourceText() {
                    if (_listSource === 0) return qsTr("尽量使用镜像源 (BMCLAPI)")
                    if (_listSource === 1) return qsTr("尽量使用官方源")
                    return qsTr("速度过慢自动切换镜像源")
                }

                Label {
                    text: qsTr("下载")
                    color: root.colorTertiary
                    font.pixelSize: StyleTokens.fontSizeSm
                    font.bold: true
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: dlCard.implicitHeight + 28
                    radius: root.radius
                    color: root.colorBg
                    border.color: root.colorBorder
                    border.width: 1

                    ColumnLayout {
                        id: dlCard
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 16

                        // ── 文件下载源 ──
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Label { text: qsTr("文件下载源"); color: root.colorSecondary; font.pixelSize: StyleTokens.fontSizeMd }
                            DownloadDropdown {
                                id: fileSourceDropdown
                                Layout.fillWidth: true
                                model: [
                                    { text: qsTr("尽量使用镜像源 (BMCLAPI)"), value: 0 },
                                    { text: qsTr("尽量使用官方源"), value: 1 },
                                    { text: qsTr("速度过慢自动切换镜像源"), value: 2 }
                                ]
                                currentValue: _fileSource
                                onValueSelected: function(v) {
                                    _fileSource = v
                                    if (root.backend) root.backend.fileDownloadSource = v
                                }
                            }
                        }

                        // ── 版本列表源 ──
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Label { text: qsTr("版本列表源"); color: root.colorSecondary; font.pixelSize: StyleTokens.fontSizeMd }
                            DownloadDropdown {
                                id: listSourceDropdown
                                Layout.fillWidth: true
                                model: [
                                    { text: qsTr("尽量使用镜像源 (BMCLAPI)"), value: 0 },
                                    { text: qsTr("尽量使用官方源"), value: 1 },
                                    { text: qsTr("速度过慢自动切换镜像源"), value: 2 }
                                ]
                                currentValue: _listSource
                                onValueSelected: function(v) {
                                    _listSource = v
                                    if (root.backend) root.backend.listDownloadSource = v
                                }
                            }
                        }

                        // ── 最大线程数 ──
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: qsTr("最大线程数"); color: root.colorSecondary; font.pixelSize: StyleTokens.fontSizeMd }
                                Item { Layout.fillWidth: true }
                                Rectangle {
                                    Layout.preferredWidth: threadBadge.implicitWidth + 16
                                    Layout.preferredHeight: 24
                                    radius: StyleTokens.radiusXl; color: root.colorAccent; opacity: 0.85
                                    Label {
                                        id: threadBadge
                                        anchors.centerIn: parent
                                        text: _threads; color: "#FFFFFF"; font.pixelSize: StyleTokens.fontSizeMd; font.bold: true
                                    }
                                }
                            }
                            SteppedSlider {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 40
                                from: 1; to: 128; stepSize: 1
                                notches: [1, 8, 16, 32, 48, 64, 80, 96, 112, 128]
                                value: _threads
                                onValueChanged: function(v) {
                                    _threads = v
                                    if (root.backend) root.backend.maxDownloadThreads = v
                                }
                            }
                        }

                        // ── 速度限制 ──
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: qsTr("速度限制"); color: root.colorSecondary; font.pixelSize: StyleTokens.fontSizeMd }
                                Item { Layout.fillWidth: true }
                                Rectangle {
                                    Layout.preferredWidth: speedBadge.implicitWidth + 20
                                    Layout.preferredHeight: 24
                                    radius: StyleTokens.radiusXl; color: root.colorAccent; opacity: 0.85
                                    Label {
                                        id: speedBadge
                                        anchors.centerIn: parent
                                        text: _speedLimit < 0 ? qsTr("无限制") : _speedLimit.toFixed(1) + " MB/s"
                                        color: "#FFFFFF"; font.pixelSize: StyleTokens.fontSizeMd; font.bold: true
                                    }
                                }
                            }
                            SteppedSlider {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 40
                                from: 0; to: 21; stepSize: 1
                                notches: [0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 21]
                                function _indexToSpeed(idx) {
                                    if (idx === 0) return -1
                                    if (idx <= 10) return idx * 0.5
                                    return (idx - 10) * 2.0
                                }
                                function _speedToIndex(spd) {
                                    if (spd < 0) return 0
                                    if (spd <= 5.0) return Math.round(spd / 0.5)
                                    return Math.round(spd / 2.0) + 10
                                }
                                value: _speedToIndex(_speedLimit)
                                onValueChanged: function(v) {
                                    _speedLimit = _indexToSpeed(v)
                                    if (root.backend) root.backend.downloadSpeedLimitMB = _speedLimit
                                }
                            }
                        }
                    }
                }
            }

            // Bottom spacer
            Item { Layout.preferredHeight: 24 }
        }
    }
}
