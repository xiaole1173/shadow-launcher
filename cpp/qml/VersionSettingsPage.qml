// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// ═══════════════════════════════════════════════════════════════════
//  VersionSettingsPage — 左侧边栏 + 右侧内容（StackLayout 模式）
//  为 Minecraft 启动器提供版本相关的设置与管理功能
// ═══════════════════════════════════════════════════════════════════

Rectangle {
    width: parent ? parent.width : 800
    height: parent ? parent.height : 600
    id: page
    color: StyleTokens.bgSecondary

    // ── Properties ──────────────────────────────────────────────
    property var backend: null
    property int currentSection: 0
    property string currentVersionId: ""
    property bool hasModLoader: false
    property bool hasShaderLoader: false
    property bool hasDataPackSupport: false
    property bool hasModpackSupport: false

    // ── Signals ─────────────────────────────────────────────────
    signal goBack()
    signal authlibInjectorApplied(string url)

    // ── Internal state ──────────────────────────────────────────
    property string _authlibMode: "none"      // "none" | "authlib" | "custom"
    property string _authlibUrl: ""
    property string _customUrl: ""
    property bool _showDeleteConfirm: false

    // ── Verify state ────────────────────────────────────────────
    property var verifyFailedFiles: []
    property bool verifyHasFailed: false

    // ── Verify signal handlers ─────────────────────────────────
    Connections {
        target: page.backend
        enabled: page.backend !== null
        function onVerifyFailedFiles(files) {
            page.verifyFailedFiles = files || []
            page.verifyHasFailed = (files && files.length > 0)
        }
        function onVerifyFinished(allPassed) {
            if (typeof allPassed !== 'undefined' && allPassed) {
                page.verifyHasFailed = false
                page.verifyFailedFiles = []
            }
        }
    }

    // ═══════════════════════════════════════════════════════════
    //  SIDEBAR MODEL — rebuilds when conditional flags change
    // ═══════════════════════════════════════════════════════════
    ListModel {
        id: sidebarModel
        Component.onCompleted: rebuildSidebar()

        function rebuildSidebar() {
            clear()
            // Always-visible items
            append({ label: "设置",             section: 0 })
            append({ label: "游戏完整性校验",     section: 1 })
            append({ label: "资源包管理",         section: 2 })

            let nextSec = 3
            if (page.hasModLoader) {
                append({ label: "Mod管理", section: nextSec })
                nextSec++
            }
            if (page.hasShaderLoader) {
                append({ label: "光影管理", section: nextSec })
                nextSec++
            }
            if (page.hasDataPackSupport) {
                append({ label: "数据包管理", section: nextSec })
                nextSec++
            }
            if (page.hasModpackSupport) {
                append({ label: "整合包管理", section: nextSec })
                nextSec++
            }
        }
    }

    // ═══════════════════════════════════════════════════════════
    //  PAGE-ENTER ANIMATION
    // ═══════════════════════════════════════════════════════════
    opacity: 0
    Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeInDuration; easing.type: AnimationTokens.itemFadeInEasing } }
    Component.onCompleted: opacity = 1

    // ═══════════════════════════════════════════════════════════
    //  LAYOUT
    // ═══════════════════════════════════════════════════════════
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 20

        // ── Top bar ─────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            // Back button
            Rectangle {
                width: backLabel.implicitWidth + 24
                height: 36
                radius: StyleTokens.radiusLg
                color: backMouse.containsMouse ? "#1A2434" : "transparent"

                RowLayout {
                    anchors.centerIn: parent
                    spacing: 4
                    Text {
                        text: "←"
                        font.pixelSize: StyleTokens.fontSizeLg
                        color: backMouse.containsMouse ? "#3B82F6" : "#B4BAC6"
                        Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                    }
                    Text {
                        id: backLabel
                        text: qsTr("启动")
                        font.pixelSize: StyleTokens.fontSizeMd
                        font.weight: Font.Medium
                        color: backMouse.containsMouse ? "#3B82F6" : "#B4BAC6"
                        Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                    }
                }

                MouseArea {
                    id: backMouse
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    onClicked: page.goBack()
                }
            }

            Item { Layout.fillWidth: true }

            Text {
                text: qsTr("版本设置")
                font.pixelSize: StyleTokens.fontSizeXl
                font.weight: Font.Bold
                color: StyleTokens.textPrimary
            }

            Item { Layout.fillWidth: true }
            Item { width: backLabel.implicitWidth + 24 }
        }

        // ── Main content: sidebar + content area ─────────────────
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // ═══════════════════════════════════════════════════
            //  LEFT SIDEBAR  (~180px, bg #0E1018)
            // ═══════════════════════════════════════════════════
            Rectangle {
                Layout.preferredWidth: 180
                Layout.fillHeight: true
                color: StyleTokens.bgPrimary
                radius: StyleTokens.radiusLg

                ListView {
                    id: sidebarList
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 2
                    model: sidebarModel
                    clip: true

                    delegate: Rectangle {
                        width: sidebarList.width - 16
                        height: 38
                        radius: StyleTokens.radiusMd
                        color: page.currentSection === section ? "#1A1D24" : "transparent"

                        Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }

                        // Left accent bar
                        Rectangle {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.margins: 5
                            width: 3
                            radius: StyleTokens.radiusXs
                            color: page.currentSection === section ? "#3B82F6" : "transparent"

                            Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                        }

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 16
                            anchors.verticalCenter: parent.verticalCenter
                            text: label
                            font.pixelSize: StyleTokens.fontSizeMd
                            font.weight: page.currentSection === section ? Font.SemiBold : Font.Normal
                            color: page.currentSection === section ? "#F1F3F6" : "#B4BAC6"

                            Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true
                            onClicked: page.currentSection = section
                        }
                    }
                }
            }

            // ═══════════════════════════════════════════════════
            //  RIGHT CONTENT AREA  (StackLayout-style by visibility)
            // ═══════════════════════════════════════════════════
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: 0
                color: "transparent"

                // ── Section 0: 设置 ──────────────────────────────
                Flickable {
                    id: section0Flickable
                    anchors.fill: parent
                    contentHeight: settingsContent.height + 40
                    clip: true
                    visible: page.currentSection === 0
                    opacity: page.currentSection === 0 ? 1 : 0

                    Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeOutDuration; easing.type: AnimationTokens.itemFadeOutEasing } }

                    ColumnLayout {
                        id: settingsContent
                        width: parent.width - 4
                        spacing: 20

                        // ── 快捷方式 ──
                        Text {
                            text: qsTr("快捷方式")
                            font.pixelSize: StyleTokens.fontSizeMd
                            font.weight: Font.SemiBold
                            color: "#7E8596"
                            Layout.leftMargin: 4
                            Layout.topMargin: 8
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 3
                            columnSpacing: 12
                            rowSpacing: 12

                            Repeater {
                                model: [
                                    { icon: "[文件夹]", label: "版本文件夹",    action: "versionFolder" },
                                    { icon: "[文件夹]", label: "存档文件夹",    action: "savesFolder" },
                                    { icon: "[文件夹]", label: "截图文件夹",    action: "screenshotsFolder" }
                                ]

                                delegate: Rectangle {
                                    Layout.preferredWidth: 160
                                    height: 42
                                    radius: StyleTokens.radiusLg
                                    color: "transparent"
                                    border.color: shortcutMouse.containsMouse ? "#3B82F6" : "#2A2F3A"
                                    border.width: 1
                                    scale: shortcutMouse.containsMouse ? 1.04 : 1.0

                                    Behavior on border.color { ColorAnimation { duration: 200 } }
                                    Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }

                                    RowLayout {
                                        anchors.centerIn: parent
                                        spacing: 6
                                        Text { text: modelData.icon; font.pixelSize: StyleTokens.fontSizeLg }
                                        Text {
                                            text: modelData.label
                                            font.pixelSize: StyleTokens.fontSizeMd
                                            color: shortcutMouse.containsMouse ? "#3B82F6" : "#B4BAC6"
                                            Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                                        }
                                    }

                                    MouseArea {
                                        id: shortcutMouse
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        hoverEnabled: true
                                        onClicked: console.log("Open " + modelData.action)
                                    }
                                }
                            }
                        }

                        // ── 高级管理 ──
                        Text {
                            text: qsTr("高级管理")
                            font.pixelSize: StyleTokens.fontSizeMd
                            font.weight: Font.SemiBold
                            color: "#7E8596"
                            Layout.leftMargin: 4
                            Layout.topMargin: 8
                        }

                        Rectangle {
                            Layout.preferredWidth: 220
                            height: 40
                            radius: StyleTokens.radiusLg
                            color: "transparent"
                            border.color: deleteBtnMouse.containsMouse ? "#EF4444" : "#2A2F3A"
                            border.width: 1
                            scale: deleteBtnMouse.containsMouse ? 1.04 : 1.0

                            Behavior on border.color { ColorAnimation { duration: 200 } }
                            Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }

                            RowLayout {
                                anchors.centerIn: parent
                                spacing: 6
                                Text {
                                    text: "[删除]"
                                    font.pixelSize: StyleTokens.fontSizeLg
                                    color: deleteBtnMouse.containsMouse ? "#EF4444" : "#B4BAC6"
                                    Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                                }
                                Text {
                                    text: qsTr("删除此版本")
                                    font.pixelSize: StyleTokens.fontSizeMd
                                    color: deleteBtnMouse.containsMouse ? "#EF4444" : "#B4BAC6"
                                    Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                                }
                            }

                            MouseArea {
                                id: deleteBtnMouse
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                hoverEnabled: true
                                onClicked: page._showDeleteConfirm = true
                            }
                        }

                        // ── 特殊登录选项 ──
                        Text {
                            text: qsTr("特殊登录选项")
                            font.pixelSize: StyleTokens.fontSizeMd
                            font.weight: Font.SemiBold
                            color: "#7E8596"
                            Layout.leftMargin: 4
                            Layout.topMargin: 8
                        }

                        ColumnLayout {
                            Layout.leftMargin: 4
                            spacing: 8

                            Text {
                                text: qsTr("第三方登录")
                                font.pixelSize: StyleTokens.fontSizeSm
                                font.weight: Font.SemiBold
                                color: "#7E8596"
                            }

                            Text {
                                text: qsTr("第三方登录允许您使用自定义认证服务器进行游戏。\n注意：第三方登录将优先于普通登录方式。")
                                font.pixelSize: StyleTokens.fontSizeSm
                                color: "#7E8596"
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                lineHeight: 1.5
                            }
                        }

                        // Authlib mode dropdown
                        RowLayout {
                            Layout.leftMargin: 4
                            spacing: 12

                            Text {
                                text: qsTr("认证方式")
                                font.pixelSize: StyleTokens.fontSizeMd
                                color: "#B4BAC6"
                                Layout.preferredWidth: 70
                            }

                            Rectangle {
                                Layout.preferredWidth: 200
                                height: 36
                                radius: StyleTokens.radiusLg
                                color: StyleTokens.surfaceLight
                                border.color: authComboHover.containsMouse ? "#3B82F6" : "#2A2F3A"
                                border.width: 1

                                Behavior on border.color { ColorAnimation { duration: 200 } }

                                ComboBox {
                                    id: authModeCombo
                                    anchors.fill: parent
                                    anchors.margins: 1
                                    model: ["无", "Authlib-Injector", "自定义"]
                                    currentIndex: 0
                                    flat: true

                                    background: Item {}
                                    contentItem: Text {
                                        leftPadding: 10
                                        text: authModeCombo.displayText
                                        font.pixelSize: StyleTokens.fontSizeMd
                                        color: "#B4BAC6"
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    indicator: Text {
                                        anchors.right: parent.right
                                        anchors.rightMargin: 10
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: "▼"
                                        font.pixelSize: StyleTokens.fontSizeXs
                                        color: "#7E8596"
                                    }

                                    delegate: ItemDelegate {
                                        width: authModeCombo.width
                                        height: 36
                                        contentItem: Text {
                                            text: modelData
                                            font.pixelSize: StyleTokens.fontSizeMd
                                            color: hovered ? "#F1F3F6" : "#B4BAC6"
                                            verticalAlignment: Text.AlignVCenter
                                            leftPadding: 10
                                        }
                                        background: Rectangle {
                                            color: hovered ? "#252A35" : "#1A1D24"
                                            radius: StyleTokens.radiusSm
                                        }
                                    }

                                    popup: Popup {
                                        y: authModeCombo.height
                                        width: authModeCombo.width
                                        height: implicitHeight
                                        padding: 4
                                        background: Rectangle {
                                            color: StyleTokens.surfaceLight
                                            radius: StyleTokens.radiusLg
                                            border.color: StyleTokens.bgHover
                                        }
                                        contentItem: ListView {
                                            clip: true
                                            implicitHeight: contentHeight
                                            model: authModeCombo.popup.visible ? authModeCombo.delegateModel : null
                                            currentIndex: authModeCombo.highlightedIndex
                                        }
                                    }

                                    onCurrentIndexChanged: {
                                        if (currentIndex === 0) page._authlibMode = "none"
                                        else if (currentIndex === 1) page._authlibMode = "authlib"
                                        else page._authlibMode = "custom"
                                    }
                                }

                                MouseArea {
                                    id: authComboHover
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    hoverEnabled: true
                                    acceptedButtons: Qt.NoButton
                                }
                            }
                        }

                        // Authlib-Injector URL field
                        TextField {
                            id: authlibUrlField
                            Layout.fillWidth: true
                            Layout.leftMargin: 82
                            Layout.rightMargin: 40
                            visible: page._authlibMode === "authlib"
                            placeholderText: "Authlib-Injector 服务器地址..."
                            placeholderTextColor: "#5A6173"
                            color: StyleTokens.textPrimary
                            font.pixelSize: StyleTokens.fontSizeMd
                            leftPadding: 12
                            topPadding: 10
                            bottomPadding: 10

                            background: Rectangle {
                                radius: StyleTokens.radiusLg
                                color: StyleTokens.surfaceLight
                                border.color: authlibUrlField.activeFocus ? "#3B82F6" : "#2A2F3A"
                                border.width: 1
                                Behavior on border.color { ColorAnimation { duration: 200 } }
                            }

                            onTextChanged: page._authlibUrl = text
                        }

                        // Custom URL field
                        TextField {
                            id: customUrlField
                            Layout.fillWidth: true
                            Layout.leftMargin: 82
                            Layout.rightMargin: 40
                            visible: page._authlibMode === "custom"
                            placeholderText: "自定义认证服务器地址..."
                            placeholderTextColor: "#5A6173"
                            color: StyleTokens.textPrimary
                            font.pixelSize: StyleTokens.fontSizeMd
                            leftPadding: 12
                            topPadding: 10
                            bottomPadding: 10

                            background: Rectangle {
                                radius: StyleTokens.radiusLg
                                color: StyleTokens.surfaceLight
                                border.color: customUrlField.activeFocus ? "#3B82F6" : "#2A2F3A"
                                border.width: 1
                                Behavior on border.color { ColorAnimation { duration: 200 } }
                            }

                            onTextChanged: page._customUrl = text
                        }

                        // Apply button
                        Rectangle {
                            Layout.leftMargin: 82
                            Layout.topMargin: 8
                            width: 80
                            height: 34
                            radius: StyleTokens.radiusLg
                            color: applyAuthMouse.containsMouse ? "#2563EB" : "#3B82F6"
                            scale: applyAuthMouse.containsMouse ? 1.04 : 1.0

                            Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                            Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }

                            Text {
                                anchors.centerIn: parent
                                text: qsTr("应用")
                                font.pixelSize: StyleTokens.fontSizeMd
                                font.weight: Font.SemiBold
                                color: StyleTokens.textInverse
                            }

                            MouseArea {
                                id: applyAuthMouse
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                hoverEnabled: true
                                onClicked: {
                                    let url = page._authlibMode === "authlib" ? page._authlibUrl :
                                              page._authlibMode === "custom"  ? page._customUrl : ""
                                    page.authlibInjectorApplied(url)
                                }
                            }
                        }
                    }
                }

                // ── Section 1: 游戏完整性校验 ────────────────────
                Flickable {
                    id: section1Flickable
                    anchors.fill: parent
                    contentHeight: integrityContent.height + 40
                    clip: true
                    visible: page.currentSection === 1
                    opacity: page.currentSection === 1 ? 1 : 0

                    Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeOutDuration; easing.type: AnimationTokens.itemFadeOutEasing } }

                    ColumnLayout {
                        id: integrityContent
                        width: parent.width - 48
                        x: 24
                        spacing: 16

                        Item { Layout.preferredHeight: 8 }

                        Text {
                            text: qsTr("游戏完整性校验")
                            font.pixelSize: StyleTokens.fontSizeLg
                            font.weight: Font.SemiBold
                            color: StyleTokens.textPrimary
                        }

                        Text {
                            text: qsTr("校验游戏文件，确保所有必要的文件都存在且未被修改。")
                            font.pixelSize: StyleTokens.fontSizeMd
                            color: "#7E8596"
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            lineHeight: 1.5
                        }

                        // Start check button
                        Rectangle {
                            Layout.preferredWidth: 140
                            height: 40
                            radius: StyleTokens.radiusLg
                            color: integrityBtnMouse.containsMouse ? "#2563EB" : "#3B82F6"
                            scale: integrityBtnMouse.containsMouse ? 1.04 : 1.0

                            Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                            Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }

                            Text {
                                anchors.centerIn: parent
                                text: qsTr("开始校验")
                                font.pixelSize: StyleTokens.fontSizeMd
                                font.weight: Font.SemiBold
                                color: StyleTokens.textInverse
                            }

                            MouseArea {
                                id: integrityBtnMouse
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                hoverEnabled: true
                                enabled: backend && !backend.verifyRunning
                                onClicked: {
                                    page.verifyFailedFiles = []
                                    page.verifyHasFailed = false
                                    if (backend) backend.verifyVersion(page.currentVersionId)
                                }
                            }
                        }

                        // Progress bar
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            visible: backend ? backend.verifyRunning : false

                            Rectangle {
                                Layout.fillWidth: true
                                height: 6
                                radius: StyleTokens.radiusXs
                                color: StyleTokens.surfaceLight

                                Rectangle {
                                    height: 6
                                    radius: StyleTokens.radiusXs
                                    width: (backend && backend.verifyTotal > 0)
                                        ? parent.width * (backend.verifyChecked / backend.verifyTotal)
                                        : 0
                                    color: StyleTokens.accent
                                    Behavior on width { NumberAnimation { duration: 100 } }
                                }
                            }

                            Text {
                                text: (backend && backend.verifyTotal > 0)
                                    ? ((backend.repairRunning
                                        ? "修复中... "
                                        : "校验中... ") + backend.verifyChecked + "/" + backend.verifyTotal)
                                    : "正在收集文件信息..."
                                font.pixelSize: StyleTokens.fontSizeSm
                                color: "#7E8596"
                            }

                            // Cancel button
                            Rectangle {
                                Layout.preferredWidth: 100
                                height: 28
                                radius: StyleTokens.radiusMd
                                color: cancelMouse.containsMouse ? "#444" : "#333"
                                border.color: "#555"

                                Text {
                                    anchors.centerIn: parent
                                    text: qsTr("取消校验")
                                    font.pixelSize: StyleTokens.fontSizeSm
                                    color: StyleTokens.textSecondary
                                }

                                MouseArea {
                                    id: cancelMouse
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    hoverEnabled: true
                                    onClicked: { if (backend) backend.cancelVerify() }
                                }
                            }
                        }

                        // Results list
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            Text {
                                text: qsTr("校验结果")
                                font.pixelSize: StyleTokens.fontSizeMd
                                font.weight: Font.SemiBold
                                color: "#B4BAC6"
                            }

                            Text {
                                text: (backend && backend.verifyResultText) ? backend.verifyResultText : "暂无校验结果"
                                font.pixelSize: StyleTokens.fontSizeSm
                                color: (backend && backend.verifyFinished !== undefined && backend.verifyFinished && backend.verifyResultText)
                                    ? "#4bc870" : "#7E8596"
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }

                            // Failed files list
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: Math.min(page.verifyFailedFiles.length * 21 + 8, 160)
                                color: StyleTokens.bgSecondary
                                radius: StyleTokens.radiusSm
                                visible: page.verifyHasFailed && page.verifyFailedFiles.length > 0
                                clip: true

                                ListView {
                                    anchors.fill: parent
                                    anchors.margins: 4
                                    model: page.verifyFailedFiles.slice(0, 8)
                                    spacing: 1
                                    delegate: Text {
                                        text: "失败: " + modelData
                                        color: "#c06060"
                                        font.pixelSize: StyleTokens.fontSizeSm
                                        font.family: "Consolas, monospace"
                                        width: parent.width
                                        elide: Text.ElideRight
                                    }
                                }

                                Text {
                                    anchors.bottom: parent.bottom
                                    anchors.right: parent.right
                                    anchors.margins: 4
                                    text: page.verifyFailedFiles.length > 8 ? ("... 共 " + page.verifyFailedFiles.length + " 个损坏文件") : ""
                                    color: "#806060"
                                    font.pixelSize: StyleTokens.fontSizeXs
                                    visible: page.verifyFailedFiles.length > 8
                                }
                            }

                            // Cleanup button (visible after failed verify)
                            Rectangle {
                                Layout.fillWidth: true
                                height: 32
                                radius: StyleTokens.radiusMd
                                color: StyleTokens.errorBg
                                border.color: StyleTokens.errorBg
                                visible: page.verifyHasFailed && page.verifyFailedFiles.length > 0
                                opacity: cleanupMouse.containsMouse ? 0.9 : 0.7
                                Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeOutDuration; easing.type: AnimationTokens.itemFadeOutEasing } }

                                Text {
                                    anchors.centerIn: parent
                                    text: qsTr("清理损坏文件")
                                    font.pixelSize: StyleTokens.fontSizeSm
                                    color: "#ff8080"
                                }

                                MouseArea {
                                    id: cleanupMouse
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    hoverEnabled: true
                                    onClicked: {
                                        if (backend) {
                                            backend.cleanCorruptVersion(page.currentVersionId)
                                            page.verifyFailedFiles = []
                                            page.verifyHasFailed = false
                                        }
                                    }
                                }
                            }

                            // Repair button — one-click re-download
                            Rectangle {
                                Layout.fillWidth: true
                                height: 32
                                radius: StyleTokens.radiusMd
                                color: StyleTokens.bgInput
                                border.color: StyleTokens.bgHover
                                visible: page.verifyHasFailed && page.verifyFailedFiles.length > 0
                                opacity: repairMouse.containsMouse ? 0.95 : 0.8
                                Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeOutDuration; easing.type: AnimationTokens.itemFadeOutEasing } }

                                Text {
                                    anchors.centerIn: parent
                                    text: qsTr("一键修复缺失文件")
                                    font.pixelSize: StyleTokens.fontSizeSm
                                    color: StyleTokens.accentLink
                                }

                                MouseArea {
                                    id: repairMouse
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    hoverEnabled: true
                                    onClicked: {
                                        if (backend) {
                                            backend.repairVersion(page.currentVersionId)
                                        }
                                    }
                                }
                            }

                            // Hint below buttons
                            Text {
                                text: qsTr("提示: 修复仅重新下载损坏/缺失的文件，已通过校验的文件不会重复下载")
                                color: "#807880"
                                font.pixelSize: StyleTokens.fontSizeSm
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                                visible: page.verifyHasFailed && page.verifyFailedFiles.length > 0
                            }
                        }
                    }
                }

                // ── Section 2: 资源包管理 ─────────────────────────
                Flickable {
                    id: section2Flickable
                    anchors.fill: parent
                    contentHeight: resourceContent.height + 40
                    clip: true
                    visible: page.currentSection === 2
                    opacity: page.currentSection === 2 ? 1 : 0

                    Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeOutDuration; easing.type: AnimationTokens.itemFadeOutEasing } }

                    ColumnLayout {
                        id: resourceContent
                        width: parent.width - 48
                        x: 24
                        spacing: 16

                        Item { Layout.preferredHeight: 8 }

                        Text {
                            text: qsTr("资源包管理")
                            font.pixelSize: StyleTokens.fontSizeLg
                            font.weight: Font.SemiBold
                            color: StyleTokens.textPrimary
                        }

                        Text {
                            text: qsTr("管理版本安装的资源包，启用或禁用以自定义游戏体验。")
                            font.pixelSize: StyleTokens.fontSizeMd
                            color: "#7E8596"
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            lineHeight: 1.5
                        }

                        // Action buttons row
                        RowLayout {
                            spacing: 12

                            Rectangle {
                                width: 130
                                height: 36
                                radius: StyleTokens.radiusLg
                                color: "transparent"
                                border.color: openFolderMouse.containsMouse ? "#3B82F6" : "#2A2F3A"
                                border.width: 1
                                scale: openFolderMouse.containsMouse ? 1.04 : 1.0

                                Behavior on border.color { ColorAnimation { duration: 200 } }
                                Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }

                                Text {
                                    anchors.centerIn: parent
                                    text: qsTr("[打开] 打开文件夹")
                                    font.pixelSize: StyleTokens.fontSizeSm
                                    color: openFolderMouse.containsMouse ? "#3B82F6" : "#B4BAC6"
                                    Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                                }

                                MouseArea {
                                    id: openFolderMouse
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    hoverEnabled: true
                                    onClicked: { /* TODO */ }
                                }
                            }

                            Rectangle {
                                width: 130
                                height: 36
                                radius: StyleTokens.radiusLg
                                color: "transparent"
                                border.color: addPackMouse.containsMouse ? "#3B82F6" : "#2A2F3A"
                                border.width: 1
                                scale: addPackMouse.containsMouse ? 1.04 : 1.0

                                Behavior on border.color { ColorAnimation { duration: 200 } }
                                Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }

                                Text {
                                    anchors.centerIn: parent
                                    text: qsTr("[添加] 添加资源包")
                                    font.pixelSize: StyleTokens.fontSizeSm
                                    color: addPackMouse.containsMouse ? "#3B82F6" : "#B4BAC6"
                                    Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                                }

                                MouseArea {
                                    id: addPackMouse
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    hoverEnabled: true
                                    onClicked: { /* TODO */ }
                                }
                            }
                        }

                        // Resource pack list placeholder
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            Text {
                                text: qsTr("已安装的资源包")
                                font.pixelSize: StyleTokens.fontSizeMd
                                font.weight: Font.SemiBold
                                color: "#B4BAC6"
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                height: 44
                                radius: StyleTokens.radiusLg
                                color: StyleTokens.surfaceLight

                                Text {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 12
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: qsTr("暂无资源包")
                                    font.pixelSize: StyleTokens.fontSizeMd
                                    color: "#7E8596"
                                }
                            }
                        }
                    }
                }

                // ── Sections 3-6: Conditional placeholders ─────────
                Flickable {
                    id: sectionConditionalFlickable
                    anchors.fill: parent
                    contentHeight: conditionalContent.height + 40
                    clip: true
                    visible: page.currentSection >= 3
                    opacity: page.currentSection >= 3 ? 1 : 0

                    Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeOutDuration; easing.type: AnimationTokens.itemFadeOutEasing } }

                    ColumnLayout {
                        id: conditionalContent
                        width: parent.width
                        spacing: 0

                        Item { Layout.fillHeight: true }

                        ColumnLayout {
                            Layout.alignment: Qt.AlignHCenter
                            spacing: 12

                            Text {
                                text: "[工具]"
                                font.pixelSize: 48
                                Layout.alignment: Qt.AlignHCenter
                            }

                            Text {
                                text: qsTr("功能开发中...")
                                font.pixelSize: StyleTokens.fontSizeLg
                                font.weight: Font.Medium
                                color: "#7E8596"
                                Layout.alignment: Qt.AlignHCenter
                            }

                            Text {
                                text: qsTr("此功能将在后续版本中推出，敬请期待。")
                                font.pixelSize: StyleTokens.fontSizeMd
                                color: StyleTokens.textMuted
                                Layout.alignment: Qt.AlignHCenter
                            }
                        }

                        Item { Layout.fillHeight: true }
                    }
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════
    //  DELETE CONFIRMATION POPUP
    // ═══════════════════════════════════════════════════════════
    Popup {
        id: deleteConfirmPopup
        visible: page._showDeleteConfirm
        modal: true
        closePolicy: Popup.NoAutoClose
        x: (page.width - width) / 2
        y: (page.height - height) / 2
        width: 380
        height: 200

        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 200; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.95; to: 1; duration: 200; easing.type: Easing.OutCubic }
        }

        background: Rectangle {
            color: StyleTokens.surfaceLight
            radius: StyleTokens.radiusXl
            border.color: StyleTokens.bgHover
            border.width: 1
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 24
            spacing: 20

            Text {
                text: qsTr("确认删除？")
                font.pixelSize: StyleTokens.fontSizeLg
                font.weight: Font.SemiBold
                color: StyleTokens.textPrimary
                Layout.alignment: Qt.AlignHCenter
            }

            Text {
                text: qsTr("此操作将永久删除该版本及其所有文件，无法恢复。")
                font.pixelSize: StyleTokens.fontSizeMd
                color: "#B4BAC6"
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 16

                // Cancel button
                Rectangle {
                    width: 100
                    height: 36
                    radius: StyleTokens.radiusLg
                    color: "transparent"
                    border.color: cancelDelMouse.containsMouse ? "#3B82F6" : "#2A2F3A"
                    border.width: 1
                    scale: cancelDelMouse.containsMouse ? 1.04 : 1.0

                    Behavior on border.color { ColorAnimation { duration: 200 } }
                    Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }

                    Text {
                        anchors.centerIn: parent
                        text: qsTr("取消")
                        font.pixelSize: StyleTokens.fontSizeMd
                        color: cancelDelMouse.containsMouse ? "#3B82F6" : "#B4BAC6"
                        Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                    }

                    MouseArea {
                        id: cancelDelMouse
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        hoverEnabled: true
                        onClicked: page._showDeleteConfirm = false
                    }
                }

                // Confirm delete button
                Rectangle {
                    width: 100
                    height: 36
                    radius: StyleTokens.radiusLg
                    color: "transparent"
                    border.color: confirmDelMouse.containsMouse ? "#DC2626" : "#EF4444"
                    border.width: 1
                    scale: confirmDelMouse.containsMouse ? 1.04 : 1.0

                    Behavior on border.color { ColorAnimation { duration: 200 } }
                    Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }

                    Text {
                        anchors.centerIn: parent
                        text: qsTr("确认删除")
                        font.pixelSize: StyleTokens.fontSizeMd
                        font.weight: Font.SemiBold
                        color: confirmDelMouse.containsMouse ? "#FCA5A5" : "#EF4444"
                        Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                    }

                    MouseArea {
                        id: confirmDelMouse
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        hoverEnabled: true
                        onClicked: {
                            page._showDeleteConfirm = false
                            if (backend && page.currentVersionId) {
                                backend.deleteVersion(page.currentVersionId)
                            }
                            page.goBack()
                        }
                    }
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════
    //  WATCHERS: rebuild sidebar when conditional flags change
    // ═══════════════════════════════════════════════════════════
    onHasModLoaderChanged:      sidebarModel.rebuildSidebar()
    onHasShaderLoaderChanged:   sidebarModel.rebuildSidebar()
    onHasDataPackSupportChanged: sidebarModel.rebuildSidebar()
    onHasModpackSupportChanged: sidebarModel.rebuildSidebar()
}
