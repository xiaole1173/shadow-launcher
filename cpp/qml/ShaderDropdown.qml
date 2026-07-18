// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    height: 28; radius: StyleTokens.radiusSm
    color: (hov.containsMouse || menu.visible) ? "#1e3260" : "#0c0e14"
    border.color: (hov.containsMouse || menu.visible) ? "#5078e0" : "#2a3040"; border.width: 1

    property var ddOptions: []
    property string ddPropName: ""
    property QtObject owner: null
    readonly property string ddCurrent: owner ? owner[ddPropName] || "" : ""

    Behavior on color { ColorAnimation { duration: 150 } }
    Behavior on border.color { ColorAnimation { duration: 150 } }

    function selectedLabel() {
        for (var j = 0; j < ddOptions.length; j++) {
            if (ddOptions[j].slug === ddCurrent) return ddOptions[j].label
        }
        return ddOptions.length > 0 ? ddOptions[0].label : ""
    }

    RowLayout {
        anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 4; spacing: 4
        Text { Layout.fillWidth: true; text: root.selectedLabel(); color: "#788db0"; font.pixelSize: StyleTokens.fontSizeSm; elide: Text.ElideRight }
        Text { text: "▾"; color: "#505468"; font.pixelSize: StyleTokens.fontSizeXs }
    }

    MouseArea {
        id: hov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
        onClicked: { menu.open() }
    }

    Popup {
        id: menu; y: parent.height + 4; width: Math.max(parent.width, 120)
        padding: 4; closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
        background: Rectangle { color: "#0f1118"; border.color: "#2a3040"; radius: StyleTokens.radiusMd }
        contentItem: ColumnLayout {
            spacing: 2
            Repeater {
                model: root.ddOptions
                Rectangle {
                    Layout.fillWidth: true; height: 26; radius: StyleTokens.radiusSm
                    color: itemHov.containsMouse ? "#1a2a50" : "transparent"
                    Text {
                        anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left; anchors.leftMargin: 10
                        text: modelData.label
                        color: root.ddCurrent === modelData.slug ? "#8ab4f8" : "#8890a0"
                        font.pixelSize: StyleTokens.fontSizeSm
                        font.weight: root.ddCurrent === modelData.slug ? Font.DemiBold : Font.Normal
                    }
                    MouseArea {
                        id: itemHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (root.owner) root.owner[root.ddPropName] = modelData.slug
                            menu.close()
                        }
                    }
                }
            }
        }
    }
}
