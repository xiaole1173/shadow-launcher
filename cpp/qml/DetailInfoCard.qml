// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

// DetailInfoCard
// Unified info card used by ModDetailPage and ResourcePackDetailPage
// Style matches InstallPage: #11141c / #1e2230 / radius 10

Rectangle {
    id: card

    // ── Card geometry ──
    Layout.fillWidth: true
    implicitHeight: mainCol.implicitHeight + 24
    radius: 10
    color: "#11141c"
    border.color: "#1e2230"
    border.width: 1

    // ── Data properties ──
    property string cardIcon: ""
    property string cardTitle: ""
    property string cardDesc: ""

    // ── Content injection via default property ──
    default property alias infoItems: infoExtraCol.data

    // ── Icon fallback letter ──
    function fallbackLetter() {
        if (cardTitle) return cardTitle[0]
        return "?"
    }

    ColumnLayout {
        id: mainCol
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 14
        spacing: 10

        // Row 1: Icon + Title + Description
        RowLayout {
            Layout.fillWidth: true
            spacing: 14

            // Icon
            Rectangle {
                width: 48; height: 48; radius: 10
                color: "#1a1f2e"
                Layout.preferredWidth: 48; Layout.preferredHeight: 48
                clip: true

                Image {
                    id: detailIconImg
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true; cache: true
                    sourceSize.width: 96; sourceSize.height: 96
                    source: cardIcon ? cardIcon.replace("cdn.modrinth.com", "mod.mcimirror.top") : ""
                    onStatusChanged: {
                        if (status === Image.Error) detailIconFb.visible = true
                    }
                }
                Text {
                    id: detailIconFb
                    anchors.centerIn: parent
                    text: fallbackLetter()
                    color: "#5068c8"
                    font.pixelSize: 22; font.weight: Font.Bold
                    visible: !cardIcon
                }
            }

            // Title + Description
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3

                Text {
                    Layout.fillWidth: true
                    text: cardTitle || "未命名"
                    color: "#d0d4e0"
                    font.pixelSize: 18; font.weight: Font.Bold
                    elide: Text.ElideRight
                }
                Text {
                    Layout.fillWidth: true
                    text: cardDesc || "无简介"
                    color: "#7888a8"
                    font.pixelSize: 10
                    maximumLineCount: 2
                    wrapMode: Text.WordWrap
                    elide: Text.ElideRight
                }
            }
        }

        // Row 2: Page-specific info items (stats, toggles, action buttons)
        ColumnLayout {
            id: infoExtraCol
            Layout.fillWidth: true
            spacing: 6
            // Children injected from caller via default property
        }
    }
}
