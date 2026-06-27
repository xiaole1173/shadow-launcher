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
    property bool showInstallPage: false
    property string installMcVersion: ""
    property var offlineHistory: []
    property bool pageLoading: false
    property var runningListModel: []

    function navLabel(key) {
        switch (key) {
            case "home": return qsTr("启动")
            case "download": return qsTr("下载")
            case "settings": return qsTr("设置")
            case "download_progress": return qsTr("下载进度")
            default: return key
        }
    }

    // Mod download error dialog state
    property bool showModDlError: false
    property var modDlErrorInfo: ({})

    Component.onCompleted: {
        console.log("[main] window completed, t=" + Date.now())
        if (backend) {
            backend.refreshInstalled()
            runningListModel = backend.runningGames()
            console.log("[main] init done, t=" + Date.now())
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
        console.log("[main] showDownloadNav called: downloadNavVisible=" + downloadNavVisible + " navModel.count=" + navModel.count)
        // Always show nav AND auto-switch (silent may pre-add the nav item)
        if (!downloadNavVisible) {
            downloadNavVisible = true
            navModel.append({ label: "下载进度", pageKey: "download_progress", icon: "package" })
            console.log("[main] showDownloadNav: appended nav, new count=" + navModel.count)
        }
        // Always auto-switch — critical: must not be blocked by silent pre-add
        console.log("[main] showDownloadNav: switching to page " + (navModel.count - 1))
        switchPage(navModel.count - 1)
    }

    function showDownloadNavSilent() {
        // Add nav item without auto-switching (for ball animation flow)
        if (!downloadNavVisible) {
            downloadNavVisible = true
            navModel.append({ label: "下载进度", pageKey: "download_progress", icon: "package" })
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

    function showModDownloadProgress() {
        // Show download nav item for mod downloads too
        if (!downloadNavVisible) {
            downloadNavVisible = true
            navModel.append({ label: "下载进度", pageKey: "download_progress", icon: "package" })
        }
        switchPage(navModel.count - 1)
    }

    Connections {
        target: backend; enabled: backend !== null
        function onInstallingChanged() {
            console.log("[main] onInstallingChanged: installing=", backend.installing, "downloadNavVisible=", downloadNavVisible)
            if (backend.installing) showDownloadNav()
        }
        function onSelectedVersionClearedAfterDelete() {
            // Binding auto-updates — no explicit assignment needed
        }
        function onInstallFinished(success) {
            // Keep nav visible for a moment, will be hidden when user switches away
        }
        function onInstallComplete(installName) {
            console.log("[main] installComplete:", installName)
            if (toastManager) {
                toastManager.show(installName + " 下载完成")
            }
            hideDownloadNav()
        }
        function onResourceDownloadStateChanged() {
            console.log("[main] resourceDownloadStateChanged downloading=", backend ? backend.isResourceDownloading : false)
            if (backend && backend.isResourceDownloading) {
                if (!downloadNavVisible) showDownloadNav()
            }
        }
        // ── Auto-test: navigate to page + sub-tab ──
        function onNavigateToRequested(pageIndex, subTab) {
            console.log("[auto-test] navigateToRequested: page", pageIndex, "tab", subTab)
            if (pageIndex === 1) {
                pendingSubTab = subTab
            }
            switchPage(pageIndex)
        }
        // ── Auto-test: open RP detail page ──
        function onOpenRpDetailRequested(slug) {
            console.log("[auto-test] openRpDetailRequested:", slug)
            switchPage(1)
            pendingSubTab = 3
            let timer = Qt.createQmlObject('import QtQuick; Timer { interval: 1500; running: true; repeat: false; onTriggered: { if (downloadPageLoader.item) { downloadPageLoader.item.rpDetailSlug = "' + slug + '"; downloadPageLoader.item.rpDetailTitle = "' + slug + '" } destroy() } }', appWindow)
        }
        function onOpenModDetailRequested(slug) {
            console.log("[auto-test] openModDetailRequested:", slug)
            switchPage(1)
            pendingSubTab = 1
            let timer = Qt.createQmlObject('import QtQuick; Timer { interval: 1500; running: true; repeat: false; onTriggered: { var item = downloadPageLoader.item; if (item) { item.modDetailSlug = "' + slug + '"; item.modDetailTitle = "' + slug + '"; item.modDetailPage.modDetailExpanded = []; item.modDetailPage.modDetailSelectedVer = ""; if (item.backend) item.backend.fetchModVersions(["' + slug + '"]) } destroy() } }', appWindow)
        }
        function onOpenShaderDetailRequested(slug) {
            console.log("[auto-test] openShaderDetailRequested:", slug)
            switchPage(1)
            pendingSubTab = 2
            let timer = Qt.createQmlObject('import QtQuick; Timer { interval: 1500; running: true; repeat: false; onTriggered: { var item = downloadPageLoader.item; if (item) { item.shaderDetailSlug = "' + slug + '"; item.shaderDetailTitle = "' + slug + '"; item.shaderDetailPage.shaderDetailExpanded = []; item.shaderDetailPage.shaderDetailSelectedVer = ""; if (item.backend) item.backend.fetchShaderVersions(["' + slug + '"]) } destroy() } }', appWindow)
        }
        // ── Auto-test: toggle pre-release switch ──
        function onSetRpShowPreReleases(show) {
            console.log("[auto-test] setRpShowPreReleases:", show)
            switchPage(1); pendingSubTab = 3
            let timer = Qt.createQmlObject('import QtQuick; Timer { interval: 1500; running: true; repeat: false; onTriggered: { if (downloadPageLoader.item) { downloadPageLoader.item.rpShowPreReleases = ' + show + ' } destroy() } }', appWindow)
        }
        // ── Auto-test: open version dropdown ──
        function onOpenRpVersionMenu() {
            console.log("[auto-test] openRpVersionMenu")
            switchPage(1); pendingSubTab = 3
            let timer = Qt.createQmlObject('import QtQuick; Timer { interval: 2000; running: true; repeat: false; onTriggered: { if (downloadPageLoader.item) { downloadPageLoader.item.toggleVersionMenu() } destroy() } }', appWindow)
        }
        // ── Auto-test: expand detail group ──
        function onExpandRpDetailGroup(major) {
            console.log("[auto-test] expandRpDetailGroup:", major)
            let t = Qt.createQmlObject('import QtQuick; Timer { interval: 500; running: true; repeat: false; onTriggered: { if (downloadPageLoader.item) { downloadPageLoader.item.rpDetailExpanded = "' + major + '" } destroy() } }', appWindow)
        }
    }

    property bool hasCustomBg: {
        if (!backend) return false
        var p = backend.customBgPath
        return typeof p === "string" && p.length > 0
    }

    // ── Rounded window container ──
    Rectangle {
        anchors.fill: parent; radius: 16
        color: hasCustomBg ? "transparent" : "#0c0f16"
        clip: true

        // ── Custom background image (clipped by container radius) ──
        Image {
            anchors.fill: parent; z: -2
            visible: hasCustomBg
            source: hasCustomBg ? backend.customBgPath : ""
            fillMode: Image.PreserveAspectCrop
            cache: false
            asynchronous: true
        }

        // ── Dark mask overlay ──
        Rectangle {
            anchors.fill: parent; z: -1
            color: "#000000"
            opacity: hasCustomBg ? (1.0 - backend.contentOpacity) : 0
            visible: hasCustomBg
            Behavior on opacity { NumberAnimation { duration: 300 } }
        }

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
            Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

            Rectangle {
                id: loadingSlider
                width: 100; height: 2; radius: 4
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
                id: sidebarContainer
                Layout.preferredWidth: 200; Layout.fillHeight: true
                layer.enabled: true
                visible: !appWindow.showInstallPage
                color: hasCustomBg ? "transparent" : "#0a0c12"; radius: 6
                opacity: hasCustomBg ? backend.sidebarOpacity : 1.0
                Behavior on opacity { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } }
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 8; spacing: 2
                    Text { Layout.topMargin: 8; Layout.bottomMargin: 20; Layout.leftMargin: 16; text: "SHADOW"; font.pixelSize: 16; font.bold: true; color: "#e4e8f2" }

                    // Navigation model
                    ListModel {
                        id: navModel
                        ListElement { label: "启动"; pageKey: "home"; icon: "home" }
                        ListElement { label: "下载"; pageKey: "download"; icon: "download" }
                        ListElement { label: "设置"; pageKey: "settings"; icon: "settings" }
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
                            Row {
                                anchors.left: parent.left; anchors.leftMargin: 20; anchors.verticalCenter: parent.verticalCenter
                                spacing: 8
                                Image {
                                    anchors.verticalCenter: parent.verticalCenter; width: 18; height: 18
                                    source: model.icon ? ("icons/lucide/" + model.icon + ".svg") : ""
                                    visible: model.icon !== undefined && model.icon !== ""
                                }
                                Text { text: appWindow.navLabel(model.pageKey); font.pixelSize: 13; color: navListIndex === index ? "#e4e8f2" : "#9498a8" }
                            }
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
                                        text: modelData.displayVersion || modelData.version || "?"
                                        font.pixelSize: 11; color: "#d4dcf0"
                                        elide: Text.ElideRight; Layout.fillWidth: true
                                    }
                                    Rectangle {
                                        width: 20; height: 20; radius: 8
                                        color: runningKillHover.containsMouse ? "#e06060" : "#c05050"
                                        scale: runningKillHover.containsMouse ? 1.15 : 1.0
                                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                                        Text { anchors.centerIn: parent; text: "\u2715"; font.pixelSize: 10; color: "#fff" }
                                        MouseArea {
                                            id: runningKillHover
                                            anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                            onClicked: { if (backend) backend.killGameByPid(modelData.pid) }
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
                    id: headerBar
                    Layout.fillWidth: true; height: 44
                    visible: !appWindow.showInstallPage
                    color: hasCustomBg ? "transparent" : "#0a0c12"
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
                            width: 28; height: 28; radius: 8
                            color: hdrMin.containsMouse ? (hdrMin.pressed ? "#3a4050" : "#252a35") : "transparent"
                            scale: hdrMin.pressed ? 0.85 : (hdrMin.containsMouse ? 1.12 : 1.0)
                            Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
                            Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                            Text { anchors.centerIn: parent; text: "\u2014"; color: hdrMin.containsMouse ? "#d0d4e0" : "#505568"; font.pixelSize: 13; font.weight: Font.Bold }
                            MouseArea { id: hdrMin; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: appWindow.showMinimized() }
                        }
                        Item { width: 6 }
                        Rectangle {
                            width: 28; height: 28; radius: 8
                            color: hdrClose.containsMouse ? (hdrClose.pressed ? "#e06060" : "#c05050") : "transparent"
                            scale: hdrClose.pressed ? 0.85 : (hdrClose.containsMouse ? 1.12 : 1.0)
                            Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
                            Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
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
                    // Sequential: overlay fades first, then page fades in
                    // Note: Behavior only animates explicit assignments, not bindings
                    opacity: 1.0

                    PropertyAnimation { id: pageFadeInAnim; target: pageContainer; property: "opacity"; to: 1.0; duration: 500; easing.type: Easing.InOutCubic }

                    // ========== HOMEPAGE ==========
                    Loader {
                        id: homePageLoader
                        asynchronous: true
                        anchors.fill: parent
                        source: "HomePage.qml"
                        opacity: navListIndex === 0 && !showVersionSelect && !showVersionSettings ? 1 : 0
                        visible: opacity > 0
                        enabled: opacity >= 1
                        Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        onItemChanged: {
                            if (item) {
                                item.backend = backend
                                item.toastManager = toastManager
                                item.appWindow = appWindow
                                item.loginMode = loginMode
                                item.currentSelectedVersion = Qt.binding(function() { return currentSelectedVersion })
                                item.versionSelectRequested.connect(function() { showVersionSelect = true })
                                item.versionSettingsRequested.connect(function() { showVersionSettings = true })
                                item.loginModeChanged.connect(function(mode) { loginMode = mode })
                                item.displayName = backend ? (loginMode === 0 ? (backend.username || "") : (backend.offlineUsername || "")) : ""
                            }
                        }
                    }

                    // ========== DOWNLOAD & SETTINGS ==========
                    Rectangle { anchors.fill: parent; visible: navListIndex === 1; color: hasCustomBg ? "transparent" : "#0c0f16"
                        Loader { id: downloadPageLoader; asynchronous: true; anchors.fill: parent; active: navListIndex === 1; source: active ? "DownloadPage.qml" : ""
                            onLoaded: {
                                item.mainWindow = appWindow
                                if (item.triggerDownloadBall) {
                                    item.triggerDownloadBall.connect(function(sx, sy) {
                                        appWindow.animateDownloadBall(sx, sy)
                                    })
                                }
                                // Apply pending sub-tab navigation from --navigate
                                if (pendingSubTab >= 0) {
                                    item.currentTab = pendingSubTab
                                    console.log("[auto-test] navigateTo: download tab", pendingSubTab)
                                    pendingSubTab = -1
                                }
                            }
                        } }
                    Rectangle { anchors.fill: parent; visible: navListIndex === 2; color: hasCustomBg ? "transparent" : "#0c0f16"
                        Loader { asynchronous: true; anchors.fill: parent; active: navListIndex === 2; source: active ? "SettingsPage.qml" : "" } }

                    // ========== DOWNLOAD PROGRESS PAGE ==========
                    Rectangle {
                        anchors.fill: parent; color: hasCustomBg ? "transparent" : "#0c0f16"
                        opacity: navListIndex >= 3 && navModel.get(navListIndex).pageKey === "download_progress" ? 1 : 0
                        visible: opacity > 0
                        Behavior on opacity { NumberAnimation { duration: 350; easing.type: Easing.InOutCubic } }
                        Loader { id: progressLoader; asynchronous: true; anchors.fill: parent; active: navListIndex >= 3; source: active ? "DownloadProgressPage.qml" : ""; onLoaded: { progressLoader.item.backend = backend; progressLoader.item.toastManager = toastManager } }
                    }

                    // ========== VERSION SELECT OVERLAY ==========
                    Loader {
                        id: versionSelectLoader
                        asynchronous: true
                        anchors.fill: parent; z: 5
                        property bool _keepActive: false
                        active: showVersionSelect || _keepActive
                        source: active ? "VersionSelectOverlay.qml" : ""
                        opacity: showVersionSelect ? 1 : 0
                        visible: opacity > 0
                        Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
                        onLoaded: {
                            _keepActive = true
                            item.backend = backend
                            item.toastManager = toastManager
                        }
                        onVisibleChanged: {
                            if (!visible && !showVersionSelect) {
                                _keepActive = false
                            }
                            if (item) {
                                item.visible = visible
                            }
                        }
                    }
                    // ========== VERSION SETTINGS OVERLAY ==========
                    Loader {
                        id: versionSettingsLoader
                        anchors.fill: parent; z: 5
                        property bool _keepActive: false
                        active: showVersionSettings || _keepActive
                        source: active ? "VersionSettingsOverlay.qml" : ""
                        opacity: showVersionSettings ? 1 : 0
                        visible: opacity > 0
                        Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
                        onLoaded: {
                            _keepActive = true
                            item.backend = backend
                            item.toastManager = toastManager
                            item.confirmDialog = confirmDialog
                        }
                        onVisibleChanged: {
                            if (!visible && !showVersionSettings) {
                                _keepActive = false
                            }
                            if (item) {
                                item.visible = visible
                            }
                        }
                    }



            }
        }
    }

    // ════════════════════════════════════════════
    //  Install Page Overlay (top-level, covers entire window)
    // ════════════════════════════════════════════
    Rectangle {
        id: installPageOverlay
        anchors.fill: parent; color: hasCustomBg ? "transparent" : "#0c0f16"; z: 21
        opacity: 0
        visible: true
        Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }

        Loader {
            id: installPageLoader
            anchors.fill: parent
            property bool _keepActive: false
            active: showInstallPage || _keepActive
            source: active ? "InstallPage.qml" : ""
            onLoaded: {
                _keepActive = true
                if (backend) backend.logMessage("[install] Loader onLoaded")
                if (item) {
                    item.backend = backend
                    item.mcVersion = installMcVersion
                    item.toastManager = toastManager
                    item.goBack.connect(function() { showInstallPage = false })
                    item.navigateToProgress.connect(function() {
                        showInstallPage = false
                        if (!downloadNavVisible) {
                            downloadNavVisible = true
                            navModel.append({ label: "下载进度", pageKey: "download_progress", icon: "package" })
                        }
                        switchPage(navModel.count - 1)
                    })
                    item.requestMinimize.connect(function() { appWindow.showMinimized() })
                    item.requestClose.connect(function() { appWindow.close() })
                }
            }
            Connections {
                target: appWindow
                function onShowInstallPageChanged() {
                    if (appWindow.showInstallPage) {
                        overlayFadeInTimer.start()
                        pageContainer.opacity = 0.15
                        if (installPageLoader.item) {
                            installPageLoader.item.mcVersion = appWindow.installMcVersion
                        }
                    } else {
                        installPageOverlay.opacity = 0
                        pageFadeInTimer.start()
                        installUnloadTimer.start()
                    }
                }
            }
        }
    }

    Timer {
        id: overlayFadeInTimer
        interval: 120  // page dims first, then overlay fades in
        onTriggered: { installPageOverlay.opacity = 1.0 }
    }

    Timer {
        id: pageFadeInTimer
        interval: 300  // Overlay almost gone
        onTriggered: { pageFadeInAnim.start() }
    }

    Timer {
        id: installUnloadTimer
        interval: 500
        repeat: false
        onTriggered: { if (!showInstallPage) installPageLoader._keepActive = false }
    }

    Loader {
        id: launchOverlayLoader; anchors.fill: parent; z: 20
        source: "LaunchOverlay.qml"
        active: true  // Always loaded for smooth hide animation
        onLoaded: {
            if (item) {
                item.backend = backend
                item.toastManager = toastManager
                }
        }
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

    Connections {
        target: backend; enabled: backend !== null
        function onLogMessage(msg) { console.log("[backend]", msg) }
        function onRunningCountChanged() {
            appWindow.runningListModel = backend ? backend.runningGames() : []
            console.log("[main] runningCountChanged → list refreshed: " + appWindow.runningListModel.length + " games")
        }
        function onCrashDetected(report) {
            console.log("[crash] crashDetected signal received:", JSON.stringify(report))
            crashDialogLoader.active = true
            if (crashDialogLoader.item) crashDialogLoader.item.crashData = report
        }
    }

    // Confirm Dialog (lazy-loaded — only builds SceneGraph when shown)
    Loader {
        id: confirmDialogLoader
        active: false; asynchronous: true
        anchors.fill: parent; z: 399
        source: "ConfirmDialog.qml"

        // Proxy for backward compatibility — external files use confirmDialog.xxx
        function open(title, message, onAccept) {
            _pendingTitle = title
            _pendingMessage = message
            _pendingOnAccept = onAccept
            active = true
            if (item) {
                item.title = title
                item.message = message
                item.onAccept = onAccept
                item.opened = true
            }
        }
        function close() {
            if (item) item.opened = false
            active = false
        }
        onItemChanged: {
            if (item) {
                item.closed.connect(function() { active = false })
                // Re-apply pending props if open() was called before item ready
                if (_pendingTitle !== "") {
                    item.title = _pendingTitle
                    item.message = _pendingMessage
                    item.onAccept = _pendingOnAccept
                    item.opened = true
                    _pendingTitle = ""
                }
            }
        }
    }

    // Compatibility object — other files still access confirmDialog.xxx
    property QtObject confirmDialog: QtObject {
        property string title: ""
        property string message: ""
        property var onAccept: null
        property bool visible: false
        onVisibleChanged: {
            if (visible) {
                confirmDialogLoader.open(title, message, onAccept)
            } else {
                confirmDialogLoader.close()
            }
        }
    }

    // Pending state for when Loader hasn't created item yet
    property string _pendingTitle: ""
    property string _pendingMessage: ""
    property var _pendingOnAccept: null

    Component.onCompleted: {
        // Override confirmDialog.visible setter to handle pending
        // (QML proxy object already handles synchronization)
    }
}

    // Mod download error dialog
    Rectangle {
        id: modDlErrorDialog; z: 400
        anchors.centerIn: parent; width: 400; height: 200; radius: 8
        color: "#141820"; border.color: "#3a1a1a"; border.width: 1
        visible: showModDlError
        Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
        ColumnLayout {
            anchors.fill: parent; anchors.margins: 20; spacing: 12
            Text { text: "⚠ 下载失败"; font.pixelSize: 15; font.bold: true; color: "#e06060" }
            Text { text: modDlErrorInfo.displayName || ""; font.pixelSize: 13; color: "#c0c8e0" }
            Text {
                Layout.fillWidth: true
                text: modDlErrorInfo.errorDetail || "未知错误"
                color: "#d08080"; font.pixelSize: 11; wrapMode: Text.WordWrap
            }
            RowLayout {
                spacing: 10; Layout.alignment: Qt.AlignRight
                Rectangle {
                    width: 80; height: 30; radius: 6
                    color: skipHov.hovered ? "#3a1818" : "#2a1010"
                    border.color: skipHov.hovered ? "#803838" : "#502020"
                    Text { anchors.centerIn: parent; text: "跳过"; color: "#c06060"; font.pixelSize: 12 }
                    MouseArea { id: skipHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: { backend.cancelModFileDownload(modDlErrorInfo.dlId || 0); showModDlError = false }
                    }
                }
                Rectangle {
                    width: 80; height: 30; radius: 6
                    color: retryHov.hovered ? "#3a3020" : "#2a2010"
                    border.color: retryHov.hovered ? "#907030" : "#604820"
                    Text { anchors.centerIn: parent; text: "重试"; color: "#e0a040"; font.pixelSize: 12 }
                    MouseArea { id: retryHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: { backend.retryModFileDownload(modDlErrorInfo.dlId || 0); showModDlError = false }
                    }
                }
            }
        }
    }
    Rectangle {
        anchors.fill: parent; z: 399; color: "#000000"
        opacity: showModDlError ? 0.5 : 0
        visible: showModDlError
        Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
        MouseArea { anchors.fill: parent; onClicked: { showModDlError = false } }
    }

    // Crash detection dialog (lazy-loaded)
    Loader {
        id: crashDialogLoader; asynchronous: true; active: false
        anchors.fill: parent; z: 500
        source: "CrashDialog.qml"
    }

    // ═══ Toast notification system ═══
    ToastManager {
        id: toastManager
        anchors.fill: parent
        z: 9999
    }
    } // ── end rounded Rectangle
}
