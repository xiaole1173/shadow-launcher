import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

// ResourcePackDetailPage
// Full-screen overlay for Resource Pack version detail
// Same pattern as ModDetailPage: self-contained, signals to parent

Rectangle {
    id: root
    anchors.fill: parent
    color: "#0c0f16"

    property var backend: null
    property var toastManager: null
    property var mainWindow: null

    property string rpDetailSlug: ""
    property string rpDetailTitle: ""
    property string rpDetailIconUrl: ""
    property string rpDetailAuthor: ""
    property string rpDetailDesc: ""
    property int rpDetailDownloads: 0
    property string rpDetailUpdated: ""
    property bool rpDetailLoading: false
    property var pendingRpDownload: ({})
    property var rpVersionCache: ({})
    property int rpVersionCacheVersion: 0
    property var rpVersionDetailCache: ({})  // { slug: { "1.21.11": {version_number,name,downloads,...} } }

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
    onRpDetailSlugChanged: {
        if (rpDetailSlug && backend) {
            rpDetailLoading = true
            rpVersionCache = ({})
            rpVersionDetailCache = ({})
            rpVersionCacheVersion = 0
            expandedGroups = []
            selectedVersion = ""
            backend.fetchResourcepackVersions([rpDetailSlug])
            console.log("[resourcepack] detail fetch versions for", rpDetailSlug)
        }
    }

    // ── Computed properties ──
    property var rpDetailGrouped: {
        var _ver = root.rpVersionCacheVersion; var d = root.rpVersionCache
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
        return result
    }
    property var expandedGroups: []
    property string selectedVersion: ""

    function isGroupExpanded(major) {
        if (typeof expandedGroups === 'string') return expandedGroups === major
        return expandedGroups.indexOf(major) >= 0
    }

    function toggleGroupExpanded(major) {
        var arr = (typeof expandedGroups === 'string') ? [expandedGroups] : expandedGroups.slice()
        var idx = arr.indexOf(major)
        if (idx >= 0) arr.splice(idx, 1)
        else arr.push(major)
        expandedGroups = arr
    }

    function getVerDetail(gameVer) {
        var cache = root.rpVersionDetailCache[root.rpDetailSlug]
        if (!cache) return null
        return cache[gameVer] || null
    }

    function formatDate(isoStr) {
        if (!isoStr) return "-"
        return isoStr.slice(0, 10)
    }

    function formatDownloads(n) {
        if (!n && n !== 0) return "0"
        if (n >= 1000000) return (n / 1000).toFixed(1) + "K"
        if (n >= 1000) return (n / 1000).toFixed(1) + "K"
        return String(n)
    }

    // ── UI ──

    // Click interceptor background
    MouseArea {
        anchors.fill: parent
        onClicked: console.log("[rp-detail] background click eaten")
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        // Back button
        Rectangle {
            Layout.preferredHeight: 30
            Layout.fillWidth: false
            width: rpBackLabel.implicitWidth + 20; radius: 6
            color: rpBackHov.containsMouse ? "#1a2848" : "transparent"
            border.color: rpBackHov.containsMouse ? "#5068c8" : "#1e2230"
            border.width: rpBackHov.containsMouse ? 1.5 : 1
            Behavior on border.color { ColorAnimation { duration: 150 } }
            Behavior on border.width { NumberAnimation { duration: 150 } }

            Row {
                anchors.centerIn: parent; spacing: 4
                Text { text: "←"; color: "#9094a8"; font.pixelSize: 14 }
                Text { id: rpBackLabel; text: "返回"; color: "#9094a8"; font.pixelSize: 12 }
            }
            MouseArea {
                id: rpBackHov; anchors.fill: parent
                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                onClicked: { root.animateOut() }
            }
        }

        // Icon + Title
        RowLayout {
            Layout.fillWidth: true; spacing: 14
            Rectangle {
                width: 48; height: 48; radius: 10; color: "#1a1f2e"
                Layout.preferredWidth: 48; Layout.preferredHeight: 48; clip: true
                Image {
                    id: rpDetailIcon
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectCrop; asynchronous: true; cache: true
                    sourceSize.width: 96; sourceSize.height: 96
                    source: root.rpDetailIconUrl || ""
                    onStatusChanged: {
                        if (status === Image.Error) rpDetailIconFallback.visible = true
                        else if (status === Image.Ready) rpDetailIconFallback.visible = false
                    }
                }
                Text {
                    id: rpDetailIconFallback
                    anchors.centerIn: parent
                    text: (rpDetailTitle || rpDetailSlug) ? (rpDetailTitle || rpDetailSlug)[0] : "R"
                    color: "#5068c8"; font.pixelSize: 22; font.bold: true
                    visible: !root.rpDetailIconUrl
                }
            }
            ColumnLayout { spacing: 2
                Text {
                    text: rpDetailTitle || rpDetailSlug; color: "#d0d4e0"
                    font.pixelSize: 18; font.bold: true; elide: Text.ElideRight
                }
                Text {
                    text: rpDetailDesc || "无简介"; color: "#7888a8"
                    font.pixelSize: 10; maximumLineCount: 2; wrapMode: Text.WordWrap; elide: Text.ElideRight
                }
            }
        }

        // Detail info card
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: rpInfoCardCol.implicitHeight + 24
            radius: 10; color: "#11141c"
            border.color: "#1e2c48"; border.width: 1
            Column {
                id: rpInfoCardCol
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 12 }
                spacing: 6
                Text {
                    text: "作者: " + (rpDetailAuthor || "未知")
                    color: "#7888a8"; font.pixelSize: 11
                    elide: Text.ElideRight; width: parent.width
                }
                Text {
                    text: rpDetailDesc || "无简介"
                    color: "#9098b0"; font.pixelSize: 11
                    elide: Text.ElideRight; maximumLineCount: 3; width: parent.width
                    wrapMode: Text.WordWrap
                }
                Row { spacing: 24
                    Text {
                        text: "下载量: " + formatDownloads(rpDetailDownloads) + " 次"
                        color: "#7888a8"; font.pixelSize: 11
                    }
                    Text {
                        text: "更新于: " + formatDate(rpDetailUpdated)
                        color: "#7888a8"; font.pixelSize: 11
                    }
                }
                // Action buttons
                Row { spacing: 8
                    Rectangle {
                        width: Math.max(rpModBtn.implicitWidth + 24, 110); height: 28; radius: 6
                        color: rpModBtnHov.containsMouse ? "#1a2a50" : "transparent"
                        border.color: "#5068c8"; border.width: 1.5
                        Behavior on color { ColorAnimation { duration: 150 } }
                        Text {
                            id: rpModBtn; anchors.centerIn: parent
                            text: "转到Modrinth"; color: "#6888e8"; font.pixelSize: 11
                        }
                        MouseArea {
                            id: rpModBtnHov; anchors.fill: parent
                            hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (rpDetailSlug) Qt.openUrlExternally("https://modrinth.com/resourcepack/" + rpDetailSlug)
                            }
                        }
                    }
                    Rectangle {
                        width: Math.max(rpCopyBtn.implicitWidth + 24, 90); height: 28; radius: 6
                        color: rpCopyBtnHov.containsMouse ? "#282018" : "transparent"
                        border.color: "#685040"; border.width: 1.5
                        Behavior on color { ColorAnimation { duration: 150 } }
                        Text {
                            id: rpCopyBtn; anchors.centerIn: parent
                            text: "复制名称"; color: "#c89860"; font.pixelSize: 11
                        }
                        MouseArea {
                            id: rpCopyBtnHov; anchors.fill: parent
                            hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (backend) backend.copyToClipboard(rpDetailTitle || rpDetailSlug)
                            }
                        }
                    }
                }
            }
        }

        // Version list header
        RowLayout {
            Layout.fillWidth: true
            Text {
                text: "所有可用游戏版本 | 大版本分组 | 点击安装"
                color: "#505468"; font.pixelSize: 11
            }
            Item { Layout.fillWidth: true }
        }

        // Loading indicator
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: rpDetailLoading || rpDetailGrouped.length === 0

            Row {
                anchors.centerIn: parent
                spacing: 8
                visible: rpDetailLoading || rpDetailGrouped.length === 0

                Rectangle {
                    id: spinnerBox
                    width: 24; height: 24; radius: 12; color: "transparent"
                    visible: rpDetailLoading

                    property real _angle: 0
                    NumberAnimation on _angle {
                        running: rpDetailLoading
                        from: 0; to: 360; duration: 1000; loops: Animation.Infinite
                    }
                    on_AngleChanged: spinCanvas.requestPaint()

                    Canvas {
                        id: spinCanvas
                        anchors.fill: parent
                        visible: rpDetailLoading
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
                    text: rpDetailLoading ? "加载版本中..."
                        : (rpDetailGrouped.length === 0 ? "无可用版本" : "")
                    color: "#606478"; font.pixelSize: 12
                }
            }
        }

        // Version list grouped by major, click to expand
        ScrollView {
            id: rpDetailScroll
            Layout.fillWidth: true
            Layout.fillHeight: !rpDetailLoading && rpDetailGrouped.length > 0
            Layout.preferredHeight: 0
            clip: true
            ScrollBar.vertical.policy: ScrollBar.AsNeeded
            visible: !rpDetailLoading && rpDetailGrouped.length > 0
            enabled: !rpDetailLoading && rpDetailGrouped.length > 0

            Column {
                width: rpDetailScroll.availableWidth - 4; spacing: 4

                Repeater {
                    model: rpDetailGrouped
                    delegate: Column {
                        width: parent.width; spacing: 2

                        // Group header
                        Rectangle {
                            id: grpHeader
                            width: parent.width; height: 40; radius: 8
                            color: rpDetGrpArea.containsMouse ? "#1e2c50" : "#141c2c"
                            border.color: rpDetGrpArea.containsMouse ? "#3858c0" : "#1a2848"
                            border.width: 1

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

                            RowLayout {
                                anchors.fill: parent; anchors.margins: 10; spacing: 10
                                Text {
                                    text: (isGroupExpanded(modelData.major) ? "\u25bc" : "\u25b8") + "  MC " + modelData.major
                                    color: "#6080d8"; font.pixelSize: 14; font.bold: true
                                }
                                Item { Layout.fillWidth: true }
                                Text {
                                    text: modelData.versions.length + " 个版本"
                                    color: "#505468"; font.pixelSize: 10
                                }
                            }
                            MouseArea {
                                id: rpDetGrpArea; anchors.fill: parent
                                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    grpHeader._eScale = 0.94
                                    grpRestoreTimer.restart()
                                    toggleGroupExpanded(modelData.major)
                                }
                            }
                        }

                        // Sub-versions (only when expanded) — Level 2 + Level 3 drill-down
                        Repeater {
                            model: isGroupExpanded(modelData.major) ? modelData.versions : []
                            delegate: Column {
                                width: parent.width - 24; x: 24; spacing: 2

                                // Level 2: sub-version row
                                Rectangle {
                                    id: verCard
                                    width: parent.width; height: 34; radius: 6
                                    color: {
                                        if (selectedVersion === modelData) return "#1a2848"
                                        return rpDetSubHov.containsMouse ? "#1a2436" : "#111820"
                                    }
                                    border.color: {
                                        if (selectedVersion === modelData) return "#3858c0"
                                        return rpDetSubHov.containsMouse ? "#1e3050" : "#141c28"
                                    }
                                    border.width: 1

                                    opacity: 0
                                    Component.onCompleted: { opacity = 1 }

                                    property real _clickScale: 1.0
                                    scale: _clickScale
                                    Timer { id: clickRestoreTimer; interval: 100
                                        onTriggered: { verCard._clickScale = 1.0 }
                                    }

                                    Behavior on color { ColorAnimation { duration: 200 } }
                                    Behavior on border.color { ColorAnimation { duration: 200 } }
                                    Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
                                    Behavior on _clickScale {
                                        SpringAnimation { spring: 1.8; damping: 0.25; epsilon: 0.01 }
                                    }

                                    RowLayout {
                                        anchors.fill: parent; anchors.margins: 8; spacing: 8
                                        Text {
                                            text: (selectedVersion === modelData ? "\u25bc" : "\u25b8") + " " + modelData
                                            color: "#8898b8"; font.pixelSize: 12
                                            font.family: "Consolas, monospace"; Layout.fillWidth: true
                                        }
                                    }
                                    MouseArea {
                                        id: rpDetSubHov; anchors.fill: parent
                                        hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            verCard._clickScale = 0.92
                                            clickRestoreTimer.restart()
                                            selectedVersion = (selectedVersion === modelData) ? "" : modelData
                                        }
                                    }
                                }

                                // Level 3: Version detail card
                                Rectangle {
                                    visible: selectedVersion === modelData
                                    width: parent.width; height: l3Content.implicitHeight + 20; radius: 8
                                    color: "#101828"
                                    border.color: "#2a3a58"; border.width: 1

                                    Column {
                                        id: l3Content
                                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 12 }
                                        spacing: 6

                                        property var detail: {
                                            var slugCache = root.rpVersionDetailCache[root.rpDetailSlug]
                                            return slugCache ? (slugCache[modelData] || {}) : {}
                                        }

                                        // Pack name
                                        Text {
                                            text: l3Content.detail.name || "-"
                                            color: "#d0d8f0"; font.pixelSize: 13; font.bold: true
                                            elide: Text.ElideRight; width: parent.width
                                        }

                                        // Version number row
                                        Row { spacing: 8
                                            Text {
                                                text: "版本:"; color: "#606880"; font.pixelSize: 10
                                                anchors.verticalCenter: parent.verticalCenter
                                            }
                                            Text {
                                                text: l3Content.detail.version_number || "-"
                                                color: "#c0c8e0"; font.pixelSize: 11
                                                font.family: "Consolas, monospace"
                                                anchors.verticalCenter: parent.verticalCenter
                                            }
                                            // Type badge
                                            Rectangle {
                                                width: typeBadge.implicitWidth + 8; height: 18; radius: 3
                                                color: (l3Content.detail.version_type === "release") ? "#1a3a28" : "#3a2a18"
                                                anchors.verticalCenter: parent.verticalCenter
                                                Text {
                                                    id: typeBadge
                                                    anchors.centerIn: parent
                                                    text: (l3Content.detail.version_type === "release") ? "正式版" :
                                                          (l3Content.detail.version_type === "beta") ? "测试版" :
                                                          (l3Content.detail.version_type || "-")
                                                    color: (l3Content.detail.version_type === "release") ? "#48d878" : "#e8a840"
                                                    font.pixelSize: 9
                                                }
                                            }
                                        }

                                        // Downloads + Date row
                                        Row { spacing: 16
                                            Row { spacing: 4
                                                Text { text: "下载量:"; color: "#606880"; font.pixelSize: 10; anchors.verticalCenter: parent.verticalCenter }
                                                Text {
                                                    text: root.formatDownloads(l3Content.detail.downloads)
                                                    color: "#a0a8c0"; font.pixelSize: 10
                                                    anchors.verticalCenter: parent.verticalCenter
                                                }
                                            }
                                            Row { spacing: 4
                                                Text { text: "日期:"; color: "#606880"; font.pixelSize: 10; anchors.verticalCenter: parent.verticalCenter }
                                                Text {
                                                    text: root.formatDate(l3Content.detail.date_published)
                                                    color: "#a0a8c0"; font.pixelSize: 10
                                                    anchors.verticalCenter: parent.verticalCenter
                                                }
                                            }
                                        }

                                        // Download button
                                        Rectangle {
                                            width: l3DownloadBtn.implicitWidth + 24; height: 28; radius: 6
                                            color: l3DownloadHov.containsMouse ? "#1a2a50" : "transparent"
                                            border.color: "#5068c8"; border.width: 2
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            Behavior on color { ColorAnimation { duration: 150 } }
                                            Text {
                                                id: l3DownloadBtn
                                                anchors.centerIn: parent
                                                text: "下载"
                                                color: "#5068c8"; font.pixelSize: 12; font.weight: Font.Medium
                                            }
                                            MouseArea {
                                                id: l3DownloadHov
                                                anchors.fill: parent; hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    console.log("[resourcepack] L3 download:", rpDetailSlug, modelData)
                                                    var detail = root.rpVersionDetailCache[root.rpDetailSlug]
                                                    var d = detail ? detail[modelData] : null
                                                    if (!d || !d.url) { if (toastManager) toastManager.show("无法获取下载地址"); return }
                                                    var title = root.rpDetailTitle || root.rpDetailSlug || "resourcepack"
                                                    var safeTitle = title.replace(/[\\\/:*?"<>|]/g, "_").replace(/\s+/g, "_")
                                                    var vn = d.version_number || modelData
                                                    var fn = safeTitle + "-" + vn + "-" + modelData + ".zip"
                                                    var mineDir = String(backend ? (backend.minecraftDir || "") : "")
                                                    var defaultPath = mineDir ? (mineDir.replace(/\\+$/, "") + "/" + fn) : fn
                                                    root.pendingRpDownload = {slug: rpDetailSlug, title: title,
                                                        versionNumber: vn, gameVersion: modelData,
                                                        url: d.url, filename: fn, size: d.size || 0, sha1: d.sha1 || "",
                                                        defaultPath: defaultPath, displayName: title + " " + vn}
                                                    rpFileDialog.currentFolder = "file:///" + (mineDir || ".").replace(/\\/g, "/")
                                                    rpFileDialog.currentFile = "file:///" + defaultPath.replace(/\\/g, "/")
                                                    rpFileDialog.open()
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
        }
    }

    // ── Backend Connections ──
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
            console.log("[resourcepack] detail versions ready:", slug, "count:", versions.length)
        }
        function onResourcepackVersionsProgress(done, total) {
            if (root.rpDetailSlug === "") return
            root.rpDetailLoading = done < total
        }
        function onResourceDownloadProgress(completed, total, fileName) {
            if (pendingRpDownload.displayName) {
                var pct = total > 0 ? Math.round(completed / total * 100) : 0
                console.log("[resourcepack] download progress:", pendingRpDownload.displayName, completed, "/", total, "(" + pct + "%)")
                if (toastManager) toastManager.show("下载中 " + pendingRpDownload.displayName + ": " + pct + "%")
                if (mainWindow && mainWindow.loadingBar) {
                    mainWindow.loadingBar.opacity = (completed < total) ? 1 : 0
                }
            }
        }
        function onResourceDownloadDone(success) {
            if (pendingRpDownload.displayName) {
                console.log("[resourcepack] download done:", pendingRpDownload.displayName, "success:", success)
                if (toastManager) toastManager.show(success ? ("下载完成: " + pendingRpDownload.displayName) : ("下载失败: " + pendingRpDownload.displayName))
                if (mainWindow && mainWindow.loadingBar) {
                    mainWindow.loadingBar.opacity = 0
                }
                pendingRpDownload = {}
            }
        }
    }

    // ── RP file save dialog ──
    FileDialog {
        id: rpFileDialog
        fileMode: FileDialog.SaveFile
        title: "保存资源包"
        nameFilters: ["ZIP 文件 (*.zip)", "所有文件 (*.*)"]
        onAccepted: {
            var p = root.pendingRpDownload
            if (!p || !p.url) return
            var savePath = String(selectedFile).replace(/^(file:\/{2,3})/i, "")
            if (!/\.zip$/i.test(savePath)) savePath += ".zip"
            console.log("[resourcepack] saving to:", savePath)
            var dlId = backend.downloadModFile(p.url, savePath, p.displayName, p.size || 0, p.sha1 || "")
            console.log("[resourcepack] download started, id:", dlId)
            if (toastManager) toastManager.show("开始下载 " + p.displayName)
            if (mainWindow) mainWindow.showModDownloadProgress()
            root.pendingRpDownload = {}
        }
        onRejected: {
            root.pendingRpDownload = {}
        }
    }
}
