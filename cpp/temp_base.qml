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

    // ── Auto-test helpers (accessible from MainWindow loader.item) ──
    property alias rpDetailExpanded: rpDetailPage.rpDetailExpanded
    function toggleVersionMenu() {
        if (rpVersionMenu.visible) { rpVersionMenu.close() } else { rpVersionMenu.open() }
    }

    // Category tabs
    property int currentTab: 0  // 0=MC版本, 1=Mod, 2=光影, 3=资源包

    // MC Version state
    property string currentFilter: "release"  // release, snapshot, old
    property string selectedVersionId: ""
    property int currentSource: -1  // -1=自动(最优), 0..N=具体镜像索引
    property var mirrorSources: []  // 从 backend.getMirrorSources() 加载
    property string clickedVersionId: ""  // Immediately-set on click, cleared on installPhase done

    // Watch for install completion/reset to clear clickedVersionId
    Connections {
        target: backend; enabled: backend !== null
        function onInstallStateChanged() {
            if (!backend.installing && page.clickedVersionId !== "") {
                console.log("[download-ui] install completed, clearing clickedVersionId: " + page.clickedVersionId)
                page.clickedVersionId = ""
            }
        }
    }
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
    property string rpCategoryFilter: ""   // 类别 filter: combat, realistic, etc.
    property string rpFeatureFilter: ""    // 功能 filter: audio, blocks, etc.
    property string rpResolutionFilter: "" // 分辨率 filter: 16x, 32x, etc.

    // Feature translation map for resource pack detail display
    property var rpFeatureMap: ({
        "audio": "音频", "blocks": "方块", "core-shaders": "核心着色器",
        "entities": "实体", "environment": "环境", "equipment": "装备",
        "fonts": "字体", "gui": "图形界面", "items": "物品",
        "locale": "本地化", "models": "模型", "minecraft": "Minecraft"
    })
    property int rpPage: 0  // current page offset for pagination
    property bool rpLoadingMore: false
    property bool rpShowPreReleases: false
    property int rpVersionCacheVersion: 0
    property bool rpHasMore: true
    property int rpTotalHits: 0

    // Fresh search (reset page + clear)
    function loadRpFirstPage() {
        if (!backend) return
        page.rpDebugSeq++
        page.rpPage = 0
        page.rpLoadingMore = false
        page.rpHasMore = true
        rpResultsModel.clear()
        if (page.mainWindow && page.mainWindow.loadingBar) {
            page.mainWindow.loadingBar.opacity = 1
        }
        var q = rpSearchInput.text || ""
        var ver = page.rpGameVersion || ""
        // Collect all 3 filter dimensions for backend facets
        var cats = []
        if (page.rpCategoryFilter) cats.push(page.rpCategoryFilter)
        if (page.rpFeatureFilter) cats.push(page.rpFeatureFilter)
        if (page.rpResolutionFilter) cats.push(page.rpResolutionFilter)
        console.log("[RP-DEBUG]", page.rpDebugSeq, "FIRSTPAGE gv=", ver, "q=", q, "cats=", cats)
        backend.searchResourcepacks(q, ver, 0, cats)
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
        page.modSearching = false
        console.log("[mod-ui] loadModResults loader=" + page.modLoader)
        var popular = backend.getPopularMods(page.modLoader)
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
            console.log("[mod-ui] loaded " + popular.length + " popular mods")
        }
    }

    function loadShaderResults() {
        if (!backend) return
        shaderResultsModel.clear()
        page.shaderSearching = false
        console.log("[shader-ui] loadShaderResults")
        var list = backend.getShaderList()
        if (list && list.length) {
            for (var i = 0; i < list.length; i++) {
                var s = list[i]
                shaderResultsModel.append({
                    slug: s.slug || "",
                    title: s.title || s.slug || "Unknown",
                    desc: s.desc || s.description || "",
                    icon: s.icon || "",
                    downloads: s.downloads || 0
                })
            }
            console.log("[shader-ui] loaded " + list.length + " shaders")
        }
    }

    property bool shaderResultsReady: false

    onCurrentTabChanged: {
        console.log("[RP-DEBUG] onCurrentTabChanged tab=", currentTab, "rpReady=", rpResultsReady)
        if (currentTab === 0) refreshVersionModel()
        if (currentTab === 1 && !modResultsReady) { loadModResults(); modResultsReady = true }
        if (currentTab === 2 && !shaderResultsReady) { loadShaderResults(); shaderResultsReady = true }
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

        function onSearchResultsReady(results) {
            console.log("[mod-ui] searchResultsReady results=" + (results ? results.length : 0))
            modResultsModel.clear()
            page.modSearching = false
            if (results && results.length > 0) {
                for (var i = 0; i < results.length; i++) {
                    var r = results[i]
                    modResultsModel.append({
                        slug: r.slug || "",
                        title: r.title || r.slug || "Unknown",
                        desc: r.desc || r.description || "",
                        icon: r.icon || "",
                        downloads: r.downloads || 0
                    })
                }
            }
            console.log("[mod-ui] model now has " + modResultsModel.count + " items")
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
                            property bool isInstallingThis: backend && backend.installing && backend.installPhase !== "done" && (backend.installVersionId === model.versionId || page.clickedVersionId === model.versionId)
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
                            enabled: !isInstalled && !isInstallingThis && !isDownloadQueued && !isDownloadActive && page.clickedVersionId !== model.versionId
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
                                    // Log pre-install state
                                    console.log("[download-ui] pre-install: version=" + model.versionId + " installing=" + backend.installing + " versionId=" + backend.installVersionId + " phase=" + backend.installPhase)
                                    // Immediately mark this version as clicked so UI updates before page destruction
                                    page.clickedVersionId = model.versionId
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

                // FIX 5: Show verify status when verifying
                RowLayout {
                    visible: downloadBar.hasActiveDownload
                    Text {
                        text: (backend && backend.installPhase === "校验中...")
                            ? "检验完整性 " + (backend ? (backend.installVersionId || "") : "")
                            : "正在安装 " + (backend ? (backend.installVersionId || "") : "")
                        color: (backend && backend.installPhase === "校验中...") ? "#8aaeff" : "#d0d4e0"
                        font.pixelSize: 13
                        font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: {
                            if (!backend || backend.installTotal <= 0) return "准备中..."
                            if (backend.installPhase === "校验中...")
                                return backend.installProgress + "/" + backend.installTotal
                            return Math.round((backend.installProgress / backend.installTotal) * 100) + "%"
                        }
                        color: (backend && backend.installPhase === "校验中...") ? "#8aaeff" : "#9094a8"
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

            // ── Header row ──
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: "热门 Mod 推荐"
                    color: "#d0d4e0"; font.pixelSize: 14; font.bold: true
                    Layout.fillWidth: true
                }

                // Loader selector
                Rectangle {
                    id: modLoaderPill
                    Layout.preferredWidth: 100; height: 28; radius: 5
                    color: modLdrHov.hovered ? "#1a2848" : "#0c0e14"
                    border.color: "#2a3040"; border.width: 1

                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 4; spacing: 4
                        Text {
                            Layout.fillWidth: true
                            text: page.modLoader === "fabric" ? "Fabric" : "Forge"
                            color: "#5068c8"; font.pixelSize: 11; font.bold: true
                        }
                        Text { text: "▾"; color: "#505468"; font.pixelSize: 8 }
                    }
                    MouseArea {
                        id: modLdrHov; anchors.fill: parent
                        hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: { if (modLoaderMenu.visible) modLoaderMenu.close(); else modLoaderMenu.open() }
                    }

                    Popup {
                        id: modLoaderMenu; closePolicy: Popup.NoAutoClose
                        y: parent.height + 4; width: 100
                        padding: 4
                        background: Rectangle { color: "#151922"; radius: 8; border.color: "#1e2230" }
                        ColumnLayout {
                            width: parent.width - 8; spacing: 2
                            Repeater {
                                model: ["fabric", "forge"]
                                Rectangle {
                                    Layout.fillWidth: true; height: 26; radius: 4
                                    color: modelData === page.modLoader ? "#1a2848" : "transparent"
                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData === "fabric" ? "Fabric" : "Forge"
                                        color: modelData === page.modLoader ? "#5068c8" : "#9094a8"; font.pixelSize: 11
                                        font.weight: modelData === page.modLoader ? Font.Bold : Font.Normal
                                    }
                                    MouseArea {
                                        anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            page.modLoader = modelData
                                            modLoaderMenu.close()
                                            modResultsModel.clear()
                                            page.modResultsReady = false
                                            loadModResults()
                                            page.modResultsReady = true
                                            console.log("[mod-ui] loader switched to " + modelData)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ── Search bar ──
            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Rectangle {
                    Layout.fillWidth: true; height: 30; radius: 5
                    color: "#0c0e14"
                    border.color: modSearchInput.activeFocus ? "#5068c8" : "#2a3040"
                    border.width: 1

                    TextInput {
                        id: modSearchInput
                        anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8
                        color: "#d0d4e0"; verticalAlignment: TextInput.AlignVCenter; font.pixelSize: 12

                        Text {
                            anchors.fill: parent; verticalAlignment: Text.AlignVCenter
                            text: "搜索 Mod..."; color: "#505468"; font.pixelSize: 11
                            visible: !modSearchInput.text
                        }
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 72; height: 30; radius: 5
                    color: modSearchBtn.hovered ? "#5a78e0" : "#5068c8"

                    Text {
                        anchors.centerIn: parent
                        text: "搜索"
                        color: "#ffffff"; font.pixelSize: 12; font.bold: true
                    }
                    MouseArea {
                        id: modSearchBtn; anchors.fill: parent
                        hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            var q = modSearchInput.text.trim()
                            if (!q) return
                            console.log("[mod-ui] search query=\"" + q + "\" loader=" + page.modLoader)
                            page.modSearching = true
                            backend.searchMods(q, page.modLoader)
                        }
                    }
                }
            }

            // ── Scrollable results area ──
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Flickable {
                    id: modFlick
                    anchors.fill: parent
                    contentHeight: modGrid.implicitHeight
                    clip: true
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    ColumnLayout {
                        id: modGrid
                        width: parent.width
                        spacing: 6

                        // Loading state
                        Text {
                            visible: page.modSearching
                            Layout.fillWidth: true
                            text: "搜索中…"
                            color: "#606478"; font.pixelSize: 12
                            horizontalAlignment: Qt.AlignHCenter
                            Layout.topMargin: 20
                        }

                        // Empty state
                        Text {
                            visible: !page.modSearching && modResultsModel.count === 0
                            Layout.fillWidth: true
                            text: "未找到匹配的Mod"
                            color: "#606478"; font.pixelSize: 12
                            horizontalAlignment: Qt.AlignHCenter
                            Layout.topMargin: 20
                        }

                        // Results cards
                        Repeater {
                            model: modResultsModel

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 56
                                radius: 8
                                color: "#161922"
                                border.color: "#1e2230"

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 10

                                    // Icon
                                    Rectangle {
                                        Layout.preferredWidth: 32; Layout.preferredHeight: 32; radius: 6
                                        color: "#0c0e14"
                                        Text {
                                            anchors.centerIn: parent
                                            text: "[修复]"
                                            font.pixelSize: 16
                                        }
                                    }

                                    // Info
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        spacing: 2
                                        Layout.alignment: Qt.AlignVCenter

                                        Text {
                                            text: model.title || "Unknown"
                                            color: "#d0d4e0"; font.pixelSize: 13; font.bold: true
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            text: model.desc || ""
                                            color: "#606478"; font.pixelSize: 11
                                            elide: Text.ElideRight
                                            maximumLineCount: 1
                                        }
                                        Text {
                                            text: "下载: " + (model.downloads || 0)
                                            color: "#5068c8"; font.pixelSize: 10
                                        }
                                    }

                                    // Install button
                                    Rectangle {
                                        Layout.preferredWidth: 60; height: 26; radius: 5
                                        color: {
                                            if (page.modDownloadingSlugs[model.slug])
                                                return "#2a3040"
                                            if (installBtn.containsMouse)
                                                return "#5a78e0"
                                            return "#5068c8"
                                        }

                                        Text {
                                            anchors.centerIn: parent
                                            text: page.modDownloadingSlugs[model.slug] ? "下载中…" : "安装"
                                            color: page.modDownloadingSlugs[model.slug] ? "#606478" : "#ffffff"
                                            font.pixelSize: 11
                                        }
                                        MouseArea {
                                            id: installBtn
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                if (page.modDownloadingSlugs[model.slug]) return
                                                console.log("[mod-ui] download slug=\"" + model.slug + "\" loader=" + page.modLoader)
                                                page.modDownloadingSlugs[model.slug] = true
                                                page.modDownloadingSlugs = Object.assign({}, page.modDownloadingSlugs)
                                                backend.downloadMod(model.slug, page.modLoader)
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

    // ════════════════════════════════════════════
    // ════════════════════════════════════════════
    // TAB 2: 光影
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

            // ── Header ──
            Text {
                Layout.fillWidth: true
                text: "热门光影推荐"
                color: "#d0d4e0"; font.pixelSize: 14; font.bold: true
            }

            // ── Search bar ──
            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Rectangle {
                    Layout.fillWidth: true; height: 30; radius: 5
                    color: "#0c0e14"
                    border.color: shaderSearchInput.activeFocus ? "#5068c8" : "#2a3040"
                    border.width: 1

                    TextInput {
                        id: shaderSearchInput
                        anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8
                        color: "#d0d4e0"; verticalAlignment: TextInput.AlignVCenter; font.pixelSize: 12

                        Text {
                            anchors.fill: parent; verticalAlignment: Text.AlignVCenter
                            text: "搜索光影..."; color: "#505468"; font.pixelSize: 11
                            visible: !shaderSearchInput.text
                        }
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 72; height: 30; radius: 5
                    color: shaderSearchBtn.hovered ? "#5a78e0" : "#5068c8"

                    Text {
                        anchors.centerIn: parent
                        text: "搜索"
                        color: "#ffffff"; font.pixelSize: 12; font.bold: true
                    }
                    MouseArea {
                        id: shaderSearchBtn; anchors.fill: parent
                        hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            var q = shaderSearchInput.text.trim()
                            if (!q) return
                            console.log("[shader-ui] search query=\"" + q + "\"")
                            page.shaderSearching = true
                            if (backend.modManager && backend.modManager.searchMods)
                                backend.modManager.searchMods(q, page.modLoader)
                        }
                    }
                }
            }

            // ── Results area ──
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Flickable {
                    id: shaderFlick
                    anchors.fill: parent
                    contentHeight: shaderGrid.implicitHeight
                    clip: true
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    ColumnLayout {
                        id: shaderGrid
                        width: parent.width
                        spacing: 6

                        // Loading state
                        Text {
                            visible: page.shaderSearching
                            Layout.fillWidth: true
                            text: "搜索中…"
                            color: "#606478"; font.pixelSize: 12
                            horizontalAlignment: Qt.AlignHCenter
                            Layout.topMargin: 20
                        }

                        // Empty state
                        Text {
                            visible: !page.shaderSearching && shaderResultsModel.count === 0
                            Layout.fillWidth: true
                            text: "未找到匹配的光影"
                            color: "#606478"; font.pixelSize: 12
                            horizontalAlignment: Qt.AlignHCenter
                            Layout.topMargin: 20
                        }

                        // Results cards
                        Repeater {
                            model: shaderResultsModel

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 56
                                radius: 8
                                color: "#161922"
                                border.color: "#1e2230"

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 10

                                    // Icon
                                    Rectangle {
                                        Layout.preferredWidth: 32; Layout.preferredHeight: 32; radius: 6
                                        color: "#0c0e14"
                                        Text {
                                            anchors.centerIn: parent
                                            text: "[山景]"
                                            font.pixelSize: 16
                                        }
                                    }

                                    // Info
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        spacing: 2
                                        Layout.alignment: Qt.AlignVCenter

                                        Text {
                                            text: model.title || "Unknown"
                                            color: "#d0d4e0"; font.pixelSize: 13; font.bold: true
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            text: model.desc || ""
                                            color: "#606478"; font.pixelSize: 11
                                            elide: Text.ElideRight
                                            maximumLineCount: 1
                                        }
                                        Text {
                                            text: "下载: " + (model.downloads || 0)
                                            color: "#5068c8"; font.pixelSize: 10
                                        }
                                    }

                                    // Download button
                                    Rectangle {
                                        Layout.preferredWidth: 60; height: 26; radius: 5
                                        color: {
                                            if (page.shaderDownloadingSlugs[model.slug])
                                                return "#2a3040"
                                            if (shaderDlBtn.containsMouse)
                                                return "#5a78e0"
                                            return "#5068c8"
                                        }

                                        Text {
                                            anchors.centerIn: parent
                                            text: page.shaderDownloadingSlugs[model.slug] ? "下载中…" : "下载"
                                            color: page.shaderDownloadingSlugs[model.slug] ? "#606478" : "#ffffff"
                                            font.pixelSize: 11
                                        }
                                        MouseArea {
                                            id: shaderDlBtn
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                if (page.shaderDownloadingSlugs[model.slug]) return
                                                console.log("[shader-ui] download slug=\"" + model.slug + "\"")
                                                page.shaderDownloadingSlugs[model.slug] = true
                                                page.shaderDownloadingSlugs = Object.assign({}, page.shaderDownloadingSlugs)
                                                backend.downloadShader(model.slug)
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

    // ════════════════════════════════════════════
    // TAB 3: 资源包
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

            // ── Filter Card ──
            Rectangle {
                Layout.fillWidth: true; height: 150; radius: 10
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
                                // REMOVED onAccepted trigger — search only on button click (Fix 1)

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
                                id: rpSourceMenu; closePolicy: Popup.NoAutoClose
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
                                                    // FIX 4: Only MCIM source is currently operational.
                                                    // modrinth-direct is non-functional — no action on click.
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
                                id: rpVersionMenu; closePolicy: Popup.NoAutoClose
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
                                                // FIX 1: Removed loadRpFirstPage() — selection only, search on button click
                                                onClicked: { page.rpGameVersion = ""; rpVersionMenu.close() }
                                            }
                                        }

                                        Repeater {
                                            model: {
                                                if (!backend || !backend.versionIds) return ["1.21.10","1.20.6"]
                                                var seen = new Set()
                                                var groups = []
                                                for (var i = 0; i < backend.versionIds.length; i++) {
                                                    var v = backend.versionIds[i]
                                                    if (!page.rpShowPreReleases && !/^[0-9.]+$/.test(v)) continue
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
                                                    // FIX 1: Removed loadRpFirstPage() — selection only, search on button click
                                                    onClicked: { page.rpGameVersion = modelData; rpVersionMenu.close() }
                                                }
                                            }
                                        }

                                    }
                                }
                            }
                        }

                        Text { text: "筛选"; color: "#9094a8"; font.pixelSize: 11; Layout.preferredWidth: 32 }

                        // Three filter dropdowns: Category | Feature | Resolution
                        RowLayout {
                            Layout.fillWidth: true; spacing: 6

                            // ── Category dropdown ──
                            Rectangle {
                                id: rpCatPill
                                Layout.preferredWidth: 95; height: 28; radius: 5
                                color: rpCatHov.hovered ? "#1a2848" : "#0c0e14"
                                border.color: page.rpCategoryFilter ? "#5068c8" : "#2a3040"; border.width: 1
                                RowLayout {
                                    anchors.fill: parent; anchors.leftMargin: 6; anchors.rightMargin: 3; spacing: 2
                                    Text {
                                        Layout.fillWidth: true
                                        text: {
                                            var k = page.rpCategoryFilter
                                            var m = { "": "类别", "combat": "战斗", "cursed": "猎奇", "decoration": "装饰",
                                                "modded": "模组适配", "realistic": "写实", "simplistic": "简约",
                                                "themed": "主题", "tweaks": "微调", "utility": "实用",
                                                "vanilla-like": "原版", "fantasy": "幻想", "modern": "现代",
                                                "medieval": "中世纪", "futuristic": "未来", "cartoon": "卡通",
                                                "pvp": "PVP", "minigame": "小游戏", "gui": "界面", "font": "字体",
                                                "hd": "高清", "photorealism": "照片", "cute": "可爱",
                                                "dark": "暗色", "light": "亮色", "clean": "简洁" }
                                            return m[k] || (k || "类别")
                                        }
                                        color: page.rpCategoryFilter ? "#8aaeff" : "#788090"; font.pixelSize: 11
                                        elide: Text.ElideRight
                                    }
                                    Text { text: "▾"; color: "#505468"; font.pixelSize: 8 }
                                }
                                MouseArea {
                                    id: rpCatHov; anchors.fill: parent
                                    hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                    onClicked: { rpFeatMenu.close(); rpResMenu.close(); if (rpCatMenu.visible) rpCatMenu.close(); else rpCatMenu.open() }
                                }
                                Popup {
                                    id: rpCatMenu; closePolicy: Popup.NoAutoClose
                                    y: parent.height + 4; width: 160
                                    height: Math.min(rpCatFlick.contentHeight + 8, 240)
                                    padding: 0
                                    background: Rectangle { color: "#151922"; radius: 8; border.color: "#1e2230" }
                                    Flickable {
                                        id: rpCatFlick
                                        anchors.fill: parent; anchors.margins: 4
                                        contentHeight: rpCatInner.implicitHeight; clip: true
                                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                                        ColumnLayout {
                                            id: rpCatInner; width: parent.width; spacing: 2
                                            Repeater {
                                                model: [
                                                    {key:"", label:"全部"},
                                                    {key:"combat", label:"战斗"},
                                                    {key:"cursed", label:"猎奇"},
                                                    {key:"decoration", label:"装饰"},
                                                    {key:"modded", label:"模组适配"},
                                                    {key:"realistic", label:"写实"},
                                                    {key:"simplistic", label:"简约"},
                                                    {key:"themed", label:"主题"},
                                                    {key:"tweaks", label:"微调"},
                                                    {key:"utility", label:"实用"},
                                                    {key:"vanilla-like", label:"原版"},
                                                    {key:"fantasy", label:"幻想"},
                                                    {key:"modern", label:"现代"},
                                                    {key:"medieval", label:"中世纪"},
                                                    {key:"futuristic", label:"未来"},
                                                    {key:"cartoon", label:"卡通"},
                                                    {key:"pvp", label:"PVP"},
                                                    {key:"minigame", label:"小游戏"},
                                                    {key:"gui", label:"界面"},
                                                    {key:"font", label:"字体"},
                                                    {key:"hd", label:"高清"},
                                                    {key:"photorealism", label:"照片"},
                                                    {key:"cute", label:"可爱"},
                                                    {key:"dark", label:"暗色"},
                                                    {key:"light", label:"亮色"},
                                                    {key:"clean", label:"简洁"}
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
                                                        onClicked: { page.rpCategoryFilter = modelData.key; rpCatMenu.close() }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            // ── Feature dropdown ──
                            Rectangle {
                                id: rpFeatPill
                                Layout.preferredWidth: 95; height: 28; radius: 5
                                color: rpFeatHov.hovered ? "#1a2848" : "#0c0e14"
                                border.color: page.rpFeatureFilter ? "#5068c8" : "#2a3040"; border.width: 1
                                RowLayout {
                                    anchors.fill: parent; anchors.leftMargin: 6; anchors.rightMargin: 3; spacing: 2
                                    Text {
                                        Layout.fillWidth: true
                                        text: {
                                            var k = page.rpFeatureFilter
                                            var m = { "audio": "音频", "blocks": "方块", "core-shaders": "核心着色器",
                                                "entities": "实体", "environment": "环境", "equipment": "装备",
                                                "fonts": "字体", "gui": "图形界面", "items": "物品",
                                                "locale": "本地化", "models": "模型", "minecraft": "Minecraft" }
                                            return k ? (m[k] || k) : "功能"
                                        }
                                        color: page.rpFeatureFilter ? "#8aaeff" : "#788090"; font.pixelSize: 11
                                        elide: Text.ElideRight
                                    }
                                    Text { text: "▾"; color: "#505468"; font.pixelSize: 8 }
                                }
                                MouseArea {
                                    id: rpFeatHov; anchors.fill: parent
                                    hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                    onClicked: { rpCatMenu.close(); rpResMenu.close(); if (rpFeatMenu.visible) rpFeatMenu.close(); else rpFeatMenu.open() }
                                }
                                Popup {
                                    id: rpFeatMenu; closePolicy: Popup.NoAutoClose
                                    y: parent.height + 4; width: 160
                                    height: Math.min(rpFeatFlick.contentHeight + 8, 240)
                                    padding: 0
                                    background: Rectangle { color: "#151922"; radius: 8; border.color: "#1e2230" }
                                    Flickable {
                                        id: rpFeatFlick
                                        anchors.fill: parent; anchors.margins: 4
                                        contentHeight: rpFeatInner.implicitHeight; clip: true
                                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                                        ColumnLayout {
                                            id: rpFeatInner; width: parent.width; spacing: 2
                                            Repeater {
                                                model: [
                                                    {key:"", label:"全部"},
                                                    {key:"audio", label:"音频"},
                                                    {key:"blocks", label:"方块"},
                                                    {key:"core-shaders", label:"核心着色器"},
                                                    {key:"entities", label:"实体"},
                                                    {key:"environment", label:"环境"},
                                                    {key:"equipment", label:"装备"},
                                                    {key:"fonts", label:"字体"},
                                                    {key:"gui", label:"图形界面"},
                                                    {key:"items", label:"物品"},
                                                    {key:"locale", label:"本地化"},
                                                    {key:"models", label:"模型"},
                                                    {key:"minecraft", label:"Minecraft"}
                                                ]
                                                Rectangle {
                                                    Layout.fillWidth: true; height: 26; radius: 4
                                                    color: page.rpFeatureFilter === modelData.key ? "#1a2848" : "transparent"
                                                    Text {
                                                        anchors.centerIn: parent; text: modelData.label
                                                        color: page.rpFeatureFilter === modelData.key ? "#5068c8" : "#9094a8"; font.pixelSize: 11
                                                        font.weight: page.rpFeatureFilter === modelData.key ? Font.Bold : Font.Normal
                                                    }
                                                    MouseArea {
                                                        anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                                        onClicked: { page.rpFeatureFilter = modelData.key; rpFeatMenu.close() }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            // ── Resolution dropdown ──
                            Rectangle {
                                id: rpResPill
                                Layout.preferredWidth: 95; height: 28; radius: 5
                                color: rpResHov.hovered ? "#1a2848" : "#0c0e14"
                                border.color: page.rpResolutionFilter ? "#5068c8" : "#2a3040"; border.width: 1
                                RowLayout {
                                    anchors.fill: parent; anchors.leftMargin: 6; anchors.rightMargin: 3; spacing: 2
                                    Text {
                                        Layout.fillWidth: true
                                        text: page.rpResolutionFilter || "分辨率"
                                        color: page.rpResolutionFilter ? "#8aaeff" : "#788090"; font.pixelSize: 11
                                        elide: Text.ElideRight
                                    }
                                    Text { text: "▾"; color: "#505468"; font.pixelSize: 8 }
                                }
                                MouseArea {
                                    id: rpResHov; anchors.fill: parent
                                    hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                    onClicked: { rpCatMenu.close(); rpFeatMenu.close(); if (rpResMenu.visible) rpResMenu.close(); else rpResMenu.open() }
                                }
                                Popup {
                                    id: rpResMenu; closePolicy: Popup.NoAutoClose
                                    y: parent.height + 4; width: 130
                                    height: Math.min(rpResFlick.contentHeight + 8, 240)
                                    padding: 0
                                    background: Rectangle { color: "#151922"; radius: 8; border.color: "#1e2230" }
                                    Flickable {
                                        id: rpResFlick
                                        anchors.fill: parent; anchors.margins: 4
                                        contentHeight: rpResInner.implicitHeight; clip: true
                                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                                        ColumnLayout {
                                            id: rpResInner; width: parent.width; spacing: 2
                                            Repeater {
                                                model: [
                                                    {key:"", label:"全部"},
                                                    {key:"8x", label:"8x"},
                                                    {key:"16x", label:"16x"},
                                                    {key:"32x", label:"32x"},
                                                    {key:"64x", label:"64x"},
                                                    {key:"128x", label:"128x"},
                                                    {key:"256x", label:"256x"},
                                                    {key:"512x", label:"512x"}
                                                ]
                                                Rectangle {
                                                    Layout.fillWidth: true; height: 26; radius: 4
                                                    color: page.rpResolutionFilter === modelData.key ? "#1a2848" : "transparent"
                                                    Text {
                                                        anchors.centerIn: parent; text: modelData.label
                                                        color: page.rpResolutionFilter === modelData.key ? "#5068c8" : "#9094a8"; font.pixelSize: 11
                                                        font.weight: page.rpResolutionFilter === modelData.key ? Font.Bold : Font.Normal
                                                    }
                                                    MouseArea {
                                                        anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                                        onClicked: { page.rpResolutionFilter = modelData.key; rpResMenu.close() }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Row 2.5: Pre-release toggle — FIX 3: moved below version/type row, left-aligned
                    RowLayout {
                        Layout.fillWidth: true; spacing: 8
                        Text {
                            text: page.rpShowPreReleases ? "\u2b07 \u9690\u85cf\u6d4b\u8bd5\u7248" : "\u25b8 \u663e\u793a\u6d4b\u8bd5\u7248"
                            color: page.rpShowPreReleases ? "#5068c8" : "#505468"; font.pixelSize: 10
                            MouseArea {
                                anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                // FIX 3: Pre-release toggle ONLY filters the version dropdown, not search results
                                onClicked: { page.rpShowPreReleases = !page.rpShowPreReleases }
                            }
                        }
                        Item { Layout.fillWidth: true }
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
                                // FIX 1: Search button — the ONLY trigger for loadRpFirstPage() from filter changes
                                onClicked: loadRpFirstPage()
                            }
                        }

                        Rectangle {
                            width: 72; height: 28; radius: 5
                            color: rpResetHov.hovered ? "#252a38" : "#151922"
                            border.color: "#2a3040"; border.width: 1
                            visible: page.rpCategoryFilter || page.rpFeatureFilter || page.rpResolutionFilter || page.rpGameVersion || rpSearchInput.text
                            Text { anchors.centerIn: parent; text: "重置"; color: "#9094a8"; font.pixelSize: 12 }
                            MouseArea {
                                id: rpResetHov; anchors.fill: parent
                                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    page.rpCategoryFilter = ""
                                    page.rpFeatureFilter = ""
                                    page.rpResolutionFilter = ""
                                    page.rpGameVersion = ""
                                    rpSearchInput.text = ""
                                    // FIX 1: Clear all filters, then trigger search
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
                        width: rpListView.width - 8; height: 130; clip: true
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
                                    property string rpIconCacheKey: ""

                                    source: ""
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true; cache: true
                                    autoTransform: true
                                    sourceSize.width: 88; sourceSize.height: 88

                                    function triggerIconLoad() {
                                        if (!model || !model.icon) return
                                        var url = model.icon
                                        url = url.replace("cdn.modrinth.com", "mod.mcimirror.top")
                                        url = url.replace("cdn-alt.modrinth.com", "mod.mcimirror.top")
                                        rpIconCacheKey = url
                                        rpIconFallback.visible = false
                                        // Check if already cached
                                        if (backend && backend.cachedIconPath) {
                                            var cached = backend.cachedIconPath(url)
                                            if (cached) {
                                                source = cached
                                                return
                                            }
                                            // Trigger async download+convert
                                            backend.cacheIconAsync(url)
                                        }
                                    }
                                    Component.onCompleted: triggerIconLoad()

                                    Connections {
                                        target: backend
                                        function onIconCached(webpUrl, pngPath) {
                                            if (webpUrl !== rpCardIcon.rpIconCacheKey) return
                                            if (pngPath) {
                                                rpCardIcon.source = pngPath
                                            } else {
                                                rpIconFallback.visible = true
                                            }
                                        }
                                    }
                                    onStatusChanged: {
                                        if (status === Image.Loading) return
                                        if (status === Image.Ready) { rpIconFallback.visible = false }
                                        else if (source && source.toString() !== "") { rpIconFallback.visible = true }
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
                                            if (d >= 1000000) return "↓" + (d/1000).toFixed(1) + "K"
                                            if (d >= 1000) return "↓" + (d/1000).toFixed(1) + "K"
                                            return "↓" + d
                                        }
                                        color: "#788090"; font.pixelSize: 10
                                    }
                                }

                                // Row 2: Chinese tags (imperative, no Repeater binding)
                                Item {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: rpTagRow.hasTags ? 18 : 0
                                    visible: rpTagRow.hasTags
                                    clip: true
                                    Row {
                                        id: rpTagRow
                                        spacing: 4
                                        property string tagsJson: ""
                                        property string _tagPending: ""
                                        property bool hasTags: false
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
                                                if (!json || json === "" || json === "[]") { rpTagRow.hasTags = false; return }
                                                var tags = []; try { tags = JSON.parse(json) } catch(e) { rpTagRow.hasTags = false; return }
                                                rpTagRow.hasTags = (tags.length > 0)
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
                                            value: model ? (model.categories || "[]") : "[]"
                                            when: model !== null
                                        }
                                    }
                                }

                                // Row 2.5: Feature tags (PBR, animations, etc.)
                                Item {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: rpFeatRow.hasFeats ? 18 : 0
                                    visible: rpFeatRow.hasFeats
                                    clip: true
                                    Row {
                                        id: rpFeatRow
                                        spacing: 4
                                        property string featJson: ""
                                        property string _featPending: ""
                                        property bool hasFeats: false
                                        Timer {
                                            id: rpFeatTimer; interval: 1
                                            onTriggered: {
                                                var json = rpFeatRow._featPending; rpFeatRow._featPending = ""
                                                for (var i = rpFeatRow.children.length - 1; i >= 0; i--) {
                                                    if (rpFeatRow.children[i] !== rpFeatComp) rpFeatRow.children[i].destroy()
                                                }
                                                if (!json || json === "" || json === "[]") { rpFeatRow.hasFeats = false; return }
                                                var tags = []; try { tags = JSON.parse(json) } catch(e) { rpFeatRow.hasFeats = false; return }
                                                rpFeatRow.hasFeats = (tags.length > 0)
                                                for (var t = 0; t < Math.min(tags.length, 4); t++) {
                                                    var en = String(tags[t]).toLowerCase()
                                                    rpFeatComp.createObject(rpFeatRow, {
                                                        "tagLabel": page.rpFeatureMap[en] || en
                                                    })
                                                }
                                            }
                                        }
                                        onFeatJsonChanged: { rpFeatRow._featPending = featJson; rpFeatTimer.restart() }
                                        Component {
                                            id: rpFeatComp
                                            Rectangle {
                                                height: 16; width: tText2.implicitWidth + 10; radius: 4
                                                color: "#1a1428"; border.color: "#382848"; border.width: 1
                                                property string tagLabel: ""
                                                Text {
                                                    id: tText2; anchors.centerIn: parent
                                                    text: tagLabel; color: "#a878c8"; font.pixelSize: 9
                                                }
                                            }
                                        }
                                        Binding {
                                            target: rpFeatRow; property: "featJson"
                                            value: model ? (model.features || "[]") : "[]"
                                            when: model !== null
                                        }
                                    }
                                }

                                // Row 2.6: Resolution tags
                                Item {
                                    Layout.fillWidth: true; Layout.preferredHeight: rpResTagRow2.hasRes ? 18 : 0
                                    visible: rpResTagRow2.hasRes
                                    clip: true
                                    Row {
                                        id: rpResTagRow2
                                        spacing: 4
                                        property bool hasRes: false
                                        property string resJson: ""
                                        property string _resPending: ""
                                        Timer {
                                            id: rpResTimer; interval: 1
                                            onTriggered: {
                                                var json = rpResTagRow2._resPending; rpResTagRow2._resPending = ""
                                                for (var i = rpResTagRow2.children.length - 1; i >= 0; i--) {
                                                    if (rpResTagRow2.children[i] !== rpResComp) rpResTagRow2.children[i].destroy()
                                                }
                                                if (!json || json === "" || json === "[]") { rpResTagRow2.hasRes = false; return }
                                                var tags = []; try { tags = JSON.parse(json) } catch(e) { rpResTagRow2.hasRes = false; return }
                                                rpResTagRow2.hasRes = (tags.length > 0)
                                                for (var t = 0; t < Math.min(tags.length, 4); t++) {
                                                    rpResComp.createObject(rpResTagRow2, {
                                                        "tagLabel": String(tags[t])
                                                    })
                                                }
                                            }
                                        }
                                        onResJsonChanged: { rpResTagRow2._resPending = resJson; rpResTimer.restart() }
                                        Component {
                                            id: rpResComp
                                            Rectangle {
                                                height: 16; width: tText3.implicitWidth + 10; radius: 4
                                                color: "#282218"; border.color: "#504828"; border.width: 1
                                                property string tagLabel: ""
                                                Text {
                                                    id: tText3; anchors.centerIn: parent
                                                    text: tagLabel; color: "#c8a860"; font.pixelSize: 9
                                                }
                                            }
                                        }
                                        Binding {
                                            target: rpResTagRow2; property: "resJson"
                                            value: model ? (model.resolutions || "[]") : "[]"
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
                                var iconUrl = model.icon || ""
                                iconUrl = iconUrl.replace("cdn.modrinth.com", "mod.mcimirror.top")
                                iconUrl = iconUrl.replace("cdn-alt.modrinth.com", "mod.mcimirror.top")
                                // Use same icon cache as main page cards (Qt lacks webp support)
                                if (backend && backend.cachedIconPath) {
                                    var cached = backend.cachedIconPath(iconUrl)
                                    if (cached) {
                                        page.rpDetailIconUrl = cached
                                    } else {
                                        page.rpDetailIconUrl = iconUrl  // temporary, will update after cache
                                        backend.cacheIconAsync(iconUrl)
                                    }
                                } else {
                                    page.rpDetailIconUrl = iconUrl
                                }
                                page.rpDetailAuthor = model.author || ""
                                page.rpDetailDesc = model.desc || ""
                                page.rpDetailDownloads = model.downloads || 0
                                page.rpDetailUpdated = model.updated || ""
                            }
                        }
                    }
                }
            }
        }
    }


    // ════════════════════════════════════════════
    // Resource pack detail page (full-screen overlay)
    // ════════════════════════════════════════════
    Timer {
        id: rpDetailBackTimer; interval: 150; repeat: false
    }

    Item {
        id: rpDetailPage
        anchors.fill: parent
        z: 10  // above all other content
        visible: rpDetailSlug !== "" && !rpDetailBackTimer.running

        // Detail page properties (must be at Item root, not inside ColumnLayout)
        property var rpDetailGrouped: {
            var _ver = page.rpVersionCacheVersion; var d = page.rpVersionCache
            var raw = (d && d[rpDetailSlug]) ? d[rpDetailSlug] : []
            var groups = {}
            for (var i = 0; i < raw.length; i++) {
                var v = raw[i]
                // Group by major.minor (first two segments): "1.21.11" -> "1.21", "26.1.2" -> "26.1"
                var segs = v.split(".")
                var major = segs.length >= 2 ? segs[0] + "." + segs[1] : v
                if (!groups[major]) groups[major] = []
                groups[major].push(v)
            }
            var result = []
            for (var k in groups) { result.push({major: k, versions: groups[k]}) }
            // Sort groups by major.minor, descending
            result.sort(function(a,b) {
                var as = a.major.split("."), bs = b.major.split(".")
                var aM = parseInt(as[0])||0, bM = parseInt(bs[0])||0
                if (aM !== bM) return bM - aM
                return (parseInt(bs[1])||0) - (parseInt(as[1])||0)
            })
            return result
        }
        property var rpDetailExpanded: []  // Array of expanded group major keys (independent toggle)
        property string rpDetailSelectedVer: ""  // Level 3: which game_version's detail card is open

        function isGroupExpanded(major) {
            // Handle legacy string mode (set by CLI --detail-expand)
            if (typeof rpDetailExpanded === 'string') return rpDetailExpanded === major
            return rpDetailExpanded.indexOf(major) >= 0
        }

        function toggleGroupExpanded(major) {
            // Convert legacy string to array if needed
            var arr = (typeof rpDetailExpanded === 'string') ? [rpDetailExpanded] : rpDetailExpanded.slice()
            var idx = arr.indexOf(major)
            if (idx >= 0) arr.splice(idx, 1)
            else arr.push(major)
            rpDetailExpanded = arr
        }

        function getVerDetail(gameVer) {
            var cache = page.rpVersionDetailCache[rpDetailSlug]
            if (!cache) return null
            return cache[gameVer] || null
        }

        function formatDate(isoStr) {
            if (!isoStr) return "-"
            var d = isoStr.slice(0, 10)
            return d
        }

        function formatDownloads(n) {
            if (!n && n !== 0) return "0"
            if (n >= 1000000) return (n / 1000).toFixed(1) + "K"
            if (n >= 1000) return (n / 1000).toFixed(1) + "K"
            return String(n)
        }

        Rectangle {
            anchors.fill: parent
            color: "#0c0f16"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                // ← Back button
                Rectangle {
                    Layout.preferredHeight: 30
                    width: rpBackLabel.implicitWidth + 20; radius: 6
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
                            rpDetailBackTimer.restart()
                            rpDetailSlug = ""
                            // Ensure we stay on RP tab (tab index 3)
                            currentTab = 3
                        }
                    }
                }

                // Title row with icon
                RowLayout {
                    Layout.fillWidth: true; spacing: 14
                    Rectangle {
                        width: 48; height: 48; radius: 10; color: "#1a1f2e"
                        Layout.preferredWidth: 48; Layout.preferredHeight: 48
                        clip: true
                        Image {
                            id: rpDetailIcon
                            anchors.fill: parent
                            source: page.rpDetailIconUrl || ""
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true; cache: true
                            sourceSize.width: 96; sourceSize.height: 96
                            onStatusChanged: {
                                if (status === Image.Error) {
                                    rpDetailIconFallback.visible = true
                                } else if (status === Image.Ready) {
                                    rpDetailIconFallback.visible = false
                                }
                            }

                            Connections {
                                target: backend
                                function onIconCached(webpUrl, pngPath) {
                                    if (webpUrl !== page.rpDetailIconUrl) return
                                    if (pngPath) {
                                        rpDetailIcon.source = pngPath
                                        rpDetailIconFallback.visible = false
                                    }
                                }
                            }
                        }
                        Text {
                            id: rpDetailIconFallback
                            anchors.centerIn: parent
                            text: (rpDetailTitle || rpDetailSlug) ? (rpDetailTitle || rpDetailSlug)[0] : "R"
                            color: "#5068c8"; font.pixelSize: 22; font.bold: true
                            visible: !page.rpDetailIconUrl
                        }
                    }
                    Text {
                        text: rpDetailTitle || rpDetailSlug; color: "#d0d4e0"
                        font.pixelSize: 18; font.bold: true
                        elide: Text.ElideRight; Layout.fillWidth: true
                    }
                }

                // ── Detail info card (Issue #4) ──
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: rpInfoCardCol.implicitHeight + 24
                    radius: 10; color: "#101828"
                    border.color: "#1e2c48"; border.width: 1
                    clip: true
                    Column {
                        id: rpInfoCardCol
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 12 }
                        spacing: 6

                        // Author
                        Text {
                            text: "作者: " + (page.rpDetailAuthor || "未知")
                            color: "#7888a8"; font.pixelSize: 11
                            elide: Text.ElideRight; width: parent.width
                        }

                        // Description
                        Text {
                            text: page.rpDetailDesc || "无简介"
                            color: "#9098b0"; font.pixelSize: 11
                            elide: Text.ElideRight; maximumLineCount: 3; width: parent.width
                            wrapMode: Text.WordWrap
                        }

                        // Downloads + Updated
                        Row { spacing: 24
                            Text {
                                text: "下载量: " + (page.rpDetailDownloads ? rpDetailPage.formatDownloads(page.rpDetailDownloads) : "-") + " 次"
                                color: "#7888a8"; font.pixelSize: 11
                            }
                            Text {
                                text: "更新于: " + (page.rpDetailUpdated ? rpDetailPage.formatDate(page.rpDetailUpdated) : "-")
                                color: "#7888a8"; font.pixelSize: 11
                            }
                        }

                        // Action buttons
                        Row { spacing: 8
                            Rectangle {
                                width: Math.max(rpModBtn.implicitWidth + 24, 110); height: 28; radius: 6
                                color: rpModBtnHov.hovered ? "#1a2a50" : "transparent"
                                border.color: "#5068c8"; border.width: 1.5
                                Text {
                                    id: rpModBtn; anchors.centerIn: parent
                                    text: "转到Modrinth"; color: "#6888e8"; font.pixelSize: 11
                                }
                                MouseArea {
                                    id: rpModBtnHov; anchors.fill: parent
                                    hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (rpDetailSlug) Qt.openUrlExternally("https://modrinth.com/resourcepack/" + rpDetailSlug)
                                    }
                                }
                            }
                            Rectangle {
                                width: Math.max(rpCopyBtn.implicitWidth + 24, 90); height: 28; radius: 6
                                color: rpCopyBtnHov.hovered ? "#282018" : "transparent"
                                border.color: "#685040"; border.width: 1.5
                                Text {
                                    id: rpCopyBtn; anchors.centerIn: parent
                                    text: "复制名称"; color: "#c89860"; font.pixelSize: 11
                                }
                                MouseArea {
                                    id: rpCopyBtnHov; anchors.fill: parent
                                    hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (backend) backend.copyToClipboard(rpDetailTitle || rpDetailSlug)
                                    }
                                }
                            }
                        }
                    }
                }

                Text {
                    text: "所有可用游戏版本 | 大版本分组 | 点击安装"
                    color: "#505468"; font.pixelSize: 11
                }

                // Version list grouped by major, click to expand
                ScrollView {
                    id: rpDetailScroll
                    Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded

                    Column {
                        width: rpDetailScroll.availableWidth - 4; spacing: 4

                        Repeater {
                            model: rpDetailPage.rpDetailGrouped
                            delegate: Column {
                                width: parent.width; spacing: 2

                                // Group header
                                Rectangle {
                                    width: parent.width; height: 40; radius: 8
                                    color: rpDetGrpArea.containsMouse ? "#1e2c50" : "#141c2c"
                                    border.color: rpDetGrpArea.containsMouse ? "#3858c0" : "#1a2848"
                                    border.width: 1
                                    RowLayout {
                                        anchors.fill: parent; anchors.margins: 10; spacing: 10
                                        Text {
                                            text: (rpDetailPage.isGroupExpanded(modelData.major) ? "\u25bc" : "\u25b8") + "  MC " + modelData.major
                                            color: "#6080d8"; font.pixelSize: 14; font.bold: true
                                        }
                                        Item { Layout.fillWidth: true }
                                        Text {
                                            text: modelData.versions.length + " 个版本"
                                            color: "#505468"; font.pixelSize: 10
                                        }
                                    }
                                    MouseArea {
                                        id: rpDetGrpArea; anchors.fill: parent
                                        hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            rpDetailPage.toggleGroupExpanded(modelData.major)
                                        }
                                    }
                                }

                                // Sub-versions (only when expanded) — Level 2 + Level 3 drill-down
                                Repeater {
                                    model: rpDetailPage.isGroupExpanded(modelData.major) ? modelData.versions : []
                                    delegate: Column {
                                        width: parent.width - 24; x: 24; spacing: 2

                                        // Level 2: sub-version row (click to expand/collapse)
                                        Rectangle {
                                            width: parent.width; height: 34; radius: 6
                                            color: {
                                                if (rpDetailPage.rpDetailSelectedVer === modelData) return "#1a2848"
                                                return rpDetSubHov.hovered ? "#1a2436" : "#111820"
                                            }
                                            border.color: {
                                                if (rpDetailPage.rpDetailSelectedVer === modelData) return "#3858c0"
                                                return rpDetSubHov.hovered ? "#1e3050" : "#141c28"
                                            }
                                            border.width: 1
                                            RowLayout {
                                                anchors.fill: parent; anchors.margins: 8; spacing: 8
                                                Text {
                                                    text: (rpDetailPage.rpDetailSelectedVer === modelData ? "\u25bc" : "\u25b8") + " " + modelData
                                                    color: "#8898b8"; font.pixelSize: 12
                                                    font.family: "Consolas, monospace"; Layout.fillWidth: true
                                                }
                                            }
                                            MouseArea {
                                                id: rpDetSubHov; anchors.fill: parent
                                                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    rpDetailPage.rpDetailSelectedVer = (rpDetailPage.rpDetailSelectedVer === modelData) ? "" : modelData
                                                }
                                            }
                                        }

                                        // Level 3: Version detail card (appears when this sub-version is selected)
                                        Rectangle {
                                            visible: rpDetailPage.rpDetailSelectedVer === modelData
                                            width: parent.width; height: l3Content.implicitHeight + 20; radius: 8
                                            color: "#101828"
                                            border.color: "#2a3a58"; border.width: 1

                                            Column {
                                                id: l3Content
                                                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 12 }
                                                spacing: 6

                                                property var detail: { var slugCache = page.rpVersionDetailCache[page.rpDetailSlug]; return slugCache ? (slugCache[modelData] || {}) : {} }

                                                // Pack name
                                                Text {
                                                    text: l3Content.detail.name || "-"
                                                    color: "#d0d8f0"; font.pixelSize: 13; font.bold: true
                                                    elide: Text.ElideRight; width: parent.width
                                                }

                                                // Version number row
                                                Row { spacing: 8
                                                    Text {
                                                        text: "版本:"; color: "#606880"; font.pixelSize: 10
                                                        anchors.verticalCenter: parent.verticalCenter
                                                    }
                                                    Text {
                                                        text: l3Content.detail.version_number || "-"
                                                        color: "#c0c8e0"; font.pixelSize: 11
                                                        font.family: "Consolas, monospace"
                                                        anchors.verticalCenter: parent.verticalCenter
                                                    }
                                                    // Type badge
                                                    Rectangle {
                                                        width: typeBadge.implicitWidth + 8; height: 18; radius: 3
                                                        color: (l3Content.detail.version_type === "release") ? "#1a3a28" : "#3a2a18"
                                                        anchors.verticalCenter: parent.verticalCenter
                                                        Text {
                                                            id: typeBadge
                                                            anchors.centerIn: parent
                                                            text: (l3Content.detail.version_type === "release") ? "正式版" :
                                                                  (l3Content.detail.version_type === "beta") ? "测试版" :
                                                                  (l3Content.detail.version_type || "-")
                                                            color: (l3Content.detail.version_type === "release") ? "#48d878" : "#e8a840"
                                                            font.pixelSize: 9
                                                        }
                                                    }
                                                }

                                                // Downloads + Date row
                                                Row { spacing: 16
                                                    Row { spacing: 4
                                                        Text { text: "下载量:"; color: "#606880"; font.pixelSize: 10; anchors.verticalCenter: parent.verticalCenter }
                                                        Text {
                                                            text: rpDetailPage.formatDownloads(l3Content.detail.downloads)
                                                            color: "#a0a8c0"; font.pixelSize: 10
                                                            anchors.verticalCenter: parent.verticalCenter
                                                        }
                                                    }
                                                    Row { spacing: 4
                                                        Text { text: "日期:"; color: "#606880"; font.pixelSize: 10; anchors.verticalCenter: parent.verticalCenter }
                                                        Text {
                                                            text: rpDetailPage.formatDate(l3Content.detail.date_published)
                                                            color: "#a0a8c0"; font.pixelSize: 10
                                                            anchors.verticalCenter: parent.verticalCenter
                                                        }
                                                    }
                                                }

                                                // Download button - blue outline style
                                                Rectangle {
                                                    width: l3DownloadBtn.implicitWidth + 24; height: 28; radius: 6
                                                    color: l3DownloadHov.hovered ? "#1a2a50" : "transparent"
                                                    border.color: "#5068c8"; border.width: 2
                                                    anchors.horizontalCenter: parent.horizontalCenter
                                                    Text {
                                                        id: l3DownloadBtn
                                                        anchors.centerIn: parent
                                                        text: "下载"
                                                        color: "#5068c8"; font.pixelSize: 12; font.weight: Font.Medium
                                                    }
                                                    MouseArea {
                                                        id: l3DownloadHov
                                                        anchors.fill: parent; hoverEnabled: true
                                                        cursorShape: Qt.PointingHandCursor
                                                        onClicked: {
                                                            console.log("[resourcepack] L3 download:", rpDetailSlug, modelData)
                                                            rpFolderDialog.slug = rpDetailSlug
                                                            page.rpGameVersion = modelData  // store selected MC version for download
                                                            rpFolderDialog.open()
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Text {
                            visible: rpDetailPage.rpDetailGrouped.length === 0
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
    ListModel { id: shaderResultsModel }
    ListModel { id: rpResultsModel }

    // Mod & Shader state
    property bool modSearching: false
    property var modDownloadingSlugs: ({})  // set<string> tracking slugs being downloaded
    property bool shaderSearching: false
    property var shaderDownloadingSlugs: ({})

    property string rpDetailSlug: ""
    property string rpDetailTitle: ""
    property string rpDetailIconUrl: ""
    property string rpDetailAuthor: ""
    property string rpDetailDesc: ""
    property int rpDetailDownloads: 0
    property string rpDetailUpdated: ""
    property string installingRpName: ""
    property var rpVersionCache: ({})
    property var rpVersionDetailCache: ({})  // { slug: { "1.21.11": {version_number,name,downloads,...} } }

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
            var featFilter = page.rpFeatureFilter.toLowerCase()
            var resFilter = page.rpResolutionFilter.toLowerCase()
            for (var i = 0; i < results.length; i++) {
                var r = results[i]
                // Filter by category
                if (catFilter) {
                    var cats = r.categories || []
                    var hasCat = false
                    for (var c = 0; c < cats.length; c++) {
                        if (String(cats[c]).toLowerCase() === catFilter) { hasCat = true; break }
                    }
                    if (!hasCat) continue
                }
                // Filter by feature
                if (featFilter) {
                    var feats = r.features || []
                    var hasFeat = false
                    for (var f = 0; f < feats.length; f++) {
                        if (String(feats[f]).toLowerCase() === featFilter) { hasFeat = true; break }
                    }
                    if (!hasFeat) continue
                }
                // Filter by resolution
                if (resFilter) {
                    var resos = r.resolutions || []
                    // Also check categories for resolution patterns (safety net)
                    var resosFromCats = (r.categories && resFilter) ? r.categories.filter(function(x) { return String(x).toLowerCase() === resFilter }) : []
                    var allResos = resos.concat(resosFromCats)
                    var hasRes = false
                    for (var x = 0; x < allResos.length; x++) {
                        if (String(allResos[x]).toLowerCase() === resFilter) { hasRes = true; break }
                    }
                    if (!hasRes) continue
                }
                slugs.push(r.slug)
                rpResultsModel.append({
                    slug: r.slug || "",
                    title: r.title || "",
                    desc: r.desc || r.description || "",
                    icon: r.icon || "",
                    downloads: r.downloads || 0,
                    categories: JSON.stringify(r.categories || []),
                    features: JSON.stringify(r.features || []),
                    resolutions: JSON.stringify(r.resolutions || []),
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

        function onResourcepackVersionsPartial(slug, versions, details) {
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
            // Clone to trigger QML binding re-evaluation (mutation of nested JS object is undetectable)
            var newVerCache = Object.assign({}, page.rpVersionCache)
            newVerCache[slug] = versions
            page.rpVersionCache = newVerCache
            if (details) {
                var newDetailCache = Object.assign({}, page.rpVersionDetailCache)
                newDetailCache[slug] = details
                page.rpVersionDetailCache = newDetailCache
            }
            page.rpVersionCacheVersion++
        }

        function onResourcepackVersionsProgress(done, total) {
            console.log("[RP-DEBUG]", page.rpDebugSeq, "progress", done, "/", total)
            if (page.mainWindow && page.mainWindow.loadingBar) {
                page.mainWindow.loadingBar.opacity = (done < total) ? 1 : 0
            }
        }

        function onResourceDownloadProgress(completed, total, fileName) {
            if (page.installingRpName) {
                var pct = total > 0 ? Math.round(completed / total * 100) : 0
                console.log("[resourcepack] download progress:", page.installingRpName, completed, "/", total, "(" + pct + "%)")
                toastManager.show("下载中 " + page.installingRpName + ": " + pct + "%")
                if (page.mainWindow && page.mainWindow.loadingBar) {
                    page.mainWindow.loadingBar.opacity = (completed < total) ? 1 : 0
                }
            }
        }

        function onResourceDownloadDone(success) {
            if (page.installingRpName) {
                console.log("[resourcepack] download done:", page.installingRpName, "success:", success)
                toastManager.show(success ? ("下载完成: " + page.installingRpName) : ("下载失败: " + page.installingRpName))
                page.installingRpName = ""
                if (page.mainWindow && page.mainWindow.loadingBar) {
                    page.mainWindow.loadingBar.opacity = 0
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
        console.log("[RP-DEBUG] rpDetailSlugChanged ->", rpDetailSlug)
        // Close any open filter card popups so they don't overlap the detail page
        if (rpSourceMenu) rpSourceMenu.close()
        if (rpVersionMenu) rpVersionMenu.close()
        if (rpCatMenu) rpCatMenu.close()
        if (rpFeatMenu) rpFeatMenu.close()
        if (rpResMenu) rpResMenu.close()
        rpDetailPage.rpDetailExpanded = []
        rpDetailPage.rpDetailSelectedVer = ""
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
                backend.downloadResourcepack(rpFolderDialog.slug, page.rpGameVersion, dest)
                toastManager.show("开始下载: " + rpFolderDialog.slug)
            }
        }
    }
}
