// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

// ModDetailPage
// Full-screen detail page for a Mod project's version list
// Architecture: InstallPage-style — fixed top bar + Flickable + cards

Rectangle {
    id: root
    readonly property bool hasBg: backend && typeof backend.customBgPath === "string" && backend.customBgPath.length > 0
    anchors.fill: parent
    color: hasBg ? "transparent" : "#0c0f16"

    // ── 入场动画：从右侧滑入 ──
    x: 60
    opacity: 0
    Component.onCompleted: { x = 0; opacity = 1 }
    Behavior on x { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } }
    Behavior on opacity { NumberAnimation { duration: 350; easing.type: Easing.OutCubic } }

    // ── Injected properties (set by Loader onLoaded) ──
    property var backend: null
    property var toastManager: null
    property var mainWindow: null

    property string modDetailSlug: ""
    property string modDetailTitle: ""
    property string modDetailDesc: ""
    property string modDetailIcon: ""
    property bool modDetailLoading: false
    property var modDetailRawVersions: []
    property var modDetailVersionMap: ({})
    property var pendingModDownload: ({})

    // ── 前置模组 ──
    property var modDetailDependencies: []
    property bool modDetailDepsLoading: false
    property bool showDeps: false
    property var modNavStack: []

    // ── 版本列表入场动画 ──
    property bool _versionListEnter: false
    // ── 版本数据缓存 (slug → {rawVersions, versionMap, grouped}) ──
    property var _versionCache: ({})

    signal goBack()

    // ── Listen for dependency resolution ──
    Connections {
        target: backend && backend.modManager ? backend.modManager : null
        function onDependenciesResolved(slug, deps) {
            if (slug !== modDetailSlug) return
            modDetailDepsLoading = false
            modDetailDependencies = deps || []
            showDeps = (deps && deps.length > 0)
        }
    }

    // ── Trigger version fetch ──
    onModDetailSlugChanged: {
        if (modDetailSlug && backend) {
            // ── 检查缓存 ──
            var cached = _versionCache[modDetailSlug]
            if (cached) {
                modDetailLoading = false
                modDetailRawVersions = cached.raw
                modDetailVersionMap = cached.map
                expandedGroups = []
                showTestVersions = false
                _versionListEnter = true
            } else {
                modDetailLoading = true
                modDetailRawVersions = []
                modDetailVersionMap = {}
                expandedGroups = []
                showTestVersions = false
                _versionListEnter = false
                backend.fetchModVersions([modDetailSlug])
            }

            // Fetch dependencies
            if (backend.modManager) {
                modDetailDepsLoading = true
                modDetailDependencies = []
                showDeps = false
                backend.modManager.getModDependencies(modDetailSlug)
            }
        }
    }

    // ── Helpers ──
    function stripSuffix(v) {
        var re = /-(?:snapshot|pre|rc|alpha|beta)[\d.\-]*$/i
        var m = v.match(re)
        return m ? v.slice(0, m.index) : v
    }
    // Strip composite-key loader suffix: "1.21.8|fabric" → "1.21.8"
    function stripLoader(v) {
        var idx = v.lastIndexOf("|")
        return idx >= 0 ? v.substring(0, idx) : v
    }
    function preReleaseTag(v) {
        if (/-snapshot/i.test(v)) return "快照版"
        if (/-pre/i.test(v)) return "预览版"
        if (/-rc/i.test(v)) return "发布候选版"
        return ""
    }
    function isTestVersion(v) {
        return /^\d{1,2}w\d{2}[a-z]$/i.test(v)
    }
    function testMajor(v) {
        var m = v.match(/^(\d{1,2}w)/i)
        return m ? m[1] : v
    }

    property bool showTestVersions: false

    property var grouped: {
        var groups = {}
        var raw = modDetailRawVersions || []
        var map = modDetailVersionMap || {}
        for (var i = 0; i < raw.length; i++) {
            var v = raw[i]
            var d = map[v]
            // Use C++-provided game_version if available, else strip composite-key loader suffix
            var gv = d ? (d.gameVersion || stripLoader(v)) : stripLoader(v)
            var gvs = d ? (d.gameVersions || []) : []
            if (gvs.length === 0) gvs = [gv]
            var first = gvs[0] || gv
            var base = stripSuffix(first)
            if (!showTestVersions && isTestVersion(base)) continue
            var major
            if (isTestVersion(base)) {
                major = testMajor(base)
            } else {
                var parts = base.split(".")
                major = parts.length >= 2 ? parts[0] + "." + parts[1] : base
            }
            if (!groups[major]) groups[major] = []
            groups[major].push(v)
        }
        for (var k in groups) {
            groups[k].sort(function(a,b){
                var da = getVersionDetail(a); var db = getVersionDetail(b)
                var dateA = da ? da.date : ""; var dateB = db ? db.date : ""
                if (dateA > dateB) return -1; if (dateA < dateB) return 1; return 0
            })
        }
        var result = []
        for (var kk in groups) { result.push({major: kk, versions: groups[kk]}) }
        result.sort(function(a,b){
            var aTest = /w$/i.test(a.major), bTest = /w$/i.test(b.major)
            if (aTest && !bTest) return -1
            if (!aTest && bTest) return 1
            if (aTest && bTest) { return parseInt(a.major) - parseInt(b.major) }
            var as = a.major.split("."), bs = b.major.split(".")
            var am = parseInt(as[0])||0, bm = parseInt(bs[0])||0
            if (am !== bm) return bm - am
            return (parseInt(bs[1])||0) - (parseInt(as[1])||0)
        })
        return result
    }
    property var expandedGroups: []

    function isExpanded(major) { return expandedGroups.indexOf(major) >= 0 }
    function toggleGroup(major) {
        var arr = expandedGroups.slice(); var idx = arr.indexOf(major)
        if (idx >= 0) arr.splice(idx, 1); else arr.push(major)
        expandedGroups = arr
    }
    function getVersionDetail(verStr) {
        var map = modDetailVersionMap || {}; return map[verStr] || null
    }
    function formatDate(isoStr) {
        if (!isoStr) return "-"; return isoStr.slice(0, 10)
    }
    function formatDL(n) {
        if (n >= 100000000) return (n / 100000000).toFixed(1) + "亿"
        if (n >= 10000) return (n / 10000).toFixed(0) + "万"
        return String(n || 0)
    }

    // ━━━━━━━━━━━━━━━━━━━━ TOP BAR ━━━━━━━━━━━━━━━━━━━━
    Rectangle {
        id: topBar
        anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
        height: 44; color: hasBg ? "transparent" : "#0c0f16"; z: 10

        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12; spacing: 10

            // Back button — InstallPage style
            Rectangle {
                id: backBtnRect
                width: backLabel.implicitWidth + 20; height: 30; radius: StyleTokens.radiusMd
                color: backMouse.containsMouse ? "#1a2440" : "transparent"
                Behavior on color { ColorAnimation { duration: 150 } }

                property real _eScale: 1.0
                scale: _eScale
                Timer { id: backRestoreTimer; interval: 120
                    onTriggered: { backBtnRect._eScale = 1.0 }
                }
                Behavior on _eScale {
                    SpringAnimation { spring: 1.8; damping: 0.3; epsilon: 0.01 }
                }

                Text {
                    id: backLabel; anchors.centerIn: parent
                    text: "\u2190 \u8fd4\u56de"; font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.Medium
                    color: backMouse.containsMouse ? "#6080e8" : "#a0a8c0"
                }
                MouseArea {
                    id: backMouse; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        backBtnRect._eScale = 0.92
                        backRestoreTimer.restart()

                        var stack = modNavStack || []
                        if (stack.length > 0) {
                            var prev = stack.pop()
                            modNavStack = stack
                            modDetailSlug = prev.slug
                            modDetailTitle = prev.title
                            modDetailDesc = prev.desc || ""
                            modDetailIcon = prev.icon || ""
                        } else {
                            root.goBack()
                        }
                    }
                }
            }

            Item { Layout.fillWidth: true }

            // Title
            Text {
                text: modDetailTitle || modDetailSlug || ""
                font.pixelSize: StyleTokens.fontSizeLg; font.weight: Font.Bold; color: StyleTokens.textSecondary
                Layout.fillWidth: true
                elide: Text.ElideRight; horizontalAlignment: Text.AlignHCenter
            }

            Item { Layout.fillWidth: true }
            Item { width: backLabel.implicitWidth + 20 } // spacer for symmetry
        }
    }

    // ━━━━━━━━━━━━━━━━━━━━ CONTENT ━━━━━━━━━━━━━━━━━━━━
    Flickable {
        id: contentFlick
        anchors.top: topBar.bottom; anchors.left: parent.left; anchors.right: parent.right
        anchors.bottom: parent.bottom
        contentWidth: width; contentHeight: contentCol.implicitHeight + 32
        clip: true; flickableDirection: Flickable.VerticalFlick
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 6 }

        ColumnLayout {
            id: contentCol
            width: parent.width - 32; x: 16; spacing: 12

            // ── INFO CARD ──
            DetailInfoCard {
                id: infoCard
                cardIcon: root.modDetailIcon
                cardTitle: root.modDetailTitle
                cardDesc: root.modDetailDesc

                // Stats
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 24
                    Text {
                        text: "Slug: " + (modDetailSlug || "")
                        color: "#7888a8"; font.pixelSize: StyleTokens.fontSizeSm
                        elide: Text.ElideRight; Layout.fillWidth: true
                    }
                    Text {
                        id: verCountText
                        property int _displayCount: 0
                        text: qsTr("版本数量: ") + _displayCount
                        color: "#7888a8"; font.pixelSize: StyleTokens.fontSizeSm
                        Behavior on _displayCount {
                            NumberAnimation { duration: 2000; easing.type: Easing.OutCubic }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 24
                    Text {
                        text: qsTr("来源: Modrinth (MCIM 镜像)")
                        color: "#7888a8"; font.pixelSize: StyleTokens.fontSizeSm
                    }
                    Rectangle {
                        id: testToggleBtn
                        width: testBtn.implicitWidth + 14; height: 22; radius: StyleTokens.radiusSm
                        color: showTestVersions ? "#1a3a68" : "#11141c"
                        border.color: (testHov.containsMouse || showTestVersions) ? "#3a5ed0" : "#2a3040"
                        border.width: (testHov.containsMouse || showTestVersions) ? 1.5 : 1

                        property real _eScale: 1.0
                        scale: _eScale
                        Timer { id: testRestoreTimer; interval: 100
                            onTriggered: { testToggleBtn._eScale = 1.0 }
                        }
                        Behavior on color { ColorAnimation { duration: 150 } }
                        Behavior on border.color { ColorAnimation { duration: 150 } }
                        Behavior on border.width { NumberAnimation { duration: 150 } }
                        Behavior on _eScale {
                            SpringAnimation { spring: 1.8; damping: 0.3; epsilon: 0.01 }
                        }
                        Text {
                            id: testBtn; anchors.centerIn: parent; font.pixelSize: StyleTokens.fontSizeXs
                            text: showTestVersions ? "隐藏测试版" : "显示测试版"
                            color: showTestVersions ? "#8aaeff" : "#505468"
                        }
                        MouseArea {
                            id: testHov; anchors.fill: parent; hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                testToggleBtn._eScale = 0.9
                                testRestoreTimer.restart()
                                showTestVersions = !showTestVersions
                            }
                        }
                    }
                }
            }

            // ── 前置模组提醒 ──
            Item {
                Layout.fillWidth: true
                height: (showDeps && !modDetailLoading) ? (20 + 8 + modDetailDependencies.length * (48 + 8)) : 0
                visible: showDeps && !modDetailLoading && modDetailDependencies.length > 0
                clip: true

                ColumnLayout {
                    id: depSection
                    width: parent.width
                    spacing: 8

                    Text {
                        text: "\u524D\u7F6E\u6A21\u7EC4"
                        color: "#e6a600"
                        font.pixelSize: StyleTokens.fontSizeSm
                        font.weight: Font.Medium
                    }

                    Repeater {
                        model: modDetailDependencies
                        delegate: Rectangle {
                            id: depCard
                            Layout.fillWidth: true
                            height: 48
                            radius: StyleTokens.radiusMd
                            color: depHover.containsMouse ? "#252a1c10" : "#1a2b1c00"
                            border.color: depHover.containsMouse ? "#804b3a00" : "#664b3a00"

                            opacity: 0
                            Timer {
                                interval: index * 80 + 200
                                running: showDeps && !modDetailLoading
                                repeat: false
                                onTriggered: depCard.opacity = 1
                            }
                            Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on border.color { ColorAnimation { duration: 150 } }

                            Rectangle {
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: 3; radius: StyleTokens.radiusXs
                                color: "#e6a600"
                            }

                            Row {
                                anchors.left: parent.left
                                anchors.leftMargin: 14
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 10

                                Text {
                                    text: "\u26A0"
                                    anchors.verticalCenter: parent.verticalCenter
                                    font.pixelSize: StyleTokens.fontSizeLg
                                }

                                Rectangle {
                                    width: 30; height: 30; radius: StyleTokens.radiusSm; color: StyleTokens.bgInput
                                    anchors.verticalCenter: parent.verticalCenter
                                    clip: true
                                    Image {
                                        anchors.fill: parent
                                        fillMode: Image.PreserveAspectCrop
                                        asynchronous: true; cache: true
                                        source: modelData.icon_url || ""
                                        sourceSize.width: 60; sourceSize.height: 60
                                    }
                                }

                                Column {
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 2
                                    Text {
                                        text: modelData.title || modelData.project_id || ""
                                        color: depHover.containsMouse ? "#f5d080" : "#f0c060"
                                        font.pixelSize: StyleTokens.fontSizeMd
                                        font.weight: Font.Medium
                                    }
                                    Text {
                                        text: modelData.dependency_type === "required" ? "\u5FC5\u9700\u524D\u7F6E" : "\u53EF\u9009\u524D\u7F6E"
                                        color: "#b09040"
                                        font.pixelSize: StyleTokens.fontSizeSm
                                    }
                                }
                            }

                            MouseArea {
                                id: depHover
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: modelData.slug ? Qt.PointingHandCursor : Qt.ArrowCursor
                                onClicked: {
                                    if (modelData.slug) {
                                        var stack = modNavStack || []
                                        stack.push({
                                            slug: modDetailSlug,
                                            title: modDetailTitle,
                                            desc: modDetailDesc,
                                            icon: modDetailIcon
                                        })
                                        modNavStack = stack
                                        modDetailTitle = modelData.title
                                        modDetailDesc = modelData.description || ""
                                        modDetailIcon = modelData.icon_url || ""
                                        modDetailSlug = modelData.slug
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ── Loading indicator ──
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: (modDetailLoading || grouped.length === 0) ? 60 : 0
                visible: modDetailLoading || grouped.length === 0

                Row {
                    anchors.centerIn: parent; spacing: 8

                    Rectangle {
                        id: spinnerBox
                        width: 24; height: 24; radius: StyleTokens.radiusXl; color: "transparent"
                        visible: modDetailLoading

                        property real _angle: 0
                        NumberAnimation on _angle {
                            running: modDetailLoading
                            from: 0; to: 360; duration: 1000; loops: Animation.Infinite
                        }
                        on_AngleChanged: spinCanvas.requestPaint()

                        Canvas {
                            id: spinCanvas
                            anchors.fill: parent; visible: modDetailLoading
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.clearRect(0, 0, width, height)
                                var cx = width / 2, cy = height / 2
                                var r = Math.min(cx, cy) - 3
                                if (r <= 0) return
                                var startRad = (spinnerBox._angle - 90) * Math.PI / 180
                                var endRad = (spinnerBox._angle + 180) * Math.PI / 180
                                ctx.strokeStyle = "#5b8def"
                                ctx.lineWidth = 2; ctx.lineCap = "round"
                                ctx.beginPath()
                                ctx.arc(cx, cy, r, startRad, endRad)
                                ctx.stroke()
                            }
                        }
                    }
                    Text {
                        text: modDetailLoading ? "加载版本中..."
                            : (grouped.length === 0 ? "无可用版本" : "")
                        color: "#606478"; font.pixelSize: StyleTokens.fontSizeSm
                    }
                }
            }

            // ── Section: Version List ──
            Text {
                visible: _versionListEnter
                opacity: _versionListEnter ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
                text: qsTr("版本列表")
                font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.DemiBold; color: "#a0a8c0"
                Layout.topMargin: 8; Layout.leftMargin: 4
            }

            // ── Version groups ──
            ColumnLayout {
                id: versionGroupsLayout
                Layout.fillWidth: true
                spacing: 8
                visible: _versionListEnter

                Repeater {
                    model: !modDetailLoading ? grouped : []
                    delegate: ExpandableGroupCard {
                        id: groupCard
                        Layout.fillWidth: true
                        title: "MC " + modelData.major
                        subtitle: modelData.versions.length + " 个版本"
                        expanded: isExpanded(modelData.major)
                        onToggled: toggleGroup(modelData.major)

                        // ── 错峰入场 ──
                        opacity: 0
                        Timer {
                            interval: index * 80 + 100
                            running: _versionListEnter
                            repeat: false
                            onTriggered: groupCard.opacity = 1
                        }
                        Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutBack; easing.overshoot: 0.2 } }

                    Repeater {
                        model: modelData.versions
                        delegate: DetailVersionCard {
                            width: parent.width - 24
                            x: 24
                                versionLabel: {
                                    var d = getVersionDetail(modelData)
                                    return d ? d.versionNumber : modelData
                                }

                                tags: {
                                    var result = []
                                    var d = getVersionDetail(modelData)
                                    if (d && d.loaders) {
                                        for (var li = 0; li < d.loaders.length; li++) {
                                            result.push({text: d.loaders[li], color: StyleTokens.accentLink, bg: "#1a2848"})
                                        }
                                    }
                                    var gvClean = d ? (d.gameVersion || "") : ""
                                    var pr = preReleaseTag(gvClean || root.stripLoader(modelData))
                                    if (pr) result.push({text: pr, color: "#d0a050", bg: "#382818"})
                                    return result
                                }

                                infoLines: {
                                    var d = getVersionDetail(modelData)
                                    var gvClean = d ? (d.gameVersion || "") : ""
                                    return [
                                        { label: "MC:", value: d ? (d.gameVersions || [gvClean || modelData]).join(", ") : (gvClean || modelData) },
                                        { label: "", value: formatDate(d ? d.date : "") + "  |  下载量 " + formatDL(d ? d.downloads : 0) }
                                    ]
                                }

                                hasDownload: true
                                onDownloadClicked: {
                                    var d = getVersionDetail(modelData)
                                    if (!d || !d.url) {
                                        if (toastManager) toastManager.show("无法获取下载地址")
                                        return
                                    }
                                    var loaders = d.loaders || []
                                    var loader = loaders.length > 0 ? loaders[0] : ""
                                    var vn = d.versionNumber || modelData
                                    var safeTitle = (modDetailTitle || modDetailSlug || "mod").replace(/[\\\/:*?"<>|]/g, "_").replace(/\s+/g, "_")
                                    var fn = safeTitle + "-" + vn + (loader ? "-" + loader : "") + ".jar"
                                    var mineDir = String(backend ? (backend.minecraftDir || "") : "")
                                    var defaultPath = mineDir ? (mineDir.replace(/\\+$/, "") + "/" + fn) : fn
                                    pendingModDownload = {
                                        slug: modDetailSlug, title: modDetailTitle || modDetailSlug,
                                        versionNumber: vn, loader: loader, gameVersion: d.gameVersion || root.stripLoader(modelData),
                                        url: d.url, filename: fn, size: d.size || 0,
                                        sha1: d.sha1 || "", defaultPath: defaultPath,
                                        displayName: (modDetailTitle || modDetailSlug) + " " + vn
                                    }
                                    modFileDialog.currentFolder = "file:///" + (mineDir || ".").replace(/\\/g, "/")
                                    modFileDialog.currentFile = "file:///" + defaultPath.replace(/\\/g, "/")
                                    modFileDialog.open()
                                }
                    }
                }
            }
            }
            }  // ColumnLayout

            Item { Layout.fillWidth: true; height: 40 }
        }
    }

    // ━━━━━━━━━━━━━━━━━━━━ CONNECTIONS ━━━━━━━━━━━━━━━━━━━━
    Connections {
        target: backend
        enabled: backend !== null
        function onModVersionsPartial(slug, versions, details) {
            if (slug !== root.modDetailSlug) return
            root.modDetailLoading = false
            var arr = []
            var map = {}
            if (versions && versions.length > 0) {
                for (var vi = 0; vi < versions.length; vi++) {
                    var v = versions[vi]
                    var d = details ? details[v] : null
                    var gvs = []
                    if (d) {
                        if (d.game_versions) gvs = d.game_versions
                        else if (d.gameVersions) gvs = d.gameVersions
                    }
                    arr.push(v)
                    // Use C++ game_version for plain MC version; fall back to
                    // stripping loader suffix from composite key (e.g. "1.21.8|fabric")
                    var gameVer = d ? (d.game_version || "") : ""
                    if (!gameVer) {
                        var pipeIdx = v.lastIndexOf("|")
                        gameVer = pipeIdx >= 0 ? v.substring(0, pipeIdx) : v
                    }
                    map[v] = {
                        versionNumber: d ? (d.version_number || v) : v,
                        gameVersion: gameVer,
                        gameVersions: gvs.length > 0 ? gvs : [gameVer],
                        loaders: d ? (d.loaders || []) : [],
                        date: d ? (d.date_published || "") : "",
                        downloads: d ? (d.downloads || 0) : 0,
                        url: d ? (d.url || d.download_url || "") : "",
                        filename: d ? (d.filename || "") : ""
                    }
                }
            }
            root.modDetailRawVersions = arr
            root.modDetailVersionMap = map
            verCountText._displayCount = arr.length
            // ── 存入缓存 ──
            root._versionCache[root.modDetailSlug] = { raw: arr, map: map }
            if (arr.length > 0) root._versionListEnter = true
        }
        function onModVersionsProgress(done, total) {
            if (root.modDetailSlug === "") return
            root.modDetailLoading = done < total
        }
        function onModFileDownloadFailed(dlId, errorDetail, displayName) {
            if (mainWindow) {
                mainWindow.modDlErrorInfo = {dlId: dlId, displayName: displayName, errorDetail: errorDetail}
                mainWindow.showModDlError = true
            }
        }
    }

    // ── Mod download file dialog ──
    FileDialog {
        id: modFileDialog
        fileMode: FileDialog.SaveFile
        title: "保存 Mod 文件"
        nameFilters: ["JAR 文件 (*.jar)", "所有文件 (*.*)"]
        onAccepted: {
            var p = pendingModDownload
            if (!p || !p.url) return
            var savePath = String(selectedFile).replace(/^(file:\/{2,3})/i, "")
            if (!/\.jar$/i.test(savePath)) savePath += ".jar"
            var dlId = backend.downloadModFile(p.url, savePath, p.displayName, p.size || 0, p.sha1 || "")
            if (toastManager) toastManager.show("开始下载 " + p.displayName)
            if (mainWindow) mainWindow.showModDownloadProgress()
            pendingModDownload = {}
        }
        onRejected: { pendingModDownload = {} }
    }
}
