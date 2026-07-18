// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Layouts

// ═══ 披风卡片 — 用于衣帽间披风选择网格 ═══
// 紧凑布局：披风在上，名称+单选圈在下
Item {
    id: root
    signal selected()

    property string capeAlias: ""
    property string capeLabel: ""
    property string capePath: ""
    property bool isActive: false

    // 披风正面 10×16，缩放倍率
    readonly property int capeScale: 8
    readonly property int capeW: 10 * capeScale  // 80
    readonly property int capeH: 16 * capeScale  // 128

    Rectangle {
        anchors.fill: parent
        radius: StyleTokens.radiusLg
        color: root.isActive ? "#1a2a40" : "#161a22"
        border.color: root.isActive ? "#4a8ad4" : "#202838"
        border.width: root.isActive ? 2 : 1
        Behavior on color { ColorAnimation { duration: 200 } }
        Behavior on border.color { ColorAnimation { duration: 200 } }

        ColumnLayout {
            anchors {
                centerIn: parent
                verticalCenterOffset: -2
            }
            spacing: 8

            // ── 披风裁剪预览 ──
            // 正面 x=1, y=1, 10×16，clip+offset 扔掉右侧鞘翅
            Item {
                Layout.alignment: Qt.AlignHCenter
                width: root.capeW
                height: root.capeH
                clip: true

                // 无披风：空状态
                Column {
                    anchors.centerIn: parent
                    spacing: 4
                    visible: root.capePath === ""

                    Image {
                        anchors.horizontalCenter: parent.horizontalCenter
                        source: "../icons/lucide/x-circle.svg"
                        width: 28; height: 28
                        sourceSize: Qt.size(28, 28)
                    }
                }

                // 有披风：裁剪正面
                Image {
                    visible: root.capePath !== ""
                    source: root.capePath
                    width: (sourceSize.width > 0 ? sourceSize.width : 64) * root.capeScale
                    height: (sourceSize.height > 0 ? sourceSize.height : 32) * root.capeScale
                    x: -1 * root.capeScale
                    y: -1 * root.capeScale
                    fillMode: Image.Stretch
                    smooth: false
                    cache: false
                }
            }

            // ── 名称 + 单选圈 ──
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 6
                Layout.maximumWidth: root.capeW + 20

                Text {
                    Layout.alignment: Qt.AlignVCenter
                    text: root.isActive ? "\u25CF" : "\u25CB"
                    color: root.isActive ? "#4a8ad4" : "#404860"
                    font.pixelSize: StyleTokens.fontSizeMd
                }

                Text {
                    Layout.alignment: Qt.AlignVCenter
                    text: root.capeLabel
                    color: root.isActive ? "#c0d8f0" : "#687080"
                    font.pixelSize: StyleTokens.fontSizeSm
                    font.family: "Microsoft YaHei"
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.selected()
        }
    }
}
