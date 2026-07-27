// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic

// DownloadCardTagRow
// 标签行子组件，用于 DownloadCard 中的类别/特性/分辨率标签显示
// 每个标签 16px 高，小字号，带背景色和边框

Item {
    id: root
    implicitWidth: parent ? parent.width : 200
    implicitHeight: 18

    property var tags: []            // string[]
    property color tagBg: "#151922"
    property color tagBorder: StyleTokens.bgHover
    property color tagColor: "#788090"

    clip: true

    Row {
        spacing: 4
        Repeater {
            model: root.tags
            delegate: Rectangle {
                height: 16
                width: tagText.implicitWidth + 10
                radius: StyleTokens.radiusSm
                color: root.tagBg
                border.color: root.tagBorder
                border.width: 1
                Text {
                    id: tagText
                    anchors.centerIn: parent
                    text: modelData || ""
                    color: root.tagColor
                    font.pixelSize: StyleTokens.fontSizeXs
                }
            }
        }
    }
}
