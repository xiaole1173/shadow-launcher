import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "qrc:/ShadowLauncher/qml"

// ─── ModLoaderCard ───
// Clickable card for a mod loader (Forge/NeoForge/Fabric/Optifine)
// Supports: disabled state, expansion to show version list, version selection

Rectangle {
    id: card
    Layout.fillWidth: true
    height: cardHeight
    radius: 8
    color: cardDisabled ? "#0e1018" : (cardHovered ? "#161a26" : "#11141c")
    border.color: cardDisabled ? "#1a1a24" : "#1e2230"
    border.width: 1
    opacity: cardDisabled ? 0.55 : 1
    scale: cardDisabled ? 0.98 : (cardHovered ? 1.01 : 1.0)

    Behavior on color { ColorAnimation { duration: 200; easing.type: Easing.OutCubic } }
    Behavior on border.color { ColorAnimation { duration: 200 } }
    Behavior on opacity { NumberAnimation { duration: 200 } }
    Behavior on scale { NumberAnimation { duration: 250; easing.type: Easing.OutBack } }
    Behavior on height { NumberAnimation { duration: 350; easing.type: Easing.OutCubic } }

    // ── Properties ──
    property string title: ""
    property string loaderKey: ""
    property bool disabled: false
    property string disabledReason: ""
    property string selectedVersion: ""
    property var versions: []
    property bool expanded: false
    property bool cardHovered: false
    property bool cardDisabled: disabled

    signal versionSelected(string version)

    // ── Computed ──
    property int headerHeight: 44
    property int cardHeight: expanded ? headerHeight + versionListContent.height + 8 : headerHeight

    // ═══ HEADER ROW ═══
    Rectangle {
        id: header
        anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
        height: headerHeight; color: "transparent"; radius: 8

        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 14; anchors.rightMargin: 10; spacing: 8

            // Indicator dot
            Rectangle {
                width: 10; height: 10; radius: 5
                color: cardDisabled ? "#404858"
                     : selectedVersion ? "#4bc870"
                     : "#505868"
            }

            // Title
            Text {
                text: card.title
                font.pixelSize: 14; font.weight: Font.DemiBold
                color: cardDisabled ? "#687080" : "#e4e8f2"
            }

            // Disabled reason
            Text {
                visible: cardDisabled && disabledReason
                text: cardDisabled ? (card.title === "Optifine" ? "[X] " + disabledReason : "[X] " + disabledReason) : ""
                font.pixelSize: 11; color: "#c06050"
                elide: Text.ElideRight
                Layout.maximumWidth: 200
            }

            Item { Layout.fillWidth: true }

            // Selected version or placeholder
            Text {
                text: selectedVersion || (cardDisabled ? "" : "Unselected")
                font.pixelSize: 12; color: selectedVersion ? "#8aa8f0" : "#606878"
            }

            // Expand arrow
            Text {
                text: card.expanded ? "\u25B2" : "\u25BC"
                font.pixelSize: 10; color: "#606878"
                visible: !cardDisabled && card.versions.length > 0
            }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: cardDisabled ? Qt.ArrowCursor : Qt.PointingHandCursor
            enabled: !cardDisabled
            onClicked: {
                if (cardDisabled) return
                card.expanded = !card.expanded
            }
            onEntered: card.cardHovered = true
            onExited: card.cardHovered = false
        }
    }

    // ═══ EXPANDED VERSION LIST ═══
    ColumnLayout {
        id: versionListContent
        anchors.top: header.bottom; anchors.left: parent.left; anchors.right: parent.right
        anchors.leftMargin: 14; anchors.rightMargin: 14
        spacing: 4
        visible: card.expanded && !cardDisabled
        clip: true

        // ── Placeholder versions (will be replaced by real data in later phases) ──
        Repeater {
            model: card.versions.length > 0 ? card.versions : (card.expanded ? [1] : [])
            delegate: Rectangle {
                Layout.fillWidth: true; height: 40; radius: 6
                color: verHover.containsMouse ? "#1a2440" : "transparent"
                border.color: card.selectedVersion === modelData.version ? "#3a50b0" : "transparent"
                border.width: card.selectedVersion === modelData.version ? 1 : 0

                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8; spacing: 8
                    Text {
                        text: modelData.version || "Loading versions..."
                        font.pixelSize: 13; color: "#e4e8f2"
                        Layout.fillWidth: true
                    }
                    Rectangle {
                        visible: modelData.type !== undefined
                        height: 18; implicitWidth: typeTag.implicitWidth + 10; radius: 3
                        color: modelData.type === "release" ? "#3a8050"
                             : modelData.type === "beta" ? "#e0a040"
                             : "#505868"
                        Text {
                            id: typeTag
                            anchors.centerIn: parent
                            text: modelData.type === "release" ? "Release"
                                : modelData.type === "beta" ? "Beta"
                                : modelData.type || ""
                            font.pixelSize: 9; color: "#e8ecf8"
                        }
                    }
                    Text {
                        text: modelData.date || ""; font.pixelSize: 11; color: "#787c90"
                    }
                }

                HoverHandler { id: verHover }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        card.selectedVersion = modelData.version
                        card.versionSelected(modelData.version)
                        card.expanded = false
                    }
                }
            }
        }

        // Empty state
        Text {
            visible: card.expanded && card.versions.length === 0
            text: card.loaderKey === "optifine" && card.disabledReason ? card.disabledReason : "No versions available"
            font.pixelSize: 12; color: "#606878"
            Layout.topMargin: 4; Layout.bottomMargin: 8
        }
    }
}
