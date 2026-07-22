// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick
import QtQuick.Controls.Basic

/*
 * Unified refresh button component.
 * Styling: 30×30, SVG refresh-cw icon, #141820→#222a3a hover
 * Usage:
 *   RefreshButton { onClicked: model.refresh() }
 */
Rectangle {
    id: root
    width: 30
    height: 30
    radius: StyleTokens.radiusMd
    color: mouseArea.containsMouse ? "#222a3a" : "#141820"
    border.color: StyleTokens.bgHover
    border.width: 1

    signal clicked()

    Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
    Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }

    Image {
        anchors.centerIn: parent
        source: "icons/lucide/refresh-cw.svg"
        width: 16; height: 16
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        scale: mouseArea.containsMouse ? 1.08 : 1.0
        onClicked: root.clicked()
    }
}
