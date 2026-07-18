// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

// ShaderDetailPage
// Full-screen detail page for a Shader project's version list
// Architecture: InstallPage-style — fixed top bar + Flickable + cards

Rectangle {
    id: root
    readonly property bool hasBg: backend && typeof backend.customBgPath === "string" && backend.customBgPath.length > 0
    anchors.fill: parent
    color: hasBg ? "transparent" : "#0c0f16"

    opacity: 0
    Component.onCompleted: opacity = 1
    Behavior on opacity { NumberAnimation { duration: 350; easing.type: Easing.OutCubic } }

    // ── Injected properties ──
    property var backend: null
    property var toastManager: null
    property var mainWindow: null

    property string shaderDetailSlug: ""
    property string shaderDetailTitle: ""
    property string shaderDetailDesc: ""
    property string shaderDetailIcon: ""
    property bool shaderDetailLoading: false
    property var shaderDetailRawVersions: []
    property var shaderDetailVersionMap: ({})
    property var pendingShaderDownload: ({})

    signal goBack()

    // ── Trigger version fetch ──
    onShaderDetailSlugChanged: {
        if (shaderDetailSlug && backend) {
            shaderDetailLoading = true
            shaderDetailRawVersions = []
            shaderDetailVersionMap = {}
            expandedGroups = []
            backend.fetchShaderVersions([shaderDetailSlug])
        }
    }

    // ── Helpers ──
    function stripSuffix(v) {
        var re = /-(?:snapshot|pre|rc|alpha|beta)[\d.\-]*$/i
        var m = v.match(re)
        return m ? v.slice(0, m.index) : v
    }
    function preReleaseTag(v) {
        if (/-snapshot/i.test(v)) return "快照版"
        if (/-pre/i.test(v)) return "预览版"
        if (/-rc/i.test(v)) return "发布候选版"
        return ""
    }
    function formatDate(isoStr) {
        if (!isoStr) return "-"
        return isoStr.slice(0, 10)
    }
    function formatDL(n) {
        if (n >= 100000000) return (n / 100000000).toFixed(1) + "亿"
        if (n >= 10000) return (n / 10000).toFixed(0) + "万"
        return String(n || 0)
    }
    function capLoader(s) {
        if (!s) return s
        var m = { "iris": "Iris", "optifine": "OptiFine" }
        var lc = s.toLowerCase()
        if (m[lc]) return m[lc]
        return s.charAt(0).toUpperCase() + s.slice(1)
    }

    // ── Group versions by MC major.minor ──
    property var grouped: {
        var groups = {}
        var raw = shaderDetailRawVersions || []
        var map = shaderDetailVersionMap || {}
        for (var i = 0; i < raw.length; i++) {
            var v = raw[i]
            var d = map[v]
            var gvs = d ? (d.gameVersions || []) : []
            if (gvs.length === 0) gvs = [v]
            var base = stripSuffix(gvs[0] || v)
            var parts = base.split(".")
            var major = parts.length >= 2 ? parts[0] + "." + parts[1] : base
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
        var map = shaderDetailVersionMap || {}; return map[verStr] || null
    }

    // ━━━━━━━━━━━━━━━━━━━━ TOP BAR ━━━━━━━━━━━━━━━━━━━━
    Rectangle {
        id: topBar
        anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
        height: 44; color: hasBg ? "transparent" : "#0c0f16"; z: 10

        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12; spacing: 10

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
                        root.goBack()
                    }
                }
            }

            Item { Layout.fillWidth: true }

            Text {
                text: shaderDetailTitle || shaderDetailSlug || ""
                font.pixelSize: StyleTokens.fontSizeLg; font.weight: Font.Bold; color: StyleTokens.textSecondary
                Layout.fillWidth: true
                elide: Text.ElideRight; horizontalAlignment: Text.AlignHCenter
            }

            Item { Layout.fillWidth: true }
            Item { width: backLabel.implicitWidth + 20 }
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
                cardIcon: root.shaderDetailIcon
                cardTitle: root.shaderDetailTitle
                cardDesc: root.shaderDetailDesc

                RowLayout {
                    Layout.fillWidth: true; spacing: 24
                    Text {
                        text: qsTr("来源: Modrinth (MCIM 镜像)")
                        color: "#7888a8"; font.pixelSize: StyleTokens.fontSizeSm
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
            }

            // ── Loading ──
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: (shaderDetailLoading || grouped.length === 0) ? 60 : 0
                visible: shaderDetailLoading || grouped.length === 0

                Row {
                    anchors.centerIn: parent; spacing: 8

                    Rectangle {
                        id: spinnerBox
                        width: 24; height: 24; radius: StyleTokens.radiusXl; color: "transparent"
                        visible: shaderDetailLoading

                        property real _angle: 0
                        NumberAnimation on _angle {
                            running: shaderDetailLoading
                            from: 0; to: 360; duration: 1000; loops: Animation.Infinite
                        }
                        on_AngleChanged: spinCanvas.requestPaint()

                        Canvas {
                            id: spinCanvas
                            anchors.fill: parent; visible: shaderDetailLoading
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
                        text: shaderDetailLoading ? "加载版本中..." : (grouped.length === 0 ? "无可用版本" : "")
                        color: "#606478"; font.pixelSize: StyleTokens.fontSizeSm
                    }
                }
            }

            // ── Version List Section ──
            Text {
                visible: !shaderDetailLoading && grouped.length > 0
                text: qsTr("版本列表")
                font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.DemiBold; color: "#a0a8c0"
                Layout.topMargin: 8; Layout.leftMargin: 4
            }

            // ── Version groups ──
            Repeater {
                model: !shaderDetailLoading ? grouped : []
                delegate: ExpandableGroupCard {
                    Layout.fillWidth: true
                    title: "MC " + modelData.major
                    subtitle: modelData.versions.length + " 个版本"
                    expanded: isExpanded(modelData.major)
                    onToggled: toggleGroup(modelData.major)

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
                                        result.push({text: root.capLoader(d.loaders[li]), color: StyleTokens.accentLink, bg: "#1a2848"})
                                    }
                                }
                                var pr = preReleaseTag(modelData)
                                if (pr) result.push({text: pr, color: "#d0a050", bg: "#382818"})
                                return result
                            }

                            infoLines: {
                                var d = getVersionDetail(modelData)
                                return [
                                    { label: "MC:", value: d ? (d.gameVersions || [modelData]).join(", ") : modelData },
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
                                var safeTitle = (shaderDetailTitle || shaderDetailSlug || "shader").replace(/[\\\/:*?"<>|]/g, "_").replace(/\s+/g, "_")
                                var ext = (d.url && d.url.match(/\.(zip|jar)$/i)) ? d.url.match(/\.(zip|jar)$/i)[1] : "zip"
                                var fn = safeTitle + "-" + vn + "." + ext
                                var mineDir = String(backend ? (backend.minecraftDir || "") : "")
                                var defaultPath = mineDir ? (mineDir.replace(/\\+$/, "") + "/" + fn) : fn
                                pendingShaderDownload = {
                                    slug: shaderDetailSlug, title: shaderDetailTitle || shaderDetailSlug,
                                    versionNumber: vn, url: d.url, filename: fn,
                                    size: d.size || 0, sha1: d.sha1 || "",
                                    defaultPath: defaultPath,
                                    displayName: (shaderDetailTitle || shaderDetailSlug) + " " + vn
                                }
                                shaderFileDialog.currentFolder = "file:///" + (mineDir || ".").replace(/\\/g, "/")
                                shaderFileDialog.currentFile = "file:///" + defaultPath.replace(/\\/g, "/")
                                shaderFileDialog.open()
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
        function onShaderVersionsPartial(slug, versions, details) {
            if (slug !== root.shaderDetailSlug) return
            root.shaderDetailLoading = false
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
                    map[v] = {
                        versionNumber: d ? (d.version_number || v) : v,
                        gameVersions: gvs.length > 0 ? gvs : [v],
                        loaders: (d ? (d.loaders || []) : []).map(function(l) { return root.capLoader(l) }),
                        date: d ? (d.date_published || "") : "",
                        downloads: d ? (d.downloads || 0) : 0,
                        url: d ? (d.url || d.download_url || "") : "",
                        filename: d ? (d.filename || "") : ""
                    }
                }
            }
            root.shaderDetailRawVersions = arr
            root.shaderDetailVersionMap = map
            verCountText._displayCount = arr.length
        }
        function onShaderVersionsProgress(done, total) {
            if (root.shaderDetailSlug === "") return
            root.shaderDetailLoading = done < total
        }
        function onShaderFileDownloadFailed(dlId, errorDetail, displayName) {
            if (mainWindow) {
                mainWindow.shaderDlErrorInfo = {dlId: dlId, displayName: displayName, errorDetail: errorDetail}
                mainWindow.showShaderDlError = true
            }
        }
    }

    // ── File dialog ──
    FileDialog {
        id: shaderFileDialog
        fileMode: FileDialog.SaveFile
        title: "保存光影文件"
        nameFilters: ["光影文件 (*.zip *.jar)", "所有文件 (*.*)"]
        onAccepted: {
            var p = pendingShaderDownload
            if (!p || !p.url) return
            var savePath = String(selectedFile).replace(/^(file:\/{2,3})/i, "")
            var dlId = backend.downloadModFile(p.url, savePath, p.displayName, p.size || 0, p.sha1 || "")
            if (toastManager) toastManager.show("开始下载 " + p.displayName)
            if (mainWindow) mainWindow.showModDownloadProgress()
            pendingShaderDownload = {}
        }
        onRejected: { pendingShaderDownload = {} }
    }
}
