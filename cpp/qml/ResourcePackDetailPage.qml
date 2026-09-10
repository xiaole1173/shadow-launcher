// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

// ResourcePackDetailPage
// Full-screen detail page for a Resource Pack project's version list
// Architecture: InstallPage-style — fixed top bar + Flickable + cards
// 版本卡片与 Mod/DataPack 等 Tab 统一：无 L3 展开（曾独占 showExpand 机制，已移除）

Rectangle {
    id: root
    readonly property bool hasBg: backend && typeof backend.customBgPath === "string" && backend.customBgPath.length > 0
    anchors.fill: parent
    color: hasBg ? "transparent" : StyleTokens.bgPrimary

    // Entrance fade-in (self-contained, no interference with parent overlay)
    opacity: 0
    Component.onCompleted: opacity = 1
    Behavior on opacity { NumberAnimation { duration: 350; easing.type: Easing.OutCubic } }

    // ── Injected properties ──
    property var backend: null
    property var toastManager: null
    property var mainWindow: null

    property string rpDetailSlug: ""
    property string rpDetailTitle: ""
    property string rpDetailIconUrl: ""
    property string rpDetailIconRaw: ""
    property string rpDetailAuthor: ""
    property string rpDetailDesc: ""
    property int rpDetailDownloads: 0
    property string rpDetailUpdated: ""
    property bool rpDetailLoading: false
    property var pendingRpDownload: ({})
    property var rpVersionCache: ({})
    property int rpVersionCacheVersion: 0
    property var rpVersionDetailCache: ({})

    // ── 详情页跳转/复制按钮（统一胶囊）──
    property var _linkItems: []
    property var _copyItems: []
    function refreshLinks() {
        if (!backend || !rpDetailSlug) { _linkItems = []; _copyItems = []; return }
        var r = backend.resolveProjectLinks(rpDetailTitle, rpDetailSlug, "resourcepack")
        var links = []
        if (r.mrUrl) links.push({ label: "Modrinth", url: r.mrUrl })
        if (r.cfUrl) links.push({ label: "CurseForge", url: r.cfUrl })
        var name = rpDetailTitle || rpDetailSlug
        var zh = backend.resolveModZh(rpDetailTitle)
        if (zh && zh !== name) name = zh + " " + name
        _linkItems = links
        _copyItems = [
            { label: "复制名称", text: name }
        ]
    }
    onRpDetailTitleChanged: if (backend) refreshLinks()

    signal goBack()

    // ── Trigger fetch ──
    onRpDetailSlugChanged: {
        refreshLinks()
        if (rpDetailSlug && backend) {
            rpDetailLoading = true
            rpVersionCache = ({})
            rpVersionDetailCache = ({})
            rpVersionCacheVersion = 0
            expandedGroups = []
            _rpGroupedCache = []
            _versionListEnter = false
            // CurseForge 资源包（slug 为纯数字 modId）
            if (/^\d+$/.test(rpDetailSlug)) {
                backend.fetchResourcepackVersionsCf(rpDetailSlug)
            } else {
                backend.fetchResourcepackVersions([rpDetailSlug])
            }
        }
    }

    // ── 2026-08-23：rpDetailGrouped 改异步分组（对齐 ModDetailPage，防大量版本同步卡主线程）──
    property var rpDetailGrouped: _rpGroupedCache
    property var _rpGroupedCache: []
    property bool _rpGroupedComputing: false
    property bool _versionListEnter: false

    function _rebuildRpGrouped() {
        if (_rpGroupedComputing) return
        _rpGroupedComputing = true
        Qt.callLater(function() {
            _rpGroupedComputing = false
            var d = rpVersionCache
            var raw = (d && d[rpDetailSlug]) ? d[rpDetailSlug] : []
            var groups = {}
            for (var i = 0; i < raw.length; i++) {
                var v = raw[i]
                var segs = v.split(".")
                var major = segs.length >= 2 ? segs[0] + "." + segs[1] : v
                if (!groups[major]) groups[major] = []
                groups[major].push(v)
            }
            var result = []
            for (var k in groups) { result.push({major: k, versions: groups[k]}) }
            result.sort(function(a,b) {
                var as = a.major.split("."), bs = b.major.split(".")
                var aM = parseInt(as[0])||0, bM = parseInt(bs[0])||0
                if (aM !== bM) return bM - aM
                return (parseInt(bs[1])||0) - (parseInt(as[1])||0)
            })
            _rpGroupedCache = result
            if (raw.length > 0) _versionListEnter = true
        })
    }
    property var expandedGroups: []

    function isGroupExpanded(major) {
        if (typeof expandedGroups === 'string') return expandedGroups === major
        return expandedGroups.indexOf(major) >= 0
    }
    function toggleGroupExpanded(major) {
        var arr = (typeof expandedGroups === 'string') ? [expandedGroups] : expandedGroups.slice()
        var idx = arr.indexOf(major)
        if (idx >= 0) arr.splice(idx, 1); else arr.push(major)
        expandedGroups = arr
    }
    function getVerDetail(gameVer) {
        var cache = rpVersionDetailCache[rpDetailSlug]
        if (!cache) return null
        return cache[gameVer] || null
    }
    function formatDate(isoStr) {
        if (!isoStr) return "-"
        return isoStr.slice(0, 10)
    }
    function formatDownloads(n) {
        if (!n && n !== 0) return "0"
        if (n >= 100000000) return (n / 100000000).toFixed(1) + "亿"
        if (n >= 10000) return (n / 10000).toFixed(0) + "万"
        return String(n)
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
            Text {
                text: rpDetailTitle || rpDetailSlug || ""
                font.pixelSize: StyleTokens.fontSizeLg; font.weight: Font.Bold; color: StyleTokens.textSecondary
                Layout.fillWidth: true; elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
            }
            Item { Layout.fillWidth: true }
            Item { width: backBtn.width }
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
                cardIcon: root.rpDetailIconUrl !== "" ? root.rpDetailIconUrl : (root.rpDetailIconRaw ? root.resolveRpDetailIcon() : "")
                cardTitle: root.rpDetailTitle
                cardDesc: root.rpDetailDesc

                // Stats row
                RowLayout {
                    Layout.fillWidth: true; spacing: 24
                    Text {
                        text: qsTr("作者: ") + (rpDetailAuthor || "未知")
                        color: StyleTokens.textSubtle; font.pixelSize: StyleTokens.fontSizeSm
                    }
                    Text {
                        text: qsTr("下载量: ") + formatDownloads(rpDetailDownloads) + " 次"
                        color: StyleTokens.textSubtle; font.pixelSize: StyleTokens.fontSizeSm
                    }
                    Text {
                        text: qsTr("更新于: ") + formatDate(rpDetailUpdated)
                        color: StyleTokens.textSubtle; font.pixelSize: StyleTokens.fontSizeSm
                    }
                }

                // 跳转 / 复制按钮（DetailLinkBar 纯文本胶囊，统一风格）
                DetailLinkBar {
                    Layout.fillWidth: true
                    Layout.topMargin: 2
                    backend: root.backend
                    toastManager: root.toastManager
                    links: root._linkItems
                    copyItems: root._copyItems
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

            // ── Loading indicator ──
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: (rpDetailLoading || rpDetailGrouped.length === 0) ? 60 : 0
                visible: rpDetailLoading || rpDetailGrouped.length === 0

                Row {
                    anchors.centerIn: parent; spacing: 8

                    Rectangle {
                        width: 24; height: 24; radius: StyleTokens.radiusXl; color: "transparent"
                        visible: rpDetailLoading
                        property real _angle: 0
                        NumberAnimation on _angle {
                            running: rpDetailLoading
                            from: 0; to: 360; duration: 1000; loops: Animation.Infinite
                        }
                        on_AngleChanged: rpSpinCanvas.requestPaint()
                        Canvas {
                            id: rpSpinCanvas
                            anchors.fill: parent; visible: rpDetailLoading
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.clearRect(0, 0, width, height)
                                var cx = width / 2, cy = height / 2
                                var r = Math.min(cx, cy) - 3
                                if (r <= 0) return
                                var startRad = (parent._angle - 90) * Math.PI / 180
                                var endRad = (parent._angle + 180) * Math.PI / 180
                                ctx.strokeStyle = StyleTokens.accentVivid
                                ctx.lineWidth = 2; ctx.lineCap = "round"
                                ctx.beginPath()
                                ctx.arc(cx, cy, r, startRad, endRad)
                                ctx.stroke()
                            }
                        }
                    }
                    Text {
                        text: rpDetailLoading ? "加载版本中..."
                            : (rpDetailGrouped.length === 0 ? "无可用版本" : "")
                        color: StyleTokens.textMuted; font.pixelSize: StyleTokens.fontSizeSm
                    }
                }
            }

            // ── Version groups（懒渲染：每组默认前 30 个版本，点按钮追加 50）──
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8
                visible: _versionListEnter

                Repeater {
                    model: !rpDetailLoading ? rpDetailGrouped : []
                    delegate: ExpandableGroupCard {
                        id: groupCard
                        Layout.fillWidth: true
                        title: "MC " + modelData.major
                        subtitle: modelData.versions.length + " 个版本"
                        expanded: isGroupExpanded(modelData.major)
                        onToggled: toggleGroupExpanded(modelData.major)

                        property int _visibleCount: 30
                        readonly property int _totalCount: modelData.versions.length
                        readonly property bool _hasMore: _visibleCount < _totalCount

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
                                    var d = getVerDetail(modelData)
                                    return d ? (d.version_number || modelData) : modelData
                                }

                                tags: {
                                    // RP versions don't have loader tags
                                    return []
                                }

                                infoLines: {
                                    var d = getVerDetail(modelData)
                                    var lines = []
                                    lines.push({ label: "MC:", value: modelData })
                                    lines.push({ label: "", value: d ? (d.name || "") : "" })
                                    lines.push({ label: "", value: (d && d.filename ? "文件: " + d.filename : "") + (d ? "  |  下载量 " + formatDownloads(d.downloads || 0) : "") })
                                    return lines
                                }

                                hasDownload: true
                                onDownloadClicked: {
                                    var d = getVerDetail(modelData)
                                    if (!d) { if (toastManager) toastManager.show("无法获取版本信息"); return }
                                    var url = d.download_url || d.url || ""
                                    if (!url) { if (toastManager) toastManager.show("无法获取下载地址"); return }
                                    var safeTitle = (rpDetailTitle || rpDetailSlug || "rp").replace(/[\\\/:*?"<>|]/g, "_").replace(/\s+/g, "_")
                                    var vn = d.version_number || modelData
                                    var fn = safeTitle + "-" + vn + ".zip"
                                    var mineDir = String(backend ? (backend.minecraftDir || "") : "")
                                    var defaultPath = mineDir ? (mineDir.replace(/\\+$/, "") + "/resourcepacks/" + fn) : fn
                                    pendingRpDownload = {
                                        slug: rpDetailSlug, title: rpDetailTitle || rpDetailSlug,
                                        versionNumber: vn, gameVersion: modelData,
                                        url: url, filename: fn,
                                        size: d.size || 0, sha1: d.sha1 || "",
                                        defaultPath: defaultPath,
                                        displayName: (rpDetailTitle || rpDetailSlug) + " " + vn
                                    }
                                    // 默认定位到 versions 文件夹，方便用户选择安装到的游戏版本
                                    var versionsFolder = backend ? backend.gameDir + "/versions" : "."
                                    rpFileDialog.currentFolder = "file:///" + versionsFolder.replace(/\\/g, "/")
                                    rpFileDialog.currentFile = "file:///" + defaultPath.replace(/\\/g, "/")
                                    rpFileDialog.open()
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
    // 司南引擎图标就绪：更新详情页大图
    Connections {
        target: backend
        enabled: backend !== null
        function onIconReady(url, localPath) {
            if (root.rpDetailIconRaw && url === root.rpDetailIconRaw)
                root.rpDetailIconUrl = localPath
        }
    }
    function resolveRpDetailIcon() {
        if (!root.rpDetailIconRaw || !backend) return ""
        return backend.resolveRpIconUrl(root.rpDetailIconRaw)
    }

    Connections {
        target: backend
        enabled: backend !== null
        function onResourcepackVersionsPartial(slug, versions, details) {
            if (slug !== root.rpDetailSlug) return
            root.rpDetailLoading = false
            var newVerCache = Object.assign({}, root.rpVersionCache)
            newVerCache[slug] = versions
            root.rpVersionCache = newVerCache
            if (details) {
                var newDetailCache = Object.assign({}, root.rpVersionDetailCache)
                newDetailCache[slug] = details
                root.rpVersionDetailCache = newDetailCache
            }
            root.rpVersionCacheVersion++
            root._rebuildRpGrouped()
        }
        function onResourcepackVersionsProgress(done, total) {
            if (root.rpDetailSlug === "") return
            root.rpDetailLoading = done < total
        }
        function onResourceDownloadProgress(completed, total, fileName) {
            if (pendingRpDownload.displayName) {
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
                if (toastManager) toastManager.show("下载中 " + pendingRpDownload.displayName + ": " + pct + "%" + (speedStr ? " (" + speedStr + ")" : ""))
            }
        }
    }

    // ── RP download file dialog ──
    FileDialog {
        id: rpFileDialog
        fileMode: FileDialog.SaveFile
        title: "保存资源包文件"
        nameFilters: ["ZIP 文件 (*.zip)", "所有文件 (*.*)"]
        onAccepted: {
            var p = pendingRpDownload
            if (!p || !p.url) return
            var savePath = String(selectedFile).replace(/^(file:\/{2,3})/i, "")
            if (!/\.zip$/i.test(savePath)) savePath += ".zip"
            var dlId = backend.downloadModFile(p.url, savePath, p.displayName, p.size || 0, p.sha1 || "")
            if (toastManager) toastManager.show("开始下载 " + p.displayName)
            if (mainWindow) mainWindow.showModDownloadProgress()
            pendingRpDownload = {}
        }
        onRejected: { pendingRpDownload = {} }
    }
}