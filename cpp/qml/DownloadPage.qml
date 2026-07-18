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
    color: hasBg ? "transparent" : "#0c0f16"

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
        if (rpVersionMenu.visible) { rpVersionMenu.close() } else { rpVersionMenu.open() }
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
                color: page.currentTab === index ? "#1a1f2e" : "transparent"
                border.color: page.currentTab === index ? "#3a4eb8" : "transparent"
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
                            Layout.alignment: Qt.AlignVCenter
                            source: "icons/lucide/" + modelData.icon + ".svg"
                            width: 14; height: 14
                        }
                        Text {
                            id: pillLabel
                            text: modelData.label
                            color: page.currentFilter === modelData.key ? "#e8ecf8" : "#9094a8"
                            font.pixelSize: StyleTokens.fontSizeMd
                            elide: Text.ElideRight
                        }
                        Text {
                            id: pillCount
                            text: "(" + modelData.countFn() + ")"
                            color: page.currentFilter === modelData.key ? "#93acf0" : "#505468"
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

            // ── 刷新按钮 ──
            Rectangle {
                width: 28; height: 28; radius: StyleTokens.radiusSm
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
                        color: "#3a4eb8"
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

            ListView {
                id: versionList
                anchors.fill: parent
                model: versionModel
                spacing: 2

                delegate: Rectangle {
                    id: versionRow
                    width: versionList.width
                    height: 42
                    color: "transparent"
                    radius: StyleTokens.radiusMd
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
                            font.pixelSize: StyleTokens.fontSizeMd
                            font.weight: Font.Medium
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Item { Layout.preferredWidth: 8 }

                        Rectangle {
                            radius: StyleTokens.radiusXs
                            height: 18
                            width: typeTag.implicitWidth + 12
                            color: model.vtype === "release" ? "#104830" :
                                   (model.vtype === "snapshot" ? "#403010" :
                                   (model.vtype === "april_fools" ? "#403020" : "#282828"))

                            Text {
                                id: typeTag
                                anchors.centerIn: parent
                                text: model.vtype === "release" ? "正式版" :
                                      (model.vtype === "snapshot" ? "快照" :
                                      (model.vtype === "april_fools" ? "愚人节" : "旧版"))
                                color: model.vtype === "release" ? "#4a8" :
                                       (model.vtype === "snapshot" ? "#b84" :
                                       (model.vtype === "april_fools" ? "#e9a" : "#999"))
                                font.pixelSize: StyleTokens.fontSizeXs
                                font.family: "Consolas, monospace"
                            }
                        }

                        // ── Status text (replaces progress bar) ──
                        Text {
                            visible: backend && backend.installing && backend.installVersionId === model.versionId
                                     && backend.installPhase !== "done"
                            text: qsTr("正在下载")
                            font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.accentLight
                        }

                        // Install button — hidden (moved to InstallPage)
                        Button {
                            id: installBtn
                            visible: false
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
                            text: (isInstallingThis || isDownloadActive) ? "下载中…" : (isDownloadQueued ? "排队中" : "安装")
                            implicitWidth: isDownloadQueued ? 56 : (isInstallingThis || isDownloadActive ? 56 : 48); implicitHeight: 24
                            font.pixelSize: StyleTokens.fontSizeXs; font.weight: Font.Medium
                            z: 10
                            flat: true
                            enabled: !isDownloadQueued && !isDownloadActive && page.clickedVersionId !== model.versionId
                            contentItem: Text {
                                text: installBtn.text
                                color: installBtn.isDownloadQueued ? "#e0a040" :
                                       (installBtn.isInstallingThis || installBtn.isDownloadActive ? "#6080e8" :
                                       (installBtn.hovered && installBtn.enabled ? "#ffffff" : "#707888"))
                                font.pixelSize: StyleTokens.fontSizeXs; font.weight: Font.Medium
                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                radius: StyleTokens.radiusSm
                                color: installBtn.isDownloadQueued ? "#1a1800" :
                                       (installBtn.isDownloadActive ? "#0a1020" :
                                       (installBtn.hovered && installBtn.enabled ? "#5068d8" : "transparent"))
                                border.color: installBtn.isDownloadQueued ? "#3a3000" :
                                              (installBtn.isDownloadActive ? "#1a2840" :
                                              (installBtn.hovered && installBtn.enabled ? "#5d6fe0" : "#1e2230"))
                                border.width: 1
                            }
                            onClicked: {
                                console.log("[DownloadPage] Install clicked for " + model.versionId)
                                if (backend) {
                                    // Log pre-install state
                                    console.log("[download-ui] pre-install: version=" + model.versionId + " installing=" + backend.installing + " versionId=" + backend.installVersionId + " phase=" + backend.installPhase)
                                    // Immediately mark this version as clicked so UI updates before page destruction
                                    page.clickedVersionId = model.versionId
                                    backend.installVersion(model.versionId)
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

        function doModSearch() {
            if (!backend) return
            page.modSearching = true
            modResultsModel.clear()
            var q = modInput.text ? modInput.text.trim() : ""
            console.log("[MOD-SEARCH] calling searchModsEx q=" + JSON.stringify(q) + " tab=" + page.currentTab)
            var gv = page.modGameVersion ? [page.modGameVersion] : []
            backend.searchModsEx(q, page.modLoader, page.modCategory, gv, page.modEnvironment, "", 0, 30)
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
            Rectangle {
                Layout.fillWidth: true; height: 130; radius: StyleTokens.radiusLg
                color: "#11141c"; border.color: StyleTokens.bgElevated

                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 12; spacing: 8

                    // Row 1: search + buttons
                    RowLayout {
                        Layout.fillWidth: true; spacing: 10

                        Text { text: qsTr("搜索"); color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: 32 }

                        Rectangle {
                            Layout.fillWidth: true; height: 28; radius: StyleTokens.radiusSm
                            color: modInput.activeFocus ? "#0f131c" : "#0c0e14"
                            border.color: modInput.activeFocus ? "#5068c8" : "#2a3040"
                            border.width: 1
                            Behavior on color { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }

                            TextInput {
                                id: modInput
                                anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8
                                color: "#d0d4e0"; verticalAlignment: TextInput.AlignVCenter; font.pixelSize: StyleTokens.fontSizeSm
                                Keys.onReturnPressed: modTab.doModSearch()

                                Text {
                                    anchors.fill: parent; verticalAlignment: Text.AlignVCenter
                                    text: qsTr("输入 Mod 名称..."); color: "#505468"; font.pixelSize: StyleTokens.fontSizeSm
                                    visible: !modInput.text
                                }
                            }
                        }

                        Rectangle {
                            width: 50; height: 28; radius: StyleTokens.radiusSm
                            color: modSearchBtn2.hovered ? "#5a78e0" : "#5068c8"
                            scale: modSearchBtn2.pressed ? 0.92 : (modSearchBtn2.hovered ? 1.06 : 1.0)
                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutBack } }
                            Text { anchors.centerIn: parent; text: qsTr("搜索"); color: "#fff"; font.pixelSize: StyleTokens.fontSizeXs; font.bold: true }
                            MouseArea {
                                id: modSearchBtn2; anchors.fill: parent
                                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: modTab.doModSearch()
                            }
                        }

                        Rectangle {
                            width: 50; height: 28; radius: StyleTokens.radiusSm
                            color: modResetBtn2.hovered ? "#2a2030" : "#1a1420"
                            border.color: "#3a2840"; border.width: 1
                            scale: modResetBtn2.pressed ? 0.92 : (modResetBtn2.hovered ? 1.06 : 1.0)
                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutBack } }
                            Text { anchors.centerIn: parent; text: qsTr("重置"); color: "#b090c8"; font.pixelSize: StyleTokens.fontSizeXs }
                            MouseArea {
                                id: modResetBtn2; anchors.fill: parent
                                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    page.modLoader = ""; page.modGameVersion = ""
                                    page.modCategory = ""; page.modEnvironment = ""
                                    modInput.text = ""; modResultsModel.clear()
                                    modTab.doModSearch()
                                }
                            }
                        }
                    }

                    // Row 2: loader + version + environment
                    RowLayout {
                        Layout.fillWidth: true; spacing: 10

                        Text { text: qsTr("加载器"); color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: 32 }

                        Rectangle {
                            id: modLdrPill2
                            Layout.preferredWidth: 120; height: 28; radius: StyleTokens.radiusSm
                            color: (modLdrHov2.containsMouse || modLdrMenu2.visible) ? "#1e3260" : "#0c0e14"
                            border.color: (modLdrHov2.containsMouse || modLdrMenu2.visible || page.modLoader) ? "#5078e0" : "#2a3040"; border.width: 1

                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on border.color { ColorAnimation { duration: 150 } }

                            RowLayout {
                                anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 4; spacing: 4
                                Text {
                                    Layout.fillWidth: true
                                    text: page.modLoaderLabels[page.modLoader] || "全部"
                                    color: page.modLoader ? "#8aaeff" : "#788090"; font.pixelSize: StyleTokens.fontSizeSm
                                    elide: Text.ElideRight
                                }
                                Text { text: "▾"; color: "#505468"; font.pixelSize: StyleTokens.fontSizeXs }
                            }
                            MouseArea {
                                id: modLdrHov2; anchors.fill: parent
                                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { if (modLdrMenu2.visible) modLdrMenu2.close(); else modLdrMenu2.open() }
                            }

                            Popup {
                                id: modLdrMenu2; closePolicy: Popup.NoAutoClose
                                y: parent.height + 4; width: 130
                                height: Math.min(modLdrFlick.contentHeight + 8, 220)
                                padding: 0
                                background: Rectangle { color: "#151922"; radius: StyleTokens.radiusLg; border.color: StyleTokens.bgElevated }

                                enter: Transition {
                                    ParallelAnimation {
                                        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180; easing.type: Easing.OutCubic }
                                        NumberAnimation { property: "y"; from: modLdrMenu2.y - 4; to: modLdrMenu2.y; duration: 180; easing.type: Easing.OutCubic }
                                    }
                                }
                                exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120 } }

                                Flickable {
                                    id: modLdrFlick
                                    anchors.fill: parent; anchors.margins: 4
                                    contentHeight: modLdrInner.implicitHeight; clip: true
                                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                    ColumnLayout {
                                        id: modLdrInner; width: parent.width; spacing: 2

                                        Rectangle {
                                            Layout.fillWidth: true; height: 26; radius: StyleTokens.radiusSm
                                            color: !page.modLoader ? "#1a2848" : "transparent"
                                            Text {
                                                anchors.centerIn: parent; text: qsTr("全部")
                                                color: !page.modLoader ? "#5068c8" : "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm
                                                font.weight: !page.modLoader ? Font.Bold : Font.Normal
                                            }
                                            MouseArea {
                                                anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                                onClicked: { page.modLoader = ""; modLdrMenu2.close() }
                                            }
                                        }

                                        Repeater {
                                            model: ["fabric", "forge", "quilt", "neoforge", "rift", "liteloader"]
                                            Rectangle {
                                                Layout.fillWidth: true; height: 26; radius: StyleTokens.radiusSm
                                                color: modelData === page.modLoader ? "#1a2848" : "transparent"
                                                Text {
                                                    anchors.centerIn: parent
                                                    text: page.modLoaderLabels[modelData] || modelData
                                                    color: modelData === page.modLoader ? "#5068c8" : "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm
                                                    font.weight: modelData === page.modLoader ? Font.Bold : Font.Normal
                                                }
                                                MouseArea {
                                                    anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                                    onClicked: { page.modLoader = modelData; modLdrMenu2.close() }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Text { text: qsTr("版本"); color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: 32 }

                        Rectangle {
                            id: modVerPill2
                            Layout.preferredWidth: 120; height: 28; radius: StyleTokens.radiusSm
                            color: (modVerHov2.containsMouse || modVerMenu2.visible) ? "#1e3260" : "#0c0e14"
                            border.color: (modVerHov2.containsMouse || modVerMenu2.visible || page.modGameVersion) ? "#5078e0" : "#2a3040"; border.width: 1

                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on border.color { ColorAnimation { duration: 150 } }

                            RowLayout {
                                anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 4; spacing: 4
                                Text {
                                    Layout.fillWidth: true
                                    text: page.modGameVersion ? ("MC " + page.modGameVersion) : "全部"
                                    color: page.modGameVersion ? "#8aaeff" : "#788090"; font.pixelSize: StyleTokens.fontSizeSm
                                    elide: Text.ElideRight
                                }
                                Text { text: "▾"; color: "#505468"; font.pixelSize: StyleTokens.fontSizeXs }
                            }
                            MouseArea {
                                id: modVerHov2; anchors.fill: parent
                                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { if (modVerMenu2.visible) modVerMenu2.close(); else modVerMenu2.open() }
                            }

                            Popup {
                                id: modVerMenu2; closePolicy: Popup.NoAutoClose
                                y: parent.height + 4; width: 140
                                height: Math.min(modVerFlick.contentHeight + 8, 220)
                                padding: 0
                                background: Rectangle { color: "#151922"; radius: StyleTokens.radiusLg; border.color: StyleTokens.bgElevated }

                                enter: Transition {
                                    ParallelAnimation {
                                        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180; easing.type: Easing.OutCubic }
                                        NumberAnimation { property: "y"; from: modVerMenu2.y - 4; to: modVerMenu2.y; duration: 180; easing.type: Easing.OutCubic }
                                    }
                                }
                                exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120 } }

                                Flickable {
                                    id: modVerFlick
                                    anchors.fill: parent; anchors.margins: 4
                                    contentHeight: modVerInner.implicitHeight; clip: true
                                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                    ColumnLayout {
                                        id: modVerInner; width: parent.width; spacing: 2

                                        Rectangle {
                                            Layout.fillWidth: true; height: 26; radius: StyleTokens.radiusSm
                                            color: !page.modGameVersion ? "#1a2848" : "transparent"
                                            Text {
                                                anchors.centerIn: parent; text: qsTr("全部")
                                                color: !page.modGameVersion ? "#5068c8" : "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm
                                                font.weight: !page.modGameVersion ? Font.Bold : Font.Normal
                                            }
                                            MouseArea {
                                                anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                                onClicked: { page.modGameVersion = ""; modVerMenu2.close() }
                                            }
                                        }

                                        Repeater {
                                            model: {
                                                if (!backend || !backend.versionIds) return ["1.21.10","1.20.6"]
                                                var seen = new Set()
                                                var groups = []
                                                for (var i = 0; i < backend.versionIds.length; i++) {
                                                    var v = backend.versionIds[i]
                                                    if (!page.modShowPreReleases && !/^[0-9.]+$/.test(v)) continue
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
                                                Layout.fillWidth: true; height: 26; radius: StyleTokens.radiusSm
                                                color: modelData === page.modGameVersion ? "#1a2848" : "transparent"
                                                Text {
                                                    anchors.centerIn: parent; text: "MC " + modelData
                                                    color: modelData === page.modGameVersion ? "#5068c8" : "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm
                                                    font.weight: modelData === page.modGameVersion ? Font.Bold : Font.Normal
                                                }
                                                MouseArea {
                                                    anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                                    onClicked: { page.modGameVersion = modelData; modVerMenu2.close() }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Text { text: qsTr("类别"); color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: 32 }

                        Rectangle {
                            id: modCatPill2
                            Layout.preferredWidth: 115; height: 28; radius: StyleTokens.radiusSm
                            color: (modCatHov2.containsMouse || modCatMenu2.visible) ? "#1e3260" : "#0c0e14"
                            border.color: (modCatHov2.containsMouse || modCatMenu2.visible || page.modCategory) ? "#5078e0" : "#2a3040"; border.width: 1

                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on border.color { ColorAnimation { duration: 150 } }

                            RowLayout {
                                anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 4; spacing: 4
                                Text {
                                    Layout.fillWidth: true
                                    text: page.modCatLabels[page.modCategory] || "全部"
                                    color: page.modCategory ? "#8aaeff" : "#788090"; font.pixelSize: StyleTokens.fontSizeSm
                                    elide: Text.ElideRight
                                }
                                Text { text: "▾"; color: "#505468"; font.pixelSize: StyleTokens.fontSizeXs }
                            }
                            MouseArea {
                                id: modCatHov2; anchors.fill: parent
                                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { if (modCatMenu2.visible) modCatMenu2.close(); else modCatMenu2.open() }
                            }

                            Popup {
                                id: modCatMenu2; closePolicy: Popup.NoAutoClose
                                y: parent.height + 4; width: 140
                                height: Math.min(modCatFlick.contentHeight + 8, 300)
                                padding: 0
                                background: Rectangle { color: "#151922"; radius: StyleTokens.radiusLg; border.color: StyleTokens.bgElevated }

                                enter: Transition {
                                    ParallelAnimation {
                                        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180; easing.type: Easing.OutCubic }
                                        NumberAnimation { property: "y"; from: modCatMenu2.y - 4; to: modCatMenu2.y; duration: 180; easing.type: Easing.OutCubic }
                                    }
                                }
                                exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120 } }

                                Flickable {
                                    id: modCatFlick
                                    anchors.fill: parent; anchors.margins: 4
                                    contentHeight: modCatInner.implicitHeight; clip: true
                                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                    ColumnLayout {
                                        id: modCatInner; width: parent.width; spacing: 2

                                        Rectangle {
                                            Layout.fillWidth: true; height: 26; radius: StyleTokens.radiusSm
                                            color: !page.modCategory ? "#1a2848" : "transparent"
                                            Text {
                                                anchors.centerIn: parent; text: qsTr("全部")
                                                color: !page.modCategory ? "#5068c8" : "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm
                                                font.weight: !page.modCategory ? Font.Bold : Font.Normal
                                            }
                                            MouseArea {
                                                anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                                onClicked: { page.modCategory = ""; modCatMenu2.close() }
                                            }
                                        }

                                        Repeater {
                                            model: Object.keys(page.modCatLabels)
                                            Rectangle {
                                                Layout.fillWidth: true; height: 26; radius: StyleTokens.radiusSm
                                                color: modelData === page.modCategory ? "#1a2848" : "transparent"
                                                Text {
                                                    anchors.centerIn: parent
                                                    text: page.modCatLabels[modelData] || modelData
                                                    color: modelData === page.modCategory ? "#5068c8" : "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm
                                                    font.weight: modelData === page.modCategory ? Font.Bold : Font.Normal
                                                }
                                                MouseArea {
                                                    anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                                    onClicked: { page.modCategory = modelData; modCatMenu2.close() }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Text { text: qsTr("环境"); color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: 32 }

                        Rectangle {
                            id: modEnvPill2
                            Layout.preferredWidth: 95; height: 28; radius: StyleTokens.radiusSm
                            color: (modEnvHov2.containsMouse || modEnvMenu2.visible) ? "#1e3260" : "#0c0e14"
                            border.color: (modEnvHov2.containsMouse || modEnvMenu2.visible || page.modEnvironment) ? "#5078e0" : "#2a3040"; border.width: 1

                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on border.color { ColorAnimation { duration: 150 } }

                            RowLayout {
                                anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 4; spacing: 4
                                Text {
                                    Layout.fillWidth: true
                                    text: page.modEnvironment ? page.modEnvLabels[page.modEnvironment] || page.modEnvironment : "全部"
                                    color: page.modEnvironment ? "#8aaeff" : "#788090"; font.pixelSize: StyleTokens.fontSizeSm
                                    elide: Text.ElideRight
                                }
                                Text { text: "▾"; color: "#505468"; font.pixelSize: StyleTokens.fontSizeXs }
                            }
                            MouseArea {
                                id: modEnvHov2; anchors.fill: parent
                                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { if (modEnvMenu2.visible) modEnvMenu2.close(); else modEnvMenu2.open() }
                            }

                            Popup {
                                id: modEnvMenu2; closePolicy: Popup.NoAutoClose
                                y: parent.height + 4; width: 120
                                height: Math.min(modEnvFlick.contentHeight + 8, 160)
                                padding: 0
                                background: Rectangle { color: "#151922"; radius: StyleTokens.radiusLg; border.color: StyleTokens.bgElevated }

                                enter: Transition {
                                    ParallelAnimation {
                                        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180; easing.type: Easing.OutCubic }
                                        NumberAnimation { property: "y"; from: modEnvMenu2.y - 4; to: modEnvMenu2.y; duration: 180; easing.type: Easing.OutCubic }
                                    }
                                }
                                exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120 } }

                                Flickable {
                                    id: modEnvFlick
                                    anchors.fill: parent; anchors.margins: 4
                                    contentHeight: modEnvInner.implicitHeight; clip: true
                                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                    ColumnLayout {
                                        id: modEnvInner; width: parent.width; spacing: 2

                                        Rectangle {
                                            Layout.fillWidth: true; height: 26; radius: StyleTokens.radiusSm
                                            color: !page.modEnvironment ? "#1a2848" : "transparent"
                                            Text {
                                                anchors.centerIn: parent; text: qsTr("全部")
                                                color: !page.modEnvironment ? "#5068c8" : "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm
                                                font.weight: !page.modEnvironment ? Font.Bold : Font.Normal
                                            }
                                            MouseArea {
                                                anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                                onClicked: { page.modEnvironment = ""; modEnvMenu2.close() }
                                            }
                                        }

                                        Repeater {
                                            model: ["client", "server"]
                                            Rectangle {
                                                Layout.fillWidth: true; height: 26; radius: StyleTokens.radiusSm
                                                color: modelData === page.modEnvironment ? "#1a2848" : "transparent"
                                                Text {
                                                    anchors.centerIn: parent
                                                    text: page.modEnvLabels[modelData] || modelData
                                                    color: modelData === page.modEnvironment ? "#5068c8" : "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm
                                                    font.weight: modelData === page.modEnvironment ? Font.Bold : Font.Normal
                                                }
                                                MouseArea {
                                                    anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                                    onClicked: { page.modEnvironment = modelData; modEnvMenu2.close() }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Row 3: pre-release toggle
                    RowLayout {
                        Layout.fillWidth: true
                        Rectangle {
                            width: modPreTogText.implicitWidth + 16; height: 24; radius: StyleTokens.radiusSm
                            color: modPreTogHov.containsMouse ? (page.modShowPreReleases ? "#282040" : "#1a1e28") : (page.modShowPreReleases ? "#1e1838" : "#12151c")
                            border.color: page.modShowPreReleases ? "#504080" : "#2a3040"; border.width: 1
                            Behavior on color { ColorAnimation { duration: 150 } }
                            Text {
                                id: modPreTogText
                                anchors.centerIn: parent
                                text: page.modShowPreReleases ? qsTr("隐藏测试版") : qsTr("显示测试版")
                                color: page.modShowPreReleases ? "#9088e0" : "#687080"; font.pixelSize: StyleTokens.fontSizeXs
                            }
                            MouseArea {
                                id: modPreTogHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { page.modShowPreReleases = !page.modShowPreReleases; modTab.doModSearch() }
                            }
                        }
                        Item { Layout.fillWidth: true }
                    }
                }
            }

            // Results
            ScrollView {
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                ScrollBar.vertical.policy: ScrollBar.AsNeeded

                ListView {
                    id: modListView2
                    anchors.fill: parent; spacing: 6
                    model: modResultsModel
                    cacheBuffer: 200

                    header: Item {
                        width: modListView2.width
                        height: modResultsModel.count > 0 ? 0 : 200
                        visible: modResultsModel.count === 0
                        Text {
                            anchors.centerIn: parent
                            text: page.modSearching ? qsTr("搜索中…") : qsTr("输入关键词搜索 Mod")
                            color: "#606478"; font.pixelSize: StyleTokens.fontSizeSm
                        }
                    }

                    delegate: Rectangle {
                        id: modCard2
                        width: modListView2.width - 8; height: 64; radius: StyleTokens.radiusLg
                        color: modCardHov2.hovered ? "#121620" : "#0e1018"
                        border.color: modCardHov2.hovered ? "#5068c8" : "#1a1f2a"; border.width: 1
                        scale: modCardHov2.hovered ? 1.01 : 1.0

                        opacity: 0
                        Component.onCompleted: { opacity = 1 }

                        Behavior on color { ColorAnimation { duration: 150 } }
                        Behavior on border.color { ColorAnimation { duration: 150 } }
                        Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutBack } }
                        Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }

                        Rectangle {
                            anchors.fill: parent; radius: parent.radius
                            opacity: modCardHov2.hovered ? 0.06 : 0
                            gradient: Gradient {
                                GradientStop { position: 0; color: StyleTokens.accentLight }
                                GradientStop { position: 1; color: StyleTokens.accentLight }
                            }
                            Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
                        }

                        RowLayout {
                            anchors.fill: parent; anchors.margins: 8; spacing: 8

                            Rectangle {
                                Layout.preferredWidth: 36; Layout.preferredHeight: 36; radius: StyleTokens.radiusMd
                                color: "#1a1f2e"; clip: true

                                Image {
                                    anchors.fill: parent; anchors.margins: 2
                                    fillMode: Image.PreserveAspectFit
                                    asynchronous: true; cache: true
                                    source: model.icon || ""
                                    onStatusChanged: {
                                        if (status === Image.Error) modIconFallback2.visible = true
                                        else if (status === Image.Ready) modIconFallback2.visible = false
                                    }
                                }
                                Text {
                                    id: modIconFallback2
                                    anchors.centerIn: parent
                                    text: model ? (model.title ? model.title[0] : "M") : "M"
                                    color: "#5068c8"; font.pixelSize: StyleTokens.fontSizeLg; font.bold: true
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
                                        font.pixelSize: StyleTokens.fontSizeSm; font.bold: true; elide: Text.ElideRight
                                    }
                                    Rectangle {
                                        Layout.preferredWidth: 56; height: 15; radius: StyleTokens.radiusXs; color: StyleTokens.bgElevated
                                        Text { anchors.centerIn: parent; text: "Modrinth"; color: "#9088e0"; font.pixelSize: StyleTokens.fontSizeXs }
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: model.desc || ""; color: "#606478"
                                    font.pixelSize: StyleTokens.fontSizeXs; elide: Text.ElideRight; maximumLineCount: 1
                                }

                                RowLayout {
                                    Layout.fillWidth: true; spacing: 12
                                    Text { text: modTab.fmtVersionRange(model.versions); color: "#687080"; font.pixelSize: StyleTokens.fontSizeXs }
                                    RowLayout {
                                        spacing: 3
                                        Image { source: "icons/lucide/download.svg"; width: 8; height: 8; sourceSize.width: 8; sourceSize.height: 8; Layout.alignment: Qt.AlignVCenter }
                                        Text { text: modTab.formatDownloads(model.downloads || 0); color: "#788090"; font.pixelSize: StyleTokens.fontSizeXs }
                                    }
                                    Text { text: modTab.fmtDate(model.dateModified); color: "#687080"; font.pixelSize: StyleTokens.fontSizeXs }
                                    Text { text: modTab.fmtLoaders(model.loadersList || model.loader || ""); color: "#687080"; font.pixelSize: StyleTokens.fontSizeXs; Layout.fillWidth: true; elide: Text.ElideRight }
                                }
                            }
                        }

                        MouseArea {
                            id: modCardHov2
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
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

        function doSearch() {
            if (!backend) return
            shaderSearching = true; shaderOffset = 0; shaderResultsModel.clear()
            var a = shaderCategory ? [shaderCategory] : []
            var b = shaderFeature ? [shaderFeature] : []
            var c = shaderPerformance ? [shaderPerformance] : []
            var d = shaderLoader ? [shaderLoader] : []
            var ver = page.shaderGameVersion ? [page.shaderGameVersion] : []
            backend.searchShadersEx(shaderInput.text.trim(), ver, a.concat(b,c,d), [], [], 0, 50)
        }
        function loadMore() {
            if (!backend || shaderSearching || !hasMoreShaders) return
            shaderSearching = true; shaderOffset += 50
            var a = shaderCategory ? [shaderCategory] : []
            var b = shaderFeature ? [shaderFeature] : []
            var c = shaderPerformance ? [shaderPerformance] : []
            var d = shaderLoader ? [shaderLoader] : []
            var ver = page.shaderGameVersion ? [page.shaderGameVersion] : []
            backend.searchShadersEx(shaderInput.text.trim(), ver, a.concat(b,c,d), [], [], shaderOffset, 50)
        }
        function resetFilters() {
            shaderCategory = ""; shaderFeature = ""; shaderPerformance = ""; shaderLoader = ""
            page.shaderGameVersion = ""; shaderInput.text = ""
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
                shaderTab.hasMoreShaders = (results && results.length >= 50)
            }
        }

        ColumnLayout {
            anchors.fill: parent; anchors.margins: 12; spacing: 8

            // ── Filter Card ──
            Rectangle {
                Layout.fillWidth: true; height: 130; radius: StyleTokens.radiusLg
                color: "#11141c"; border.color: StyleTokens.bgElevated
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 12; spacing: 8

                    // Row 1: search + buttons
                    RowLayout {
                        Layout.fillWidth: true; spacing: 10
                        Text { text: qsTr("搜索"); color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: 32 }
                        Rectangle {
                            Layout.fillWidth: true; height: 28; radius: StyleTokens.radiusSm
                            color: shaderInput.activeFocus ? "#0f131c" : "#0c0e14"
                            border.color: shaderInput.activeFocus ? "#5068c8" : "#2a3040"; border.width: 1
                            Behavior on color { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                            TextInput {
                                id: shaderInput
                                anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8
                                color: "#d0d4e0"; verticalAlignment: TextInput.AlignVCenter; font.pixelSize: StyleTokens.fontSizeSm
                                Keys.onReturnPressed: shaderTab.doSearch()
                                Text {
                                    anchors.fill: parent; verticalAlignment: Text.AlignVCenter
                                    text: qsTr("输入光影名称..."); color: "#505468"; font.pixelSize: StyleTokens.fontSizeSm
                                    visible: !shaderInput.text
                                }
                            }
                        }
                        Rectangle {
                            width: 50; height: 28; radius: StyleTokens.radiusSm
                            color: sBtnHov.containsMouse ? "#5a78e0" : "#5068c8"
                            scale: sBtnHov.pressed ? 0.92 : (sBtnHov.containsMouse ? 1.06 : 1.0)
                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutBack } }
                            Text { anchors.centerIn: parent; text: qsTr("搜索"); color: "#fff"; font.pixelSize: StyleTokens.fontSizeXs; font.bold: true }
                            MouseArea {
                                id: sBtnHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: shaderTab.doSearch()
                            }
                        }
                        Rectangle {
                            width: 50; height: 28; radius: StyleTokens.radiusSm
                            color: rBtnHov.containsMouse ? "#2a2030" : "#1a1420"
                            border.color: "#3a2840"; border.width: 1
                            scale: rBtnHov.pressed ? 0.92 : (rBtnHov.containsMouse ? 1.06 : 1.0)
                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutBack } }
                            Text { anchors.centerIn: parent; text: qsTr("重置"); color: "#b090c8"; font.pixelSize: StyleTokens.fontSizeXs }
                            MouseArea {
                                id: rBtnHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: shaderTab.resetFilters()
                            }
                        }
                    }

                    // Row 2: 风格 + 特性 + 性能 + 加载器 (all in one row)
                    RowLayout {
                        Layout.fillWidth: true; spacing: 8

                        Text { text: qsTr("风格"); color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: 28 }
                        Rectangle {
                            id: sCatPill; Layout.preferredWidth: 90; height: 28; radius: StyleTokens.radiusSm
                            color: (sCatHov.containsMouse || sCatMenu.visible) ? "#1e3260" : "#0c0e14"
                            border.color: (sCatHov.containsMouse || sCatMenu.visible || shaderTab.shaderCategory) ? "#5078e0" : "#2a3040"; border.width: 1
                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on border.color { ColorAnimation { duration: 150 } }
                            RowLayout {
                                anchors.fill: parent; anchors.leftMargin: 6; anchors.rightMargin: 4; spacing: 2
                                Text {
                                    Layout.fillWidth: true
                                    text: shaderTab.ddLabel(shaderTab.shaderCats, shaderTab.shaderCategory)
                                    color: shaderTab.shaderCategory ? "#8aaeff" : "#788090"; font.pixelSize: StyleTokens.fontSizeSm
                                    elide: Text.ElideRight
                                }
                                Text { text: "\u25BE"; color: "#505468"; font.pixelSize: StyleTokens.fontSizeXs }
                            }
                            MouseArea {
                                id: sCatHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { if (sCatMenu.visible) sCatMenu.close(); else sCatMenu.open() }
                            }
                            Popup {
                                id: sCatMenu; closePolicy: Popup.NoAutoClose
                                y: parent.height + 4; width: 130; padding: 4
                                background: Rectangle { color: "#0f1118"; border.color: "#2a3040"; radius: StyleTokens.radiusMd }
                                
                                enter: Transition {
                                    ParallelAnimation {
                                        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180; easing.type: Easing.OutCubic }
                                        NumberAnimation { property: "y"; from: sCatMenu.y - 4; to: sCatMenu.y; duration: 180; easing.type: Easing.OutCubic }
                                    }
                                }
                                exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120 } }
                                contentItem: ColumnLayout {
                                    spacing: 2
                                    Repeater {
                                        model: shaderTab.shaderCats
                                        Rectangle {
                                            Layout.fillWidth: true; height: 26; radius: StyleTokens.radiusSm
                                            color: catItemHov.containsMouse ? "#1a2a50" : "transparent"
                                            Text {
                                                Layout.alignment: Qt.AlignVCenter; anchors.left: parent.left; anchors.leftMargin: 10
                                                text: modelData.label
                                                color: shaderTab.shaderCategory === modelData.slug ? "#8ab4f8" : "#8890a0"
                                                font.pixelSize: StyleTokens.fontSizeSm
                                            }
                                            MouseArea {
                                                id: catItemHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                                onClicked: { shaderTab.shaderCategory = modelData.slug; sCatMenu.close() }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Text { text: qsTr("特性"); color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: 28 }
                        Rectangle {
                            id: sFeatPill; Layout.preferredWidth: 110; height: 28; radius: StyleTokens.radiusSm
                            color: (sFeatHov.containsMouse || sFeatMenu.visible) ? "#1e3260" : "#0c0e14"
                            border.color: (sFeatHov.containsMouse || sFeatMenu.visible || shaderTab.shaderFeature) ? "#5078e0" : "#2a3040"; border.width: 1
                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on border.color { ColorAnimation { duration: 150 } }
                            RowLayout {
                                anchors.fill: parent; anchors.leftMargin: 6; anchors.rightMargin: 4; spacing: 2
                                Text {
                                    Layout.fillWidth: true
                                    text: shaderTab.ddLabel(shaderTab.shaderFeatures, shaderTab.shaderFeature)
                                    color: shaderTab.shaderFeature ? "#8aaeff" : "#788090"; font.pixelSize: StyleTokens.fontSizeSm
                                    elide: Text.ElideRight
                                }
                                Text { text: "\u25BE"; color: "#505468"; font.pixelSize: StyleTokens.fontSizeXs }
                            }
                            MouseArea {
                                id: sFeatHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { if (sFeatMenu.visible) sFeatMenu.close(); else sFeatMenu.open() }
                            }
                            Popup {
                                id: sFeatMenu; closePolicy: Popup.NoAutoClose
                                y: parent.height + 4; width: 140; padding: 4
                                background: Rectangle { color: "#0f1118"; border.color: "#2a3040"; radius: StyleTokens.radiusMd }
                                
                                enter: Transition {
                                    ParallelAnimation {
                                        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180; easing.type: Easing.OutCubic }
                                        NumberAnimation { property: "y"; from: sFeatMenu.y - 4; to: sFeatMenu.y; duration: 180; easing.type: Easing.OutCubic }
                                    }
                                }
                                exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120 } }
                                contentItem: ColumnLayout {
                                    spacing: 2
                                    Repeater {
                                        model: shaderTab.shaderFeatures
                                        Rectangle {
                                            Layout.fillWidth: true; height: 26; radius: StyleTokens.radiusSm
                                            color: featItemHov.containsMouse ? "#1a2a50" : "transparent"
                                            Text {
                                                Layout.alignment: Qt.AlignVCenter; anchors.left: parent.left; anchors.leftMargin: 10
                                                text: modelData.label
                                                color: shaderTab.shaderFeature === modelData.slug ? "#8ab4f8" : "#8890a0"
                                                font.pixelSize: StyleTokens.fontSizeSm
                                            }
                                            MouseArea {
                                                id: featItemHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                                onClicked: { shaderTab.shaderFeature = modelData.slug; sFeatMenu.close() }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Text { text: qsTr("性能"); color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: 28 }
                        Rectangle {
                            id: sPerfPill; Layout.preferredWidth: 80; height: 28; radius: StyleTokens.radiusSm
                            color: (sPerfHov.containsMouse || sPerfMenu.visible) ? "#1e3260" : "#0c0e14"
                            border.color: (sPerfHov.containsMouse || sPerfMenu.visible || shaderTab.shaderPerformance) ? "#5078e0" : "#2a3040"; border.width: 1
                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on border.color { ColorAnimation { duration: 150 } }
                            RowLayout {
                                anchors.fill: parent; anchors.leftMargin: 6; anchors.rightMargin: 4; spacing: 2
                                Text {
                                    Layout.fillWidth: true
                                    text: shaderTab.ddLabel(shaderTab.shaderPerfs, shaderTab.shaderPerformance)
                                    color: shaderTab.shaderPerformance ? "#8aaeff" : "#788090"; font.pixelSize: StyleTokens.fontSizeSm
                                    elide: Text.ElideRight
                                }
                                Text { text: "\u25BE"; color: "#505468"; font.pixelSize: StyleTokens.fontSizeXs }
                            }
                            MouseArea {
                                id: sPerfHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { if (sPerfMenu.visible) sPerfMenu.close(); else sPerfMenu.open() }
                            }
                            Popup {
                                id: sPerfMenu; closePolicy: Popup.NoAutoClose
                                y: parent.height + 4; width: 110; padding: 4
                                background: Rectangle { color: "#0f1118"; border.color: "#2a3040"; radius: StyleTokens.radiusMd }
                                
                                enter: Transition {
                                    ParallelAnimation {
                                        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180; easing.type: Easing.OutCubic }
                                        NumberAnimation { property: "y"; from: sPerfMenu.y - 4; to: sPerfMenu.y; duration: 180; easing.type: Easing.OutCubic }
                                    }
                                }
                                exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120 } }
                                contentItem: ColumnLayout {
                                    spacing: 2
                                    Repeater {
                                        model: shaderTab.shaderPerfs
                                        Rectangle {
                                            Layout.fillWidth: true; height: 26; radius: StyleTokens.radiusSm
                                            color: perfItemHov.containsMouse ? "#1a2a50" : "transparent"
                                            Text {
                                                Layout.alignment: Qt.AlignVCenter; anchors.left: parent.left; anchors.leftMargin: 10
                                                text: modelData.label
                                                color: shaderTab.shaderPerformance === modelData.slug ? "#8ab4f8" : "#8890a0"
                                                font.pixelSize: StyleTokens.fontSizeSm
                                            }
                                            MouseArea {
                                                id: perfItemHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                                onClicked: { shaderTab.shaderPerformance = modelData.slug; sPerfMenu.close() }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Text { text: qsTr("加载器"); color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: 32 }
                        Rectangle {
                            id: sLdrPill; Layout.preferredWidth: 90; height: 28; radius: StyleTokens.radiusSm
                            color: (sLdrHov.containsMouse || sLdrMenu.visible) ? "#1e3260" : "#0c0e14"
                            border.color: (sLdrHov.containsMouse || sLdrMenu.visible || shaderTab.shaderLoader) ? "#5078e0" : "#2a3040"; border.width: 1
                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on border.color { ColorAnimation { duration: 150 } }
                            RowLayout {
                                anchors.fill: parent; anchors.leftMargin: 6; anchors.rightMargin: 4; spacing: 2
                                Text {
                                    Layout.fillWidth: true
                                    text: shaderTab.ddLabel(shaderTab.shaderLoaders, shaderTab.shaderLoader)
                                    color: shaderTab.shaderLoader ? "#8aaeff" : "#788090"; font.pixelSize: StyleTokens.fontSizeSm
                                    elide: Text.ElideRight
                                }
                                Text { text: "\u25BE"; color: "#505468"; font.pixelSize: StyleTokens.fontSizeXs }
                            }
                            MouseArea {
                                id: sLdrHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { if (sLdrMenu.visible) sLdrMenu.close(); else sLdrMenu.open() }
                            }
                            Popup {
                                id: sLdrMenu; closePolicy: Popup.NoAutoClose
                                y: parent.height + 4; width: 120; padding: 4
                                background: Rectangle { color: "#0f1118"; border.color: "#2a3040"; radius: StyleTokens.radiusMd }
                                
                                enter: Transition {
                                    ParallelAnimation {
                                        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180; easing.type: Easing.OutCubic }
                                        NumberAnimation { property: "y"; from: sLdrMenu.y - 4; to: sLdrMenu.y; duration: 180; easing.type: Easing.OutCubic }
                                    }
                                }
                                exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120 } }
                                contentItem: ColumnLayout {
                                    spacing: 2
                                    Repeater {
                                        model: shaderTab.shaderLoaders
                                        Rectangle {
                                            Layout.fillWidth: true; height: 26; radius: StyleTokens.radiusSm
                                            color: ldrItemHov.containsMouse ? "#1a2a50" : "transparent"
                                            Text {
                                                Layout.alignment: Qt.AlignVCenter; anchors.left: parent.left; anchors.leftMargin: 10
                                                text: modelData.label
                                                color: shaderTab.shaderLoader === modelData.slug ? "#8ab4f8" : "#8890a0"
                                                font.pixelSize: StyleTokens.fontSizeSm
                                            }
                                            MouseArea {
                                                id: ldrItemHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                                onClicked: { shaderTab.shaderLoader = modelData.slug; sLdrMenu.close() }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Row 3: 版本 + toggle
                    RowLayout {
                        Layout.fillWidth: true; spacing: 8

                        Text { text: qsTr("版本"); color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: 28 }

                        Rectangle {
                            id: sVerPill; Layout.preferredWidth: 100; height: 28; radius: StyleTokens.radiusSm
                            color: (sVerHov.containsMouse || sVerMenu.visible) ? "#1e3260" : "#0c0e14"
                            border.color: (sVerHov.containsMouse || sVerMenu.visible || page.shaderGameVersion) ? "#5078e0" : "#2a3040"; border.width: 1
                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on border.color { ColorAnimation { duration: 150 } }
                            RowLayout {
                                anchors.fill: parent; anchors.leftMargin: 6; anchors.rightMargin: 4; spacing: 2
                                Text {
                                    Layout.fillWidth: true
                                    text: page.shaderGameVersion ? ("MC " + page.shaderGameVersion) : "全部"
                                    color: page.shaderGameVersion ? "#8aaeff" : "#788090"; font.pixelSize: StyleTokens.fontSizeSm
                                    elide: Text.ElideRight
                                }
                                Text { text: "\u25BE"; color: "#505468"; font.pixelSize: StyleTokens.fontSizeXs }
                            }
                            MouseArea {
                                id: sVerHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { if (sVerMenu.visible) sVerMenu.close(); else sVerMenu.open() }
                            }
                            Popup {
                                id: sVerMenu; closePolicy: Popup.NoAutoClose
                                y: parent.height + 4; width: 130
                                height: Math.min(sVerFlick.contentHeight + 8, 220)
                                padding: 0
                                background: Rectangle { color: "#151922"; radius: StyleTokens.radiusLg; border.color: StyleTokens.bgElevated }
                                
                                enter: Transition {
                                    ParallelAnimation {
                                        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180; easing.type: Easing.OutCubic }
                                        NumberAnimation { property: "y"; from: sVerMenu.y - 4; to: sVerMenu.y; duration: 180; easing.type: Easing.OutCubic }
                                    }
                                }
                                exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120 } }
                                
                                Flickable {
                                    id: sVerFlick
                                    anchors.fill: parent; anchors.margins: 4
                                    contentHeight: sVerInner.implicitHeight; clip: true
                                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                                    
                                    ColumnLayout {
                                        id: sVerInner; width: parent.width; spacing: 2
                                        
                                        Rectangle {
                                            Layout.fillWidth: true; height: 26; radius: StyleTokens.radiusSm
                                            color: !page.shaderGameVersion ? "#1a2848" : "transparent"
                                            Text {
                                                anchors.centerIn: parent
                                                text: qsTr("全部")
                                                color: !page.shaderGameVersion ? "#5068c8" : "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm
                                                font.weight: !page.shaderGameVersion ? Font.Bold : Font.Normal
                                            }
                                            MouseArea {
                                                anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                                onClicked: { page.shaderGameVersion = ""; sVerMenu.close() }
                                            }
                                        }
                                        Repeater {
                                            model: {
                                                if (!backend || !backend.versionIds) return ["1.21.10","1.20.6"]
                                                var seen = new Set(); var groups = []
                                                for (var i = 0; i < backend.versionIds.length; i++) {
                                                    var v = backend.versionIds[i]
                                                    if (!page.shaderShowPreReleases && !/^[0-9.]+$/.test(v)) continue
                                                    var major = v.split(/[.\-]/).slice(0,2).join(".")
                                                    if (!seen.has(major)) { seen.add(major); groups.push(major) }
                                                    if (groups.length >= 30) break
                                                }
                                                return groups
                                            }
                                            Rectangle {
                                                Layout.fillWidth: true; height: 26; radius: StyleTokens.radiusSm
                                                color: modelData === page.shaderGameVersion ? "#1a2848" : "transparent"
                                                Text {
                                                    anchors.centerIn: parent
                                                    text: "MC " + modelData
                                                    color: modelData === page.shaderGameVersion ? "#5068c8" : "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm
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

                        // Show/hide pre-release toggle
                        Rectangle {
                            width: preTogText.implicitWidth + 16; height: 24; radius: StyleTokens.radiusSm
                            color: preTogHov.containsMouse ? (page.shaderShowPreReleases ? "#282040" : "#1a1e28") : (page.shaderShowPreReleases ? "#1e1838" : "#12151c")
                            border.color: page.shaderShowPreReleases ? "#504080" : "#2a3040"; border.width: 1
                            Behavior on color { ColorAnimation { duration: 150 } }
                            Text {
                                id: preTogText
                                anchors.centerIn: parent
                                text: page.shaderShowPreReleases ? qsTr("隐藏测试版") : qsTr("显示测试版")
                                color: page.shaderShowPreReleases ? "#9088e0" : "#687080"; font.pixelSize: StyleTokens.fontSizeXs
                            }
                            MouseArea {
                                id: preTogHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { page.shaderShowPreReleases = !page.shaderShowPreReleases }
                            }
                        }

                        Item { Layout.fillWidth: true }
                    }
                }
            }

            // ── Card Grid ──
            ListView {
                id: shaderCardView
                Layout.fillWidth: true; Layout.fillHeight: true
                model: shaderResultsModel
                spacing: 6; cacheBuffer: 200
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                    contentItem: Rectangle { implicitWidth: 4; radius: StyleTokens.radiusXs; color: StyleTokens.textMuted }
                }

                header: Item {
                    width: shaderCardView.width
                    height: shaderResultsModel.count > 0 ? 0 : 200
                    visible: shaderResultsModel.count === 0
                    Text {
                        anchors.centerIn: parent
                        text: shaderTab.shaderSearching ? qsTr("搜索中\u2026") : qsTr("输入关键词搜索光影")
                        color: "#606478"; font.pixelSize: StyleTokens.fontSizeSm
                    }
                }

                footer: Item {
                    width: shaderCardView.width
                    height: shaderTab.hasMoreShaders ? 36 : 0
                    visible: shaderTab.hasMoreShaders
                    Text {
                        anchors.centerIn: parent
                        text: shaderTab.shaderSearching ? qsTr("加载中...") : qsTr("加载更多")
                        color: "#5068c8"; font.pixelSize: StyleTokens.fontSizeSm
                    }
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: { if (!shaderTab.shaderSearching && shaderTab.hasMoreShaders) shaderTab.loadMore() }
                    }
                }

                onContentYChanged: {
                    if (!shaderTab.shaderSearching && shaderTab.hasMoreShaders && shaderCardView.count > 0) {
                        var bottomEdge = contentHeight - height
                        if (contentY >= bottomEdge - 100) shaderTab.loadMore()
                    }
                }

                delegate: Rectangle {
                    width: shaderCardView.width; height: 64; radius: StyleTokens.radiusLg
                    color: "#11141c"; border.color: StyleTokens.bgElevated

                    opacity: 0
                    Component.onCompleted: { opacity = 1 }
                    Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }

                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        hoverEnabled: true
                        onClicked: {
                            page._shaderDetailSlug = model.slug
                            page._shaderDetailTitle = model.title || ""
                            page._shaderDetailDesc = model.desc || ""
                            page._shaderDetailIcon = model.icon || ""
                            page._showShaderDetail = true
                        }
                    }

                    RowLayout {
                        anchors.fill: parent; anchors.margins: 8; spacing: 8

                        Rectangle {
                            Layout.preferredWidth: 36; Layout.preferredHeight: 36; radius: StyleTokens.radiusMd
                            color: "#1a1f2e"; clip: true
                            Image {
                                anchors.fill: parent; anchors.margins: 2
                                fillMode: Image.PreserveAspectFit
                                asynchronous: true; cache: true
                                source: model.icon || ""
                                onStatusChanged: {
                                    if (status === Image.Error) sIconFallback.visible = true
                                    else if (status === Image.Ready) sIconFallback.visible = false
                                }
                            }
                            Text {
                                id: sIconFallback
                                anchors.centerIn: parent
                                text: model ? (model.title ? model.title[0] : "S") : "S"
                                color: "#5068c8"; font.pixelSize: StyleTokens.fontSizeLg; font.bold: true
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
                                    font.pixelSize: StyleTokens.fontSizeSm; font.bold: true; elide: Text.ElideRight
                                }
                                Rectangle {
                                    Layout.preferredWidth: 56; height: 15; radius: StyleTokens.radiusXs; color: StyleTokens.bgElevated
                                    Text { anchors.centerIn: parent; text: "Modrinth"; color: "#9088e0"; font.pixelSize: StyleTokens.fontSizeXs }
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: model.desc || ""; color: "#606478"
                                font.pixelSize: StyleTokens.fontSizeXs; elide: Text.ElideRight; maximumLineCount: 1
                            }

                            RowLayout {
                                Layout.fillWidth: true; spacing: 12
                                Text { text: shaderTab.fVersions(model.versions) ? "适用: " + shaderTab.fVersions(model.versions) : ""; color: "#687080"; font.pixelSize: StyleTokens.fontSizeXs }
                                RowLayout {
                                    spacing: 3
                                    Image { source: "icons/lucide/download.svg"; width: 8; height: 8; sourceSize.width: 8; sourceSize.height: 8; Layout.alignment: Qt.AlignVCenter }
                                    Text { text: rpPage.fmtDownloads(model.downloads || 0); color: "#788090"; font.pixelSize: StyleTokens.fontSizeXs }
                                }
                                Text { text: shaderTab.fDate(model.dateModified) || "无日期"; color: "#687080"; font.pixelSize: StyleTokens.fontSizeXs }
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
            Rectangle {
                Layout.fillWidth: true; height: 150; radius: StyleTokens.radiusLg
                color: "#11141c"; border.color: StyleTokens.bgElevated

                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 12; spacing: 8

                    // Row 1: 名称 + 来源
                    RowLayout {
                        Layout.fillWidth: true; spacing: 10

                        Text { text: qsTr("名称"); color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: 32 }

                        Rectangle {
                            Layout.fillWidth: true; height: 28; radius: StyleTokens.radiusSm
                            color: rpSearchInput.activeFocus ? "#0f131c" : "#0c0e14"
                            border.color: rpSearchInput.activeFocus ? "#5068c8" : "#2a3040"
                            border.width: 1
                            Behavior on color { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }

                            TextInput {
                                id: rpSearchInput
                                anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8
                                color: "#d0d4e0"; verticalAlignment: TextInput.AlignVCenter; font.pixelSize: StyleTokens.fontSizeSm
                                // REMOVED onAccepted trigger — search only on button click (Fix 1)

                                Text {
                                    anchors.fill: parent; verticalAlignment: Text.AlignVCenter
                                    text: qsTr("输入资源包名称..."); color: "#505468"; font.pixelSize: StyleTokens.fontSizeSm
                                    visible: !rpSearchInput.text
                                }
                            }
                        }

                        Text { text: qsTr("来源"); color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: 32 }

                        Rectangle {
                            id: rpSourcePill
                            Layout.preferredWidth: 140; height: 28; radius: StyleTokens.radiusSm
                            color: (rpSrcHov.containsMouse || rpSourceMenu.visible) ? "#1e3260" : "#0c0e14"
                            border.color: (rpSrcHov.containsMouse || rpSourceMenu.visible) ? "#5078e0" : "#2a3040"; border.width: 1

                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on border.color { ColorAnimation { duration: 150 } }

                            RowLayout {
                                anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 4; spacing: 4
                                Text {
                                    Layout.fillWidth: true
                                    text: "Modrinth (MCIM)"; color: "#788db0"; font.pixelSize: StyleTokens.fontSizeSm
                                    elide: Text.ElideRight
                                }
                                Text { text: "▾"; color: "#505468"; font.pixelSize: StyleTokens.fontSizeXs }
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
                                background: Rectangle { color: "#151922"; radius: StyleTokens.radiusLg; border.color: StyleTokens.bgElevated }
                                ColumnLayout {
                                    width: parent.width - 8; spacing: 2
                                    Repeater {
                                        model: [
                                            { value: "modrinth", label: "Modrinth (MCIM镜像)" },
                                            { value: "modrinth-direct", label: "Modrinth (直连)" }
                                        ]
                                        Rectangle {
                                            Layout.fillWidth: true; height: 26; radius: StyleTokens.radiusSm
                                            color: mouse2.hovered ? "#1a2848" : "transparent"
                                            Text {
                                                anchors.centerIn: parent; text: modelData.label
                                                color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm
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

                        Text { text: qsTr("版本"); color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: 32 }

                        Rectangle {
                            id: rpVerPill
                            Layout.preferredWidth: 120; height: 28; radius: StyleTokens.radiusSm
                            color: (rpVerHov.containsMouse || rpVersionMenu.visible) ? "#1e3260" : "#0c0e14"
                            border.color: (rpVerHov.containsMouse || rpVersionMenu.visible || page.rpGameVersion) ? "#5078e0" : "#2a3040"; border.width: 1

                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on border.color { ColorAnimation { duration: 150 } }

                            RowLayout {
                                anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 4; spacing: 4
                                Text {
                                    Layout.fillWidth: true
                                    text: page.rpGameVersion ? ("MC " + page.rpGameVersion) : "全部"
                                    color: page.rpGameVersion ? "#8aaeff" : "#788090"; font.pixelSize: StyleTokens.fontSizeSm
                                    elide: Text.ElideRight
                                }
                                Text { text: "▾"; color: "#505468"; font.pixelSize: StyleTokens.fontSizeXs }
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
                                background: Rectangle { color: "#151922"; radius: StyleTokens.radiusLg; border.color: StyleTokens.bgElevated }

                                Flickable {
                                    id: rpVerFlick
                                    anchors.fill: parent; anchors.margins: 4
                                    contentHeight: rpVerInner.implicitHeight; clip: true
                                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                    ColumnLayout {
                                        id: rpVerInner
                                        width: parent.width; spacing: 2

                                        Rectangle {
                                            Layout.fillWidth: true; height: 26; radius: StyleTokens.radiusSm
                                            color: !page.rpGameVersion ? "#1a2848" : "transparent"
                                            Text {
                                                anchors.centerIn: parent; text: qsTr("全部")
                                                color: !page.rpGameVersion ? "#5068c8" : "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm
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
                                                Layout.fillWidth: true; height: 26; radius: StyleTokens.radiusSm
                                                color: modelData === page.rpGameVersion ? "#1a2848" : "transparent"
                                                Text {
                                                    anchors.centerIn: parent; text: modelData
                                                    color: modelData === page.rpGameVersion ? "#5068c8" : "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm
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

                        Text { text: qsTr("筛选"); color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: 32 }

                        // Three filter dropdowns: Category | Feature | Resolution
                        RowLayout {
                            Layout.fillWidth: true; spacing: 6

                            // ── Category dropdown ──
                            Rectangle {
                                id: rpCatPill
                                Layout.preferredWidth: 95; height: 28; radius: StyleTokens.radiusSm
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
                                        color: page.rpCategoryFilter ? "#8aaeff" : "#788090"; font.pixelSize: StyleTokens.fontSizeSm
                                        elide: Text.ElideRight
                                    }
                                    Text { text: "▾"; color: "#505468"; font.pixelSize: StyleTokens.fontSizeXs }
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
                                    background: Rectangle { color: "#151922"; radius: StyleTokens.radiusLg; border.color: StyleTokens.bgElevated }
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
                                                    Layout.fillWidth: true; height: 26; radius: StyleTokens.radiusSm
                                                    color: page.rpCategoryFilter === modelData.key ? "#1a2848" : "transparent"
                                                    Text {
                                                        anchors.centerIn: parent; text: modelData.label
                                                        color: page.rpCategoryFilter === modelData.key ? "#5068c8" : "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm
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
                                Layout.preferredWidth: 95; height: 28; radius: StyleTokens.radiusSm
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
                                        color: page.rpFeatureFilter ? "#8aaeff" : "#788090"; font.pixelSize: StyleTokens.fontSizeSm
                                        elide: Text.ElideRight
                                    }
                                    Text { text: "▾"; color: "#505468"; font.pixelSize: StyleTokens.fontSizeXs }
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
                                    background: Rectangle { color: "#151922"; radius: StyleTokens.radiusLg; border.color: StyleTokens.bgElevated }
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
                                                    Layout.fillWidth: true; height: 26; radius: StyleTokens.radiusSm
                                                    color: page.rpFeatureFilter === modelData.key ? "#1a2848" : "transparent"
                                                    Text {
                                                        anchors.centerIn: parent; text: modelData.label
                                                        color: page.rpFeatureFilter === modelData.key ? "#5068c8" : "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm
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
                                Layout.preferredWidth: 95; height: 28; radius: StyleTokens.radiusSm
                                color: (rpResHov.containsMouse || rpResMenu.visible) ? "#1e3260" : "#0c0e14"
                                border.color: (rpResHov.containsMouse || rpResMenu.visible || page.rpResolutionFilter) ? "#5078e0" : "#2a3040"; border.width: 1

                                Behavior on color { ColorAnimation { duration: 150 } }
                                Behavior on border.color { ColorAnimation { duration: 150 } }
                                RowLayout {
                                    anchors.fill: parent; anchors.leftMargin: 6; anchors.rightMargin: 3; spacing: 2
                                    Text {
                                        Layout.fillWidth: true
                                        text: page.rpResolutionFilter || "分辨率"
                                        color: page.rpResolutionFilter ? "#8aaeff" : "#788090"; font.pixelSize: StyleTokens.fontSizeSm
                                        elide: Text.ElideRight
                                    }
                                    Text { text: "▾"; color: "#505468"; font.pixelSize: StyleTokens.fontSizeXs }
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
                                    background: Rectangle { color: "#151922"; radius: StyleTokens.radiusLg; border.color: StyleTokens.bgElevated }
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
                                                    Layout.fillWidth: true; height: 26; radius: StyleTokens.radiusSm
                                                    color: page.rpResolutionFilter === modelData.key ? "#1a2848" : "transparent"
                                                    Text {
                                                        anchors.centerIn: parent; text: modelData.label
                                                        color: page.rpResolutionFilter === modelData.key ? "#5068c8" : "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm
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
                        Rectangle {
                            width: rpPreTogText.implicitWidth + 16; height: 24; radius: StyleTokens.radiusSm
                            color: rpPreTogHov.containsMouse ? (page.rpShowPreReleases ? "#282040" : "#1a1e28") : (page.rpShowPreReleases ? "#1e1838" : "#12151c")
                            border.color: page.rpShowPreReleases ? "#504080" : "#2a3040"; border.width: 1
                            Behavior on color { ColorAnimation { duration: 150 } }
                            Text {
                                id: rpPreTogText
                                anchors.centerIn: parent
                                text: page.rpShowPreReleases ? qsTr("隐藏测试版") : qsTr("显示测试版")
                                color: page.rpShowPreReleases ? "#9088e0" : "#687080"; font.pixelSize: StyleTokens.fontSizeXs
                            }
                            MouseArea {
                                id: rpPreTogHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
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
                            width: 72; height: 28; radius: StyleTokens.radiusSm
                            color: StyleTokens.accentHover
                            Text { anchors.centerIn: parent; text: qsTr("搜索"); color: "#e8ecf8"; font.pixelSize: StyleTokens.fontSizeSm }
                            MouseArea {
                                anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                // FIX 1: Search button — the ONLY trigger for loadRpFirstPage() from filter changes
                                onClicked: loadRpFirstPage()
                            }
                        }

                        Rectangle {
                            width: 72; height: 28; radius: StyleTokens.radiusSm
                            color: rpResetHov.hovered ? "#252a38" : "#151922"
                            border.color: "#2a3040"; border.width: 1
                            visible: page.rpCategoryFilter || page.rpFeatureFilter || page.rpResolutionFilter || page.rpGameVersion || rpSearchInput.text
                            Text { anchors.centerIn: parent; text: qsTr("重置"); color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm }
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
                color: "#505468"; font.pixelSize: StyleTokens.fontSizeSm
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
                            color: "#5068c8"; font.pixelSize: StyleTokens.fontSizeSm
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
                        radius: StyleTokens.radiusLg; border.color: rpCardHov.hovered ? "#5068c8" : "#1a1f2a"; border.width: 1
                        scale: rpCardHov.hovered ? 1.01 : 1.0

                        opacity: 0
                        Component.onCompleted: { opacity = 1 }

                        Behavior on color { ColorAnimation { duration: 150 } }
                        Behavior on border.color { ColorAnimation { duration: 150 } }
                        Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutBack } }
                        Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }

                        // Hover glow overlay
                        Rectangle {
                            anchors.fill: parent; radius: parent.radius
                            opacity: rpCardHov.hovered ? 0.06 : 0
                            gradient: Gradient {
                                GradientStop { position: 0; color: StyleTokens.accentLight }
                                GradientStop { position: 1; color: StyleTokens.accentLight }
                            }
                            Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
                        }

                        RowLayout {
                            anchors.fill: parent; anchors.margins: 10; spacing: 10

                            // Icon (network image with MCIM fallback)
                            Rectangle {
                                width: 44; height: 44; radius: StyleTokens.radiusLg; color: StyleTokens.bgCard
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
                                    color: "#5068c8"; font.pixelSize: StyleTokens.fontSizeXl; font.bold: true
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
                                        font.pixelSize: StyleTokens.fontSizeMd; font.bold: true; elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                    RowLayout {
                                        spacing: 3
                                        Image { source: "icons/lucide/download.svg"; width: 8; height: 8; sourceSize.width: 8; sourceSize.height: 8; Layout.alignment: Qt.AlignVCenter }
                                        Text {
                                            text: rpPage.fmtDownloads(model.downloads || 0)
                                            color: "#788090"; font.pixelSize: StyleTokens.fontSizeXs
                                        }
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
                                                height: 16; width: tText.implicitWidth + 10; radius: StyleTokens.radiusSm
                                                color: "#151922"; border.color: "#2a3040"; border.width: 1
                                                property string tagLabel: ""
                                                Text {
                                                    id: tText; anchors.centerIn: parent
                                                    text: tagLabel; color: "#788090"; font.pixelSize: StyleTokens.fontSizeXs
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
                                                height: 16; width: tText2.implicitWidth + 10; radius: StyleTokens.radiusSm
                                                color: "#1a1428"; border.color: "#382848"; border.width: 1
                                                property string tagLabel: ""
                                                Text {
                                                    id: tText2; anchors.centerIn: parent
                                                    text: tagLabel; color: "#a878c8"; font.pixelSize: StyleTokens.fontSizeXs
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
                                                height: 16; width: tText3.implicitWidth + 10; radius: StyleTokens.radiusSm
                                                color: "#282218"; border.color: "#504828"; border.width: 1
                                                property string tagLabel: ""
                                                Text {
                                                    id: tText3; anchors.centerIn: parent
                                                    text: tagLabel; color: "#c8a860"; font.pixelSize: StyleTokens.fontSizeXs
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
                                    text: model.desc || ""; color: "#505468"; font.pixelSize: StyleTokens.fontSizeXs
                                    elide: Text.ElideRight; maximumLineCount: 1
                                }

                                // Row 4: Version range + date
                                RowLayout {
                                    Layout.fillWidth: true; spacing: 6
                                    Text {
                                        text: rpPage.fmtVersionRange(model.versionStr || "")
                                        color: "#687080"; font.pixelSize: StyleTokens.fontSizeXs
                                        visible: text !== ""
                                    }
                                    Text {
                                        text: model.updated ? String(model.updated).slice(0, 10) : ""
                                        color: "#687080"; font.pixelSize: StyleTokens.fontSizeXs; visible: model.updated !== undefined
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
        color: hasBg ? Qt.rgba(0.047, 0.059, 0.086, 0.92) : "#0c0f16"
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
        color: hasBg ? Qt.rgba(0.047, 0.059, 0.086, 0.92) : "#0c0f16"
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
        color: hasBg ? Qt.rgba(0.047, 0.059, 0.086, 0.92) : "#0c0f16"
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
