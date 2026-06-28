// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

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
    // loginModeChanged is auto-generated from property int loginMode

    gradient: Gradient {
        GradientStop { position: 0.0; color: hasBg ? "transparent" : "#0c0f16" }
        GradientStop { position: 0.5; color: hasBg ? "transparent" : "#111520" }
        GradientStop { position: 1.0; color: hasBg ? "transparent" : "#0e111a" }
    }

    property string displayName: backend ? (loginMode === 0 ? (backend.username || "") : (backend.offlineUsername || "")) : ""
    property bool loggedIn: backend ? (loginMode === 0 ? backend.username !== "" : backend.offlineUsername !== "") : false
    property int backendLoginType: backend ? (backend.isOnline ? 0 : 1) : -1

    Connections {
        target: backend; enabled: backend !== null
        function onUsernameChanged() {
            displayName = backend.username
        }
    }

    // Login switch tabs
    Rectangle {
        id: loginSwitch
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top; anchors.topMargin: 32
        width: 280; height: 38; radius: 8
        color: "#11141c"; border.color: "#1a1f2a"
        RowLayout {
            anchors.fill: parent; spacing: 0
            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true
                color: loginMode === 0 ? "#1a1f2e" : "transparent"; radius: 8
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
                    Text { text: qsTr("正版登录"); font.pixelSize: 13; color: loginMode === 0 ? "#e4e8f2" : "#9498a8"; font.weight: loginMode === 0 ? Font.DemiBold : Font.Normal }
                }
                MouseArea { anchors.fill: parent; onClicked: { loginMode = 0; if (backend) { backend.lastLoginMode = 0; toastManager.show("已切换至正版登录") } } }
            }
            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true
                color: loginMode === 1 ? "#1a1f2e" : "transparent"; radius: 8
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
                    Text { text: qsTr("离线模式"); font.pixelSize: 13; color: loginMode === 1 ? "#e4e8f2" : "#9498a8"; font.weight: loginMode === 1 ? Font.DemiBold : Font.Normal }
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
                Layout.alignment: Qt.AlignHCenter; width: 200; height: 40; radius: 8
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
                        font.pixelSize: 14; font.weight: Font.DemiBold
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
                color: "#11141c"; radius: 8; border.color: "#1e2230"
                Behavior on height { NumberAnimation { duration: 300 } }

                ColumnLayout {
                    anchors.centerIn: parent; spacing: 10; width: parent.width - 24

                    // Status text
                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: msLoginForm.msStatusText
                        color: "#8088a0"; font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap; Layout.maximumWidth: 260
                    }

                    // Loading bar
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 3; radius: 4
                        color: "#1a1f2a"
                        Rectangle {
                            height: 3; radius: 4; color: "#6080e8"
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
                        font.pixelSize: 12; font.underline: true
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

    // Microsoft login signal handlers
    // Login form progress — active during both manual login and background refresh
    property bool msInProgress: false
    Connections {
        target: backend
        function onMicrosoftLoginProgress(step, detail) {
            msLoginForm.msStatusText = step + ": " + detail
        }
        function onMicrosoftLoginSuccess(username, uuid) {
            msLoginForm.msInProgress = false
            msLoginForm.msStatusText = ""
            displayName = username
            toastManager.show("正版登录成功: " + username)
        }
        function onMicrosoftLoginFailed(error) {
            msLoginForm.msInProgress = false
            msLoginForm.msStatusText = ""
            toastManager.show("登录失败: " + error)
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

            // Name input + dropdown
            Item {
                Layout.fillWidth: true; height: 40
                Rectangle {
                    id: nameInputBox
                    anchors.fill: parent; radius: 8
                    color: "#11141c"; border.color: "#1e2230"
                    clip: true
                    // Click anywhere on the box to activate input
                    MouseArea {
                        anchors.fill: parent
                        enabled: !offlineNameInput.activeFocus
                        onClicked: offlineNameInput.forceActiveFocus()
                    }
                    TextInput {
                        id: offlineNameInput
                        anchors.left: parent.left; anchors.right: dropdownBtn.left
                        anchors.leftMargin: 12; anchors.rightMargin: 4
                        height: parent.height
                        color: "#e4e8f2"; font.pixelSize: 13
                        verticalAlignment: TextInput.AlignVCenter
                        onTextChanged: {
                            if (backend) backend.updateOfflineSkin(text)
                        }
                    }
                    Text {
                        anchors.left: parent.left; anchors.leftMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("输入用户名"); color: "#a8b0c0"
                        font.pixelSize: 13
                        visible: !offlineNameInput.text
                    }
                }
                // Dropdown arrow button
                Rectangle {
                    id: dropdownBtn
                    anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                    anchors.rightMargin: 4
                    width: 28; height: 32; radius: 4
                    color: offlineHistoryPopup.visible ? "#1e2840" : "transparent"
                    Text {
                        anchors.centerIn: parent
                        text: "▼"; color: "#a8b0c0"; font.pixelSize: 10
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            offlineHistoryPopup.visible = !offlineHistoryPopup.visible
                        }
                    }
                }
            }

            // History dropdown popup
            Rectangle {
                id: offlineHistoryPopup
                Layout.fillWidth: true
                visible: false
                height: Math.min(backend ? backend.offlineUsernames.length * 32 : 0, 160)
                color: "#141820"; radius: 6
                border.color: "#1e2840"
                clip: true
                z: 10
                ListView {
                    anchors.fill: parent
                    model: backend ? backend.offlineUsernames : []
                    delegate: Rectangle {
                        width: offlineHistoryPopup.width; height: 32
                        color: mouseArea.containsMouse ? "#1a2840" : "transparent"
                        Text {
                            anchors.left: parent.left; anchors.leftMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData; color: "#e4e8f2"; font.pixelSize: 13
                        }
                        MouseArea {
                            id: mouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                offlineNameInput.text = modelData
                                offlineHistoryPopup.visible = false
                            }
                        }
                    }
                }
            }
        }
    }

    // Logged-in display
    Rectangle {
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
                Text { text: displayName; font.pixelSize: 16; font.bold: true; color: "#e4e8f2" }
            }

            // UUID
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: backend ? (backend.accountUuid || "") : ""
                font.pixelSize: 10; color: "#a8b0c0"
                font.family: "Consolas, monospace"
            }

            // Login type + Logout
            RowLayout {
                Layout.alignment: Qt.AlignHCenter; spacing: 12
                Text { text: loginMode === 1 ? "离线模式" : "正版登录"; font.pixelSize: 11; color: "#a8b0c0" }
                Rectangle {
                    width: 60; height: 24; radius: 4
                    color: "transparent"; border.color: "#2a1f24"
                    Row {
                        anchors.centerIn: parent; spacing: 4
                        Image { source: "icons/lucide/log-out.svg"; width: 12; height: 12; anchors.verticalCenter: parent.verticalCenter }
                        Text { text: qsTr("登出"); font.pixelSize: 11; color: "#c05050" }
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: { if (backend) { backend.logout(); toastManager.show("已登出") } }
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
                Layout.fillWidth: true; height: 32; radius: 6
                color: "#11141c"; border.color: currentSelectedVersion ? "#1a2848" : "#0e1118"
                border.width: currentSelectedVersion ? 1 : 0
                RowLayout {
                    anchors.centerIn: parent; spacing: 8
                    Rectangle { width: 8; height: 8; radius: 4; color: "#6080e8"; visible: currentSelectedVersion !== "" }
                    Text { text: currentSelectedVersion || "未选择版本"; font.pixelSize: 16; font.weight: Font.Bold; color: currentSelectedVersion ? "#8aa8f0" : "#787c90" }
                }
            }

            Rectangle {
                Layout.fillWidth: true; height: 44; radius: 8
                color: "#3a4eb8"
                Row {
                    anchors.centerIn: parent; spacing: 8
                    Image {
                        source: "icons/lucide/play.svg"; width: 18; height: 18
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text { text: qsTr("启动游戏"); font.pixelSize: 15; font.weight: Font.Bold; color: "#e8ecf8" }
                }
                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (!backend) return
                        if (!currentSelectedVersion) {
                            toastManager.show("请先选择版本")
                            return
                        }
                        // Offline mode: auto-login with name if needed
                        if (loginMode === 1 && !backend.offlineUsername) {
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
                    Layout.fillWidth: true; height: 36; radius: 8; color: "#0e1118"; border.color: "#1e2230"
                    MouseArea { anchors.fill: parent; onClicked: { homePage.versionSelectRequested() } }
                    RowLayout {
                        anchors.centerIn: parent; spacing: 6
                        Image { source: "icons/lucide/list.svg"; width: 16; height: 16; }
                        Text { text: qsTr("版本选择"); font.pixelSize: 13; font.weight: Font.Medium; color: "#b0b8c8" }
                    }
                }
                Rectangle {
                    Layout.fillWidth: true; height: 36; radius: 8; color: "#0e1118"; border.color: "#1e2230"
                    MouseArea { anchors.fill: parent; onClicked: { homePage.versionSettingsRequested() } }
                    RowLayout {
                        anchors.centerIn: parent; spacing: 6
                        Image { source: "icons/lucide/wrench.svg"; width: 16; height: 16; }
                        Text { text: qsTr("版本设置"); font.pixelSize: 13; font.weight: Font.Medium; color: "#b0b8c8" }
                    }
                }
            }
        }
    }
}
