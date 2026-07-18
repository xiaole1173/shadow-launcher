// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick

Rectangle {
    anchors.fill: parent
    color: StyleTokens.bgPrimary

    Item {
        anchors.centerIn: parent
        width: 280; height: 80

        // Loading bar — thin shimmer effect
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width; height: 2
            radius: StyleTokens.radiusXs
            color: StyleTokens.bgElevated

            Rectangle {
                height: parent.height; width: parent.width * 0.35; radius: StyleTokens.radiusXs
                color: StyleTokens.accent
                SequentialAnimation on x {
                    running: true; loops: Animation.Infinite
                    NumberAnimation { from: 0; to: 280 * 0.65; duration: 1800; easing.type: Easing.InOutCubic }
                    NumberAnimation { from: 280 * 0.65; to: 0; duration: 1800; easing.type: Easing.InOutCubic }
                }
            }
        }

        // App name
        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom; anchors.bottomMargin: 12
            spacing: 6
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Shadow Launcher"
                font.pixelSize: StyleTokens.fontSize2xl; font.bold: true
                color: StyleTokens.textSecondary
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("正在启动...")
                font.pixelSize: StyleTokens.fontSizeSm
                color: "#5a647a"
            }
        }
    }
}
