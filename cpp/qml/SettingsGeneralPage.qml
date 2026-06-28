// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    color: "#121418"
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
    readonly property int    radius:           8

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
                        font.pixelSize: 15
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
                    font.pixelSize: 20
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
                    font.pixelSize: 12
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
                                font.pixelSize: 14
                            }
                            Label {
                                text: qsTr("每个游戏版本使用独立的 libraries 和 assets 文件夹")
                                color: root.colorTertiary
                                font.pixelSize: 11
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
                            radius: 12
                            color: d.versionIsolation ? root.colorAccent : root.colorQuaternary

                            Rectangle {
                                width: 18
                                height: 18
                                radius: 9
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
                    font.pixelSize: 12
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
                                font.pixelSize: 13
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
                                font.pixelSize: 13
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
                    font.pixelSize: 12
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
                                font.pixelSize: 13
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
                                font.pixelSize: 13
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
                                font.pixelSize: 13
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
                    font.pixelSize: 12
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
                                font.pixelSize: 13
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
                                font.pixelSize: 13
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
                                font.pixelSize: 13
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
                                font.pixelSize: 11
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
                                            font.pixelSize: 16
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
                                        font.pixelSize: 14
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
                                            font.pixelSize: 16
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
                            font.pixelSize: 18
                            Layout.alignment: Qt.AlignVCenter
                            Layout.topMargin: 14
                        }

                        // Height spinbox
                        ColumnLayout {
                            spacing: 4
                            Label {
                                text: qsTr("高度")
                                color: root.colorTertiary
                                font.pixelSize: 11
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
                                            font.pixelSize: 16
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
                                        font.pixelSize: 14
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
                                            font.pixelSize: 16
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

            // Bottom spacer
            Item { Layout.preferredHeight: 24 }
        }
    }
}
