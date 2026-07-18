// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

// DetailVersionCard
// Unified version card used by ModDetailPage and ResourcePackDetailPage
// Features: version label, tags row, info lines, download button, L3 expand

Rectangle {
    id: card

    Layout.fillWidth: true
    implicitHeight: Math.max(52, contentLayout.implicitHeight + 20 + (card.showExpand && card.expanded ? l3Container.implicitHeight + 16 : 0))
    radius: StyleTokens.radiusLg
    color: card.cardHovered ? "#161a26" : "#11141c"
    border.color: card.cardHovered ? "#4068c8" : "#1e2230"
    border.width: card.cardHovered ? 1.5 : 1

    // ── Opacity fade-in entrance ──
    opacity: 0
    Component.onCompleted: { opacity = 1 }

    // ── Elastic click feedback ──
    property real _clickScale: 1.0
    scale: _clickScale
    Timer { id: clickRestoreTimer; interval: 100
        onTriggered: { card._clickScale = 1.0 }
    }

    // ── Data properties ──
    property string versionLabel: ""        // version number string
    property var tags: []                  // [{text, color, bg}] — loader/prerelease tags
    property var infoLines: []             // [{label, value}] — info rows below version
    property bool hasDownload: false       // enable click-to-download

    // ── L3 expand (RP detail) ──
    property bool showExpand: false        // show expand toggle
    property bool expanded: false          // current expand state
    property var l3Detail: null            // expanded content data

    signal downloadClicked()
    signal expandToggled()

    // ── Animations ──
    Behavior on color { ColorAnimation { duration: 200 } }
    Behavior on border.color { ColorAnimation { duration: 200 } }
    Behavior on border.width { NumberAnimation { duration: 150 } }
    Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
    Behavior on implicitHeight { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
    Behavior on _clickScale {
        SpringAnimation { spring: 1.8; damping: 0.25; epsilon: 0.01 }
    }

    // ── Hover state (replaces MouseArea.containsMouse) ──
    property bool cardHovered: false

    // ── Hover gradient overlay ──
    Rectangle {
        anchors.fill: parent; radius: parent.radius
        opacity: card.cardHovered ? 0.12 : 0
        gradient: Gradient {
            GradientStop { position: 0; color: StyleTokens.accentHover }
            GradientStop { position: 1; color: StyleTokens.accentLight }
        }
        Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
    }

    HoverHandler {
        id: cardHov
        onHoveredChanged: card.cardHovered = hovered
    }

    TapHandler {
        cursorShape: hasDownload ? Qt.PointingHandCursor : Qt.ArrowCursor
        onTapped: {
            if (hasDownload) {
                card._clickScale = 0.92
                clickRestoreTimer.restart()
                downloadClicked()
            }
            if (showExpand) {
                expandToggled()
            }
        }
    }

    // ── Layout ──
    ColumnLayout {
        id: contentLayout
        anchors.fill: parent
        anchors.margins: 10
        spacing: 4

        // Main row: version label + tags
        RowLayout {
            id: mainRow
            Layout.fillWidth: true
            spacing: 6

            // Version label
            Text {
                text: card.versionLabel
                color: "#d0d4e0"
                font.pixelSize: StyleTokens.fontSizeSm; font.weight: Font.DemiBold
                Layout.alignment: Qt.AlignVCenter
                Layout.fillWidth: true
                elide: Text.ElideRight
            }

            // Tags row (wrapped, no download button)
            Row {
                id: tagRow
                spacing: 3
                Repeater {
                    model: card.tags
                    delegate: Rectangle {
                        width: tagText.implicitWidth + 8; height: 16
                        radius: StyleTokens.radiusXs; color: modelData.bg || "#1a2848"
                        Text {
                            id: tagText
                            anchors.centerIn: parent
                            text: modelData.text
                            color: modelData.color || "#8aaeff"
                            font.pixelSize: StyleTokens.fontSizeXs
                        }
                    }
                }
            }

            Item { Layout.fillWidth: true }
        }

        // Info lines (e.g. "MC: 1.21, 1.21.1", "2024-01-15  |  下载量 12.5K")
        Repeater {
            model: card.infoLines
            delegate: RowLayout {
                Layout.fillWidth: true
                spacing: 3
                Text {
                    text: modelData.label + " "
                    color: "#606478"; font.pixelSize: StyleTokens.fontSizeXs
                    visible: modelData.label !== ""
                }
                Text {
                    text: modelData.value
                    color: "#788090"; font.pixelSize: StyleTokens.fontSizeXs
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
            }
        }
    }

    // ── L3 Detail (expandable, for RP) ──
    // Injected via default property alias from parent page
    // Usage: DetailVersionCard { ... L3Content { ... } }
    default property alias expandedContent: l3Container.data
    Item {
        id: l3Container
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: mainRow.bottom
        anchors.topMargin: 8
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        visible: card.showExpand && card.expanded
        height: visible ? l3Container.implicitHeight : 0
        Behavior on height { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
    }
}
