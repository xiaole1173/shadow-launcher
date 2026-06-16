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
    color: "#0c0f16"

    property int navListIndex: 0
    property string currentSelectedVersion: backend ? backend.selectedVersion : ""
    property int loginMode: backend ? backend.lastLoginMode : 0
    property bool showVersionSelect: false
    property bool showVersionSettings: false
    property var offlineHistory: []
    // Toast disabled - see bottom of file for TODO
    function showToast(msg) { /* TODO: fix black bar issue */ }

    Component.onCompleted: {
        if (backend) {
            backend.refreshInstalled()
            backend.refreshVersionDetails()
        }
    }

    function addOfflineHistory(name) {
        var h = offlineHistory.slice()
        h = h.filter(function(x) { return x !== name })
        h.unshift(name)
        if (h.length > 10) h = h.slice(0, 10)
        offlineHistory = h
    }
    function switchPage(index) { navListIndex = index; showVersionSelect = false; showVersionSettings = false }

    // 鈺愨晲鈺?Download progress nav item management 鈺愨晲鈺?
    property bool downloadNavVisible: false

    function showDownloadNav() {
        if (!downloadNavVisible) {
            downloadNavVisible = true
            navModel.append({ label: "涓嬭浇杩涘害", pageKey: "download_progress" })
            switchPage(navModel.count - 1)  // auto-switch to the new page
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
            // Or optionally auto-hide:
            // hideDownloadNav()
        }
    }

    ColumnLayout {
        anchors.fill: parent; spacing: 0

        Rectangle {
            Layout.fillWidth: true; height: 36; color: "#0a0c12"
            MouseArea {
                anchors.fill: parent
                property point lastPos: Qt.point(0, 0)
                onPressed: lastPos = Qt.point(mouse.x, mouse.y)
                onPositionChanged: { appWindow.x += mouse.x - lastPos.x; appWindow.y += mouse.y - lastPos.y }
            }
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 12
                Text { text: "Shadow Launcher"; color: "#606478"; font.pixelSize: 12; Layout.alignment: Qt.AlignVCenter }
                Item { Layout.fillWidth: true }
                Rectangle {
                    width: 36; height: 24; color: minBtn.containsMouse ? "#1a1f2e" : "transparent"
                    Text { anchors.centerIn: parent; text: "\u2014"; color: "#606478"; font.pixelSize: 14 }
                    MouseArea { id: minBtn; anchors.fill: parent; hoverEnabled: true; onClicked: appWindow.showMinimized() }
                }
                Rectangle {
                    width: 36; height: 24; color: closeBtn.containsMouse ? "#c05050" : "transparent"
                    Text { anchors.centerIn: parent; text: "\u2715"; color: closeBtn.containsMouse ? "#e8ecf8" : "#606478"; font.pixelSize: 12 }
                    MouseArea { id: closeBtn; anchors.fill: parent; hoverEnabled: true; onClicked: appWindow.close() }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true
            Layout.margins: 8; spacing: 8

            Rectangle {
                Layout.preferredWidth: 200; Layout.fillHeight: true
                color: "#0a0c12"; radius: 6
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 8; spacing: 2
                    Text { Layout.topMargin: 8; Layout.bottomMargin: 20; Layout.leftMargin: 16; text: "SHADOW"; font.pixelSize: 16; font.bold: true; color: "#d0d4e0" }

                    // Navigation model
                    ListModel {
                        id: navModel
                        ListElement { label: "鍚姩"; pageKey: "home" }
                        ListElement { label: "涓嬭浇"; pageKey: "download" }
                        ListElement { label: "璁剧疆"; pageKey: "settings" }
                    }

                    Repeater {
                        model: navModel
                        Item {
                            width: parent ? parent.width : 180; Layout.fillWidth: true; height: 44
                            Rectangle { anchors.fill: parent; color: navMouse.containsMouse ? "#11141c" : "transparent" }
                            Rectangle { anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 2; color: navListIndex === index ? "#6080e8" : "transparent" }
                            Text { anchors.left: parent.left; anchors.leftMargin: 24; anchors.verticalCenter: parent.verticalCenter; text: model.label || modelData; font.pixelSize: 13; color: navListIndex === index ? "#d0d4e0" : "#606478" }
                            MouseArea { id: navMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: switchPage(index) }
                        }
                    }
                    Item { Layout.fillHeight: true }
                    Text { Layout.alignment: Qt.AlignHCenter; text: "v0.3.0"; font.pixelSize: 10; color: "#303440" }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0

                Rectangle {
                    id: pageContainer
                    Layout.fillWidth: true; Layout.fillHeight: true; color: "transparent"

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
                                    Text { anchors.centerIn: parent; text: "姝ｇ増鐧诲綍"; font.pixelSize: 13; color: loginMode === 0 ? "#d0d4e0" : "#606478"; font.weight: loginMode === 0 ? Font.DemiBold : Font.Normal }
                                    MouseArea { anchors.fill: parent; onClicked: { loginMode = 0; if (backend) backend.setLastLoginMode(0) } }
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
                                    Text { anchors.centerIn: parent; text: "绂荤嚎妯″紡"; font.pixelSize: 13; color: loginMode === 1 ? "#d0d4e0" : "#606478"; font.weight: loginMode === 1 ? Font.DemiBold : Font.Normal }
                                    MouseArea { anchors.fill: parent; onClicked: { loginMode = 1; if (backend) backend.setLastLoginMode(1) } }
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
                                    Text { anchors.centerIn: parent; text: "选择已登录账号"; color: "#606478"; font.pixelSize: 13 }
                                    MouseArea { anchors.fill: parent }
                                }
                                Rectangle {
                                    Layout.alignment: Qt.AlignHCenter; width: 160; height: 36; radius: 7
                                    color: "#2a3878"
                                    Text { anchors.centerIn: parent; text: "Microsoft 鐧诲綍"; color: "#d0d4e0"; font.pixelSize: 13; font.weight: Font.DemiBold }
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
                                        color: "#d0d4e0"; font.pixelSize: 13
                                        verticalAlignment: TextInput.AlignVCenter
                                    }
                                    Text {
                                        anchors.left: parent.left; anchors.leftMargin: 12
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: "杈撳叆鐢ㄦ埛鍚?.."; color: "#505468"
                                        font.pixelSize: 13
                                        visible: !offlineNameInput.text
                                    }
                                }
                                Rectangle {
                                    Layout.alignment: Qt.AlignHCenter; width: 100; height: 36; radius: 7
                                    color: "#3a4eb8"
                                    Text { anchors.centerIn: parent; text: "鐧诲綍"; color: "#e8ecf8"; font.pixelSize: 13; font.weight: Font.DemiBold }
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
                                    Text { text: homePage.displayName; font.pixelSize: 16; font.bold: true; color: "#d0d4e0" }
                                }

                                // UUID
                                Text {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: backend ? (backend.accountUuid || "") : ""
                                    font.pixelSize: 10; color: "#505468"
                                    font.family: "Consolas, monospace"
                                }

                                // Login type + Logout
                                RowLayout {
                                    Layout.alignment: Qt.AlignHCenter; spacing: 12
                                    Text { text: loginMode === 1 ? "绂荤嚎妯″紡" : "姝ｇ増鐧诲綍"; font.pixelSize: 11; color: "#505468" }
                                    Rectangle {
                                        width: 60; height: 24; radius: 4
                                        color: "transparent"; border.color: "#2a1f24"
                                        Text { anchors.centerIn: parent; text: "鐧诲嚭"; font.pixelSize: 11; color: "#c05050" }
                                        MouseArea {
                                            anchors.fill: parent
                                            onClicked: { if (backend) backend.logout() }
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
                                        Text { text: currentSelectedVersion || "鏈€夋嫨鐗堟湰"; font.pixelSize: 16; font.weight: Font.Bold; color: currentSelectedVersion ? "#8aa8f0" : "#404458" }
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true; height: 44; radius: 8
                                    color: "#3a4eb8"
                                    Text { anchors.centerIn: parent; text: "鍚?鍔?娓?鎴?"; font.pixelSize: 15; font.weight: Font.Bold; color: "#e8ecf8" }
                                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            if (!backend) return
                                            if (!currentSelectedVersion) {
                                                showToast("请先登录账号")
                                                return
                                            }
                                            if (!backend.username) {
                                                showToast("璇峰厛鐧诲綍璐﹀彿")
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
                                            Text { text: "鐗堟湰閫夋嫨"; font.pixelSize: 13; font.weight: Font.Medium; color: "#9094a8" }
                                        }
                                    }
                                    Rectangle {
                                        Layout.fillWidth: true; height: 36; radius: 7; color: "#0e1118"; border.color: "#1e2230"
                                        MouseArea { anchors.fill: parent; onClicked: { showVersionSettings = true } }
                                        RowLayout {
                                            anchors.centerIn: parent; spacing: 6
                                            Rectangle { width: 8; height: 8; radius: 2; color: "transparent"; border.color: "#606478"; border.width: 1.5 }
                                            Text { text: "鐗堟湰璁剧疆"; font.pixelSize: 13; font.weight: Font.Medium; color: "#9094a8" }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // ========== DOWNLOAD & SETTINGS ==========
                    Rectangle { anchors.fill: parent; visible: navListIndex === 1; color: "#0c0f16"
                        Loader { anchors.fill: parent; active: navListIndex === 1; source: active ? "DownloadPage.qml" : "" } }
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
                        onVisibleChanged: { if (visible && backend) backend.refreshVersionDetails() }

                        Rectangle { x: 16; y: 16; height: 28; width: 80; radius: 5; color: "transparent"
                            Text { anchors.centerIn: parent; text: "\u2190 杩斿洖鍚姩"; color: "#505468"; font.pixelSize: 12 }
                            MouseArea { anchors.fill: parent; onClicked: { showVersionSelect = false } }
                        }
                        RowLayout {
                            anchors.fill: parent; anchors.margins: 16; anchors.topMargin: 52; spacing: 16
                            Rectangle {
                                Layout.preferredWidth: Math.min(220, parent.width * 0.35); Layout.fillHeight: true
                                color: "#11141c"; radius: 8; border.color: "#1a1e28"
                                ColumnLayout {
                                    anchors.fill: parent; anchors.margins: 14; spacing: 6
                                    Text { text: "娓告垙鏂囦欢澶?"; font.pixelSize: 10; color: "#404458"; font.letterSpacing: 1.5 }
                                    ListModel { id: gameDirModel }
                                    Component.onCompleted: {
                                        var dirs = backend ? backend.gameDirectories : []
                                        for (var d = 0; d < dirs.length; d++) {
                                            gameDirModel.append({ path: dirs[d], display: d === 0 ? ".minecraft锛堥粯璁わ級" : dirs[d] })
                                        }
                                    }
                                    Connections {
                                        target: backend; enabled: backend !== null
                                        function onGameDirsChanged() {
                                            gameDirModel.clear()
                                            var dirs = backend.gameDirectories
                                            for (var d = 0; d < dirs.length; d++) {
                                                gameDirModel.append({ path: dirs[d], display: d === 0 ? ".minecraft锛堥粯璁わ級" : dirs[d] })
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
                                                Text {
                                                    anchors.left: parent.left; anchors.leftMargin: 12
                                                    anchors.right: parent.right; anchors.rightMargin: 8
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    text: model.display
                                                    font.pixelSize: 12
                                                    color: versionSelectOverlay.activeGameDirIndex === index ? "#8aa8f0" : "#a0a5b8"
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
                                                        if (backend) { backend.setGameDir(index); backend.refreshInstalled(); backend.refreshGameDirInfo() }
                                                    }
                                                    onPressed: function(mouse) {
                                                        if (mouse.button === Qt.RightButton) {
                                                            if (index === 0) {
                                                                if (backend) backend.openGameDir(0)
                                                            } else {
                                                                confirmDialog.title = "绉婚櫎鐩綍"
                                                                confirmDialog.message = "鏄惁绉婚櫎鐩綍 芦 " + model.display + " 禄锛焅n\n锛堜笉浼氬垹闄ゆ湰鍦版枃浠讹級"
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
                                                Text { text: "鐗堟湰"; font.pixelSize: 10; color: "#505468"; Layout.preferredWidth: 40 }
                                                Text { text: backend ? (backend.gameDirInfo.versionCount || 0) : 0; font.pixelSize: 12; font.weight: Font.Medium; color: "#8aa8f0" }
                                            }
                                            RowLayout {
                                                Text { text: "妯＄粍"; font.pixelSize: 10; color: "#505468"; Layout.preferredWidth: 40 }
                                                Text { text: backend ? (backend.gameDirInfo.modCount || 0) : 0; font.pixelSize: 12; font.weight: Font.Medium; color: "#a0a5b8" }
                                            }
                                            RowLayout {
                                                Text { text: "鍗犵敤"; font.pixelSize: 10; color: "#505468"; Layout.preferredWidth: 40 }
                                                Text { text: backend ? (backend.gameDirInfo.sizeDisplay || "") : ""; font.pixelSize: 12; font.weight: Font.Medium; color: "#a0a5b8" }
                                            }
                                        }
                                    }

                                    Item { height: 4; width: 1 }
                                    Rectangle { Layout.fillWidth: true; height: 30; radius: 6; color: "transparent"; border.color: "#1e2230"; border.width: 1
                                        Text { anchors.centerIn: parent; text: "+ 添加文件夹"; font.pixelSize: 11; color: "#606478" }
                                        MouseArea {
                                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                            onClicked: { showToast("鍔熻兘寮€鍙戜腑锛岃鍓嶅線鏂囦欢澶规墜鍔ㄦ坊鍔?) }
                                        }
                                    }
                                    Rectangle { Layout.fillWidth: true; height: 30; radius: 6; color: "transparent"; border.color: "#1e2230"; border.width: 1
                                        Text { anchors.centerIn: parent; text: "瀵煎叆鏁村悎鍖?"; font.pixelSize: 11; color: "#606478" }
                                        MouseArea {
                                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                            onClicked: { showToast("瀵煎叆鏁村悎鍖呭姛鑳藉紑鍙戜腑") }
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
                                                if (!backend) return "纾佺洏淇℃伅涓嶅彲鐢?
                                                var pct = backend.diskPercent
                                                var free = backend.diskFree
                                                var status = pct > 95 ? "鍗遍櫓" : (pct > 80 ? "鍋忎綆" : "姝ｅ父")
                                                return "鍓╀綑 " + free + "  (" + status + ")"
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

                                    // Header row: title + search + sort
                                    RowLayout {
                                        Layout.fillWidth: true; spacing: 8
                                        Text { text: "宸插畨瑁呯増鏈?"; font.pixelSize: 10; color: "#404458"; font.letterSpacing: 1.5 }
                                        Rectangle {
                                            Layout.fillWidth: true; height: 28; radius: 4; color: "#0d1018"
                                            border.color: searchField.activeFocus ? "#5068c8" : "#1a1f2e"
                                            TextInput {
                                                id: searchField; anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 10
                                                color: "#d0d4e0"; font.pixelSize: 12
                                                verticalAlignment: TextInput.AlignVCenter
                                                Text {
                                                    anchors.left: parent.left; anchors.leftMargin: 10
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    text: "鎼滅储鐗堟湰..."; color: "#404458"; font.pixelSize: 12
                                                    visible: !searchField.text
                                                }
                                            }
                                        }
                                        // Install button
                                        Rectangle {
                                            width: 28; height: 28; radius: 4; color: installBtnHover.hovered ? "#2553a8" : "#3a4eb8"
                                            Text { anchors.centerIn: parent; text: "+"; font.pixelSize: 18; font.weight: Font.Bold; color: "#e8ecf8" }
                                            HoverHandler { id: installBtnHover }
                                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                onClicked: { showVersionSelect = false; navListIndex = 2 }  // Switch to download
                                            }
                                        }
                                        // Sort button
                                        Rectangle {
                                            id: sortBtn
                                            width: 70; height: 28; radius: 4; color: sortHover.hovered ? "#1a2848" : "#0d1018"
                                            border.color: sortHover.hovered ? "#5068c8" : "#1a1f2e"
                                            property int versionSortIndex: 0
                                            RowLayout {
                                                anchors.centerIn: parent; spacing: 4
                                                Text { text: sortBtn.versionSortIndex === 0 ? "鈫?鐗堟湰" : (sortBtn.versionSortIndex === 1 ? "鈫?鐗堟湰" : (sortBtn.versionSortIndex === 2 ? "鈫?澶у皬" : "鈫?妯＄粍")); font.pixelSize: 10; color: "#9094a8" }
                                            }
                                            HoverHandler { id: sortHover }
                                            MouseArea {
                                                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
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
                                            property int loaderFilterIndex: 0  // 0=鍏ㄩ儴 1=鍘熺増 2=Forge 3=Fabric 4=NeoForge 5=Quilt
                                            property var loaderLabels: ["鍏ㄩ儴绫诲瀷", "鍘熺増", "Forge", "Fabric", "NeoForge", "Quilt"]
                                            RowLayout {
                                                anchors.centerIn: parent; spacing: 4
                                                Text { text: loaderFilter.loaderLabels[loaderFilter.loaderFilterIndex]; font.pixelSize: 10; color: "#9094a8" }
                                            }
                                            HoverHandler { id: loaderFiltHover }
                                            MouseArea {
                                                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
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
                                            if (li === 0) return true  // 鍏ㄩ儴
                                            var lt = (d.loaderType || "鍘熺増").toLowerCase()
                                            if (li === 1) return lt === "鍘熺増" || lt === "vanilla"
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
                                                        font.pixelSize: 14; font.weight: Font.DemiBold; color: "#d0d4e0"
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
                                                                if (vt === "snapshot") return "蹇収"
                                                                if (vt === "old") return "鏃х増"
                                                                if (vt === "modded") return "鏁村悎"
                                                                return "姝ｅ紡鐗?
                                                            }
                                                            font.pixelSize: 9; color: "#e8ecf8"
                                                        }
                                                    }

                                                    Item { Layout.fillWidth: true }

                                                    // Size + mod count
                                                    ColumnLayout {
                                                        spacing: 0
                                                        Text { text: model.sizeDisplay || ""; font.pixelSize: 11; font.weight: Font.Medium; color: "#808898"; Layout.alignment: Qt.AlignRight }
                                                        Text { text: model.modCount > 0 ? model.modCount + " 妯＄粍" : ""; font.pixelSize: 10; color: "#707488"; Layout.alignment: Qt.AlignRight }
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
                                              ? "娌℃湁鍖归厤鐨勭増鏈? : (backend && backend.installedVersions && backend.installedVersions.length === 0 ? "杩樻病鏈夊畨瑁呬换浣曠増鏈琝n鍓嶅線涓嬭浇椤甸潰瀹夎绗竴涓増鏈惂" : "")
                                        font.pixelSize: 12; color: "#505468"
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

                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: 16; spacing: 0

                            // 鈺愨晲鈺?TOP BAR: version info + actions 鈺愨晲鈺?
                            Rectangle {
                                Layout.fillWidth: true; height: 56; radius: 8
                                color: "#11141c"; border.color: "#1a1e28"
                                RowLayout {
                                    anchors.fill: parent; anchors.margins: 14; spacing: 12

                                    // Back button
                                    Rectangle {
                                        width: 60; height: 28; radius: 6; color: "transparent"; border.color: "#1a1f2e"
                                        Text { anchors.centerIn: parent; text: "鈫?杩斿洖"; font.pixelSize: 11; color: "#707488" }
                                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { showVersionSettings = false } }
                                    }

                                    // Version label
                                    Text {
                                        Layout.fillWidth: true
                                        text: currentSelectedVersion || "鏈€夋嫨鐗堟湰"
                                        font.pixelSize: 16; font.weight: Font.Bold; color: "#d0d4e0"
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
                                        Text { anchors.centerIn: parent; text: "鍚?鍔?"; font.pixelSize: 13; font.weight: Font.Bold; color: "#e8ecf8" }
                                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                if (!backend) return
                                                if (!currentSelectedVersion) { showToast("请先选择一个版本"); return }
                                                if (!backend.username) { showToast("璇峰厛鐧诲綍璐﹀彿"); return }
                                                backend.launch(currentSelectedVersion)
                                            }
                                        }
                                    }
                                }
                            }

                            Item { Layout.preferredHeight: 12 }

                            // 鈺愨晲鈺?BODY: sidebar + content 鈺愨晲鈺?
                            RowLayout {
                                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 16
                            Rectangle {
                                Layout.preferredWidth: 170; Layout.fillHeight: true; color: "transparent"
                                property var sectionModel: [
                                    { text: "姒傝", icon: "" },
                                    { text: "鍚姩閰嶇疆", icon: "" },
                                    { text: "Mod 绠＄悊", icon: "" },
                                    { text: "璧勬簮鍖呯鐞?, icon: "" },
                                    { text: "瀛樻。绠＄悊", icon: "" },
                                    { text: "宸ュ叿涓庣淮鎶?, icon: "" }
                                ]
                                ListView {
                                    id: settingsNav
                                    anchors.fill: parent
                                    model: parent.sectionModel
                                    delegate: Rectangle {
                                        width: settingsNav.width; height: 36; radius: 6
                                        color: ListView.isCurrentItem ? "#162040" : (mouseArea2.containsMouse ? "#11141c" : "transparent")
                                        Rectangle { anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 3; color: ListView.isCurrentItem ? "#5080e8" : "transparent" }
                                        RowLayout {
                                            anchors.fill: parent; anchors.leftMargin: 16; spacing: 10
                                            Text {
                                                text: modelData.text
                                                font.pixelSize: 13
                                                color: ListView.isCurrentItem ? "#e0e4f8" : (mouseArea2.containsMouse ? "#c0c4d8" : "#686c80")
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
                                Layout.fillWidth: true; Layout.fillHeight: true
                                color: "#11141c"; radius: 8; border.color: "#1a1e28"

                                // 鈺愨晲鈺?Section 0: 姒傝 鈺愨晲鈺?
                                ColumnLayout {
                                    anchors.fill: parent; anchors.margins: 24; spacing: 12
                                    visible: settingsNav.currentIndex === 0

                                    // Quick info
                                    Rectangle {
                                        Layout.fillWidth: true; height: 52; radius: 6; color: "#0d1018"
                                        RowLayout {
                                            anchors.fill: parent; anchors.margins: 14; spacing: 12
                                            ColumnLayout { Layout.fillWidth: true; spacing: 2
                                                Text { text: "鍗犵敤绌洪棿"; font.pixelSize: 10; color: "#505468" }
                                                Text { text: backend && backend.currentVersionSummary ? backend.currentVersionSummary.sizeDisplay : "-"; font.pixelSize: 14; font.weight: Font.Medium; color: "#a0a5b8" }
                                            }
                                            Rectangle { width: 1; height: 32; color: "#1a1f2e" }
                                            ColumnLayout { Layout.fillWidth: true; spacing: 2
                                                Text { text: "宸茶 Mod"; font.pixelSize: 10; color: "#505468" }
                                                Text { text: (backend && backend.currentVersionSummary ? backend.currentVersionSummary.modCount : 0) + " 涓?; font.pixelSize: 14; font.weight: Font.Medium; color: "#a0a5b8" }
                                            }
                                            Rectangle { width: 1; height: 32; color: "#1a1f2e" }
                                            ColumnLayout { Layout.fillWidth: true; spacing: 2
                                                Text { text: "鐗堟湰闅旂"; font.pixelSize: 10; color: "#505468" }
                                                Text { text: backend && backend.isolationEnabled ? "宸插紑鍚? : "鏈紑鍚?; font.pixelSize: 14; font.weight: Font.Medium; color: backend && backend.isolationEnabled ? "#4bc870" : "#707088" }
                                            }
                                        }
                                    }

                                    // Shortcuts
                                    Text { text: "蹇嵎鍏ュ彛"; font.pixelSize: 10; color: "#404458"; font.letterSpacing: 1.5 }
                                    Flow {
                                        Layout.fillWidth: true; spacing: 8

                                        // Always visible
                                        Rectangle { width: 120; height: 32; radius: 6; color: shortcutHover0.hovered ? "#2538b0" : "#3a4eb8"
                                            Text { anchors.centerIn: parent; text: "馃搧 鐗堟湰鏂囦欢澶?"; font.pixelSize: 11; color: "#e8ecf8" }
                                            HoverHandler { id: shortcutHover0 }
                                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                onClicked: { if (!currentSelectedVersion) { showToast("璇峰厛閫夋嫨涓€涓増鏈?); return }; if (backend) backend.openVersionDir(currentSelectedVersion) }
                                            }
                                        }
                                        Rectangle { width: 120; height: 32; radius: 6; color: shortcutHover1.hovered ? "#2538b0" : "#3a4eb8"
                                            Text { anchors.centerIn: parent; text: "馃捑 瀛樻。鏂囦欢澶?"; font.pixelSize: 11; color: "#e8ecf8" }
                                            HoverHandler { id: shortcutHover1 }
                                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                onClicked: { if (!currentSelectedVersion) { showToast("璇峰厛閫夋嫨涓€涓増鏈?); return }; if (backend) backend.openSavesFolder() }
                                            }
                                        }
                                        Rectangle { width: 120; height: 32; radius: 6; color: shortcutHover2.hovered ? "#2538b0" : "#3a4eb8"
                                            Text { anchors.centerIn: parent; text: "馃摲 鎴浘鏂囦欢澶?"; font.pixelSize: 11; color: "#e8ecf8" }
                                            HoverHandler { id: shortcutHover2 }
                                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                onClicked: { if (!currentSelectedVersion) { showToast("璇峰厛閫夋嫨涓€涓増鏈?); return }; if (backend) backend.openScreenshotsFolder() }
                                            }
                                        }
                                        Rectangle { width: 120; height: 32; radius: 6; color: shortcutHover6.hovered ? "#2538b0" : "#3a4eb8"
                                            Text { anchors.centerIn: parent; text: "馃搵 logs 鏃ュ織"; font.pixelSize: 11; color: "#e8ecf8" }
                                            HoverHandler { id: shortcutHover6 }
                                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                onClicked: { if (backend && !backend.openLogsFolder()) showToast("鏃犳棩蹇楁枃浠?) }
                                            }
                                        }
                                        Rectangle { width: 130; height: 32; radius: 6; color: shortcutHover7.hovered ? "#2538b0" : "#3a4eb8"
                                            Text { anchors.centerIn: parent; text: "馃搫 鏈€鏂板惎鍔ㄦ棩蹇?"; font.pixelSize: 11; color: "#e8ecf8" }
                                            HoverHandler { id: shortcutHover7 }
                                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                onClicked: { if (backend && !backend.openLatestLog()) showToast("鏈壘鍒板惎鍔ㄦ棩蹇?) }
                                            }
                                        }
                                        Rectangle { width: 130; height: 32; radius: 6; color: shortcutHover8.hovered ? "#b04040" : "#8a3030"
                                            Text { anchors.centerIn: parent; text: "馃挜 宕╂簝鏃ュ織"; font.pixelSize: 11; color: "#e8ecf8" }
                                            HoverHandler { id: shortcutHover8 }
                                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                onClicked: { if (backend && !backend.openCrashLog()) showToast("鏃犲穿婧冩姤鍛?) }
                                            }
                                        }

                                        // Copy path
                                        Rectangle { width: 130; height: 32; radius: 6; color: shortcutHoverCp.hovered ? "#2538b0" : "#3a4eb8"
                                            Text { anchors.centerIn: parent; text: "馃搵 澶嶅埗鐗堟湰璺緞"; font.pixelSize: 11; color: "#e8ecf8" }
                                            HoverHandler { id: shortcutHoverCp }
                                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    if (!currentSelectedVersion) return
                                                    if (backend) backend.copyVersionPath(currentSelectedVersion)
                                                }
                                            }
                                        }

                                        // Mod-only: visible only for modded versions
                                        Rectangle { width: 120; height: 32; radius: 6; color: shortcutHover3.hovered ? "#2538b0" : "#4a5ec8"
                                            visible: backend ? backend.isModdedVersion(currentSelectedVersion) : false
                                            Text { anchors.centerIn: parent; text: "馃З Mod 鏂囦欢澶?"; font.pixelSize: 11; color: "#e8ecf8" }
                                            HoverHandler { id: shortcutHover3 }
                                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                onClicked: { if (!currentSelectedVersion) { showToast("璇峰厛閫夋嫨涓€涓増鏈?); return }; if (backend) backend.openModsFolder() }
                                            }
                                        }
                                        Rectangle { width: 120; height: 32; radius: 6; color: shortcutHover4.hovered ? "#2538b0" : "#4a5ec8"
                                            visible: backend ? backend.isModdedVersion(currentSelectedVersion) : false
                                            Text { anchors.centerIn: parent; text: "鈿?config 閰嶇疆"; font.pixelSize: 11; color: "#e8ecf8" }
                                            HoverHandler { id: shortcutHover4 }
                                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                onClicked: { if (!currentSelectedVersion) { showToast("璇峰厛閫夋嫨涓€涓増鏈?); return }; if (backend) backend.openConfigFolder() }
                                            }
                                        }
                                        Rectangle { width: 120; height: 32; radius: 6; color: shortcutHover5.hovered ? "#2538b0" : "#4a5ec8"
                                            visible: backend ? backend.isModdedVersion(currentSelectedVersion) : false
                                            Text { anchors.centerIn: parent; text: "鉁?鍏夊奖鍖?"; font.pixelSize: 11; color: "#e8ecf8" }
                                            HoverHandler { id: shortcutHover5 }
                                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                onClicked: { if (!currentSelectedVersion) { showToast("璇峰厛閫夋嫨涓€涓増鏈?); return }; if (backend) backend.openShaderPacksFolder() }
                                            }
                                        }
                                    }

                                    Item { Layout.fillHeight: true }
                                }

                                // 鈺愨晲鈺?Section 1: 鍚姩閰嶇疆 鈺愨晲鈺?
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

                                            // Java 鐜
                                            Text { text: "Java 鐜"; font.pixelSize: 12; font.weight: Font.DemiBold; color: "#d0d4e0" }

                                            // Recommended Java hint
                                            Text {
                                                text: {
                                                    if (!backend || !currentSelectedVersion) return ""
                                                    var vid = currentSelectedVersion
                                                    var parts = vid.split(".")
                                                    if (parts[0] === "1" && parts.length > 1) {
                                                        var minor = parseInt(parts[1]) || 0
                                                        if (minor >= 18) return "鎺ㄨ崘: Java 17+"
                                                        if (minor === 17) return "鎺ㄨ崘: Java 16+"
                                                        if (minor >= 12) return "鎺ㄨ崘: Java 8"
                                                        return "鎺ㄨ崘: Java 8"
                                                    }
                                                    return ""
                                                }
                                                font.pixelSize: 10; color: "#4bc870"
                                            }

                                            Rectangle {
                                                Layout.fillWidth: true; height: 36; radius: 6; color: "transparent"; border.color: "#1a1f2e"
                                                Text { anchors.left: parent.left; anchors.leftMargin: 12; anchors.verticalCenter: parent.verticalCenter
                                                    text: backend ? (backend.javaPath || "鏈娴嬪埌 Java") : "鏈娴嬪埌 Java"
                                                    font.pixelSize: 12; color: "#9094a8"; elide: Text.ElideMiddle; width: parent.width - 90 }
                                                Rectangle { anchors.right: parent.right; anchors.rightMargin: 6; anchors.verticalCenter: parent.verticalCenter
                                                    width: 90; height: 26; radius: 4; color: "#3a4eb8"
                                                    Text { anchors.centerIn: parent; text: "鑷姩妫€娴?"; font.pixelSize: 11; color: "#e8ecf8" }
                                                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { if (backend) backend.detectJava() } }
                                                }
                                                Rectangle { anchors.right: parent.right; anchors.rightMargin: 102; anchors.verticalCenter: parent.verticalCenter
                                                    width: 50; height: 26; radius: 4; color: "transparent"; border.color: "#1a1f2e"
                                                    Text { anchors.centerIn: parent; text: "娴忚"; font.pixelSize: 11; color: "#9094a8" }
                                                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { if (backend) backend.pickJava() } }
                                                }
                                            }
                                            Text {
                                                text: {
                                                    if (!backend || !backend.javaVersion) return ""
                                                    var compat = backend.javaCompatibility
                                                    var base = backend.javaVersion
                                                    if (compat === "recommended") return base + "  鉁?鎺ㄨ崘"
                                                    if (compat === "compatible") return base + "  鈿?鍏煎"
                                                    if (compat === "incompatible") return base + "  鉂?涓嶅吋瀹?
                                                    return base
                                                }
                                                font.pixelSize: 10
                                                color: {
                                                    var compat = backend ? backend.javaCompatibility : "unknown"
                                                    if (compat === "recommended") return "#4bc870"
                                                    if (compat === "compatible") return "#e0a040"
                                                    if (compat === "incompatible") return "#c05050"
                                                    return "#505468"
                                                }
                                            }

                                            // 鍐呭瓨璁剧疆
                                            Text { text: "鍐呭瓨鍒嗛厤"; font.pixelSize: 12; font.weight: Font.DemiBold; color: "#d0d4e0"; Layout.topMargin: 8 }
                                            RowLayout {
                                                Layout.fillWidth: true; spacing: 12
                                                ColumnLayout { Layout.fillWidth: true; spacing: 4
                                                    Text { text: "鏈€澶у唴瀛? " + (backend ? backend.maxMemoryMb + " MB" : "-"); font.pixelSize: 12; color: "#a0a5b8" }
                                                    Slider {
                                                        id: maxMemSlider; Layout.fillWidth: true
                                                        from: 512; to: 16384; stepSize: 512
                                                        value: backend ? backend.maxMemoryMb : 2048
                                                        enabled: backend ? !backend.autoMemoryEnabled : true
                                                        onMoved: { if (backend) backend.setMaxMemory(value) }
                                                    }
                                                }
                                                Text { text: "鏈€灏?" + (backend ? backend.minMemoryMb + " MB" : "-"); font.pixelSize: 11; color: "#707488" }
                                            }
                                            Text { text: backend ? backend.systemMemoryInfo : ""; font.pixelSize: 9; color: "#505468" }

                                            // 鑷姩鍐呭瓨
                                            RowLayout {
                                                Text { text: "鑷姩鍒嗛厤"; font.pixelSize: 12; color: "#a0a5b8" }
                                                Item { Layout.fillWidth: true }
                                                Switch {
                                                    checked: backend ? backend.autoMemoryEnabled : true
                                                    onToggled: { if (backend) backend.setAutoMemoryEnabled(checked) }
                                                }
                                            }

                                            // JVM 鍙傛暟
                                            Text { text: "JVM 鍙傛暟"; font.pixelSize: 12; font.weight: Font.DemiBold; color: "#d0d4e0"; Layout.topMargin: 8 }
                                            Rectangle { Layout.fillWidth: true; height: 60; radius: 6; color: "#0d1018"; border.color: "#1a1f2e"
                                                TextInput {
                                                    id: jvmArgsInput; anchors.fill: parent; anchors.margins: 8
                                                    color: "#9094a8"; font.pixelSize: 11; font.family: "Consolas, monospace"
                                                    text: backend ? backend.jvmArgs : ""
                                                    onEditingFinished: { if (backend) backend.setJvmArgs(text) }
                                                }
                                            }

                                            // 棰勮
                                            Text { text: "棰勮"; font.pixelSize: 10; color: "#505468" }
                                            Flow {
                                                Layout.fillWidth: true; spacing: 8
                                                Repeater {
                                                    model: [
                                                        { label: "榛樿骞宠　", args: "-Xmx2G -XX:+UseG1GC" },
                                                        { label: "鎬ц兘浼樺厛", args: "-Xmx4G -XX:+UseG1GC -XX:+AggressiveOpts" },
                                                        { label: "浣庡欢杩?, args: "-Xmx2G -XX:+UseZGC" }
                                                    ]
                                                    Rectangle { width: 90; height: 26; radius: 4; color: presetHover.hovered ? "#1a2848" : "#0d1018"; border.color: "#1a1f2e"
                                                        Text { anchors.centerIn: parent; text: modelData.label; font.pixelSize: 11; color: "#9094a8" }
                                                        HoverHandler { id: presetHover }
                                                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                            onClicked: { jvmArgsInput.text = modelData.args; if (backend) backend.setJvmArgs(modelData.args) }
                                                        }
                                                    }
                                                }
                                            }

                                            // 鐗堟湰闅旂
                                            Text { text: "鐗堟湰琛屼负"; font.pixelSize: 12; font.weight: Font.DemiBold; color: "#d0d4e0"; Layout.topMargin: 8 }
                                            RowLayout {
                                                Text { text: "鐗堟湰闅旂锛堟ā缁?閰嶇疆/瀛樻。鐙珛锛?"; font.pixelSize: 12; color: "#a0a5b8" }
                                                Item { Layout.fillWidth: true }
                                                Switch {
                                                    checked: backend ? backend.isolationEnabled : false
                                                    onToggled: { if (backend) backend.setIsolationEnabled(checked) }
                                                }
                                            }

                                            // 鍚姩鍚庡叧闂惎鍔ㄥ櫒
                                            RowLayout {
                                                Text { text: "鍚姩鍚庡叧闂惎鍔ㄥ櫒"; font.pixelSize: 12; color: "#a0a5b8" }
                                                Item { Layout.fillWidth: true }
                                                Switch {
                                                    checked: backend ? backend.closeOnLaunch : false
                                                    onToggled: { if (backend) backend.setCloseOnLaunch(checked) }
                                                }
                                            }

                                            // 娓告垙闄勫姞鍙傛暟
                                            Text { text: "娓告垙闄勫姞鍙傛暟"; font.pixelSize: 12; font.weight: Font.DemiBold; color: "#d0d4e0"; Layout.topMargin: 10 }
                                            Text { text: "锛堢ず渚? --width 1920 --height 1080 --fullscreen锛?"; font.pixelSize: 9; color: "#505468" }
                                            Rectangle { Layout.fillWidth: true; height: 44; radius: 6; color: "#0d1018"; border.color: "#1a1f2e"
                                                TextInput {
                                                    id: gameArgsInput; anchors.fill: parent; anchors.margins: 8
                                                    color: "#9094a8"; font.pixelSize: 11; font.family: "Consolas, monospace"
                                                    text: backend ? backend.gameArgs : ""
                                                    onEditingFinished: { if (backend) backend.setGameArgs(text) }
                                                }
                                            }
                                        }
                                    }
                                }

                                // 鈺愨晲鈺?Section 2: Mod 绠＄悊 鈺愨晲鈺?
                                ColumnLayout {
                                    anchors.fill: parent; anchors.margins: 24; spacing: 8
                                    visible: settingsNav.currentIndex === 2

                                    Text { text: "Mod 绠＄悊"; font.pixelSize: 14; font.bold: true; color: "#d0d4e0" }
                                    Text { text: "绠＄悊宸插畨瑁呯殑 Mod锛屾煡鐪嬬増鏈拰鍏煎鎬с€?"; font.pixelSize: 11; color: "#606478" }

                                    // Refresh + search
                                    RowLayout {
                                        spacing: 8
                                        Rectangle {
                                            width: 80; height: 28; radius: 4; color: "#3a4eb8"
                                            Text { anchors.centerIn: parent; text: "鍒锋柊鍒楄〃"; font.pixelSize: 11; color: "#e8ecf8" }
                                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                onClicked: { modListModel.clear(); if (backend) { var m = backend.listMods(); for (var i = 0; i < m.length; i++) modListModel.append(m[i]) } }
                                            }
                                        }
                                        Rectangle {
                                            Layout.fillWidth: true; height: 28; radius: 4; color: "#0d1018"; border.color: modSearchField.activeFocus ? "#5068c8" : "#1a1f2e"
                                            TextInput {
                                                id: modSearchField; anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 10
                                                color: "#d0d4e0"; font.pixelSize: 12; verticalAlignment: TextInput.AlignVCenter
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
                                                    text: "鎼滅储 Mod..."; color: "#404458"; font.pixelSize: 12
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
                                                Text { text: name; font.pixelSize: 12; color: "#c0c4d8"; Layout.fillWidth: true; elide: Text.ElideRight }
                                                Text { text: sizeDisplay; font.pixelSize: 10; color: "#606478" }
                                                Rectangle { width: 60; height: 24; radius: 3; color: delBtnHover.hovered ? "#6b2020" : "transparent"; border.color: "#4a2828"
                                                    Text { anchors.centerIn: parent; text: "鍒犻櫎"; font.pixelSize: 10; color: "#c05050" }
                                                    MouseArea { id: delBtnHover; anchors.fill: parent; cursorShape: Qt.PointingHandCursor; hoverEnabled: true
                                                        onClicked: { if (backend) backend.deleteMod(name); modListModel.remove(index) }
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

                                // 鈺愨晲鈺?Section 3: 璧勬簮鍖呯鐞?鈺愨晲鈺?
                                ColumnLayout {
                                    anchors.fill: parent; anchors.margins: 24; spacing: 8
                                    visible: settingsNav.currentIndex === 3

                                    Text { text: "璧勬簮鍖呯鐞?"; font.pixelSize: 14; font.bold: true; color: "#d0d4e0" }
                                    Text { text: "绠＄悊宸插畨瑁呯殑璧勬簮鍖呭拰鏉愯川鍖呫€?"; font.pixelSize: 11; color: "#606478" }

                                    RowLayout {
                                        spacing: 8
                                        Rectangle { height: 28; radius: 4; color: "#3a4eb8"; implicitWidth: rpRefreshText.implicitWidth + 20
                                            Text { id: rpRefreshText; anchors.centerIn: parent; text: "鍒锋柊鍒楄〃"; font.pixelSize: 11; color: "#e8ecf8" }
                                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                onClicked: { rpListModel.clear(); if (backend) { var p = backend.listResourcePacks(); for (var i = 0; i < p.length; i++) rpListModel.append(p[i]) } }
                                            }
                                        }
                                        Rectangle { height: 28; radius: 4; color: "transparent"; border.color: "#1a1f2e"; implicitWidth: rpOpenText.implicitWidth + 20
                                            Text { id: rpOpenText; anchors.centerIn: parent; text: "鎵撳紑鏂囦欢澶?"; font.pixelSize: 11; color: "#9094a8" }
                                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { if (backend) backend.openResourcePacksFolder() } }
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
                                                    Text { anchors.centerIn: parent; text: isDir ? "馃搧" : "馃梻"; font.pixelSize: 10 }
                                                }
                                                Text { text: name; font.pixelSize: 12; color: "#c0c4d8"; Layout.fillWidth: true; elide: Text.ElideRight }
                                                Text { text: sizeDisplay; font.pixelSize: 10; color: "#606478" }
                                                Rectangle { width: 60; height: 24; radius: 3; color: "transparent"; border.color: "#4a2828"
                                                    Text { anchors.centerIn: parent; text: "鍒犻櫎"; font.pixelSize: 10; color: "#c05050" }
                                                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                        onClicked: { if (backend) backend.deleteResourcePack(name); rpListModel.remove(index) }
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

                                // 鈺愨晲鈺?Section 4: 瀛樻。绠＄悊 鈺愨晲鈺?
                                ColumnLayout {
                                    anchors.fill: parent; anchors.margins: 24; spacing: 8
                                    visible: settingsNav.currentIndex === 4

                                    Text { text: "瀛樻。绠＄悊"; font.pixelSize: 14; font.bold: true; color: "#d0d4e0" }
                                    Text { text: "鏌ョ湅銆佸浠藉拰绠＄悊浣犵殑娓告垙瀛樻。銆?"; font.pixelSize: 11; color: "#606478" }

                                    Rectangle {
                                        width: 80; height: 28; radius: 4; color: "#3a4eb8"
                                        Text { anchors.centerIn: parent; text: "鍒锋柊鍒楄〃"; font.pixelSize: 11; color: "#e8ecf8" }
                                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                            onClicked: { saveListModel.clear(); if (backend) { var s = backend.listSaves(); for (var i = 0; i < s.length; i++) saveListModel.append(s[i]) } }
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
                                                Text { text: name; font.pixelSize: 12; color: "#c0c4d8"; Layout.fillWidth: true; elide: Text.ElideRight }
                                                Text { text: sizeDisplay; font.pixelSize: 10; color: "#606478" }
                                                Rectangle { width: 60; height: 24; radius: 3; color: "transparent"; border.color: "#4a2828"
                                                    Text { anchors.centerIn: parent; text: "鍒犻櫎"; font.pixelSize: 10; color: "#c05050" }
                                                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                        onClicked: { if (backend) backend.deleteSave(name); saveListModel.remove(index) }
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

                                // 鈺愨晲鈺?Section 5: 宸ュ叿涓庣淮鎶?鈺愨晲鈺?
                                ColumnLayout {
                                    anchors.fill: parent; anchors.margins: 24; spacing: 12
                                    visible: settingsNav.currentIndex === 5

                                    Text { text: "娓告垙瀹屾暣鎬ф牎楠?"; font.pixelSize: 14; font.bold: true; color: "#d0d4e0" }
                                    Text { text: "鎵弿閫夊畾鐗堟湰鐨勬父鎴忔枃浠跺畬鏁存€э紝妫€鏌ユ崯鍧忔垨缂哄け鐨勬枃浠躲€?"; font.pixelSize: 11; color: "#606478"; wrapMode: Text.WordWrap; Layout.fillWidth: true }

                                    // Start button
                                    Rectangle {
                                        width: 140; height: 36; radius: 6
                                        color: (backend && backend.verifyRunning) ? "#2a3040" : (verifyBtnMouse.containsMouse ? "#2563EB" : "#3a4eb8")
                                        Text { anchors.centerIn: parent; text: (backend && backend.verifyRunning) ? "鏍￠獙涓?.." : "寮€濮嬫牎楠?; font.pixelSize: 12; color: "#e8ecf8" }
                                        MouseArea {
                                            id: verifyBtnMouse; anchors.fill: parent; cursorShape: Qt.PointingHandCursor; hoverEnabled: true
                                            enabled: !(backend && backend.verifyRunning)
                                            onClicked: { if (!currentSelectedVersion) { showToast("璇峰厛閫夋嫨涓€涓増鏈?); return } if (backend) backend.verifyVersion(currentSelectedVersion) }
                                        }
                                    }

                                    // Progress bar
                                    ColumnLayout {
                                        Layout.fillWidth: true; spacing: 6
                                        visible: backend ? backend.verifyRunning || backend.verifyProgressTotal > 0 : false
                                        Rectangle {
                                            Layout.fillWidth: true; height: 8; radius: 4; color: "#1a1e28"
                                            Rectangle {
                                                height: 8; radius: 4
                                                width: backend && backend.verifyProgressTotal > 0 ? parent.width * (backend.verifyProgressDone / backend.verifyProgressTotal) : 0
                                                color: backend && backend.verifyFinished !== undefined && backend._verify_running ? "#6080e8" : (backend && backend.verifyProgressDone === backend.verifyProgressTotal ? "#4bc870" : "#c05050")
                                                Behavior on width { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                                            }
                                        }
                                        Text {
                                            text: {
                                                if (backend && backend.verifyProgressTotal > 0) {
                                                    var pct = Math.round(backend.verifyProgressDone / backend.verifyProgressTotal * 100)
                                                    return "鏍￠獙涓?.. " + backend.verifyProgressDone + "/" + backend.verifyProgressTotal + " (" + pct + "%)"
                                                }
                                                return backend && backend.verifyRunning ? "鍑嗗涓?.." : ""
                                            }
                                            font.pixelSize: 11; color: "#707488"
                                        }
                                    }

                                    Rectangle {
                                        Layout.fillWidth: true; Layout.fillHeight: true
                                        color: "#0d1018"; radius: 6; border.color: "#1a1e28"
                                        visible: backend && backend.verifyResultText !== ""
                                        clip: true
                                        Flickable {
                                            anchors.fill: parent; anchors.margins: 12
                                            contentHeight: verifyResultLabel.implicitHeight
                                            Text {
                                                id: verifyResultLabel
                                                width: parent.width
                                                text: backend ? backend.verifyResultText : ""
                                                font.pixelSize: 11; color: "#9094a8"
                                                font.family: "Consolas, Microsoft YaHei, monospace"
                                                wrapMode: Text.WordWrap
                                                lineHeight: 1.6
                                            }
                                        }
                                    }

                                    Item { height: 12; width: 1 }

                                    // Repair button (only visible when verification found issues)
                                    Rectangle {
                                        width: 140; height: 36; radius: 6
                                        color: repairBtnHover.hovered ? "#c07830" : "#b06820"
                                        visible: verifyResultLabel.text.indexOf("涓枃浠舵湁闂") > 0 || verifyResultLabel.text.indexOf("鎹熷潖") > 0 || verifyResultLabel.text.indexOf("缂哄け") > 0
                                        Text { anchors.centerIn: parent; text: "涓€閿慨澶?"; font.pixelSize: 12; color: "#e8ecf8" }
                                        HoverHandler { id: repairBtnHover }
                                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                            onClicked: { if (!currentSelectedVersion) { showToast("璇峰厛閫夋嫨涓€涓増鏈?); return }; if (backend) backend.repairVersion(currentSelectedVersion) }
                                        }
                                    }

                                    Item { height: 12; width: 1 }

                                    // Version tools
                                    Text { text: "鐗堟湰宸ュ叿"; font.pixelSize: 10; color: "#404458"; font.letterSpacing: 1.5 }
                                    Flow {
                                        Layout.fillWidth: true; spacing: 8

                                        // Clone
                                        Rectangle { width: 110; height: 32; radius: 5; color: cloneHover.hovered ? "#1a2848" : "#0d1018"; border.color: "#1a1f2e"
                                            Text { anchors.centerIn: parent; text: "鍏嬮殕鐗堟湰"; font.pixelSize: 11; color: "#a0a5b8" }
                                            HoverHandler { id: cloneHover }
                                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    if (!currentSelectedVersion) { showToast("璇峰厛閫夋嫨涓€涓増鏈?); return }
                                                    if (backend) backend.cloneVersion(currentSelectedVersion)
                                                    showToast("已克隆版本: " + currentSelectedVersion)                                }
                                            }
                                        }

                                        // Rename
                                        Rectangle { width: 110; height: 32; radius: 5; color: renameHover.hovered ? "#1a2848" : "#0d1018"; border.color: "#1a1f2e"
                                            Text { anchors.centerIn: parent; text: "閲嶅懡鍚嶇増鏈?"; font.pixelSize: 11; color: "#a0a5b8" }
                                            HoverHandler { id: renameHover }
                                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    if (!currentSelectedVersion) { showToast("璇峰厛閫夋嫨涓€涓増鏈?); return }
                                                    if (backend) backend.renameVersion(currentSelectedVersion)
                                                }
                                            }
                                        }

                                        // Migrate
                                        Rectangle { width: 110; height: 32; radius: 5; color: migrateHover.hovered ? "#1a2848" : "#0d1018"; border.color: "#1a1f2e"
                                            Text { anchors.centerIn: parent; text: "迁移目录"; font.pixelSize: 11; color: "#a0a5b8" }
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
                                    Text { text: "鍗遍櫓鎿嶄綔"; font.pixelSize: 10; color: "#c05050"; font.letterSpacing: 1.5 }
                                    Rectangle {
                                        Layout.fillWidth: true; height: 36; radius: 6; color: "transparent"; border.color: "#2a1f24"
                                        Text { anchors.left: parent.left; anchors.leftMargin: 14; anchors.verticalCenter: parent.verticalCenter; text: "鍒犻櫎姝ょ増鏈?"; font.pixelSize: 13; color: "#c05050" }
                                        MouseArea {
                                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                if (!currentSelectedVersion) { showToast("璇峰厛閫夋嫨涓€涓増鏈?); return }
                                                confirmDialog.title = "鍒犻櫎鐗堟湰"
                                                confirmDialog.message = "纭畾瑕佸垹闄ょ増鏈?芦 " + currentSelectedVersion + " 禄 鍚楋紵\n姝ゆ搷浣滀笉鍙挙閿€锛岀増鏈枃浠跺す灏嗚姘镐箙鍒犻櫎銆?
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
            }
        }
    }

    Loader {
        id: launchOverlayLoader; anchors.fill: parent; z: 20
        source: "LaunchOverlay.qml"
        active: backend && backend.launching; visible: active
    }

    // 鈺愨晲鈺?Toast (disabled - TODO: fix black bar on popup) 鈺愨晲鈺?
    // See issue: toast with anchors.bottom causes layout jitter
    property string _toastMsg: ""
    function showToast(msg) { /* TODO */ }

    Rectangle {
        id: killButton
        width: 48; height: 48; radius: 24
        anchors.right: parent.right; anchors.rightMargin: 20
        anchors.bottom: parent.bottom; anchors.bottomMargin: 56
        z: 200; color: "#c05050"
        opacity: backend ? (backend.isRunning ? 1 : 0) : 0
        scale: backend ? (backend.isRunning ? 1 : 0.3) : 0.3
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
        Behavior on scale { NumberAnimation { duration: 400; easing.type: Easing.OutBack } }
        Text { anchors.centerIn: parent; text: "\u25A0"; color: "#e8ecf8"; font.pixelSize: 16 }
        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { if (backend) backend.killMinecraft() } }
    }

    Connections {
        target: backend; enabled: backend !== null
        function onLogMessage(msg) { logArea.text += msg + "\n" }
        function onMinecraftStarted() { killButton.visible = true }
        function onMinecraftStopped() { killButton.visible = false }
    }

    // 鈺愨晲鈺?Confirm Dialog 鈺愨晲鈺?
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
            Text { text: confirmDialog.title; font.pixelSize: 15; font.weight: Font.Bold; color: "#d0d4e0" }
            Text { text: confirmDialog.message; font.pixelSize: 12; color: "#9094a8"; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            Item { Layout.fillHeight: true }
            RowLayout {
                Layout.alignment: Qt.AlignRight; spacing: 10
                Rectangle { width: 80; height: 32; radius: 5; color: "transparent"; border.color: "#2a2e3c"
                    Text { anchors.centerIn: parent; text: "鍙栨秷"; font.pixelSize: 12; color: "#9094a8" }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: confirmDialog.visible = false }
                }
                Rectangle { width: 80; height: 32; radius: 5; color: "#c05050"
                    Text { anchors.centerIn: parent; text: "纭"; font.pixelSize: 12; color: "#e8ecf8" }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
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
}
