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
    property string modLoader: ""  // empty = all
    property var modSearchResults: []
    property bool modResultsReady: false
    property string modGameVersion: ""
    property string modCategory: ""
    property string modEnvironment: ""
    property string modLicense: ""
    property bool modShowPreReleases: false

    // Install state
    property bool installingVersion: false
    property bool installingMod: false
    property string installingModName: ""
    property var pendingModDownload: ({})  // {slug, title, versionNumber, loader, gameVersion, url, filename, size, sha1, defaultPath, displayName}

    // Common versions (for mod/Shader/RP tabs)
    property var commonVersions: []

    // Shader state
    property string shaderGameVersion: ""
    property bool shaderShowPreReleases: false
    property bool shaderSearching: false
    property string modDetailSlug: ""
    property string modDetailStep1Ver: ""
    property string modDetailStep2Loader: ""  // unused, kept for compat
    property var modDetailRawVersions: []
    property var modDetailVersionMap: ({})  // versionString → {versionNumber, gameVersions, loaders, date, downloads, url, filename}
    property string modDetailTitle: ""
    property string modDetailDesc: ""
    property string modDetailIcon: ""
    property bool modDetailLoading: false
    property var modDetailVersions: []  // [{gameVersion, versionNumber, date, downloads, loader, url}]
    property var modDetailGrouped: []  // [{major, versions[]}]

    // Shader env labels (same API field as mods)

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
        console.log("[mod-ui] loadModResults loader=" + page.modLoader)
        page.modSearching = true
        backend.searchModsEx(
            "", page.modLoader,
            page.modCategory, page.modGameVersion,
            page.modEnvironment, page.modLicense,
            0, 30
        )
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
    Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeInDuration; easing.type: AnimationTokens.itemFadeInEasing } }
    Behavior on y { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
    Component.onCompleted: {
        console.log("[dlpage] loaded, t=" + Date.now())
        opacity = 1; y = 0
        // Load mirror sources for download source selector
        if (backend) {
            page.mirrorSources = backend.getMirrorSources()
            page.complianceNotice = backend.bmclapiComplianceNotice()
        }
        // Initial load of version list if already available
        refreshVersionModel()
        console.log("[dlpage] init done, t=" + Date.now())
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

        // Populate commonVersions for filter dropdowns
        if (backend.releaseVersions && backend.releaseVersions.length > 0) {
            var seen = {}
            var versions = []
            // Use release versions first, then snapshots
            var all = (backend.releaseVersions || []).concat(backend.snapshotVersions || []).concat(backend.oldVersions || [])
            for (var vi = 0; vi < all.length; vi++) {
                var vv = all[vi]
                if (vv && !seen[vv]) {
                    seen[vv] = true
                    versions.push(vv)
                }
            }
            page.commonVersions = versions
        }

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
        height: 34
        spacing: 4

        property var tabLabels: ["MC 版本", "Mod", "光影", "资源包"]

        Repeater {
            model: [
                { label: "MC 版本", icon: "box" },
                { label: "Mod", icon: "puzzle" },
                { label: "光影", icon: "sparkles" },
                { label: "资源包", icon: "palette" }
            ]
            Rectangle {
                Layout.preferredWidth: 90
                Layout.fillHeight: true
                radius: 8
                color: page.currentTab === index ? "#1a1f2e" : "transparent"
                border.color: page.currentTab === index ? "#3a4eb8" : "transparent"
                border.width: page.currentTab === index ? 1 : 0
                scale: tabMouse.containsMouse ? 1.04 : 1.0
                Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }

                Row {
                    anchors.centerIn: parent; spacing: 6
                    Image {
                        anchors.verticalCenter: parent.verticalCenter
                        source: "icons/lucide/" + modelData.icon + ".svg"
                        width: 16; height: 16
                    }
                    Text {
                        text: modelData.label
                    color: page.currentTab === index ? "#d0d4e0" : "#606478"
                    font.pixelSize: 13
                    font.weight: page.currentTab === index ? Font.DemiBold : Font.Normal
                    }
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

        Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

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
                    { key: "release", label: "正式版", icon: "check-circle", countFn: function() { return page.getReleaseCount() } },
                    { key: "snapshot", label: "快照版", icon: "flask-conical", countFn: function() { return page.getSnapshotCount() } },
                    { key: "old", label: "远古版", icon: "landmark", countFn: function() { return page.getOldCount() } }
                ]

                Rectangle {
                    height: 30; radius: 15
                    Layout.preferredWidth: Math.max(60, Math.min(pillRow.implicitWidth + 20, 140))
                    Layout.minimumWidth: 60
                    color: page.currentFilter === modelData.key ? "#3a4eb8" : "#11141c"
                    border.color: page.currentFilter === modelData.key ? "#3a4eb8" : "#1e2230"
                    border.width: 1
                    clip: true
                    scale: pillMouse.containsMouse ? 1.04 : 1.0
                    Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }

                    Row {
                        id: pillRow
                        anchors.centerIn: parent
                        spacing: 4
                        Image {
                            anchors.verticalCenter: parent.verticalCenter
                            source: "icons/lucide/" + modelData.icon + ".svg"
                            width: 14; height: 14
                        }
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
                Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }
                visible: page.currentTab === 0
                Image {
                    anchors.centerIn: parent
                    source: "icons/lucide/refresh-cw.svg"
                    width: 16; height: 16
                }
                HoverHandler { id: refreshHover }
                ToolTip { visible: refreshHover.hovered; text: qsTr("刷新版本列表"); delay: 500 }
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
                    Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }
                    Text { id: multiLabel; anchors.centerIn: parent; text: qsTr("自动"); color: page.currentSource === -1 ? "#ffffff" : "#9094a8"; font.pixelSize: 11 }
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
                        Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }
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
                    Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeInDuration; easing.type: AnimationTokens.itemFadeInEasing } }
                    Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }
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
                                text: qsTr("已安装")
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

                        // ── Status text (replaces progress bar) ──
                        Text {
                            visible: backend && backend.installing && backend.installVersionId === model.versionId
                                     && backend.installPhase !== "done"
                            text: qsTr("正在下载")
                            font.pixelSize: 9; color: "#5d6fe0"
                        }

                        // Install button — hidden (moved to InstallPage)
                        Button {
                            id: installBtn
                            visible: false
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
                            enabled: !isInstalled && !isDownloadQueued && !isDownloadActive && page.clickedVersionId !== model.versionId
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
                                       (installBtn.isDownloadActive ? "#0a1020" :
                                       (installBtn.hovered && installBtn.enabled ? "#5068d8" : "transparent")))
                                border.color: installBtn.isInstalled ? "#1a3028" :
                                              (installBtn.isDownloadQueued ? "#3a3000" :
                                              (installBtn.isDownloadActive ? "#1a2840" :
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
                                if (backend) backend.logMessage("[download-ui] card clicked: " + model.versionId + " -> InstallPage")
                                if (page.mainWindow) {
                                    page.mainWindow.installMcVersion = model.versionId
                                    page.mainWindow.showInstallPage = true
                                }
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


    // TAB 1: Mod// ════════════════════════════════════════════
    // ════════════════════════════════════════════

    // ════════════════════════════════════════════
    // TAB 1: Mod 下载
    // ════════════════════════════════════════════
    // TAB 1: Mod
    // ════════════════════════════════════════════
    Item {
        anchors.top: tabBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.topMargin: 8
        opacity: page.currentTab === 1 ? 1 : 0
        enabled: page.currentTab === 1
        Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

        function doModSearch() {
            if (!backend) return
            page.modSearching = true
            modResultsModel.clear()
            var q = modInput.text ? modInput.text.trim() : ""
            backend.searchModsEx(q, page.modLoader, page.modCategory, page.modGameVersion, page.modEnvironment, "", 0, 30)
        }

        function formatDownloads(n) {
            if (n >= 1000000) return (n / 1000000).toFixed(1) + "M 下载"
            if (n >= 1000) return (n / 1000).toFixed(1) + "K 下载"
            return n + " 下载"
        }

    function formatVerDL(n) {
        if (n >= 1000000) return (n / 1000000).toFixed(1) + "M"
        if (n >= 1000) return (n / 1000).toFixed(1) + "K"
        return String(n || 0)
    }

        // Version list (always show all; pre-release toggle filters search only)
        property var modFilteredVersions: page.commonVersions || []

        Connections {
            target: backend
            enabled: backend !== null
            function onSearchResultsReady(results) {
                if (page.currentTab !== 1) return  // mod-only
                modResultsModel.clear()
                for (var j = 0; j < results.length; j++) {
                    var r = results[j]
                    modResultsModel.append({
                        slug: r.slug || "",
                        title: r.title || r.slug || "Unknown",
                        desc: r.desc || "",
                        icon: r.icon || "",
                        downloads: r.downloads || 0,
                        loader: r.loader || "",
                        clientSide: r.clientSide || ""
                    })
                }
                page.modSearching = false
            }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8

            // ── Filter Card ──
            Rectangle {
                Layout.fillWidth: true; height: 88; radius: 10
                color: "#11141c"; border.color: "#1e2230"

                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 12; spacing: 8

                    // Row 1: search + buttons
                    RowLayout {
                        Layout.fillWidth: true; spacing: 5

                        Rectangle {
                            id: modSearchBox
                            Layout.fillWidth: true; height: 28; radius: 5
                            color: (modInput.activeFocus || modSearchBoxHov.hovered) ? "#0f131c" : "#0c0e14"
                            border.color: (modInput.activeFocus || modSearchBoxHov.hovered) ? "#5068c8" : "#2a3040"
                            border.width: (modInput.activeFocus || modSearchBoxHov.hovered) ? 1.5 : 1
                            Behavior on color { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                            Behavior on border.width { NumberAnimation { duration: 150 } }

                            HoverHandler { id: modSearchBoxHov }

                            TextInput {
                                id: modInput
                                anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8
                                color: "#d0d4e0"; verticalAlignment: TextInput.AlignVCenter; font.pixelSize: 12
                                Keys.onReturnPressed: doModSearch()

                                Text {
                                    anchors.fill: parent; verticalAlignment: Text.AlignVCenter
                                    text: qsTr("搜索 Mod…"); color: "#505468"; font.pixelSize: 11
                                    visible: !modInput.text
                                }
                            }
                        }

                        Rectangle {
                            width: 46; height: 28; radius: 5
                            color: modSearchBtn.hovered ? "#5a78e0" : "#5068c8"
                            scale: modSearchBtn.pressed ? 0.92 : (modSearchBtn.hovered ? 1.06 : 1.0)
                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutBack } }
                            Text { anchors.centerIn: parent; text: qsTr("搜索"); color: "#fff"; font.pixelSize: 10; font.bold: true }
                            MouseArea {
                                id: modSearchBtn; anchors.fill: parent
                                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: doModSearch()
                            }
                        }

                        Rectangle {
                            width: 46; height: 28; radius: 5
                            color: modResetBtn.hovered ? "#2a2030" : "#1a1420"
                            border.color: "#3a2840"; border.width: 1
                            scale: modResetBtn.pressed ? 0.92 : (modResetBtn.hovered ? 1.06 : 1.0)
                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutBack } }
                            Text { anchors.centerIn: parent; text: qsTr("重置"); color: "#b090c8"; font.pixelSize: 10 }
                            MouseArea {
                                id: modResetBtn; anchors.fill: parent
                                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    page.modLoader = ""; page.modGameVersion = ""
                                    page.modCategory = ""; page.modEnvironment = ""
                                    modInput.text = ""; modResultsModel.clear()
                                    doModSearch()
                                }
                            }
                        }
                    }

                    // Row 2: 加载器 | 版本 | 类型 | 环境 | 预发布
                    RowLayout {
                        Layout.fillWidth: true; spacing: 3

                        // ── 加载器 ──
                        Text { text: qsTr("加载器"); color: "#9094a8"; font.pixelSize: 11; Layout.preferredWidth: 40 }
                        Rectangle {
                            id: modLdrPill
                            Layout.preferredWidth: 95; height: 28; radius: 5
                            property bool hovered: false
                            property bool menuOpen: false
                            color: (hovered || menuOpen) ? "#1e3260" : "#0c0e14"
                            border.color: (hovered || menuOpen || page.modLoader) ? "#5078e0" : "#2a3040"
                            border.width: (hovered || menuOpen || page.modLoader) ? 1.5 : 1

                            property real _eScale: 1.0
                            scale: _eScale
                            Timer { id: ldrRestoreTimer; interval: 100; onTriggered: { modLdrPill._eScale = 1.0 } }

                            Behavior on border.color { ColorAnimation { duration: 150 } }
                            Behavior on border.width { NumberAnimation { duration: 150 } }
                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on _eScale { SpringAnimation { spring: 1.8; damping: 0.25; epsilon: 0.01 } }

                            RowLayout {
                                anchors.fill: parent; anchors.leftMargin: 5; anchors.rightMargin: 2; spacing: 1
                                Text {
                                    Layout.fillWidth: true
                                    text: page.modLoaderLabels[page.modLoader] || "全部"
                                    color: page.modLoader ? "#8aaeff" : "#788090"; font.pixelSize: 11
                                    elide: Text.ElideRight
                                }
                                Text { text: "▾"; color: "#505468"; font.pixelSize: 8 }
                            }
                            MouseArea {
                                id: ldrHov; anchors.fill: parent
                                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onEntered: modLdrPill.hovered = true
                                onExited: modLdrPill.hovered = false
                                onClicked: {
                                    modLdrPill.hovered = false
                                    modLdrPill._eScale = 0.9
                                    ldrRestoreTimer.restart()
                                    if (ldrMenu.visible) ldrMenu.close(); else ldrMenu.open()
                                }
                            }

                            Popup {
                                id: ldrMenu; closePolicy: Popup.NoAutoClose
                                y: parent.height + 4; width: 110
                                height: Math.min(ldrMenuFlick.contentHeight + 8, 220)
                                padding: 0
                                onOpened: modLdrPill.menuOpen = true
                                onClosed: { modLdrPill.menuOpen = false; modLdrPill.hovered = false }

                                enter: Transition {
                                    ParallelAnimation {
                                        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180; easing.type: Easing.OutCubic }
                                        NumberAnimation { property: "y"; from: ldrMenu.y - 4; to: ldrMenu.y; duration: 180; easing.type: Easing.OutCubic }
                                    }
                                }
                                exit: Transition {
                                    NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120 }
                                }

                                background: Rectangle { color: "#151922"; radius: 8; border.color: "#1e2230" }

                                Flickable {
                                    id: ldrMenuFlick
                                    anchors.fill: parent; anchors.margins: 4
                                    contentHeight: ldrContent.implicitHeight; clip: true
                                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                    ColumnLayout {
                                        id: ldrContent; width: parent.width; spacing: 1
                                        Repeater {
                                            model: ["", "fabric", "forge", "quilt", "neoforge", "rift", "liteloader"]
                                            Rectangle {
                                                Layout.fillWidth: true; height: 26; radius: 4
                                                color: modelData === page.modLoader ? "#1a2848" : "transparent"
                                                Text {
                                                    anchors.centerIn: parent
                                                    text: page.modLoaderLabels[modelData] || modelData
                                                    color: modelData === page.modLoader ? "#5068c8" : "#9094a8"; font.pixelSize: 11
                                                    font.weight: modelData === page.modLoader ? Font.Bold : Font.Normal
                                                }
                                                MouseArea {
                                                    anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                                    onClicked: { page.modLoader = modelData; ldrMenu.close() }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // ── 版本 ──
                        Text { text: qsTr("版本"); color: "#9094a8"; font.pixelSize: 11; Layout.preferredWidth: 28 }
                        Rectangle {
                            id: modVerPill
                            Layout.preferredWidth: 95; height: 28; radius: 5
                            property bool hovered: false
                            property bool menuOpen: false
                            color: (hovered || menuOpen) ? "#1e3260" : "#0c0e14"
                            border.color: (hovered || menuOpen || page.modGameVersion) ? "#5078e0" : "#2a3040"
                            border.width: (hovered || menuOpen || page.modGameVersion) ? 1.5 : 1

                            property real _eScale: 1.0
                            scale: _eScale
                            Timer { id: verRestoreTimer; interval: 100; onTriggered: { modVerPill._eScale = 1.0 } }

                            Behavior on border.color { ColorAnimation { duration: 150 } }
                            Behavior on border.width { NumberAnimation { duration: 150 } }
                            Behavior on _eScale { SpringAnimation { spring: 1.8; damping: 0.25; epsilon: 0.01 } }

                            RowLayout {
                                anchors.fill: parent; anchors.leftMargin: 5; anchors.rightMargin: 2; spacing: 1
                                Text {
                                    Layout.fillWidth: true
                                    text: page.modGameVersion || "全部"
                                    color: page.modGameVersion ? "#8aaeff" : "#788090"; font.pixelSize: 11
                                    elide: Text.ElideRight
                                }
                                Text { text: "▾"; color: "#505468"; font.pixelSize: 8 }
                            }
                            MouseArea {
                                id: verHov2; anchors.fill: parent
                                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onEntered: modVerPill.hovered = true
                                onExited: modVerPill.hovered = false
                                onClicked: {
                                    modVerPill.hovered = false
                                    modVerPill._eScale = 0.9
                                    verRestoreTimer.restart()
                                    if (verMenu.visible) verMenu.close(); else verMenu.open()
                                }
                            }

                            Popup {
                                id: verMenu; closePolicy: Popup.NoAutoClose
                                y: parent.height + 4; width: 130
                                height: Math.min(verFlick.contentHeight + 8, 220)
                                padding: 0
                                onOpened: modVerPill.menuOpen = true
                                onClosed: { modVerPill.menuOpen = false; modVerPill.hovered = false }

                                enter: Transition {
                                    ParallelAnimation {
                                        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180; easing.type: Easing.OutCubic }
                                        NumberAnimation { property: "y"; from: verMenu.y - 4; to: verMenu.y; duration: 180; easing.type: Easing.OutCubic }
                                    }
                                }
                                exit: Transition {
                                    NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120 }
                                }

                                background: Rectangle { color: "#151922"; radius: 8; border.color: "#1e2230" }

                                Flickable {
                                    id: verFlick
                                    anchors.fill: parent; anchors.margins: 4
                                    contentHeight: verContent.implicitHeight; clip: true
                                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                    ColumnLayout {
                                        id: verContent; width: parent.width; spacing: 1
                                        Rectangle {
                                            Layout.fillWidth: true; height: 26; radius: 4
                                            color: !page.modGameVersion ? "#1a2848" : "transparent"
                                            Text {
                                                anchors.centerIn: parent; text: qsTr("全部")
                                                color: !page.modGameVersion ? "#5068c8" : "#9094a8"; font.pixelSize: 11
                                                font.weight: !page.modGameVersion ? Font.Bold : Font.Normal
                                            }
                                            MouseArea {
                                                anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                                onClicked: { page.modGameVersion = ""; verMenu.close() }
                                            }
                                        }
                                        Repeater {
                                            model: {
                                                if (!backend || !backend.versionIds) return []
                                                var seen = []
                                                var groups = []
                                                for (var vi = 0; vi < backend.versionIds.length; vi++) {
                                                    var vv = backend.versionIds[vi]
                                                    if (!page.modShowPreReleases && !/^[0-9.]+$/.test(vv)) continue
                                                    var major = vv.split(/[.\-]/).slice(0, 2).join(".")
                                                    if (seen.indexOf(major) < 0) {
                                                        seen.push(major)
                                                        groups.push(major)
                                                    }
                                                    if (groups.length >= 30) break
                                                }
                                                return groups
                                            }
                                            Rectangle {
                                                Layout.fillWidth: true; height: 26; radius: 4
                                                color: modelData === page.modGameVersion ? "#1a2848" : "transparent"
                                                Text {
                                                    anchors.centerIn: parent; text: modelData
                                                    color: modelData === page.modGameVersion ? "#5068c8" : "#9094a8"; font.pixelSize: 11
                                                    font.weight: modelData === page.modGameVersion ? Font.Bold : Font.Normal
                                                }
                                                MouseArea {
                                                    anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                                    onClicked: { page.modGameVersion = modelData; verMenu.close() }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // ── 类型 ──
                        Text { text: qsTr("类型"); color: "#9094a8"; font.pixelSize: 11; Layout.preferredWidth: 28 }
                        Rectangle {
                            id: modCatPill
                            Layout.fillWidth: true; Layout.maximumWidth: 95; height: 28; radius: 5
                            property bool hovered: false
                            property bool menuOpen: false
                            color: (hovered || menuOpen) ? "#1e3260" : "#0c0e14"
                            border.color: (hovered || menuOpen || page.modCategory) ? "#5078e0" : "#2a3040"
                            border.width: (hovered || menuOpen || page.modCategory) ? 1.5 : 1

                            property real _eScale: 1.0
                            scale: _eScale
                            Timer { id: catRestoreTimer; interval: 100; onTriggered: { modCatPill._eScale = 1.0 } }

                            Behavior on border.color { ColorAnimation { duration: 150 } }
                            Behavior on border.width { NumberAnimation { duration: 150 } }
                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on _eScale { SpringAnimation { spring: 1.8; damping: 0.25; epsilon: 0.01 } }

                            RowLayout {
                                anchors.fill: parent; anchors.leftMargin: 5; anchors.rightMargin: 2; spacing: 1
                                Text {
                                    Layout.fillWidth: true
                                    text: page.modCatLabels[page.modCategory] || "全部"
                                    color: page.modCategory ? "#8aaeff" : "#788090"; font.pixelSize: 11
                                    elide: Text.ElideRight
                                }
                                Text { text: "▾"; color: "#505468"; font.pixelSize: 8 }
                            }
                            MouseArea {
                                id: catHov; anchors.fill: parent
                                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onEntered: modCatPill.hovered = true
                                onExited: modCatPill.hovered = false
                                onClicked: {
                                    modCatPill.hovered = false
                                    modCatPill._eScale = 0.9
                                    catRestoreTimer.restart()
                                    if (catMenu.visible) catMenu.close(); else catMenu.open()
                                }
                            }

                            Popup {
                                id: catMenu; closePolicy: Popup.NoAutoClose
                                y: parent.height + 4; width: 140
                                height: Math.min(catFlick.contentHeight + 8, 300)
                                padding: 0
                                onOpened: modCatPill.menuOpen = true
                                onClosed: { modCatPill.menuOpen = false; modCatPill.hovered = false }

                                enter: Transition {
                                    ParallelAnimation {
                                        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180; easing.type: Easing.OutCubic }
                                        NumberAnimation { property: "y"; from: catMenu.y - 4; to: catMenu.y; duration: 180; easing.type: Easing.OutCubic }
                                    }
                                }
                                exit: Transition {
                                    NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120 }
                                }

                                background: Rectangle { color: "#151922"; radius: 8; border.color: "#1e2230" }

                                Flickable {
                                    id: catFlick
                                    anchors.fill: parent; anchors.margins: 4
                                    contentHeight: catContent.implicitHeight; clip: true
                                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                    ColumnLayout {
                                        id: catContent; width: parent.width; spacing: 1
                                        Rectangle {
                                            Layout.fillWidth: true; height: 26; radius: 4
                                            color: !page.modCategory ? "#1a2848" : "transparent"
                                            Text {
                                                anchors.centerIn: parent; text: qsTr("全部")
                                                color: !page.modCategory ? "#5068c8" : "#9094a8"; font.pixelSize: 11
                                                font.weight: !page.modCategory ? Font.Bold : Font.Normal
                                            }
                                            MouseArea {
                                                anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                                onClicked: { page.modCategory = ""; catMenu.close() }
                                            }
                                        }
                                        Repeater {
                                            model: ["adventure","cursed","decoration","economy","equipment","food","game-mechanics","library","magic","management","minigame","mobs","optimization","social","storage","technology","transportation","utility","world-generation"]
                                            Rectangle {
                                                Layout.fillWidth: true; height: 26; radius: 4
                                                color: modelData === page.modCategory ? "#1a2848" : "transparent"
                                                Text {
                                                    anchors.centerIn: parent
                                                    text: page.modCatLabels[modelData] || modelData
                                                    color: modelData === page.modCategory ? "#5068c8" : "#9094a8"; font.pixelSize: 11
                                                    font.weight: modelData === page.modCategory ? Font.Bold : Font.Normal
                                                }
                                                MouseArea {
                                                    anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                                    onClicked: { page.modCategory = modelData; catMenu.close() }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // ── 环境 ──
                        Text { text: qsTr("环境"); color: "#9094a8"; font.pixelSize: 10; Layout.preferredWidth: 26 }
                        Rectangle {
                            id: modEnvPill
                            Layout.preferredWidth: 72; height: 24; radius: 4
                            color: envHov.containsMouse ? "#1a2848" : "#0c0e14"
                            border.color: (envHov.containsMouse || page.modEnvironment) ? "#4068c8" : "#2a3040"
                            border.width: (envHov.containsMouse || page.modEnvironment) ? 1.5 : 1

                            property real _eScale: 1.0
                            scale: _eScale
                            Timer { id: envRestoreTimer; interval: 100; onTriggered: { modEnvPill._eScale = 1.0 } }

                            Behavior on border.color { ColorAnimation { duration: 150 } }
                            Behavior on border.width { NumberAnimation { duration: 150 } }
                            Behavior on _eScale { SpringAnimation { spring: 1.8; damping: 0.25; epsilon: 0.01 } }

                            RowLayout {
                                anchors.fill: parent; anchors.leftMargin: 4; anchors.rightMargin: 2; spacing: 1
                                Text {
                                    Layout.fillWidth: true
                                    text: page.modEnvLabels[page.modEnvironment] || "全部"
                                    color: page.modEnvironment ? "#8aaeff" : "#788090"; font.pixelSize: 10
                                    elide: Text.ElideRight
                                }
                                Text { text: "▾"; color: "#505468"; font.pixelSize: 7 }
                            }
                            MouseArea {
                                id: envHov; anchors.fill: parent
                                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    modEnvPill._eScale = 0.9
                                    envRestoreTimer.restart()
                                    if (envMenu.visible) envMenu.close(); else envMenu.open()
                                }
                            }

                            Popup {
                                id: envMenu; closePolicy: Popup.NoAutoClose
                                y: parent.height + 4; width: 120
                                height: Math.min(envFlick.contentHeight + 8, 160)
                                padding: 0

                                enter: Transition {
                                    ParallelAnimation {
                                        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180; easing.type: Easing.OutCubic }
                                        NumberAnimation { property: "y"; from: envMenu.y - 4; to: envMenu.y; duration: 180; easing.type: Easing.OutCubic }
                                    }
                                }
                                exit: Transition {
                                    NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120 }
                                }

                                background: Rectangle { color: "#151922"; radius: 8; border.color: "#1e2230" }

                                Flickable {
                                    id: envFlick
                                    anchors.fill: parent; anchors.margins: 4
                                    contentHeight: envContent.implicitHeight; clip: true
                                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                    ColumnLayout {
                                        id: envContent; width: parent.width; spacing: 1
                                        Repeater {
                                            model: ["", "required", "optional", "unsupported"]
                                            Rectangle {
                                                Layout.fillWidth: true; height: 26; radius: 4
                                                color: modelData === page.modEnvironment ? "#1a2848" : "transparent"
                                                Text {
                                                    anchors.centerIn: parent
                                                    text: page.modEnvLabels[modelData] || modelData
                                                    color: modelData === page.modEnvironment ? "#5068c8" : "#9094a8"; font.pixelSize: 11
                                                    font.weight: modelData === page.modEnvironment ? Font.Bold : Font.Normal
                                                }
                                                MouseArea {
                                                    anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                                    onClicked: { page.modEnvironment = modelData; envMenu.close() }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // ── 含预发布 ──
                        Rectangle {
                            width: 20; height: 24; radius: 4
                            color: page.modShowPreReleases ? "#1a2848" : "#0c0e14"
                            border.color: (betaHov.hovered || page.modShowPreReleases) ? "#4068c8" : "#2a3040"
                            border.width: (betaHov.hovered || page.modShowPreReleases) ? 1.5 : 1
                            Behavior on border.color { ColorAnimation { duration: 150 } }
                            Behavior on border.width { NumberAnimation { duration: 150 } }
                            Text {
                                anchors.centerIn: parent
                                text: page.modShowPreReleases ? "✓" : "✕"
                                color: page.modShowPreReleases ? "#8aaeff" : "#505468"; font.pixelSize: 9
                            }
                            MouseArea {
                                id: betaHov; anchors.fill: parent
                                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { page.modShowPreReleases = !page.modShowPreReleases }
                            }
                        }
                    }
                }
            }

            // ── Results ──
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Text {
                    visible: page.modSearching
                    anchors.centerIn: parent
                    text: qsTr("搜索中…"); color: "#606478"; font.pixelSize: 12
                }
                Text {
                    visible: !page.modSearching && modResultsModel.count === 0
                    anchors.centerIn: parent
                    text: qsTr("输入关键词搜索 Mod"); color: "#606478"; font.pixelSize: 12
                }

                Flickable {
                    anchors.fill: parent
                    contentHeight: modGrid.implicitHeight
                    clip: true
                    visible: modResultsModel.count > 0
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    ColumnLayout {
                        id: modGrid; width: parent.width; spacing: 6

                        Repeater {
                            model: modResultsModel

                            Rectangle {
                                id: modItem
                                Layout.fillWidth: true
                                height: 64; radius: 8
                                color: modItemMA.containsMouse ? "#161a26" : "#161922"
                                border.color: modItemMA.containsMouse ? "#4068c8" : "#1e2230"
                                border.width: modItemMA.containsMouse ? 1.5 : 1

                                // Entrance animation
                                opacity: 0
                                Component.onCompleted: { opacity = 1 }

                                // Elastic shake on click
                                property bool _shaking: false
                                x: _shaking ? shakeAnim.running ? 0 : 0 : 0
                                SequentialAnimation {
                                    id: shakeAnim
                                    running: modItem._shaking
                                    PropertyAnimation { target: modItem; property: "x"; from: 0; to: 4; duration: 50; easing.type: Easing.OutQuad }
                                    PropertyAnimation { target: modItem; property: "x"; to: -4; duration: 50; easing.type: Easing.OutQuad }
                                    PropertyAnimation { target: modItem; property: "x"; to: 2; duration: 50; easing.type: Easing.OutQuad }
                                    PropertyAnimation { target: modItem; property: "x"; to: 0; duration: 50; easing.type: Easing.OutQuad }
                                    onStopped: { modItem._shaking = false }
                                }

                                Behavior on color { ColorAnimation { duration: 150 } }
                                Behavior on border.color { ColorAnimation { duration: 150 } }
                                Behavior on border.width { NumberAnimation { duration: 150 } }
                                Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }

                                RowLayout {
                                    anchors.fill: parent; anchors.margins: 8; spacing: 8

                                    // Icon
                                    Rectangle {
                                        Layout.preferredWidth: 36; Layout.preferredHeight: 36; radius: 6
                                        color: "#0c0e14"; clip: true

                                        Image {
                                            anchors.fill: parent; anchors.margins: 2
                                            fillMode: Image.PreserveAspectFit
                                            asynchronous: true; cache: true
                                            sourceSize.width: 72; sourceSize.height: 72
                                            Component.onCompleted: {
                                                if (model && model.icon) {
                                                    var url = model.icon
                                                    url = url.replace("cdn.modrinth.com", "mod.mcimirror.top")
                                                    source = url
                                                }
                                            }
                                            onStatusChanged: {
                                                if (status === Image.Error) iconFallback.visible = true
                                                else if (status === Image.Ready) iconFallback.visible = false
                                            }
                                        }
                                        Text {
                                            id: iconFallback
                                            anchors.centerIn: parent
                                            text: model ? (model.title ? model.title[0] : "M") : "M"
                                            color: "#5068c8"; font.pixelSize: 16; font.bold: true
                                            visible: !model || !model.icon
                                        }
                                    }

                                    // Info + downloads
                                    ColumnLayout {
                                        Layout.fillWidth: true; spacing: 1
                                        Layout.alignment: Qt.AlignVCenter

                                        RowLayout {
                                            Layout.fillWidth: true; spacing: 4
                                            Text {
                                                Layout.fillWidth: true
                                                text: model.title || ""; color: "#d0d4e0"
                                                font.pixelSize: 12; font.bold: true; elide: Text.ElideRight
                                            }
                                            Rectangle {
                                                visible: model.loader
                                                width: ldrBadge.implicitWidth + 8; height: 15; radius: 3
                                                color: "#1a2848"
                                                Text {
                                                    id: ldrBadge; anchors.centerIn: parent
                                                    text: model.loader || ""; color: "#8aaeff"; font.pixelSize: 8
                                                }
                                            }
                                            Rectangle {
                                                visible: model.clientSide
                                                width: cliBadge.implicitWidth + 8; height: 15; radius: 3
                                                color: "#282038"
                                                Text {
                                                    id: cliBadge; anchors.centerIn: parent
                                                    text: page.modEnvLabels[model.clientSide] || model.clientSide || ""
                                                    color: "#c0a0e0"; font.pixelSize: 8
                                                }
                                            }
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: model.desc || ""; color: "#606478"
                                            font.pixelSize: 10; elide: Text.ElideRight; maximumLineCount: 1
                                        }

                                        Text {
                                            text: formatDownloads(model.downloads || 0); color: "#788090"; font.pixelSize: 10
                                        }
                                    }
                                }

                                // MouseArea on top of children for reliable hover
                                MouseArea {
                                    id: modItemMA
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        modItem._shaking = true
                                        page._modDetailSlug = model.slug
                                        page._modDetailTitle = model.title || ""
                                        page._modDetailDesc = model.desc || ""
                                        page._modDetailIcon = model.icon || ""
                                        page._showModDetail = true
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    // TAB 2: 光影
    // ════════════════════════════════════════════
    Item {
        anchors.top: tabBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.topMargin: 8
        opacity: page.currentTab === 2 ? 1 : 0
        enabled: page.currentTab === 2
        Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

        function doShaderSearch() {
            if (!backend) return
            page.shaderSearching = true
            shaderResultsModel.clear()
            var q = shaderInput.text ? shaderInput.text.trim() : ""
            backend.searchShaders(q, page.shaderGameVersion)
        }

        function formatDownloads(n) {
            if (n >= 1000000) return (n / 1000000).toFixed(1) + "M"
            if (n >= 1000) return (n / 1000).toFixed(1) + "K"
            return n + ""
        }

        Connections {
            target: backend
            enabled: backend !== null
            function onSearchResultsReady(results) {
                if (page.currentTab !== 2) return  // shader-only
                shaderResultsModel.clear()
                for (var j = 0; j < results.length; j++) {
                    var r = results[j]
                    shaderResultsModel.append({
                        slug: r.slug || "",
                        title: r.title || r.slug || "Unknown",
                        desc: r.desc || "",
                        icon: r.icon || "",
                        downloads: r.downloads || 0,
                        clientSide: r.clientSide || ""
                    })
                }
                page.shaderSearching = false
            }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8

            // ── Filter Card ──
            Rectangle {
                Layout.fillWidth: true; height: 60; radius: 10
                color: "#11141c"; border.color: "#1e2230"

                RowLayout {
                    anchors.fill: parent; anchors.margins: 10; spacing: 6

                    // Search
                    Rectangle {
                        id: shaderSearchBox
                        Layout.fillWidth: true; height: 26; radius: 4
                        color: "#0c0e14"
                        border.color: (shaderInput.activeFocus || shaderSearchBoxHov.hovered) ? "#5068c8" : "#2a3040"
                        border.width: (shaderInput.activeFocus || shaderSearchBoxHov.hovered) ? 1.5 : 1
                        Behavior on border.color { ColorAnimation { duration: 200 } }
                        Behavior on border.width { NumberAnimation { duration: 150 } }

                        HoverHandler { id: shaderSearchBoxHov }

                        TextInput {
                            id: shaderInput
                            anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8
                            color: "#d0d4e0"; verticalAlignment: TextInput.AlignVCenter; font.pixelSize: 12
                            Keys.onReturnPressed: doShaderSearch()

                            Text {
                                anchors.fill: parent; verticalAlignment: Text.AlignVCenter
                                text: qsTr("搜索光影…"); color: "#505468"; font.pixelSize: 11
                                visible: !shaderInput.text
                            }
                        }
                    }

                    // Search button
                    Rectangle {
                        width: 44; height: 26; radius: 4
                        color: sSbtn.hovered ? "#5a78e0" : "#5068c8"
                        scale: sSbtn.pressed ? 0.92 : (sSbtn.hovered ? 1.06 : 1.0)
                        Behavior on color { ColorAnimation { duration: 150 } }
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutBack } }
                        Text { anchors.centerIn: parent; text: qsTr("搜索"); color: "#fff"; font.pixelSize: 10; font.bold: true }
                        MouseArea {
                            id: sSbtn; anchors.fill: parent
                            hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            onClicked: doShaderSearch()
                        }
                    }

                    // Version dropdown
                    Text { text: qsTr("版本"); color: "#9094a8"; font.pixelSize: 10 }
                    Rectangle {
                        Layout.preferredWidth: 80; height: 26; radius: 4
                        color: sVerHov.hovered ? "#1a2848" : "#0c0e14"
                        border.color: page.shaderGameVersion ? "#5068c8" : "#2a3040"; border.width: 1

                        RowLayout {
                            anchors.fill: parent; anchors.leftMargin: 5; anchors.rightMargin: 2; spacing: 1
                            Text {
                                Layout.fillWidth: true
                                text: page.shaderGameVersion || "全部"
                                color: page.shaderGameVersion ? "#8aaeff" : "#788090"; font.pixelSize: 10
                                elide: Text.ElideRight
                            }
                            Text { text: "▾"; color: "#505468"; font.pixelSize: 7 }
                        }
                        MouseArea {
                            id: sVerHov; anchors.fill: parent
                            hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            onClicked: { if (sVerMenu.visible) sVerMenu.close(); else sVerMenu.open() }
                        }

                        Popup {
                            id: sVerMenu; closePolicy: Popup.NoAutoClose
                            y: parent.height + 4; width: 130
                            height: Math.min(sVerFlick.contentHeight + 8, 220)
                            padding: 0
                            background: Rectangle { color: "#151922"; radius: 8; border.color: "#1e2230" }

                            Flickable {
                                id: sVerFlick
                                anchors.fill: parent; anchors.margins: 4
                                contentHeight: sVerInner.implicitHeight; clip: true
                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                ColumnLayout {
                                    id: sVerInner; width: parent.width; spacing: 1
                                    Rectangle {
                                        Layout.fillWidth: true; height: 26; radius: 4
                                        color: !page.shaderGameVersion ? "#1a2848" : "transparent"
                                        Text {
                                            anchors.centerIn: parent; text: qsTr("全部")
                                            color: !page.shaderGameVersion ? "#5068c8" : "#9094a8"; font.pixelSize: 11
                                            font.weight: !page.shaderGameVersion ? Font.Bold : Font.Normal
                                        }
                                        MouseArea {
                                            anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                            onClicked: { page.shaderGameVersion = ""; sVerMenu.close() }
                                        }
                                    }
                                    Repeater {
                                        model: {
                                            if (!backend || !backend.versionIds) return []
                                            var seen = []
                                            var groups = []
                                            for (var vi = 0; vi < backend.versionIds.length; vi++) {
                                                var vv = backend.versionIds[vi]
                                                if (!page.shaderShowPreReleases && !/^[0-9.]+$/.test(vv)) continue
                                                var major = vv.split(/[.\-]/).slice(0, 2).join(".")
                                                if (seen.indexOf(major) < 0) {
                                                    seen.push(major); groups.push(major)
                                                }
                                                if (groups.length >= 30) break
                                            }
                                            return groups
                                        }
                                        Rectangle {
                                            Layout.fillWidth: true; height: 26; radius: 4
                                            color: modelData === page.shaderGameVersion ? "#1a2848" : "transparent"
                                            Text {
                                                anchors.centerIn: parent; text: modelData
                                                color: modelData === page.shaderGameVersion ? "#5068c8" : "#9094a8"; font.pixelSize: 11
                                                font.weight: modelData === page.shaderGameVersion ? Font.Bold : Font.Normal
                                            }
                                            MouseArea {
                                                anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                                onClicked: { page.shaderGameVersion = modelData; sVerMenu.close() }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Pre-release toggle
                    Rectangle {
                        width: 20; height: 26; radius: 4
                        color: page.shaderShowPreReleases ? "#1a2848" : "#0c0e14"
                        border.color: page.shaderShowPreReleases ? "#5068c8" : "#2a3040"; border.width: 1
                        Text {
                            anchors.centerIn: parent
                            text: page.shaderShowPreReleases ? "✓" : "✕"
                            color: page.shaderShowPreReleases ? "#8aaeff" : "#505468"; font.pixelSize: 9
                        }
                        MouseArea {
                            anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            onClicked: { page.shaderShowPreReleases = !page.shaderShowPreReleases }
                        }
                    }

                    // Reset
                    Rectangle {
                        width: 44; height: 26; radius: 4
                        color: sRbtn.hovered ? "#2a2030" : "#1a1420"
                        border.color: "#3a2840"; border.width: 1
                        scale: sRbtn.pressed ? 0.92 : (sRbtn.hovered ? 1.06 : 1.0)
                        Behavior on color { ColorAnimation { duration: 150 } }
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutBack } }
                        Text { anchors.centerIn: parent; text: qsTr("重置"); color: "#b090c8"; font.pixelSize: 10 }
                        MouseArea {
                            id: sRbtn; anchors.fill: parent
                            hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                page.shaderGameVersion = ""
                                shaderInput.text = ""
                                shaderResultsModel.clear()
                            }
                        }
                    }
                }
            }

            // ── Results ──
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Text {
                    visible: page.shaderSearching
                    anchors.centerIn: parent
                    text: qsTr("搜索中…"); color: "#606478"; font.pixelSize: 12
                }
                Text {
                    visible: !page.shaderSearching && shaderResultsModel.count === 0
                    anchors.centerIn: parent
                    text: qsTr("输入关键词搜索光影"); color: "#606478"; font.pixelSize: 12
                }

                Flickable {
                    anchors.fill: parent
                    contentHeight: sGrid.implicitHeight
                    clip: true
                    visible: shaderResultsModel.count > 0
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    ColumnLayout {
                        id: sGrid; width: parent.width; spacing: 6

                        Repeater {
                            model: shaderResultsModel

                            Rectangle {
                                id: shaderItem
                                Layout.fillWidth: true
                                height: 64; radius: 8
                                color: sHov.hovered ? "#1a2030" : "#161922"
                                border.color: "#1e2230"; border.width: 1
                                scale: sHov.hovered ? 1.015 : 1.0

                                opacity: 0
                                Component.onCompleted: { opacity = 1 }

                                Behavior on color { ColorAnimation { duration: 150 } }
                                Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutBack } }
                                Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }

                                HoverHandler { id: sHov }
                                TapHandler { cursorShape: Qt.PointingHandCursor; onTapped: console.log("[shader] open " + model.slug) }

                                RowLayout {
                                    anchors.fill: parent; anchors.margins: 8; spacing: 8

                                    Rectangle {
                                        Layout.preferredWidth: 36; Layout.preferredHeight: 36; radius: 6
                                        color: "#0c0e14"; clip: true

                                        Image {
                                            anchors.fill: parent; anchors.margins: 2
                                            fillMode: Image.PreserveAspectFit
                                            asynchronous: true; cache: true
                                            sourceSize.width: 72; sourceSize.height: 72
                                            Component.onCompleted: {
                                                if (model && model.icon) {
                                                    var u = model.icon
                                                    u = u.replace("cdn.modrinth.com", "mod.mcimirror.top")
                                                    source = u
                                                }
                                            }
                                            onStatusChanged: {
                                                if (status === Image.Error) sIconFb.visible = true
                                                else if (status === Image.Ready) sIconFb.visible = false
                                            }
                                        }
                                        Text {
                                            id: sIconFb
                                            anchors.centerIn: parent
                                            text: model ? (model.title ? model.title[0] : "S") : "S"
                                            color: "#5068c8"; font.pixelSize: 16; font.bold: true
                                            visible: !model || !model.icon
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true; spacing: 1
                                        Layout.alignment: Qt.AlignVCenter

                                        RowLayout {
                                            Layout.fillWidth: true; spacing: 4
                                            Text {
                                                Layout.fillWidth: true
                                                text: model.title || ""; color: "#d0d4e0"
                                                font.pixelSize: 12; font.bold: true; elide: Text.ElideRight
                                            }
                                            Rectangle {
                                                visible: model.clientSide
                                                width: sCliLbl.implicitWidth + 8; height: 15; radius: 3
                                                color: "#282038"
                                                Text {
                                                    id: sCliLbl; anchors.centerIn: parent
                                                    text: page.modEnvLabels[model.clientSide] || model.clientSide || ""
                                                    color: "#c0a0e0"; font.pixelSize: 8
                                                }
                                            }
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: model.desc || ""; color: "#606478"
                                            font.pixelSize: 10; elide: Text.ElideRight; maximumLineCount: 1
                                        }
                                        Text {
                                            text: "📥 " + formatDownloads(model.downloads || 0); color: "#788090"; font.pixelSize: 10
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
    // TAB 3: 资源包
    // ════════════════════════════════════════════
    Item {
        anchors.top: tabBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.topMargin: 8
        opacity: page.currentTab === 3 ? 1 : 0
        enabled: page.currentTab === 3
        Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

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

                        Text { text: qsTr("名称"); color: "#9094a8"; font.pixelSize: 11; Layout.preferredWidth: 32 }

                        Rectangle {
                            Layout.fillWidth: true; height: 28; radius: 5
                            color: rpSearchInput.activeFocus ? "#0f131c" : "#0c0e14"
                            border.color: rpSearchInput.activeFocus ? "#5068c8" : "#2a3040"
                            border.width: 1
                            Behavior on color { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }

                            TextInput {
                                id: rpSearchInput
                                anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8
                                color: "#d0d4e0"; verticalAlignment: TextInput.AlignVCenter; font.pixelSize: 12
                                // REMOVED onAccepted trigger — search only on button click (Fix 1)

                                Text {
                                    anchors.fill: parent; verticalAlignment: Text.AlignVCenter
                                    text: qsTr("输入资源包名称..."); color: "#505468"; font.pixelSize: 11
                                    visible: !rpSearchInput.text
                                }
                            }
                        }

                        Text { text: qsTr("来源"); color: "#9094a8"; font.pixelSize: 11; Layout.preferredWidth: 32 }

                        Rectangle {
                            id: rpSourcePill
                            Layout.preferredWidth: 140; height: 28; radius: 5
                            color: (rpSrcHov.containsMouse || rpSourceMenu.visible) ? "#1e3260" : "#0c0e14"
                            border.color: (rpSrcHov.containsMouse || rpSourceMenu.visible || sourceActive) ? "#5078e0" : "#2a3040"; border.width: 1

                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on border.color { ColorAnimation { duration: 150 } }

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

                        Text { text: qsTr("版本"); color: "#9094a8"; font.pixelSize: 11; Layout.preferredWidth: 32 }

                        Rectangle {
                            id: rpVerPill
                            Layout.preferredWidth: 120; height: 28; radius: 5
                            color: (rpVerHov.containsMouse || rpVersionMenu.visible) ? "#1e3260" : "#0c0e14"
                            border.color: (rpVerHov.containsMouse || rpVersionMenu.visible || page.rpGameVersion) ? "#5078e0" : "#2a3040"; border.width: 1

                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on border.color { ColorAnimation { duration: 150 } }

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
                                                anchors.centerIn: parent; text: qsTr("全部")
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

                        Text { text: qsTr("筛选"); color: "#9094a8"; font.pixelSize: 11; Layout.preferredWidth: 32 }

                        // Three filter dropdowns: Category | Feature | Resolution
                        RowLayout {
                            Layout.fillWidth: true; spacing: 6

                            // ── Category dropdown ──
                            Rectangle {
                                id: rpCatPill
                                Layout.preferredWidth: 95; height: 28; radius: 5
                                color: (rpCatHov.containsMouse || rpCatMenu.visible) ? "#1e3260" : "#0c0e14"
                                border.color: (rpCatHov.containsMouse || rpCatMenu.visible || page.rpCategoryFilter) ? "#5078e0" : "#2a3040"; border.width: 1

                                Behavior on color { ColorAnimation { duration: 150 } }
                                Behavior on border.color { ColorAnimation { duration: 150 } }
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
                                color: (rpFeatHov.containsMouse || rpFeatMenu.visible) ? "#1e3260" : "#0c0e14"
                                border.color: (rpFeatHov.containsMouse || rpFeatMenu.visible || page.rpFeatureFilter) ? "#5078e0" : "#2a3040"; border.width: 1

                                Behavior on color { ColorAnimation { duration: 150 } }
                                Behavior on border.color { ColorAnimation { duration: 150 } }
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
                                color: (rpResHov.containsMouse || rpResMenu.visible) ? "#1e3260" : "#0c0e14"
                                border.color: (rpResHov.containsMouse || rpResMenu.visible || page.rpResolutionFilter) ? "#5078e0" : "#2a3040"; border.width: 1

                                Behavior on color { ColorAnimation { duration: 150 } }
                                Behavior on border.color { ColorAnimation { duration: 150 } }
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
                            Text { anchors.centerIn: parent; text: qsTr("搜索"); color: "#e8ecf8"; font.pixelSize: 12 }
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
                            Text { anchors.centerIn: parent; text: qsTr("重置"); color: "#9094a8"; font.pixelSize: 12 }
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
                text: qsTr("资源包 | 来源: MCIM (mcimirror.top) | ") + (rpTotalHits || rpResultsModel.count || 0) + " 个结果"
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
                        id: rpCard
                        width: rpListView.width - 8; height: 130; clip: true
                        color: rpCardHov.hovered ? "#121620" : "#0e1018"
                        radius: 10; border.color: rpCardHov.hovered ? "#5068c8" : "#1a1f2a"; border.width: 1
                        scale: rpCardHov.hovered ? 1.01 : 1.0

                        opacity: 0
                        Component.onCompleted: { opacity = 1 }

                        Behavior on color { ColorAnimation { duration: 150 } }
                        Behavior on border.color { ColorAnimation { duration: 150 } }
                        Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutBack } }
                        Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }

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

                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true; cache: true
                                    autoTransform: true
                                    sourceSize.width: 88; sourceSize.height: 88

                                    Component.onCompleted: {
                                        if (model && model.icon) {
                                            var url = model.icon
                                            url = url.replace("cdn.modrinth.com", "mod.mcimirror.top")
                                            url = url.replace("cdn-alt.modrinth.com", "mod.mcimirror.top")
                                            source = url
                                        }
                                    }
                                    onStatusChanged: {
                                        if (status === Image.Ready) { rpIconFallback.visible = false }
                                        else if (status === Image.Error) { rpIconFallback.visible = true }
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
                                                Text { anchors.centerIn: parent; text: qsTr("加载中"); color: "#404860"; font.pixelSize: 8 }
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
                                var iconUrl = (model.icon || "").replace("cdn.modrinth.com", "mod.mcimirror.top").replace("cdn-alt.modrinth.com", "mod.mcimirror.top")
                                page._rpDetailIconUrl = iconUrl
                                page._rpDetailAuthor = model.author || ""
                                page._rpDetailDesc = model.desc || ""
                                page._rpDetailSlug = model.slug
                                page._rpDetailTitle = model.title
                                page._rpDetailDownloads = model.downloads || 0
                                page._rpDetailUpdated = model.updated || ""
                                page._showRpDetail = true
                            }
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
    property var shaderDownloadingSlugs: ({})


    // Label maps for dropdowns (on root page for id-based access)
    property var modLoaderLabels: ({
        "": "全部", "fabric": "Fabric", "forge": "Forge",
        "quilt": "Quilt", "neoforge": "NeoForge", "rift": "Rift", "liteloader": "LiteLoader"
    })
    property var modCatLabels: ({
        "adventure": "冒险类", "cursed": "猎奇诡异类", "decoration": "装饰类",
        "economy": "经济系统类", "equipment": "装备武器类", "food": "食物食材类",
        "game-mechanics": "游戏机制类", "library": "前置依赖库", "magic": "魔法类",
        "management": "管理辅助类", "minigame": "迷你小游戏类", "mobs": "生物怪物类",
        "optimization": "性能优化类", "social": "社交交互类", "storage": "仓储存储类",
        "technology": "科技工业类", "transportation": "交通载具类", "utility": "实用工具类",
        "world-generation": "世界生成类"
    })
    property var modEnvLabels: ({
        "": "全部", "required": "客户端", "optional": "客户端+服务端", "unsupported": "纯服务端"
    })
    property string installingRpName: ""

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
                // Set chips on model items (for cards already rendered)
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

    // ── Resource Pack Detail (extracted to ResourcePackDetailPage.qml) ──
    property bool _showRpDetail: false
    property string _rpDetailSlug: ""
    property string _rpDetailTitle: ""
    property string _rpDetailIconUrl: ""
    property string _rpDetailAuthor: ""
    property string _rpDetailDesc: ""
    property int _rpDetailDownloads: 0
    property string _rpDetailUpdated: ""

    // ── RP Detail Overlay ──
    Rectangle {
        id: rpDetailOverlay
        anchors.fill: parent
        color: "#0c0f16"
        z: 10
        opacity: page._showRpDetail ? 1 : 0
        visible: opacity > 0

        Loader {
            id: rpDetailLoader
            anchors.fill: parent
            property bool _keepActive: false
            active: page._showRpDetail || _keepActive
            source: active ? "ResourcePackDetailPage.qml" : ""

            onLoaded: {
                _keepActive = true
                if (item) {
                    item.goBack.connect(function() { parent.opacity = 0; _showRpDetail = false; _keepActive = false })
                    item.backend = backend
                    item.toastManager = toastManager
                    item.mainWindow = mainWindow
                    item.rpDetailSlug = page._rpDetailSlug
                    item.rpDetailTitle = page._rpDetailTitle
                    item.rpDetailIconUrl = page._rpDetailIconUrl
                    item.rpDetailAuthor = page._rpDetailAuthor
                    item.rpDetailDesc = page._rpDetailDesc
                    item.rpDetailDownloads = page._rpDetailDownloads
                    item.rpDetailUpdated = page._rpDetailUpdated
                }
            }

            Connections {
                target: page
                function on_ShowRpDetailChanged() {
                    if (page._showRpDetail) {
                        rpDetailOverlay.opacity = Qt.binding(function() { return page._showRpDetail ? 1 : 0 })
                    } else {
                        rpUnloadTimer.start()
                    }
                }
            }

            Timer {
                id: rpUnloadTimer
                interval: 500
                onTriggered: { if (!page._showRpDetail) rpDetailLoader._keepActive = false }
            }
        }
    }

    // ── Mod Detail (extracted to ModDetailPage.qml) ──
    property bool _showModDetail: false
    property string _modDetailSlug: ""
    property string _modDetailTitle: ""
    property string _modDetailDesc: ""
    property string _modDetailIcon: ""

    // ── Mod Detail Overlay ──
    Rectangle {
        id: modDetailOverlay
        anchors.fill: parent
        color: "#0c0f16"
        z: 10
        opacity: page._showModDetail ? 1 : 0
        visible: opacity > 0

        Loader {
            id: modDetailLoader
            anchors.fill: parent
            property bool _keepActive: false
            active: page._showModDetail || _keepActive
            source: active ? "ModDetailPage.qml" : ""

            onLoaded: {
                _keepActive = true
                if (item) {
                    item.goBack.connect(function() { parent.opacity = 0; _showModDetail = false; _keepActive = false })
                    item.backend = backend
                    item.toastManager = toastManager
                    item.mainWindow = mainWindow
                    item.modDetailSlug = page._modDetailSlug
                    item.modDetailTitle = page._modDetailTitle
                    item.modDetailDesc = page._modDetailDesc
                    item.modDetailIcon = page._modDetailIcon
                }
            }

            Connections {
                target: page
                function on_ShowModDetailChanged() {
                    if (page._showModDetail) {
                        modDetailOverlay.opacity = Qt.binding(function() { return page._showModDetail ? 1 : 0 })
                    } else {
                        modUnloadTimer.start()
                    }
                }
            }

            Timer {
                id: modUnloadTimer
                interval: 500
                onTriggered: { if (!_showModDetail) modDetailLoader._keepActive = false }
            }
        }
    }

}
