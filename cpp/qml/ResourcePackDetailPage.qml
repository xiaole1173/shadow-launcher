import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

// ResourcePackDetailPage
// Full-screen detail page for a Resource Pack project's version list
// Architecture: InstallPage-style — fixed top bar + Flickable + cards
// Features L3 detail expansion within version cards

Rectangle {
    id: root
    anchors.fill: parent
    color: "#0c0f16"

    // ── Injected properties ──
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
    property var rpVersionDetailCache: ({})

    signal goBack()

    // ── Trigger fetch ──
    onRpDetailSlugChanged: {
        if (rpDetailSlug && backend) {
            rpDetailLoading = true
            rpVersionCache = ({})
            rpVersionDetailCache = ({})
            rpVersionCacheVersion = 0
            expandedGroups = []
            selectedVersion = ""
            backend.fetchResourcepackVersions([rpDetailSlug])
        }
    }

    // ── Computed: grouped versions ──
    property var rpDetailGrouped: {
        var _ver = rpVersionCacheVersion
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
        if (n >= 1000000) return (n / 1000).toFixed(1) + "K"
        if (n >= 1000) return (n / 1000).toFixed(1) + "K"
        return String(n)
    }

    // ━━━━━━━━━━━━━━━━━━━━ TOP BAR ━━━━━━━━━━━━━━━━━━━━
    Rectangle {
        id: topBar
        anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
        height: 44; color: "#0c0f16"; z: 10

        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12; spacing: 10

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
            Text {
                text: rpDetailTitle || rpDetailSlug || ""
                font.pixelSize: 16; font.weight: Font.Bold; color: "#e4e8f2"
                Layout.fillWidth: true; elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
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
                cardIcon: root.rpDetailIconUrl
                cardTitle: root.rpDetailTitle
                cardDesc: root.rpDetailDesc

                // Stats row
                RowLayout {
                    Layout.fillWidth: true; spacing: 24
                    Text {
                        text: "作者: " + (rpDetailAuthor || "未知")
                        color: "#7888a8"; font.pixelSize: 11
                    }
                    Text {
                        text: "下载量: " + formatDownloads(rpDetailDownloads) + " 次"
                        color: "#7888a8"; font.pixelSize: 11
                    }
                    Text {
                        text: "更新于: " + formatDate(rpDetailUpdated)
                        color: "#7888a8"; font.pixelSize: 11
                    }
                }

                // Action buttons row
                RowLayout {
                    Layout.fillWidth: true; spacing: 8
                    Rectangle {
                        implicitWidth: Math.max(rpModBtn.implicitWidth + 24, 110)
                        implicitHeight: 30; radius: 6
                        color: rpModBtnHov.containsMouse ? "#1a2a50" : "transparent"
                        border.color: "#3a5ed0"; border.width: 1.5

                        property real _eScale: 1.0; scale: _eScale
                        Timer { id: modBtnRestore; interval: 100
                            onTriggered: { rpModBtnRect._eScale = 1.0 }
                        }
                        Behavior on color { ColorAnimation { duration: 150 } }
                        Behavior on _eScale { SpringAnimation { spring: 1.8; damping: 0.3; epsilon: 0.01 } }

                        Text {
                            id: rpModBtn; anchors.centerIn: parent
                            text: "转到 Modrinth"; color: "#6080e8"; font.pixelSize: 12
                        }
                        MouseArea {
                            id: rpModBtnHov; anchors.fill: parent
                            hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                rpModBtnRect._eScale = 0.92; modBtnRestore.restart()
                                if (rpDetailSlug) Qt.openUrlExternally("https://modrinth.com/resourcepack/" + rpDetailSlug)
                            }
                        }
                    }
                    Rectangle {
                        id: rpCopyBtnRect
                        implicitWidth: Math.max(rpCopyBtn.implicitWidth + 24, 90)
                        implicitHeight: 30; radius: 6
                        color: rpCopyBtnHov.containsMouse ? "#282018" : "transparent"
                        border.color: "#685040"; border.width: 1.5

                        property real _eScale: 1.0; scale: _eScale
                        Timer { id: copyBtnRestore; interval: 100
                            onTriggered: { rpCopyBtnRect._eScale = 1.0 }
                        }
                        Behavior on color { ColorAnimation { duration: 150 } }
                        Behavior on _eScale { SpringAnimation { spring: 1.8; damping: 0.3; epsilon: 0.01 } }

                        Text {
                            id: rpCopyBtn; anchors.centerIn: parent
                            text: "复制名称"; color: "#c89860"; font.pixelSize: 12
                        }
                        MouseArea {
                            id: rpCopyBtnHov; anchors.fill: parent
                            hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                rpCopyBtnRect._eScale = 0.92; copyBtnRestore.restart()
                                if (backend) backend.copyToClipboard(rpDetailTitle || rpDetailSlug)
                            }
                        }
                    }
                }
            }

            // ── Section: Version List ──
            Text {
                visible: !rpDetailLoading && rpDetailGrouped.length > 0
                text: "版本列表"
                font.pixelSize: 14; font.weight: Font.DemiBold; color: "#a0a8c0"
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
                        width: 24; height: 24; radius: 12; color: "transparent"
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

            // ── Version groups ──
            Repeater {
                model: !rpDetailLoading ? rpDetailGrouped : []
                delegate: Column {
                    Layout.fillWidth: true; spacing: 2

                    // Group header
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
                                text: (isGroupExpanded(modelData.major) ? "\u25bc" : "\u25b8") + "  MC " + modelData.major
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
                                grpHeader._eScale = 0.94; grpRestoreTimer.restart()
                                toggleGroupExpanded(modelData.major)
                            }
                        }
                    }

                    // Sub-versions
                    Repeater {
                        model: isGroupExpanded(modelData.major) ? modelData.versions : []
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
                                    lines.push({ label: "名称:", value: d ? (d.name || "") : "" })
                                    lines.push({ label: "下载量:", value: formatDownloads(d ? d.downloads : 0) })
                                    return lines
                                }

                                hasDownload: true
                                downloadLabel: "安装"
                                showExpand: true
                                expanded: (root.selectedVersion === modelData)
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
                                        defaultPath: defaultPath,
                                        displayName: (rpDetailTitle || rpDetailSlug) + " " + vn
                                    }
                                    rpFileDialog.currentFolder = "file:///" + (mineDir || ".").replace(/\\/g, "/")
                                    rpFileDialog.currentFile = "file:///" + defaultPath.replace(/\\/g, "/")
                                    rpFileDialog.open()
                                }
                                onExpandToggled: {
                                    if (root.selectedVersion === modelData) {
                                        root.selectedVersion = ""
                                    } else {
                                        root.selectedVersion = modelData
                                    }
                                }

                                // ── L3 Detail (expanded content) ──
                                ColumnLayout {
                                    anchors.fill: parent
                                    spacing: 6

                                    // Feature flags (if any)
                                    Repeater {
                                        model: {
                                            var d = getVerDetail(modelData)
                                            if (!d || !d.features) return []
                                            var features = d.features
                                            var result = []
                                            for (var fk in features) {
                                                if (features[fk]) result.push(fk)
                                            }
                                            return result
                                        }
                                        delegate: RowLayout {
                                            spacing: 4
                                            Rectangle { width: 6; height: 6; radius: 3; color: "#4bc870" }
                                            Text {
                                                text: modelData.replace(/_/g, " ").replace(/\b\w/g, function(c){return c.toUpperCase()})
                                                color: "#9098b0"; font.pixelSize: 10
                                            }
                                        }
                                    }

                                    // Dependencies
                                    Text {
                                        visible: {
                                            var d = getVerDetail(modelData)
                                            return d && d.dependencies && d.dependencies.length > 0
                                        }
                                        text: "依赖: " + (function(){
                                            var d = getVerDetail(modelData)
                                            return d && d.dependencies ? d.dependencies.join(", ") : ""
                                        })()
                                        color: "#7888a8"; font.pixelSize: 10
                                        Layout.fillWidth: true; wrapMode: Text.WordWrap
                                    }

                                    // Screenshots placeholder / download info
                                    Text {
                                        visible: {
                                            var d = getVerDetail(modelData)
                                            return d && d.filename
                                        }
                                        text: (function(){
                                            var d = getVerDetail(modelData)
                                            return d ? ("文件: " + (d.filename || "") + "  |  " + formatDownloads(d.downloads || 0) + " 下载") : ""
                                        })()
                                        color: "#606478"; font.pixelSize: 10
                                        Layout.fillWidth: true; elide: Text.ElideRight
                                    }
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
        }
        function onResourcepackVersionsProgress(done, total) {
            if (root.rpDetailSlug === "") return
            root.rpDetailLoading = done < total
        }
        function onResourceDownloadProgress(completed, total, fileName) {
            if (pendingRpDownload.displayName) {
                var pct = total > 0 ? Math.round(completed / total * 100) : 0
                if (toastManager) toastManager.show("下载中 " + pendingRpDownload.displayName + ": " + pct + "%")
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
            var dlId = backend.downloadResourcepackFile(p.url, savePath, p.displayName)
            if (toastManager) toastManager.show("开始下载 " + p.displayName)
            pendingRpDownload = {}
        }
        onRejected: { pendingRpDownload = {} }
    }
}
