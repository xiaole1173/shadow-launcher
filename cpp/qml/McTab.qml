import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: root
    anchors.fill: parent
    anchors.margins: 12

    // References passed from parent
    property var backend: null
    property var mainWindow: null
    property var appWindow: null
    property var toastManager: null

    // Signal for flying ball animation
    signal triggerDownloadBall(real sourceX, real sourceY)

    // State properties
    property string currentFilter: "release"
    property string selectedVersionId: ""
    property int currentSource: -1
    property var mirrorSources: []
    property string clickedVersionId: ""
    property bool installingVersion: false
    property string complianceNotice: ""

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

    // ── Watch for install completion to clear clickedVersionId ──
    Connections {
        target: backend; enabled: backend !== null
        function onInstallStateChanged() {
            if (!backend.installing && root.clickedVersionId !== "") {
                console.log("[download-ui] install completed, clearing clickedVersionId: " + root.clickedVersionId)
                root.clickedVersionId = ""
            }
        }
    }

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
                { key: "release", label: "正式版", countFn: function() { return root.getReleaseCount() } },
                { key: "snapshot", label: "快照版", countFn: function() { return root.getSnapshotCount() } },
                { key: "old", label: "远古版", countFn: function() { return root.getOldCount() } }
            ]

            Rectangle {
                height: 30; radius: 15
                width: Math.min(pillRow.implicitWidth + 20, 140)
                color: root.currentFilter === modelData.key ? "#3a4eb8" : "#11141c"
                border.color: root.currentFilter === modelData.key ? "#3a4eb8" : "#1e2230"
                border.width: 1
                clip: true
                scale: pillMouse.containsMouse ? 1.04 : 1.0
                Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

                Row {
                    id: pillRow
                    anchors.centerIn: parent
                    spacing: 4
                    Text {
                        id: pillLabel
                        text: modelData.label
                        color: root.currentFilter === modelData.key ? "#e8ecf8" : "#9094a8"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                    Text {
                        id: pillCount
                        text: "(" + modelData.countFn() + ")"
                        color: root.currentFilter === modelData.key ? "#93acf0" : "#505468"
                        font.pixelSize: 11
                        visible: pillRow.width + 20 <= 140
                    }
                }

                MouseArea {
                    id: pillMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.currentFilter = modelData.key
                        if (appWindow) appWindow.pageLoading = true
                        Qt.callLater(refreshVersionModel)
                    }
                }
            }
        }

        // ── 刷新按钮 ──
        Rectangle {
            width: 28; height: 28; radius: 5
            color: refreshHover.hovered ? "#1a2848" : "transparent"
            border.color: refreshHover.hovered ? "#5068d8" : "#1e2230"
            border.width: 1
            scale: refreshHover.hovered ? 1.08 : 1.0
            Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
            Text {
                anchors.centerIn: parent
                text: "↻"
                color: refreshHover.hovered ? "#8aa8f0" : "#9094a8"
                font.pixelSize: 16
            }
            HoverHandler { id: refreshHover }
            ToolTip { visible: refreshHover.hovered; text: "刷新版本列表"; delay: 500 }
            MouseArea {
                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (backend) {
                        if (toastManager) toastManager.show("正在刷新...")
                        if (appWindow) appWindow.pageLoading = true
                        versionModel.clear()
                        backend.refreshVersionList()
                    }
                }
            }
        }

        Item { Layout.fillWidth: true }

        // ── Download source selector ──
        RowLayout {
            spacing: 4

            // Auto (default)
            Rectangle {
                height: 26; radius: 5
                width: multiLabel.implicitWidth + 14
                color: root.currentSource === -1 ? "#5068d8" : "#11141c"
                border.color: root.currentSource === -1 ? "#5068d8" : "#1e2230"
                scale: multiSrcMouse.containsMouse ? 1.04 : 1.0
                Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                Text { id: multiLabel; anchors.centerIn: parent; text: "自动"; color: root.currentSource === -1 ? "#ffffff" : "#9094a8"; font.pixelSize: 11 }
                MouseArea { id: multiSrcMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.currentSource = -1 }
            }
            // Divider
            Rectangle { width: 1; height: 16; color: "#1e2230" }
            // Dynamic mirror sources from backend
            Repeater {
                model: root.mirrorSources
                Rectangle {
                    height: 26; radius: 5
                    width: srcLabel.implicitWidth + 14
                    color: root.currentSource === modelData.index ? "#5068d8" : "#11141c"
                    border.color: root.currentSource === modelData.index ? "#5068d8" : "#1e2230"
                    scale: srcMouse.containsMouse ? 1.04 : 1.0
                    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                    Text { id: srcLabel; anchors.centerIn: parent; text: modelData.name; color: root.currentSource === modelData.index ? "#ffffff" : "#9094a8"; font.pixelSize: 11 }
                    MouseArea { id: srcMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.currentSource = modelData.index }
                }
            }
        }
    }

    // ── Latest version highlight ──
    Rectangle {
        id: latestHighlight
        anchors.top: filterRow.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 12
        anchors.topMargin: 8
        height: 48
        color: "#11141c"
        radius: 8
        border.color: "#1e2230"
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8

            Text {
                text: "Latest"
                color: "#3a4eb8"
                font.pixelSize: 11
                font.bold: true
                font.letterSpacing: 1
            }

            Text {
                text: backend ? backend.versionIds[0] || "" : ""
                color: "#d0d4e0"
                font.pixelSize: 15
                font.bold: true
            }

            Rectangle {
                width: 1
                height: 16
                color: "#1a1f2a"
            }

            Text {
                text: backend && backend.versionIds.length > 1 ? "snapshot " + backend.versionIds[1] : ""
                color: "#9094a8"
                font.pixelSize: 12
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
                color: itemHover.containsMouse ? "#11141c" : "transparent"
                radius: 6
                border.color: root.selectedVersionId === model.versionId ? "#3a4eb8" : "transparent"
                border.width: root.selectedVersionId === model.versionId ? 1 : 0

                // Entrance animation
                opacity: 0
                scale: 1.0
                Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
                Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
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
                        font.pixelSize: 13
                        font.weight: Font.Medium
                        Layout.preferredWidth: 130
                    }

                    // Installed badge
                    Rectangle {
                        visible: model.versionId && backend && backend.installedVersions && (backend.installedVersions.indexOf(model.versionId) >= 0)
                        radius: 3; height: 18
                        width: installedTag.implicitWidth + 10
                        color: "#1a3028"
                        Text {
                            id: installedTag
                            anchors.centerIn: parent
                            text: "已安装"
                            color: "#4bc870"; font.pixelSize: 9
                            font.family: "Consolas, monospace"
                        }
                    }

                    Rectangle {
                        radius: 3
                        height: 18
                        width: typeTag.implicitWidth + 12
                        color: model.vtype === "release" ? "#104830" :
                               (model.vtype === "snapshot" ? "#403010" : "#282828")

                        Text {
                            id: typeTag
                            anchors.centerIn: parent
                            text: model.vtype === "release" ? "正式版" :
                                  (model.vtype === "snapshot" ? "快照" : "旧版")
                            color: model.vtype === "release" ? "#4a8" :
                                   (model.vtype === "snapshot" ? "#b84" : "#999")
                            font.pixelSize: 10
                            font.family: "Consolas, monospace"
                        }
                    }

                    Item { Layout.fillWidth: true }

                    // Per-item download progress bar (visible when this version is installing)
                    Rectangle {
                        visible: backend && backend.installing && backend.installVersionId === model.versionId
                                 && backend.installTotal > 0 && backend.installPhase !== "done"
                        width: 80; height: 4; radius: 2
                        color: "#1a1f2a"
                        Rectangle {
                            height: 4; radius: 2; color: "#5068d8"
                            width: backend && backend.installTotal > 0 ? parent.width * (backend.installProgress / backend.installTotal) : 0
                            Behavior on width { NumberAnimation { duration: 200 } }
                        }
                    }

                    // Install button
                    Button {
                        id: installBtn
                        property bool isInstalled: backend && backend.installedVersions && (backend.installedVersions.indexOf(model.versionId) >= 0)
                        property bool isInstallingThis: backend && backend.installing && backend.installPhase !== "done" && (backend.installVersionId === model.versionId || root.clickedVersionId === model.versionId)
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
                        // Installing/queued takes priority over "已安装"
                        text: (isInstallingThis || isDownloadActive) ? "下载中…" : (isDownloadQueued ? "排队中" : (isInstalled ? "已安装" : "安装"))
                        implicitWidth: isInstalled ? 56 : (isDownloadQueued ? 56 : (isInstallingThis || isDownloadActive ? 56 : 48)); implicitHeight: 24
                        font.pixelSize: 10; font.weight: Font.Medium
                        z: 10
                        flat: true
                        enabled: !isInstalled && !isInstallingThis && !isDownloadQueued && !isDownloadActive && root.clickedVersionId !== model.versionId
                        contentItem: Text {
                            text: installBtn.text
                            color: installBtn.isInstalled ? "#4bc870" :
                                   (installBtn.isDownloadQueued ? "#e0a040" :
                                   (installBtn.isInstallingThis || installBtn.isDownloadActive ? "#6080e8" :
                                   (installBtn.hovered && installBtn.enabled ? "#ffffff" : "#707888")))
                            font.pixelSize: 10; font.weight: Font.Medium
                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 4
                            color: installBtn.isInstalled ? "#0a1810" :
                                   (installBtn.isDownloadQueued ? "#1a1800" :
                                   (installBtn.isInstallingThis || installBtn.isDownloadActive ? "#0a1020" :
                                   (installBtn.hovered && installBtn.enabled ? "#5068d8" : "transparent")))
                            border.color: installBtn.isInstalled ? "#1a3028" :
                                          (installBtn.isDownloadQueued ? "#3a3000" :
                                          (installBtn.isInstallingThis || installBtn.isDownloadActive ? "#1a2840" :
                                          (installBtn.hovered && installBtn.enabled ? "#5d6fe0" : "#1e2230")))
                            border.width: 1
                        }
                        onClicked: {
                            console.log("[McTab] Install clicked for " + model.versionId + " source=" + root.currentSource)
                            if (backend) {
                                // Log pre-install state
                                console.log("[download-ui] pre-install: version=" + model.versionId + " installing=" + backend.installing + " versionId=" + backend.installVersionId + " phase=" + backend.installPhase)
                                // Immediately mark this version as clicked so UI updates before page destruction
                                root.clickedVersionId = model.versionId
                                backend.installVersion(model.versionId, root.currentSource)
                                // Row bounce animation
                                rowBounceAnim.start()
                                // Show download nav + trigger flying ball via signal
                                if (root.mainWindow) {
                                    root.mainWindow.showDownloadNavSilent()
                                    var gp = installBtn.mapToItem(null, installBtn.width / 2, installBtn.height / 2)
                                    root.triggerDownloadBall(gp.x, gp.y)
                                }
                            }
                        }
                    }
                }

                MouseArea {
                    id: itemHover
                    anchors.fill: parent
                    z: -1  // below Button
                    hoverEnabled: true
                    onClicked: {
                        if (model.versionId) {
                            root.selectedVersionId = model.versionId
                        }
                    }
                }
            }
        }
    }

    // ── Backend version list ready ──
    Connections {
        target: backend
        enabled: backend !== null
        function onVersionListReady() { refreshVersionModel(); if (appWindow) appWindow.pageLoading = false }
    }

    // ── Version install progress overlay ──
    Rectangle {
        id: downloadBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        property bool hasActiveDownload: backend && backend.installing && backend.installPhase !== "done" && backend.installPhase !== "failed"
        property bool hasQueue: backend && backend.downloadQueue && backend.downloadQueue.length > 0
        property bool hasDownloads: hasActiveDownload || hasQueue
        height: hasDownloads ? (hasActiveDownload ? 80 : 52) : 0
        visible: hasDownloads
        color: "#11141c"
        radius: 8
        border.color: "#1e2230"
        border.width: 1
        Behavior on height { NumberAnimation { duration: 200 } }
        clip: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 4

            RowLayout {
                visible: downloadBar.hasActiveDownload
                Text {
                    text: (backend && backend.installPhase === "校验中...")
                        ? "检验完整性 " + (backend ? (backend.installVersionId || "") : "")
                        : "正在安装 " + (backend ? (backend.installVersionId || "") : "")
                    color: (backend && backend.installPhase === "校验中...") ? "#8aaeff" : "#d0d4e0"
                    font.pixelSize: 13
                    font.bold: true
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: {
                        if (!backend || backend.installTotal <= 0) return "准备中..."
                        if (backend.installPhase === "校验中...")
                            return backend.installProgress + "/" + backend.installTotal
                        return Math.round((backend.installProgress / backend.installTotal) * 100) + "%"
                    }
                    color: (backend && backend.installPhase === "校验中...") ? "#8aaeff" : "#9094a8"
                    font.pixelSize: 12
                }
            }

            RowLayout {
                visible: downloadBar.hasQueue || downloadBar.hasActiveDownload
                Text {
                    text: downloadBar.hasActiveDownload ? "" : "等待下载开始..."
                    color: "#9094a8"
                    font.pixelSize: 11
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: downloadBar.hasQueue ? ("排队: " + backend.downloadQueue.length + " 个版本") : ""
                    color: "#e0a040"
                    font.pixelSize: 11
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 4
                radius: 2
                color: "#1a1f2a"

                Rectangle {
                    height: 4
                    radius: 2
                    color: "#3a4eb8"
                    width: (backend && backend.installTotal > 0)
                        ? parent.width * (backend.installProgress / backend.installTotal)
                        : 0
                    Behavior on width { NumberAnimation { duration: 150 } }
                }
            }

            Text {
                text: "当前: " + (backend ? backend.installFile || "" : "")
                color: "#606478"
                font.pixelSize: 11
                elide: Text.ElideMiddle
                Layout.fillWidth: true
            }

            RowLayout {
                Item { Layout.fillWidth: true }
                Rectangle {
                    width: 70
                    height: 24
                    radius: 4
                    color: "transparent"
                    border.color: "#402428"
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "取消"
                        color: "#c05050"
                        font.pixelSize: 11
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (backend) backend.cancelInstall()
                            root.installingVersion = false
                        }
                    }
                }
            }
        }
    }

    // ──── Batch population timer ────
    property var _batchList: []
    property int _batchIndex: 0
    Timer {
        id: _batchTimer
        interval: 1
        repeat: true
        onTriggered: {
            var list = root._batchList
            var start = root._batchIndex
            var end = Math.min(start + 20, list.length)
            for (var i = start; i < end; i++) {
                var vid = list[i]
                if (vid === undefined || vid === null) continue
                versionModel.append({versionId: vid, vtype: root.currentFilter})
            }
            root._batchIndex = end
            if (end >= list.length) {
                _batchTimer.stop()
                if (appWindow) appWindow.pageLoading = false
            }
        }
    }

    function refreshVersionModel() {
        versionModel.clear()
        if (!backend) { if (appWindow) appWindow.pageLoading = false; return }
        var list
        if (currentFilter === "snapshot") list = backend.snapshotVersions
        else if (currentFilter === "old") list = backend.oldVersions
        else list = backend.releaseVersions
        if (!list || list.length === 0) { if (appWindow) appWindow.pageLoading = false; return }

        // Filter out undefined/null entries
        var cleanList = []
        for (var j = 0; j < list.length; j++) {
            if (list[j] !== undefined && list[j] !== null) {
                cleanList.push(list[j])
            }
        }
        // Batch populate to avoid UI freeze (20 items per tick)
        root._batchList = cleanList
        root._batchIndex = 0
        _batchTimer.restart()
    }

    function getReleaseCount() { return backend ? backend.releaseVersions.length : 0 }
    function getSnapshotCount() { return backend ? backend.snapshotVersions.length : 0 }
    function getOldCount() { return backend ? backend.oldVersions.length : 0 }

    // ── Load initial version list on component creation ──
    Component.onCompleted: {
        refreshVersionModel()
    }
}
