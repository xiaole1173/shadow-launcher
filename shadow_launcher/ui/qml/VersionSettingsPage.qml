import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// ═══════════════════════════════════════════════════════════════════
//  VersionSettingsPage — 左侧边栏 + 右侧内容（StackLayout 模式）
//  为 Minecraft 启动器提供版本相关的设置与管理功能
// ═══════════════════════════════════════════════════════════════════

Rectangle {
    id: page
    color: "#121418"

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
    Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
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
                radius: 8
                color: backMouse.containsMouse ? "#1A2434" : "transparent"

                RowLayout {
                    anchors.centerIn: parent
                    spacing: 4
                    Text {
                        text: "←"
                        font.pixelSize: 16
                        color: backMouse.containsMouse ? "#3B82F6" : "#B4BAC6"
                        Behavior on color { ColorAnimation { duration: 200 } }
                    }
                    Text {
                        id: backLabel
                        text: "启动"
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        color: backMouse.containsMouse ? "#3B82F6" : "#B4BAC6"
                        Behavior on color { ColorAnimation { duration: 200 } }
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
                text: "版本设置"
                font.pixelSize: 18
                font.weight: Font.Bold
                color: "#F1F3F6"
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
                color: "#0E1018"
                radius: 8

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
                        radius: 6
                        color: page.currentSection === section ? "#1A1D24" : "transparent"

                        Behavior on color { ColorAnimation { duration: 200 } }

                        // Left accent bar
                        Rectangle {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.margins: 5
                            width: 3
                            radius: 2
                            color: page.currentSection === section ? "#3B82F6" : "transparent"

                            Behavior on color { ColorAnimation { duration: 200 } }
                        }

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 16
                            anchors.verticalCenter: parent.verticalCenter
                            text: label
                            font.pixelSize: 13
                            font.weight: page.currentSection === section ? Font.SemiBold : Font.Normal
                            color: page.currentSection === section ? "#F1F3F6" : "#B4BAC6"

                            Behavior on color { ColorAnimation { duration: 200 } }
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

                    Behavior on opacity {
                        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                    }

                    ColumnLayout {
                        id: settingsContent
                        width: parent.width - 4
                        spacing: 20

                        // ── 快捷方式 ──
                        Text {
                            text: "快捷方式"
                            font.pixelSize: 13
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
                                    { icon: "📁", label: "版本文件夹",    action: "versionFolder" },
                                    { icon: "📁", label: "存档文件夹",    action: "savesFolder" },
                                    { icon: "📁", label: "截图文件夹",    action: "screenshotsFolder" }
                                ]

                                delegate: Rectangle {
                                    Layout.preferredWidth: 160
                                    height: 42
                                    radius: 8
                                    color: "transparent"
                                    border.color: shortcutMouse.containsMouse ? "#3B82F6" : "#2A2F3A"
                                    border.width: 1
                                    scale: shortcutMouse.containsMouse ? 1.04 : 1.0

                                    Behavior on border.color { ColorAnimation { duration: 200 } }
                                    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

                                    RowLayout {
                                        anchors.centerIn: parent
                                        spacing: 6
                                        Text { text: modelData.icon; font.pixelSize: 16 }
                                        Text {
                                            text: modelData.label
                                            font.pixelSize: 13
                                            color: shortcutMouse.containsMouse ? "#3B82F6" : "#B4BAC6"
                                            Behavior on color { ColorAnimation { duration: 200 } }
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
                            text: "高级管理"
                            font.pixelSize: 13
                            font.weight: Font.SemiBold
                            color: "#7E8596"
                            Layout.leftMargin: 4
                            Layout.topMargin: 8
                        }

                        Rectangle {
                            Layout.preferredWidth: 220
                            height: 40
                            radius: 8
                            color: "transparent"
                            border.color: deleteBtnMouse.containsMouse ? "#EF4444" : "#2A2F3A"
                            border.width: 1
                            scale: deleteBtnMouse.containsMouse ? 1.04 : 1.0

                            Behavior on border.color { ColorAnimation { duration: 200 } }
                            Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

                            RowLayout {
                                anchors.centerIn: parent
                                spacing: 6
                                Text {
                                    text: "🗑"
                                    font.pixelSize: 16
                                    color: deleteBtnMouse.containsMouse ? "#EF4444" : "#B4BAC6"
                                    Behavior on color { ColorAnimation { duration: 200 } }
                                }
                                Text {
                                    text: "删除此版本"
                                    font.pixelSize: 13
                                    color: deleteBtnMouse.containsMouse ? "#EF4444" : "#B4BAC6"
                                    Behavior on color { ColorAnimation { duration: 200 } }
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
                            text: "特殊登录选项"
                            font.pixelSize: 13
                            font.weight: Font.SemiBold
                            color: "#7E8596"
                            Layout.leftMargin: 4
                            Layout.topMargin: 8
                        }

                        ColumnLayout {
                            Layout.leftMargin: 4
                            spacing: 8

                            Text {
                                text: "第三方登录"
                                font.pixelSize: 12
                                font.weight: Font.SemiBold
                                color: "#7E8596"
                            }

                            Text {
                                text: "第三方登录允许您使用自定义认证服务器进行游戏。\n注意：第三方登录将优先于普通登录方式。"
                                font.pixelSize: 12
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
                                text: "认证方式"
                                font.pixelSize: 13
                                color: "#B4BAC6"
                                Layout.preferredWidth: 70
                            }

                            Rectangle {
                                Layout.preferredWidth: 200
                                height: 36
                                radius: 8
                                color: "#1A1D24"
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
                                        font.pixelSize: 13
                                        color: "#B4BAC6"
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    indicator: Text {
                                        anchors.right: parent.right
                                        anchors.rightMargin: 10
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: "▼"
                                        font.pixelSize: 10
                                        color: "#7E8596"
                                    }

                                    delegate: ItemDelegate {
                                        width: authModeCombo.width
                                        height: 36
                                        contentItem: Text {
                                            text: modelData
                                            font.pixelSize: 13
                                            color: hovered ? "#F1F3F6" : "#B4BAC6"
                                            verticalAlignment: Text.AlignVCenter
                                            leftPadding: 10
                                        }
                                        background: Rectangle {
                                            color: hovered ? "#252A35" : "#1A1D24"
                                            radius: 4
                                        }
                                    }

                                    popup: Popup {
                                        y: authModeCombo.height
                                        width: authModeCombo.width
                                        height: implicitHeight
                                        padding: 4
                                        background: Rectangle {
                                            color: "#1A1D24"
                                            radius: 8
                                            border.color: "#2A2F3A"
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
                            color: "#F1F3F6"
                            font.pixelSize: 13
                            leftPadding: 12
                            topPadding: 10
                            bottomPadding: 10

                            background: Rectangle {
                                radius: 8
                                color: "#1A1D24"
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
                            color: "#F1F3F6"
                            font.pixelSize: 13
                            leftPadding: 12
                            topPadding: 10
                            bottomPadding: 10

                            background: Rectangle {
                                radius: 8
                                color: "#1A1D24"
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
                            radius: 8
                            color: applyAuthMouse.containsMouse ? "#2563EB" : "#3B82F6"
                            scale: applyAuthMouse.containsMouse ? 1.04 : 1.0

                            Behavior on color { ColorAnimation { duration: 200 } }
                            Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

                            Text {
                                anchors.centerIn: parent
                                text: "应用"
                                font.pixelSize: 13
                                font.weight: Font.SemiBold
                                color: "#FFFFFF"
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

                    Behavior on opacity {
                        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                    }

                    ColumnLayout {
                        id: integrityContent
                        width: parent.width - 48
                        x: 24
                        spacing: 16

                        Item { Layout.preferredHeight: 8 }

                        Text {
                            text: "游戏完整性校验"
                            font.pixelSize: 16
                            font.weight: Font.SemiBold
                            color: "#F1F3F6"
                        }

                        Text {
                            text: "校验游戏文件，确保所有必要的文件都存在且未被修改。"
                            font.pixelSize: 13
                            color: "#7E8596"
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            lineHeight: 1.5
                        }

                        // Start check button
                        Rectangle {
                            Layout.preferredWidth: 140
                            height: 40
                            radius: 8
                            color: integrityBtnMouse.containsMouse ? "#2563EB" : "#3B82F6"
                            scale: integrityBtnMouse.containsMouse ? 1.04 : 1.0

                            Behavior on color { ColorAnimation { duration: 200 } }
                            Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

                            Text {
                                anchors.centerIn: parent
                                text: "开始校验"
                                font.pixelSize: 14
                                font.weight: Font.SemiBold
                                color: "#FFFFFF"
                            }

                            MouseArea {
                                id: integrityBtnMouse
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                hoverEnabled: true
                                enabled: backend && !backend.verifyRunning
                                onClicked: {
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
                                radius: 3
                                color: "#1A1D24"

                                Rectangle {
                                    height: 6
                                    radius: 3
                                    width: (backend && backend.verifyProgressTotal > 0)
                                        ? parent.width * (backend.verifyProgressDone / backend.verifyProgressTotal)
                                        : 0
                                    color: "#3B82F6"
                                    Behavior on width { NumberAnimation { duration: 100 } }
                                }
                            }

                            Text {
                                text: (backend && backend.verifyProgressTotal > 0)
                                    ? "校验中... " + backend.verifyProgressDone + "/" + backend.verifyProgressTotal
                                    : "正在准备校验..."
                                font.pixelSize: 12
                                color: "#7E8596"
                            }
                        }

                        // Results list
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            Text {
                                text: "校验结果"
                                font.pixelSize: 13
                                font.weight: Font.SemiBold
                                color: "#B4BAC6"
                            }

                            Text {
                                text: (backend && backend.verifyResultText) ? backend.verifyResultText : "暂无校验结果"
                                font.pixelSize: 12
                                color: (backend && backend.verifyFinished !== undefined && backend.verifyFinished && backend.verifyResultText)
                                    ? "#4bc870" : "#7E8596"
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
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

                    Behavior on opacity {
                        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                    }

                    ColumnLayout {
                        id: resourceContent
                        width: parent.width - 48
                        x: 24
                        spacing: 16

                        Item { Layout.preferredHeight: 8 }

                        Text {
                            text: "资源包管理"
                            font.pixelSize: 16
                            font.weight: Font.SemiBold
                            color: "#F1F3F6"
                        }

                        Text {
                            text: "管理版本安装的资源包，启用或禁用以自定义游戏体验。"
                            font.pixelSize: 13
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
                                radius: 8
                                color: "transparent"
                                border.color: openFolderMouse.containsMouse ? "#3B82F6" : "#2A2F3A"
                                border.width: 1
                                scale: openFolderMouse.containsMouse ? 1.04 : 1.0

                                Behavior on border.color { ColorAnimation { duration: 200 } }
                                Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

                                Text {
                                    anchors.centerIn: parent
                                    text: "📂 打开文件夹"
                                    font.pixelSize: 12
                                    color: openFolderMouse.containsMouse ? "#3B82F6" : "#B4BAC6"
                                    Behavior on color { ColorAnimation { duration: 200 } }
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
                                radius: 8
                                color: "transparent"
                                border.color: addPackMouse.containsMouse ? "#3B82F6" : "#2A2F3A"
                                border.width: 1
                                scale: addPackMouse.containsMouse ? 1.04 : 1.0

                                Behavior on border.color { ColorAnimation { duration: 200 } }
                                Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

                                Text {
                                    anchors.centerIn: parent
                                    text: "➕ 添加资源包"
                                    font.pixelSize: 12
                                    color: addPackMouse.containsMouse ? "#3B82F6" : "#B4BAC6"
                                    Behavior on color { ColorAnimation { duration: 200 } }
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
                                text: "已安装的资源包"
                                font.pixelSize: 13
                                font.weight: Font.SemiBold
                                color: "#B4BAC6"
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                height: 44
                                radius: 8
                                color: "#1A1D24"

                                Text {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 12
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: "暂无资源包"
                                    font.pixelSize: 13
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

                    Behavior on opacity {
                        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                    }

                    ColumnLayout {
                        id: conditionalContent
                        width: parent.width
                        spacing: 0

                        Item { Layout.fillHeight: true }

                        ColumnLayout {
                            Layout.alignment: Qt.AlignHCenter
                            spacing: 12

                            Text {
                                text: "🛠️"
                                font.pixelSize: 48
                                Layout.alignment: Qt.AlignHCenter
                            }

                            Text {
                                text: "功能开发中..."
                                font.pixelSize: 16
                                font.weight: Font.Medium
                                color: "#7E8596"
                                Layout.alignment: Qt.AlignHCenter
                            }

                            Text {
                                text: "此功能将在后续版本中推出，敬请期待。"
                                font.pixelSize: 13
                                color: "#5A6173"
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
            color: "#1A1D24"
            radius: 12
            border.color: "#2A2F3A"
            border.width: 1
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 24
            spacing: 20

            Text {
                text: "确认删除？"
                font.pixelSize: 16
                font.weight: Font.SemiBold
                color: "#F1F3F6"
                Layout.alignment: Qt.AlignHCenter
            }

            Text {
                text: "此操作将永久删除该版本及其所有文件，无法恢复。"
                font.pixelSize: 13
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
                    radius: 8
                    color: "transparent"
                    border.color: cancelDelMouse.containsMouse ? "#3B82F6" : "#2A2F3A"
                    border.width: 1
                    scale: cancelDelMouse.containsMouse ? 1.04 : 1.0

                    Behavior on border.color { ColorAnimation { duration: 200 } }
                    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

                    Text {
                        anchors.centerIn: parent
                        text: "取消"
                        font.pixelSize: 13
                        color: cancelDelMouse.containsMouse ? "#3B82F6" : "#B4BAC6"
                        Behavior on color { ColorAnimation { duration: 200 } }
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
                    radius: 8
                    color: "transparent"
                    border.color: confirmDelMouse.containsMouse ? "#DC2626" : "#EF4444"
                    border.width: 1
                    scale: confirmDelMouse.containsMouse ? 1.04 : 1.0

                    Behavior on border.color { ColorAnimation { duration: 200 } }
                    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

                    Text {
                        anchors.centerIn: parent
                        text: "确认删除"
                        font.pixelSize: 13
                        font.weight: Font.SemiBold
                        color: confirmDelMouse.containsMouse ? "#FCA5A5" : "#EF4444"
                        Behavior on color { ColorAnimation { duration: 200 } }
                    }

                    MouseArea {
                        id: confirmDelMouse
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        hoverEnabled: true
                        onClicked: {
                            page._showDeleteConfirm = false
                            // TODO: trigger version deletion via backend
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
