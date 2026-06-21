import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "qrc:/ShadowLauncher/qml"

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
        forgeVersions = [
            {version: "47.3.0", type: "release", date: "2024-12-01"},
            {version: "47.2.0", type: "release", date: "2024-10-15"},
            {version: "47.1.0-beta", type: "beta", date: "2024-09-01"}
        ]
        neoforgeVersions = [
            {version: "21.4.0", type: "release", date: "2024-12-05"},
            {version: "21.3.0-beta", type: "beta", date: "2024-11-10"}
        ]
        fabricVersions = [
            {version: "0.16.9", type: "release", date: "2024-12-03"},
            {version: "0.16.7", type: "release", date: "2024-10-20"}
        ]
        optifineVersions = [
            {version: "HD_U_J6", type: "release", date: "2024-12-01"},
            {version: "HD_U_J5_pre1", type: "beta", date: "2024-11-15"}
        ]
    }

    // ── State ──
    property string selectedForge: ""
    property string selectedNeoForge: ""
    property string selectedFabric: ""
    property string selectedOptifine: ""
    property string selectedFabricApi: ""
    property string activeLoader: ""

    property var forgeVersions: []
    property var neoforgeVersions: []
    property var fabricVersions: []
    property var optifineVersions: []

    property bool hasModLoader: activeLoader !== ""
    property string customName: ""

    // Auto-generated default name
    property string versionSuffix: {
        var parts = []
        if (activeLoader === "forge" && selectedForge) parts.push("Forge-" + selectedForge)
        if (activeLoader === "neoforge" && selectedNeoForge) parts.push("NeoForge-" + selectedNeoForge)
        if (activeLoader === "fabric" && selectedFabric) parts.push("Fabric-" + selectedFabric)
        if (selectedOptifine) parts.push("OptiFine-" + selectedOptifine)
        return parts.join("-")
    }

    property string defaultName: mcVersion + (versionSuffix ? "-" + versionSuffix : "")
    property string displayName: customName !== "" ? customName : defaultName

    // ═══ TOP BAR ═══
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
                    id: backLabel
                    anchors.centerIn: parent
                    text: "\u2190 返回"
                    color: backMouse.containsMouse ? "#6080e8" : "#a0a8c0"
                    font.pixelSize: 13; font.weight: Font.Medium
                }
                MouseArea {
                    id: backMouse
                    anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.goBack()
                }
            }
            Item { Layout.fillWidth: true }
        }
    }

    Flickable {
        id: contentFlick
        anchors.top: topBar.bottom; anchors.left: parent.left; anchors.right: parent.right
        anchors.bottom: parent.bottom
        contentWidth: width; contentHeight: contentCol.implicitHeight + 32
        clip: true
        flickableDirection: Flickable.VerticalFlick
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 6 }

        ColumnLayout {
            id: contentCol
            width: parent.width - 32; x: 16; spacing: 12

            // ═══ CARD 1 — MC Version (with download button on right) ═══
            Rectangle {
                Layout.fillWidth: true; height: 72; radius: 10
                color: "#11141c"; border.color: "#1e2230"; border.width: 1
                RowLayout {
                    anchors.fill: parent; anchors.margins: 14; spacing: 12
                    ColumnLayout {
                        spacing: 2; Layout.fillWidth: true
                        RowLayout { spacing: 6
                            Text {
                                text: mcVersion; font.pixelSize: 18; font.weight: Font.Bold; color: "#e4e8f2"
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                        // Editable custom name
                        RowLayout { spacing: 4; visible: displayName !== mcVersion
                            Text {
                                text: "名称:"; font.pixelSize: 11; color: "#787c90"
                                verticalAlignment: Text.AlignVCenter
                            }
                            TextInput {
                                id: nameInput
                                text: displayName
                                font.pixelSize: 12; color: "#c0c8e0"
                                selectByMouse: true
                                Layout.fillWidth: true
                                onTextChanged: { root.customName = text }
                                Rectangle {
                                    anchors.fill: parent; z: -1; color: "#1a1e2a"; radius: 3
                                    anchors.margins: -4
                                }
                            }
                        }
                    }
                    // Download button on the right
                    Rectangle {
                        implicitWidth: dlLabel.implicitWidth + 24; height: 32; radius: 6
                        color: dlMouse.containsMouse ? "#3a5ed0" : "#2a4590"
                        Behavior on color { ColorAnimation { duration: 150 } }
                        Text {
                            id: dlLabel
                            anchors.centerIn: parent
                            text: "开始下载"; color: "#e8ecf8"
                            font.pixelSize: 13; font.weight: Font.DemiBold
                        }
                        MouseArea {
                            id: dlMouse
                            anchors.fill: parent; hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                var targetName = root.customName !== "" ? root.customName : root.defaultName
                                if (backend) backend.logMessage("[install] download clicked: " + targetName)
                            }
                        }
                    }
                }
            }

            // ═══ SECTION LABEL ═══
            Text {
                text: "Mod 加载器"; font.pixelSize: 14; font.weight: Font.DemiBold; color: "#a0a8c0"
                Layout.topMargin: 8; Layout.leftMargin: 4
            }

            // ═══ CARD 2 — Forge ═══
            ModLoaderCard {
                Layout.fillWidth: true; title: "Forge"; loaderKey: "forge"
                versions: root.forgeVersions
                disabled: root.hasModLoader && root.activeLoader !== "forge"
                disabledReason: root.activeLoader === "neoforge" ? "NeoForge 已选"
                              : root.activeLoader === "fabric" ? "Fabric 已选"
                              : root.selectedOptifine ? "与当前 Optifine 不兼容" : ""
                selectedVersion: root.selectedForge
                onVersionSelected: function(ver) {
                    root.selectedForge = ver; root.activeLoader = "forge"
                }
                onVersionCleared: {
                    root.selectedForge = ""; root.activeLoader = ""
                    root.customName = ""
                }
            }

            // ═══ CARD 3 — NeoForge ═══
            ModLoaderCard {
                Layout.fillWidth: true; title: "NeoForge"; loaderKey: "neoforge"
                versions: root.neoforgeVersions
                disabled: root.hasModLoader && root.activeLoader !== "neoforge"
                disabledReason: root.activeLoader === "forge" ? "Forge 已选"
                              : root.activeLoader === "fabric" ? "Fabric 已选"
                              : root.selectedOptifine ? "Optifine 不兼容 NeoForge" : ""
                selectedVersion: root.selectedNeoForge
                onVersionSelected: function(ver) {
                    root.selectedNeoForge = ver; root.activeLoader = "neoforge"
                }
                onVersionCleared: {
                    root.selectedNeoForge = ""; root.activeLoader = ""
                    root.customName = ""
                }
            }

            // ═══ CARD 4 — Fabric ═══
            ModLoaderCard {
                Layout.fillWidth: true; title: "Fabric"; loaderKey: "fabric"
                versions: root.fabricVersions
                disabled: root.hasModLoader && root.activeLoader !== "fabric"
                disabledReason: root.activeLoader === "forge" ? "Forge 已选"
                              : root.activeLoader === "neoforge" ? "NeoForge 已选"
                              : root.selectedOptifine ? "Optifine 不兼容 Fabric" : ""
                selectedVersion: root.selectedFabric
                onVersionSelected: function(ver) {
                    root.selectedFabric = ver; root.activeLoader = "fabric"
                }
                onVersionCleared: {
                    root.selectedFabric = ""; root.activeLoader = ""
                    root.customName = ""
                }
            }

            // ═══ CARD 6 — Fabric API ═══
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

            // ═══ CARD 5 — Optifine ═══
            ModLoaderCard {
                Layout.fillWidth: true; title: "Optifine"; loaderKey: "optifine"
                versions: root.optifineVersions
                disabled: (root.hasModLoader && root.selectedOptifine === "")
                         || root.activeLoader === "fabric" || root.activeLoader === "neoforge"
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
