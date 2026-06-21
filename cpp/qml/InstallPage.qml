import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "qrc:/ShadowLauncher/qml"

// InstallPage
// MC version + mod loader installation page
// Cards: MC version, Forge, NeoForge, Fabric, Optifine, Fabric API

Rectangle {
    id: root
    anchors.fill: parent
    color: "#0c0f16"

    property var backend: null
    property string mcVersion: ""
    property var toastManager: null
    signal goBack()

    Component.onCompleted: {
        if (backend) backend.logMessage("[install] InstallPage loaded, mcVersion=" + mcVersion)
    }

    // Triggered when MainWindow pushes mcVersion (on first entry or version change)
    onMcVersionChanged: {
        if (mcVersion && backend) triggerQueries()
    }

    function triggerQueries() {
        if (!backend || !mcVersion) return
        backend.logMessage("[install] querying mod loaders for " + mcVersion)
        // Reset: clear old data, show loading
        forgeVersions = []; fabricVersions = []; neoforgeVersions = []; optifineVersions = []
        forgeLoading = true; fabricLoading = true; neoforgeLoading = true; optifineLoading = true
        // Reset selections
        selectedForge = ""; selectedNeoForge = ""; selectedFabric = ""; selectedOptifine = ""
        activeLoader = ""; customName = ""
        backend.queryForgeVersions(mcVersion)
        backend.queryFabricVersions(mcVersion)
        backend.queryNeoForgeVersions(mcVersion)
        backend.queryOptifineVersions(mcVersion)
    }

    property bool forgeLoading: false
    property bool fabricLoading: false
    property bool neoforgeLoading: false
    property bool optifineLoading: false

    Connections {
        target: backend
        function onForgeVersionsReady(list) {
            forgeLoading = false
            forgeVersions = annotateTypes(list)
        }
        function onFabricVersionsReady(list) {
            fabricLoading = false
            fabricVersions = annotateTypes(list)
        }
        function onNeoforgeVersionsReady(list) {
            neoforgeLoading = false
            neoforgeVersions = annotateTypes(list)
        }
        function onOptifineVersionsReady(list) {
            optifineLoading = false
            optifineVersions = annotateTypes(list)
        }
    }

    function annotateTypes(inputList) {
        var result = []
        for (var i = 0; i < inputList.length; i++) {
            var item = inputList[i]
            if (!item.version) continue
            var m = { version: item.version, date: item.date || "" }
            var v = item.version.toLowerCase()
            if (v.indexOf("snapshot") >= 0) m.type = "snapshot"
            else if (v.indexOf("preview") >= 0 || v.indexOf("_pre") >= 0) m.type = "preview"
            else if (v.indexOf("alpha") >= 0) m.type = "alpha"
            else if (v.indexOf("beta") >= 0 || v.indexOf("-pre") >= 0 || v.indexOf("rc") >= 0) m.type = "beta"
            else m.type = item.type || "release"
            result.push(m)
        }
        // Sort: release > beta > snapshot > preview > alpha, then by version desc
        var typeOrder = { release: 0, beta: 1, snapshot: 2, preview: 3, alpha: 4 }
        result.sort(function(a, b) {
            if (typeOrder[a.type] !== typeOrder[b.type]) return typeOrder[a.type] - typeOrder[b.type]
            return String(b.version).localeCompare(String(a.version))
        })
        // Mark latest release and latest stable version
        var foundLatest = false
        var foundStable = false
        for (var j = 0; j < result.length; j++) {
            if (!foundLatest && result[j].type === "release") {
                result[j].isLatestRelease = true
                foundLatest = true
            }
            if (!foundStable && (result[j].type === "release" || result[j].type === "beta")) {
                result[j].isLatestStable = true
                foundStable = true
            }
        }
        return result
    }

    // Selected mod loader state
    property string selectedForge: ""
    property string selectedNeoForge: ""
    property string selectedFabric: ""
    property string selectedOptifine: ""
    property string selectedFabricApi: ""
    property string activeLoader: ""

    // Version lists (populated from BMCLAPI via C++ signals)
    property var forgeVersions: []
    property var neoforgeVersions: []
    property var fabricVersions: []
    property var optifineVersions: []

    property bool hasModLoader: activeLoader !== ""

    property string versionSuffix: {
        var parts = []
        if (activeLoader === "forge" && selectedForge) parts.push("Forge-" + selectedForge)
        if (activeLoader === "neoforge" && selectedNeoForge) parts.push("NeoForge-" + selectedNeoForge)
        if (activeLoader === "fabric" && selectedFabric) parts.push("Fabric-" + selectedFabric)
        if (selectedOptifine) parts.push("OptiFine-" + selectedOptifine)
        return parts.join("-")
    }

    property string fullVersionName: mcVersion + (versionSuffix ? "-" + versionSuffix : "")
    property string customName: ""

    onFullVersionNameChanged: {
        // Reset input when selections change, unless user has typed a custom name
        if (customName === "") { nameInput.text = fullVersionName }
    }

    // TOP BAR
    Rectangle {
        id: topBar
        anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
        height: 44; color: "#0c0f16"; z: 10

        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12; spacing: 10

            // Back button
            Rectangle {
                width: backLabel.implicitWidth + 20; height: 30; radius: 6
                color: backMouse.containsMouse ? "#1a2440" : "transparent"
                Behavior on color { ColorAnimation { duration: 150 } }
                Text {
                    id: backLabel
                    anchors.centerIn: parent
                    text: "← 返回"
                    color: backMouse.containsMouse ? "#6080e8" : "#a0a8c0"
                    font.pixelSize: 13; font.weight: Font.Medium
                }
                MouseArea {
                    id: backMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (backend) backend.logMessage("[install] back button clicked")
                        root.goBack()
                    }
                }
            }

            Item { Layout.fillWidth: true }

            Text {
                text: mcVersion || "???"
                font.pixelSize: 16; font.weight: Font.Bold; color: "#e4e8f2"
            }

            Item { Layout.fillWidth: true }
        }
    }

    // CONTENT AREA
    Flickable {
        id: contentFlick
        anchors.top: topBar.bottom
        anchors.left: parent.left; anchors.right: parent.right
        anchors.bottom: parent.bottom
        contentWidth: width
        contentHeight: contentCol.implicitHeight + 32
        clip: true
        flickableDirection: Flickable.VerticalFlick
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
            width: 6
        }

        ColumnLayout {
            id: contentCol
            width: parent.width - 32
            x: 16
            spacing: 12

            // CARD 1 — MC Version (with download button + editable name)
            Rectangle {
                Layout.fillWidth: true; implicitHeight: 80; radius: 10
                color: "#11141c"; border.color: "#1e2230"; border.width: 1

                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 12; spacing: 8

                    // Row 1: version + download button
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "Minecraft " + mcVersion
                            font.pixelSize: 18; font.weight: Font.Bold; color: "#e4e8f2"
                            verticalAlignment: Text.AlignVCenter
                        }
                        Item { Layout.fillWidth: true }
                        Rectangle {
                            implicitWidth: dlLabel.implicitWidth + 24; height: 32; radius: 6
                            color: dlMouse.containsMouse ? "#3a5ed0" : "#2a4590"
                            Behavior on color { ColorAnimation { duration: 150 } }
                            Text {
                                id: dlLabel
                                anchors.centerIn: parent
                                text: "开始下载"
                                color: "#e8ecf8"; font.pixelSize: 13; font.weight: Font.DemiBold
                            }
                            MouseArea {
                                id: dlMouse
                                anchors.fill: parent; hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    var n = root.customName !== "" ? root.customName : root.fullVersionName
                                    if (backend) backend.logMessage("[install] download: " + n)
                                }
                            }
                        }
                    }

                    // Row 2: editable version name
                    Rectangle {
                        Layout.fillWidth: true; height: 32; radius: 4
                        color: "#161a22"
                        border.color: nameInput.activeFocus ? "#3a5ed0" : "#2a3040"
                        border.width: 1
                        Behavior on border.color { ColorAnimation { duration: 200 } }
                        TextInput {
                            id: nameInput
                            anchors.fill: parent; anchors.margins: 8
                            text: root.fullVersionName
                            font.pixelSize: 13; color: "#c0c8e0"
                            selectByMouse: true
                            verticalAlignment: TextInput.AlignVCenter
                            onTextChanged: { root.customName = text }
                        }
                    }
                }
            }

            // SECTION LABEL: Mod Loader
            Text {
                text: "Mod 加载器"
                font.pixelSize: 14; font.weight: Font.DemiBold; color: "#a0a8c0"
                Layout.topMargin: 8; Layout.leftMargin: 4
            }

            // CARD 2 — Forge
            ModLoaderCard {
                Layout.fillWidth: true
                title: "Forge"
                loaderKey: "forge"
                versions: root.forgeLoading ? [{version: "Loading...", type: "", date: ""}] : root.forgeVersions
                disabled: root.hasModLoader && root.activeLoader !== "forge"
                disabledReason: root.activeLoader === "neoforge" ? "NeoForge 已选中"
                              : root.activeLoader === "fabric" ? "Fabric 已选中"
                              : root.selectedOptifine ? "与当前 Optifine 不兼容" : ""
                selectedVersion: root.selectedForge
                onVersionSelected: function(ver) {
                    if (backend) backend.logMessage("[install] Forge selected: " + ver)
                    root.selectedForge = ver
                    root.activeLoader = "forge"
                }
                onVersionCleared: {
                    root.selectedForge = ""
                    root.activeLoader = ""
                    root.customName = ""
                }
            }

            // CARD 3 — NeoForge
            ModLoaderCard {
                Layout.fillWidth: true
                title: "NeoForge"
                loaderKey: "neoforge"
                versions: root.neoforgeLoading ? [{version: "Loading...", type: "", date: ""}] : root.neoforgeVersions
                disabled: root.hasModLoader && root.activeLoader !== "neoforge"
                disabledReason: root.activeLoader === "forge" ? "Forge 已选中"
                              : root.activeLoader === "fabric" ? "Fabric 已选中"
                              : root.selectedOptifine ? "Optifine 不兼容 NeoForge" : ""
                selectedVersion: root.selectedNeoForge
                onVersionSelected: function(ver) {
                    root.selectedNeoForge = ver
                    root.activeLoader = "neoforge"
                }
                onVersionCleared: {
                    root.selectedNeoForge = ""
                    root.activeLoader = ""
                    root.customName = ""
                }
            }

            // CARD 4 — Fabric
            ModLoaderCard {
                Layout.fillWidth: true
                title: "Fabric"
                loaderKey: "fabric"
                versions: root.fabricLoading ? [{version: "Loading...", type: "", date: ""}] : root.fabricVersions
                disabled: root.hasModLoader && root.activeLoader !== "fabric"
                disabledReason: root.activeLoader === "forge" ? "Forge 已选中"
                              : root.activeLoader === "neoforge" ? "NeoForge 已选中"
                              : root.selectedOptifine ? "Optifine 不兼容 Fabric" : ""
                selectedVersion: root.selectedFabric
                onVersionSelected: function(ver) {
                    root.selectedFabric = ver
                    root.activeLoader = "fabric"
                }
                onVersionCleared: {
                    root.selectedFabric = ""
                    root.activeLoader = ""
                    root.customName = ""
                }
            }

            // Fabric API (conditional)
            Rectangle {
                Layout.fillWidth: true
                height: root.activeLoader === "fabric" ? 52 : 0
                visible: root.activeLoader === "fabric"
                color: "#11141c"; radius: 8; border.color: "#1e2230"; border.width: 1
                clip: true
                Behavior on height { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
                RowLayout {
                    anchors.fill: parent; anchors.margins: 14; spacing: 8
                    Rectangle { width: 8; height: 8; radius: 4; color: root.selectedFabricApi ? "#4bc870" : "#505868" }
                    Text { text: "Fabric API"; font.pixelSize: 14; font.weight: Font.DemiBold; color: "#e4e8f2" }
                    Item { Layout.fillWidth: true }
                    Text { text: root.selectedFabricApi || "未选择"; font.pixelSize: 12; color: "#9498a8" }
                }
            }

            // SECTION LABEL: Shader
            Text {
                text: "光影加载（高清修复）"
                font.pixelSize: 14; font.weight: Font.DemiBold; color: "#a0a8c0"
                Layout.topMargin: 8; Layout.leftMargin: 4
            }

            // CARD 5 — Optifine
            ModLoaderCard {
                Layout.fillWidth: true
                title: "Optifine"
                loaderKey: "optifine"
                versions: root.optifineLoading ? [{version: "Loading...", type: "", date: ""}] : root.optifineVersions
                disabled: (root.hasModLoader && root.selectedOptifine === "")
                         || root.activeLoader === "fabric"
                         || root.activeLoader === "neoforge"
                disabledReason: root.activeLoader === "fabric" ? "Fabric 不兼容 Optifine"
                              : root.activeLoader === "neoforge" ? "Optifine 不支持 NeoForge"
                              : root.hasModLoader && root.selectedOptifine === "" ? "请先选择加载器" : ""
                selectedVersion: root.selectedOptifine
                onVersionSelected: function(ver) { root.selectedOptifine = ver }
                onVersionCleared: { root.selectedOptifine = "" }
            }

            Item { Layout.fillWidth: true; height: 40 }
        }
    }
}
