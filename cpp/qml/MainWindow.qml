import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

Window {
    id: appWindow
    visible: true
    flags: Qt.FramelessWindowHint | Qt.Window
    minimumWidth: 800; minimumHeight: 550
    width: 960; height: 620
    title: "Shadow Launcher"
    color: "transparent"

    property int navListIndex: 0
    property int pendingSubTab: -1  // for --navigate auto-test
    property string currentSelectedVersion: backend ? backend.selectedVersion : ""
    property int loginMode: backend ? backend.lastLoginMode : 0
    property bool showVersionSelect: false
    property bool showVersionSettings: false
    property var offlineHistory: []
    property bool pageLoading: false
    property var runningListModel: []

    Component.onCompleted: {
        // refreshVersionList() 已由 VersionBackend 构造时异步触发，此处不重复
        if (backend) {
            backend.refreshInstalled()
            runningListModel = backend.runningGames()
        }
    }

    function addOfflineHistory(name) {
        var h = offlineHistory.slice()
        h = h.filter(function(x) { return x !== name })
        h.unshift(name)
        if (h.length > 10) h = h.slice(0, 10)
        offlineHistory = h
    }
    function switchPage(index) { navListIndex = index; showVersionSelect = false; showVersionSettings = false; pageLoading = true; loadTimer.restart() }

    Timer {
        id: loadTimer
        interval: 100
        onTriggered: pageLoading = false
    }

    // 自动检测游戏文件变化（每30秒）
    Timer {
        id: fileChangeTimer
        interval: 30000
        running: true
        repeat: true
        onTriggered: {
            if (backend) backend.checkFileChanges()
        }
    }

    // Download progress nav item management ===
    property bool downloadNavVisible: false

    function showDownloadNav() {
        if (!downloadNavVisible) {
            downloadNavVisible = true
            navModel.append({ label: "下载进度", pageKey: "download_progress" })
            switchPage(navModel.count - 1)  // auto-switch to the new page
        }
    }

    function showDownloadNavSilent() {
        // Add nav item without auto-switching (for ball animation flow)
        if (!downloadNavVisible) {
            downloadNavVisible = true
            navModel.append({ label: "下载进度", pageKey: "download_progress" })
        }
    }

    function hideDownloadNav() {
        if (downloadNavVisible) {
            downloadNavVisible = false
            // Remove last item if it's download_progress
            for (var i = navModel.count - 1; i >= 0; i--) {
                if (navModel.get(i).pageKey === "download_progress") {
                    navModel.remove(i)
                    break
                }
            }
            // If we're currently on that page, switch to home
            if (navListIndex >= navModel.count) {
                switchPage(0)
            }
        }
    }

    Connections {
        target: backend; enabled: backend !== null
        function onInstallingChanged() {
            if (backend.installing) showDownloadNav()
        }
        function onInstallFinished(success) {
            // Keep nav visible for a moment, will be hidden when user switches away
        }
        // ── Auto-test: navigate to page + sub-tab ──
        function onNavigateToRequested(pageIndex, subTab) {
            console.log("[auto-test] navigateToRequested: page", pageIndex, "tab", subTab)
            if (pageIndex === 1) {
                pendingSubTab = subTab
            }
            switchPage(pageIndex)
        }
    }

    // ── Rounded window container ──
    Rectangle {
        anchors.fill: parent; radius: 16
        color: "#0c0f16"
        clip: true

    ColumnLayout {
        anchors.fill: parent; spacing: 0

        // Spacer — buttons moved to floating right edge (same height as sidebar SHADOW)
        Item { Layout.fillWidth: true; height: 2 }

        // ── Loading bar (Android-style indeterminate) ──
        // FIX: fixed height 2px + opacity control → zero layout jitter
        // FIX: inset from rounded window corners (radius: 16)
        Rectangle {
            id: loadingBar
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.preferredHeight: 2
            opacity: pageLoading ? 1 : 0
            color: "transparent"
            clip: true
            Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

            Rectangle {
                id: loadingSlider
                width: 100; height: 2; radius: 1
                color: "#6080e8"
                x: -100
                y: 0

                SequentialAnimation on x {
                    running: loadingBar.opacity > 0
                    loops: Animation.Infinite
                    NumberAnimation { from: -100; to: 100; duration: 600; easing.type: Easing.InOutCubic }
                    NumberAnimation { from: 100; to: appWindow.width + 100; duration: 400; easing.type: Easing.InCubic }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true
            Layout.margins: 8; spacing: 8

            Rectangle {
                Layout.preferredWidth: 200; Layout.fillHeight: true
                layer.enabled: true
                color: "#0a0c12"; radius: 6
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 8; spacing: 2
                    Text { Layout.topMargin: 8; Layout.bottomMargin: 20; Layout.leftMargin: 16; text: "SHADOW"; font.pixelSize: 16; font.bold: true; color: "#e4e8f2" }

                    // Navigation model
                    ListModel {
                        id: navModel
                        ListElement { label: "启动"; pageKey: "home" }
                        ListElement { label: "下载"; pageKey: "download" }
                        ListElement { label: "设置"; pageKey: "settings" }
                    }

                    Repeater {
                        id: navRepeater
                        model: navModel
                        Item {
                            id: navItemDelegate
                            width: parent ? parent.width : 180; Layout.fillWidth: true; height: 44
                            // Expose window-position for fly ball animation
                            property var windowRoot: appWindow
                            scale: navMouse.containsMouse ? 1.03 : 1.0
                            Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                            Rectangle { anchors.fill: parent; color: navMouse.containsMouse ? "#11141c" : "transparent" }
                            Text { anchors.left: parent.left; anchors.leftMargin: 24; anchors.verticalCenter: parent.verticalCenter; text: model.label || modelData; font.pixelSize: 13; color: navListIndex === index ? "#e4e8f2" : "#9498a8" }
                            MouseArea { id: navMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: switchPage(index) }
                        }
                    }
                    Item { Layout.fillHeight: true }

                    // ═══ Running Games ═══
                    Text {
                        visible: backend ? backend.runningCount > 0 : false
                        Layout.leftMargin: 16; Layout.topMargin: 4
                        text: "运行中 (" + (backend ? backend.runningCount : 0) + ")"
                        font.pixelSize: 10; color: "#6080e8"
                    }
                    Repeater {
                        id: runningList
                        model: appWindow.runningListModel
                        Item {
                            width: parent ? parent.width - 16 : 180; Layout.fillWidth: true; height: 32
                            Rectangle {
                                anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8
                                color: runningItemHover.containsMouse ? "#151a26" : "#0d1018"
                                radius: 4
                                RowLayout {
                                    anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 6
                                    Text {
                                        text: modelData.version || "?"
                                        font.pixelSize: 11; color: "#d4dcf0"
                                        elide: Text.ElideRight; Layout.fillWidth: true
                                    }
                                    Rectangle {
                                        width: 20; height: 20; radius: 10
                                        color: runningKillHover.containsMouse ? "#e06060" : "#c05050"
                                        scale: runningKillHover.containsMouse ? 1.15 : 1.0
                                        Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                                        Text { anchors.centerIn: parent; text: "\u2715"; font.pixelSize: 10; color: "#fff" }
                                        MouseArea {
                                            id: runningKillHover
                                            anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                            onClicked: { if (backend) backend.killGameById(modelData.index) }
                                        }
                                    }
                                }
                            }
                            HoverHandler { id: runningItemHover }
                        }
                    }

                    Text { Layout.alignment: Qt.AlignHCenter; text: "v0.3.0"; font.pixelSize: 10; color: "#303440" }
                }

                // Animated selection indicator overlay
                Rectangle {
                    id: navIndicator
                    z: 10
                    x: 8; y: 8 + 52 + navListIndex * 44
                    width: 2; height: 44; color: "#6080e8"
                    Behavior on y { SmoothedAnimation { velocity: 200; duration: 300 } }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0

                // ── Right-side header (matched to SHADOW height, sidebar color) ──
                Rectangle {
                    Layout.fillWidth: true; height: 44
                    color: "#0a0c12"
                    MouseArea {
                        anchors.fill: parent
                        property point lastPos: Qt.point(0, 0)
                        onPressed: lastPos = Qt.point(mouse.x, mouse.y)
                        onPositionChanged: { appWindow.x += mouse.x - lastPos.x; appWindow.y += mouse.y - lastPos.y }
                    }
                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 6
                        spacing: 0
                        Item { Layout.fillWidth: true }
                        Rectangle {
                            width: 28; height: 28; radius: 14
                            color: hdrMin.containsMouse ? (hdrMin.pressed ? "#3a4050" : "#252a35") : "transparent"
                            scale: hdrMin.pressed ? 0.85 : (hdrMin.containsMouse ? 1.12 : 1.0)
                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                            Text { anchors.centerIn: parent; text: "\u2014"; color: hdrMin.containsMouse ? "#d0d4e0" : "#505568"; font.pixelSize: 13; font.weight: Font.Bold }
                            MouseArea { id: hdrMin; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: appWindow.showMinimized() }
                        }
                        Item { width: 6 }
                        Rectangle {
                            width: 28; height: 28; radius: 14
                            color: hdrClose.containsMouse ? (hdrClose.pressed ? "#e06060" : "#c05050") : "transparent"
                            scale: hdrClose.pressed ? 0.85 : (hdrClose.containsMouse ? 1.12 : 1.0)
                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                            Text { anchors.centerIn: parent; text: "\u2715"; color: hdrClose.containsMouse ? "#fff" : "#505568"; font.pixelSize: 12; font.weight: Font.Bold }
                            MouseArea { id: hdrClose; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: appWindow.close() }
                        }
                    }
                }

                Rectangle {
                    id: pageContainer
                    Layout.fillWidth: true; Layout.fillHeight: true; color: "transparent"
                    layer.enabled: true; layer.smooth: true
                    clip: true

                    // ========== HOMEPAGE ==========
                    Rectangle {
                        id: homePage
                        anchors.fill: parent
                        opacity: navListIndex === 0 && !showVersionSelect && !showVersionSettings ? 1 : 0
                        visible: opacity > 0
                        enabled: opacity >= 1
                        Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

                        gradient: Gradient {
                            GradientStop { position: 0.0; color: "#0c0f16" }
                            GradientStop { position: 0.5; color: "#111520" }
                            GradientStop { position: 1.0; color: "#0e111a" }
                        }

                        property string displayName: backend ? (backend.username || "") : ""
                        property bool loggedIn: backend ? backend.username !== "" : false
                        property int backendLoginType: backend ? (backend.isOnline ? 0 : 1) : -1

                        Connections {
                            target: backend; enabled: backend !== null
                            function onUsernameChanged() {
                                homePage.displayName = backend.username
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
                                        Behavior on color { ColorAnimation { duration: 200 } }
                                    }
                                    Text { anchors.centerIn: parent; text: "正版登录"; font.pixelSize: 13; color: loginMode === 0 ? "#e4e8f2" : "#9498a8"; font.weight: loginMode === 0 ? Font.DemiBold : Font.Normal }
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
                                        Behavior on color { ColorAnimation { duration: 200 } }
                                    }
                                    Text { anchors.centerIn: parent; text: "离线模式"; font.pixelSize: 13; color: loginMode === 1 ? "#e4e8f2" : "#9498a8"; font.weight: loginMode === 1 ? Font.DemiBold : Font.Normal }
                                    MouseArea { anchors.fill: parent; onClicked: { loginMode = 1; if (backend) { backend.lastLoginMode = 1; toastManager.show("已切换至离线模式") } } }
                                }
                            }
                        }

                        // Premium login form
                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: loginSwitch.bottom; anchors.topMargin: 20
                            width: 300
                            property bool showForm: loginMode === 0 && !(homePage.loggedIn && homePage.backendLoginType === 0)
                            opacity: showForm ? 1 : 0
                            visible: opacity > 0
                            scale: showForm ? 1 : 0.9
                            transformOrigin: Item.Top
                            Behavior on opacity { NumberAnimation { duration: 350; easing.type: Easing.OutCubic } }
                            Behavior on scale { NumberAnimation { duration: 450; easing.type: Easing.OutBack } }
                            height: childrenRect.height; color: "transparent"
                            ColumnLayout {
                                width: parent.width; spacing: 12
                                Rectangle {
                                    Layout.fillWidth: true; height: 40; radius: 7
                                    color: "#11141c"; border.color: "#1e2230"
                                    Text { anchors.centerIn: parent; text: "选择已登录账号"; color: "#9498a8"; font.pixelSize: 13 }
                                    MouseArea { anchors.fill: parent }
                                }
                                Rectangle {
                                    Layout.alignment: Qt.AlignHCenter; width: 160; height: 36; radius: 7
                                    color: "#2a3878"
                                    Text { anchors.centerIn: parent; text: "Microsoft 登录"; color: "#e4e8f2"; font.pixelSize: 13; font.weight: Font.DemiBold }
                                    MouseArea { anchors.fill: parent }
                                }
                            }
                        }

                        // Offline login form
                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: loginSwitch.bottom; anchors.topMargin: 20
                            width: 300
                            property bool showForm: loginMode === 1 && !(homePage.loggedIn && homePage.backendLoginType === 1)
                            opacity: showForm ? 1 : 0
                            visible: opacity > 0
                            scale: showForm ? 1 : 0.9
                            transformOrigin: Item.Top
                            Behavior on opacity { NumberAnimation { duration: 350; easing.type: Easing.OutCubic } }
                            Behavior on scale { NumberAnimation { duration: 450; easing.type: Easing.OutBack } }
                            height: childrenRect.height; color: "transparent"
                            ColumnLayout {
                                width: parent.width; spacing: 12
                                Rectangle {
                                    Layout.fillWidth: true; height: 40; radius: 7
                                    color: "#11141c"; border.color: "#1e2230"
                                    TextInput {
                                        id: offlineNameInput
                                        anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12
                                        color: "#e4e8f2"; font.pixelSize: 13
                                        verticalAlignment: TextInput.AlignVCenter
                                    }
                                    Text {
                                        anchors.left: parent.left; anchors.leftMargin: 12
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: "输入用户名"; color: "#a8b0c0"
                                        font.pixelSize: 13
                                        visible: !offlineNameInput.text
                                    }
                                }
                                Rectangle {
                                    Layout.alignment: Qt.AlignHCenter; width: 100; height: 36; radius: 7
                                    color: "#3a4eb8"
                                    Text { anchors.centerIn: parent; text: "登录"; color: "#e8ecf8"; font.pixelSize: 13; font.weight: Font.DemiBold }
                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: {
                                            if (backend) {
                                                var name = offlineNameInput.text.trim() || "Player"
                                                backend.offlineLogin(name)
                                                addOfflineHistory(name)
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
                            property bool showState: homePage.loggedIn && homePage.backendLoginType === loginMode
                            opacity: showState ? 1 : 0
                            visible: opacity > 0
                            scale: showState ? 1 : 0.85
                            transformOrigin: Item.Center
                            Behavior on opacity { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } }
                            Behavior on scale { NumberAnimation { duration: 500; easing.type: Easing.OutElastic } }
                            height: childrenRect.height; color: "transparent"
                            ColumnLayout {
                                anchors.horizontalCenter: parent.horizontalCenter; spacing: 8

                                // Avatar + Name
                                RowLayout {
                                    Layout.alignment: Qt.AlignHCenter; spacing: 10
                                    Rectangle {
                                        width: 40; height: 40; radius: 20; color: "#1a1f2e"
                                        Image {
                                            anchors.fill: parent; anchors.margins: 2
                                            source: backend ? backend.skinPath : ""
                                            fillMode: Image.PreserveAspectFit
                                        }
                                    }
                                    Text { text: homePage.displayName; font.pixelSize: 16; font.bold: true; color: "#e4e8f2" }
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
                                        Text { anchors.centerIn: parent; text: "登出"; font.pixelSize: 11; color: "#c05050" }
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
                                    Text { anchors.centerIn: parent; text: "启动游戏"; font.pixelSize: 15; font.weight: Font.Bold; color: "#e8ecf8" }
                                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            if (!backend) return
                                            if (!currentSelectedVersion) {
                                                toastManager.show("请先选择版本")
                                                return
                                            }
                                            if (!backend.username) {
                                                toastManager.show("请先登录账号")
                                                return
                                            }
                                            backend.launch(currentSelectedVersion)
                                        }
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true; spacing: 10
                                    Rectangle {
                                        Layout.fillWidth: true; height: 36; radius: 7; color: "#0e1118"; border.color: "#1e2230"
                                        MouseArea { anchors.fill: parent; onClicked: { showVersionSelect = true } }
                                        RowLayout {
                                            anchors.centerIn: parent; spacing: 6
                                            Rectangle { width: 8; height: 2; radius: 1; color: "#5068c8" }
                                            Text { text: "版本选择"; font.pixelSize: 13; font.weight: Font.Medium; color: "#b0b8c8" }
                                        }
                                    }
                                    Rectangle {
                                        Layout.fillWidth: true; height: 36; radius: 7; color: "#0e1118"; border.color: "#1e2230"
                                        MouseArea { anchors.fill: parent; onClicked: { showVersionSettings = true } }
                                        RowLayout {
                                            anchors.centerIn: parent; spacing: 6
                                            Rectangle { width: 8; height: 8; radius: 2; color: "transparent"; border.color: "#9498a8"; border.width: 1.5 }
                                            Text { text: "版本设置"; font.pixelSize: 13; font.weight: Font.Medium; color: "#b0b8c8" }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // ========== DOWNLOAD & SETTINGS ==========
                    Rectangle { anchors.fill: parent; visible: navListIndex === 1; color: "#0c0f16"
                        Loader { id: downloadPageLoader; anchors.fill: parent; active: navListIndex === 1; source: active ? "DownloadPage.qml" : ""
                            onLoaded: {
                                item.mainWindow = appWindow
                                item.triggerDownloadBall.connect(function(sx, sy) {
                                    appWindow.animateDownloadBall(sx, sy)
                                })
                                // Apply pending sub-tab navigation from --navigate
                                if (pendingSubTab >= 0) {
                                    item.currentTab = pendingSubTab
                                    console.log("[auto-test] navigateTo: download tab", pendingSubTab)
                                    pendingSubTab = -1
                                }
                            }
                        } }
                    Rectangle { anchors.fill: parent; visible: navListIndex === 2; color: "#0c0f16"
                        Loader { anchors.fill: parent; active: navListIndex === 2; source: active ? "SettingsPage.qml" : "" } }

                    // ========== DOWNLOAD PROGRESS PAGE ==========
                    Rectangle { anchors.fill: parent; visible: navListIndex >= 3 && navModel.get(navListIndex).pageKey === "download_progress"; color: "#0c0f16"
                        Loader { id: progressLoader; anchors.fill: parent; active: true; source: "DownloadProgressPage.qml" } }

                    // ========== VERSION SELECT OVERLAY ==========
                    Rectangle {
                        id: versionSelectOverlay
                        anchors.fill: parent; color: "#0c0f16"; z: 5
                        opacity: showVersionSelect ? 1 : 0
                        visible: opacity > 0
                        Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
                        property int activeGameDirIndex: 0

                        // Refresh installed versions when overlay opens
                        onVisibleChanged: {
                            if (visible && backend) {
                                backend.refreshVersionDetails()
                                deferRefreshTimer.start()
                            }
                        }

                        // 延迟刷新定时器 — 先更新UI再扫描文件避免卡顿
                        Timer {
                            id: deferRefreshTimer
                            interval: 80
                            onTriggered: {
                                if (backend) { backend.refreshInstalledList(); backend.refreshGameDirInfo() }
                            }
                        }

                        Rectangle { x: 16; y: 16; height: 28; width: 80; radius: 5; color: "transparent"
                            scale: vsBackHover.containsMouse ? 1.06 : 1.0
                            Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                            Text { anchors.centerIn: parent; text: "\u2190 返回启动"; color: "#a8b0c0"; font.pixelSize: 12 }
                            MouseArea { id: vsBackHover; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { showVersionSelect = false } }
                        }
                        RowLayout {
                            anchors.fill: parent; anchors.margins: 16; anchors.topMargin: 52; spacing: 16
                            Rectangle {
                                Layout.preferredWidth: Math.min(220, parent.width * 0.35); Layout.fillHeight: true
                                color: "#11141c"; radius: 8; border.color: "#1a1e28"
                                ColumnLayout {
                                    anchors.fill: parent; anchors.margins: 14; spacing: 6
                                    Text { text: "游戏文件夹"; font.pixelSize: 10; color: "#9ca0b4"; font.letterSpacing: 1.5 }
                                    ListModel { id: gameDirModel }
                                    Component.onCompleted: {
                                        var dirs = backend ? backend.gameDirectories : []
                                        for (var d = 0; d < dirs.length; d++) {
                                            gameDirModel.append({ path: dirs[d], display: d === 0 ? ".minecraft（默认）" : dirs[d] })
                                        }
                                    }
                                    Connections {
                                        target: backend; enabled: backend !== null
                                        function onGameDirChanged() {
                                            gameDirModel.clear()
                                            var dirs = backend.gameDirectories
                                            for (var d = 0; d < dirs.length; d++) {
                                                gameDirModel.append({ path: dirs[d], display: d === 0 ? ".minecraft（默认）" : dirs[d] })
                                            }
                                        }
                                    }
                                    ScrollView {
                                        Layout.fillWidth: true; Layout.preferredHeight: Math.min(gameDirModel.count * 36 + 4, 160)
                                        clip: true
                                        ListView {
                                            id: gameDirList
                                            anchors.fill: parent
                                            model: gameDirModel
                                            spacing: 2
                                            delegate: Rectangle {
                                                width: gameDirList.width
                                                height: 36; radius: 6
                                                color: dirMouse.containsMouse ? "#1a1f2e" : (versionSelectOverlay.activeGameDirIndex === index ? "#0e131a" : "transparent")
                                                border.color: versionSelectOverlay.activeGameDirIndex === index ? "#2a4eb8" : "transparent"
                                                scale: dirMouse.containsMouse ? 1.02 : 1.0
                                                Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                                                Text {
                                                    anchors.left: parent.left; anchors.leftMargin: 12
                                                    anchors.right: parent.right; anchors.rightMargin: 8
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    text: model.display
                                                    font.pixelSize: 12
                                                    color: versionSelectOverlay.activeGameDirIndex === index ? "#8aa8f0" : "#c0c8d8"
                                                    elide: Text.ElideRight
                                                    maximumLineCount: 1
                                                }
                                                ToolTip {
                                                    visible: dirMouse.containsMouse
                                                    text: model.path || model.display
                                                    delay: 600
                                                }
                                                MouseArea {
                                                    id: dirMouse; anchors.fill: parent; hoverEnabled: true
                                                    cursorShape: Qt.PointingHandCursor
                                                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                                                    onClicked: {
                                                        versionSelectOverlay.activeGameDirIndex = index
                                                        if (backend) { backend.setGameDir(index); toastManager.show("正在扫描版本...") }
                                                    }
                                                    onPressed: function(mouse) {
                                                        if (mouse.button === Qt.RightButton) {
                                                            if (index === 0) {
                                                                if (backend) backend.openGameDir(0)
                                                            } else {
                                                                confirmDialog.title = "移除文件夹"
                                                                confirmDialog.message = "确定要移除 " + model.display + " 吗？\n（不会删除本地文件）"
                                                                confirmDialog.onAccept = function() { if (backend) backend.removeGameDir(index) }
                                                                confirmDialog.visible = true
                                                            }
                                                            mouse.accepted = true
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    Item { height: 4; width: 1 }

                                    // Directory info
                                    Rectangle {
                                        Layout.fillWidth: true; height: childrenRect.height + 16; radius: 6
                                        color: "#0d1018"
                                        visible: backend ? (backend.gameDirInfo !== undefined && backend.gameDirInfo.versionCount > 0) : false
                                        ColumnLayout {
                                            anchors.left: parent.left; anchors.leftMargin: 12
                                            anchors.right: parent.right; anchors.rightMargin: 12
                                            anchors.top: parent.top; anchors.topMargin: 8
                                            spacing: 4
                                            RowLayout {
                                                Text { text: "版本"; font.pixelSize: 10; color: "#a8b0c0"; Layout.preferredWidth: 40 }
                                                Text { text: backend ? (backend.gameDirInfo.versionCount || 0) : 0; font.pixelSize: 12; font.weight: Font.Medium; color: "#8aa8f0" }
                                            }
                                            RowLayout {
                                                Text { text: "模组"; font.pixelSize: 10; color: "#a8b0c0"; Layout.preferredWidth: 40 }
                                                Text { text: backend ? (backend.gameDirInfo.modCount || 0) : 0; font.pixelSize: 12; font.weight: Font.Medium; color: "#d4dcf0" }
                                            }
                                            RowLayout {
                                                Text { text: "占用"; font.pixelSize: 10; color: "#a8b0c0"; Layout.preferredWidth: 40 }
                                                Text { text: backend ? (backend.gameDirInfo.sizeDisplay || "") : ""; font.pixelSize: 12; font.weight: Font.Medium; color: "#d4dcf0" }
                                            }
                                        }
                                    }

                                    Item { height: 4; width: 1 }
                                    Rectangle { Layout.fillWidth: true; height: 30; radius: 6; color: "transparent"; border.color: "#1e2230"; border.width: 1
                                        scale: addDirHover.containsMouse ? 1.03 : 1.0
                                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                                        Text { anchors.centerIn: parent; text: "+ 添加文件夹"; font.pixelSize: 11; color: addDirHover.containsMouse ? "#b0b8e0" : "#9498a8" }
                                        MouseArea {
                                            id: addDirHover
                                            anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                            onClicked: { toastManager.show("功能开发中，请前往文件夹手动添加") }
                                        }
                                    }
                                    Rectangle { Layout.fillWidth: true; height: 30; radius: 6; color: "transparent"; border.color: "#1e2230"; border.width: 1
                                        scale: importHover.containsMouse ? 1.03 : 1.0
                                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                                        Text { anchors.centerIn: parent; text: "导入整合包"; font.pixelSize: 11; color: importHover.containsMouse ? "#b0b8e0" : "#9498a8" }
                                        MouseArea {
                                            id: importHover
                                            anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                            onClicked: { toastManager.show("导入整合包功能开发中") }
                                        }
                                    }

                                    // Disk space bar
                                    Rectangle {
                                        Layout.fillWidth: true; height: 36; radius: 6; color: "#0d1018"
                                        Rectangle { anchors.left: parent.left; anchors.leftMargin: 10; anchors.verticalCenter: parent.verticalCenter
                                            width: 14; height: 14; radius: 7
                                            color: backend && backend.diskPercent > 90 ? "#c05050" : (backend && backend.diskPercent > 70 ? "#e0a040" : "#4bc870")
                                        }
                                        Text {
                                            anchors.left: parent.left; anchors.leftMargin: 30; anchors.verticalCenter: parent.verticalCenter
                                            text: {
                                                if (!backend) return "磁盘信息不可用"
                                                var pct = backend.diskPercent
                                                var freeGb = (backend.diskFree / 1073741824).toFixed(1)
                                                var status = pct > 95 ? "危险" : (pct > 80 ? "偏低" : "正常")
                                                return "剩余 " + freeGb + " GB  (" + status + ")"
                                            }
                                            font.pixelSize: 11; color: "#808898"
                                        }
                                    }
                                }
                            }
                            Rectangle {
                                Layout.fillWidth: true; Layout.fillHeight: true
                                color: "#11141c"; radius: 8; border.color: "#1a1e28"
                                ColumnLayout {
                                    id: versionRightPanel
                                    anchors.fill: parent; anchors.margins: 12; spacing: 6

                                    // Header row: title + refresh + search + sort
                                    property bool verRefreshPressed: false
                                    property bool installBtnPressed: false
                                    RowLayout {
                                        Layout.fillWidth: true; spacing: 8
                                        Text { text: "已安装版本"; font.pixelSize: 10; color: "#9ca0b4"; font.letterSpacing: 1.5 }
                                        // Refresh installed versions button
                                        Rectangle {
                                            id: verRefreshBtn
                                            width: verRefreshText.implicitWidth + 16; height: 28; radius: 4
                                            color: verRefreshHover.hovered ? "#1a2848" : "#0d1018"
                                            border.color: verRefreshHover.hovered ? "#5068c8" : "#1a1f2e"
                                            border.width: 1
                                            scale: versionRightPanel.verRefreshPressed ? 0.88 : (verRefreshHover.hovered ? 1.06 : 1.0)
                                            Behavior on color { ColorAnimation { duration: 150 } }
                                            Behavior on border.color { ColorAnimation { duration: 150 } }
                                            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                                            Text {
                                                id: verRefreshText
                                                anchors.centerIn: parent
                                                text: "⟳ 刷新"; font.pixelSize: 11
                                                color: verRefreshHover.hovered ? "#8aa8f0" : "#7e8596"
                                            }
                                            HoverHandler { id: verRefreshHover }
                                            ToolTip { visible: verRefreshHover.hovered; text: "重新扫描已安装版本"; delay: 500 }
                                            MouseArea {
                                                anchors.fill: parent; hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onPressed: versionRightPanel.verRefreshPressed = true
                                                onReleased: versionRightPanel.verRefreshPressed = false
                                                onClicked: {
                                                    if (backend) {
                                                        backend.refreshVersionDetails()
                                                        toastManager.show("正在扫描版本...")
                                                    }
                                                }
                                            }
                                        }
                                        Rectangle {
                                            Layout.fillWidth: true; height: 28; radius: 4; color: "#0d1018"
                                            border.color: searchField.activeFocus ? "#5068c8" : "#1a1f2e"
                                            TextInput {
                                                id: searchField; anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 10
                                                color: "#e4e8f2"; font.pixelSize: 12
                                                verticalAlignment: TextInput.AlignVCenter
                                                Text {
                                                    anchors.left: parent.left; anchors.leftMargin: 10
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    text: "搜索版本..."; color: "#9ca0b4"; font.pixelSize: 12
                                                    visible: !searchField.text
                                                }
                                            }
                                        }
                                        // Install button — shortcut to download new versions
                                        Rectangle {
                                            width: 28; height: 28; radius: 4; color: installBtnHover.hovered ? "#2553a8" : "#3a4eb8"
                                            scale: versionRightPanel.installBtnPressed ? 0.9 : 1.0
                                            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                                            Text { anchors.centerIn: parent; text: "+"; font.pixelSize: 18; font.weight: Font.Bold; color: "#e8ecf8" }
                                            HoverHandler { id: installBtnHover }
                                            ToolTip { visible: installBtnHover.hovered; text: "安装新版本"; delay: 500 }
                                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                onPressed: versionRightPanel.installBtnPressed = true
                                                onReleased: versionRightPanel.installBtnPressed = false
                                                onClicked: { showVersionSelect = false; switchPage(1); toastManager.show("正在前往下载页面") }  // 导航到下载页
                                            }
                                        }
                                        // Sort button
                                        Rectangle {
                                            id: sortBtn
                                            width: 70; height: 28; radius: 4; color: sortHover.hovered ? "#1a2848" : "#0d1018"
                                            border.color: sortHover.hovered ? "#5068c8" : "#1a1f2e"
                                            scale: sortPressed ? 0.92 : (sortHover.hovered ? 1.03 : 1.0)
                                            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                                            property int versionSortIndex: 0
                                            RowLayout {
                                                anchors.centerIn: parent; spacing: 4
                                                Text { text: sortBtn.versionSortIndex === 0 ? "↓ 版本" : (sortBtn.versionSortIndex === 1 ? "↓ 版本" : (sortBtn.versionSortIndex === 2 ? "↓ 大小" : "↓ 模组")); font.pixelSize: 10; color: "#b0b8c8" }
                                            }
                                            HoverHandler { id: sortHover }
                                            property bool sortPressed: false
                                            MouseArea {
                                                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                onPressed: sortPressed = true
                                                onReleased: sortPressed = false
                                                onClicked: {
                                                    sortBtn.versionSortIndex = (sortBtn.versionSortIndex + 1) % 4
                                                    versionRightPanel.applyFilterSort()
                                                }
                                            }
                                        }

                                        // Loader filter
                                        Rectangle {
                                            id: loaderFilter
                                            width: 80; height: 28; radius: 4; color: loaderFiltHover.hovered ? "#1a2848" : "#0d1018"
                                            border.color: loaderFiltHover.hovered ? "#5068c8" : "#1a1f2e"
                                            scale: loadFiltPressed ? 0.92 : (loaderFiltHover.hovered ? 1.03 : 1.0)
                                            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                                            property int loaderFilterIndex: 0  // 0=全部 1=原版 2=Forge 3=Fabric 4=NeoForge 5=Quilt
                                            property var loaderLabels: ["全部类型", "原版", "Forge", "Fabric", "NeoForge", "Quilt"]
                                            property bool loadFiltPressed: false
                                            RowLayout {
                                                anchors.centerIn: parent; spacing: 4
                                                Text { text: loaderFilter.loaderLabels[loaderFilter.loaderFilterIndex]; font.pixelSize: 10; color: "#b0b8c8" }
                                            }
                                            HoverHandler { id: loaderFiltHover }
                                            MouseArea {
                                                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                onPressed: loadFiltPressed = true
                                                onReleased: loadFiltPressed = false
                                                onClicked: {
                                                    loaderFilter.loaderFilterIndex = (loaderFilter.loaderFilterIndex + 1) % loaderFilter.loaderLabels.length
                                                    versionRightPanel.applyFilterSort()
                                                }
                                            }
                                        }
                                    }

                                    // Version list from detail model
                                    ListModel { id: versionDetailModel }
                                    ListModel { id: versionFilteredModel }

                                    function applyFilterSort() {
                                        var data = []
                                        for (var i = 0; i < versionDetailModel.count; i++) {
                                            data.push(versionDetailModel.get(i))
                                        }
                                        var srch = searchField.text.toLowerCase()
                                        data = data.filter(function(d) {
                                            var nameMatch = !srch || d.id.toLowerCase().indexOf(srch) >= 0 || d.loaderType.toLowerCase().indexOf(srch) >= 0
                                            if (!nameMatch) return false
                                            var li = loaderFilter.loaderFilterIndex
                                            if (li === 0) return true  // 全部
                                            var lt = (d.loaderType || "原版").toLowerCase()
                                            if (li === 1) return lt === "原版" || lt === "vanilla"
                                            if (li === 2) return lt === "forge"
                                            if (li === 3) return lt === "fabric"
                                            if (li === 4) return lt === "neoforge"
                                            if (li === 5) return lt === "quilt"
                                            return true
                                        })
                                        var si = sortBtn.versionSortIndex
                                        data.sort(function(a, b) {
                                            if (si === 0) return b.id.localeCompare(a.id, undefined, {numeric: true})
                                            if (si === 1) return a.id.localeCompare(b.id, undefined, {numeric: true})
                                            if (si === 2) return b.sizeBytes - a.sizeBytes
                                            return b.modCount - a.modCount
                                        })
                                        versionFilteredModel.clear()
                                        for (var j = 0; j < data.length; j++) {
                                            versionFilteredModel.append(data[j])
                                        }
                                    }

                                    function populateVersionDetails() {
                                        versionDetailModel.clear()
                                        var details = backend ? backend.versionDetails : []
                                        if (!details || details.length === 0) return
                                        for (var i = 0; i < details.length; i++) {
                                            versionDetailModel.append(details[i])
                                        }
                                        applyFilterSort()
                                    }

                                    Connections {
                                        target: backend; enabled: backend !== null
                                        function onVersionDetailsChanged() { versionRightPanel.populateVersionDetails() }
                                        function onVersionDetailsReady() {
                                            versionRightPanel.populateVersionDetails()
                                            toastManager.show("版本扫描完成")
                                        }
                                    }

                                    Connections {
                                        target: searchField
                                        function onTextChanged() { versionRightPanel.applyFilterSort() }
                                    }

                                    ScrollView {
                                        Layout.fillWidth: true; Layout.fillHeight: true
                                        clip: true
                                        ScrollBar.vertical.policy: ScrollBar.AsNeeded
                                        ListView {
                                            id: versionSelectList
                                            anchors.fill: parent
                                            model: versionFilteredModel
                                            spacing: 4
                                            clip: true
                                            delegate: Rectangle {
                                                width: versionSelectList.width - 4
                                                height: 52; radius: 6
                                                color: verMouse2.containsMouse ? "#191e2a" : "transparent"
                                                border.color: currentSelectedVersion === model.id ? "#4a5ec8" : "transparent"
                                                border.width: currentSelectedVersion === model.id ? 1.5 : 0

                                                RowLayout {
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    anchors.left: parent.left; anchors.leftMargin: 12
                                                    anchors.right: parent.right; anchors.rightMargin: 6
                                                    spacing: 8

                                                    // Loader tag
                                                    Rectangle {
                                                        height: 20; implicitWidth: loaderTag.implicitWidth + 12; radius: 4
                                                        color: {
                                                            var t = model.loaderType
                                                            if (t === "Forge") return "#c05050"
                                                            if (t === "Fabric") return "#50a0c0"
                                                            if (t === "NeoForge") return "#c08050"
                                                            if (t === "Quilt") return "#50c0a0"
                                                            return "#4a6a8a"
                                                        }
                                                        Text {
                                                            id: loaderTag
                                                            anchors.centerIn: parent
                                                            text: model.loaderType + (model.loaderVersion ? " " + model.loaderVersion : "")
                                                            font.pixelSize: 10; font.weight: Font.Medium; color: "#e8ecf8"
                                                        }
                                                    }

                                                    // Version id
                                                    Text {
                                                        text: model.id
                                                        font.pixelSize: 14; font.weight: Font.DemiBold; color: "#e4e8f2"
                                                        Layout.preferredWidth: 100
                                                        elide: Text.ElideRight
                                                    }

                                                    // Version type tag
                                                    Rectangle {
                                                        height: 18; implicitWidth: vtypeTag.implicitWidth + 10; radius: 3
                                                        color: {
                                                            var vt = model.versionType
                                                            if (vt === "snapshot") return "#e0a040"
                                                            if (vt === "old") return "#707088"
                                                            if (vt === "modded") return "#c060a0"
                                                            return "#4bc870"
                                                        }
                                                        Text {
                                                            id: vtypeTag
                                                            anchors.centerIn: parent
                                                            text: {
                                                                var vt = model.versionType
                                                                if (vt === "snapshot") return "快照"
                                                                if (vt === "old") return "旧版"
                                                                if (vt === "modded") return "整合"
                                                                return "正式版"
                                                            }
                                                            font.pixelSize: 9; color: "#e8ecf8"
                                                        }
                                                    }

                                                    Item { Layout.fillWidth: true }

                                                    // Size + mod count
                                                    ColumnLayout {
                                                        spacing: 0
                                                        Text { text: model.sizeDisplay || ""; font.pixelSize: 11; font.weight: Font.Medium; color: "#808898"; Layout.alignment: Qt.AlignRight }
                                                        Text { text: model.modCount > 0 ? model.modCount + " 模组" : ""; font.pixelSize: 10; color: "#989cb0"; Layout.alignment: Qt.AlignRight }
                                                    }

                                                    Item { width: 4 }
                                                }

                                                MouseArea {
                                                    id: verMouse2
                                                    anchors.fill: parent
                                                    hoverEnabled: true
                                                    cursorShape: Qt.PointingHandCursor
                                                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                                                    onClicked: function(mouse) {
                                                        currentSelectedVersion = model.id
                                                        if (backend) backend.setSelectedVersion(model.id)
                                                        showVersionSelect = false
                                                    }
                                                    onPressed: function(mouse) {
                                                        if (mouse.button === Qt.RightButton) {
                                                            currentSelectedVersion = model.id
                                                            if (backend) backend.setSelectedVersion(model.id)
                                                            showVersionSettings = true
                                                            mouse.accepted = true
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    // Empty state
                                    Text {
                                        Layout.alignment: Qt.AlignHCenter; Layout.topMargin: 40
                                        text: versionFilteredModel.count === 0 && backend && backend.versionDetails && backend.versionDetails.length > 0
                                              ? "没有匹配的版本" : (backend && backend.installedVersions && backend.installedVersions.length === 0 ? "还没有安装任何版本\n前往下载页面安装第一个版本吧" : "")
                                        font.pixelSize: 12; color: "#a8b0c0"
                                        horizontalAlignment: Text.AlignHCenter
                                        visible: versionFilteredModel.count === 0
                                    }
                                }
                            }
                        }
                    }

                    // ========== VERSION SETTINGS OVERLAY ==========
                    Rectangle {
                        id: versionSettingsOverlay
                        anchors.fill: parent; color: "#0c0f16"; z: 5
                        opacity: showVersionSettings ? 1 : 0
                        visible: opacity > 0
                        Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
                        onVisibleChanged: {
                            if (visible && backend) {
                                // 版本切换时刷新所有数据列表（跟随版本隔离）
                                backend.refreshVersionDetails()
                                modListModel.clear(); var m = backend.listMods(); for (var i = 0; i < m.length; i++) modListModel.append(m[i])
                                rpListModel.clear(); var p = backend.listResourcePacks(); for (var i = 0; i < p.length; i++) rpListModel.append(p[i])
                                saveListModel.clear(); var s = backend.listSaves(); for (var i = 0; i < s.length; i++) saveListModel.append(s[i])
                                // 重置校验状态
                                _verifyRunning = false
                                _verifyProgressDone = 0
                                _verifyProgressTotal = 0
                                _verifyResultText = ""
                                _verifyResultOk = false
                            }
                        }

                        // ── 校验状态（本地追踪，不依赖后端不触发的NOTIFY信号） ──
                        property bool _verifyRunning: false
                        property int _verifyProgressDone: 0
                        property int _verifyProgressTotal: 0
                        property string _verifyResultText: ""
                        property bool _verifyResultOk: false
                        property var _verifyFailedFiles: []
                        property bool _verifyHasFailed: false

                        // ── 信号连接 ──
                        Connections {
                            target: backend
                            enabled: versionSettingsOverlay.visible

                            function onVerifyStarted() {
                                versionSettingsOverlay._verifyRunning = true
                                versionSettingsOverlay._verifyProgressDone = 0
                                versionSettingsOverlay._verifyProgressTotal = 0
                                versionSettingsOverlay._verifyResultText = ""
                                versionSettingsOverlay._verifyResultOk = false
                                versionSettingsOverlay._verifyFailedFiles = []
                                versionSettingsOverlay._verifyHasFailed = false
                            }

                            function onVerifyProgress(checked, total) {
                                versionSettingsOverlay._verifyProgressDone = checked
                                versionSettingsOverlay._verifyProgressTotal = total
                            }

                            function onVerifyFinished(allPassed) {
                                versionSettingsOverlay._verifyRunning = false
                                versionSettingsOverlay._verifyResultOk = allPassed
                                // 不覆盖 logMessage 已积累的详细结果，仅在结果为空时显示默认文本
                                if (versionSettingsOverlay._verifyResultText === "") {
                                    var total = versionSettingsOverlay._verifyProgressTotal
                                    versionSettingsOverlay._verifyResultText = allPassed
                                        ? ("✅ 校验完成: " + total + " 个文件全部通过")
                                        : ("❌ 校验完成: " + total + " 个文件全部通过。")
                                }
                            }

                            function onVerifyFailedFiles(files) {
                                versionSettingsOverlay._verifyFailedFiles = files || []
                                versionSettingsOverlay._verifyHasFailed = (files && files.length > 0)
                            }

                            function onLogMessage(msg) {
                                // 如果正在校验并且收到日志，追加到结果文本
                                if (versionSettingsOverlay._verifyRunning || msg.indexOf("校验") >= 0) {
                                    if (versionSettingsOverlay._verifyResultText !== "") {
                                        versionSettingsOverlay._verifyResultText += "\n" + msg
                                    } else {
                                        versionSettingsOverlay._verifyResultText = msg
                                    }
                                }
                            }
                        }

                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: 16; spacing: 0

                            // TOP BAR: version info + actions ===
                            Rectangle {
                                Layout.fillWidth: true; height: 56; radius: 8
                                color: "#11141c"; border.color: "#1a1e28"
                                RowLayout {
                                    anchors.fill: parent; anchors.margins: 14; spacing: 12

                                    // Back button
                                    Rectangle {
                                        width: 60; height: 28; radius: 6; color: "transparent"; border.color: "#1a1f2e"
                                        scale: backHover.containsMouse ? 1.06 : 1.0
                                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                                        Text { anchors.centerIn: parent; text: "← 返回"; font.pixelSize: 11; color: "#989cb0" }
                                        MouseArea { id: backHover; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { showVersionSettings = false } }
                                    }

                                    // Version label
                                    Text {
                                        Layout.fillWidth: true
                                        text: currentSelectedVersion || "未选择版本"
                                        font.pixelSize: 16; font.weight: Font.Bold; color: "#e4e8f2"
                                    }

                                    // Loader tag
                                    Rectangle {
                                        visible: {
                                            if (!backend || !backend.versionDetails) return false
                                            for (var i = 0; i < backend.versionDetails.length; i++)
                                                if (backend.versionDetails[i].id === currentSelectedVersion) return true
                                            return false
                                        }
                                        height: 20; implicitWidth: settingsLoaderTag.implicitWidth + 10; radius: 4
                                        color: {
                                            if (!backend || !backend.versionDetails) return "#4a6a8a"
                                            for (var i = 0; i < backend.versionDetails.length; i++) {
                                                if (backend.versionDetails[i].id === currentSelectedVersion) {
                                                    var t = backend.versionDetails[i].loaderType
                                                    if (t === "Forge") return "#c05050"
                                                    if (t === "Fabric") return "#50a0c0"
                                                    if (t === "NeoForge") return "#c08050"
                                                    if (t === "Quilt") return "#50c0a0"
                                                    return "#4a6a8a"
                                                }
                                            }
                                            return "#4a6a8a"
                                        }
                                        Text {
                                            id: settingsLoaderTag
                                            anchors.centerIn: parent
                                            text: {
                                                if (!backend || !backend.versionDetails) return ""
                                                for (var i = 0; i < backend.versionDetails.length; i++) {
                                                    if (backend.versionDetails[i].id === currentSelectedVersion)
                                                        return backend.versionDetails[i].loaderType || ""
                                                }
                                                return ""
                                            }
                                            font.pixelSize: 10; font.weight: Font.Medium; color: "#e8ecf8"
                                        }
                                    }

                                    // Spacer
                                    Item { Layout.fillWidth: true }

                                    // Launch button
                                    Rectangle {
                                        width: 100; height: 32; radius: 6; color: "#3a4eb8"
                                        Text { anchors.centerIn: parent; text: "启动"; font.pixelSize: 13; font.weight: Font.Bold; color: "#e8ecf8" }
                                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                if (!backend) return
                                                if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }
                                                backend.launch(currentSelectedVersion)
                                            }
                                        }
                                    }
                                }
                            }

                            Item { Layout.preferredHeight: 12 }

                            // BODY: sidebar + content ═══
                            RowLayout {
                                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 16
                            Rectangle {
                                Layout.preferredWidth: 170; Layout.fillHeight: true; color: "transparent"
                                property var sectionModel: [
                                    { text: "概览", icon: "" },
                                    { text: "启动配置", icon: "" },
                                    { text: "Mod 管理", icon: "" },
                                    { text: "资源包管理", icon: "" },
                                    { text: "存档管理", icon: "" },
                                    { text: "工具与维护", icon: "" }
                                ]
                                ListView {
                                    id: settingsNav
                                    anchors.fill: parent
                                    model: parent.sectionModel
                                    delegate: Rectangle {
                                        width: settingsNav.width; height: 36; radius: 6
                                        color: ListView.isCurrentItem ? "#162040" : (mouseArea2.containsMouse ? "#11141c" : "transparent")
                                        scale: mouseArea2.containsMouse ? 1.03 : 1.0
                                        Behavior on scale { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                                        Rectangle { anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 3; color: ListView.isCurrentItem ? "#5080e8" : "transparent" }
                                        RowLayout {
                                            anchors.fill: parent; anchors.leftMargin: 16; spacing: 10
                                            Text {
                                                text: modelData.text
                                                font.pixelSize: 13
                                                color: ListView.isCurrentItem ? "#e0e4f8" : (mouseArea2.containsMouse ? "#e4e8fc" : "#9498ac")
                                                font.weight: ListView.isCurrentItem ? Font.Bold : Font.Normal
                                            }
                                            Item { Layout.fillWidth: true }
                                        }
                                        MouseArea { id: mouseArea2; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: settingsNav.currentIndex = index }
                                    }
                                    currentIndex: 0
                                }
                            }
                            Rectangle {
                            }  // close sidebar Rectangle
                            Rectangle {
                                Layout.fillWidth: true; Layout.fillHeight: true
                                color: "#11141c"; radius: 8; border.color: "#1a1e28"

                                // Section 0: 概览 ===
                                ColumnLayout {
                                    anchors.fill: parent; anchors.margins: 24; spacing: 12
                                    visible: settingsNav.currentIndex === 0

                                    // Quick info
                                    Rectangle {
                                        Layout.fillWidth: true; height: 52; radius: 6; color: "#0d1018"
                                        RowLayout {
                                            anchors.fill: parent; anchors.margins: 14; spacing: 12
                                            ColumnLayout { Layout.fillWidth: true; spacing: 2
                                                Text { text: "占用空间"; font.pixelSize: 10; color: "#a8b0c0" }
                                                Text { text: backend && backend.currentVersionSummary ? backend.currentVersionSummary.sizeDisplay : "-"; font.pixelSize: 14; font.weight: Font.Medium; color: "#d4dcf0" }
                                            }
                                            Rectangle { width: 1; height: 32; color: "#1a1f2e" }
                                            ColumnLayout { Layout.fillWidth: true; spacing: 2
                                                Text { text: "已装 Mod"; font.pixelSize: 10; color: "#a8b0c0" }
                                                Text { text: (backend && backend.currentVersionSummary ? backend.currentVersionSummary.modCount : 0) + " 个"; font.pixelSize: 14; font.weight: Font.Medium; color: "#d4dcf0" }

                                            }
                                            Rectangle { width: 1; height: 32; color: "#1a1f2e" }
                                            ColumnLayout { Layout.fillWidth: true; spacing: 2
                                                Text { text: "版本隔离"; font.pixelSize: 10; color: "#a8b0c0" }
                                                Text { text: backend && backend.isolationEnabled ? "已开启" : "未开启"; font.pixelSize: 14; font.weight: Font.Medium; color: backend && backend.isolationEnabled ? "#4bc870" : "#707088" }
                                            }
                                        }
                                    }

                                    // Shortcuts
                                    Text { text: "快捷入口"; font.pixelSize: 10; color: "#9ca0b4"; font.letterSpacing: 1.5 }
                                    Flow {
                                        Layout.fillWidth: true; spacing: 8

                                        // Always visible
                                        Rectangle { width: 120; height: 32; radius: 6; color: shortcutHover0.hovered ? "#3a5ed0" : "#2a4590"
                                            scale: shMouse0.pressed ? 0.92 : 1.0
                                            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                                            Text { anchors.centerIn: parent; text: "版本文件夹"; font.pixelSize: 11; color: "#e8ecf8" }
                                            HoverHandler { id: shortcutHover0 }
                                            MouseArea { id: shMouse0; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }
                                                    if (backend) {
                                                        backend.openVersionDir(currentSelectedVersion)
                                                        toastManager.show("已打开版本文件夹")
                                                    }
                                                }
                                            }
                                        }
                                        Rectangle { width: 120; height: 32; radius: 6; color: shortcutHover1.hovered ? "#3a5ed0" : "#2a4590"
                                            scale: shMouse1.pressed ? 0.92 : 1.0
                                            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                                            Text { anchors.centerIn: parent; text: "存档文件夹"; font.pixelSize: 11; color: "#e8ecf8" }
                                            HoverHandler { id: shortcutHover1 }
                                            MouseArea { id: shMouse1; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                onClicked: { if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }; if (backend) { backend.openSavesFolder(); toastManager.show("已打开存档文件夹") } }
                                            }
                                        }
                                        Rectangle { width: 120; height: 32; radius: 6; color: shortcutHover2.hovered ? "#3a5ed0" : "#2a4590"
                                            scale: shMouse2.pressed ? 0.92 : 1.0
                                            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                                            Text { anchors.centerIn: parent; text: "截图文件夹"; font.pixelSize: 11; color: "#e8ecf8" }
                                            HoverHandler { id: shortcutHover2 }
                                            MouseArea { id: shMouse2; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                onClicked: { if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }; if (backend) { backend.openScreenshotsFolder(); toastManager.show("已打开截图文件夹") } }
                                            }
                                        }
                                        Rectangle { width: 120; height: 32; radius: 6; color: shortcutHover6.hovered ? "#3a5ed0" : "#2a4590"
                                            scale: shMouse6.pressed ? 0.92 : 1.0
                                            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                                            Text { anchors.centerIn: parent; text: "logs 日志"; font.pixelSize: 11; color: "#e8ecf8" }
                                            HoverHandler { id: shortcutHover6 }
                                            MouseArea { id: shMouse6; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                onClicked: { if (backend) { var ok = backend.openLogsFolder(); if (ok) toastManager.show("已打开日志文件夹"); else toastManager.show("无日志文件") } }
                                            }
                                        }
                                        Rectangle { width: 130; height: 32; radius: 6; color: shortcutHover7.hovered ? "#3a5ed0" : "#2a4590"
                                            scale: shMouse7.pressed ? 0.92 : 1.0
                                            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                                            Text { anchors.centerIn: parent; text: "最新启动日志"; font.pixelSize: 11; color: "#e8ecf8" }
                                            HoverHandler { id: shortcutHover7 }
                                            MouseArea { id: shMouse7; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                onClicked: { if (backend) { var ok = backend.openLatestLog(); if (ok) toastManager.show("已打开最新日志"); else toastManager.show("无日志文件") } }
                                            }
                                        }
                                        Rectangle { width: 130; height: 32; radius: 6; color: shortcutHover8.hovered ? "#c85050" : "#9a3838"
                                            scale: shMouse8.pressed ? 0.92 : 1.0
                                            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                                            Text { anchors.centerIn: parent; text: "崩溃日志"; font.pixelSize: 11; color: "#e8ecf8" }
                                            HoverHandler { id: shortcutHover8 }
                                            MouseArea { id: shMouse8; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                onClicked: { if (backend) { var ok = backend.openCrashLog(); if (ok) toastManager.show("已打开崩溃日志"); else toastManager.show("无崩溃报告") } }
                                            }
                                        }

                                        // Copy path
                                        Rectangle { width: 130; height: 32; radius: 6; color: shortcutHoverCp.hovered ? "#3a5ed0" : "#2a4590"
                                            scale: shMouseCp.pressed ? 0.92 : 1.0
                                            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                                            Text { anchors.centerIn: parent; text: "复制版本路径"; font.pixelSize: 11; color: "#e8ecf8" }
                                            HoverHandler { id: shortcutHoverCp }
                                            MouseArea { id: shMouseCp; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }
                                                    if (backend) { backend.copyVersionPath(currentSelectedVersion); toastManager.show("已复制版本路径") }
                                                }
                                            }
                                        }

                                        // Mod-only: visible only for modded versions
                                        Rectangle { width: 120; height: 32; radius: 6; color: shortcutHover3.hovered ? "#3a5ed0" : "#3a4a90"
                                            visible: backend ? backend.isModdedVersion(currentSelectedVersion) : false
                                            scale: shMouse3.pressed ? 0.92 : 1.0
                                            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                                            Text { anchors.centerIn: parent; text: "Mod 文件夹"; font.pixelSize: 11; color: "#e8ecf8" }
                                            HoverHandler { id: shortcutHover3 }
                                            MouseArea { id: shMouse3; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                onClicked: { if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }; if (backend) { backend.openModsFolder(); toastManager.show("已打开 Mod 文件夹") } }
                                            }
                                        }
                                        Rectangle { width: 120; height: 32; radius: 6; color: shortcutHover4.hovered ? "#3a5ed0" : "#3a4a90"
                                            visible: backend ? backend.isModdedVersion(currentSelectedVersion) : false
                                            scale: shMouse4.pressed ? 0.92 : 1.0
                                            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                                            Text { anchors.centerIn: parent; text: "config 文件夹"; font.pixelSize: 11; color: "#e8ecf8" }
                                            HoverHandler { id: shortcutHover4 }
                                            MouseArea { id: shMouse4; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                onClicked: { if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }; if (backend) { backend.openConfigFolder(); toastManager.show("已打开 config 文件夹") } }
                                            }
                                        }
                                        Rectangle { width: 120; height: 32; radius: 6; color: shortcutHover5.hovered ? "#3a5ed0" : "#3a4a90"
                                            visible: backend ? backend.isModdedVersion(currentSelectedVersion) : false
                                            scale: shMouse5.pressed ? 0.92 : 1.0
                                            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                                            Text { anchors.centerIn: parent; text: "光影包"; font.pixelSize: 11; color: "#e8ecf8" }
                                            HoverHandler { id: shortcutHover5 }
                                            MouseArea { id: shMouse5; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                onClicked: { if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }; if (backend) { backend.openShaderPacksFolder(); toastManager.show("已打开光影包文件夹") } }
                                            }
                                        }
                                    }

                                    Item { Layout.fillHeight: true }
                                }

                                // Section 1: 启动配置 ===
                                ColumnLayout {
                                    anchors.fill: parent; anchors.margins: 24; spacing: 12
                                    visible: settingsNav.currentIndex === 1

                                    Flickable {
                                        Layout.fillWidth: true; Layout.fillHeight: true
                                        contentHeight: launchSettingsContent.implicitHeight
                                        clip: true
                                        ColumnLayout {
                                            id: launchSettingsContent
                                            width: parent.width; spacing: 14

                                            // Java 环境
                                            Text { text: "Java 环境"; font.pixelSize: 12; font.weight: Font.DemiBold; color: "#e4e8f2" }
                                            // Recommended Java hint
                                            Text {
                                                text: {
                                                    if (!backend || !currentSelectedVersion) return ""
                                                    var vid = currentSelectedVersion
                                                    var parts = vid.split(".")
                                                    if (parts[0] === "1" && parts.length > 1) {
                                                        var minor = parseInt(parts[1]) || 0
                                                        if (minor >= 18) return "推荐: Java 17+"
                                                        if (minor === 17) return "推荐: Java 16+"
                                                        if (minor >= 12) return "推荐: Java 8"
                                                        return "推荐: Java 8"
                                                    }
                                                    return ""
                                                }
                                                font.pixelSize: 10; color: "#4bc870"
                                            }

                                            Rectangle {
                                                Layout.fillWidth: true; height: 36; radius: 6; color: "transparent"; border.color: "#1a1f2e"
                                                Text { anchors.left: parent.left; anchors.leftMargin: 12; anchors.verticalCenter: parent.verticalCenter
                                                    text: backend ? (backend.javaPath || "未找到 Java 可执行文件") : "未找到 Java 可执行文件"
                                                    font.pixelSize: 12; color: "#b0b8c8"; elide: Text.ElideMiddle; width: parent.width - 90 }
                                                Rectangle { anchors.right: parent.right; anchors.rightMargin: 6; anchors.verticalCenter: parent.verticalCenter
                                                    width: 90; height: 26; radius: 4; color: "#3a4eb8"
                                                    scale: autoDetectMa.pressed ? 0.9 : 1.0
                                                    Behavior on scale { NumberAnimation { duration: 100; easing.type: Easing.OutCubic } }
                                                    Text { anchors.centerIn: parent; text: "自动检测"; font.pixelSize: 11; color: "#e8ecf8" }
                                                    MouseArea { id: autoDetectMa; anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { if (backend) { backend.detectJava(); toastManager.show("正在检测 Java 环境...") } } }
                                                }
                                                Rectangle { anchors.right: parent.right; anchors.rightMargin: 102; anchors.verticalCenter: parent.verticalCenter
                                                    width: 50; height: 26; radius: 4; color: "transparent"; border.color: "#1a1f2e"
                                                    scale: browseJavaMa.pressed ? 0.9 : 1.0
                                                    Behavior on scale { NumberAnimation { duration: 100; easing.type: Easing.OutCubic } }
                                                    Text { anchors.centerIn: parent; text: "浏览"; font.pixelSize: 11; color: "#b0b8c8" }
                                                    MouseArea { id: browseJavaMa; anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { if (backend) { var ok = backend.pickJava(); if (ok) toastManager.show("已选择 Java 路径") } } }
                                                }
                                            }
                                            Text {
                                                text: {
                                                    if (!backend || !backend.javaVersion) return ""
                                                    var compat = backend.javaCompatibility
                                                    var base = backend.javaVersion
                                                    if (compat === "recommended") return base + "  推荐"
                                                    if (compat === "compatible") return base + "  兼容"
                                                    if (compat === "incompatible") return base + "  不兼容"

                                                    return base
                                                }
                                                font.pixelSize: 10
                                                color: {
                                                    var compat = backend ? backend.javaCompatibility : "unknown"
                                                    if (compat === "recommended") return "#4bc870"
                                                    if (compat === "compatible") return "#e0a040"
                                                    if (compat === "incompatible") return "#c05050"
                                                    return "#888ca0"
                                                }
                                            }

                                            // 鍐呭瓨设置
                                            Text { text: "内存分配"; font.pixelSize: 12; font.weight: Font.DemiBold; color: "#e4e8f2"; Layout.topMargin: 8 }
                                            RowLayout {
                                                Layout.fillWidth: true; spacing: 12
                                                ColumnLayout { Layout.fillWidth: true; spacing: 4
                                                    Text { text: "最大内存 " + (backend ? backend.maxMemoryMb + " MB" : "-"); font.pixelSize: 12; color: "#d4dcf0" }
                                                    Slider {
                                                        id: maxMemSlider; Layout.fillWidth: true
                                                        from: 512; to: 16384; stepSize: 512
                                                        value: backend ? backend.maxMemoryMb : 2048
                                                        enabled: backend ? !backend.autoMemoryEnabled : true
                                                        onMoved: { if (backend) backend.setMaxMemory(value) }
                                                    }
                                                }
                                                Text { text: "最小" + (backend ? backend.minMemoryMb + " MB" : "-"); font.pixelSize: 11; color: "#989cb0" }
                                            }
                                            Text { text: backend ? (JSON.stringify(backend.systemMemoryInfo) || "") : ""; font.pixelSize: 9; color: "#a8b0c0" }

                                            // 自动内存
                                            RowLayout {
                                                Text { text: "自动内存"; font.pixelSize: 12; color: "#d4dcf0" }
                                                Item { Layout.fillWidth: true }
                                                Switch {
                                                    checked: backend ? backend.autoMemoryEnabled : true
                                                    onToggled: { if (backend) backend.setAutoMemoryEnabled(checked) }
                                                }
                                            }

                                            // JVM 参数
                                            Text { text: "JVM 参数"; font.pixelSize: 12; font.weight: Font.DemiBold; color: "#e4e8f2"; Layout.topMargin: 8 }
                                            Rectangle { Layout.fillWidth: true; height: 60; radius: 6; color: "#0d1018"; border.color: "#1a1f2e"
                                                TextInput {
                                                    id: jvmArgsInput; anchors.fill: parent; anchors.margins: 8
                                                    color: "#b0b8c8"; font.pixelSize: 11; font.family: "Consolas, monospace"
                                                    text: backend ? backend.jvmArgs : ""
                                                    onEditingFinished: { if (backend) backend.setJvmArgs(text) }
                                                }
                                            }

                                            // GC 预设
                                            Text { text: "GC 预设"; font.pixelSize: 10; color: "#a8b0c0" }
                                            Flow {
                                                Layout.fillWidth: true; spacing: 8
                                                Repeater {
                                                    model: [
                                                        { label: "G1GC 平衡", args: "-XX:+UseG1GC -XX:G1NewSizePercent=20 -XX:MaxGCPauseMillis=50" },
                                                        { label: "ZGC 低延迟", args: "-XX:+UseZGC -XX:+UnlockExperimentalVMOptions" },
                                                        { label: "Shenandoah", args: "-XX:+UseShenandoahGC" },
                                                        { label: "Parallel", args: "-XX:+UseParallelGC" },
                                                        { label: "Serial", args: "-XX:+UseSerialGC" },
                                                        { label: "清空自定", args: "" }
                                                    ]
                                                    Rectangle { width: 90; height: 26; radius: 4; color: presetHover.hovered ? "#1a2848" : "#0d1018"; border.color: "#1a1f2e"
                                                        Text { anchors.centerIn: parent; text: modelData.label; font.pixelSize: 11; color: "#b0b8c8" }
                                                        HoverHandler { id: presetHover }
                                                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                            onClicked: {
                                                                jvmArgsInput.text = modelData.args
                                                                if (backend) {
                                                                    console.log("[jvm] GC preset:", modelData.label, modelData.args)
                                                                    backend.setJvmArgs(modelData.args)
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }

                                            // 版本隔离
                                            Text { text: "版本隔离"; font.pixelSize: 12; font.weight: Font.DemiBold; color: "#e4e8f2"; Layout.topMargin: 8 }
                                            RowLayout {
                                                Text { text: "版本隔离（模组配置/存档独立）"; font.pixelSize: 12; color: "#d4dcf0" }
                                                Item { Layout.fillWidth: true }
                                                Switch {
                                                    checked: backend ? backend.isolationEnabled : false
                                                    onToggled: { if (backend) backend.setIsolationEnabled(checked) }
                                                }
                                            }

                                            // 启动后关闭启动器
                                            RowLayout {
                                                Text { text: "启动后关闭启动器"; font.pixelSize: 12; color: "#d4dcf0" }
                                                Item { Layout.fillWidth: true }
                                                Switch {
                                                    checked: backend ? backend.closeOnLaunch : false
                                                    onToggled: { if (backend) backend.setCloseOnLaunch(checked) }
                                                }
                                            }

                                            // 游戏附加参数
                                            Text { text: "游戏附加参数"; font.pixelSize: 12; font.weight: Font.DemiBold; color: "#e4e8f2"; Layout.topMargin: 10 }
                                            Text { text: "（示例 --width 1920 --height 1080 --fullscreen）"; font.pixelSize: 9; color: "#a8b0c0" }
                                            Rectangle { Layout.fillWidth: true; height: 44; radius: 6; color: "#0d1018"; border.color: "#1a1f2e"
                                                TextInput {
                                                    id: gameArgsInput; anchors.fill: parent; anchors.margins: 8
                                                    color: "#b0b8c8"; font.pixelSize: 11; font.family: "Consolas, monospace"
                                                    text: backend ? backend.gameArgs : ""
                                                    onEditingFinished: { if (backend) backend.setGameArgs(text) }
                                                }
                                            }
                                        }
                                    }
                                }

                                // Section 2: Mod 管理 ===
                                ColumnLayout {
                                    anchors.fill: parent; anchors.margins: 24; spacing: 8
                                    visible: settingsNav.currentIndex === 2

                                    Text { text: "Mod 管理"; font.pixelSize: 14; font.bold: true; color: "#e4e8f2" }
                                    Text { text: "管理已安装的 Mod，查看版本和兼容性。"; font.pixelSize: 11; color: "#9498a8" }

                                    // Refresh + search
                                    RowLayout {
                                        spacing: 8
                                        Rectangle {
                                            width: 80; height: 28; radius: 4; color: "#3a4eb8"
                                            scale: modRefreshMa.pressed ? 0.9 : 1.0
                                            Behavior on scale { NumberAnimation { duration: 100; easing.type: Easing.OutCubic } }
                                            Text { anchors.centerIn: parent; text: "刷新列表"; font.pixelSize: 11; color: "#e8ecf8" }
                                            MouseArea { id: modRefreshMa; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                onClicked: { modListModel.clear(); if (backend) { var m = backend.listMods(); for (var i = 0; i < m.length; i++) modListModel.append(m[i]); toastManager.show("Mod 列表已刷新") } }
                                            }
                                        }
                                        Rectangle {
                                            Layout.fillWidth: true; height: 28; radius: 4; color: "#0d1018"; border.color: modSearchField.activeFocus ? "#5068c8" : "#1a1f2e"
                                            TextInput {
                                                id: modSearchField; anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 10
                                                color: "#e4e8f2"; font.pixelSize: 12; verticalAlignment: TextInput.AlignVCenter
                                                onTextChanged: {
                                                    modListModel.clear()
                                                    if (backend) {
                                                        var allMods = backend.listMods()
                                                        var query = text.toLowerCase()
                                                        for (var i = 0; i < allMods.length; i++) {
                                                            if (!query || allMods[i].name.toLowerCase().indexOf(query) >= 0)
                                                                modListModel.append(allMods[i])
                                                        }
                                                    }
                                                }
                                                Text {
                                                    anchors.left: parent.left; anchors.leftMargin: 10
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    text: "搜索 Mod..."; color: "#9ca0b4"; font.pixelSize: 12
                                                    visible: !modSearchField.text
                                                }
                                            }
                                        }
                                    }

                                    // Mod list
                                    ListView {
                                        id: modListView
                                        Layout.fillWidth: true; Layout.fillHeight: true
                                        model: ListModel { id: modListModel }
                                        clip: true; spacing: 4
                                        delegate: Rectangle {
                                            width: modListView.width; height: 36; radius: 4; color: modRowHover.hovered ? "#11141c" : "transparent"
                                            RowLayout {
                                                anchors.fill: parent; anchors.margins: 10; spacing: 10
                                                Text { text: name; font.pixelSize: 12; color: "#d4d8e8"; Layout.fillWidth: true; elide: Text.ElideRight }
                                                Text { text: sizeDisplay; font.pixelSize: 10; color: "#9498a8" }
                                                Rectangle { width: 60; height: 24; radius: 3; color: delBtnHover.hovered ? "#6b2020" : "transparent"; border.color: "#4a2828"
                                                    Text { anchors.centerIn: parent; text: "删除"; font.pixelSize: 10; color: "#c05050" }
                                                    MouseArea { id: delBtnHover; anchors.fill: parent; cursorShape: Qt.PointingHandCursor; hoverEnabled: true
                                                        onClicked: { if (backend) { backend.deleteMod(name); modListModel.remove(index); toastManager.show("已删除 Mod: " + name) } }
                                                    }
                                                }
                                            }
                                            MouseArea { id: modRowHover; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton }
                                        }

                                        Component.onCompleted: {
                                            if (backend) { var m = backend.listMods(); for (var i = 0; i < m.length; i++) modListModel.append(m[i]) }
                                        }
                                    }
                                }

                                // Section 3: 资源包管理 ===
                                ColumnLayout {
                                    anchors.fill: parent; anchors.margins: 24; spacing: 8
                                    visible: settingsNav.currentIndex === 3

                                    Text { text: "资源包管理"; font.pixelSize: 14; font.bold: true; color: "#e4e8f2" }
                                    Text { text: "管理已安装的资源包和材质包"; font.pixelSize: 11; color: "#9498a8" }

                                    RowLayout {
                                        spacing: 8
                                        Rectangle { height: 28; radius: 4; color: "#3a4eb8"; implicitWidth: rpRefreshText.implicitWidth + 20
                                            scale: rpRefreshMa.pressed ? 0.9 : 1.0
                                            Behavior on scale { NumberAnimation { duration: 100; easing.type: Easing.OutCubic } }
                                            Text { id: rpRefreshText; anchors.centerIn: parent; text: "刷新列表"; font.pixelSize: 11; color: "#e8ecf8" }
                                            MouseArea { id: rpRefreshMa; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                onClicked: { rpListModel.clear(); if (backend) { var p = backend.listResourcePacks(); for (var i = 0; i < p.length; i++) rpListModel.append(p[i]); toastManager.show("资源包列表已刷新") } }
                                            }
                                        }
                                        Rectangle { height: 28; radius: 4; color: "transparent"; border.color: "#1a1f2e"; implicitWidth: rpOpenText.implicitWidth + 20
                                            Text { id: rpOpenText; anchors.centerIn: parent; text: "打开文件夹"; font.pixelSize: 11; color: "#b0b8c8" }
                                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { if (backend) { backend.openResourcePacksFolder(); toastManager.show("已打开资源包文件夹") } } }
                                        }
                                    }

                                    ListView {
                                        id: rpListView
                                        Layout.fillWidth: true; Layout.fillHeight: true
                                        model: ListModel { id: rpListModel }
                                        clip: true; spacing: 4
                                        delegate: Rectangle {
                                            width: rpListView.width; height: 36; radius: 4; color: rpRowHover.hovered ? "#11141c" : "transparent"
                                            RowLayout {
                                                anchors.fill: parent; anchors.margins: 10; spacing: 10
                                                Rectangle { width: 20; height: 20; radius: 3; color: isDir ? "#5060a0" : "#3a6040"
                                                    Text { anchors.centerIn: parent; text: isDir ? "文件夹" : "文件"; font.pixelSize: 10 }
                                                }
                                                Text { text: name; font.pixelSize: 12; color: "#d4d8e8"; Layout.fillWidth: true; elide: Text.ElideRight }
                                                Text { text: sizeDisplay; font.pixelSize: 10; color: "#9498a8" }
                                                Rectangle { width: 60; height: 24; radius: 3; color: "transparent"; border.color: "#4a2828"
                                                    Text { anchors.centerIn: parent; text: "删除"; font.pixelSize: 10; color: "#c05050" }
                                                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                        onClicked: { if (backend) { backend.deleteResourcePack(name); rpListModel.remove(index); toastManager.show("已删除资源包: " + name) } }
                                                    }
                                                }
                                            }
                                            MouseArea { id: rpRowHover; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton }
                                        }

                                        Component.onCompleted: {
                                            if (backend) { var p = backend.listResourcePacks(); for (var i = 0; i < p.length; i++) rpListModel.append(p[i]) }
                                        }
                                    }
                                }

                                // Section 4: 存档管理 ===
                                ColumnLayout {
                                    anchors.fill: parent; anchors.margins: 24; spacing: 8
                                    visible: settingsNav.currentIndex === 4

                                    Text { text: "存档管理"; font.pixelSize: 14; font.bold: true; color: "#e4e8f2" }
                                    Text { text: "管理已保存的世界存档，可备份或迁移存档文件。"; font.pixelSize: 11; color: "#9498a8" }

                                    Rectangle {
                                        width: 80; height: 28; radius: 4; color: "#3a4eb8"
                                        scale: saveRefreshMa.pressed ? 0.9 : 1.0
                                        Behavior on scale { NumberAnimation { duration: 100; easing.type: Easing.OutCubic } }
                                        Text { anchors.centerIn: parent; text: "刷新列表"; font.pixelSize: 11; color: "#e8ecf8" }
                                        MouseArea { id: saveRefreshMa; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                            onClicked: { saveListModel.clear(); if (backend) { var s = backend.listSaves(); for (var i = 0; i < s.length; i++) saveListModel.append(s[i]); toastManager.show("存档列表已刷新") } }
                                        }
                                    }

                                    ListView {
                                        id: saveListView
                                        Layout.fillWidth: true; Layout.fillHeight: true
                                        model: ListModel { id: saveListModel }
                                        clip: true; spacing: 4
                                        delegate: Rectangle {
                                            width: saveListView.width; height: 36; radius: 4; color: saveRowHover.hovered ? "#11141c" : "transparent"
                                            RowLayout {
                                                anchors.fill: parent; anchors.margins: 10; spacing: 10
                                                Text { text: name; font.pixelSize: 12; color: "#d4d8e8"; Layout.fillWidth: true; elide: Text.ElideRight }
                                                Text { text: sizeDisplay; font.pixelSize: 10; color: "#9498a8" }
                                                Rectangle { width: 60; height: 24; radius: 3; color: "transparent"; border.color: "#4a2828"
                                                    Text { anchors.centerIn: parent; text: "删除"; font.pixelSize: 10; color: "#c05050" }
                                                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                        onClicked: { if (backend) { backend.deleteSave(name); saveListModel.remove(index); toastManager.show("已删除存档: " + name) } }
                                                    }
                                                }
                                            }
                                            MouseArea { id: saveRowHover; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton }
                                        }

                                        Component.onCompleted: {
                                            if (backend) { var s = backend.listSaves(); for (var i = 0; i < s.length; i++) saveListModel.append(s[i]) }
                                        }
                                    }
                                }

                                // Section 5: 工具与维护
                                ColumnLayout {
                                    anchors.fill: parent; anchors.margins: 24; spacing: 12
                                    visible: settingsNav.currentIndex === 5

                                    Text { text: "游戏完整性校验"; font.pixelSize: 14; font.bold: true; color: "#e4e8f2" }
                                    Text { text: "扫描选定版本的游戏文件完整性，检查损坏或缺失的文件。"; font.pixelSize: 11; color: "#9498a8"; wrapMode: Text.WordWrap; Layout.fillWidth: true }

                                    // Start button
                                    Rectangle {
                                        width: 140; height: 36; radius: 6
                                        color: versionSettingsOverlay._verifyRunning ? "#2a3040" : (verifyBtnMouse.containsMouse ? "#2563EB" : "#3a4eb8")
                                        scale: verifyBtnMouse.containsMouse && !versionSettingsOverlay._verifyRunning ? 1.04 : 1.0
                                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                                        Behavior on color { ColorAnimation { duration: 200 } }
                                        Text { anchors.centerIn: parent; text: versionSettingsOverlay._verifyRunning ? "校验中..." : "开始校验"; font.pixelSize: 12; color: "#e8ecf8" }

                                        MouseArea {
                                            id: verifyBtnMouse; anchors.fill: parent; cursorShape: Qt.PointingHandCursor; hoverEnabled: true
                                            enabled: !versionSettingsOverlay._verifyRunning
                                            onClicked: { if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }; if (backend) backend.verifyVersion(currentSelectedVersion) }
                                        }
                                    }

                                    // Progress bar
                                    ColumnLayout {
                                        Layout.fillWidth: true; spacing: 6
                                        visible: versionSettingsOverlay._verifyRunning || versionSettingsOverlay._verifyProgressTotal > 0
                                        Rectangle {
                                            Layout.fillWidth: true; height: 8; radius: 4; color: "#1a1e28"
                                            Rectangle {
                                                height: 8; radius: 4
                                                width: versionSettingsOverlay._verifyProgressTotal > 0 ? parent.width * (versionSettingsOverlay._verifyProgressDone / versionSettingsOverlay._verifyProgressTotal) : 0
                                                color: versionSettingsOverlay._verifyRunning ? "#6080e8" : (versionSettingsOverlay._verifyProgressDone === versionSettingsOverlay._verifyProgressTotal ? "#4bc870" : "#c05050")
                                                Behavior on width { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                                            }
                                        }
                                        Text {
                                            text: {
                                                if (versionSettingsOverlay._verifyRunning && versionSettingsOverlay._verifyProgressTotal > 0) {
                                                    var pct = Math.round(versionSettingsOverlay._verifyProgressDone / versionSettingsOverlay._verifyProgressTotal * 100)
                                                    return "校验中 " + versionSettingsOverlay._verifyProgressDone + "/" + versionSettingsOverlay._verifyProgressTotal + " (" + pct + "%)"
                                                }
                                                return versionSettingsOverlay._verifyRunning ? "校验中..." : ""
                                            }
                                            font.pixelSize: 11; color: "#989cb0"
                                        }
                                    }

                                    Item { height: 12; width: 1 }

                                    // Red failure notification
                                    Rectangle {
                                        Layout.fillWidth: true
                                        height: 40; radius: 6
                                        color: "#2A1518"
                                        border.color: "#804040"
                                        border.width: 1
                                        visible: versionSettingsOverlay._verifyHasFailed && versionSettingsOverlay._verifyFailedFiles.length > 0
                                        RowLayout {
                                            anchors.centerIn: parent
                                            spacing: 8
                                            Text {
                                                text: "❌ 检测到 " + versionSettingsOverlay._verifyFailedFiles.length + " 个文件异常"
                                                font.pixelSize: 12; color: "#ff8080"
                                            }
                                        }
                                    }

                                    // Action buttons
                                    RowLayout {
                                        spacing: 10
                                        visible: versionSettingsOverlay._verifyHasFailed && versionSettingsOverlay._verifyFailedFiles.length > 0

                                        // Repair button (hollow orange)
                                        Rectangle {
                                            width: 140; height: 36; radius: 6
                                            color: "transparent"
                                            border.color: repairBtnHover.hovered ? "#ff8c42" : "#c06420"
                                            border.width: 1.5
                                            Text { anchors.centerIn: parent; text: "🔧 一键修复"; font.pixelSize: 12; color: repairBtnHover.hovered ? "#ff8c42" : "#e08050" }
                                            HoverHandler { id: repairBtnHover }
                                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }
                                                    if (backend) {
                                                        backend.repairVersion(currentSelectedVersion)
                                                        versionSettingsOverlay._verifyHasFailed = false
                                                        versionSettingsOverlay._verifyFailedFiles = []
                                                    }
                                                }
                                            }
                                        }

                                        // View report button
                                        Rectangle {
                                            width: 140; height: 36; radius: 6
                                            color: "transparent"
                                            border.color: reportBtnHover.hovered ? "#ff8080" : "#804040"
                                            border.width: 1.5
                                            Text { anchors.centerIn: parent; text: "📋 查看异常详情"; font.pixelSize: 12; color: reportBtnHover.hovered ? "#ff8080" : "#e07070" }
                                            HoverHandler { id: reportBtnHover }
                                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    if (backend) backend.openVerifyReport()
                                                }
                                            }
                                        }
                                    }

                                    Item { height: 12; width: 1 }

                                    // Version tools
                                    Text { text: "版本工具"; font.pixelSize: 10; color: "#9ca0b4"; font.letterSpacing: 1.5 }
                                    Flow {
                                        Layout.fillWidth: true; spacing: 8

                                        // Clone
                                        Rectangle { width: 110; height: 32; radius: 5; color: cloneHover.hovered ? "#1a2848" : "#0d1018"; border.color: "#1a1f2e"
                                            Text { anchors.centerIn: parent; text: "克隆版本"; font.pixelSize: 11; color: "#d4dcf0" }
                                            HoverHandler { id: cloneHover }
                                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }

                                                    if (backend) backend.cloneVersion(currentSelectedVersion)

                                                    toastManager.show("已克隆版本:  " + currentSelectedVersion) }


                                            }
                                        }

                                        // Rename
                                        Rectangle { width: 110; height: 32; radius: 5; color: renameHover.hovered ? "#1a2848" : "#0d1018"; border.color: "#1a1f2e"
                                            Text { anchors.centerIn: parent; text: "重命名版本"; font.pixelSize: 11; color: "#d4dcf0" }
                                            HoverHandler { id: renameHover }
                                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }

                                                    if (backend) backend.renameVersion(currentSelectedVersion)

                                                }
                                            }
                                        }

                                        // Migrate
                                        Rectangle { width: 110; height: 32; radius: 5; color: migrateHover.hovered ? "#1a2848" : "#0d1018"; border.color: "#1a1f2e"
                                            Text { anchors.centerIn: parent; text: "迁移目录"; font.pixelSize: 11; color: "#d4dcf0" }
                                            HoverHandler { id: migrateHover }
                                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    if (!currentSelectedVersion) return
                                                    if (backend) backend.migrateVersion(currentSelectedVersion)
                                                }
                                            }
                                        }
                                    }

                                    Item { height: 12; width: 1 }

                                    // Delete version
                                    Text { text: "危险操作"; font.pixelSize: 10; color: "#c05050"; font.letterSpacing: 1.5 }
                                    Rectangle {
                                        Layout.fillWidth: true; height: 36; radius: 6; color: "transparent"; border.color: "#2a1f24"
                                        scale: delVerHover.containsMouse ? 1.02 : 1.0
                                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                                        Text { anchors.left: parent.left; anchors.leftMargin: 14; anchors.verticalCenter: parent.verticalCenter; text: "删除此版本"; font.pixelSize: 13; color: delVerHover.containsMouse ? "#f05050" : "#c05050" }
                                        MouseArea {
                                            id: delVerHover
                                            anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }
                                                confirmDialog.title = "删除版本"
                                                confirmDialog.message = "确认要删除版本 " + currentSelectedVersion + " 吗？\n此操作不可撤销，版本的所有文件将被删除。"

                                                confirmDialog.onAccept = function() {
                                                    showVersionSettings = false
                                                    backend.deleteVersion(currentSelectedVersion)
                                                }
                                                confirmDialog.visible = true
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }



                // ═══ Kill button overlay (bottom-right of pageContainer) ═══
                Rectangle {
                    id: killButton
                    width: 48; height: 48; radius: 24
                    anchors.right: parent.right; anchors.rightMargin: 16
                    anchors.bottom: parent.bottom; anchors.bottomMargin: 16
                    z: 200; color: killMouse.containsMouse ? "#e06060" : "#c05050"
                    opacity: backend ? (backend.isRunning ? 1 : 0) : 0
                    scale: killMouse.containsMouse ? 1.1 : (backend ? (backend.isRunning ? 1 : 0.3) : 0.3)
                    visible: opacity > 0
                    Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
                    Behavior on scale { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                    Behavior on color { ColorAnimation { duration: 150 } }
                    Text { anchors.centerIn: parent; text: "\u25A0"; color: "#e8ecf8"; font.pixelSize: 16 }
                    MouseArea { id: killMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { console.log("[main] killButton clicked"); if (backend) { killPressAnim.start(); killDelayTimer.start() } } }

                    Timer {
                        id: killDelayTimer
                        interval: 300
                        onTriggered: { if (backend) backend.killMinecraft() }
                    }

                    SequentialAnimation {
                        id: killPressAnim
                        NumberAnimation { target: killButton; property: "scale"; to: 0.75; duration: 80; easing.type: Easing.OutCubic }
                        NumberAnimation { target: killButton; property: "scale"; to: 1.0; duration: 200; easing.type: Easing.OutBack }
                    }
                }
            }
        }
    }

    Loader {
        id: launchOverlayLoader; anchors.fill: parent; z: 20
        source: "LaunchOverlay.qml"
        active: true  // Always loaded for smooth hide animation
    }

    // ════════════════════════════════════════════
    //  Download animation — flying ball ═══
    // ════════════════════════════════════════════
    Rectangle {
        id: flyBall
        z: 500
        width: 12; height: 12; radius: 6
        color: "#6080e8"
        visible: false
        opacity: 0
    }

    ParallelAnimation {
        id: flyBallAnim
        property real startX: 0
        property real startY: 0
        property real endX: 0
        property real endY: 0
        NumberAnimation { target: flyBall; property: "x"; from: flyBallAnim.startX; to: flyBallAnim.endX; duration: 400; easing.type: Easing.InCubic }
        NumberAnimation { target: flyBall; property: "y"; from: flyBallAnim.startY; to: flyBallAnim.endY; duration: 400; easing.type: Easing.InCubic }
        onStopped: {
            // When fly reaches target, bounce then fade
            flyBallBounce.restart()
            flyBallFade.start()
        }
    }

    SequentialAnimation {
        id: flyBallBounce
        NumberAnimation { target: flyBall; property: "scale"; from: 1.0; to: 1.7; duration: 140; easing.type: Easing.OutBack }
        NumberAnimation { target: flyBall; property: "scale"; from: 1.7; to: 0.8; duration: 100; easing.type: Easing.InCubic }
        NumberAnimation { target: flyBall; property: "scale"; from: 0.8; to: 1.0; duration: 120; easing.type: Easing.OutCubic }
    }

    NumberAnimation {
        id: flyBallFade
        target: flyBall; property: "opacity"; to: 0; duration: 350; easing.type: Easing.OutCubic
        onStopped: { flyBall.visible = false }
    }

    // Nav item bounce overlay (inside sidebar)
    Rectangle {
        id: navBounceOverlay
        z: 200
        visible: false
        width: 184; height: 40; radius: 6
        color: "#306080e8"
        x: 8; y: 0
        opacity: 0
        
        NumberAnimation on opacity {
            id: navOverlayFade
            from: 0.3; to: 0; duration: 420; easing.type: Easing.OutCubic
            onStopped: { navBounceOverlay.visible = false }
        }
        NumberAnimation on scale {
            id: navOverlayScale
            from: 0.9; to: 1.06; duration: 260; easing.type: Easing.OutBack
        }
    }

    function animateDownloadBall(sourceX, sourceY) {
        // Find the download progress nav item (last in navModel)
        var idx = downloadNavVisible ? navModel.count - 1 : -1
        if (idx < 0) { return }

        var targetX, targetY
        var delegate = navRepeater.itemAt(idx)
        if (delegate) {
            var pt = delegate.mapToItem(null, delegate.width / 2, delegate.height / 2)
            targetX = pt.x; targetY = pt.y
        } else {
            targetX = 108
            targetY = 46 + 8 + 48 + idx * 46 + 22
        }

        // Diagnostic: write trace to file
        if (backend) backend.logMessage("[flyBall] (" + sourceX.toFixed(0) + "," + sourceY.toFixed(0) + ") → (" + targetX.toFixed(0) + "," + targetY.toFixed(0) + ")")

        // Position and show ball
        flyBall.x = sourceX
        flyBall.y = sourceY
        flyBall.opacity = 1.0
        flyBall.scale = 1.0
        flyBall.visible = true

        // Set anim targets and fly
        flyBallAnim.startX = sourceX
        flyBallAnim.startY = sourceY
        flyBallAnim.endX = targetX
        flyBallAnim.endY = targetY
        flyBallAnim.restart()

        // Nav overlay bounce
        navBounceOverlay.y = targetY - 20
        navBounceOverlay.opacity = 0.3
        navBounceOverlay.scale = 0.9
        navBounceOverlay.visible = true
        navOverlayFade.restart()
        navOverlayScale.restart()
    }

    // ═══ Kill button (inside pageContainer — see pageContainer above) ═══
    // Reference to killButton via id still works from Window scope due to QML id resolution

    Connections {
        target: backend; enabled: backend !== null
        function onLogMessage(msg) { console.log("[backend]", msg) }
        function onMinecraftStarted() {
            killButton.visible = true
            console.log("[main] minecraftStarted → killButton shown")
        }
        function onMinecraftStopped() {
            killButton.visible = false
            console.log("[main] minecraftStopped → killButton hidden")
        }
        function onRunningCountChanged() {
            appWindow.runningListModel = backend ? backend.runningGames() : []
            console.log("[main] runningCountChanged → list refreshed: " + appWindow.runningListModel.length + " games")
        }
    }

    // Confirm Dialog ===
    Rectangle {
        id: confirmDialog; z: 400
        anchors.centerIn: parent; width: 360; height: 180; radius: 10
        color: "#141820"; border.color: "#2a1f24"; border.width: 1
        opacity: confirmDialog.visible ? 1 : 0
        visible: false
        Behavior on opacity { NumberAnimation { duration: 150 } }
        property string title: ""
        property string message: ""
        property var onAccept: null

        ColumnLayout {
            anchors.fill: parent; anchors.margins: 20; spacing: 12
            Text { text: confirmDialog.title; font.pixelSize: 15; font.weight: Font.Bold; color: "#e4e8f2" }
            Text { text: confirmDialog.message; font.pixelSize: 12; color: "#b0b8c8"; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            Item { Layout.fillHeight: true }
            RowLayout {
                Layout.alignment: Qt.AlignRight; spacing: 10
                Rectangle { width: 80; height: 32; radius: 5; color: "transparent"; border.color: "#2a2e3c"
                    scale: cancelDlgMa.pressed ? 0.9 : 1.0
                    Behavior on scale { NumberAnimation { duration: 100; easing.type: Easing.OutCubic } }
                    Text { anchors.centerIn: parent; text: "取消"; font.pixelSize: 12; color: "#b0b8c8" }
                    MouseArea { id: cancelDlgMa; anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: confirmDialog.visible = false }
                }
                Rectangle { width: 80; height: 32; radius: 5; color: "#c05050"
                    scale: confirmDlgMa.pressed ? 0.9 : 1.0
                    Behavior on scale { NumberAnimation { duration: 100; easing.type: Easing.OutCubic } }
                    Text { anchors.centerIn: parent; text: "确认"; font.pixelSize: 12; color: "#e8ecf8" }
                    MouseArea { id: confirmDlgMa; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: { confirmDialog.visible = false; if (confirmDialog.onAccept) confirmDialog.onAccept() }
                    }
                }
            }
        }
    }

    // Dim overlay behind dialog
    Rectangle {
        anchors.fill: parent; z: 399; color: "#000000"
        opacity: confirmDialog.visible ? 0.5 : 0
        visible: confirmDialog.visible
        Behavior on opacity { NumberAnimation { duration: 150 } }
        MouseArea { anchors.fill: parent; onClicked: { confirmDialog.visible = false } }
}
}

    // ═══ Toast notification system ═══
    ToastManager {
        id: toastManager
        anchors.fill: parent
        z: 9999
    }
    } // ── end rounded Rectangle
}
