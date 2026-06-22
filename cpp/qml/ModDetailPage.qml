import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

// ModDetailPage
// Full-screen overlay for Mod version detail
// Same pattern as InstallPage: self-contained, signals to parent

Rectangle {
    id: root
    anchors.fill: parent
    color: "#0c0f16"

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

    // Slide in from right
    x: 0
    opacity: 1
    property bool _entered: false

    Behavior on x {
        NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
    }
    Behavior on opacity {
        NumberAnimation { duration: 250; easing.type: Easing.OutCubic }
    }

    Component.onCompleted: {
        x = parent.width
        opacity = 1
        slideInTimer.start()
    }

    Timer { id: slideInTimer; interval: 16; onTriggered: { root.x = 0; root._entered = true } }

    // Slide out and close
    function animateOut() {
        root.opacity = 0
        root.x = parent.width
        slideOutTimer.start()
    }

    Timer {
        id: slideOutTimer
        interval: 320
        onTriggered: {
            root.goBack()
        }
    }

    // Trigger version fetch when slug arrives
    onModDetailSlugChanged: {
        if (modDetailSlug && backend && _entered) {
            modDetailLoading = true
            modDetailRawVersions = []
            modDetailVersionMap = {}
            expandedGroups = []
            selectedVersion = ""
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
    property string selectedVersion: ""

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

    // ── UI ──

    // Click interceptor background
    MouseArea {
        anchors.fill: parent
        onClicked: console.log("[mod-detail] background click eaten")
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        // Back button
        Rectangle {
            id: backBtn
            Layout.preferredHeight: 30
            Layout.fillWidth: false
            width: backLabel.implicitWidth + 20; radius: 6
            color: backHov.containsMouse ? "#1a2848" : "transparent"
            border.color: backHov.containsMouse ? "#5068c8" : "#1e2230"
            border.width: backHov.containsMouse ? 1.5 : 1

            property real _eScale: 1.0
            scale: _eScale
            Timer { id: backRestoreTimer; interval: 120
                onTriggered: { backBtn._eScale = 1.0 }
            }

            Behavior on color { ColorAnimation { duration: 150 } }
            Behavior on border.color { ColorAnimation { duration: 150 } }
            Behavior on border.width { NumberAnimation { duration: 150 } }
            Behavior on _eScale {
                SpringAnimation { spring: 1.8; damping: 0.3; epsilon: 0.01 }
            }

            Row {
                anchors.centerIn: parent; spacing: 4
                Text { text: "←"; color: "#9094a8"; font.pixelSize: 14 }
                Text { id: backLabel; text: "返回"; color: "#9094a8"; font.pixelSize: 12 }
            }
            MouseArea {
                id: backHov; anchors.fill: parent
                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                onClicked: {
                    backBtn._eScale = 0.92
                    backRestoreTimer.restart()
                    root.animateOut()
                }
            }
        }

        // Icon + Title
        RowLayout {
            Layout.fillWidth: true; spacing: 14
            Rectangle {
                width: 48; height: 48; radius: 10; color: "#1a1f2e"
                Layout.preferredWidth: 48; Layout.preferredHeight: 48; clip: true
                Image {
                    id: modDetailIconImg
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectCrop; asynchronous: true; cache: true
                    sourceSize.width: 96; sourceSize.height: 96
                    source: modDetailIcon ? modDetailIcon.replace("cdn.modrinth.com", "mod.mcimirror.top") : ""
                    onStatusChanged: { if (status === Image.Error) iconFb.visible = true }
                }
                Text {
                    id: iconFb; anchors.centerIn: parent
                    text: modDetailTitle ? modDetailTitle[0] : "M"
                    color: "#5068c8"; font.pixelSize: 22; font.bold: true
                    visible: !modDetailIcon
                }
            }
            ColumnLayout { spacing: 2
                Text {
                    text: modDetailTitle || modDetailSlug; color: "#d0d4e0"
                    font.pixelSize: 18; font.bold: true; elide: Text.ElideRight
                }
                Text {
                    text: modDetailDesc || "无简介"; color: "#7888a8"
                    font.pixelSize: 10; maximumLineCount: 2; wrapMode: Text.WordWrap; elide: Text.ElideRight
                }
            }
        }

        // Detail info card
        Rectangle {
            Layout.fillWidth: true; implicitHeight: infoCol.implicitHeight + 24
            radius: 10; color: "#11141c"; border.color: "#1e2230"; border.width: 1
            Column {
                id: infoCol
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 12 }
                spacing: 6
                Text { text: "Slug: " + (modDetailSlug || ""); color: "#7888a8"; font.pixelSize: 11; elide: Text.ElideRight; width: parent.width }
                Text {
                    id: modVerCountText
                    property int _displayCount: 0
                    text: "版本数量: " + _displayCount
                    color: "#7888a8"; font.pixelSize: 11

                    Behavior on _displayCount {
                        NumberAnimation { duration: 2000; easing.type: Easing.OutCubic }
                    }
                }
                Row { spacing: 24
                    Text { text: "来源: Modrinth (MCIM镜像)"; color: "#7888a8"; font.pixelSize: 11 }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Text { text: "所有版本 | 按MC版本分组 | 点击展开"; color: "#505468"; font.pixelSize: 11 }
            Item { Layout.fillWidth: true }
            Rectangle {
                id: testToggleBtn
                width: testBtn.implicitWidth + 14; height: 22; radius: 4
                color: showTestVersions ? "#1a3a68" : "#11141c"
                border.color: (testHov.containsMouse || showTestVersions) ? "#4068c8" : "#2a3040"
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

                Text { id: testBtn; anchors.centerIn: parent; font.pixelSize: 10
                    text: showTestVersions ? "隐藏测试版" : "显示测试版"
                    color: showTestVersions ? "#8aaeff" : "#505468"
                }
                MouseArea { id: testHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        testToggleBtn._eScale = 0.9
                        testRestoreTimer.restart()
                        showTestVersions = !showTestVersions
                    }
                }
            }
        }

        // Loading indicator
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: modDetailLoading || grouped.length === 0

            Row {
                anchors.centerIn: parent
                spacing: 8
                visible: modDetailLoading || grouped.length === 0

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
                        anchors.fill: parent
                        visible: modDetailLoading

                        onPaint: {
                            var ctx = getContext("2d")
                            var cw = width, ch = height
                            ctx.clearRect(0, 0, cw, ch)
                            var cx = cw / 2, cy = ch / 2, r = Math.min(cx, cy) - 3
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

        // Version list
        ScrollView {
            id: modDetailScroll
            Layout.fillWidth: true
            Layout.fillHeight: !modDetailLoading && grouped.length > 0
            Layout.preferredHeight: 0
            clip: true
            ScrollBar.vertical.policy: ScrollBar.AlwaysOn
            visible: !modDetailLoading && grouped.length > 0
            enabled: !modDetailLoading && grouped.length > 0

            Column {
                width: modDetailScroll.availableWidth - 4; spacing: 4

                Repeater {
                    model: grouped
                    delegate: Column {
                        width: parent.width; spacing: 2

                        // Group header
                        Rectangle {
                            id: grpHeader
                            width: parent.width; height: 40; radius: 8
                            color: grpHover.containsMouse ? "#283860" : "#1a2848"
                            border.color: grpHover.containsMouse ? "#4068c8" : "#3858c0"
                            border.width: grpHover.containsMouse ? 1.5 : 1

                            property real _eScale: 1.0
                            transform: Scale {
                                origin.x: grpHeader.width / 2
                                origin.y: grpHeader.height / 2
                                xScale: grpHeader._eScale
                                yScale: grpHeader._eScale
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
                                    color: "#6080d8"; font.pixelSize: 14; font.bold: true
                                }
                                Item { Layout.fillWidth: true }
                                Text { text: modelData.versions.length + " 个版本"; color: "#505468"; font.pixelSize: 10 }
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
                            delegate: Column {
                                width: parent.width - 24; x: 24; spacing: 2

                                Rectangle {
                                    id: verCard
                                    width: parent.width; height: 50; radius: 6
                                    color: verHov.containsMouse ? "#1a2436" : "#11141c"
                                    border.color: verHov.containsMouse ? "#4068c8" : "#1e2230"
                                    border.width: verHov.containsMouse ? 1.5 : 1

                                    opacity: 0
                                    Component.onCompleted: { opacity = 1 }

                                    property real _clickScale: 1.0
                                    scale: _clickScale
                                    Timer { id: clickRestoreTimer; interval: 100
                                        onTriggered: { verCard._clickScale = 1.0 }
                                    }

                                    Behavior on color { ColorAnimation { duration: 200 } }
                                    Behavior on border.color { ColorAnimation { duration: 200 } }
                                    Behavior on border.width { NumberAnimation { duration: 150 } }
                                    Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
                                    Behavior on _clickScale {
                                        SpringAnimation { spring: 1.8; damping: 0.25; epsilon: 0.01 }
                                    }

                                    Rectangle {
                                        anchors.fill: parent; radius: parent.radius
                                        opacity: verHov.containsMouse ? 0.15 : 0
                                        gradient: Gradient {
                                            GradientStop { position: 0; color: "#5068c8" }
                                            GradientStop { position: 1; color: "#6080d8" }
                                        }
                                        Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
                                    }

                                    RowLayout {
                                        anchors.fill: parent; anchors.margins: 8; spacing: 8

                                        ColumnLayout {
                                            Layout.fillWidth: true; spacing: 1
                                            Layout.alignment: Qt.AlignVCenter

                                            RowLayout { spacing: 6
                                                Text {
                                                    text: (getVersionDetail(modelData) || {}).versionNumber || modelData
                                                    color: "#d0d4e0"; font.pixelSize: 11; font.bold: true
                                                }
                                                Repeater {
                                                    model: (getVersionDetail(modelData) || {}).loaders || []
                                                    Rectangle {
                                                        width: loaderTag.implicitWidth + 6; height: 14; radius: 2
                                                        color: "#1a2848"
                                                        Text { id: loaderTag; anchors.centerIn: parent; text: modelData; color: "#8aaeff"; font.pixelSize: 8 }
                                                    }
                                                }
                                                Rectangle {
                                                    visible: preReleaseTag(modelData)
                                                    width: prTag.implicitWidth + 6; height: 14; radius: 2
                                                    color: "#382818"
                                                    Text { id: prTag; anchors.centerIn: parent; text: preReleaseTag(modelData); color: "#d0a050"; font.pixelSize: 8 }
                                                }
                                            }
                                            RowLayout { spacing: 8
                                                Text {
                                                    text: "MC: " + ((getVersionDetail(modelData) || {}).gameVersions || [modelData]).join(", ")
                                                    color: "#788090"; font.pixelSize: 9; Layout.fillWidth: true; elide: Text.ElideRight
                                                }
                                            }
                                            RowLayout { spacing: 12
                                                Text { text: formatDate((getVersionDetail(modelData) || {}).date); color: "#606478"; font.pixelSize: 9 }
                                                Text { text: "下载量 " + formatDL((getVersionDetail(modelData) || {}).downloads); color: "#606478"; font.pixelSize: 9 }
                                            }
                                        }
                                    }

                                    MouseArea {
                                        id: verHover; anchors.fill: parent
                                        hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            verCard._clickScale = 0.92
                                            clickRestoreTimer.restart()

                                            var d = getVersionDetail(modelData)
                                            if (!d || !d.url) { if (toastManager) toastManager.show("无法获取下载地址"); return }
                                            var loaders = d.loaders || []
                                            var loader = loaders.length > 0 ? loaders[0] : ""
                                            var vn = d.versionNumber || modelData
                                            var title = modDetailTitle || modDetailSlug || "mod"
                                            var safeTitle = title.replace(/[\\\/:*?"<>|]/g, "_").replace(/\s+/g, "_")
                                            var fn = safeTitle + "-" + vn + (loader ? ("-" + loader) : "") + (modelData ? ("-" + modelData) : "") + ".jar"
                                            var mineDir = String(backend ? (backend.minecraftDir || "") : "")
                                            var defaultPath = mineDir ? (mineDir.replace(/\\+$/, "") + "/" + fn) : fn
                                            pendingModDownload = {slug: modDetailSlug, title: title,
                                                versionNumber: vn, loader: loader, gameVersion: modelData,
                                                url: d.url, filename: fn, size: d.size || 0, sha1: d.sha1 || "",
                                                defaultPath: defaultPath, displayName: title + " " + vn}
                                            modFileDialog.currentFolder = "file:///" + (mineDir || ".").replace(/\\/g, "/")
                                            modFileDialog.currentFile = "file:///" + defaultPath.replace(/\\/g, "/")
                                            modFileDialog.open()
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

    // ── Version count animation trigger ──
    onModDetailLoadingChanged: {
        if (!modDetailLoading) {
            var total = (modDetailRawVersions || []).length
            modVerCountText._displayCount = total
        }
    }

    // ── Backend Connections ──
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
        onRejected: {
            pendingModDownload = {}
        }
    }
}
