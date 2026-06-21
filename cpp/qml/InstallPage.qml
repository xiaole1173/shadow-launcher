import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "qrc:/ShadowLauncher/qml"

// ─── InstallPage ───
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

    // ── Selected mod loader state ──
    property string selectedForge: ""
    property string selectedNeoForge: ""
    property string selectedFabric: ""
    property string selectedOptifine: ""
    property string selectedFabricApi: ""
    property string activeLoader: ""

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

    // ═══ TOP BAR ═══
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
                    text: "\u2190 Back"
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

            // Download button (placeholder)
            Rectangle {
                width: dlLabel.implicitWidth + 24; height: 30; radius: 6
                color: dlMouse.containsMouse ? "#3a5ed0" : "#2a4590"
                Behavior on color { ColorAnimation { duration: 150 } }
                Text {
                    id: dlLabel
                    anchors.centerIn: parent
                    text: "Start Download"
                    color: "#e8ecf8"; font.pixelSize: 13; font.weight: Font.DemiBold
                }
                MouseArea {
                    id: dlMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (backend) backend.logMessage("[install] download clicked: " + root.fullVersionName)
                    }
                }
            }
        }
    }

    // ═══ CONTENT AREA ═══
    // Use Flickable instead of ScrollView for more reliable child sizing
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

            // ═══ CARD 1 — MC Version ═══
            Rectangle {
                Layout.fillWidth: true; height: 64; radius: 10
                color: "#11141c"; border.color: "#1e2230"; border.width: 1
                RowLayout {
                    anchors.fill: parent; anchors.margins: 16; spacing: 12
                    ColumnLayout {
                        spacing: 2; Layout.fillWidth: true
                        Text {
                            text: "Minecraft " + mcVersion
                            font.pixelSize: 18; font.weight: Font.Bold; color: "#e4e8f2"
                        }
                        Text {
                            visible: root.fullVersionName !== root.mcVersion
                            text: "Full name: " + root.fullVersionName
                            font.pixelSize: 12; color: "#787c90"
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            // ═══ SECTION LABEL ═══
            Text {
                text: "Mod Loader"
                font.pixelSize: 14; font.weight: Font.DemiBold; color: "#a0a8c0"
                Layout.topMargin: 8; Layout.leftMargin: 4
            }

            // ═══ CARD 2 — Forge ═══
            ModLoaderCard {
                Layout.fillWidth: true
                title: "Forge"
                loaderKey: "forge"
                disabled: root.hasModLoader && root.activeLoader !== "forge"
                disabledReason: root.activeLoader === "neoforge" ? "NeoForge selected"
                              : root.activeLoader === "fabric" ? "Fabric selected"
                              : root.selectedOptifine ? "Incompatible with this OptiFine" : ""
                selectedVersion: root.selectedForge
                onVersionSelected: function(ver) {
                    if (backend) backend.logMessage("[install] Forge selected: " + ver)
                    root.selectedForge = ver
                    root.activeLoader = "forge"
                }
            }

            // ═══ CARD 3 — NeoForge ═══
            ModLoaderCard {
                Layout.fillWidth: true
                title: "NeoForge"
                loaderKey: "neoforge"
                disabled: root.hasModLoader && root.activeLoader !== "neoforge"
                disabledReason: root.activeLoader === "forge" ? "Forge selected"
                              : root.activeLoader === "fabric" ? "Fabric selected"
                              : root.selectedOptifine ? "Incompatible with OptiFine" : ""
                selectedVersion: root.selectedNeoForge
                onVersionSelected: function(ver) {
                    root.selectedNeoForge = ver
                    root.activeLoader = "neoforge"
                }
            }

            // ═══ CARD 4 — Fabric ═══
            ModLoaderCard {
                Layout.fillWidth: true
                title: "Fabric"
                loaderKey: "fabric"
                disabled: root.hasModLoader && root.activeLoader !== "fabric"
                disabledReason: root.activeLoader === "forge" ? "Forge selected"
                              : root.activeLoader === "neoforge" ? "NeoForge selected"
                              : root.selectedOptifine ? "Incompatible with OptiFine" : ""
                selectedVersion: root.selectedFabric
                onVersionSelected: function(ver) {
                    root.selectedFabric = ver
                    root.activeLoader = "fabric"
                }
            }

            // ═══ CARD 6 — Fabric API (conditional) ═══
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
                    Text { text: root.selectedFabricApi || "Unselected"; font.pixelSize: 12; color: "#9498a8" }
                }
            }

            // ═══ CARD 5 — Optifine ═══
            ModLoaderCard {
                Layout.fillWidth: true
                title: "Optifine"
                loaderKey: "optifine"
                disabled: (root.hasModLoader && root.selectedOptifine === "")
                         || root.activeLoader === "fabric"
                         || root.activeLoader === "neoforge"
                disabledReason: root.activeLoader === "fabric" ? "Fabric incompatible with OptiFine"
                              : root.activeLoader === "neoforge" ? "OptiFine unsupported on NeoForge"
                              : root.hasModLoader && root.selectedOptifine === "" ? "Select a loader first" : ""
                selectedVersion: root.selectedOptifine
                onVersionSelected: function(ver) {
                    root.selectedOptifine = ver
                }
            }

            Item { Layout.fillWidth: true; height: 40 }
        }
    }
}
