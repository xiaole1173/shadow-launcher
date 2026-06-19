import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

Rectangle {
    id: page
    color: "#0c0f16"

    // Reference back to the main window (set by Loader onLoaded)
    property var mainWindow: null

    // Signal for the flying ball animation — emitted to main window
    signal triggerDownloadBall(real sourceX, real sourceY)

    // Category tabs
    property int currentTab: 0  // 0=MC版本, 1=Mod, 2=光影, 3=资源包

    // MC Version state
    property string currentFilter: "release"  // release, snapshot, old
    property string selectedVersionId: ""
    property int currentSource: -1  // -1=自动(最优), 0..N=具体镜像索引
    property var mirrorSources: []  // 从 backend.getMirrorSources() 加载
    property string complianceNotice: ""  // BMCLAPI 协议合规文案

    // Mod state
    property string modSearchQuery: ""
    property string modLoader: "fabric"
    property var modSearchResults: []
    property bool modResultsReady: false

    // Install state
    property bool installingVersion: false
    property bool installingMod: false
    property string installingModName: ""

    // Resource pack state
    property string rpGameVersion: ""  // "" = 全部, 默认不筛选版本
    property string rpDownloadDir: ""
    property bool rpResultsReady: false

    function loadResourcepackResults() {
        if (!backend) return
        console.log("[resourcepack] auto-loading popular packs, ver:", page.rpGameVersion)
        backend.searchResourcepacks("", page.rpGameVersion)
    }  // user-selected target folder

    signal goBack()

    function loadModResults() {
        if (!backend) return
        modResultsModel.clear()
        var popular = backend.getPopularMods(modLoader)
        if (popular && popular.length) {
            for (var i = 0; i < popular.length; i++) {
                var p = popular[i]
                modResultsModel.append({
                    slug: p.slug || "",
                    title: p.title || p.slug || "Unknown",
                    desc: p.desc || p.description || "",
                    icon: p.icon || "",
                    downloads: p.downloads || 0
                })
            }
        }
    }

    onCurrentTabChanged: {
        if (currentTab === 0) refreshVersionModel()
        if (currentTab === 1 && !modResultsReady) { loadModResults(); modResultsReady = true }
        if (currentTab === 3 && !rpResultsReady) { loadResourcepackResults(); rpResultsReady = true }
    }

    // ──── Animations ────
    opacity: 0
    y: 10
    Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
    Behavior on y { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
    Component.onCompleted: {
        opacity = 1; y = 0
        // Load mirror sources for download source selector
        if (backend) {
            page.mirrorSources = backend.getMirrorSources()
            page.complianceNotice = backend.bmclapiComplianceNotice()
        }
        // Initial load of version list if already available
        refreshVersionModel()
    }

    // ──── Backend connections (global) ────
    Connections {
        target: backend
        enabled: backend !== null

        function onVersionListReady() { refreshVersionModel(); appWindow.pageLoading = false }

        function onResourceDownloadDone(success) {
            page.installingMod = false
            page.installingModName = ""
        }

        function onResourceDownloadProgress(cf, tf, name) {
            page.installingModName = name || ""
        }

        function onInstallFinished(success) {
            page.installingVersion = false
            page.selectedVersionId = ""
        }
    }

    // ──── Helper: categorize versions ────
    property var _versionTypeMap: null
    function getVersionTypeMap() {
        if (_versionTypeMap) return _versionTypeMap
        var map = {}
        if (backend && backend.versionList) {
            var vl = backend.versionList
            for (var i = 0; i < vl.length; i++) {
                map[vl[i].id] = vl[i].type
            }
        }
        _versionTypeMap = map
        return map
    }
    function isSnapshotVersion(v) {
        var map = getVersionTypeMap()
        if (map[v]) return map[v] === "snapshot"
        return v.indexOf("pre") >= 0 || v.indexOf("rc") >= 0 || /^\d{2}w\d{2}[a-z]$/i.test(v)
    }
    function isOldVersion(v) {
        var map = getVersionTypeMap()
        if (map[v]) return map[v] === "old_alpha" || map[v] === "old_beta"
        return v.indexOf("alpha") >= 0 || v.indexOf("beta") >= 0 ||
               v.indexOf("inf") >= 0 || v.indexOf("rd") >= 0 ||
               v.indexOf("a1") >= 0 || v.indexOf("b1") >= 0
    }

    function getVersionType(v) {
        var map = getVersionTypeMap()
        if (map[v]) return map[v]
        if (isSnapshotVersion(v)) return "snapshot"
        if (isOldVersion(v)) return "old"
        return "release"
    }

    function refreshVersionModel() {
        versionModel.clear()
        if (!backend) { appWindow.pageLoading = false; return }
        var list
        if (currentFilter === "snapshot") list = backend.snapshotVersions
        else if (currentFilter === "old") list = backend.oldVersions
        else list = backend.releaseVersions
        if (!list || list.length === 0) { appWindow.pageLoading = false; return }

        // Filter out undefined/null entries
        var cleanList = []
        for (var j = 0; j < list.length; j++) {
            if (list[j] !== undefined && list[j] !== null) {
                cleanList.push(list[j])
            }
        }
        // Batch populate to avoid UI freeze (20 items per tick)
        page._batchList = cleanList
        page._batchIndex = 0
        _batchTimer.restart()
    }

    property var _batchList: []
    property int _batchIndex: 0
    Timer {
        id: _batchTimer
        interval: 1
        repeat: true
        onTriggered: {
            var list = page._batchList
            var start = page._batchIndex
            var end = Math.min(start + 20, list.length)
            for (var i = start; i < end; i++) {
                var vid = list[i]
                if (vid === undefined || vid === null) continue
                versionModel.append({versionId: vid, vtype: page.currentFilter})
            }
            page._batchIndex = end
            if (end >= list.length) {
                _batchTimer.stop()
                appWindow.pageLoading = false
            }
        }
    }

    function getReleaseCount() { return backend ? backend.releaseVersions.length : 0 }
    function getSnapshotCount() { return backend ? backend.snapshotVersions.length : 0 }
    function getOldCount() { return backend ? backend.oldVersions.length : 0 }

    // ──── Tab bar ────
    RowLayout {
        id: tabBar
        anchors.top: parent.top
        anchors.margins: 12
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.topMargin: 8
        height: 38
        spacing: 4

        property var tabLabels: ["MC 版本", "Mod", "光影", "资源包"]
        property var tabIcons: ["", "", "", ""]

        Repeater {
            model: 4
            Rectangle {
                Layout.preferredWidth: 90
                Layout.fillHeight: true
                radius: 8
                color: page.currentTab === index ? "#1a1f2e" : "transparent"
                border.color: page.currentTab === index ? "#3a4eb8" : "transparent"
                border.width: page.currentTab === index ? 1 : 0
                scale: tabMouse.containsMouse ? 1.04 : 1.0
                Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

                Text {
                    anchors.centerIn: parent
                    text: tabBar.tabLabels[index]
                    color: page.currentTab === index ? "#d0d4e0" : "#606478"
                    font.pixelSize: 13
                    font.weight: page.currentTab === index ? Font.DemiBold : Font.Normal
                }

                MouseArea {
                    id: tabMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        page.currentTab = index
                    }
                }
            }
        }
        Item { Layout.fillWidth: true }
    }

    // ──── Divider ────
    Rectangle {
        anchors.top: tabBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        height: 1
        color: "#1a1f2a"
    }

    // ════════════════════════════════════════════
    // TAB 0: MC 版本下载
    // ════════════════════════════════════════════
    Item {
        anchors.top: tabBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.topMargin: 8
        visible: page.currentTab === 0

        // ── Filter pills ──
        RowLayout {
            id: filterRow
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 12
            spacing: 6
            height: 34

            Repeater {
                model: [
                    { key: "release", label: "正式版", countFn: function() { return page.getReleaseCount() } },
                    { key: "snapshot", label: "快照版", countFn: function() { return page.getSnapshotCount() } },
                    { key: "old", label: "远古版", countFn: function() { return page.getOldCount() } }
                ]

                Rectangle {
                    height: 30; radius: 15
                    width: Math.min(pillRow.implicitWidth + 20, 140)
                    color: page.currentFilter === modelData.key ? "#3a4eb8" : "#11141c"
                    border.color: page.currentFilter === modelData.key ? "#3a4eb8" : "#1e2230"
                    border.width: 1
                    clip: true
                    scale: pillMouse.containsMouse ? 1.04 : 1.0
                    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

                    Row {
                        id: pillRow
                        anchors.centerIn: parent
                        spacing: 4
                        Text {
                            id: pillLabel
                            text: modelData.label
                            color: page.currentFilter === modelData.key ? "#e8ecf8" : "#9094a8"
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }
                        Text {
                            id: pillCount
                            text: "(" + modelData.countFn() + ")"
                            color: page.currentFilter === modelData.key ? "#93acf0" : "#505468"
                            font.pixelSize: 11
                            visible: pillRow.width + 20 <= 140
                        }
                    }

                    MouseArea {
                        id: pillMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            page.currentFilter = modelData.key
                            appWindow.pageLoading = true
                            Qt.callLater(refreshVersionModel)
                        }
                    }
                }
            }

            // ── 刷新按钮 ──
            Rectangle {
                width: 28; height: 28; radius: 5
                color: refreshHover.hovered ? "#1a2848" : "transparent"
                border.color: refreshHover.hovered ? "#5068d8" : "#1e2230"
                border.width: 1
                scale: refreshHover.hovered ? 1.08 : 1.0
                Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                visible: page.currentTab === 0
                Text {
                    anchors.centerIn: parent
                    text: "↻"
                    color: refreshHover.hovered ? "#8aa8f0" : "#9094a8"
                    font.pixelSize: 16
                }
                HoverHandler { id: refreshHover }
                ToolTip { visible: refreshHover.hovered; text: "刷新版本列表"; delay: 500 }
                MouseArea {
                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (backend) {
                            toastManager.show("正在刷新...")
                            appWindow.pageLoading = true
                            versionModel.clear()
                            backend.refreshVersionList()
                        }
                    }
                }
            }

            Item { Layout.fillWidth: true }

            // ── Download source selector ──
            RowLayout {
                spacing: 4
                visible: page.currentTab === 0

                // Auto (default)
                Rectangle {
                    height: 26; radius: 5
                    width: multiLabel.implicitWidth + 14
                    color: page.currentSource === -1 ? "#5068d8" : "#11141c"
                    border.color: page.currentSource === -1 ? "#5068d8" : "#1e2230"
                    scale: multiSrcMouse.containsMouse ? 1.04 : 1.0
                    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                    Text { id: multiLabel; anchors.centerIn: parent; text: "自动"; color: page.currentSource === -1 ? "#ffffff" : "#9094a8"; font.pixelSize: 11 }
                    MouseArea { id: multiSrcMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: page.currentSource = -1 }
                }
                // Divider
                Rectangle { width: 1; height: 16; color: "#1e2230" }
                // Dynamic mirror sources from backend
                Repeater {
                    model: page.mirrorSources
                    Rectangle {
                        height: 26; radius: 5
                        width: srcLabel.implicitWidth + 14
                        color: page.currentSource === modelData.index ? "#5068d8" : "#11141c"
                        border.color: page.currentSource === modelData.index ? "#5068d8" : "#1e2230"
                        scale: srcMouse.containsMouse ? 1.04 : 1.0
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        Text { id: srcLabel; anchors.centerIn: parent; text: modelData.name; color: page.currentSource === modelData.index ? "#ffffff" : "#9094a8"; font.pixelSize: 11 }
                        MouseArea { id: srcMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: page.currentSource = modelData.index }
                    }
                }
            }
        }

        // ── Latest version highlight ──
        Rectangle {
            id: latestHighlight
            anchors.top: filterRow.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 12
            anchors.topMargin: 8
            height: 48
            color: "#11141c"
            radius: 8
            border.color: "#1e2230"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                Text {
                    text: "Latest"
                    color: "#3a4eb8"
                    font.pixelSize: 11
                    font.bold: true
                    font.letterSpacing: 1
                }

                Text {
                    text: backend ? backend.versionIds[0] || "" : ""
                    color: "#d0d4e0"
                    font.pixelSize: 15
                    font.bold: true
                }

                Rectangle {
                    width: 1
                    height: 16
                    color: "#1a1f2a"
                }

                Text {
                    text: backend && backend.versionIds.length > 1 ? "snapshot " + backend.versionIds[1] : ""
                    color: "#9094a8"
                    font.pixelSize: 12
                }
            }
        }

        // ── Version list ──
        ListModel { id: versionModel }

        ScrollView {
            anchors.top: latestHighlight.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.topMargin: 8
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            anchors.bottomMargin: 8
            clip: true
            ScrollBar.vertical.policy: ScrollBar.AsNeeded

            ListView {
                id: versionList
                anchors.fill: parent
                model: versionModel
                spacing: 2

                delegate: Rectangle {
                    id: versionRow
                    width: versionList.width
                    height: 42
                    color: itemHover.containsMouse ? "#11141c" : "transparent"
                    radius: 6
                    border.color: page.selectedVersionId === model.versionId ? "#3a4eb8" : "transparent"
                    border.width: page.selectedVersionId === model.versionId ? 1 : 0

                    // Entrance animation
                    opacity: 0
                    scale: 1.0
                    Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
                    Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                    Component.onCompleted: {
                        opacity = 1
                    }

                    // Row bounce animation for download feedback
                    SequentialAnimation {
                        id: rowBounceAnim
                        NumberAnimation { target: versionRow; property: "scale"; from: 1.0; to: 1.04; duration: 150; easing.type: Easing.OutCubic }
                        NumberAnimation { target: versionRow; property: "scale"; from: 1.04; to: 1.0; duration: 150; easing.type: Easing.InCubic }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 10

                        Text {
                            text: model.versionId
                            color: "#d0d4e0"
                            font.pixelSize: 13
                            font.weight: Font.Medium
                            Layout.preferredWidth: 130
                        }

                        // Installed badge
                        Rectangle {
                            visible: model.versionId && backend && backend.installedVersions && (backend.installedVersions.indexOf(model.versionId) >= 0)
                            radius: 3; height: 18
                            width: installedTag.implicitWidth + 10
                            color: "#1a3028"
                            Text {
                                id: installedTag
                                anchors.centerIn: parent
                                text: "已安装"
                                color: "#4bc870"; font.pixelSize: 9
                                font.family: "Consolas, monospace"
                            }
                        }

                        Rectangle {
                            radius: 3
                            height: 18
                            width: typeTag.implicitWidth + 12
                            color: model.vtype === "release" ? "#104830" :
                                   (model.vtype === "snapshot" ? "#403010" : "#282828")

                            Text {
                                id: typeTag
                                anchors.centerIn: parent
                                text: model.vtype === "release" ? "正式版" :
                                      (model.vtype === "snapshot" ? "快照" : "旧版")
                                color: model.vtype === "release" ? "#4a8" :
                                       (model.vtype === "snapshot" ? "#b84" : "#999")
                                font.pixelSize: 10
                                font.family: "Consolas, monospace"
                            }
                        }

                        Item { Layout.fillWidth: true }

                        // Per-item download progress bar (visible when this version is installing)
                        Rectangle {
                            visible: backend && backend.installing && backend.installVersionId === model.versionId
                                     && backend.installTotal > 0 && backend.installPhase !== "done"
                            width: 80; height: 4; radius: 2
                            color: "#1a1f2a"
                            Rectangle {
                                height: 4; radius: 2; color: "#5068d8"
                                width: backend && backend.installTotal > 0 ? parent.width * (backend.installProgress / backend.installTotal) : 0
                                Behavior on width { NumberAnimation { duration: 200 } }
                            }
                        }

                        // Install button - using QtQuick.Controls.Button
                        Button {
                            id: installBtn
                            property bool isInstalled: backend && backend.installedVersions && (backend.installedVersions.indexOf(model.versionId) >= 0)
                            property bool isInstallingThis: backend && backend.installing && backend.installVersionId === model.versionId && backend.installPhase !== "done"
                            property bool isDownloadQueued: {
                                if (!backend || !backend.downloadQueue) return false
                                for (var i = 0; i < backend.downloadQueue.length; i++) {
                                    if (backend.downloadQueue[i].versionId === model.versionId) return true
                                }
                                return false
                            }
                            property bool isDownloadActive: {
                                if (!backend || !backend.activeDownloads) return false
                                for (var i = 0; i < backend.activeDownloads.length; i++) {
                                    if (backend.activeDownloads[i].versionId === model.versionId) return true
                                }
                                return false
                            }
                            // Installing/queued takes priority over "已安装"
                            text: (isInstallingThis || isDownloadActive) ? "下载中…" : (isDownloadQueued ? "排队中" : (isInstalled ? "已安装" : "安装"))
                            implicitWidth: isInstalled ? 56 : (isDownloadQueued ? 56 : (isInstallingThis || isDownloadActive ? 56 : 48)); implicitHeight: 24
                            font.pixelSize: 10; font.weight: Font.Medium
                            z: 10
                            flat: true
                            enabled: !isInstalled && !isInstallingThis && !isDownloadQueued && !isDownloadActive
                            contentItem: Text {
                                text: installBtn.text
                                color: installBtn.isInstalled ? "#4bc870" :
                                       (installBtn.isDownloadQueued ? "#e0a040" :
                                       (installBtn.isInstallingThis || installBtn.isDownloadActive ? "#6080e8" :
                                       (installBtn.hovered && installBtn.enabled ? "#ffffff" : "#707888")))
                                font.pixelSize: 10; font.weight: Font.Medium
                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                radius: 4
                                color: installBtn.isInstalled ? "#0a1810" :
                                       (installBtn.isDownloadQueued ? "#1a1800" :
                                       (installBtn.isInstallingThis || installBtn.isDownloadActive ? "#0a1020" :
                                       (installBtn.hovered && installBtn.enabled ? "#5068d8" : "transparent")))
                                border.color: installBtn.isInstalled ? "#1a3028" :
                                              (installBtn.isDownloadQueued ? "#3a3000" :
                                              (installBtn.isInstallingThis || installBtn.isDownloadActive ? "#1a2840" :
                                              (installBtn.hovered && installBtn.enabled ? "#5d6fe0" : "#1e2230")))
                                border.width: 1
                            }
                            onClicked: {
                                console.log("[DownloadPage] Install clicked for " + model.versionId + " source=" + page.currentSource)
                                if (backend) {
                                    backend.installVersion(model.versionId, page.currentSource)
                                    // Row bounce animation
                                    rowBounceAnim.start()
                                    // Show download nav + trigger flying ball via signal (qrc-safe)
                                    if (page.mainWindow) {
                                        page.mainWindow.showDownloadNavSilent()
                                        var gp = installBtn.mapToItem(null, installBtn.width / 2, installBtn.height / 2)
                                        page.triggerDownloadBall(gp.x, gp.y)
                                    }
                                }
                            }
                        }
                    }

                    MouseArea {
                        id: itemHover
                        anchors.fill: parent
                        z: -1  // below Button
                        hoverEnabled: true
                        onClicked: {
                            if (model.versionId) {
                                page.selectedVersionId = model.versionId
                            }
                        }
                    }
                }
                }
            }
        }

        Connections {
            target: backend
            enabled: backend !== null && page.currentTab === 0
            function onVersionListReady() { refreshVersionModel(); appWindow.pageLoading = false }
        }

        // ── Version install progress overlay ──
        Rectangle {
            id: downloadBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 8
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            property bool hasActiveDownload: backend && backend.installing && backend.installPhase !== "done" && backend.installPhase !== "failed"
            property bool hasQueue: backend && backend.downloadQueue && backend.downloadQueue.length > 0
            property bool hasDownloads: hasActiveDownload || hasQueue
            height: hasDownloads ? (hasActiveDownload ? 80 : 52) : 0
            visible: hasDownloads
            color: "#11141c"
            radius: 8
            border.color: "#1e2230"
            border.width: 1
            Behavior on height { NumberAnimation { duration: 200 } }
            clip: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 4

                RowLayout {
                    visible: downloadBar.hasActiveDownload
                    Text {
                        text: "正在安装 " + (backend ? (backend.installVersionId || "") : "")
                        color: "#d0d4e0"
                        font.pixelSize: 13
                        font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: (backend && backend.installTotal > 0)
                            ? Math.round((backend.installProgress / backend.installTotal) * 100) + "%"
                            : "准备中..."
                        color: "#9094a8"
                        font.pixelSize: 12
                    }
                }

                RowLayout {
                    visible: downloadBar.hasQueue || downloadBar.hasActiveDownload
                    Text {
                        text: downloadBar.hasActiveDownload ? "" : "等待下载开始..."
                        color: "#9094a8"
                        font.pixelSize: 11
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: downloadBar.hasQueue ? ("排队: " + backend.downloadQueue.length + " 个版本") : ""
                        color: "#e0a040"
                        font.pixelSize: 11
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 4
                    radius: 2
                    color: "#1a1f2a"

                    Rectangle {
                        height: 4
                        radius: 2
                        color: "#3a4eb8"
                        width: (backend && backend.installTotal > 0)
                            ? parent.width * (backend.installProgress / backend.installTotal)
                            : 0
                        Behavior on width { NumberAnimation { duration: 150 } }
                    }
                }

                Text {
                    text: "当前: " + (backend ? backend.installFile || "" : "")
                    color: "#606478"
                    font.pixelSize: 11
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }

                RowLayout {
                    Item { Layout.fillWidth: true }
                    Rectangle {
                        width: 70
                        height: 24
                        radius: 4
                        color: "transparent"
                        border.color: "#402428"
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: "取消"
                            color: "#c05050"
                            font.pixelSize: 11
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (backend) backend.cancelInstall()
                                page.installingVersion = false
                            }
                        }
                    }
                }
            }
        }

    // ════════════════════════════════════════════
    // TAB 1: Mod 下载
    // ════════════════════════════════════════════
    Item {
        anchors.top: tabBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.topMargin: 8
        visible: page.currentTab === 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8

            // Search bar
            RowLayout {
                Layout.fillWidth: true
                height: 34
                spacing: 8

                // Loader selector
                Row {
                    spacing: 4
                    Repeater {
                        model: ["Fabric", "Forge", "Quilt", "NeoForge"]
                        Rectangle {
                            width: 56
                            height: 28
                            radius: 6
                            color: page.modLoader === modelData.toLowerCase() ? "#3a4eb8" : "#11141c"
                            border.color: page.modLoader === modelData.toLowerCase() ? "#3a4eb8" : "#1e2230"
                            border.width: 1
                            scale: loaderBtnMouse.containsMouse ? 1.05 : 1.0
                            Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

                            Text {
                                anchors.centerIn: parent
                                text: modelData
                                color: page.modLoader === modelData.toLowerCase() ? "#e8ecf8" : "#9094a8"
                                font.pixelSize: 11
                            }

                            MouseArea {
                                id: loaderBtnMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (page.modLoader !== modelData.toLowerCase()) {
                                        page.modLoader = modelData.toLowerCase()
                                        page.modResultsReady = false
                                        modResultsModel.clear()
                                        loadModResults()
                                    }
                                }
                            }
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                // Search input
                Rectangle {
                    width: 200
                    height: 30
                    radius: 6
                    color: "#11141c"
                    border.color: "#1e2230"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 4
                        spacing: 4

                        TextInput {
                            id: searchInput
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            color: "#d0d4e0"
                            verticalAlignment: TextInput.AlignVCenter
                            font.pixelSize: 12
                            onAccepted: {
                                if (backend && text) {
                                    backend.searchMods(text, page.modLoader)
                                }
                            }

                            Text {
                                anchors.fill: parent
                                verticalAlignment: Text.AlignVCenter
                                text: "搜索 Modrinth..."
                                color: "#505468"
                                font.pixelSize: 12
                                visible: !searchInput.text
                            }
                        }

                        Rectangle {
                            width: 28
                            height: 22
                            radius: 4
                            color: searchIconArea.containsMouse ? "#3a4eb8" : "transparent"

                            Text {
                                anchors.centerIn: parent
                                text: "Go"
                                color: searchIconArea.containsMouse ? "#e8ecf8" : "#606478"
                                font.pixelSize: 10
                            }

                            MouseArea {
                                id: searchIconArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (backend && searchInput.text) {
                                        backend.searchMods(searchInput.text, page.modLoader)
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Text {
                text: "推荐 Mod | 来源: Modrinth API | 搜索或点击安装"
                color: "#505468"
                font.pixelSize: 11
            }

            // Results grid
            ListModel { id: modResultsModel }

            // Backend search results
            Connections {
                target: backend
                enabled: backend !== null && page.currentTab === 1
                function onSearchResultsReady(results) {
                    if (results) {
                        page.modSearchResults = results
                        page.modResultsReady = true
                        modResultsModel.clear()
                        for (var i = 0; i < results.length; i++) {
                            var m = results[i]
                            modResultsModel.append({
                                slug: m.slug || "",
                                title: m.title || "",
                                desc: m.desc || m.description || "",
                                icon: m.icon || "",
                                downloads: m.downloads || 0
                            })
                        }
                    }
                }
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                ScrollBar.vertical.policy: ScrollBar.AsNeeded

                GridView {
                    id: modGrid
                    anchors.fill: parent
                    cellWidth: 224
                    cellHeight: 118
                    model: modResultsModel

                    delegate: Rectangle {
                        width: 214
                        height: 110
                        color: cardMouse.containsMouse ? "#11141c" : "#0e1018"
                        radius: 8
                        border.color: cardMouse.containsMouse ? "#3a4eb8" : "#1a1f2a"
                        border.width: 1

                        opacity: 0
                        Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
                        Component.onCompleted: { opacity = 1 }

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 4

                            RowLayout {
                                spacing: 6
                                Rectangle {
                                    width: 28
                                    height: 28
                                    radius: 6
                                    color: "#1a1f2e"

                                    Text {
                                        anchors.centerIn: parent
                                        text: model.title ? model.title[0] : "M"
                                        color: "#6080e8"
                                        font.pixelSize: 14
                                        font.bold: true
                                    }
                                }
                                Text {
                                    text: model.title || ""
                                    color: "#d0d4e0"
                                    font.pixelSize: 13
                                    font.bold: true
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }

                            Text {
                                text: model.desc || "暂无简介"
                                color: "#606478"
                                font.pixelSize: 11
                                elide: Text.ElideRight
                                maximumLineCount: 2
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                            }

                            RowLayout {
                                Text {
                                    visible: model.downloads > 0
                                    text: model.downloads >= 1000000 ? "DL " + (model.downloads / 1000000).toFixed(1) + "M" :
                                          model.downloads >= 1000 ? "DL " + Math.round(model.downloads / 1000) + "K" : ""
                                    color: "#505468"
                                    font.pixelSize: 10
                                }
                                Item { Layout.fillWidth: true }
                                Rectangle {
                                    width: 44
                                    height: 22
                                    radius: 4
                                    color: cardMouse.containsMouse ? "#3a4eb8" : "transparent"
                                    border.color: cardMouse.containsMouse ? "#3a4eb8" : "#1e2230"
                                    border.width: 1

                                    Text {
                                        anchors.centerIn: parent
                                        text: "安装"
                                        color: cardMouse.containsMouse ? "#e8ecf8" : "#606478"
                                        font.pixelSize: 10
                                    }
                                }
                            }
                        }

                        MouseArea {
                            id: cardMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (model.slug && backend) {
                                    page.installingMod = true
                                    page.installingModName = model.title || model.slug
                                    backend.downloadMod(model.slug, page.modLoader)
                                }
                            }
                        }
                    }
                }
            }

            // Mod install progress bar
            Rectangle {
                visible: page.installingMod
                Layout.fillWidth: true
                Layout.preferredHeight: 4
                radius: 2
                color: "#1a1f2a"

                Rectangle {
                    height: 4
                    radius: 2
                    color: "#3a4eb8"
                    width: parent.width * 0.4  // indeterminate-ish
                    NumberAnimation on x {
                        from: 0
                        to: parent.width * 0.6
                        duration: 1000
                        loops: Animation.Infinite
                        running: page.installingMod
                    }
                }
            }
            Text {
                visible: page.installingMod
                text: "正在安装 " + (page.installingModName || "Mod...")
                color: "#606478"
                font.pixelSize: 11
            }
        }
    }

    // ════════════════════════════════════════════
    // TAB 2: 光影下载
    // ════════════════════════════════════════════
    Item {
        anchors.top: tabBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.topMargin: 8
        visible: page.currentTab === 2

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8

            Text {
                text: "热门光影包 | 来源: Modrinth / 社区推荐"
                color: "#505468"
                font.pixelSize: 11
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                ScrollBar.vertical.policy: ScrollBar.AsNeeded

                ListModel { id: shaderModel }

                GridView {
                    id: shaderGrid
                    anchors.fill: parent
                    cellWidth: 224
                    cellHeight: 118

                    model: shaderModel

                    Component.onCompleted: {
                        populateShaderModel()
                    }

                    function populateShaderModel() {
                        shaderModel.clear()
                        var items = []
                        // First try backend
                        if (backend) {
                            var shaders = backend.getShaderList()
                            if (shaders && shaders.length) {
                                items = shaders
                            }
                        }
                        // Fallback: hardcoded popular shaders if backend returned nothing or errored
                        if (!items.length) {
                            items = [
                                { slug: "complementary-reimagined", title: "Complementary Reimagined", desc: "高品质光影，兼容性好" },
                                { slug: "bsl-shaders", title: "BSL Shaders", desc: "经典光影，性能优化出色" },
                                { slug: "seus-ptgi", title: "SEUS PTGI", desc: "光线追踪级画质" },
                                { slug: "sildurs-vibrant-shaders", title: "Sildur's Vibrant", desc: "色彩鲜艳，多档可选" },
                                { slug: "solas-shader", title: "Solas Shader", desc: "轻量级，中低配首选" },
                                { slug: "rethinking-voxels", title: "Rethinking Voxels", desc: "体积光，r0.1 系列" },
                                { slug: "bliss-shader", title: "Bliss Shader", desc: "Chocapic13 继承者" },
                                { slug: "kuda-shaders", title: "KUDA Shaders", desc: "写实风格光影" },
                                { slug: "pastel-shaders", title: "Pastel Shaders", desc: "柔和色彩，低配友好" },
                                { slug: "photon-shader", title: "Photon Shader", desc: "氛围感强" },
                                { slug: "makeup-ultra-fast", title: "MakeUp Ultra Fast", desc: "超快渲染" },
                                { slug: "chocapic13", title: "Chocapic13", desc: "经典光影代表" },
                                { slug: "voyager-shader", title: "Voyager Shader", desc: "现代光影，真实感" },
                                { slug: "noble-shaders", title: "Noble Shaders", desc: "SEUS 系列优化版" },
                                { slug: "luminance-shaders", title: "Luminance", desc: "Natural 风格" }
                            ]
                        }
                        for (var i = 0; i < items.length; i++) {
                            var it = items[i]
                            if (!it) continue
                            shaderModel.append({
                                slug: it.slug || "",
                                title: it.title || "",
                                desc: it.desc || it.description || ""
                            })
                        }
                    }

                    delegate: Rectangle {
                        width: 214
                        height: 110
                        color: cardMouse2.containsMouse ? "#11141c" : "#0e1018"
                        radius: 8
                        border.color: cardMouse2.containsMouse ? "#3a4eb8" : "#1a1f2a"
                        border.width: 1

                        opacity: 0
                        Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
                        Component.onCompleted: { opacity = 1 }

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 4

                            RowLayout {
                                spacing: 6
                                Rectangle {
                                    width: 28
                                    height: 28
                                    radius: 6
                                    color: "#1a1f2e"

                                    Text {
                                        anchors.centerIn: parent
                                        text: model.title ? model.title[0] : "S"
                                        color: "#d4b840"
                                        font.pixelSize: 14
                                        font.bold: true
                                    }
                                }
                                Text {
                                    text: model.title || ""
                                    color: "#d0d4e0"
                                    font.pixelSize: 13
                                    font.bold: true
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }

                            Text {
                                text: model.desc || ""
                                color: "#606478"
                                font.pixelSize: 11
                                elide: Text.ElideRight
                                maximumLineCount: 2
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                            }

                            RowLayout {
                                Item { Layout.fillWidth: true }
                                Rectangle {
                                    width: 44
                                    height: 22
                                    radius: 4
                                    color: cardMouse2.containsMouse ? "#3a4eb8" : "transparent"
                                    border.color: cardMouse2.containsMouse ? "#3a4eb8" : "#1e2230"
                                    border.width: 1

                                    Text {
                                        anchors.centerIn: parent
                                        text: "安装"
                                        color: cardMouse2.containsMouse ? "#e8ecf8" : "#606478"
                                        font.pixelSize: 10
                                    }
                                }
                            }
                        }

                        MouseArea {
                            id: cardMouse2
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                var slug = model.slug || ""
                                if (slug && backend) {
                                    page.installingMod = true
                                    page.installingModName = model.title || slug
                                    backend.downloadShader(slug)
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                visible: page.installingMod
                Layout.fillWidth: true
                Layout.preferredHeight: 4
                radius: 2
                color: "#1a1f2a"

                Rectangle {
                    height: 4
                    radius: 2
                    color: "#d4b840"
                    width: parent.width * 0.4

                    NumberAnimation on x {
                        from: 0
                        to: parent.width * 0.6
                        duration: 1200
                        loops: Animation.Infinite
                        running: page.installingMod
                    }
                }
            }
            Text {
                visible: page.installingMod
                text: "正在安装 " + (page.installingModName || "光影包...")
                color: "#606478"
                font.pixelSize: 11
            }
        }
    }

    // ════════════════════════════════════════════
    // ════════════════════════════════════════════
    // TAB 3: 资源包下载
    // ════════════════════════════════════════════
    Item {
        anchors.top: tabBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.topMargin: 8
        visible: page.currentTab === 3

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8

            // Search bar
            RowLayout {
                Layout.fillWidth: true
                height: 34
                spacing: 8

                // Version filter pill — grouped by major version
                Rectangle {
                    id: rpVerPill
                    height: 26; radius: 13
                    width: rpVerLabel.implicitWidth + 24
                    color: rpVerMouse.containsMouse ? "#1a2848" : "#11141c"
                    border.color: page.rpGameVersion === "" ? "#a0a8c0" : "#c060a0"
                    border.width: 1

                    Text {
                        id: rpVerLabel
                        anchors.centerIn: parent
                        text: page.rpGameVersion === "" ? "全部" : ("MC " + page.rpGameVersion)
                        color: "#d0d4e0"; font.pixelSize: 12; font.weight: Font.Medium
                    }

                    MouseArea {
                        id: rpVerMouse
                        anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: rpVersionMenu.open()
                    }

                    Popup {
                        id: rpVersionMenu
                        y: parent.height + 4; width: 150
                        height: Math.min(rpVerFlick.contentHeight + 8, 260)
                        padding: 0
                        background: Rectangle { color: "#151922"; radius: 8; border.color: "#1e2230" }

                        Flickable {
                            id: rpVerFlick
                            anchors.fill: parent; anchors.margins: 4
                            contentHeight: rpVerInner.implicitHeight; clip: true
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                            ColumnLayout {
                                id: rpVerInner
                                width: parent.width; spacing: 2

                                // "全部" option
                                Rectangle {
                                    Layout.fillWidth: true; height: 28; radius: 4
                                    color: page.rpGameVersion === "" ? "#1a2848" : "transparent"
                                    Text {
                                        anchors.centerIn: parent; text: "全部"; font.pixelSize: 11
                                        color: page.rpGameVersion === "" ? "#c060a0" : "#9094a8"
                                        font.weight: page.rpGameVersion === "" ? Font.Bold : Font.Normal
                                    }
                                    MouseArea {
                                        anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            page.rpGameVersion = ""; rpVersionMenu.close()
                                            console.log("[resourcepack] filter: ALL")
                                            if (backend) backend.searchResourcepacks(rpSearchInput.text || "", "")
                                        }
                                    }
                                }

                                // Major-version grouped list
                                Repeater {
                                    model: {
                                        if (!backend || !backend.versionIds) return ["1.26.2","1.25.5","1.21.10","1.20.6"]
                                        // Group by major version
                                        var seen = new Set()
                                        var groups = []
                                        for (var i = 0; i < backend.versionIds.length; i++) {
                                            var v = backend.versionIds[i]
                                            var major = v.split(".").slice(0, 2).join(".")
                                            if (!seen.has(major)) {
                                                seen.add(major)
                                                groups.push(major)
                                            }
                                            if (groups.length >= 30) break
                                        }
                                        return groups
                                    }
                                    Rectangle {
                                        Layout.fillWidth: true; height: 28; radius: 4
                                        color: modelData === page.rpGameVersion ? "#1a2848" : "transparent"
                                        Text {
                                            anchors.centerIn: parent; text: modelData; font.pixelSize: 11
                                            color: modelData === page.rpGameVersion ? "#c060a0" : "#9094a8"
                                            font.weight: modelData === page.rpGameVersion ? Font.Bold : Font.Normal
                                        }
                                        MouseArea {
                                            anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                page.rpGameVersion = modelData; rpVersionMenu.close()
                                                console.log("[resourcepack] filter:", modelData)
                                                if (backend) backend.searchResourcepacks(rpSearchInput.text || "", modelData)
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                Rectangle {
                    width: 220; height: 30; radius: 6
                    color: "#11141c"; border.color: rpSearchInput.activeFocus ? "#c060a0" : "#1e2230"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10; anchors.rightMargin: 4; spacing: 4

                        TextInput {
                            id: rpSearchInput
                            Layout.fillWidth: true; Layout.fillHeight: true
                            color: "#d0d4e0"; verticalAlignment: TextInput.AlignVCenter; font.pixelSize: 12
                            onAccepted: {
                                if (backend) {
                                    console.log("[resourcepack] search:", text, "ver:", page.rpGameVersion)
                                    backend.searchResourcepacks(text, page.rpGameVersion)
                                }
                            }

                            Text {
                                anchors.fill: parent; verticalAlignment: Text.AlignVCenter
                                text: "搜索 Modrinth 资源包..."; color: "#505468"; font.pixelSize: 12
                                visible: !rpSearchInput.text
                            }
                        }

                        Rectangle {
                            width: 28; height: 22; radius: 4
                            color: rpGoHov.hovered ? "#c060a0" : "transparent"
                            Text {
                                anchors.centerIn: parent; text: "Go"
                                color: rpGoHov.hovered ? "#e8ecf8" : "#9094a8"; font.pixelSize: 10
                            }
                            MouseArea {
                                id: rpGoHov
                                anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (backend && rpSearchInput.text) {
                                        console.log("[resourcepack] search:", rpSearchInput.text)
                                        backend.searchResourcepacks(rpSearchInput.text, page.rpGameVersion)
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Text {
                text: "资源包 | 来源: Modrinth API | " + (rpResultsModel.count || 0) + " 个结果"
                color: "#505468"; font.pixelSize: 11
            }

            // ── Results: vertical full-width cards ──
            ScrollView {
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                ScrollBar.vertical.policy: ScrollBar.AsNeeded

                ListView {
                    id: rpListView
                    anchors.fill: parent; spacing: 6
                    model: rpResultsModel

                    delegate: Rectangle {
                        width: rpListView.width - 8; height: 108
                        color: rpCardHov.hovered ? "#121620" : "#0e1018"
                        radius: 10; border.color: rpCardHov.hovered ? "#c060a0" : "#1a1f2a"; border.width: 1

                        opacity: 0
                        Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
                        Component.onCompleted: { opacity = 1 }

                        RowLayout {
                            anchors.fill: parent; anchors.margins: 10; spacing: 10

                            // Icon
                            Rectangle {
                                width: 48; height: 48; radius: 10; color: "#1a1f2e"
                                Layout.preferredWidth: 48

                                Image {
                                    anchors.fill: parent; anchors.margins: 2
                                    source: model.icon || ""; fillMode: Image.PreserveAspectFit
                                    // Fallback: first letter if no icon
                                    onStatusChanged: {
                                        if (status === Image.Error || status === Image.Null) {
                                            source = ""
                                        }
                                    }
                                }

                                Text {
                                    anchors.centerIn: parent
                                    text: model.title ? model.title[0] : "R"
                                    color: "#c060a0"; font.pixelSize: 18; font.bold: true
                                    visible: !model.icon || model.icon === ""
                                }
                            }

                            // Content column
                            ColumnLayout {
                                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 4

                                // Title + download count
                                RowLayout {
                                    Text {
                                        text: model.title || ""; color: "#d0d4e0"
                                        font.pixelSize: 14; font.bold: true; elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                    Text {
                                        visible: model.downloads >= 0
                                        text: model.downloads >= 1000000 ? "↓" + (model.downloads / 1000000).toFixed(1) + "M" :
                                              model.downloads >= 1000 ? "↓" + (Math.round(model.downloads / 100) / 10).toFixed(1) + "K" : "↓" + model.downloads
                                        color: "#788090"; font.pixelSize: 11
                                    }
                                }

                                // Description
                                Text {
                                    text: model.desc || "暂无简介"; color: "#606478"; font.pixelSize: 11
                                    elide: Text.ElideRight; maximumLineCount: 2; wrapMode: Text.WordWrap
                                    Layout.fillWidth: true; Layout.fillHeight: true
                                }

                                // Version chips
                                RowLayout {
                                    spacing: 4; Layout.fillWidth: true
                                    clip: true
                                    Repeater {
                                        model: {
                                            if (!model) return [{text: "加载中…", color: "#404860"}]
                                            var vers = page.rpVersionCache[model.slug] || []
                                            if (vers.length === 0) return [{text: "加载中…", color: "#404860"}]
                                            var chips = []
                                            var maxChips = Math.min(vers.length, 6)
                                            for (var i = 0; i < maxChips; i++) {
                                                chips.push({text: vers[i], color: "#90a0c8"})
                                            }
                                            if (vers.length > 6) chips.push({text: "+" + (vers.length - 6), color: "#c060a0"})
                                            return chips
                                        }
                                        Rectangle {
                                            height: 18; width: chipText.implicitWidth + 10; radius: 9
                                            color: "#151922"; border.color: modelData.color; border.width: 1
                                            Text {
                                                id: chipText
                                                anchors.centerIn: parent
                                                text: modelData.text; color: modelData.color
                                                font.pixelSize: 9; font.family: "Consolas, monospace"
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        MouseArea {
                            id: rpCardHov
                            anchors.fill: parent
                            hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                console.log("[resourcepack] card clicked:", model.slug)
                                rpDetailSlug = model.slug
                                rpDetailTitle = model.title
                            }
                        }
                    }
                }
            }

            // Download progress bar
            Rectangle {
                visible: page.installingMod
                Layout.fillWidth: true; Layout.preferredHeight: 4; radius: 2; color: "#1a1f2a"
                Rectangle {
                    height: 4; radius: 2; color: "#c060a0"; width: parent.width * 0.4
                    NumberAnimation on x {
                        from: 0; to: parent.width * 0.6; duration: 1200
                        loops: Animation.Infinite; running: page.installingMod
                    }
                }
            }
            Text {
                visible: page.installingMod
                text: "正在安装 " + (page.installingModName || "资源包...")
                color: "#606478"; font.pixelSize: 11
            }
        }
    }

    // ════════════════════════════════════════════
    // Resource pack detail page (full-screen overlay)
    // ════════════════════════════════════════════
    Item {
        id: rpDetailPage
        anchors.fill: parent
        z: 10  // above all other content
        visible: rpDetailSlug !== ""

        Rectangle {
            anchors.fill: parent
            color: "#0c0f16"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                // ← Back button
                Rectangle {
                    Layout.preferredHeight: 32
                    width: rpBackLabel.implicitWidth + 20; height: 30; radius: 6
                    color: rpBackHov.hovered ? "#1a2848" : "transparent"
                    border.color: rpBackHov.hovered ? "#5068c8" : "#1e2230"
                    border.width: 1

                    Row {
                        anchors.centerIn: parent; spacing: 4
                        Text { text: "←"; color: "#9094a8"; font.pixelSize: 14 }
                        Text {
                            id: rpBackLabel
                            text: "返回"; color: "#9094a8"; font.pixelSize: 12
                        }
                    }
                    MouseArea {
                        id: rpBackHov
                        anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            console.log("[resourcepack] detail closed")
                            rpDetailSlug = ""
                        }
                    }
                }

                // Title
                Text {
                    text: rpDetailSlug; color: "#d0d4e0"; font.pixelSize: 18; font.bold: true
                    elide: Text.ElideRight; Layout.fillWidth: true
                }

                Text {
                    text: "所有可用游戏版本 | 大版本分组 | 点击安装"
                    color: "#505468"; font.pixelSize: 11
                }

                // Version list
                ScrollView {
                    Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded

                    ColumnLayout {
                        width: parent.width - 4; spacing: 6

                        // Loaded versions
                        Repeater {
                            model: {
                                var d = page.rpVersionCache
                                if (!d || !d[rpDetailSlug]) return []
                                return d[rpDetailSlug]
                            }
                            delegate: Rectangle {
                                Layout.fillWidth: true; height: 40; radius: 8
                                color: rpDetVerHov.hovered ? "#1a2848" : "#11141c"
                                border.color: rpDetVerHov.hovered ? "#c060a0" : "#1e2230"
                                border.width: 1

                                RowLayout {
                                    anchors.fill: parent; anchors.margins: 10; spacing: 10
                                    Text {
                                        text: modelData; color: "#c060a0"; font.pixelSize: 14; font.bold: true
                                        font.family: "Consolas, monospace"; Layout.fillWidth: true
                                    }
                                    Rectangle {
                                        width: 56; height: 26; radius: 5
                                        color: rpDetVerHov.hovered ? "#c060a0" : "transparent"
                                        border.color: rpDetVerHov.hovered ? "#c060a0" : "#1e2230"
                                        border.width: 1
                                        Text {
                                            anchors.centerIn: parent; text: "安装"
                                            color: rpDetVerHov.hovered ? "#e8ecf8" : "#606478"
                                            font.pixelSize: 10
                                        }
                                    }
                                }
                                MouseArea {
                                    id: rpDetVerHov
                                    anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        console.log("[resourcepack] install from detail:", rpDetailSlug, "ver:", modelData)
                                        page.installingMod = true
                                        page.installingModName = rpDetailSlug
                                        if (backend) backend.downloadResourcepack(rpDetailSlug, modelData)
                                        rpDetailSlug = ""
                                    }
                                }
                            }
                        }

                        // Loading indicator
                        Text {
                            visible: {
                                var d = page.rpVersionCache
                                return !d || !d[rpDetailSlug] || d[rpDetailSlug].length === 0
                            }
                            text: "正在加载版本信息..."
                            color: "#505468"; font.pixelSize: 12
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }
                }
            }
        }
    }

    // ════════════════════════════════════════════
    // Shared state (stands outside Item scope)
    // ════════════════════════════════════════════
    ListModel { id: rpResultsModel }

    property string rpDetailSlug: ""
    property string rpDetailTitle: ""
    property var rpVersionCache: ({})

    Connections {
        target: backend
        enabled: backend !== null && page.currentTab === 3

        function onResourcepackSearchCompleted(results, totalHits) {
            console.log("[resourcepack] search:", results ? results.length : 0, "total:", totalHits)
            if (results && results.length > 0) {
                rpResultsModel.clear()
                var slugs = []
                for (var i = 0; i < results.length; i++) {
                    var r = results[i]
                    slugs.push(r.slug)
                    rpResultsModel.append({
                        slug: r.slug || "",
                        title: r.title || "",
                        desc: r.desc || r.description || "",
                        icon: r.icon || "",
                        downloads: r.downloads || 0
                    })
                }
                console.log("[resourcepack] populating", rpResultsModel.count, "cards, fetching versions for", slugs.length)
                if (backend && slugs.length > 0) {
                    backend.fetchResourcepackVersions(slugs)
                }
            } else {
                console.log("[resourcepack] empty results")
            }
        }

        function onResourcepackSearchFailed(error) {
            console.log("[resourcepack] search FAILED:", error)
            toastManager.show("资源包搜索失败: " + error)
        }

        function onResourcepackDownloadFinished(slug, success, filePath) {
            console.log("[resourcepack] download:", slug, "success:", success, "path:", filePath)
            page.installingMod = false
            page.installingModName = ""
            if (success) toastManager.show("资源包已安装: " + slug)
        }

        function onResourcepackVersionsLoaded(data) {
            console.log("[resourcepack] versions loaded, keys:", data ? Object.keys(data).length : 0)
            if (data) {
                page.rpVersionCache = data
                // Trigger list view refresh by setting a dummy property
                rpResultsModel.setProperty(0, "downloads", rpResultsModel.get(0).downloads)
                if (rpDetailSlug && data[rpDetailSlug]) {
                    console.log("[resourcepack] detail versions:", data[rpDetailSlug])
                }
            }
        }

        function onLogMessage(msg) {
            if (msg.indexOf("[MODRINTH") >= 0) {
                console.log("[resourcepack] " + msg)
            }
        }
    }

    // Show detail when slug is set (triggers version fetch)
    onRpDetailSlugChanged: {
        if (rpDetailSlug && backend) {
            console.log("[resourcepack] detail opened:", rpDetailSlug)
            backend.fetchResourcepackVersions([rpDetailSlug])
        }
    }

    // ── Folder picker ──
    FolderDialog {
        id: rpFolderDialog
        property string slug: ""
        title: "选择资源包安装文件夹"
        currentFolder: backend ? backend.gameDir + "/resourcepacks" : ""

        onAccepted: {
            var dest = selectedFolder.toString().replace("file:///", "")
            if (Qt.platform.os === "windows") dest = dest.replace(/\//g, "\\")
            console.log("[resourcepack] folder selected:", dest)
            page.installingMod = true
            page.installingModName = rpFolderDialog.slug
            if (backend && rpFolderDialog.slug) {
                backend.downloadResourcepack(rpFolderDialog.slug, page.rpGameVersion)
                toastManager.show("开始下载: " + rpFolderDialog.slug)
            }
        }
    }
}
