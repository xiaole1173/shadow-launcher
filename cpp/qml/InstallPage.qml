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
    z: 10

    property var backend: null
    property string mcVersion: ""
    property var toastManager: null
    signal goBack()

    Component.onCompleted: { console.log("[install] InstallPage loaded, mcVersion=" + mcVersion) }

    // ── Selected mod loader state ──
    property string selectedForge: ""
    property string selectedNeoForge: ""
    property string selectedFabric: ""
    property string selectedOptifine: ""
    property string selectedFabricApi: ""
    property string activeLoader: ""  // "forge" | "neoforge" | "fabric"

    // Derived: whether any non-optifine loader is selected
    property bool hasModLoader: activeLoader !== ""

    // Derived: selected version name suffix
    property string versionSuffix: {
        var parts = []
        if (activeLoader === "forge" && selectedForge) parts.push("Forge-" + selectedForge)
        if (activeLoader === "neoforge" && selectedNeoForge) parts.push("NeoForge-" + selectedNeoForge)
        if (activeLoader === "fabric" && selectedFabric) parts.push("Fabric-" + selectedFabric)
        if (selectedOptifine) parts.push("OptiFine-" + selectedOptifine)
        return parts.join("-")
    }

    property string fullVersionName: mcVersion + (versionSuffix ? "-" + versionSuffix : "")

    // ── Scrollable content ──
    ScrollView {
        anchors.fill: parent
        anchors.topMargin: 52
        clip: true
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        ColumnLayout {
            width: parent ? Math.min(parent.width - 32, 680) : 680
            anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
            spacing: 16

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
                            text: fullVersionName !== mcVersion ? "版本名称: " + fullVersionName : ""
                            font.pixelSize: 12; color: "#787c90"
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            // ═══ SECTION LABEL: Mod Loader ═══
            Text {
                text: "Mod Loader"
                font.pixelSize: 14; font.weight: Font.DemiBold; color: "#a0a8c0"
                Layout.leftMargin: 4; Layout.topMargin: 8
            }

            // ═══ CARD 2 — Forge ═══
            ModLoaderCard {
                title: "Forge"
                loaderKey: "forge"
                disabled: root.hasModLoader && root.activeLoader !== "forge"
                disabledReason: root.activeLoader === "neoforge" ? "NeoForge 已选"
                              : root.activeLoader === "fabric" ? "Fabric 已选"
                              : root.selectedOptifine ? "Optifine 仅兼容特定 Forge 版本" : ""
                selectedVersion: root.selectedForge
                onVersionSelected: function(ver) {
                    root.selectedForge = ver
                    root.activeLoader = "forge"
                }
            }

            // ═══ CARD 3 — NeoForge ═══
            ModLoaderCard {
                title: "NeoForge"
                loaderKey: "neoforge"
                disabled: root.hasModLoader && root.activeLoader !== "neoforge"
                disabledReason: root.activeLoader === "forge" ? "Forge 已选"
                              : root.activeLoader === "fabric" ? "Fabric 已选"
                              : root.selectedOptifine ? "Optifine 不兼容 NeoForge" : ""
                selectedVersion: root.selectedNeoForge
                onVersionSelected: function(ver) {
                    root.selectedNeoForge = ver
                    root.activeLoader = "neoforge"
                }
            }

            // ═══ CARD 4 — Fabric ═══
            ModLoaderCard {
                title: "Fabric"
                loaderKey: "fabric"
                disabled: root.hasModLoader && root.activeLoader !== "fabric"
                disabledReason: root.activeLoader === "forge" ? "Forge 已选"
                              : root.activeLoader === "neoforge" ? "NeoForge 已选"
                              : root.selectedOptifine ? "Optifine 不兼容 Fabric" : ""
                selectedVersion: root.selectedFabric
                onVersionSelected: function(ver) {
                    root.selectedFabric = ver
                    root.activeLoader = "fabric"
                }
            }

            // ═══ CARD 6 — Fabric API (conditional) ═══
            Rectangle {
                Layout.fillWidth: true; height: root.activeLoader === "fabric" ? 56 : 0
                visible: root.activeLoader === "fabric"
                color: "#11141c"; radius: 8; border.color: "#1e2230"; border.width: 1
                clip: true
                Behavior on height { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
                RowLayout {
                    anchors.fill: parent; anchors.margins: 14; spacing: 8
                    Rectangle {
                        width: 8; height: 8; radius: 4
                        color: root.selectedFabricApi ? "#4bc870" : "#505868"
                    }
                    Text { text: "Fabric API"; font.pixelSize: 14; font.weight: Font.DemiBold; color: "#e4e8f2" }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: root.selectedFabricApi || "未选择"; font.pixelSize: 12; color: "#9498a8"
                    }
                    Text { text: "\u25B6"; font.pixelSize: 10; color: "#606878" }
                }
            }

            // ═══ CARD 5 — Optifine ═══
            ModLoaderCard {
                title: "Optifine"
                loaderKey: "optifine"
                disabled: (root.hasModLoader && root.selectedOptifine === "")
                         || (root.activeLoader === "fabric")
                         || (root.activeLoader === "neoforge")
                disabledReason: root.activeLoader === "fabric" ? "Fabric 不兼容 Optifine"
                              : root.activeLoader === "neoforge" ? "Optifine 不支持 NeoForge"
                              : root.hasModLoader && root.selectedOptifine === "" ? "Optifine 暂未选择" : ""
                selectedVersion: root.selectedOptifine
                onVersionSelected: function(ver) {
                    root.selectedOptifine = ver
                    // Optifine selected → disable mod loaders if Optifine is standalone or doesn't match
                }
            }

            // ═══ BOTTOM SPACER ═══
            Item { Layout.fillWidth: true; height: 40 }
        }
    }

    // ═══ TOP BAR ═══
    Rectangle {
        anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
        height: 44; color: "#0c0f16"; z: 11

        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12; spacing: 10

            // Back button
            Rectangle {
                width: backText.implicitWidth + 16; height: 30; radius: 6
                color: backArea.containsMouse ? "#1A222D" : "transparent"
                Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
                Text {
                    id: backText
                    anchors.centerIn: parent
                    text: "\u2190 返回"
                    color: backArea.containsMouse ? "#3B82F6" : "#B4BAC6"
                    font.pixelSize: 13
                }
                MouseArea {
                    id: backArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.goBack()
                }
            }

            // Version name (centered)
            Item { Layout.fillWidth: true }
            Text {
                text: mcVersion
                font.pixelSize: 16; font.weight: Font.Bold; color: "#e4e8f2"
            }
            Item { Layout.fillWidth: true }

            // Download button
            Rectangle {
                width: downloadText.implicitWidth + 24; height: 30; radius: 6
                color: downloadArea.containsMouse ? "#3a4eb8" : "#2a3878"
                Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
                Text {
                    id: downloadText
                    anchors.centerIn: parent
                    text: "Start Download"
                    color: "#e8ecf8"; font.pixelSize: 13; font.weight: Font.DemiBold
                }
                MouseArea {
                    id: downloadArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        console.log("[install] download clicked, version=" + root.fullVersionName)
                    }
                }
            }
        }
    }
}
