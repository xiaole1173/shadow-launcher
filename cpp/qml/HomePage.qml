// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Effects

Rectangle {
    id: homePage
    anchors.fill: parent

    // External properties (set by Loader in MainWindow.qml)
    property var backend
    property var toastManager
    property var appWindow
    property string currentSelectedVersion: ""
    property int loginMode: 0
    readonly property bool hasBg: backend && typeof backend.customBgPath === "string" && backend.customBgPath.length > 0

    // Signals
    signal versionSelectRequested()
    signal versionSettingsRequested()

    function isVersionSelected() {
        return currentSelectedVersion && currentSelectedVersion.length > 0
    }
    // loginModeChanged is auto-generated from property int loginMode

    gradient: Gradient {
        GradientStop { position: 0.0; color: hasBg ? "transparent" : "#0c0f16" }
        GradientStop { position: 0.5; color: hasBg ? "transparent" : "#111520" }
        GradientStop { position: 1.0; color: hasBg ? "transparent" : "#0e111a" }
    }

    property alias msLoginForm: msLoginForm

    property string displayName: backend ? (loginMode === 0 ? (backend.username || "") : (backend.offlineUsername || "")) : ""
    property bool loggedIn: backend ? (loginMode === 0 ? backend.username !== "" : backend.offlineUsername !== "") : false
    property bool skinCapeExpanded: false
    property bool capeExpanded: false
    property bool _skinRefreshPending: false
    property bool showCapePopup: false

    function doModifySkin() {
        if (!backend) return
        backend.browseSkin()
        // After browseSkin, m_selectedSkinPath is set; emit skinChanged triggers upload
        // Show model selection (auto/steve/alex)
        if (backend.selectedSkinPath) {
            if (loginMode === 0) {
                // Online: upload to Mojang API - auto-detect variant from skin dimensions
                backend.uploadSkin(backend.selectedSkinPath, 0)
            } else {
                // Offline: save locally
                backend.saveWardrobeSettings(backend.selectedSkinPath, "", 0)
            }
        }
    }
    property int backendLoginType: backend ? (backend.isOnline ? 0 : 1) : -1

    // Login switch tabs
    Rectangle {
        id: loginSwitch
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top; anchors.topMargin: 32
        width: 280; height: 38; radius: StyleTokens.radiusLg
        color: "#11141c"; border.color: StyleTokens.bgInput
        RowLayout {
            anchors.fill: parent; spacing: 0
            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true
                color: loginMode === 0 ? "#1a1f2e" : "transparent"; radius: StyleTokens.radiusLg
                Rectangle {
                    anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                    width: loginMode === 0 ? 8 : 6; height: loginMode === 0 ? 8 : 6
                    radius: loginMode === 0 ? 4 : 3; color: loginMode === 0 ? "#6080e8" : "#3a3d50"
                    Behavior on width { NumberAnimation { duration: 200 } }
                    Behavior on height { NumberAnimation { duration: 200 } }
                    Behavior on radius { NumberAnimation { duration: 200 } }
                    Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
                }
                Row {
                    anchors.centerIn: parent; spacing: 6
                    Image {
                        source: "icons/lucide/key.svg"; width: 14; height: 14
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text { text: qsTr("正版登录"); font.pixelSize: StyleTokens.fontSizeMd; color: loginMode === 0 ? "#e4e8f2" : "#9498a8"; font.weight: loginMode === 0 ? Font.DemiBold : Font.Normal }
                }
                MouseArea { anchors.fill: parent; onClicked: { loginMode = 0; if (backend) { backend.lastLoginMode = 0; toastManager.show("已切换至正版登录") } } }
            }
            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true
                color: loginMode === 1 ? "#1a1f2e" : "transparent"; radius: StyleTokens.radiusLg
                Rectangle {
                    anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                    width: loginMode === 1 ? 8 : 6; height: loginMode === 1 ? 8 : 6
                    radius: loginMode === 1 ? 4 : 3; color: loginMode === 1 ? "#6080e8" : "#3a3d50"
                    Behavior on width { NumberAnimation { duration: 200 } }
                    Behavior on height { NumberAnimation { duration: 200 } }
                    Behavior on radius { NumberAnimation { duration: 200 } }
                    Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
                }
                Row {
                    anchors.centerIn: parent; spacing: 6
                    Image {
                        source: "icons/lucide/user.svg"; width: 14; height: 14
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text { text: qsTr("离线模式"); font.pixelSize: StyleTokens.fontSizeMd; color: loginMode === 1 ? "#e4e8f2" : "#9498a8"; font.weight: loginMode === 1 ? Font.DemiBold : Font.Normal }
                }
                MouseArea { anchors.fill: parent; onClicked: { loginMode = 1; if (backend) { backend.lastLoginMode = 1; toastManager.show("已切换至离线模式") } } }
            }
        }
    }

    // Premium login form
    Rectangle {
        id: msLoginForm
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: loginSwitch.bottom; anchors.topMargin: 20
        width: 300
        property bool showForm: loginMode === 0 && !loggedIn
        opacity: showForm ? 1 : 0
        visible: opacity > 0
        scale: showForm ? 1 : 0.9
        transformOrigin: Item.Top
        Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
        Behavior on scale { NumberAnimation { duration: 450; easing.type: Easing.OutBack } }
        height: childrenRect.height; color: "transparent"

        property bool msInProgress: false
        property string msStatusText: ""

        ColumnLayout {
            width: parent.width; spacing: 12

            // Start button
            Rectangle {
                Layout.alignment: Qt.AlignHCenter; width: 200; height: 40; radius: StyleTokens.radiusLg
                color: msLoginForm.msInProgress ? "#1a1f2e" : (startMsBtn.containsMouse ? "#3a4aa0" : "#2a3878")
                Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
                Row {
                    anchors.centerIn: parent; spacing: 8
                    Image {
                        source: "icons/lucide/key.svg"; width: 16; height: 16
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: msLoginForm.msInProgress ? "登录中..." : "Microsoft 登录"
                        color: msLoginForm.msInProgress ? "#9498a8" : "#e4e8f2"
                        font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.DemiBold
                    }
                }
                MouseArea {
                    id: startMsBtn
                    anchors.fill: parent
                    hoverEnabled: !msLoginForm.msInProgress
                    cursorShape: msLoginForm.msInProgress ? Qt.ArrowCursor : Qt.PointingHandCursor
                    enabled: !msLoginForm.msInProgress
                    onClicked: {
                        if (backend) {
                            msLoginForm.msInProgress = true
                            msLoginForm.msStatusText = "正在打开浏览器..."
                            backend.microsoftLogin()
                        }
                    }
                }
            }

            // Progress area
            Rectangle {
                Layout.fillWidth: true; height: msLoginForm.msInProgress ? 80 : 0
                visible: msLoginForm.msInProgress
                color: "#11141c"; radius: StyleTokens.radiusLg; border.color: StyleTokens.bgElevated
                Behavior on height { NumberAnimation { duration: 300 } }

                ColumnLayout {
                    anchors.centerIn: parent; spacing: 10; width: parent.width - 24

                    // Status text
                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: msLoginForm.msStatusText
                        color: "#8088a0"; font.pixelSize: StyleTokens.fontSizeSm
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap; Layout.maximumWidth: 260
                    }

                    // Loading bar
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 3; radius: StyleTokens.radiusSm
                        color: StyleTokens.bgInput
                        Rectangle {
                            height: 3; radius: StyleTokens.radiusSm; color: StyleTokens.accentLight
                            width: parent.width * 0.4
                            SequentialAnimation on x {
                                running: msLoginForm.msInProgress
                                loops: Animation.Infinite
                                NumberAnimation { from: 0; to: 140; duration: 1200; easing.type: Easing.InOutSine }
                                NumberAnimation { from: 140; to: 0; duration: 1200; easing.type: Easing.InOutSine }
                            }
                        }
                    }

                    // Cancel button link
                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("取消"); color: cancelMsBtn2.containsMouse ? "#e06060" : "#604040"
                        font.pixelSize: StyleTokens.fontSizeSm; font.underline: true
                        MouseArea {
                            id: cancelMsBtn2
                            anchors.fill: parent; hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                backend.cancelMicrosoftLogin()
                                msLoginForm.msInProgress = false
                                msLoginForm.msStatusText = ""
                            }
                        }
                    }
                }
            }
        }
    }

    // Offline login form
    Rectangle {
        id: offlineForm
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: loginSwitch.bottom; anchors.topMargin: 20
        width: 360
        property bool showForm: loginMode === 1
        opacity: showForm ? 1 : 0
        visible: opacity > 0
        scale: showForm ? 1 : 0.9
        transformOrigin: Item.Top
        Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
        Behavior on scale { NumberAnimation { duration: 450; easing.type: Easing.OutBack } }
        height: childrenRect.height; color: "transparent"

        // Refresh avatar + auto-fill when switching to offline tab
        onVisibleChanged: {
            if (visible && backend) {
                // Auto-fill last used username if input is empty
                if (!offlineNameInput.text && backend.offlineUsernames.length > 0) {
                    offlineNameInput.text = backend.offlineUsernames[0]
                }
                backend.updateOfflineSkin(offlineNameInput.text)
            }
        }

        ColumnLayout {
            width: parent.width; spacing: 12

            // Avatar (updates in real-time as name changes)
            MinecraftHead2D {
                Layout.alignment: Qt.AlignHCenter
                width: 48; height: 48
                showSpinner: false
                skinSource: (backend && backend.offlineSkinPath) ? backend.offlineSkinPath : ""
            }

            // Name input + dropdown (merged)
            Item {
                id: nameInputContainer
                property bool popupOpen: false
                Layout.fillWidth: true
                height: 40 + (popupOpen ? 1 + Math.min(backend ? backend.offlineUsernames.length * 32 : 0, 160) : 0)
                Behavior on height { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

                // Unified dark background
                Rectangle {
                    anchors.fill: parent
                    color: StyleTokens.bgSecondary
                    border.color: StyleTokens.bgElevated
                    border.width: 1
                    radius: StyleTokens.radiusLg
                    clip: true
                }

                // Input area
                Rectangle {
                    id: nameInputBox
                    anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
                    height: 40
                    color: "transparent"

                    TextInput {
                        id: offlineNameInput
                        anchors.left: parent.left; anchors.right: dropdownBtn.left
                        anchors.leftMargin: 12; anchors.rightMargin: 4
                        height: parent.height
                        color: "#e4e8f2"; font.pixelSize: StyleTokens.fontSizeMd
                        verticalAlignment: TextInput.AlignVCenter
                        onTextChanged: {
                            if (backend) backend.updateOfflineSkin(text)
                        }
                    }
                    Text {
                        anchors.left: parent.left; anchors.leftMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("输入用户名"); color: "#a8b0c0"
                        font.pixelSize: StyleTokens.fontSizeMd
                        visible: !offlineNameInput.text
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: !offlineNameInput.activeFocus
                        onClicked: offlineNameInput.forceActiveFocus()
                    }
                }

                // Dropdown arrow button
                Rectangle {
                    id: dropdownBtn
                    anchors.right: parent.right; anchors.verticalCenter: nameInputBox.verticalCenter
                    anchors.rightMargin: 4
                    width: 28; height: 32; radius: StyleTokens.radiusSm
                    color: nameInputContainer.popupOpen ? "#1e2840" : "transparent"
                    Text {
                        anchors.centerIn: parent
                        text: nameInputContainer.popupOpen ? "▲" : "▼"
                        color: "#a8b0c0"; font.pixelSize: StyleTokens.fontSizeXs
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: { nameInputContainer.popupOpen = !nameInputContainer.popupOpen }
                    }
                }

                // Separator line
                Rectangle {
                    id: popupSeparator
                    anchors.top: nameInputBox.bottom
                    anchors.left: parent.left; anchors.right: parent.right
                    anchors.leftMargin: 8; anchors.rightMargin: 8
                    height: 1
                    color: StyleTokens.bgElevated
                    visible: nameInputContainer.popupOpen
                }

                // History dropdown (attached directly below input)
                Rectangle {
                    id: offlineHistoryPopup
                    anchors.top: popupSeparator.bottom
                    anchors.left: parent.left; anchors.right: parent.right
                    height: Math.min(backend ? backend.offlineUsernames.length * 32 : 0, 160)
                    visible: nameInputContainer.popupOpen
                    color: "transparent"
                    clip: true

                    ListView {
                        id: historyList
                        anchors.fill: parent
                        model: backend ? backend.offlineUsernames : []
                        delegate: Rectangle {
                            width: historyList.width; height: 32
                            color: rowMouse.containsMouse ? "#1a2840" : "transparent"

                            Text {
                                anchors.left: parent.left; anchors.leftMargin: 12
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData; color: "#e4e8f2"; font.pixelSize: StyleTokens.fontSizeMd
                            }

                            // Delete button (×)
                            Rectangle {
                                id: delBtn
                                anchors.right: parent.right; anchors.rightMargin: 6
                                anchors.verticalCenter: parent.verticalCenter
                                width: 22; height: 22; radius: StyleTokens.radiusSm
                                color: delMouse.containsMouse ? (delMouse.pressed ? "#882020" : "#551818") : "transparent"
                                scale: delMouse.pressed ? 0.85 : 1.0
                                Behavior on color { ColorAnimation { duration: 120; easing.type: Easing.OutCubic } }
                                Behavior on scale { NumberAnimation { duration: 100; easing.type: Easing.OutCubic } }

                                Text {
                                    anchors.centerIn: parent
                                    text: "[失败]"; color: delMouse.containsMouse ? "#ff6666" : "#686c78"
                                    font.pixelSize: StyleTokens.fontSizeSm
                                }
                                MouseArea {
                                    id: delMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        var deletedName = modelData
                                        backend.removeOfflineUsername(deletedName)
                                        toastManager.show("玩家名\u201C" + deletedName + "\u201D已删除")
                                    }
                                }
                            }

                            MouseArea {
                                id: rowMouse
                                anchors.left: parent.left
                                anchors.right: delBtn.left
                                anchors.top: parent.top; anchors.bottom: parent.bottom
                                hoverEnabled: true
                                onClicked: {
                                    offlineNameInput.text = modelData
                                    nameInputContainer.popupOpen = false
                                }
                            }
                        }
                    }
                }
            }

            // Name validation (1.18+) — InlineToast
            InlineToast {
                Layout.fillWidth: true
                type: "warning"
                message: offlineNameInput.text !== "" && /[^a-zA-Z0-9_]/.test(offlineNameInput.text)
                    ? qsTr("对于1.18+版本，玩家名中如果出现除了英文、数字、下划线以外的非法字符，单人存档、局域网联机、进服务器全都可能无法进入世界！")
                    : ""
            }
        }
    }

    // Logged-in display
    Rectangle {
        id: loggedInDisplay
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: loginSwitch.bottom; anchors.topMargin: 20
        width: 300
        property bool showState: loggedIn && loginMode === 0
        opacity: showState ? 1 : 0
        visible: opacity > 0
        scale: showState ? 1 : 0.85
        transformOrigin: Item.Center
        Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
        Behavior on scale { NumberAnimation { duration: 500; easing.type: Easing.OutElastic } }
        height: childrenRect.height; color: "transparent"
        ColumnLayout {
            anchors.horizontalCenter: parent.horizontalCenter; spacing: 8

            // Avatar + Name
            RowLayout {
                Layout.alignment: Qt.AlignHCenter; spacing: 10
                MinecraftHead2D {
                    Layout.alignment: Qt.AlignVCenter
                    width: 48; height: 48
                    skinSource: (backend && backend.skinPath) ? backend.skinPath : ""
                }
                Text { text: displayName; font.pixelSize: StyleTokens.fontSizeLg; font.bold: true; color: StyleTokens.textSecondary }
            }

            // UUID
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: backend ? (backend.accountUuid || "") : ""
                font.pixelSize: StyleTokens.fontSizeXs; color: "#a8b0c0"
                font.family: "Consolas, monospace"
            }

            // Login type + Logout
            RowLayout {
                Layout.alignment: Qt.AlignHCenter; spacing: 12
                Text { text: loginMode === 1 ? "离线模式" : "正版登录"; font.pixelSize: StyleTokens.fontSizeSm; color: "#a8b0c0" }
                Rectangle {
                    width: 60; height: 24; radius: StyleTokens.radiusSm
                    color: "transparent"; border.color: StyleTokens.surfaceLight
                    Row {
                        anchors.centerIn: parent; spacing: 4
                        Image { source: "icons/lucide/log-out.svg"; width: 12; height: 12; anchors.verticalCenter: parent.verticalCenter }
                        Text { text: qsTr("登出"); font.pixelSize: StyleTokens.fontSizeSm; color: "#c05050" }
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: { if (backend) { backend.logout(); toastManager.show("已登出") } }
                    }
                }
            }
        }
    }

    // ═══ 皮肤与披风 — 精简管理面板 ═══
    Rectangle {
        id: skinCapePanel
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: loginMode === 0 ? loggedInDisplay.bottom : offlineForm.bottom
        anchors.topMargin: 12
        width: 340; color: "transparent"
        visible: loginMode === 0 && backend && backend.username
        height: skinCapeColumn.height
        clip: true

        Column {
            id: skinCapeColumn
            width: parent.width; spacing: 4

            // ── 折叠标题行 ──
            Rectangle {
                width: parent.width; height: 34; radius: StyleTokens.radiusMd
                color: headerMouse.containsMouse ? StyleTokens.bgElevated : StyleTokens.bgSecondary
                Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
                scale: headerMouse.pressed ? 0.97 : 1.0
                Behavior on scale { NumberAnimation { duration: 100; easing.type: Easing.OutCubic } }

                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 10
                    spacing: 6
                    Image {
                        source: "icons/lucide/image.svg"; width: 14; height: 14
                        sourceSize: Qt.size(14, 14)
                        Layout.alignment: Qt.AlignVCenter
                    }
                    Text {
                        text: qsTr("皮肤与披风")
                        color: StyleTokens.textSecondary
                        font.pixelSize: StyleTokens.fontSizeMd
                        font.weight: Font.Medium
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                    }
                    Item { Layout.fillWidth: true }
                    Image {
                        source: "icons/lucide/chevron-down.svg"; width: 12; height: 12
                        sourceSize: Qt.size(12, 12)
                        Layout.alignment: Qt.AlignVCenter
                        rotation: skinCapeExpanded ? 180 : 0
                        Behavior on rotation { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                    }
                }

                MouseArea {
                    id: headerMouse
                    anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        skinCapeExpanded = !skinCapeExpanded
                        if (skinCapeExpanded)
                            skinContentArea.height = skinContentColumn.implicitHeight
                        else
                            skinContentArea.height = 0
                    }
                }
            }

            // ── 展开内容区域 ──
            Rectangle {
                id: skinContentArea
                width: parent.width
                height: 0
                clip: true; color: "transparent"
                Behavior on height { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }

                Column {
                    id: skinContentColumn
                    width: parent.width; spacing: 8

                    // ── 第一行 ──
                    RowLayout { spacing: 8; width: parent.width

                        // 修改皮肤
                        Rectangle {
                            Layout.fillWidth: true; Layout.preferredWidth: 1; height: 36; radius: StyleTokens.radiusMd
                            color: btn1.containsMouse ? StyleTokens.bgHover : StyleTokens.bgCard
                            border.color: btn1.containsMouse ? StyleTokens.borderLight : StyleTokens.border; border.width: 1
                            Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
                            Behavior on border.color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
                            scale: btn1.pressed ? 0.95 : (btn1.containsMouse ? 1.03 : 1.0)
                            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                            RowLayout { anchors.centerIn: parent; spacing: 5
                                Image { source: "icons/lucide/image.svg"; width: 14; height: 14; sourceSize: Qt.size(14,14) }
                                Text { text: qsTr("修改皮肤"); color: StyleTokens.textPrimary; font.pixelSize: StyleTokens.fontSizeSm }
                            }
                            MouseArea { id: btn1; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: doModifySkin()
                            }
                        }

                        // 刷新皮肤
                        Rectangle {
                            Layout.fillWidth: true; Layout.preferredWidth: 1; height: 36; radius: StyleTokens.radiusMd
                            color: btn2.containsMouse ? StyleTokens.bgHover : StyleTokens.bgCard
                            border.color: btn2.containsMouse ? StyleTokens.borderLight : StyleTokens.border; border.width: 1
                            Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
                            Behavior on border.color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
                            scale: btn2.pressed ? 0.95 : (btn2.containsMouse ? 1.03 : 1.0)
                            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                            RowLayout { anchors.centerIn: parent; spacing: 5
                                Image { source: "icons/lucide/refresh-cw.svg"; width: 14; height: 14; sourceSize: Qt.size(14,14) }
                                Text { text: qsTr("刷新皮肤"); color: StyleTokens.textPrimary; font.pixelSize: StyleTokens.fontSizeSm }
                            }
                            MouseArea { id: btn2; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (backend && backend.account) {
                                        _skinRefreshPending = true
                                        toastManager.show(qsTr("正在刷新皮肤..."))
                                        backend.account.refreshSkin()
                                    } else {
                                        toastManager.show(qsTr("未登录"))
                                    }
                                }
                            }
                        }
                    }

                    // ── 第二行 ──
                    RowLayout { spacing: 8; width: parent.width

                        // 保存皮肤到本地
                        Rectangle {
                            Layout.fillWidth: true; Layout.preferredWidth: 1; height: 36; radius: StyleTokens.radiusMd
                            color: btn3.containsMouse ? StyleTokens.bgHover : StyleTokens.bgCard
                            border.color: btn3.containsMouse ? StyleTokens.borderLight : StyleTokens.border; border.width: 1
                            Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
                            Behavior on border.color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
                            scale: btn3.pressed ? 0.95 : (btn3.containsMouse ? 1.03 : 1.0)
                            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                            RowLayout { anchors.centerIn: parent; spacing: 5
                                Image { source: "icons/lucide/download.svg"; width: 14; height: 14; sourceSize: Qt.size(14,14) }
                                Text { text: qsTr("保存到本地"); color: StyleTokens.textPrimary; font.pixelSize: StyleTokens.fontSizeSm }
                            }
                            MouseArea { id: btn3; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { if (backend) backend.saveSkinToFile() }
                            }
                        }

                        // 修改披风
                        Rectangle {
                            Layout.fillWidth: true; Layout.preferredWidth: 1; height: 36; radius: StyleTokens.radiusMd
                            color: btn4.containsMouse ? StyleTokens.bgHover : StyleTokens.bgCard
                            border.color: btn4.containsMouse ? StyleTokens.borderLight : StyleTokens.border; border.width: 1
                            Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
                            Behavior on border.color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
                            scale: btn4.pressed ? 0.95 : (btn4.containsMouse ? 1.03 : 1.0)
                            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                            RowLayout { anchors.centerIn: parent; spacing: 5
                                Image { source: "icons/lucide/tag.svg"; width: 14; height: 14; sourceSize: Qt.size(14,14) }
                                Text { text: qsTr("修改披风"); color: StyleTokens.textPrimary; font.pixelSize: StyleTokens.fontSizeSm }
                            }
                            MouseArea { id: btn4; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { showCapePopup = true }
                            }
                        }
                    }

                    // ── 披风选择面板 ──
                    Rectangle {
                        id: capeArea
                        width: parent.width
                        height: 0
                        clip: true; color: StyleTokens.bgSecondary; radius: StyleTokens.radiusMd
                        // Always visible so Repeater delegates are created;
                        // height: 0 + clip: true hides it when collapsed
                        Behavior on height { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

                        ColumnLayout {
                            id: capeListColumn
                            width: parent.width; spacing: 2

                            // 离线模式提示
                            Text {
                                Layout.fillWidth: true
                                visible: loginMode === 1
                                text: qsTr("披风仅正版登录可用")
                                color: "#a8b0c0"
                                font.pixelSize: StyleTokens.fontSizeSm; font.italic: true
                                padding: 10
                            }

                            // Cape 列表
                            Repeater {
                                model: (backend && backend.account) ? backend.account.capes : []

                                Rectangle {
                                    Layout.fillWidth: true; height: 32; radius: StyleTokens.radiusSm
                                    color: capeRow.containsMouse ? StyleTokens.bgHover : "transparent"
                                    Behavior on color { ColorAnimation { duration: 120 } }
                                    visible: loginMode === 0

                                    RowLayout {
                                        anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 10
                                        spacing: 6
                                        Rectangle {
                                            width: 8; height: 8; radius: 4
                                            color: modelData.active ? StyleTokens.success : "transparent"
                                            visible: modelData.active
                                        }
                                        Text {
                                            text: modelData.alias || qsTr("未知披风")
                                            color: modelData.active ? StyleTokens.accentLight : StyleTokens.textSecondary
                                            font.pixelSize: StyleTokens.fontSizeSm
                                            font.weight: modelData.active ? Font.Bold : Font.Normal
                                            Layout.fillWidth: true
                                        }
                                        Text {
                                            text: qsTr("当前")
                                            visible: modelData.active
                                            color: StyleTokens.success
                                            font.pixelSize: StyleTokens.fontSizeXs
                                        }
                                    }

                                    MouseArea {
                                        id: capeRow
                                        anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            if (backend && backend.account)
                                                backend.account.selectCape(modelData.alias || "")
                                        }
                                    }
                                }
                            }

                            // 无披风选项
                            Rectangle {
                                Layout.fillWidth: true; height: 32; radius: StyleTokens.radiusSm
                                color: noneCapeRow.containsMouse ? StyleTokens.bgHover : "transparent"
                                Behavior on color { ColorAnimation { duration: 120 } }
                                visible: loginMode === 0

                                RowLayout {
                                    anchors.fill: parent; anchors.leftMargin: 10
                                    spacing: 6
                                    Text {
                                        text: qsTr("不显示披风")
                                        color: StyleTokens.textTertiary
                                        font.pixelSize: StyleTokens.fontSizeSm
                                    }
                                }

                                MouseArea {
                                    id: noneCapeRow
                                    anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (backend && backend.account)
                                            backend.account.selectCape("")
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Bottom fixed buttons
    Rectangle {
        anchors.bottom: parent.bottom; anchors.bottomMargin: 20
        anchors.horizontalCenter: parent.horizontalCenter
        width: 360; color: "transparent"
        height: childrenRect.height
        ColumnLayout {
            width: parent.width; spacing: 10

            // Current version indicator
            Rectangle {
                Layout.fillWidth: true; height: 32; radius: StyleTokens.radiusMd
                color: "#11141c"; border.color: currentSelectedVersion ? "#1a2848" : "#0e1118"
                border.width: currentSelectedVersion ? 1 : 0
                RowLayout {
                    anchors.centerIn: parent; spacing: 8
                    Rectangle { width: 8; height: 8; radius: StyleTokens.radiusSm; color: "#6080e8"; visible: currentSelectedVersion !== "" }
                    Text { text: currentSelectedVersion || "未选择版本"; font.pixelSize: StyleTokens.fontSizeLg; font.weight: Font.Bold; color: currentSelectedVersion ? "#8aa8f0" : "#787c90" }
                }
            }

            Rectangle {
                id: launchBtn
                Layout.fillWidth: true; height: 44; radius: StyleTokens.radiusLg
                color: launchHover.containsMouse ? (launchHover.pressed ? "#2a3a90" : "#4a5ec8") : "#3a4eb8"
                scale: launchHover.containsMouse ? (launchHover.pressed ? 0.95 : 1.03) : 1.0
                Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
                Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                Row {
                    anchors.centerIn: parent; spacing: 8
                    Image {
                        source: "icons/lucide/play.svg"; width: 18; height: 18
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text { text: qsTr("启动游戏"); font.pixelSize: StyleTokens.fontSizeLg; font.weight: Font.Bold; color: StyleTokens.textPrimary }
                }
                MouseArea { id: launchHover; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (!backend) return
                        if (!currentSelectedVersion) {
                            toastManager.show("请先选择版本")
                            return
                        }
                        // Offline mode: always sync input name to session (also records history)
                        if (loginMode === 1) {
                            var name = offlineNameInput.text.trim() || "Player"
                            backend.offlineLogin(name)
                            // offlineLogin is synchronous — offlineUsername is now set
                        }
                        // Premium mode: must be logged in
                        if (loginMode === 0 && !backend.username) {
                            toastManager.show("请先完成正版登录")
                            return
                        }
                        backend.launch(currentSelectedVersion, loginMode === 0)
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true; spacing: 10
                Rectangle {
                    id: verSelectBtn
                    Layout.fillWidth: true; height: 36; radius: StyleTokens.radiusLg
                    color: verSelectHover.containsMouse ? "#151a24" : "#0e1118"; border.color: StyleTokens.bgElevated
                    scale: verSelectHover.containsMouse ? (verSelectHover.pressed ? 0.95 : 1.03) : 1.0
                    Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
                    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                    MouseArea { id: verSelectHover; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { homePage.versionSelectRequested() } }
                    RowLayout {
                        anchors.centerIn: parent; spacing: 6
                        Image { source: "icons/lucide/list.svg"; width: 16; height: 16; }
                        Text { text: qsTr("版本选择"); font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.Medium; color: "#b0b8c8" }
                    }
                }
                Rectangle {
                    id: verSettingsBtn
                    Layout.fillWidth: true; height: 36; radius: StyleTokens.radiusLg
                    color: verSettingsHover.containsMouse ? "#151a24" : "#0e1118"; border.color: StyleTokens.bgElevated
                    scale: verSettingsHover.containsMouse ? (verSettingsHover.pressed ? 0.95 : 1.03) : 1.0
                    Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
                    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                    MouseArea { id: verSettingsHover; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { if (isVersionSelected()) { homePage.versionSettingsRequested() } else { if (toastManager) toastManager.show(qsTr("请先选择游戏版本")) } } }
                    RowLayout {
                        anchors.centerIn: parent; spacing: 6
                        Image { source: "icons/lucide/wrench.svg"; width: 16; height: 16; }
                        Text { text: qsTr("版本设置"); font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.Medium; color: "#b0b8c8" }
                    }
                }
            }
        }
    }

    // ═══ 披风选择弹窗 ═══
    Rectangle {
        id: capePopupOverlay
        anchors.fill: parent
        z: 100
        color: "#80000000"
        opacity: showCapePopup ? 1 : 0
        visible: showCapePopup || opacity > 0
        Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

        MouseArea {
            anchors.fill: parent
            onClicked: showCapePopup = false
        }

        // ── Card ──
        Rectangle {
            id: capePopupCard
            width: 360
            height: Math.min(capePopupContent.implicitHeight + 60, parent.height - 80)
            anchors.centerIn: parent
            radius: StyleTokens.radiusLg
            color: StyleTokens.bgSecondary
            border.color: StyleTokens.border; border.width: 1

            scale: showCapePopup ? 1 : 0.9
            opacity: showCapePopup ? 1 : 0
            Behavior on scale {
                NumberAnimation { duration: 350; easing.type: Easing.OutBack }
            }
            Behavior on opacity {
                NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
            }

            // ── Header ──
            Rectangle {
                id: capePopupHeader
                width: parent.width; height: 48
                color: "transparent"

                Text {
                    text: qsTr("选择披风")
                    anchors.left: parent.left; anchors.leftMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    color: StyleTokens.textPrimary
                    font.pixelSize: StyleTokens.fontSizeLg
                    font.weight: Font.DemiBold
                }

                Rectangle {
                    id: capePopupCloseBtn
                    anchors.right: parent.right; anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    width: 32; height: 32; radius: StyleTokens.radiusSm
                    color: capePopupCloseArea.containsMouse ? StyleTokens.bgHover : "transparent"
                    Behavior on color { ColorAnimation { duration: 120 } }

                    Image {
                        source: "icons/lucide/x.svg"
                        width: 16; height: 16
                        anchors.centerIn: parent
                        sourceSize: Qt.size(16, 16)
                    }

                    MouseArea {
                        id: capePopupCloseArea
                        anchors.fill: parent; hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: showCapePopup = false
                    }
                }
            }

            Rectangle {
                width: parent.width; height: 1
                anchors.top: capePopupHeader.bottom
                color: StyleTokens.borderLight
            }

            // ── Scrollable content ──
            Flickable {
                id: capePopupFlick
                width: parent.width
                anchors.top: capePopupHeader.bottom; anchors.topMargin: 1
                anchors.bottom: parent.bottom
                contentHeight: capePopupContent.implicitHeight + 16
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                ColumnLayout {
                    id: capePopupContent
                    width: parent.width - 16
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: 8
                    spacing: 4

                    // Offline warning
                    Text {
                        Layout.fillWidth: true
                        visible: loginMode === 1
                        text: qsTr("披风仅正版登录可用")
                        color: "#a8b0c0"
                        font.pixelSize: StyleTokens.fontSizeSm; font.italic: true
                        Layout.topMargin: 4; Layout.bottomMargin: 4
                        padding: 12
                    }

                    // Cape list
                    Repeater {
                        model: (backend && backend.account && loginMode === 0) ? backend.account.capes : []

                        Rectangle {
                            Layout.fillWidth: true; height: 40; radius: StyleTokens.radiusSm
                            color: capePopupItemMouse.containsMouse ? StyleTokens.bgHover : "transparent"
                            Behavior on color { ColorAnimation { duration: 120 } }

                            RowLayout {
                                anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 8
                                spacing: 8

                                Rectangle {
                                    width: 8; height: 8; radius: 4
                                    color: modelData.active ? StyleTokens.success : "transparent"
                                    visible: modelData.active
                                }

                                Text {
                                    text: modelData.alias || qsTr("未知披风")
                                    color: modelData.active ? StyleTokens.accentLight : StyleTokens.textPrimary
                                    font.pixelSize: StyleTokens.fontSizeMd
                                    font.weight: modelData.active ? Font.Bold : Font.Normal
                                    Layout.fillWidth: true
                                }

                                Text {
                                    text: qsTr("[完成] 当前")
                                    visible: modelData.active
                                    color: StyleTokens.success
                                    font.pixelSize: StyleTokens.fontSizeXs
                                }
                            }

                            MouseArea {
                                id: capePopupItemMouse
                                anchors.fill: parent; hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (backend && backend.account) {
                                        backend.account.selectCape(modelData.alias || "")
                                        showCapePopup = false
                                    }
                                }
                            }
                        }
                    }

                    // Empty state
                    Repeater {
                        model: (backend && backend.account && backend.account.capes && backend.account.capes.length === 0 && loginMode === 0) ? [1] : []

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("暂无可用披风")
                            color: StyleTokens.textTertiary
                            font.pixelSize: StyleTokens.fontSizeMd
                            padding: 16
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }

                    // "No cape" option
                    Rectangle {
                        Layout.fillWidth: true; height: 40; radius: StyleTokens.radiusSm
                        visible: loginMode === 0
                        color: noneCapePopupMouse.containsMouse ? StyleTokens.bgHover : "transparent"
                        Behavior on color { ColorAnimation { duration: 120 } }

                        RowLayout {
                            anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 8
                            spacing: 8

                            Text {
                                text: qsTr("不显示披风")
                                color: StyleTokens.textTertiary
                                font.pixelSize: StyleTokens.fontSizeMd
                                Layout.fillWidth: true
                            }
                        }

                        MouseArea {
                            id: noneCapePopupMouse
                            anchors.fill: parent; hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (backend && backend.account) {
                                    backend.account.selectCape("")
                                    showCapePopup = false
                                }
                            }
                        }
                    }

                    Item { Layout.preferredHeight: 8 }
                }
            }
        }
    }

    // ── 信号处理 ──
    Connections {
        target: backend && backend.account ? backend.account : null
        function onSkinReady() {
            if (_skinRefreshPending) {
                _skinRefreshPending = false
                toastManager.show(qsTr("刷新成功"))
            }
        }
        function onOfflineSkinReady() {
            if (_skinRefreshPending) {
                _skinRefreshPending = false
                toastManager.show(qsTr("刷新成功"))
            }
        }
        function onCapeChangeFinished(success, errorMsg) {
            if (success)
                toastManager.show(qsTr("披风已切换"))
            else
                toastManager.show(qsTr("披风切换失败: ") + errorMsg)
        }
    }
    Connections {
        target: backend
        function onWardrobeError(error) {
            toastManager.show(error)
        }
    }
}
