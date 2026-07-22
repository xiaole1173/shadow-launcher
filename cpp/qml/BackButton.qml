// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick
import QtQuick.Controls.Basic

/*
 * Unified back button component.
 * Styling: 30px, SVG arrow + text, #1a2440 hover bg / #6080e8 text
 * Usage:
 *   BackButton { onClicked: stack.pop() }
 */
Rectangle {
    id: root
    width: implicitWidth
    height: 30
    radius: StyleTokens.radiusSm
    color: mouseArea.containsMouse ? "#1a2440" : "transparent"
    border.width: 0

    property string text: qsTr("返回")
    signal clicked()

    Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }

    implicitWidth: row.implicitWidth + 14

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 4

        Image {
            anchors.verticalCenter: parent.verticalCenter
            source: "icons/lucide/arrow-left.svg"
            width: 16; height: 16
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.text
            font.pixelSize: StyleTokens.fontSizeSm
            color: mouseArea.containsMouse ? StyleTokens.accentLight : StyleTokens.textSecondary
            Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        onClicked: {
            root.clicked()
        }
    }
}
