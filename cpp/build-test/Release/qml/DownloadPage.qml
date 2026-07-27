// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

Rectangle {
    id: page
    readonly property bool hasBg: backend && typeof backend.customBgPath === "string" && backend.customBgPath.length > 0
    color: hasBg ? "transparent" : StyleTokens.bgPrimary

    Timer {
        id: javaInitTimer
        interval: 0
        running: true
        repeat: false
        onTriggered: {
            if (javaPage && typeof toastManager !== "undefined" && toastManager) {
                javaPage.toastManager = toastManager
            }
        }
    }

    // Reference back to the main window (set by Loader onLoaded)
    property var mainWindow: null

    // Signal for the flying ball animation — emitted to main window
    signal triggerDownloadBall(real sourceX, real sourceY)

    // ── Auto-test helpers (accessible from MainWindow loader.item) ──
    function toggleVersionMenu() {
        // Auto-test helper: ShadowDropdown is click-driven, no programmatic toggle needed
    }

    // Category tabs
    property int currentTab: 0  // 0=MC版本, 1=Mod, 2=光影, 3=资源包

    // MC Version state
    property string currentFilter: "release"  // release, snapshot, old, april_fools
    property string selectedVersionId: ""
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
        function onDownloadQueueFull(displayName) {
            if (toastManager) toastManager.show("当前并行任务已达到上限（" + displayName + "），请稍后再试")
        }
    }


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
    property bool rpSearching: false
    property bool rpShowPreReleases: false
    property bool rpHasMore: true
    property int rpTotalHits: 0
    readonly property int rpPageSize: 20

    property int modCurrentPage: 0
    property bool modHasMore: false
    readonly property int modPageSize: 30

    // Search resource packs with page number
    function searchRpPage(pageNum) {
        if (!backend) return
        pageNum = (pageNum !== undefined) ? pageNum : 0
        page.rpSearching = true
        page.rpPage = pageNum
        rpResultsModel.clear()
        if (page.mainWindow && page.mainWindow.loadingBar) {
            page.mainWindow.loadingBar.opacity = 1
        }
        var q = rpFilterCard.searchText || ""
        var ver = page.rpGameVersion || ""
        var cats = []
        if (page.rpCategoryFilter) cats.push(page.rpCategoryFilter)
        if (page.rpFeatureFilter) cats.push(page.rpFeatureFilter)
        if (page.rpResolutionFilter) cats.push(page.rpResolutionFilter)
        var offset = pageNum * page.rpPageSize
        console.log("[RP-DEBUG] searchRpPage page=", pageNum, "offset=", offset, "q=", q)
        backend.searchResourcepacks(q, ver, offset, cats)
    }

    function filterRpResults() {
        searchRpPage(0)
    }

    function loadResourcepackResults() {
        searchRpPage(0)
    }

    signal goBack()

    function loadModResults() {
        if (!backend) return
        modResultsModel.clear()
        console.log("[mod-ui] loadModResults loader=" + page.modLoader)
        page.modSearching = true
        var gv = page.modGameVersion ? [page.modGameVersion] : []
        backend.searchModsEx(
            "", page.modLoader,
            page.modCategory, gv,
            page.modEnvironment, page.modLicense,
            0, 30
        )
    }

    onCurrentTabChanged: {
        console.log("[RP-DEBUG] onCurrentTabChanged tab=", currentTab, "rpReady=", rpResultsReady)
        if (currentTab === 0) refreshVersionModel()
        if (currentTab === 1 && !modResultsReady) { loadModResults(); modResultsReady = true }
        if (currentTab === 2 && !_shaderLoaded) { _shaderLoaded = true; shaderTab.doSearch() }
        if (currentTab === 3 && !rpResultsReady) { loadResourcepackResults(); rpResultsReady = true }
    }
    property bool _shaderLoaded: false

    // ──── Animations ────
    opacity: 0
    y: 10
    Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeInDuration; easing.type: AnimationTokens.itemFadeInEasing } }
    Behavior on y { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
    Component.onCompleted: {
        console.log("[dlpage] loaded, t=" + Date.now())
        opacity = 1; y = 0
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

        function onInstallFinished(success) {
            page.installingVersion = false
            page.selectedVersionId = ""
            if (!success && page.appToast) {
                page.appToast.show("安装失败: 文件下载不完整", "", 5000)
            }
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

        // Re-scan local versions
        backend.refreshInstalled()

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
        else if (currentFilter === "april_fools") list = backend.aprilFoolVersions
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
    function getAprilFoolCount() { return backend ? backend.aprilFoolVersions.length : 0 }

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
        height: 28
        spacing: 4

        property var tabLabels: ["MC 版本", "Mod", "光影", "资源包", "Java"]

        Repeater {
            model: [
                { label: "MC 版本", icon: "box" },
                { label: "Mod", icon: "puzzle" },
                { label: "光影", icon: "sparkles" },
                { label: "资源包", icon: "palette" },
                { label: "Java", icon: "terminal" }
            ]
            Rectangle {
                Layout.preferredWidth: 84
                Layout.fillHeight: true
                radius: StyleTokens.radiusMd
                color: page.currentTab === index ? StyleTokens.bgCard : "transparent"
                border.color: page.currentTab === index ? StyleTokens.accent : "transparent"
                border.width: page.currentTab === index ? 1 : 0
                Behavior on color { ColorAnimation { duration: 200; easing.type: Easing.OutCubic } }
                Behavior on border.color { ColorAnimation { duration: 200; easing.type: Easing.OutCubic } }
                scale: tabMouse.containsMouse ? 1.04 : 1.0
                Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }

                Row {
                    anchors.centerIn: parent; spacing: 6
                    Image {
                        Layout.alignment: Qt.AlignVCenter
                        source: "icons/lucide/" + modelData.icon + ".svg"
                        width: 14; height: 14
                    }
                    Text {
                        text: modelData.label
                    color: page.currentTab === index ? "#d0d4e0" : "#606478"
                    Behavior on color { ColorAnimation { duration: 200; easing.type: Easing.OutCubic } }
                    font.pixelSize: StyleTokens.fontSizeSm
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
                        console.info("[UI] 切Tab " + (index < tabLabels.length ? tabLabels[index] : index))
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
        color: StyleTokens.bgInput
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
        opacity: page.currentTab === 0 ? 1 : 0
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
                    { key: "old", label: "远古版", icon: "landmark", countFn: function() { return page.getOldCount() } },
                    { key: "april_fools", label: "愚人节版", icon: "sparkles", countFn: function() { return page.getAprilFoolCount() } }
                ]

                Rectangle {
                    height: 30; radius: 15
                    Layout.preferredWidth: Math.max(70, Math.min(pillRow.implicitWidth + 24, 160))
                    Layout.minimumWidth: 70
                    color: page.currentFilter === modelData.key ? StyleTokens.accent : StyleTokens.bgSecondary
                    border.color: page.currentFilter === modelData.key ? StyleTokens.accent : StyleTokens.border
                    border.width: 1
                    clip: true
                    scale: pillMouse.containsMouse ? 1.04 : 1.0
                    Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }

                    Row {
                        id: pillRow
                        anchors.centerIn: parent
                        spacing: 4
                        Image {
                            Layout.alignment: Qt.AlignVCenter
                            source: "icons/lucide/" + modelData.icon + ".svg"
                            width: 14; height: 14
                        }
                        Text {
                            id: pillLabel
                            text: modelData.label
                            color: page.currentFilter === modelData.key ? StyleTokens.textPrimary : "#9094a8"
                            font.pixelSize: StyleTokens.fontSizeMd
                            elide: Text.ElideRight
                        }
                        Text {
                            id: pillCount
                            text: "(" + modelData.countFn() + ")"
                            color: page.currentFilter === modelData.key ? "#93acf0" : StyleTokens.textMuted
                            font.pixelSize: StyleTokens.fontSizeSm
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

            RefreshButton {
                visible: page.currentTab === 0
                onClicked: {
                    if (backend) {
                        toastManager.show("正在刷新...")
                        appWindow.pageLoading = true
                        versionModel.clear()
                        backend.refreshVersionList()
                    }
                }
            }

            Item { Layout.fillWidth: true }

        }

        // ── Latest version highlight ──
        Rectangle {
            id: latestHighlight
            anchors.top: filterRow.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 12
            anchors.topMargin: 8
            height: 72
            color: StyleTokens.bgSecondary
            radius: StyleTokens.radiusLg
            border.color: StyleTokens.bgElevated
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 0

                // Left: Release version
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        text: "最新正式版"
                        color: StyleTokens.accent
                        font.pixelSize: StyleTokens.fontSizeSm
                        font.bold: true
                        font.letterSpacing: 1
                    }

                    Text {
                        text: backend && backend.versionIds.length > 1 ? backend.versionIds[1] || "" : ""
                        color: "#d0d4e0"
                        font.pixelSize: StyleTokens.fontSize2xl
                        font.bold: true
                    }
                }

                // Divider
                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.fillHeight: true
                    Layout.margins: 16
                    color: StyleTokens.bgElevated
                }

                // Right: Snapshot version
                ColumnLayout {
                    Layout.alignment: Qt.AlignRight
                    spacing: 2

                    Text {
                        Layout.alignment: Qt.AlignRight
                        text: "最新快照版"
                        color: StyleTokens.textMuted
                        font.pixelSize: StyleTokens.fontSizeSm
                    }

                    Text {
                        Layout.alignment: Qt.AlignRight
                        text: backend ? backend.versionIds[0] || "" : ""
                        color: StyleTokens.textTertiary
                        font.pixelSize: StyleTokens.fontSizeLg
                        font.bold: true
                    }
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
            Component.onCompleted: contentItem.flickDeceleration = 250
            

            ListView {
                id: versionList
                anchors.fill: parent
                model: versionModel
                spacing: 2

                delegate: VersionCard {
                    width: versionList.width
                    versionId: model.versionId
                    versionType: model.vtype
                    isSelected: page.selectedVersionId === model.versionId
                    transparent: page.hasBg
                    onClicked: {
                        if (model.versionId) {
                            page.selectedVersionId = model.versionId
                            if (backend) backend.logMessage("[download-ui] card clicked: " + model.versionId + " -> InstallPage")
                            console.info("[UI] 安装版本 vid=" + model.versionId)
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
    // TAB 1: Mod // ════════════════════════════════════════════
    // ════════════════════════════════════════════
    Item {
        id: modTab
        anchors.top: tabBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.topMargin: 8
        opacity: page.currentTab === 1 ? 1 : 0
        visible: page.currentTab === 1
        enabled: page.currentTab === 1
        Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

        function doModSearch(pageNum) {
            if (!backend) return
            pageNum = (pageNum !== undefined) ? pageNum : 0
            page.modCurrentPage = pageNum
            page.modSearching = true
            modResultsModel.clear()
            var q = modFilterCard.searchText ? modFilterCard.searchText.trim() : ""
            console.log("[MOD-SEARCH] calling searchModsEx q=" + JSON.stringify(q) + " tab=" + page.currentTab + " page=" + pageNum)
            var gv = page.modGameVersion ? [page.modGameVersion] : []
            var offset = pageNum * page.modPageSize
            backend.searchModsEx(q, page.modLoader, page.modCategory, gv, page.modEnvironment, "", offset, page.modPageSize)
        }

        function fmtVersionRange(vs) {
            if (!vs || typeof vs !== "string" || vs.length === 0) return ""
            var parts = vs.split(","); var minV = null, maxV = null
            for (var i = 0; i < parts.length; i++) {
                var v = parts[i].trim(); if (!v) continue
                var segs = v.split("."); if (segs.length < 2) continue
                var key = segs[0] + "." + segs[1]
                if (!minV || key < minV) minV = key
                if (!maxV || key > maxV) maxV = key
            }
            if (!minV) return ""
            return minV === maxV ? "适用: " + minV : "适用: " + minV + "-" + maxV
        }
        function fmtDate(iso) { return iso ? iso.substring(0,10) : "" }
        function fmtLoaders(s) {
            if (!s) return ""
            var parts = s.split(",")
            for (var i = 0; i < parts.length; i++) {
                var t = parts[i].trim()
                if (t.length > 0) parts[i] = t.charAt(0).toUpperCase() + t.slice(1)
            }
            return "加载器：" + parts.filter(function(x){return x}).join(", ")
        }
        function formatDownloads(n) {
            if (n >= 100000000) return (n / 100000000).toFixed(1) + "亿"
            if (n >= 10000) return (n / 10000).toFixed(0) + "万"
            return (n || 0).toString()
        }

        property var modFilteredVersions: page.commonVersions || []

        Connections {
            target: backend
            enabled: backend !== null
            function onModSearchResultsReady(results) {
                modResultsModel.clear()
                var urlsToCache = []
                for (var j = 0; j < results.length; j++) {
                    var r = results[j]
                    var iconUrl = (r.icon || "").replace("cdn.modrinth.com", "mod.mcimirror.top").replace("cdn-alt.modrinth.com", "mod.mcimirror.top")
                    if (iconUrl && backend) {
                        iconUrl = backend.resolveIconUrl(iconUrl)
                        urlsToCache.push(iconUrl)
                    }
                    modResultsModel.append({
                        slug: r.slug || "",
                        title: r.title || r.slug || "Unknown",
                        desc: r.desc || "",
                        icon: iconUrl,
                        downloads: r.downloads || 0,
                        versions: (r.versions && r.versions.length ? r.versions.join(",") : ""),
                        dateModified: r.dateModified || "",
                        loader: r.loader || "",
                        loadersList: (r.loadersList || []).join(", "),
                        clientSide: r.clientSide || ""
                    })
                    if (r.loadersList && r.loadersList.length > 0) console.log("[MOD-QML] slug=" + (r.slug||"?") + " loadersList=" + JSON.stringify(r.loadersList))
                }
                page.modSearching = false
                page.modHasMore = !!(results && results.length >= page.modPageSize)
                if (urlsToCache.length > 0 && backend) {
                    backend.cacheIconBatchAsync(urlsToCache)
                }
            }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8

            // Filter Card
            FilterCard {
                id: modFilterCard
                Layout.fillWidth: true
                cardType: "mod"
                searchPlaceholder: qsTr("输入Mod名称...（仅支持英文搜索）")
                rawVersionIds: backend ? backend.versionIds : []
                showPreReleases: page.modShowPreReleases
                onPreReleaseToggled: { page.modShowPreReleases = showPreReleases; modTab.doModSearch() }
                // modLoaderLabels 含 "": "全部" 所以只加一次
                modLoaderModel: [""].concat(Object.keys(page.modLoaderLabels).filter(function(k) { return k !== "" }))
                modLoaderLabels: page.modLoaderLabels
                modCatModel: [""].concat(Object.keys(page.modCatLabels))
                modCatLabels: page.modCatLabels
                modEnvModel: ["", "client", "server"]
                modEnvLabels: page.modEnvLabels

                Component.onCompleted: {
                    modLoader = page.modLoader
                    modCategory = page.modCategory
                    modEnvironment = page.modEnvironment
                    mcVersion = page.modGameVersion
                }
                onSearchClicked: modTab.doModSearch()
                onResetClicked: {
                    modLoader = ""; modCategory = ""; modEnvironment = ""; mcVersion = ""
                    searchText = ""; modResultsModel.clear()
                    modTab.doModSearch()
                }
                onModLoaderChanged: page.modLoader = modLoader
                onModCategoryChanged: page.modCategory = modCategory
                onModEnvironmentChanged: page.modEnvironment = modEnvironment
                onMcVersionChanged: page.modGameVersion = mcVersion
            }

            // Results
            ScrollView {
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                ScrollBar.vertical.policy: ScrollBar.AsNeeded
                Component.onCompleted: contentItem.flickDeceleration = 250
                

                ListView {
                    id: modListView2
                    anchors.fill: parent; spacing: 6
                    model: modResultsModel
                    cacheBuffer: 200

                    header: LoadStatus {
                        width: modListView2.width
                        loading: page.modSearching
                        emptyText: qsTr("输入关键词搜索 Mod")
                        count: modResultsModel.count
                    }
                    footer: PaginationFooter {
                        currentPage: page.modCurrentPage
                        hasNext: page.modHasMore
                        loading: page.modSearching
                        onFirstClicked: modTab.doModSearch(0)
                        onPrevClicked: modTab.doModSearch(page.modCurrentPage - 1)
                        onNextClicked: modTab.doModSearch(page.modCurrentPage + 1)
                    }

                    delegate: DownloadCard {
                        width: modListView2.width - 8
                        title: model.title || ""
                        description: model.desc || ""
                        iconUrl: model.icon || ""
                        slug: model.slug || ""
                        downloads: model.downloads || 0
                        source: "Modrinth"
                        gameVersions: model.versions || ""
                        dateModified: model.dateModified || ""
                        loaders: (model.loadersList || model.loader || "")
                        onClicked: {
                            page._modDetailSlug = model.slug
                            page._modDetailTitle = model.title || ""
                            page._modDetailDesc = model.desc || ""
                            page._modDetailIcon = model.icon || ""
                            page._showModDetail = true
                            console.info("[UI] 打开 模组详情 slug=" + model.slug)
                        }
                    }
                }
            }
        }
    }


    // TAB 2: 光影
    // ════════════════════════════════════════════
    Item {
        id: shaderTab
        anchors.top: tabBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.topMargin: 8
        opacity: page.currentTab === 2 ? 1 : 0
        visible: page.currentTab === 2
        enabled: page.currentTab === 2
        Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

        property bool shaderSearching: false
        property int shaderOffset: 0
        property int shaderCurrentPage: 0
        readonly property int shaderPageSize: 50
        property bool hasMoreShaders: false

        property string shaderCategory: ""
        property string shaderFeature: ""
        property string shaderPerformance: ""
        property string shaderLoader: ""

        readonly property var shaderCats: [
            {label: "全部", slug: ""}, {label: "原版风格", slug: "vanilla-like"},
            {label: "幻想", slug: "fantasy"}, {label: "半写实", slug: "semi-realistic"},
            {label: "写实", slug: "realistic"}, {label: "卡通", slug: "cartoon"}, {label: "搞怪", slug: "cursed"}
        ]
        readonly property var shaderFeatures: [
            {label: "全部", slug: ""}, {label: "阴影", slug: "shadows"},
            {label: "泛光", slug: "bloom"}, {label: "反射", slug: "reflections"},
            {label: "植被", slug: "foliage"}, {label: "PBR 材质", slug: "pbr"},
            {label: "彩色光照", slug: "colored-lighting"}, {label: "路径追踪", slug: "path-tracing"}
        ]
        readonly property var shaderPerfs: [
            {label: "全部", slug: ""}, {label: "极低", slug: "potato"},
            {label: "低", slug: "low"}, {label: "中", slug: "medium"}, {label: "高", slug: "high"}
        ]
        readonly property var shaderLoaders: [
            {label: "全部", slug: ""}, {label: "Iris", slug: "iris"}, {label: "OptiFine", slug: "optifine"}
        ]

        function ddLabel(opts, key) {
            for (var j = 0; j < opts.length; j++)
                if (opts[j].slug === key) return opts[j].label
            return opts.length > 0 ? opts[0].label : ""
        }
        function fDownloads(n) {
            if (n >= 100000000) return (n/100000000).toFixed(1) + "亿"
            if (n >= 10000) return (n/10000).toFixed(0) + "万"
            return n.toString()
        }
        function fDate(iso) { return iso ? iso.substring(0,10) : "" }
        function fVersions(vs) {
            if (!vs || typeof vs !== "string" || vs.length === 0) return ""
            var parts = vs.split(",")
            if (parts.length === 0) return ""
            // Extract major.minor, find min/max
            var minVer = null, maxVer = null
            for (var i = 0; i < parts.length; i++) {
                var v = parts[i].trim()
                if (!v) continue
                var segs = v.split(".")
                if (segs.length < 2) continue
                var key = segs[0] + "." + segs[1]
                if (!minVer || key < minVer) minVer = key
                if (!maxVer || key > maxVer) maxVer = key
            }
            if (!minVer) return ""
            return minVer === maxVer ? minVer : minVer + "-" + maxVer
        }

        function doSearch(pageNum) {
            if (!backend) return
            pageNum = (pageNum !== undefined) ? pageNum : 0
            shaderSearching = true
            shaderCurrentPage = pageNum
            shaderOffset = pageNum * shaderPageSize
            shaderResultsModel.clear()
            var a = shaderCategory ? [shaderCategory] : []
            var b = shaderFeature ? [shaderFeature] : []
            var c = shaderPerformance ? [shaderPerformance] : []
            var d = shaderLoader ? [shaderLoader] : []
            var ver = page.shaderGameVersion ? [page.shaderGameVersion] : []
            backend.searchShadersEx(shaderFilterCard.searchText.trim(), ver, a.concat(b,c,d), [], [], shaderOffset, shaderPageSize)
        }
        function resetFilters() {
            shaderCategory = ""; shaderFeature = ""; shaderPerformance = ""; shaderLoader = ""
            page.shaderGameVersion = ""; shaderFilterCard.searchText = ""
            doSearch()
        }

        Connections {
            target: backend; enabled: backend !== null
            function onShaderSearchResultsReady(results) {
                if (shaderTab.shaderOffset === 0) shaderResultsModel.clear()
                shaderTab.shaderSearching = false
                if (results && results.length > 0) {
                    console.log("[shader] got " + results.length + " results, first.versions=" + JSON.stringify(results[0].versions) + " first.dateModified=" + results[0].dateModified)
                    var urlsToCache = []
                    for (var i = 0; i < results.length; i++) {
                        var r = results[i]
                        var iconUrl = (r.icon || "").replace("cdn.modrinth.com", "mod.mcimirror.top").replace("cdn-alt.modrinth.com", "mod.mcimirror.top")
                        if (iconUrl && backend) {
                            iconUrl = backend.resolveShaderIconUrl(iconUrl)
                            urlsToCache.push(iconUrl)
                        }
                        shaderResultsModel.append({
                            slug: r.slug || "", title: r.title || r.slug || "Unknown",
                            desc: r.desc || "", icon: iconUrl,
                            downloads: r.downloads || 0, versions: (r.versions && r.versions.length ? r.versions.join(",") : ""),
                            dateModified: r.dateModified || "", categories: (r.categories || []).join(",")
                        })
                    }
                    if (urlsToCache.length > 0 && backend) {
                        backend.cacheShaderIconBatchAsync(urlsToCache)
                    }
                }
                shaderTab.hasMoreShaders = (results && results.length >= shaderTab.shaderPageSize)
            }
        }

        ColumnLayout {
            anchors.fill: parent; anchors.margins: 12; spacing: 8

            // ── Filter Card ──
            FilterCard {
                id: shaderFilterCard
                Layout.fillWidth: true
                cardType: "shader"
                searchPlaceholder: qsTr("输入光影名称...（仅支持英文搜索）")
                rawVersionIds: backend ? backend.versionIds : []
                showPreReleases: page.shaderShowPreReleases
                shaderCategory: shaderTab.shaderCategory
                shaderFeature: shaderTab.shaderFeature
                shaderPerformance: shaderTab.shaderPerformance
                shaderLoader: shaderTab.shaderLoader

                Component.onCompleted: { mcVersion = page.shaderGameVersion }
                onPreReleaseToggled: { page.shaderShowPreReleases = showPreReleases }
                onMcVersionChanged: page.shaderGameVersion = mcVersion
                onShaderCategoryChanged: shaderTab.shaderCategory = shaderCategory
                onShaderFeatureChanged: shaderTab.shaderFeature = shaderFeature
                onShaderPerformanceChanged: shaderTab.shaderPerformance = shaderPerformance
                onShaderLoaderChanged: shaderTab.shaderLoader = shaderLoader
                onSearchClicked: shaderTab.doSearch()
                onResetClicked: {
                    shaderCategory = ""; shaderFeature = ""; shaderPerformance = ""; shaderLoader = ""
                    mcVersion = ""; searchText = ""
                    shaderTab.doSearch()
                }
            }

            // ── Card Grid ──
            ScrollView {
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                ScrollBar.vertical.policy: ScrollBar.AsNeeded
                Component.onCompleted: contentItem.flickDeceleration = 250

                ListView {
                    id: shaderCardView
                    anchors.fill: parent
                    model: shaderResultsModel
                    spacing: 6; cacheBuffer: 200
                    clip: true

                    header: LoadStatus {
                        width: shaderCardView.width
                        loading: shaderTab.shaderSearching
                        emptyText: qsTr("输入关键词搜索光影")
                        count: shaderResultsModel.count
                    }

                    footer: PaginationFooter {
                        currentPage: shaderTab.shaderCurrentPage
                        hasNext: shaderTab.hasMoreShaders
                        loading: shaderTab.shaderSearching
                        onFirstClicked: shaderTab.doSearch(0)
                        onPrevClicked: shaderTab.doSearch(shaderTab.shaderCurrentPage - 1)
                        onNextClicked: shaderTab.doSearch(shaderTab.shaderCurrentPage + 1)
                    }

                    delegate: DownloadCard {
                        width: shaderCardView.width
                        title: model.title || ""
                        description: model.desc || ""
                        iconUrl: model.icon || ""
                        slug: model.slug || ""
                        downloads: model.downloads || 0
                        source: "Modrinth"
                        gameVersions: model.versions || ""
                        dateModified: model.dateModified || ""
                        onClicked: {
                            page._shaderDetailSlug = model.slug
                            page._shaderDetailTitle = model.title || ""
                            page._shaderDetailDesc = model.desc || ""
                            page._shaderDetailIcon = model.icon || ""
                            page._showShaderDetail = true
                            console.info("[UI] 打开 光影详情 slug=" + model.slug)
                        }
                    }
                }
            }
        }
    }

    // TAB 3: 资源包
    // TAB 3: 资源包
    // ════════════════════════════════════════════
    Item {
        id: rpPage
        anchors.top: tabBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.topMargin: 8
        opacity: page.currentTab === 3 ? 1 : 0
        visible: page.currentTab === 3
        enabled: page.currentTab === 3
        Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

        function fmtDownloads(n) {
            if (n >= 100000000) return (n / 100000000).toFixed(1) + "亿"
            if (n >= 10000) return (n / 10000).toFixed(0) + "万"
            return (n || 0).toString()
        }
        function fmtVersionRange(vs) {
            if (!vs || typeof vs !== "string" || vs.length === 0) return ""
            var parts = vs.split(","); var minV = null, maxV = null
            for (var i = 0; i < parts.length; i++) {
                var v = parts[i].trim(); if (!v) continue
                var segs = v.split("."); if (segs.length < 2) continue
                var key = segs[0] + "." + segs[1]
                if (!minV || key < minV) minV = key
                if (!maxV || key > maxV) maxV = key
            }
            if (!minV) return ""
            return minV === maxV ? "适用: " + minV : "适用: " + minV + "-" + maxV
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8

            // ── Filter Card ──
            FilterCard {
                id: rpFilterCard
                Layout.fillWidth: true
                cardType: "resourcepack"
                searchPlaceholder: qsTr("输入资源包名称...（仅支持英文搜索）")
                rawVersionIds: backend ? backend.versionIds : []
                showPreReleases: page.rpShowPreReleases
                rpCategory: page.rpCategoryFilter
                rpFeature: page.rpFeatureFilter
                rpResolution: page.rpResolutionFilter
                Component.onCompleted: { mcVersion = page.rpGameVersion }
                onPreReleaseToggled: { page.rpShowPreReleases = showPreReleases }
                onMcVersionChanged: page.rpGameVersion = mcVersion
                onRpCategoryChanged: page.rpCategoryFilter = rpCategory
                onRpFeatureChanged: page.rpFeatureFilter = rpFeature
                onRpResolutionChanged: page.rpResolutionFilter = rpResolution
                onSearchClicked: searchRpPage(0)
                onResetClicked: {
                    rpCategory = ""; rpFeature = ""; rpResolution = ""
                    mcVersion = ""; searchText = ""
                    searchRpPage(0)
                }
            }


            // ── Results: vertical full-width cards ──
            ScrollView {
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                ScrollBar.vertical.policy: ScrollBar.AsNeeded
                Component.onCompleted: contentItem.flickDeceleration = 250
                

                ListView {
                    id: rpListView
                    anchors.fill: parent; spacing: 6
                    model: rpResultsModel
                    cacheBuffer: 200

                    header: LoadStatus {
                        width: rpListView.width
                        loading: page.rpSearching
                        emptyText: qsTr("搜索资源包")
                        count: rpResultsModel.count
                    }
                    footer: PaginationFooter {
                        currentPage: page.rpPage
                        hasNext: page.rpHasMore
                        loading: page.rpSearching
                        onFirstClicked: searchRpPage(0)
                        onPrevClicked: searchRpPage(page.rpPage - 1)
                        onNextClicked: searchRpPage(page.rpPage + 1)
                    }

                    delegate: DownloadCard {
                        width: rpListView.width - 8
                        title: model.title || ""
                        description: model.desc || ""
                        iconUrl: model.icon || ""
                        slug: model.slug || ""
                        downloads: model.downloads || 0
                        gameVersions: model.versionStr || ""
                        dateModified: model.updated || ""
                        categoriesJson: model.categories || "[]"
                        featuresJson: model.features || "[]"
                        resolutionsJson: model.resolutions || "[]"
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
                            console.info("[UI] 打开 资源包详情 slug=" + model.slug)
                            page._showRpDetail = true
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
            page.rpSearching = false

            rpResultsModel.clear()
            page.rpHasMore = ((page.rpPage + 1) * page.rpPageSize < totalHits)
            if (page.mainWindow && page.mainWindow.loadingBar) {
                page.mainWindow.loadingBar.opacity = 0
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
                var rpIconUrl = (r.icon || "").replace("cdn.modrinth.com", "mod.mcimirror.top").replace("cdn-alt.modrinth.com", "mod.mcimirror.top")
                if (rpIconUrl && backend) {
                    rpIconUrl = backend.resolveRpIconUrl(rpIconUrl)
                }
                rpResultsModel.append({
                    slug: r.slug || "",
                    title: r.title || "",
                    desc: r.desc || r.description || "",
                    icon: rpIconUrl,
                    downloads: r.downloads || 0,
                    categories: JSON.stringify(r.categories || []),
                    features: JSON.stringify(r.features || []),
                    resolutions: JSON.stringify(r.resolutions || []),
                    updated: r.updated || "",
                    author: r.author || "",
                    source: r.source || "Modrinth",
                    chips: "",
                    versionStr: ""
                })
            }
            console.log("[RP-DEBUG]", page.rpDebugSeq, "model now", rpResultsModel.count, "/", totalHits)
            if (backend && slugs.length > 0) {
                backend.fetchResourcepackVersions(slugs)
            }
        } catch(e) { console.log('[RP-DEBUG] searchCompleted ERROR:', e.message) }
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
                    if (vers.length > 6) chips.push({text: "+" + (vers.length - 6), color: StyleTokens.accentHover})
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
            if (versions.length > 6) chips.push({text: "+" + (versions.length - 6), color: StyleTokens.accentHover})
            var chipsJson = JSON.stringify(chips)

            var found = false
            for (var i = 0; i < rpResultsModel.count; i++) {
                if (rpResultsModel.get(i).slug === slug) {
                    console.log("[RP-DEBUG]", page.rpDebugSeq, "SET chips on index", i, "slug=", slug, "chipsJson len=", chipsJson.length)
                    rpResultsModel.setProperty(i, "chips", chipsJson)
                    rpResultsModel.setProperty(i, "versionStr", versions.length > 0 ? versions.join(",") : "")
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
                var speedStr = ""
                var spd = backend.resourceDownloadSpeed || 0
                if (spd > 0) {
                    var speedUnits = ["B/s", "KB/s", "MB/s"]
                    var su = 0
                    var sv = spd
                    while (sv >= 1024 && su < 2) { sv /= 1024; su++ }
                    speedStr = su === 0 ? sv.toFixed(0) + " " + speedUnits[su] : sv.toFixed(1) + " " + speedUnits[su]
                }
                if (toastManager) toastManager.show("下载中 " + page.installingRpName + ": " + pct + "%" + (speedStr ? " (" + speedStr + ")" : ""))
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
        color: hasBg ? Qt.rgba(0.047, 0.059, 0.086, 0.92) : StyleTokens.bgPrimary
        z: 10
        opacity: page._showRpDetail ? 1 : 0
        visible: page._showRpDetail
        Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }

        // Exit fade-out animation
        SequentialAnimation {
            id: rpExitAnim
            NumberAnimation { target: rpDetailOverlay; property: "opacity"; to: 0; duration: 300; easing.type: Easing.OutCubic }
            ScriptAction { script: { page._showRpDetail = false; rpDetailLoader._keepActive = false } }
        }

        Loader {
            id: rpDetailLoader
            anchors.fill: parent
            property bool _keepActive: false
            active: page._showRpDetail || _keepActive
            source: active ? "ResourcePackDetailPage.qml" : ""

            onLoaded: {
                _keepActive = true
                if (item) {
                    item.goBack.connect(function() { rpExitAnim.start() })
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
        color: hasBg ? Qt.rgba(0.047, 0.059, 0.086, 0.92) : StyleTokens.bgPrimary
        z: 10
        opacity: page._showModDetail ? 1 : 0
        visible: page._showModDetail
        Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }

        // Exit fade-out animation
        SequentialAnimation {
            id: modExitAnim
            NumberAnimation { target: modDetailOverlay; property: "opacity"; to: 0; duration: 300; easing.type: Easing.OutCubic }
            ScriptAction { script: { page._showModDetail = false; modDetailLoader._keepActive = false } }
        }

        Loader {
            id: modDetailLoader
            anchors.fill: parent
            property bool _keepActive: false
            active: page._showModDetail || _keepActive
            source: active ? "ModDetailPage.qml" : ""

            onLoaded: {
                _keepActive = true
                if (item) {
                    item.goBack.connect(function() { modExitAnim.start() })
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

    // ── Shader Detail (ShaderDetailPage.qml) ──
    property bool _showShaderDetail: false
    property string _shaderDetailSlug: ""
    property string _shaderDetailTitle: ""
    property string _shaderDetailDesc: ""
    property string _shaderDetailIcon: ""

    Rectangle {
        id: shaderDetailOverlay
        anchors.fill: parent
        color: hasBg ? Qt.rgba(0.047, 0.059, 0.086, 0.92) : StyleTokens.bgPrimary
        z: 10
        opacity: page._showShaderDetail ? 1 : 0
        visible: page._showShaderDetail
        Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }

        SequentialAnimation {
            id: shaderExitAnim
            NumberAnimation { target: shaderDetailOverlay; property: "opacity"; to: 0; duration: 300; easing.type: Easing.OutCubic }
            ScriptAction { script: { page._showShaderDetail = false; shaderDetailLoader._keepActive = false } }
        }

        Loader {
            id: shaderDetailLoader
            anchors.fill: parent
            property bool _keepActive: false
            active: page._showShaderDetail || _keepActive
            source: active ? "ShaderDetailPage.qml" : ""

            onLoaded: {
                _keepActive = true
                if (item) {
                    item.backend = backend
                    item.toastManager = toastManager
                    item.mainWindow = mainWindow
                    item.shaderDetailSlug = page._shaderDetailSlug
                    item.shaderDetailTitle = page._shaderDetailTitle
                    item.shaderDetailDesc = page._shaderDetailDesc
                    item.shaderDetailIcon = page._shaderDetailIcon
                    item.goBack.connect(function() {
                        shaderExitAnim.start()
                    })
                }
            }

            Connections {
                target: page
                function on_ShowShaderDetailChanged() {
                    if (page._showShaderDetail) {
                        shaderDetailOverlay.opacity = Qt.binding(function() { return page._showShaderDetail ? 1 : 0 })
                    } else {
                        shaderUnloadTimer.start()
                    }
                }
            }

            Timer {
                id: shaderUnloadTimer
                interval: 500
                onTriggered: { if (!page._showShaderDetail) shaderDetailLoader._keepActive = false }
            }
        }
    }

    // ════════════════════════════════════════════
    // TAB 4: Java 下载
    // ════════════════════════════════════════════
    JavaPage {
        id: javaPage
        anchors.top: tabBar.bottom
        anchors.topMargin: 8
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        opacity: page.currentTab === 4 ? 1 : 0
        enabled: page.currentTab === 4
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }

        javaBackend: backend ? backend.javaBackend() : null
    }

}
