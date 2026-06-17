import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: page
    color: "#0c0f16"

    // Category tabs
    property int currentTab: 0  // 0=MC版本, 1=Mod, 2=光影, 3=资源包

    // MC Version state
    property string currentFilter: "release"  // release, snapshot, old
    property string selectedVersionId: ""
    property int currentSource: -1  // -1=multi-source, 0=BMCLAPI, 1=official

    // Mod state
    property string modSearchQuery: ""
    property string modLoader: "fabric"
    property var modSearchResults: []
    property bool modResultsReady: false

    // Install state
    property bool installingVersion: false
    property bool installingMod: false
    property string installingModName: ""

    signal goBack()

    function loadModResults() {
        if (!backend) return
        modResultsModel.clear()
        var popular = backend.getPopularMods(modLoader)
        if (popular && popular.length) {
            for (var i = 0; i < popular.length; i++) {
                var p = popular[i]
                modResultsModel.append({
                    slug: p.slug || "",
                    title: p.title || p.slug || "Unknown",
                    desc: p.desc || p.description || "",
                    icon: p.icon || "",
                    downloads: p.downloads || 0
                })
            }
        }
    }

    onCurrentTabChanged: {
        if (currentTab === 0) refreshVersionModel()
        if (currentTab === 1 && !modResultsReady) { loadModResults(); modResultsReady = true }
    }

    // ──── Animations ────
    opacity: 0
    y: 10
    Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
    Behavior on y { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
    Component.onCompleted: {
        opacity = 1; y = 0
        // Initial load of version list if already available
        refreshVersionModel()
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
        if (!backend) return
        var list
        if (currentFilter === "snapshot") list = backend.snapshotVersions
        else if (currentFilter === "old") list = backend.oldVersions
        else list = backend.releaseVersions
        if (!list) return
        for (var i = 0; i < list.length; i++) {
            versionModel.append({versionId: list[i], vtype: currentFilter})
        }
        appWindow.pageLoading = false
    }

    function getReleaseCount() { return backend ? backend.releaseVersions.length : 0 }
    function getSnapshotCount() { return backend ? backend.snapshotVersions.length : 0 }
    function getOldCount() { return backend ? backend.oldVersions.length : 0 }

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
        height: 38
        spacing: 4

        property var tabLabels: ["MC 版本", "Mod", "光影", "资源包"]
        property var tabIcons: ["", "", "", ""]

        Repeater {
            model: 4
            Rectangle {
                Layout.preferredWidth: 90
                Layout.fillHeight: true
                radius: 8
                color: page.currentTab === index ? "#1a1f2e" : "transparent"
                border.color: page.currentTab === index ? "#3a4eb8" : "transparent"
                border.width: page.currentTab === index ? 1 : 0

                Text {
                    anchors.centerIn: parent
                    text: tabBar.tabLabels[index]
                    color: page.currentTab === index ? "#d0d4e0" : "#606478"
                    font.pixelSize: 13
                    font.weight: page.currentTab === index ? Font.DemiBold : Font.Normal
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        page.currentTab = index
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
        color: "#1a1f2a"
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
        visible: page.currentTab === 0

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
                    { key: "release", label: "正式版", countFn: function() { return page.getReleaseCount() } },
                    { key: "snapshot", label: "快照版", countFn: function() { return page.getSnapshotCount() } },
                    { key: "old", label: "远古版", countFn: function() { return page.getOldCount() } }
                ]

                Rectangle {
                    height: 30; radius: 15
                    width: Math.min(pillRow.implicitWidth + 20, 140)
                    color: page.currentFilter === modelData.key ? "#3a4eb8" : "#11141c"
                    border.color: page.currentFilter === modelData.key ? "#3a4eb8" : "#1e2230"
                    border.width: 1
                    clip: true

                    Row {
                        id: pillRow
                        anchors.centerIn: parent
                        spacing: 4
                        Text {
                            id: pillLabel
                            text: modelData.label
                            color: page.currentFilter === modelData.key ? "#e8ecf8" : "#9094a8"
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }
                        Text {
                            id: pillCount
                            text: "(" + modelData.countFn() + ")"
                            color: page.currentFilter === modelData.key ? "#93acf0" : "#505468"
                            font.pixelSize: 11
                            visible: pillRow.width + 20 <= 140
                        }
                    }

                    MouseArea {
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

            Item { Layout.fillWidth: true }

            // ── Download source selector ──
            RowLayout {
                spacing: 4
                visible: page.currentTab === 0

                // Multi-source mode (default)
                Rectangle {
                    height: 26; radius: 5
                    width: multiLabel.implicitWidth + 14
                    color: page.currentSource === -1 ? "#5068d8" : "#11141c"
                    border.color: page.currentSource === -1 ? "#5068d8" : "#1e2230"
                    Text { id: multiLabel; anchors.centerIn: parent; text: "多源"; color: page.currentSource === -1 ? "#ffffff" : "#9094a8"; font.pixelSize: 11 }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: page.currentSource = -1 }
                }
                // Divider
                Rectangle { width: 1; height: 16; color: "#1e2230" }
                Repeater {
                    model: [
                        { key: 0, label: "BMCLAPI" },
                        { key: 1, label: "官方" }
                    ]
                    Rectangle {
                        height: 26; radius: 5
                        width: srcLabel.implicitWidth + 14
                        color: page.currentSource === modelData.key ? "#5068d8" : "#11141c"
                        border.color: page.currentSource === modelData.key ? "#5068d8" : "#1e2230"
                        Text { id: srcLabel; anchors.centerIn: parent; text: modelData.label; color: page.currentSource === modelData.key ? "#ffffff" : "#9094a8"; font.pixelSize: 11 }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: page.currentSource = modelData.key }
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
                    width: versionList.width
                    height: 42
                    color: itemHover.containsMouse ? "#11141c" : "transparent"
                    radius: 6
                    border.color: page.selectedVersionId === model.versionId ? "#3a4eb8" : "transparent"
                    border.width: page.selectedVersionId === model.versionId ? 1 : 0

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
                            visible: backend && backend.installedVersions && (backend.installedVersions.indexOf(model.versionId) >= 0)
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

                    // Install button - using QtQuick.Controls.Button (bypass Loader MA bug)
                        Button {
                            id: installBtn
                            property bool isInstalled: backend && backend.installedVersions && (backend.installedVersions.indexOf(model.versionId) >= 0)
                            property bool isInstallingThis: backend && backend.installing && backend.installVersionId === model.versionId && backend.installPhase !== "done"
                            text: isInstalled ? "已安装" : (isInstallingThis ? "下载中…" : "安装")
                            implicitWidth: isInstalled ? 56 : (isInstallingThis ? 56 : 48); implicitHeight: 24
                            font.pixelSize: 10; font.weight: Font.Medium
                            z: 10
                            flat: true
                            enabled: !isInstalled && !isInstallingThis
                            contentItem: Text {
                                text: installBtn.text
                                color: installBtn.isInstalled ? "#4bc870" : (installBtn.isInstallingThis ? "#6080e8" : (installBtn.hovered && installBtn.enabled ? "#ffffff" : "#707888"))
                                font.pixelSize: 10; font.weight: Font.Medium
                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                radius: 4
                                color: installBtn.isInstalled ? "#0a1810" : (installBtn.isInstallingThis ? "#0a1020" : (installBtn.hovered && installBtn.enabled ? "#5068d8" : "transparent"))
                                border.color: installBtn.isInstalled ? "#1a3028" : (installBtn.isInstallingThis ? "#1a2840" : (installBtn.hovered && installBtn.enabled ? "#5d6fe0" : "#1e2230"))
                                border.width: 1
                            }
                            onClicked: {
                                console.log("[DownloadPage] Install clicked for " + model.versionId + " source=" + page.currentSource)
                                if (backend) backend.installVersion(model.versionId, page.currentSource)
                            }
                        }
                    }

                    MouseArea {
                        id: itemHover
                        anchors.fill: parent
                        z: -1  // below Button
                        hoverEnabled: true
                        onClicked: {
                            page.selectedVersionId = model.versionId
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

        // ── Version install progress overlay ──
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 8
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            height: (backend && backend.installing && backend.installPhase !== "done" && backend.installPhase !== "failed") ? 80 : 0
            visible: backend && backend.installing && backend.installPhase !== "done" && backend.installPhase !== "failed"
            color: "#11141c"
            radius: 8
            border.color: "#1e2230"
            border.width: 1
            Behavior on height { NumberAnimation { duration: 200 } }
            clip: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 6

                RowLayout {
                    Text {
                        text: "正在安装 " + (page.selectedVersionId || "")
                        color: "#d0d4e0"
                        font.pixelSize: 13
                        font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: (backend && backend.installTotal > 0)
                            ? Math.round((backend.installProgress / backend.installTotal) * 100) + "%"
                            : "准备中..."
                        color: "#9094a8"
                        font.pixelSize: 12
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
                                page.installingVersion = false
                            }
                        }
                    }
                }
            }
        }

    // ════════════════════════════════════════════
    // TAB 1: Mod 下载
    // ════════════════════════════════════════════
    Item {
        anchors.top: tabBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.topMargin: 8
        visible: page.currentTab === 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8

            // Search bar
            RowLayout {
                Layout.fillWidth: true
                height: 34
                spacing: 8

                // Loader selector
                Row {
                    spacing: 4
                    Repeater {
                        model: ["Fabric", "Forge", "Quilt", "NeoForge"]
                        Rectangle {
                            width: 56
                            height: 28
                            radius: 6
                            color: page.modLoader === modelData.toLowerCase() ? "#3a4eb8" : "#11141c"
                            border.color: page.modLoader === modelData.toLowerCase() ? "#3a4eb8" : "#1e2230"
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: modelData
                                color: page.modLoader === modelData.toLowerCase() ? "#e8ecf8" : "#9094a8"
                                font.pixelSize: 11
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (page.modLoader !== modelData.toLowerCase()) {
                                        page.modLoader = modelData.toLowerCase()
                                        page.modResultsReady = false
                                        modResultsModel.clear()
                                        loadModResults()
                                    }
                                }
                            }
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                // Search input
                Rectangle {
                    width: 200
                    height: 30
                    radius: 6
                    color: "#11141c"
                    border.color: "#1e2230"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 4
                        spacing: 4

                        TextInput {
                            id: searchInput
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            color: "#d0d4e0"
                            verticalAlignment: TextInput.AlignVCenter
                            font.pixelSize: 12
                            onAccepted: {
                                if (backend && text) {
                                    backend.searchMods(text, page.modLoader)
                                }
                            }

                            Text {
                                anchors.fill: parent
                                verticalAlignment: Text.AlignVCenter
                                text: "搜索 Modrinth..."
                                color: "#505468"
                                font.pixelSize: 12
                                visible: !searchInput.text
                            }
                        }

                        Rectangle {
                            width: 28
                            height: 22
                            radius: 4
                            color: searchIconArea.containsMouse ? "#3a4eb8" : "transparent"

                            Text {
                                anchors.centerIn: parent
                                text: "Go"
                                color: searchIconArea.containsMouse ? "#e8ecf8" : "#606478"
                                font.pixelSize: 10
                            }

                            MouseArea {
                                id: searchIconArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (backend && searchInput.text) {
                                        backend.searchMods(searchInput.text, page.modLoader)
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Text {
                text: "推荐 Mod | 来源: Modrinth API | 搜索或点击安装"
                color: "#505468"
                font.pixelSize: 11
            }

            // Results grid
            ListModel { id: modResultsModel }

            // Backend search results
            Connections {
                target: backend
                enabled: backend !== null && page.currentTab === 1
                function onSearchResultsReady(results) {
                    if (results) {
                        page.modSearchResults = results
                        page.modResultsReady = true
                        modResultsModel.clear()
                        for (var i = 0; i < results.length; i++) {
                            var m = results[i]
                            modResultsModel.append({
                                slug: m.slug || "",
                                title: m.title || "",
                                desc: m.desc || m.description || "",
                                icon: m.icon || "",
                                downloads: m.downloads || 0
                            })
                        }
                    }
                }
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                ScrollBar.vertical.policy: ScrollBar.AsNeeded

                GridView {
                    id: modGrid
                    anchors.fill: parent
                    cellWidth: 224
                    cellHeight: 118
                    model: modResultsModel

                    delegate: Rectangle {
                        width: 214
                        height: 110
                        color: cardMouse.containsMouse ? "#11141c" : "#0e1018"
                        radius: 8
                        border.color: cardMouse.containsMouse ? "#3a4eb8" : "#1a1f2a"
                        border.width: 1

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 4

                            RowLayout {
                                spacing: 6
                                Rectangle {
                                    width: 28
                                    height: 28
                                    radius: 6
                                    color: "#1a1f2e"

                                    Text {
                                        anchors.centerIn: parent
                                        text: model.title ? model.title[0] : "M"
                                        color: "#6080e8"
                                        font.pixelSize: 14
                                        font.bold: true
                                    }
                                }
                                Text {
                                    text: model.title || ""
                                    color: "#d0d4e0"
                                    font.pixelSize: 13
                                    font.bold: true
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }

                            Text {
                                text: model.desc || "暂无简介"
                                color: "#606478"
                                font.pixelSize: 11
                                elide: Text.ElideRight
                                maximumLineCount: 2
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                            }

                            RowLayout {
                                Text {
                                    visible: model.downloads > 0
                                    text: model.downloads >= 1000000 ? "DL " + (model.downloads / 1000000).toFixed(1) + "M" :
                                          model.downloads >= 1000 ? "DL " + Math.round(model.downloads / 1000) + "K" : ""
                                    color: "#505468"
                                    font.pixelSize: 10
                                }
                                Item { Layout.fillWidth: true }
                                Rectangle {
                                    width: 44
                                    height: 22
                                    radius: 4
                                    color: cardMouse.containsMouse ? "#3a4eb8" : "transparent"
                                    border.color: cardMouse.containsMouse ? "#3a4eb8" : "#1e2230"
                                    border.width: 1

                                    Text {
                                        anchors.centerIn: parent
                                        text: "安装"
                                        color: cardMouse.containsMouse ? "#e8ecf8" : "#606478"
                                        font.pixelSize: 10
                                    }
                                }
                            }
                        }

                        MouseArea {
                            id: cardMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (model.slug && backend) {
                                    page.installingMod = true
                                    page.installingModName = model.title || model.slug
                                    backend.downloadMod(model.slug, page.modLoader)
                                }
                            }
                        }
                    }
                }
            }

            // Mod install progress bar
            Rectangle {
                visible: page.installingMod
                Layout.fillWidth: true
                Layout.preferredHeight: 4
                radius: 2
                color: "#1a1f2a"

                Rectangle {
                    height: 4
                    radius: 2
                    color: "#3a4eb8"
                    width: parent.width * 0.4  // indeterminate-ish
                    NumberAnimation on x {
                        from: 0
                        to: parent.width * 0.6
                        duration: 1000
                        loops: Animation.Infinite
                        running: page.installingMod
                    }
                }
            }
            Text {
                visible: page.installingMod
                text: "正在安装 " + (page.installingModName || "Mod...")
                color: "#606478"
                font.pixelSize: 11
            }
        }
    }

    // ════════════════════════════════════════════
    // TAB 2: 光影下载
    // ════════════════════════════════════════════
    Item {
        anchors.top: tabBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.topMargin: 8
        visible: page.currentTab === 2

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8

            Text {
                text: "热门光影包 | 来源: Modrinth / 社区推荐"
                color: "#505468"
                font.pixelSize: 11
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                ScrollBar.vertical.policy: ScrollBar.AsNeeded

                GridView {
                    id: shaderGrid
                    anchors.fill: parent
                    cellWidth: 224
                    cellHeight: 118

                    property var shaderData: []

                    Component.onCompleted: {
                        if (backend) {
                            var shaders = backend.getShaderList()
                            if (shaders && shaders.length) {
                                shaderData = shaders
                            }
                        }
                        // Fallback: hardcoded popular shaders if backend returns nothing
                        if (!shaderData.length) {
                            shaderData = [
                                { slug: "complementary-reimagined", title: "Complementary Reimagined", desc: "高品质光影，兼容性好" },
                                { slug: "bsl-shaders", title: "BSL Shaders", desc: "经典光影，性能优化出色" },
                                { slug: "seus-ptgi", title: "SEUS PTGI", desc: "光线追踪级画质" },
                                { slug: "sildurs-vibrant-shaders", title: "Sildur's Vibrant", desc: "色彩鲜艳，多档可选" },
                                { slug: "solas-shader", title: "Solas Shader", desc: "轻量级，中低配首选" },
                                { slug: "rethinking-voxels", title: "Rethinking Voxels", desc: "体积光，r0.1 系列" },
                                { slug: "bliss-shader", title: "Bliss Shader", desc: "Chocapic13 继承者" },
                                { slug: "kuda-shaders", title: "KUDA Shaders", desc: "写实风格光影" },
                                { slug: "pastel-shaders", title: "Pastel Shaders", desc: "柔和色彩，低配友好" },
                                { slug: "photon-shader", title: "Photon Shader", desc: "氛围感强" },
                                { slug: "makeup-ultra-fast", title: "MakeUp Ultra Fast", desc: "超快渲染" },
                                { slug: "chocapic13", title: "Chocapic13", desc: "经典光影代表" },
                                { slug: "voyager-shader", title: "Voyager Shader", desc: "现代光影，真实感" },
                                { slug: "noble-shaders", title: "Noble Shaders", desc: "SEUS 系列优化版" },
                                { slug: "luminance-shaders", title: "Luminance", desc: "Natural 风格" }
                            ]
                        }
                    }

                    model: shaderGrid.shaderData.length > 0 ? shaderGrid.shaderData.length : 0

                    delegate: Rectangle {
                        width: 214
                        height: 110
                        color: cardMouse2.containsMouse ? "#11141c" : "#0e1018"
                        radius: 8
                        border.color: cardMouse2.containsMouse ? "#3a4eb8" : "#1a1f2a"
                        border.width: 1

                        property var itemData: shaderGrid.shaderData[index] || {}

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 4

                            RowLayout {
                                spacing: 6
                                Rectangle {
                                    width: 28
                                    height: 28
                                    radius: 6
                                    color: "#1a1f2e"

                                    Text {
                                        anchors.centerIn: parent
                                        text: parent.parent.parent.parent.itemData.title
                                            ? parent.parent.parent.parent.itemData.title[0]
                                            : "S"
                                        color: "#d4b840"
                                        font.pixelSize: 14
                                        font.bold: true
                                    }
                                }
                                Text {
                                    text: parent.parent.parent.parent.itemData.title || ""
                                    color: "#d0d4e0"
                                    font.pixelSize: 13
                                    font.bold: true
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }

                            Text {
                                text: parent.parent.parent.parent.itemData.desc || ""
                                color: "#606478"
                                font.pixelSize: 11
                                elide: Text.ElideRight
                                maximumLineCount: 2
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                            }

                            RowLayout {
                                Item { Layout.fillWidth: true }
                                Rectangle {
                                    width: 44
                                    height: 22
                                    radius: 4
                                    color: cardMouse2.containsMouse ? "#3a4eb8" : "transparent"
                                    border.color: cardMouse2.containsMouse ? "#3a4eb8" : "#1e2230"
                                    border.width: 1

                                    Text {
                                        anchors.centerIn: parent
                                        text: "安装"
                                        color: cardMouse2.containsMouse ? "#e8ecf8" : "#606478"
                                        font.pixelSize: 10
                                    }
                                }
                            }
                        }

                        MouseArea {
                            id: cardMouse2
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                var d = shaderGrid.shaderData[index]
                                if (d && d.slug && backend) {
                                    page.installingMod = true
                                    page.installingModName = d.title || d.slug
                                    backend.downloadShader(d.slug)
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                visible: page.installingMod
                Layout.fillWidth: true
                Layout.preferredHeight: 4
                radius: 2
                color: "#1a1f2a"

                Rectangle {
                    height: 4
                    radius: 2
                    color: "#d4b840"
                    width: parent.width * 0.4

                    NumberAnimation on x {
                        from: 0
                        to: parent.width * 0.6
                        duration: 1200
                        loops: Animation.Infinite
                        running: page.installingMod
                    }
                }
            }
            Text {
                visible: page.installingMod
                text: "正在安装 " + (page.installingModName || "光影包...")
                color: "#606478"
                font.pixelSize: 11
            }
        }
    }

    // ════════════════════════════════════════════
    // TAB 3: 资源包下载
    // ════════════════════════════════════════════
    Item {
        anchors.top: tabBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.topMargin: 8
        visible: page.currentTab === 3

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 24

            Item { Layout.fillHeight: true }

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                width: 200
                height: 200
                radius: 16
                color: "#11141c"
                border.color: "#1e2230"
                border.width: 1

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 12

                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        width: 48
                        height: 48
                        radius: 12
                        color: "#1a1f2e"
                        border.color: "#1e2230"
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: "..."
                            color: "#505468"
                            font.pixelSize: 20
                        }
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "资源包下载"
                        color: "#d0d4e0"
                        font.pixelSize: 14
                        font.bold: true
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "即将支持"
                        color: "#606478"
                        font.pixelSize: 12
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 160
                        text: "资源包下载功能正在开发中\n敬请期待"
                        color: "#505468"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }

            Item { Layout.fillHeight: true }
        }
    }
}
