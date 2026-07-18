// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: card
    Layout.fillWidth: true
    Layout.preferredHeight: cardHeight
    radius: StyleTokens.radiusLg
    color: cardDisabled ? "#0e1018" : (cardHovered ? "#161a26" : "#11141c")
    border.color: cardDisabled ? "#1a1a24" : (cardHovered ? "#3a50b0" : "#1e2230")
    border.width: cardHovered ? 1.5 : 1
    opacity: cardDisabled ? 0.55 : 1
    scale: cardDisabled ? 0.98 : (cardHovered ? 1.01 : 1.0)

    Behavior on color { ColorAnimation { duration: 200; easing.type: Easing.OutCubic } }
    Behavior on border.color { ColorAnimation { duration: 200 } }
    Behavior on opacity { NumberAnimation { duration: 200 } }
    Behavior on scale { NumberAnimation { duration: 250; easing.type: Easing.OutBack } }
    Behavior on Layout.preferredHeight { NumberAnimation { duration: 350; easing.type: Easing.OutCubic } }

    property string title: ""
    property string loaderKey: ""
    property bool disabled: false
    property string disabledReason: ""
    property string selectedVersion: ""
    property var versions: []
    onVersionsChanged: { if (versions.length === 0) expanded = false }
    property var versionEnabled: function(ver) { return true }  // per-item enable/disable for OptiFine-Forge compat
    property bool expanded: false
    property bool cardHovered: false
    property bool cardDisabled: disabled
    onCardDisabledChanged: { if (cardDisabled) expanded = false }

    property string badgeText: ""  // Optional badge after title, e.g. "建议安装"

    signal versionSelected(string version, string installerSha1)
    signal versionCleared()

    // Fixed-height version list — no layout feedback loop
    property int headerHeight: 44
    property int itemRowH: 40
    property int maxListHeight: 280
    property int listHeight: Math.min(versions.length * itemRowH, maxListHeight)
    property int cardHeight: expanded ? headerHeight + listHeight + 8 : headerHeight

    function typeLabel(t) {
        if (t === "release") return "正式版"
        if (t === "beta") return "测试版"
        if (t === "alpha") return "预览版"
        if (t === "snapshot") return "快照版"
        if (t === "preview") return "预览版"
        return t
    }

    function typeColor(t) {
        if (t === "release") return "#3a8050"
        if (t === "beta") return "#e0a040"
        if (t === "alpha") return "#a060d0"
        if (t === "snapshot") return "#40a0c0"
        if (t === "preview") return "#d06080"
        return "#505868"
    }

    Rectangle {
        id: header
        anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
        height: headerHeight; color: "transparent"; radius: StyleTokens.radiusLg

        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 14; anchors.rightMargin: 10; spacing: 8
            Rectangle { width: 10; height: 10; radius: StyleTokens.radiusSm; color: cardDisabled ? "#404858" : selectedVersion ? "#4bc870" : "#505868" }
            Text { text: card.title; font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.DemiBold; color: cardDisabled ? "#687080" : "#e4e8f2" }
            // Optional badge (e.g. "建议安装")
            Rectangle {
                visible: card.badgeText !== "" && !cardDisabled
                implicitWidth: badgeLabel.implicitWidth + 12; implicitHeight: 20; radius: StyleTokens.radiusSm
                color: "#1a2e1a"; border.color: "#3a6830"; border.width: 1
                Text { id: badgeLabel; anchors.centerIn: parent; text: card.badgeText; font.pixelSize: StyleTokens.fontSizeXs; color: "#60b050" }
            }
            Text {
                visible: cardDisabled && disabledReason
                text: cardDisabled ? "[X] " + disabledReason : ""
                font.pixelSize: StyleTokens.fontSizeSm; color: "#c06050"; elide: Text.ElideRight; Layout.maximumWidth: 200
            }
            Item { Layout.fillWidth: true }
            Text { text: selectedVersion || (cardDisabled ? "" : (versions.length === 0 ? "无可用版本" : "未选择")); font.pixelSize: StyleTokens.fontSizeSm; color: selectedVersion ? "#8aa8f0" : "#606878" }
            Text {
                text: card.expanded ? "\u25B2" : "\u25BC"; font.pixelSize: StyleTokens.fontSizeXs; color: "#606878"
                visible: !cardDisabled && (card.versions.length > 0 || card.expanded)
            }
            Text {
                visible: selectedVersion !== ""
                text: "\u2715"; font.pixelSize: StyleTokens.fontSizeMd
                color: cancelArea.containsMouse ? "#e06060" : "#787c90"
                Behavior on color { ColorAnimation { duration: 150 } }
                TapHandler {
                    id: cancelArea; cursorShape: Qt.PointingHandCursor
                    onTapped: { card.versionCleared() }
                }
            }
        }

        TapHandler { enabled: !cardDisabled; cursorShape: Qt.PointingHandCursor; onTapped: { card.expanded = !card.expanded } }
        HoverHandler { enabled: !cardDisabled; onHoveredChanged: { card.cardHovered = hovered } }
    }

    // Version list — inside fixed-height container
    Item {
        id: listContainer
        anchors.top: header.bottom; anchors.left: parent.left; anchors.right: parent.right
        anchors.leftMargin: 14; anchors.rightMargin: 14
        height: listHeight
        visible: card.expanded && !cardDisabled
        clip: true

        ListView {
            id: versionListView
            anchors.fill: parent; spacing: 4
            model: card.versions
            interactive: listHeight >= maxListHeight  // Scroll only when content exceeds
            boundsBehavior: Flickable.StopAtBounds

            delegate: Rectangle {
                // Read per-item enabled BEFORE binding (capture outside)
                readonly property bool itemEnabled: card.versionEnabled(modelData.version)
                width: versionListView.width; height: itemRowH; radius: StyleTokens.radiusMd
                opacity: itemEnabled ? 1.0 : 0.35
                color: hoverDeleg.containsMouse ? (itemEnabled ? "#1a2440" : "#151520") : "transparent"
                border.color: card.selectedVersion === modelData.version ? "#3a50b0" : "transparent"
                border.width: card.selectedVersion === modelData.version ? 1 : 0

                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8; spacing: 8
                    Text { text: modelData.version || "Loading..."; font.pixelSize: StyleTokens.fontSizeMd; color: "#e4e8f2"; Layout.fillWidth: true; elide: Text.ElideRight }
                    // "Latest" badge
                    Rectangle {
                        visible: modelData.isLatestRelease !== undefined && modelData.isLatestRelease
                        height: 18; implicitWidth: latestTag.implicitWidth + 10; radius: StyleTokens.radiusXs; color: "#3a50b0"
                        Text { id: latestTag; anchors.centerIn: parent; text: qsTr("最新"); font.pixelSize: StyleTokens.fontSizeXs; color: "#e8ecf8"; font.weight: Font.Bold }
                    }
                    Rectangle {
                        visible: modelData.type !== undefined && modelData.type !== ""
                        height: 18; implicitWidth: tagImp.implicitWidth + 10; radius: StyleTokens.radiusXs; color: card.typeColor(modelData.type)
                        Text { id: tagImp; anchors.centerIn: parent; text: card.typeLabel(modelData.type); font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.textPrimary }
                    }
                    Text { text: modelData.date || ""; font.pixelSize: StyleTokens.fontSizeSm; color: "#787c90" }
                }
                HoverHandler { id: hoverDeleg; enabled: itemEnabled }
                TapHandler { cursorShape: itemEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor; onTapped: { if (itemEnabled) { card.versionSelected(modelData.version, modelData.installerSha1 || ""); card.expanded = false } } }
            }

            Text {
                visible: card.expanded && card.versions.length === 0
                text: card.disabledReason ? card.disabledReason : "暂无可用版本"
                font.pixelSize: StyleTokens.fontSizeSm; color: "#606878"; x: 0; y: 4; width: parent.width
            }
        }
    }
}
