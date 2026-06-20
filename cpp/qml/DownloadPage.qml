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

    // ════════════════════════════════════════════
    // Tab content loader (replaces inline Items)
    // ════════════════════════════════════════════
    Loader {
        id: tabLoader
        anchors.top: tabBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.topMargin: 8

        sourceComponent: {
            switch (currentTab) {
                case 0: return mcTabComp
                case 1: return modTabComp
                case 2: return shaderTabComp
                case 3: return rpTabComp
                default: return mcTabComp
            }
        }
    }

    Component {
        id: mcTabComp
        McTab {
            backend: backend
            mainWindow: page.mainWindow
            appWindow: page.mainWindow
            currentFilter: page.currentFilter
            selectedVersionId: page.selectedVersionId
            currentSource: page.currentSource
            mirrorSources: page.mirrorSources
            clickedVersionId: page.clickedVersionId
            complianceNotice: page.complianceNotice
            installingVersion: page.installingVersion
            onTriggerDownloadBall: function(x, y) { page.triggerDownloadBall(x, y) }
        }
    }

    Component {
        id: modTabComp
        ModTab {
            backend: backend
            mainWindow: page.mainWindow
            modSearchQuery: page.modSearchQuery
            modLoader: page.modLoader
            modSearchResults: page.modSearchResults
            modResultsReady: page.modResultsReady
            installingMod: page.installingMod
            installingModName: page.installingModName
            complianceNotice: page.complianceNotice
            onTriggerDownloadBall: function(x, y) { page.triggerDownloadBall(x, y) }
        }
    }

    Component {
        id: shaderTabComp
        ShaderTab {
            backend: backend
            mainWindow: page.mainWindow
            shaderResultsReady: page.shaderResultsReady
            onTriggerDownloadBall: function(x, y) { page.triggerDownloadBall(x, y) }
        }
    }

    Component {
        id: rpTabComp
        RpTab {
            backend: backend
            mainWindow: page.mainWindow
            rpGameVersion: page.rpGameVersion
            rpDownloadDir: page.rpDownloadDir
            rpResultsReady: page.rpResultsReady
            rpLoadingProgress: page.rpLoadingProgress
            rpDebugSeq: page.rpDebugSeq
            rpCategoryFilter: page.rpCategoryFilter
            rpFeatureFilter: page.rpFeatureFilter
            rpResolutionFilter: page.rpResolutionFilter
            rpFeatureMap: page.rpFeatureMap
            rpPage: page.rpPage
            rpLoadingMore: page.rpLoadingMore
            rpShowPreReleases: page.rpShowPreReleases
            rpVersionCacheVersion: page.rpVersionCacheVersion
            rpHasMore: page.rpHasMore
            rpTotalHits: page.rpTotalHits
            complianceNotice: page.complianceNotice
            onTriggerDownloadBall: function(x, y) { page.triggerDownloadBall(x, y) }
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
