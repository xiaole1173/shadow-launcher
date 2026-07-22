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
        var q = rpFilterCard.searchText || ""
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
        var q = rpFilterCard.searchText || ""
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
                    border.color: page.selectedVersionId === model.versionId ? StyleTokens.accent : "transparent"
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
                                font.family: StyleTokens.fontFamilyMono
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
                                       (installBtn.hovered && installBtn.enabled ? StyleTokens.textInverse : "#707888"))
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
                                              (installBtn.hovered && installBtn.enabled ? "#5d6fe0" : StyleTokens.border))
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
            var q = modFilterCard.searchText ? modFilterCard.searchText.trim() : ""
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

                        // Filter Card (DownloadFilterCard)
            DownloadFilterCard {
                id: modFilterCard
                searchPlaceholder: qsTr("输入 Mod 名称...")
                buttonsPosition: 0
                showPreRelease: true
                preReleaseActive: page.modShowPreReleases
                onPreReleaseToggled: function(v) { page.modShowPreReleases = v; modTab.doModSearch() }
                onSearchRequested: modTab.doModSearch()
                onResetRequested: function() {
                    page.modLoader = ""; page.modGameVersion = ""
                    page.modCategory = ""; page.modEnvironment = ""
                    modFilterCard.searchText = ""; modResultsModel.clear()
                    modTab.doModSearch()
                }

                content: ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    // Row 2: loader + version + category + environment
                    RowLayout {
                        Layout.fillWidth: true; spacing: 10

                        Text { text: qsTr("加载器"); color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: 32 }
                        ShadowDropdown {
                            Layout.preferredWidth: 120
                            model: ["", "fabric", "forge", "quilt", "neoforge", "rift", "liteloader"]
                            labelFn: function(v) { return page.modLoaderLabels[v] || "全部" }
                            currentValue: page.modLoader
                            onValueSelected: function(v) { page.modLoader = v }
                        }

                        Text { text: qsTr("版本"); color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: 32 }
                        ShadowDropdown {
                            Layout.preferredWidth: 120
                            model: {
                                if (!backend || !backend.versionIds) return ["1.21.10","1.20.6"]
                                var seen = new Set(); var groups = []
                                for (var i = 0; i < backend.versionIds.length; i++) {
                                    var v = backend.versionIds[i]
                                    if (!page.modShowPreReleases && !/^[0-9.]+$/.test(v)) continue
                                    var major = v.split(/[.\-]/).slice(0, 2).join(".")
                                    if (!seen.has(major)) { seen.add(major); groups.push(major) }
                                    if (groups.length >= 30) break
                                }
                                return [""].concat(groups)
                            }
                            labelFn: function(v) { return v ? "MC " + v : "全部" }
                            currentValue: page.modGameVersion
                            onValueSelected: function(v) { page.modGameVersion = v }
                        }

                        Text { text: qsTr("类别"); color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: 32 }
                        ShadowDropdown {
                            Layout.preferredWidth: 115
                            model: [""].concat(Object.keys(page.modCatLabels))
                            labelFn: function(v) { return page.modCatLabels[v] || "全部" }
                            currentValue: page.modCategory
                            onValueSelected: function(v) { page.modCategory = v }
                        }

                        Text { text: qsTr("环境"); color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: 32 }
                        ShadowDropdown {
                            Layout.preferredWidth: 95
                            model: ["", "client", "server"]
                            labelFn: function(v) { return page.modEnvLabels[v] || v || "全部" }
                            currentValue: page.modEnvironment
                            onValueSelected: function(v) { page.modEnvironment = v }
                        }
                    }
                }
            }
            // Results

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
            backend.searchShadersEx(shaderFilterCard.searchText.trim(), ver, a.concat(b,c,d), [], [], 0, 50)
        }
        function loadMore() {
            if (!backend || shaderSearching || !hasMoreShaders) return
            shaderSearching = true; shaderOffset += 50
            var a = shaderCategory ? [shaderCategory] : []
            var b = shaderFeature ? [shaderFeature] : []
            var c = shaderPerformance ? [shaderPerformance] : []
            var d = shaderLoader ? [shaderLoader] : []
            var ver = page.shaderGameVersion ? [page.shaderGameVersion] : []
            backend.searchShadersEx(shaderFilterCard.searchText.trim(), ver, a.concat(b,c,d), [], [], shaderOffset, 50)
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
                shaderTab.hasMoreShaders = (results && results.length >= 50)
            }
        }

        ColumnLayout {
            anchors.fill: parent; anchors.margins: 12; spacing: 8

                        // -- Filter Card (DownloadFilterCard) --
            DownloadFilterCard {
                id: shaderFilterCard
                searchPlaceholder: qsTr("输入光影名称...")
                buttonsPosition: 0
                showPreRelease: true
                preReleaseActive: page.shaderShowPreReleases
                onPreReleaseToggled: function(v) { page.shaderShowPreReleases = v }
                onSearchRequested: shaderTab.doSearch()
                onResetRequested: shaderTab.resetFilters()

                content: ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true; spacing: 8

                        Text { text: qsTr("风格"); color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: 28 }
                        ShadowDropdown {
                            Layout.preferredWidth: 90
                            model: shaderTab.shaderCats
                            valueKey: "slug"
                            currentValue: shaderTab.shaderCategory
                            onValueSelected: function(v) { shaderTab.shaderCategory = v }
                        }

                        Text { text: qsTr("特性"); color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: 28 }
                        ShadowDropdown {
                            Layout.preferredWidth: 110
                            model: shaderTab.shaderFeatures
                            valueKey: "slug"
                            currentValue: shaderTab.shaderFeature
                            onValueSelected: function(v) { shaderTab.shaderFeature = v }
                        }

                        Text { text: qsTr("性能"); color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: 28 }
                        ShadowDropdown {
                            Layout.preferredWidth: 80
                            model: shaderTab.shaderPerfs
                            valueKey: "slug"
                            currentValue: shaderTab.shaderPerformance
                            onValueSelected: function(v) { shaderTab.shaderPerformance = v }
                        }

                        Text { text: qsTr("加载器"); color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: 32 }
                        ShadowDropdown {
                            Layout.preferredWidth: 90
                            model: shaderTab.shaderLoaders
                            valueKey: "slug"
                            currentValue: shaderTab.shaderLoader
                            onValueSelected: function(v) { shaderTab.shaderLoader = v }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true; spacing: 8

                        Text { text: qsTr("版本"); color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: 28 }
                        ShadowDropdown {
                            Layout.preferredWidth: 100
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
                                return [""].concat(groups)
                            }
                            labelFn: function(v) { return v ? "MC " + v : "全部" }
                            currentValue: page.shaderGameVersion
                            onValueSelected: function(v) { page.shaderGameVersion = v }
                        }

                        Item { Layout.fillWidth: true }
                    }
                }
            }
            // -- Card Grid --

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
                        color: StyleTokens.accentHover; font.pixelSize: StyleTokens.fontSizeSm
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

                        // -- Filter Card (DownloadFilterCard) --
            DownloadFilterCard {
                id: rpFilterCard
                searchPlaceholder: qsTr("输入资源包名称...")
                searchText: rpFilterCard.searchText
                showSourceDropdown: true
                sourceModel: [
                    { value: "modrinth", label: "Modrinth (MCIM镜像)" },
                    { value: "modrinth-direct", label: "Modrinth (直连)" }
                ]
                sourceDisplayText: "Modrinth (MCIM)"
                sourceValue: "modrinth"
                sourceValueKey: "value"

                buttonsPosition: 1
                showPreRelease: true
                preReleaseActive: page.rpShowPreReleases
                onPreReleaseToggled: function(v) { page.rpShowPreReleases = v }
                onSearchRequested: rpPage.doRpSearch()
                onResetRequested: rpPage.resetFilters()

                content: ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true; spacing: 10

                        Text { text: qsTr("版本"); color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: 32 }
                        ShadowDropdown {
                            Layout.preferredWidth: 120
                            model: {
                                if (!backend || !backend.versionIds) return ["1.21.10","1.20.6"]
                                var seen = new Set(); var groups = []
                                for (var i = 0; i < backend.versionIds.length; i++) {
                                    var v = backend.versionIds[i]
                                    if (!page.rpShowPreReleases && !/^[0-9.]+$/.test(v)) continue
                                    var major = v.split(/[.\-]/).slice(0, 2).join(".")
                                    if (!seen.has(major)) { seen.add(major); groups.push(major) }
                                    if (groups.length >= 30) break
                                }
                                return [""].concat(groups)
                            }
                            labelFn: function(v) { return v ? "MC " + v : "全部" }
                            currentValue: page.rpGameVersion
                            onValueSelected: function(v) { page.rpGameVersion = v }
                        }

                        Text { text: qsTr("筛选"); color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: 32 }

                        ShadowDropdown {
                            Layout.preferredWidth: 95
                            model: [
                                {key:"", label:"全部"},
                                {key:"combat", label:"战斗"}, {key:"cursed", label:"猎奇"}, {key:"decoration", label:"装饰"},
                                {key:"modded", label:"模组适配"}, {key:"realistic", label:"写实"}, {key:"simplistic", label:"简约"},
                                {key:"themed", label:"主题"}, {key:"tweaks", label:"微调"}, {key:"utility", label:"实用"},
                                {key:"vanilla-like", label:"原版"}, {key:"fantasy", label:"幻想"}, {key:"modern", label:"现代"},
                                {key:"medieval", label:"中世纪"}, {key:"futuristic", label:"未来"}, {key:"cartoon", label:"卡通"},
                                {key:"pvp", label:"PVP"}, {key:"minigame", label:"小游戏"}, {key:"gui", label:"界面"},
                                {key:"font", label:"字体"}, {key:"hd", label:"高清"}, {key:"photorealism", label:"照片"},
                                {key:"cute", label:"可爱"}, {key:"dark", label:"暗色"}, {key:"light", label:"亮色"}, {key:"clean", label:"简洁"}
                            ]
                            valueKey: "key"
                            displayText: {
                                var k = page.rpCategoryFilter
                                var m = { "":"类别", "combat":"战斗", "cursed":"猎奇", "decoration":"装饰",
                                    "modded":"模组适配", "realistic":"写实", "simplistic":"简约",
                                    "themed":"主题", "tweaks":"微调", "utility":"实用",
                                    "vanilla-like":"原版", "fantasy":"幻想", "modern":"现代",
                                    "medieval":"中世纪", "futuristic":"未来", "cartoon":"卡通",
                                    "pvp":"PVP", "minigame":"小游戏", "gui":"界面", "font":"字体",
                                    "hd":"高清", "photorealism":"照片", "cute":"可爱",
                                    "dark":"暗色", "light":"亮色", "clean":"简洁" }
                                return m[k] || (k || "类别")
                            }
                            currentValue: page.rpCategoryFilter
                            onValueSelected: function(v) { page.rpCategoryFilter = v }
                        }

                        ShadowDropdown {
                            Layout.preferredWidth: 95
                            model: [
                                {key:"", label:"全部"},
                                {key:"audio", label:"音频"}, {key:"blocks", label:"方块"},
                                {key:"core-shaders", label:"核心着色器"}, {key:"entities", label:"实体"},
                                {key:"environment", label:"环境"}, {key:"equipment", label:"装备"},
                                {key:"fonts", label:"字体"}, {key:"gui", label:"图形界面"},
                                {key:"items", label:"物品"}, {key:"locale", label:"本地化"},
                                {key:"models", label:"模型"}, {key:"minecraft", label:"Minecraft"}
                            ]
                            valueKey: "key"
                            displayText: {
                                var k = page.rpFeatureFilter
                                var m = { "audio":"音频", "blocks":"方块", "core-shaders":"核心着色器",
                                    "entities":"实体", "environment":"环境", "equipment":"装备",
                                    "fonts":"字体", "gui":"图形界面", "items":"物品",
                                    "locale":"本地化", "models":"模型", "minecraft":"Minecraft" }
                                return k ? (m[k] || k) : "功能"
                            }
                            currentValue: page.rpFeatureFilter
                            onValueSelected: function(v) { page.rpFeatureFilter = v }
                        }

                        ShadowDropdown {
                            Layout.preferredWidth: 95
                            model: [
                                {key:"", label:"全部"},
                                {key:"8x", label:"8x"}, {key:"16x", label:"16x"},
                                {key:"32x", label:"32x"}, {key:"64x", label:"64x"},
                                {key:"128x", label:"128x"}, {key:"256x", label:"256x"},
                                {key:"512x", label:"512x"}
                            ]
                            valueKey: "key"
                            displayText: page.rpResolutionFilter || "分辨率"
                            currentValue: page.rpResolutionFilter
                            onValueSelected: function(v) { page.rpResolutionFilter = v }
                        }
                    }
                }
            }
            // -- Card Grid --
