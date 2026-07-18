// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

// ExpandableGroupCard
// Reusable expandable card for version groups (Mod/RP detail pages)
// Design: ModLoaderCard-inspired — header bar + animated expansion

Rectangle {
    id: card
    Layout.fillWidth: true
    // height driven by ColumnLayout via preferredHeight; Behavior on height provides the animation
    property int cardHeight: headerH + (expanded ? contentColumn.implicitHeight + 8 : 0)
    Layout.preferredHeight: cardHeight
    clip: true; radius: StyleTokens.radiusLg
    color: _hovered ? "#161a26" : "#11141c"
    border.color: _hovered ? "#3a5ed0" : "#1e2230"
    border.width: _hovered ? 1.5 : 1

    // ── Public API ──
    property string title: ""
    property string subtitle: ""
    property bool expanded: false
    property bool _hovered: false

    // Children go into the content column
    default property alias content: contentColumn.data

    signal toggled()

    readonly property int headerH: 40

    // ── Animations ──
    Behavior on Layout.preferredHeight { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
    Behavior on color { ColorAnimation { duration: 200 } }
    Behavior on border.color { ColorAnimation { duration: 200 } }
    Behavior on border.width { NumberAnimation { duration: 150 } }

    // ── Header bar ──
    Rectangle {
        id: headerBar
        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: headerH; color: "transparent"

        // Hover gradient overlay
        Rectangle {
            anchors.fill: parent; radius: card.radius
            opacity: card._hovered ? 0.12 : 0
            gradient: Gradient {
                GradientStop { position: 0; color: StyleTokens.accentHover }
                GradientStop { position: 1; color: StyleTokens.accentLight }
            }
            Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
        }

        RowLayout {
            anchors.fill: parent; anchors.margins: 10; spacing: 10
            Text {
                text: (card.expanded ? "\u25BC" : "\u25B8") + "  " + card.title
                color: "#6080d8"; font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.Bold
            }
            Item { Layout.fillWidth: true }
            Text {
                visible: card.subtitle !== ""
                text: card.subtitle
                color: "#505468"; font.pixelSize: StyleTokens.fontSizeXs
            }
        }

        HoverHandler { onHoveredChanged: card._hovered = hovered }
        TapHandler { cursorShape: Qt.PointingHandCursor; onTapped: card.toggled() }
    }

    // ── Content area (always laid out, clipped by card height) ──
    Column {
        id: contentColumn
        anchors { top: headerBar.bottom; left: parent.left; right: parent.right; rightMargin: 8 }
    }
}
