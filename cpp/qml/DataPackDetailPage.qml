// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

// ═══════════════════════════════════════════════════════════════════
// DataPackDetailPage — 数据包详情页（Modrinth + CurseForge 双源）
//
// 结构对齐 ModDetailPage：topBar + DetailInfoCard + 版本分组列表。
// 版本数据复用 modVersionsPartial 信号（fetchModpackVersions 自动路由
// Modrinth slug / CF 数字 id），版本行复用 DetailVersionCard。
// 下载与 Mod 详情页同构：downloadModFile → 全局下载队列 mod:N 卡片，
// 取消/重试/失败弹窗全部走现有通道；文件扩展名 .zip（数据包格式），
// 默认定位到 versions 文件夹（数据包属于版本内资源，由用户选择目标版本）。
// ═══════════════════════════════════════════════════════════════════
Rectangle {
    id: root
    readonly property bool hasBg: backend && typeof backend.customBgPath === "string" && backend.customBgPath.length > 0
    anchors.fill: parent
    color: hasBg ? "transparent" : StyleTokens.bgPrimary

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

    property string dpDetailSlug: ""
    property string dpDetailTitle: ""
    property string dpDetailDesc: ""
    property string dpDetailIcon: ""
    property string dpDetailIconRaw: ""
    property string dpDetailSource: ""
    property int dpDetailDownloads: 0
    property string dpDetailUpdated: ""
    property bool dpDetailLoading: false
    property var dpDetailRawVersions: []
    property var dpDetailVersionMap: ({})
    property var pendingDpDownload: ({})

    // ── 版本列表入场动画 ──
    property bool _versionListEnter: false
    // ── 版本数据缓存 (slug → {rawVersions, versionMap, grouped}) ──
    property var _versionCache: ({})

    // ── 详情页跳转/复制按钮（数据包：Modrinth + 复制，无 CurseForge / MC百科）──
    property var _linkItems: []
    property var _copyItems: []
    function refreshLinks() {
        if (!backend || !dpDetailSlug) { _linkItems = []; _copyItems = []; return }
        var r = backend.resolveProjectLinks(dpDetailTitle, dpDetailSlug, "datapack")
        var links = []
        if (r.mrUrl) links.push({ label: "Modrinth", url: r.mrUrl })
        var name = dpDetailTitle || dpDetailSlug
        var zh = backend.resolveModZh(dpDetailTitle)
        if (zh && zh !== name) name = zh + " " + name
        _linkItems = links
        _copyItems = [
            { label: "复制名称", text: name }
        ]
    }
    onDpDetailTitleChanged: if (backend) refreshLinks()

    signal goBack()

    // ── 司南引擎图标就绪：更新详情页大图 ──
    Connections {
        target: backend
        function onIconReady(url, localPath) {
            if (dpDetailIconRaw && url === dpDetailIconRaw)
                dpDetailIcon = localPath
        }
    }
    function resolveDetailIcon() {
        if (!dpDetailIconRaw || !backend) return ""
        return backend.resolveIconUrl(dpDetailIconRaw)
    }

    // ── Trigger version fetch（复用 fetchModpackVersions：自动路由 MR slug / CF 数字 id）──
    onDpDetailSlugChanged: {
        refreshLinks()
        if (dpDetailSlug && backend) {
            var cached = _versionCache[dpDetailSlug]
            if (cached) {
                dpDetailLoading = false
                dpDetailRawVersions = cached.raw
                dpDetailVersionMap = cached.map
                expandedGroups = []
                showTestVersions = false
                _rebuildGrouped()
            } else {
                dpDetailLoading = true
                dpDetailRawVersions = []
                dpDetailVersionMap = {}
                expandedGroups = []
                showTestVersions = false
                _versionListEnter = false
                _groupedCache = []
                backend.fetchModpackVersions(dpDetailSlug)
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

    // ── 2026-08-23：grouped 改异步分组（防大量版本同步卡主线程）──
    property var grouped: _groupedCache
    property var _groupedCache: []
    property bool _groupedComputing: false

    function _rebuildGrouped() {
        if (_groupedComputing) return
        _groupedComputing = true
        Qt.callLater(function() {
            _groupedComputing = false
            var groups = {}
            var raw = dpDetailRawVersions || []
            var map = dpDetailVersionMap || {}
            for (var i = 0; i < raw.length; i++) {
                var v = raw[i]
                var d = map[v]
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
            _groupedCache = result
            // 版本计数回填（缓存命中路径不走 onModVersionsPartial，需在此更新）
            verCountText._displayCount = raw.length
            if (raw.length > 0) _versionListEnter = true
        })
    }
    property var expandedGroups: []

    function isExpanded(major) { return expandedGroups.indexOf(major) >= 0 }
    function toggleGroup(major) {
        var arr = expandedGroups.slice(); var idx = arr.indexOf(major)
        if (idx >= 0) arr.splice(idx, 1); else arr.push(major)
        expandedGroups = arr
    }
    function getVersionDetail(verStr) {
        var map = dpDetailVersionMap || {}; return map[verStr] || null
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
        height: 44; color: hasBg ? "transparent" : StyleTokens.bgPrimary; z: 10

        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12; spacing: 10

            BackButton {
                id: backBtn
                onClicked: root.goBack()
            }

            Item { Layout.fillWidth: true }

            // Title
            Text {
                text: dpDetailTitle || dpDetailSlug || ""
                font.pixelSize: StyleTokens.fontSizeLg; font.weight: Font.Bold; color: StyleTokens.textSecondary
                Layout.fillWidth: true
                elide: Text.ElideRight; horizontalAlignment: Text.AlignHCenter
            }

            Item { Layout.fillWidth: true }
            Item { width: backBtn.width } // spacer for symmetry
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
                cardIcon: root.dpDetailIcon !== "" ? root.dpDetailIcon : (root.dpDetailIconRaw ? root.resolveDetailIcon() : "")
                cardTitle: root.dpDetailTitle
                cardDesc: root.dpDetailDesc

                // Stats
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 24
                    Text {
                        text: "Slug: " + (dpDetailSlug || "")
                        color: StyleTokens.textSubtle; font.pixelSize: StyleTokens.fontSizeSm
                        elide: Text.ElideRight; Layout.fillWidth: true
                    }
                    Text {
                        id: verCountText
                        property int _displayCount: 0
                        text: qsTr("版本数量: ") + _displayCount
                        color: StyleTokens.textSubtle; font.pixelSize: StyleTokens.fontSizeSm
                        Behavior on _displayCount {
                            NumberAnimation { duration: 2000; easing.type: Easing.OutCubic }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    // 来源标识（Modrinth / CurseForge）
                    // 宽度自适应用 Layout.preferredWidth + Math.max 兜底（Text.implicitWidth
                    // 在字体就绪前可能为 0，直接绑 width 会溢出——与 DownloadCard 同款写法）
                    Rectangle {
                        Layout.preferredWidth: Math.max(48, srcTag.implicitWidth + 14)
                        Layout.preferredHeight: 22
                        Layout.alignment: Qt.AlignVCenter
                        radius: StyleTokens.radiusSm
                        color: dpDetailSource === "CurseForge" ? StyleTokens.warningBg : StyleTokens.bgElevated
                        Text {
                            id: srcTag
                            anchors.centerIn: parent
                            text: dpDetailSource || ""
                            color: dpDetailSource === "CurseForge" ? StyleTokens.brandCurseForge : StyleTokens.accentLight
                            font.pixelSize: StyleTokens.fontSizeXs
                        }
                    }
                    Rectangle {
                        id: testToggleBtn
                        Layout.preferredWidth: Math.max(64, testBtn.implicitWidth + 14)
                        Layout.preferredHeight: 22
                        Layout.alignment: Qt.AlignVCenter
                        radius: StyleTokens.radiusSm
                        color: showTestVersions ? StyleTokens.infoBg : StyleTokens.bgSecondary
                        border.color: (testHov.containsMouse || showTestVersions) ? StyleTokens.accentHover : StyleTokens.borderLight
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
                            color: showTestVersions ? StyleTokens.accentLink : StyleTokens.textMuted
                        }
                        MouseArea {
                            id: testHov; anchors.fill: parent; hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                testToggleBtn._eScale = 0.9
                                testRestoreTimer.restart()
                                showTestVersions = !showTestVersions
                                _rebuildGrouped()
                            }
                        }
                    }

                    // ── 跳转 / 复制按钮（与测试版开关同行）──
                    DetailLinkBar {
                        Layout.fillWidth: true
                        backend: root.backend
                        toastManager: root.toastManager
                        links: root._linkItems
                        copyItems: root._copyItems
                    }
                }
            }

            // ── Loading indicator ──
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: (dpDetailLoading || grouped.length === 0) ? 60 : 0
                visible: dpDetailLoading || grouped.length === 0

                Row {
                    anchors.centerIn: parent; spacing: 8

                    Rectangle {
                        id: spinnerBox
                        width: 24; height: 24; radius: StyleTokens.radiusXl; color: "transparent"
                        visible: dpDetailLoading

                        property real _angle: 0
                        NumberAnimation on _angle {
                            running: dpDetailLoading
                            from: 0; to: 360; duration: 1000; loops: Animation.Infinite
                        }
                        on_AngleChanged: spinCanvas.requestPaint()

                        Canvas {
                            id: spinCanvas
                            anchors.fill: parent; visible: dpDetailLoading
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.clearRect(0, 0, width, height)
                                var cx = width / 2, cy = height / 2
                                var r = Math.min(cx, cy) - 3
                                if (r <= 0) return
                                var startRad = (spinnerBox._angle - 90) * Math.PI / 180
                                var endRad = (spinnerBox._angle + 180) * Math.PI / 180
                                ctx.strokeStyle = StyleTokens.accentVivid
                                ctx.lineWidth = 2; ctx.lineCap = "round"
                                ctx.beginPath()
                                ctx.arc(cx, cy, r, startRad, endRad)
                                ctx.stroke()
                            }
                        }
                    }
                    Text {
                        text: dpDetailLoading ? "加载版本中..."
                            : (grouped.length === 0 ? "无可用版本" : "")
                        color: StyleTokens.textMuted; font.pixelSize: StyleTokens.fontSizeSm
                    }
                }
            }

            // ── Section: Version List ──
            Text {
                visible: _versionListEnter
                opacity: _versionListEnter ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
                text: qsTr("版本列表")
                font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.DemiBold; color: StyleTokens.textTertiary
                Layout.topMargin: 8; Layout.leftMargin: 4
            }

            // ── Version groups ──
            ColumnLayout {
                id: versionGroupsLayout
                Layout.fillWidth: true
                spacing: 8
                visible: _versionListEnter

                Repeater {
                    model: !dpDetailLoading ? grouped : []
                    delegate: ExpandableGroupCard {
                        id: groupCard
                        Layout.fillWidth: true
                        title: "MC " + modelData.major
                        subtitle: modelData.versions.length + " 个版本"
                        expanded: isExpanded(modelData.major)
                        onToggled: toggleGroup(modelData.major)

                        property int _visibleCount: 30
                        readonly property int _totalCount: modelData.versions.length
                        readonly property bool _hasMore: _visibleCount < _totalCount

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
                            model: groupCard.expanded ? modelData.versions.slice(0, groupCard._visibleCount) : []
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
                                    var gvClean = d ? (d.gameVersion || "") : ""
                                    var pr = preReleaseTag(gvClean || root.stripLoader(modelData))
                                    if (pr) result.push({text: pr, color: StyleTokens.warning, bg: StyleTokens.warningBg})
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
                                    var vn = d.versionNumber || modelData
                                    var safeTitle = (dpDetailTitle || dpDetailSlug || "datapack").replace(/[\\\/:*?"<>|]/g, "_").replace(/\s+/g, "_")
                                    var fn = safeTitle + "-" + vn + ".zip"
                                    var mineDir = String(backend ? (backend.minecraftDir || "") : "")
                                    var defaultPath = mineDir ? (mineDir.replace(/\\+$/, "") + "/" + fn) : fn
                                    pendingDpDownload = {
                                        slug: dpDetailSlug, title: dpDetailTitle || dpDetailSlug,
                                        versionNumber: vn, gameVersion: d.gameVersion || root.stripLoader(modelData),
                                        url: d.url, filename: fn, size: d.size || 0,
                                        sha1: d.sha1 || "", defaultPath: defaultPath,
                                        displayName: (dpDetailTitle || dpDetailSlug) + " " + vn
                                    }
                                    // 默认定位到 versions 文件夹（数据包属于版本内资源，选择安装到的游戏版本）
                                    var versionsFolder = backend ? backend.gameDir + "/versions" : "."
                                    dpFileDialog.currentFolder = "file:///" + versionsFolder.replace(/\\/g, "/")
                                    dpFileDialog.currentFile = "file:///" + defaultPath.replace(/\\/g, "/")
                                    dpFileDialog.open()
                                }
                            }
                        }

                        // ── 懒渲染：还有 X 个版本 ──
                        Rectangle {
                            visible: groupCard.expanded && groupCard._hasMore
                            width: parent.width - 48
                            x: 24
                            height: 30
                            radius: StyleTokens.radiusMd
                            color: moreHov.containsMouse ? StyleTokens.accentHover : "transparent"
                            border.color: StyleTokens.infoBg
                            border.width: 1
                            Behavior on color { ColorAnimation { duration: 150 } }
                            Text {
                                anchors.centerIn: parent
                                text: "还有 " + (groupCard._totalCount - groupCard._visibleCount) + " 个版本"
                                color: moreHov.containsMouse ? StyleTokens.accentLink : StyleTokens.textMuted
                                font.pixelSize: StyleTokens.fontSizeSm
                            }
                            MouseArea {
                                id: moreHov
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: { groupCard._visibleCount += 50 }
                            }
                        }
                    }
                }
            }

            Item { Layout.fillWidth: true; height: 40 }
        }
    }

    // ━━━━━━━━━━━━━━━━━━━━ CONNECTIONS ━━━━━━━━━━━━━━━━━━━━
    Connections {
        target: backend
        enabled: backend !== null
        function onModVersionsPartial(slug, versions, details) {
            if (slug !== root.dpDetailSlug) return
            root.dpDetailLoading = false
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
                        filename: d ? (d.filename || "") : "",
                        size: d ? (d.size || 0) : 0,
                        sha1: d ? (d.sha1 || "") : ""
                    }
                }
            }
            root.dpDetailRawVersions = arr
            root.dpDetailVersionMap = map
            verCountText._displayCount = arr.length
            root._versionCache[root.dpDetailSlug] = { raw: arr, map: map }
            root._rebuildGrouped()
        }
        function onModVersionsProgress(done, total) {
            if (root.dpDetailSlug === "") return
            root.dpDetailLoading = done < total
        }
        function onModFileDownloadFailed(dlId, errorDetail, displayName) {
            if (mainWindow) {
                mainWindow.modDlErrorInfo = {dlId: dlId, displayName: displayName, errorDetail: errorDetail}
                mainWindow.showModDlError = true
            }
        }
    }

    // ── Data Pack download file dialog ──
    FileDialog {
        id: dpFileDialog
        fileMode: FileDialog.SaveFile
        title: "保存数据包文件"
        nameFilters: ["ZIP 文件 (*.zip)", "所有文件 (*.*)"]
        onAccepted: {
            var p = pendingDpDownload
            if (!p || !p.url) return
            var savePath = String(selectedFile).replace(/^(file:\/{2,3})/i, "")
            if (!/\.zip$/i.test(savePath)) savePath += ".zip"
            var dlId = backend.downloadModFile(p.url, savePath, p.displayName, p.size || 0, p.sha1 || "")
            if (toastManager) toastManager.show("开始下载 " + p.displayName)
            if (mainWindow) mainWindow.showModDownloadProgress()
            pendingDpDownload = {}
        }
        onRejected: { pendingDpDownload = {} }
    }
}
