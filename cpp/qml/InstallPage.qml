import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

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
    signal navigateToProgress()
    signal requestMinimize()
    signal requestClose()

    Component.onCompleted: {
        if (backend) backend.logMessage("[install] InstallPage loaded, mcVersion=" + mcVersion)
    }

    onMcVersionChanged: {
        if (mcVersion && backend) triggerQueries()
    }

    function triggerQueries() {
        console.log("[install] triggerQueries for mcVersion=" + mcVersion)
        if (!backend || !mcVersion) return
        backend.logMessage("[install] querying mod loaders for " + mcVersion)
        forgeVersions = []; fabricVersions = []; neoforgeVersions = []; optifineVersions = []
        forgeLoading = true; fabricLoading = true; neoforgeLoading = true; optifineLoading = true
        selectedForge = ""; selectedNeoForge = ""; selectedFabric = ""; selectedOptifine = ""
        activeLoader = ""; customName = ""
        console.log("[install] selections reset: selectedForge='" + selectedForge + "' selectedFabric='" + selectedFabric + "'")
        backend.queryForgeVersions(mcVersion)
        backend.queryFabricVersions(mcVersion)
        backend.queryNeoForgeVersions(mcVersion)
        backend.queryOptifineVersions(mcVersion)
        backend.queryFabricApiVersions(mcVersion)
    }

    property bool forgeLoading: false
    property bool fabricLoading: false
    property bool neoforgeLoading: false
    property var fabricApiVersions: []
    property bool fabricApiLoading: false

    property bool optifineLoading: false

    Connections {
        target: backend
        function onForgeVersionsReady(list) {
            forgeLoading = false; forgeVersions = annotateTypes(list)
        }
        function onFabricVersionsReady(list) {
            fabricLoading = false; fabricVersions = annotateTypes(list)
        }
        function onNeoforgeVersionsReady(list) {
            neoforgeLoading = false; neoforgeVersions = annotateTypes(list)
        }
        function onOptifineVersionsReady(list) {
            optifineLoading = false; optifineVersions = annotateTypes(list)
        }
        function onFabricApiVersionsReady(list) {
            fabricApiLoading = false; fabricApiVersions = list
        }
        function onInstalledVersionsChanged() {
            var n = customName !== "" ? customName : fullVersionName
            checkVersionConflict(n)
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
        var typeOrder = { release: 0, beta: 1, snapshot: 2, preview: 3, alpha: 4 }
        result.sort(function(a, b) {
            if (typeOrder[a.type] !== typeOrder[b.type]) return typeOrder[a.type] - typeOrder[b.type]
            return String(b.version).localeCompare(String(a.version))
        })
        var foundLatest = false
        for (var j = 0; j < result.length; j++) {
            if (!foundLatest && result[j].type === "release") {
                result[j].isLatestRelease = true
                foundLatest = true
            }
        }
        return result
    }

    property string selectedForge: ""
    property string selectedNeoForge: ""
    property string selectedFabric: ""
    property string selectedOptifine: ""
    property string selectedFabricApi: ""
    property string selectedFabricApiUrl: ""
    property string selectedFabricApiFile: ""
    property string activeLoader: ""

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
        if (activeLoader === "fabric" && selectedFabricApi) parts.push("FabricAPI-" + selectedFabricApi)
        if (selectedOptifine) parts.push("OptiFine-" + selectedOptifine)
        return parts.join("-")
    }

    property string fullVersionName: mcVersion + (versionSuffix ? "-" + versionSuffix : "")
    property string customName: ""
    property bool versionConflict: false
    property string conflictMessage: ""

    function checkVersionConflict(name) {
        var prev = versionConflict
        if (!backend || !name) {
            versionConflict = false; conflictMessage = ""; return
        }
        var names = backend.activeVersionNames || backend.installedVersions
        if (!names) {
            versionConflict = false; conflictMessage = ""; return
        }
        for (var i = 0; i < names.length; i++) {
            if (names[i] === name) {
                versionConflict = true
                conflictMessage = "\u7248\u672c \"" + name + "\" \u5df2\u5b58\u5728\uff0c\u8bf7\u4fee\u6539\u540d\u79f0"
                console.log("[install] versionConflict=true, name=" + name + " matches activeVersionNames[" + i + "]")
                return
            }
        }
        versionConflict = false; conflictMessage = ""
        if (prev !== versionConflict) console.log("[install] versionConflict cleared, name=" + name)
    }

    onFullVersionNameChanged: {
        checkVersionConflict(fullVersionName)
    }

    onCustomNameChanged: {
        var n = customName !== "" ? customName : fullVersionName
        checkVersionConflict(n)
    }

    // TOP BAR
    Rectangle {
        id: topBar
        anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
        height: 44; color: "#0c0f16"; z: 10

        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12; spacing: 10
            Rectangle {
                width: backLabel.implicitWidth + 20; height: 30; radius: 6
                color: backMouse.containsMouse ? "#1a2440" : "transparent"
                Behavior on color { ColorAnimation { duration: 150 } }
                Text {
                    id: backLabel; anchors.centerIn: parent
                    text: "\u2190 \u8fd4\u56de"; font.pixelSize: 13; font.weight: Font.Medium
                    color: backMouse.containsMouse ? "#6080e8" : "#a0a8c0"
                }
                MouseArea {
                    id: backMouse; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: { if (backend) backend.logMessage("[install] back"); root.goBack() }
                }
            }
            Item { Layout.fillWidth: true }
            Text { text: mcVersion || "???"; font.pixelSize: 16; font.weight: Font.Bold; color: "#e4e8f2" }
            Item { Layout.fillWidth: true }
            Rectangle {
                width: 28; height: 28; radius: 14
                color: minHov.containsMouse ? (minHov.pressed ? "#3a4050" : "#252a35") : "transparent"
                scale: minHov.pressed ? 0.85 : (minHov.containsMouse ? 1.12 : 1.0)
                Behavior on color { ColorAnimation { duration: 150 } }
                Behavior on scale { NumberAnimation { duration: 150 } }
                Text { anchors.centerIn: parent; text: "\u2014"; color: minHov.containsMouse ? "#d0d4e0" : "#505568"; font.pixelSize: 13; font.weight: Font.Bold }
                MouseArea { id: minHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { root.requestMinimize() } }
            }
            Item { width: 6 }
            Rectangle {
                width: 28; height: 28; radius: 14
                color: closeHov.containsMouse ? (closeHov.pressed ? "#e06060" : "#c05050") : "transparent"
                scale: closeHov.pressed ? 0.85 : (closeHov.containsMouse ? 1.12 : 1.0)
                Behavior on color { ColorAnimation { duration: 150 } }
                Behavior on scale { NumberAnimation { duration: 150 } }
                Text { anchors.centerIn: parent; text: "\u2715"; color: closeHov.containsMouse ? "#fff" : "#505568"; font.pixelSize: 12; font.weight: Font.Bold }
                MouseArea { id: closeHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { root.requestClose() } }
            }
        }
    }

    // CONTENT AREA
    Flickable {
        id: contentFlick
        anchors.top: topBar.bottom; anchors.left: parent.left; anchors.right: parent.right
        anchors.bottom: parent.bottom
        contentWidth: width; contentHeight: contentCol.implicitHeight + 32
        clip: true; flickableDirection: Flickable.VerticalFlick; boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 6 }

        ColumnLayout {
            id: contentCol
            width: parent.width - 32; x: 16; spacing: 12

            // CARD 1 - MC Version
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: root.versionConflict ? 116 : 96
                radius: 10; color: "#11141c"
                border.color: root.versionConflict ? "#803040" : "#1e2230"
                border.width: 1; clip: true
                Behavior on implicitHeight { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                Behavior on border.color { ColorAnimation { duration: 250 } }

                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 12; spacing: 8
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: "Minecraft " + mcVersion; font.pixelSize: 18; font.weight: Font.Bold; color: "#e4e8f2"; verticalAlignment: Text.AlignVCenter }
                        Item { Layout.fillWidth: true }
                        Rectangle {
                            implicitWidth: dlLabel.implicitWidth + 24; height: 32; radius: 6
                            color: root.versionConflict ? "#3a3040" : (dlMouse.containsMouse ? "#3a5ed0" : "#2a4590")
                            opacity: root.versionConflict ? 0.5 : 1.0
                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on opacity { NumberAnimation { duration: 200 } }
                            Text {
                                id: dlLabel; anchors.centerIn: parent; text: "\u5f00\u59cb\u4e0b\u8f7d"
                                color: root.versionConflict ? "#706878" : "#e8ecf8"
                                font.pixelSize: 13; font.weight: Font.DemiBold
                            }
                            MouseArea {
                                id: dlMouse; anchors.fill: parent; hoverEnabled: true
                                cursorShape: root.versionConflict ? Qt.ArrowCursor : Qt.PointingHandCursor
                                enabled: !root.versionConflict
                                onClicked: {
                                    var n = root.customName !== "" ? root.customName : root.fullVersionName
                                    console.log("[install] QML button clicked: name=" + n + " forge=" + selectedForge + " fabric=" + selectedFabric + " neo=" + selectedNeoForge + " opti=" + selectedOptifine)
                                    if (backend) {
                                        backend.logMessage("[install] download: " + n)
                                        if (selectedForge !== "") {
                                            backend.installModLoader(mcVersion, "forge", selectedForge, n)
                                        } else if (selectedNeoForge !== "") {
                                            backend.installModLoader(mcVersion, "neoforge", selectedNeoForge, n)
                                        } else if (selectedFabric !== "") {
                                            backend.installModLoader(mcVersion, "fabric", selectedFabric, n)
                                            if (selectedFabricApi !== "" && selectedFabricApiUrl !== "") {
                                                var gameDir = backend.getVersionGameDir(n) || ""
                                                var apiPath = gameDir + "/mods/" + selectedFabricApiFile
                                                backend.installFabricApi(selectedFabricApi, selectedFabricApiUrl, apiPath)
                                            }
                                        } else if (selectedOptifine !== "") {
                                            backend.installOptifine(mcVersion, selectedOptifine, selectedForge, n)
                                        } else {
                                            backend.installVersion(n)
                                        }
                                        root.navigateToProgress()
                                    }
                                }
                            }
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true; height: 32; radius: 4
                        color: "#161a22"
                        border.color: root.versionConflict ? "#c06050" : (nameInput.activeFocus ? "#3a5ed0" : "#2a3040")
                        border.width: 1
                        Behavior on border.color { ColorAnimation { duration: 200 } }
                        TextInput {
                            id: nameInput; anchors.fill: parent; anchors.margins: 8
                            text: root.customName !== "" ? root.customName : root.fullVersionName
                            font.pixelSize: 13; color: "#c0c8e0"
                            selectByMouse: true; clip: true; verticalAlignment: TextInput.AlignVCenter
                            onActiveFocusChanged: {
                                if (!activeFocus) {
                                    if (text !== root.fullVersionName) {
                                        root.customName = text
                                    } else {
                                        root.customName = ""
                                    }
                                }
                            }
                        }
                    }
                    // Conflict warning
                    Text {
                        visible: root.versionConflict
                        text: root.conflictMessage
                        font.pixelSize: 11; color: "#e06060"
                        Layout.fillWidth: true; elide: Text.ElideRight
                    }
                }
            }

            // SECTION LABEL: Mod Loader
            Text {
                text: "Mod \u52a0\u8f7d\u5668"
                font.pixelSize: 14; font.weight: Font.DemiBold; color: "#a0a8c0"
                Layout.topMargin: 8; Layout.leftMargin: 4
            }

            // Forge
            ModLoaderCard {
                Layout.fillWidth: true; title: "Forge"; loaderKey: "forge"
                versions: root.forgeLoading ? [{version: "Loading...", type: "", date: ""}] : root.forgeVersions
                disabled: root.hasModLoader && root.activeLoader !== "forge"
                disabledReason: root.activeLoader === "neoforge" ? "NeoForge \u5df2\u9009\u4e2d" : root.activeLoader === "fabric" ? "Fabric \u5df2\u9009\u4e2d" : root.selectedOptifine ? "\u4e0e Optifine \u4e0d\u517c\u5bb9" : ""
                selectedVersion: root.selectedForge
                onVersionSelected: function(ver) { root.selectedForge = ver; root.activeLoader = "forge"; root.customName = ""; if (backend) backend.logMessage("[install] Forge: " + ver) }
                onVersionCleared: { root.selectedForge = ""; root.activeLoader = ""; root.customName = "" }
            }

            // NeoForge
            ModLoaderCard {
                Layout.fillWidth: true; title: "NeoForge"; loaderKey: "neoforge"
                versions: root.neoforgeLoading ? [{version: "Loading...", type: "", date: ""}] : root.neoforgeVersions
                disabled: root.hasModLoader && root.activeLoader !== "neoforge"
                disabledReason: root.activeLoader === "forge" ? "Forge \u5df2\u9009\u4e2d" : root.activeLoader === "fabric" ? "Fabric \u5df2\u9009\u4e2d" : root.selectedOptifine ? "\u4e0d\u517c\u5bb9 NeoForge" : ""
                selectedVersion: root.selectedNeoForge
                onVersionSelected: function(ver) { root.selectedNeoForge = ver; root.activeLoader = "neoforge"; root.customName = "" }
                onVersionCleared: { root.selectedNeoForge = ""; root.activeLoader = ""; root.customName = "" }
            }

            // Fabric
            ModLoaderCard {
                Layout.fillWidth: true; title: "Fabric"; loaderKey: "fabric"
                versions: root.fabricLoading ? [{version: "Loading...", type: "", date: ""}] : root.fabricVersions
                disabled: root.hasModLoader && root.activeLoader !== "fabric"
                disabledReason: root.activeLoader === "forge" ? "Forge \u5df2\u9009\u4e2d" : root.activeLoader === "neoforge" ? "NeoForge \u5df2\u9009\u4e2d" : root.selectedOptifine ? "\u4e0d\u517c\u5bb9 Fabric" : ""
                selectedVersion: root.selectedFabric
                onVersionSelected: function(ver) { root.selectedFabric = ver; root.activeLoader = "fabric"; root.customName = "" }
                onVersionCleared: { root.selectedFabric = ""; root.activeLoader = ""; root.customName = "" }
            }

            // Fabric API (unified card, visible only when Fabric is selected)
            ModLoaderCard {
                Layout.fillWidth: true; title: "Fabric API"; loaderKey: "fabric_api"
                visible: root.activeLoader === "fabric"
                badgeText: root.activeLoader === "fabric" && !root.selectedFabricApi ? "\u5efa\u8bae\u5b89\u88c5" : ""
                disabled: false
                versions: root.fabricApiLoading ? [{version: "Loading...", type: "", date: ""}] : root.fabricApiVersions
                selectedVersion: root.selectedFabricApi
                onVersionSelected: function(ver) {
                    root.selectedFabricApi = ver
                    for (var i = 0; i < root.fabricApiVersions.length; i++) {
                        if (root.fabricApiVersions[i].version === ver) {
                            root.selectedFabricApiUrl = root.fabricApiVersions[i].url
                            root.selectedFabricApiFile = root.fabricApiVersions[i].filename
                            break
                        }
                    }
                }
                onVersionCleared: {
                    root.selectedFabricApi = ""
                    root.selectedFabricApiUrl = ""
                    root.selectedFabricApiFile = ""
                }
            }

            // SECTION LABEL: Shader
            Text { text: "\u5149\u5f71\u52a0\u8f7d\uff08\u9ad8\u6e05\u4fee\u590d\uff09"; font.pixelSize: 14; font.weight: Font.DemiBold; color: "#a0a8c0"; Layout.topMargin: 8; Layout.leftMargin: 4 }

            // Optifine
            ModLoaderCard {
                Layout.fillWidth: true; title: "Optifine"; loaderKey: "optifine"
                versions: root.optifineLoading ? [{version: "Loading...", type: "", date: ""}] : root.optifineVersions
                disabled: (root.hasModLoader && root.selectedOptifine === "") || root.activeLoader === "fabric" || root.activeLoader === "neoforge"
                disabledReason: root.activeLoader === "fabric" ? "Fabric \u4e0d\u517c\u5bb9 Optifine" : root.activeLoader === "neoforge" ? "Optifine \u4e0d\u652f\u6301 NeoForge" : root.hasModLoader ? "\u8bf7\u5148\u9009\u62e9\u52a0\u8f7d\u5668" : ""
                selectedVersion: root.selectedOptifine
                onVersionSelected: function(ver) { root.selectedOptifine = ver }
                onVersionCleared: { root.selectedOptifine = "" }
            }

            Item { Layout.fillWidth: true; height: 40 }
        }
    }
}
