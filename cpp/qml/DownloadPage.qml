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
    property real rpLoadingProgress: 0  // 0..1 for version fetch progress
    property int rpDebugSeq: 0  // sequence counter for log correlation
    property string rpCategoryFilter: ""  // category key for filtering
    property int rpPage: 0  // current page offset for pagination
    property bool rpLoadingMore: false
    property bool rpHasMore: true
    property int rpTotalHits: 0

    // Fresh search (reset page + clear)
    function loadRpFirstPage() {
        if (!backend) return
        page.rpDebugSeq++
        page.rpPage = 0
        page.rpLoadingMore = false
        page.rpHasMore = true
        page.rpCategoryFilter = ""
        rpResultsModel.clear()
        if (page.mainWindow && page.mainWindow.loadingBar) {
            page.mainWindow.loadingBar.opacity = 1
        }
        var q = rpSearchInput.text || ""
        var ver = page.rpGameVersion || ""
        console.log("[RP-DEBUG]", page.rpDebugSeq, "FIRSTPAGE gv=", ver, "q=", q)
        backend.searchResourcepacks(q, ver, 0)
    }

    // Load next page of resource packs
    function loadNextRpPage() {
        if (!backend || rpLoadingMore || !rpHasMore) return
        rpLoadingMore = true
        page.rpPage++
        var offset = page.rpPage * 20
        var q = rpSearchInput.text || ""
        var ver = page.rpGameVersion || ""
        console.log("[RP-DEBUG] loadNextPage p=", page.rpPage, "offset=", offset)
        backend.searchResourcepacks(q, ver, offset)
    }

    function filterRpResults() {
        loadRpFirstPage()
    }

    function loadResourcepackResults() {
        loadRpFirstPage()
    }

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
        console.log("[RP-DEBUG] onCurrentTabChanged tab=", currentTab, "rpReady=", rpResultsReady)
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

            // ── Filter Card ──
            Rectangle {
                Layout.fillWidth: true; height: 125; radius: 10
                color: "#11141c"; border.color: "#1e2230"

                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 12; spacing: 8

                    // Row 1: 名称 + 来源
                    RowLayout {
                        Layout.fillWidth: true; spacing: 10

                        Text { text: "名称"; color: "#9094a8"; font.pixelSize: 11; Layout.preferredWidth: 32 }

                        Rectangle {
                            Layout.fillWidth: true; height: 28; radius: 5
                            color: "#0c0e14"
                            border.color: rpSearchInput.activeFocus ? "#5068c8" : "#2a3040"
                            border.width: 1

                            TextInput {
                                id: rpSearchInput
                                anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8
                                color: "#d0d4e0"; verticalAlignment: TextInput.AlignVCenter; font.pixelSize: 12
                                onAccepted: loadRpFirstPage()

                                Text {
                                    anchors.fill: parent; verticalAlignment: Text.AlignVCenter
                                    text: "输入资源包名称..."; color: "#505468"; font.pixelSize: 11
                                    visible: !rpSearchInput.text
                                }
                            }
                        }

                        Text { text: "来源"; color: "#9094a8"; font.pixelSize: 11; Layout.preferredWidth: 32 }

                        Rectangle {
                            id: rpSourcePill
                            Layout.preferredWidth: 140; height: 28; radius: 5
                            color: rpSrcHov.hovered ? "#1a2848" : "#0c0e14"
                            border.color: "#2a3040"; border.width: 1

                            RowLayout {
                                anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 4; spacing: 4
                                Text {
                                    Layout.fillWidth: true
                                    text: "Modrinth (MCIM)"; color: "#788db0"; font.pixelSize: 11
                                    elide: Text.ElideRight
                                }
                                Text { text: "▾"; color: "#505468"; font.pixelSize: 8 }
                            }
                            MouseArea {
                                id: rpSrcHov; anchors.fill: parent
                                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { if (rpSourceMenu.visible) rpSourceMenu.close(); else rpSourceMenu.open() }
                            }

                            Popup {
                                id: rpSourceMenu
                                y: parent.height + 4; width: 160
                                padding: 4
                                background: Rectangle { color: "#151922"; radius: 8; border.color: "#1e2230" }
                                ColumnLayout {
                                    width: parent.width - 8; spacing: 2
                                    Repeater {
                                        model: [
                                            { value: "modrinth", label: "Modrinth (MCIM镜像)" },
                                            { value: "modrinth-direct", label: "Modrinth (直连)" }
                                        ]
                                        Rectangle {
                                            Layout.fillWidth: true; height: 26; radius: 4
                                            color: mouse2.hovered ? "#1a2848" : "transparent"
                                            Text {
                                                anchors.centerIn: parent; text: modelData.label
                                                color: "#9094a8"; font.pixelSize: 11
                                            }
                                            MouseArea {
                                                id: mouse2; anchors.fill: parent
                                                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    rpSourceMenu.close()
                                                    // Currently only MCIM is operational
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Row 2: 版本 + 类型
                    RowLayout {
                        Layout.fillWidth: true; spacing: 10

                        Text { text: "版本"; color: "#9094a8"; font.pixelSize: 11; Layout.preferredWidth: 32 }

                        Rectangle {
                            id: rpVerPill
                            Layout.preferredWidth: 120; height: 28; radius: 5
                            color: rpVerHov.hovered ? "#1a2848" : "#0c0e14"
                            border.color: page.rpGameVersion ? "#5068c8" : "#2a3040"; border.width: 1

                            RowLayout {
                                anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 4; spacing: 4
                                Text {
                                    Layout.fillWidth: true
                                    text: page.rpGameVersion ? ("MC " + page.rpGameVersion) : "全部"
                                    color: page.rpGameVersion ? "#8aaeff" : "#788090"; font.pixelSize: 11
                                    elide: Text.ElideRight
                                }
                                Text { text: "▾"; color: "#505468"; font.pixelSize: 8 }
                            }
                            MouseArea {
                                id: rpVerHov; anchors.fill: parent
                                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { if (rpVersionMenu.visible) { rpVersionMenu.close() } else { rpVersionMenu.open() } }
                            }

                            Popup {
                                id: rpVersionMenu
                                y: parent.height + 4; width: 140
                                height: Math.min(rpVerFlick.contentHeight + 8, 220)
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

                                        Rectangle {
                                            Layout.fillWidth: true; height: 26; radius: 4
                                            color: !page.rpGameVersion ? "#1a2848" : "transparent"
                                            Text {
                                                anchors.centerIn: parent; text: "全部"
                                                color: !page.rpGameVersion ? "#5068c8" : "#9094a8"; font.pixelSize: 11
                                                font.weight: !page.rpGameVersion ? Font.Bold : Font.Normal
                                            }
                                            MouseArea {
                                                anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                                onClicked: { page.rpGameVersion = ""; rpVersionMenu.close(); loadRpFirstPage() }
                                            }
                                        }

                                        Repeater {
                                            model: {
                                                if (!backend || !backend.versionIds) return ["1.21.10","1.20.6"]
                                                var seen = new Set()
                                                var groups = []
                                                for (var i = 0; i < backend.versionIds.length; i++) {
                                                    var v = backend.versionIds[i]
                                                    var major = v.split(/[.\-]/).slice(0, 2).join(".")
                                                    if (!seen.has(major)) {
                                                        seen.add(major)
                                                        groups.push(major)
                                                    }
                                                    if (groups.length >= 30) break
                                                }
                                                return groups
                                            }
                                            Rectangle {
                                                Layout.fillWidth: true; height: 26; radius: 4
                                                color: modelData === page.rpGameVersion ? "#1a2848" : "transparent"
                                                Text {
                                                    anchors.centerIn: parent; text: modelData
                                                    color: modelData === page.rpGameVersion ? "#5068c8" : "#9094a8"; font.pixelSize: 11
                                                    font.weight: modelData === page.rpGameVersion ? Font.Bold : Font.Normal
                                                }
                                                MouseArea {
                                                    anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                                    onClicked: {
                                                        page.rpGameVersion = modelData; rpVersionMenu.close()
                                                        loadRpFirstPage()
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Text { text: "类型"; color: "#9094a8"; font.pixelSize: 11; Layout.preferredWidth: 32 }

                        Rectangle {
                            id: rpTypePill
                            Layout.fillWidth: true; height: 28; radius: 5
                            color: rpTypeHov.hovered ? "#1a2848" : "#0c0e14"
                            border.color: page.rpCategoryFilter ? "#5068c8" : "#2a3040"; border.width: 1

                            RowLayout {
                                anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 4; spacing: 4
                                Text {
                                    Layout.fillWidth: true
                                    text: {
                                        var k = page.rpCategoryFilter
                                        var m = { "": "全部", "realistic": "写实风格", "simplistic": "简约风格",
                                            "themed": "主题风格", "vanilla-like": "原版风格", "pvp": "PVP",
                                            "gui": "界面", "font": "字体", "utility": "实用工具",
                                            "decoration": "装饰", "combat": "战斗", "cursed": "猎奇魔改",
                                            "modded": "模组适配", "tweaks": "细节微调" }
                                        return m[k] || (k || "全部")
                                    }
                                    color: page.rpCategoryFilter ? "#8aaeff" : "#788090"; font.pixelSize: 11
                                    elide: Text.ElideRight
                                }
                                Text { text: "▾"; color: "#505468"; font.pixelSize: 8 }
                            }
                            MouseArea {
                                id: rpTypeHov; anchors.fill: parent
                                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { if (rpTypeMenu.visible) { rpTypeMenu.close() } else { rpTypeMenu.open() } }
                            }

                            Popup {
                                id: rpTypeMenu
                                y: parent.height + 4; width: 160
                                height: Math.min(rpTypeFlick.contentHeight + 8, 240)
                                padding: 0
                                background: Rectangle { color: "#151922"; radius: 8; border.color: "#1e2230" }
                                Flickable {
                                    id: rpTypeFlick
                                    anchors.fill: parent; anchors.margins: 4
                                    contentHeight: rpTypeInner.implicitHeight; clip: true
                                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                                    ColumnLayout {
                                        id: rpTypeInner; width: parent.width; spacing: 2
                                        Repeater {
                                            model: [
                                                {key:"", label:"全部"},
                                                {key:"realistic", label:"写实风格"},
                                                {key:"simplistic", label:"简约风格"},
                                                {key:"themed", label:"主题风格"},
                                                {key:"vanilla-like", label:"原版风格"},
                                                {key:"pvp", label:"PVP"},
                                                {key:"gui", label:"界面"},
                                                {key:"font", label:"字体"},
                                                {key:"utility", label:"实用工具"},
                                                {key:"decoration", label:"装饰"},
                                                {key:"combat", label:"战斗"},
                                                {key:"cursed", label:"猎奇魔改"},
                                                {key:"modded", label:"模组适配"},
                                                {key:"tweaks", label:"细节微调"}
                                            ]
                                            Rectangle {
                                                Layout.fillWidth: true; height: 26; radius: 4
                                                color: page.rpCategoryFilter === modelData.key ? "#1a2848" : "transparent"
                                                Text {
                                                    anchors.centerIn: parent; text: modelData.label
                                                    color: page.rpCategoryFilter === modelData.key ? "#5068c8" : "#9094a8"; font.pixelSize: 11
                                                    font.weight: page.rpCategoryFilter === modelData.key ? Font.Bold : Font.Normal
                                                }
                                                MouseArea {
                                                    anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                                    onClicked: {
                                                        page.rpCategoryFilter = modelData.key
                                                        rpTypeMenu.close()
                                                        filterRpResults()
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Row 3: Buttons
                    RowLayout {
                        Layout.fillWidth: true; spacing: 8
                        Item { Layout.fillWidth: true }

                        Rectangle {
                            width: 72; height: 28; radius: 5
                            color: "#5068c8"
                            Text { anchors.centerIn: parent; text: "搜索"; color: "#e8ecf8"; font.pixelSize: 12 }
                            MouseArea {
                                anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: loadRpFirstPage()
                            }
                        }

                        Rectangle {
                            width: 72; height: 28; radius: 5
                            color: rpResetHov.hovered ? "#252a38" : "#151922"
                            border.color: "#2a3040"; border.width: 1
                            visible: page.rpCategoryFilter || page.rpGameVersion || rpSearchInput.text
                            Text { anchors.centerIn: parent; text: "重置"; color: "#9094a8"; font.pixelSize: 12 }
                            MouseArea {
                                id: rpResetHov; anchors.fill: parent
                                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    page.rpCategoryFilter = ""
                                    page.rpGameVersion = ""
                                    rpSearchInput.text = ""
                                    loadRpFirstPage()
                                }
                            }
                        }
                    }
                }
            }

            Text {
                text: "资源包 | 来源: MCIM (mcimirror.top) | " + (rpTotalHits || rpResultsModel.count || 0) + " 个结果"
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
                    cacheBuffer: 200

                    // Footer: load more indicator
                    footer: Rectangle {
                        width: rpListView.width; height: rpHasMore ? 40 : 0
                        color: "transparent"; visible: rpHasMore
                        Text {
                            anchors.centerIn: parent
                            text: rpLoadingMore ? "加载中..." : "加载更多"
                            color: "#5068c8"; font.pixelSize: 11
                        }
                        MouseArea {
                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: { if (!rpLoadingMore && rpHasMore) loadNextRpPage() }
                        }
                    }

                    // Auto-load when scrolled near bottom
                    onContentYChanged: {
                        if (!rpLoadingMore && rpHasMore && rpListView.count > 0) {
                            var bottomEdge = contentHeight - height
                            if (contentY >= bottomEdge - 100) {
                                loadNextRpPage()
                            }
                        }
                    }

                    delegate: Rectangle {
                        width: rpListView.width - 8; height: 105
                        color: rpCardHov.hovered ? "#121620" : "#0e1018"
                        radius: 10; border.color: rpCardHov.hovered ? "#5068c8" : "#1a1f2a"; border.width: 1

                        RowLayout {
                            anchors.fill: parent; anchors.margins: 10; spacing: 10

                            // Icon (network image with MCIM fallback)
                            Rectangle {
                                width: 44; height: 44; radius: 10; color: "#1a1f2e"
                                Layout.preferredWidth: 44; Layout.preferredHeight: 44
                                clip: true

                                Image {
                                    id: rpCardIcon
                                    anchors.fill: parent
                                    source: {
                                        if (!model || !model.icon) return ""
                                        var url = model.icon
                                        // MCIM CDN: mod.mcimirror.top proxies Modrinth CDN with 302 → works
                                        url = url.replace("cdn.modrinth.com", "mod.mcimirror.top")
                                        url = url.replace("cdn-alt.modrinth.com", "mod.mcimirror.top")
                                        return url
                                    }
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true; cache: false
                                    autoTransform: true
                                    sourceSize.width: 88; sourceSize.height: 88
                                    onStatusChanged: {
                                        if (status === Image.Loading) return
                                        rpIconFallback.visible = (status !== Image.Ready)
                                    }
                                }

                                Text {
                                    id: rpIconFallback
                                    anchors.centerIn: parent
                                    text: model ? (model.title ? model.title[0] : "R") : "R"
                                    color: "#5068c8"; font.pixelSize: 18; font.bold: true
                                }
                            }

                            // Content
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 3

                                // Row 1: Title + downloads
                                RowLayout {
                                    Layout.fillWidth: true; spacing: 6
                                    Text {
                                        text: model.title || ""; color: "#d0d4e0"
                                        font.pixelSize: 13; font.bold: true; elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                    Text {
                                        text: {
                                            var d = model ? (model.downloads || 0) : 0
                                            if (d >= 1000000) return "↓" + (d/1000000).toFixed(1) + "M"
                                            if (d >= 1000) return "↓" + (Math.round(d/100)/10).toFixed(1) + "K"
                                            return "↓" + d
                                        }
                                        color: "#788090"; font.pixelSize: 10
                                    }
                                }

                                // Row 2: Chinese tags (imperative, no Repeater binding)
                                Item {
                                    Layout.fillWidth: true; Layout.preferredHeight: 18
                                    clip: true
                                    Row {
                                        id: rpTagRow
                                        spacing: 4
                                        property string tagsJson: ""
                                        property string _tagPending: ""
                                        property var tagMap: ({
                                            "combat":"战斗", "cursed":"猎奇", "decoration":"装饰",
                                            "modded":"模组适配", "realistic":"写实", "simplistic":"简约",
                                            "themed":"主题", "tweaks":"微调", "utility":"实用",
                                            "vanilla-like":"原版", "fantasy":"幻想", "modern":"现代",
                                            "medieval":"中世纪", "futuristic":"未来", "cartoon":"卡通",
                                            "pvp":"PVP", "minigame":"小游戏", "gui":"界面", "font":"字体",
                                            "hd":"高清", "photorealism":"照片", "cute":"可爱",
                                            "dark":"暗色", "light":"亮色", "clean":"简洁"
                                        })
                                        Timer {
                                            id: rpTagTimer; interval: 1
                                            onTriggered: {
                                                var json = rpTagRow._tagPending; rpTagRow._tagPending = ""
                                                for (var i = rpTagRow.children.length - 1; i >= 0; i--) {
                                                    if (rpTagRow.children[i] !== rpTagComp) rpTagRow.children[i].destroy()
                                                }
                                                if (!json || json === "" || json === "[]") return
                                                var tags = []; try { tags = JSON.parse(json) } catch(e) { return }
                                                for (var t = 0; t < Math.min(tags.length, 4); t++) {
                                                    var en = String(tags[t]).toLowerCase()
                                                    rpTagComp.createObject(rpTagRow, {
                                                        "tagLabel": rpTagRow.tagMap[en] || en
                                                    })
                                                }
                                            }
                                        }
                                        onTagsJsonChanged: { rpTagRow._tagPending = tagsJson; rpTagTimer.restart() }
                                        Component {
                                            id: rpTagComp
                                            Rectangle {
                                                height: 16; width: tText.implicitWidth + 10; radius: 4
                                                color: "#151922"; border.color: "#2a3040"; border.width: 1
                                                property string tagLabel: ""
                                                Text {
                                                    id: tText; anchors.centerIn: parent
                                                    text: tagLabel; color: "#788090"; font.pixelSize: 9
                                                }
                                            }
                                        }
                                        Binding {
                                            target: rpTagRow; property: "tagsJson"
                                            value: model ? JSON.stringify(model.categories || []) : ""
                                            when: model !== null
                                        }
                                    }
                                }

                                // Row 3: Description (1 line)
                                Text {
                                    Layout.fillWidth: true
                                    text: model.desc || ""; color: "#505468"; font.pixelSize: 10
                                    elide: Text.ElideRight; maximumLineCount: 1
                                }

                                // Row 4: Version chips + date
                                RowLayout {
                                    Layout.fillWidth: true; spacing: 6
                                    Item {
                                        Layout.fillWidth: true; Layout.preferredHeight: 18
                                        clip: true
                                        Row {
                                            id: rpChipRow
                                            spacing: 3
                                            property string chipsJson: ""
                                            property string _pending: ""
                                            Timer {
                                                id: rpChipTimer; interval: 1
                                                onTriggered: {
                                                    var json = rpChipRow._pending; rpChipRow._pending = ""
                                                    for (var i = rpChipRow.children.length - 1; i >= 0; i--) {
                                                        if (rpChipRow.children[i] !== rpChipComp && rpChipRow.children[i] !== rpChipPlaceholder)
                                                            rpChipRow.children[i].destroy()
                                                    }
                                                    if (!json || json === "") return
                                                    var items = []; try { items = JSON.parse(json) } catch(e) { return }
                                                    rpChipPlaceholder.visible = (items.length === 0)
                                                    for (var j = 0; j < items.length; j++) {
                                                        rpChipComp.createObject(rpChipRow, {
                                                            "chipText": items[j].text, "chipColor": items[j].color
                                                        })
                                                    }
                                                }
                                            }
                                            onChipsJsonChanged: { rpChipRow._pending = chipsJson; rpChipTimer.restart() }
                                            Component {
                                                id: rpChipComp
                                                Rectangle {
                                                    height: 16; width: cText.implicitWidth + 8; radius: 4
                                                    color: "#151922"; border.color: chipColor; border.width: 1
                                                    property string chipColor: "#90a0c8"; property string chipText: ""
                                                    Text {
                                                        id: cText; anchors.centerIn: parent
                                                        text: chipText; color: chipColor; font.pixelSize: 8
                                                        font.family: "Consolas, monospace"
                                                    }
                                                }
                                            }
                                            Rectangle {
                                                id: rpChipPlaceholder; height: 16; width: 48; radius: 4
                                                color: "#151922"; border.color: "#404860"; border.width: 1
                                                Text { anchors.centerIn: parent; text: "加载中"; color: "#404860"; font.pixelSize: 8 }
                                            }
                                            Binding {
                                                target: rpChipRow; property: "chipsJson"
                                                value: model ? (model.chips || "") : ""
                                                when: model !== null
                                            }
                                        }
                                    }
                                    Text {
                                        text: model.updated ? String(model.updated).slice(0, 10) : ""
                                        color: "#404860"; font.pixelSize: 9; visible: model.updated !== undefined
                                    }
                                }
                            }
                        }

                        MouseArea {
                            id: rpCardHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                console.log("[RP-DEBUG] card clicked:", model.slug)
                                rpDetailSlug = model.slug; rpDetailTitle = model.title
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
                    height: 4; radius: 2; color: "#5068c8"; width: parent.width * 0.4
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
                                border.color: rpDetVerHov.hovered ? "#5068c8" : "#1e2230"
                                border.width: 1

                                RowLayout {
                                    anchors.fill: parent; anchors.margins: 10; spacing: 10
                                    Text {
                                        text: modelData; color: "#5068c8"; font.pixelSize: 14; font.bold: true
                                        font.family: "Consolas, monospace"; Layout.fillWidth: true
                                    }
                                    Rectangle {
                                        width: 56; height: 26; radius: 5
                                        color: rpDetVerHov.hovered ? "#5068c8" : "transparent"
                                        border.color: rpDetVerHov.hovered ? "#5068c8" : "#1e2230"
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
    ListModel { id: modResultsModel }
    ListModel { id: rpResultsModel }

    property string rpDetailSlug: ""
    property string rpDetailTitle: ""
    property string installingRpName: ""
    property var rpVersionCache: ({})

    Connections {
        target: backend

        function onResourcepackSearchCompleted(results, totalHits) { console.log('[RP-DEBUG] >>> SIGNAL RECEIVED, enter handler'); try {
            console.log("[RP-DEBUG]", page.rpDebugSeq, "searchCompleted hits=", results ? results.length : 0, "total=", totalHits)
            if (!results || results.length === 0) {
                console.log("[RP-DEBUG]", page.rpDebugSeq, "EMPTY results")
                page.rpHasMore = false
                return
            }
            page.rpTotalHits = totalHits
            page.rpLoadingMore = false

            var isFirstPage = (page.rpPage === 0)
            if (isFirstPage) {
                rpResultsModel.clear()
                page.rpHasMore = (totalHits > 20)
                if (page.mainWindow && page.mainWindow.loadingBar) {
                    page.mainWindow.loadingBar.opacity = 0
                }
            } else {
                page.rpHasMore = ((page.rpPage + 1) * 20 < totalHits)
            }

            var slugs = []
            var catFilter = page.rpCategoryFilter.toLowerCase()
            for (var i = 0; i < results.length; i++) {
                var r = results[i]
                if (catFilter) {
                    var cats = r.categories || []
                    var hasCat = false
                    for (var c = 0; c < cats.length; c++) {
                        if (String(cats[c]).toLowerCase() === catFilter) { hasCat = true; break }
                    }
                    if (!hasCat) continue
                }
                slugs.push(r.slug)
                rpResultsModel.append({
                    slug: r.slug || "",
                    title: r.title || "",
                    desc: r.desc || r.description || "",
                    icon: r.icon || "",
                    downloads: r.downloads || 0,
                    categories: r.categories || [],
                    updated: r.updated || "",
                    author: r.author || "",
                    source: r.source || "Modrinth",
                    chips: ""
                })
            }
            console.log("[RP-DEBUG]", page.rpDebugSeq, "model now", rpResultsModel.count, "/", totalHits)
            if (backend && slugs.length > 0) {
                backend.fetchResourcepackVersions(slugs)
            }
        } catch(e) { console.log('[RP-DEBUG] searchCompleted ERROR:', e.message); page.rpLoadingMore = false }
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
                // Also set chips on model items (for cards already rendered)
                var slugs = Object.keys(data)
                for (var s = 0; s < slugs.length; s++) {
                    var slug = slugs[s]
                    var vers = data[slug]
                    var chips = []
                    var maxChips = Math.min(vers.length, 6)
                    for (var j = 0; j < maxChips; j++) {
                        chips.push({text: vers[j], color: "#90a0c8"})
                    }
                    if (vers.length > 6) chips.push({text: "+" + (vers.length - 6), color: "#5068c8"})
                    var chipsJson = JSON.stringify(chips)
                    for (var i = 0; i < rpResultsModel.count; i++) {
                        if (rpResultsModel.get(i).slug === slug) {
                            rpResultsModel.setProperty(i, "chips", chipsJson)
                            break
                        }
                    }
                }
                if (rpDetailSlug && data[rpDetailSlug]) {
                    console.log("[resourcepack] detail versions:", data[rpDetailSlug])
                }
            }
        }

        function onResourcepackVersionsPartial(slug, versions) {
            console.log("[RP-DEBUG]", page.rpDebugSeq, "PARTIAL", slug, "vers=", versions.length, "modelCount=", rpResultsModel.count)
            var chips = []
            var maxChips = Math.min(versions.length, 6)
            for (var j = 0; j < maxChips; j++) {
                chips.push({text: versions[j], color: "#90a0c8"})
            }
            if (versions.length > 6) chips.push({text: "+" + (versions.length - 6), color: "#5068c8"})
            var chipsJson = JSON.stringify(chips)

            var found = false
            for (var i = 0; i < rpResultsModel.count; i++) {
                if (rpResultsModel.get(i).slug === slug) {
                    console.log("[RP-DEBUG]", page.rpDebugSeq, "SET chips on index", i, "slug=", slug, "chipsJson len=", chipsJson.length)
                    rpResultsModel.setProperty(i, "chips", chipsJson)
                    found = true
                    break
                }
            }
            if (!found) {
                console.log("[RP-DEBUG]", page.rpDebugSeq, "WARN: slug not found in model:", slug)
            }
            page.rpVersionCache[slug] = versions
        }

        function onResourcepackVersionsProgress(done, total) {
            console.log("[RP-DEBUG]", page.rpDebugSeq, "progress", done, "/", total)
            if (page.mainWindow && page.mainWindow.loadingBar) {
                page.mainWindow.loadingBar.opacity = (done < total) ? 1 : 0
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
        console.log("[RP-DEBUG] rpDetailSlugChanged ->", rpDetailSlug)
        if (rpDetailSlug && backend) {
            backend.fetchResourcepackVersions([rpDetailSlug])
            console.log("[RP-DEBUG] detail fetch versions for", rpDetailSlug)
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
            // Do NOT set installingMod — that's for Mod tab progress only
            page.installingRpName = rpFolderDialog.slug
            if (backend && rpFolderDialog.slug) {
                backend.downloadResourcepack(rpFolderDialog.slug, page.rpGameVersion)
                toastManager.show("开始下载: " + rpFolderDialog.slug)
            }
        }
    }
}
