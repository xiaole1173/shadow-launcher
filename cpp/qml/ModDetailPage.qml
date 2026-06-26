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

    // Entrance fade-in (self-contained, no interference with parent overlay)
    opacity: 0
    Component.onCompleted: opacity = 1
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

    signal goBack()

    // ── Trigger version fetch ──
    onModDetailSlugChanged: {
        if (modDetailSlug && backend) {
            modDetailLoading = true
            modDetailRawVersions = []
            modDetailVersionMap = {}
            expandedGroups = []
            showTestVersions = false
            backend.fetchModVersions([modDetailSlug])
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
            var gvs = d ? (d.gameVersions || []) : []
            if (gvs.length === 0) gvs = [v]
            var first = gvs[0] || v
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
        if (n >= 1000000) return (n / 1000000).toFixed(1) + "M"
        if (n >= 1000) return (n / 1000).toFixed(1) + "K"
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
                width: backLabel.implicitWidth + 20; height: 30; radius: 6
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
                    text: "\u2190 \u8fd4\u56de"; font.pixelSize: 13; font.weight: Font.Medium
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

            // Title
            Text {
                text: modDetailTitle || modDetailSlug || ""
                font.pixelSize: 16; font.weight: Font.Bold; color: "#e4e8f2"
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
                        color: "#7888a8"; font.pixelSize: 11
                        elide: Text.ElideRight; Layout.fillWidth: true
                    }
                    Text {
                        id: verCountText
                        property int _displayCount: 0
                        text: qsTr("版本数量: ") + _displayCount
                        color: "#7888a8"; font.pixelSize: 11
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
                        color: "#7888a8"; font.pixelSize: 11
                    }
                    Rectangle {
                        id: testToggleBtn
                        width: testBtn.implicitWidth + 14; height: 22; radius: 4
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
                            id: testBtn; anchors.centerIn: parent; font.pixelSize: 10
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

            // ── Loading indicator ──
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: (modDetailLoading || grouped.length === 0) ? 60 : 0
                visible: modDetailLoading || grouped.length === 0

                Row {
                    anchors.centerIn: parent; spacing: 8

                    Rectangle {
                        id: spinnerBox
                        width: 24; height: 24; radius: 12; color: "transparent"
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
                        color: "#606478"; font.pixelSize: 12
                    }
                }
            }

            // ── Section: Version List ──
            Text {
                visible: !modDetailLoading && grouped.length > 0
                text: qsTr("版本列表")
                font.pixelSize: 14; font.weight: Font.DemiBold; color: "#a0a8c0"
                Layout.topMargin: 8; Layout.leftMargin: 4
            }

            // ── Version groups ──
            Repeater {
                model: !modDetailLoading ? grouped : []
                delegate: Column {
                    Layout.fillWidth: true; spacing: 6

                    // Group header card (same style as version cards)
                    Rectangle {
                        id: grpHeader
                        width: parent.width; height: 40; radius: 8
                        color: grpHover.containsMouse ? "#161a26" : "#11141c"
                        border.color: grpHover.containsMouse ? "#3a5ed0" : "#1e2230"
                        border.width: grpHover.containsMouse ? 1.5 : 1

                        property real _eScale: 1.0
                        transform: Scale {
                            origin.x: grpHeader.width / 2
                            origin.y: grpHeader.height / 2
                            xScale: grpHeader._eScale; yScale: grpHeader._eScale
                        }
                        Timer { id: grpRestoreTimer; interval: 100
                            onTriggered: { grpHeader._eScale = 1.0 }
                        }
                        Behavior on _eScale {
                            SpringAnimation { spring: 1.8; damping: 0.25; epsilon: 0.01 }
                        }
                        Behavior on color { ColorAnimation { duration: 200 } }
                        Behavior on border.color { ColorAnimation { duration: 200 } }
                        Behavior on border.width { NumberAnimation { duration: 150 } }

                        Rectangle {
                            anchors.fill: parent; radius: parent.radius
                            opacity: grpHover.containsMouse ? 0.12 : 0
                            gradient: Gradient {
                                GradientStop { position: 0; color: "#5068c8" }
                                GradientStop { position: 1; color: "#6080d8" }
                            }
                            Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
                        }

                        RowLayout {
                            anchors.fill: parent; anchors.margins: 10; spacing: 10
                            Text {
                                text: (isExpanded(modelData.major) ? "\u25bc" : "\u25b8") + "  MC " + modelData.major
                                color: "#6080d8"; font.pixelSize: 14; font.weight: Font.Bold
                            }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: modelData.versions.length + " 个版本"
                                color: "#505468"; font.pixelSize: 10
                            }
                        }
                        MouseArea {
                            id: grpHover; anchors.fill: parent
                            hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                grpHeader._eScale = 0.94
                                grpRestoreTimer.restart()
                                toggleGroup(modelData.major)
                            }
                        }
                    }

                    // Sub-versions
                    Repeater {
                        model: isExpanded(modelData.major) ? modelData.versions : []
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
                                            result.push({text: d.loaders[li], color: "#8aaeff", bg: "#1a2848"})
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
                                    var loaders = d.loaders || []
                                    var loader = loaders.length > 0 ? loaders[0] : ""
                                    var vn = d.versionNumber || modelData
                                    var safeTitle = (modDetailTitle || modDetailSlug || "mod").replace(/[\\\/:*?"<>|]/g, "_").replace(/\s+/g, "_")
                                    var fn = safeTitle + "-" + vn + (loader ? "-" + loader : "") + ".jar"
                                    var mineDir = String(backend ? (backend.minecraftDir || "") : "")
                                    var defaultPath = mineDir ? (mineDir.replace(/\\+$/, "") + "/" + fn) : fn
                                    pendingModDownload = {
                                        slug: modDetailSlug, title: modDetailTitle || modDetailSlug,
                                        versionNumber: vn, loader: loader, gameVersion: modelData,
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

            Item { Layout.fillWidth: true; height: 40 }
        }
    }

    // ── Version count animation trigger ──
    onModDetailLoadingChanged: {
        if (!modDetailLoading) {
            verCountText._displayCount = (modDetailRawVersions || []).length
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
                    map[v] = {
                        versionNumber: d ? (d.version_number || v) : v,
                        gameVersions: gvs.length > 0 ? gvs : [v],
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
