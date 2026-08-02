// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Layouts

// ModpackInfoPanel — 整合包基本信息面板（解析结果预览）
// 解析完成前显示骨架（加载中动画）；解析完成后显示 2×4 信息网格：
// 名称 / 版本号 / 游戏版本 / 加载器 / 在线模组 / 内置文件 / 来源 / 目标版本。
//
// 用法：
//   ModpackInfoPanel {
//       revealed: root._infoRevealed
//       packName: root._info.name
//       packVersion: root._info.version
//       mcVersion: root._info.mc
//       loader: root._info.loader
//       modCount: root._info.modCount
//       fileCount: root._info.fileCount
//       format: root._info.format
//       targetName: root._info.targetName
//   }
Item {
    id: root

    implicitHeight: 112

    property bool revealed: false
    property string packName: ""
    property string packVersion: ""
    property string mcVersion: ""
    property string loader: ""
    property int modCount: 0
    property string fileCount: ""       // "" = 未知（显示 —）
    property string format: ""
    property string targetName: ""
    property string iconUrl: ""         // 整合包图标（下载 tab 来源）；空则显示占位图标

    Rectangle {
        anchors.fill: parent
        radius: StyleTokens.radiusLg
        color: StyleTokens.bgCard
        border.color: StyleTokens.borderLight
        border.width: 1

        // ── 骨架（解析中）──
        RowLayout {
            anchors.centerIn: parent
            spacing: 10
            visible: !root.revealed

            LoadingSpinner {
                Layout.preferredWidth: 16
                Layout.preferredHeight: 16
                running: true
            }
            Text {
                text: qsTr("正在解析整合包…（识别名称 / 版本 / 加载器 / 模组清单）")
                color: StyleTokens.textTertiary
                font.pixelSize: StyleTokens.fontSizeSm
            }
        }

        // ── 数据面板 ──
        RowLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 12
            visible: root.revealed

            opacity: root.revealed ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }

            // 左侧来源图标：有整合包图标（下载 Tab 来源 URL）则显示，否则占位 box
            Rectangle {
                radius: StyleTokens.radiusLg
                color: StyleTokens.accentSubtle
                Layout.preferredWidth: 38
                Layout.preferredHeight: 38
                Layout.alignment: Qt.AlignVCenter

                Image {
                    anchors.centerIn: parent
                    source: root.iconUrl ? root.iconUrl : "icons/lucide/box.svg"
                    width: root.iconUrl ? 38 : 18
                    height: root.iconUrl ? 38 : 18
                    radius: root.iconUrl ? StyleTokens.radiusLg : 0
                    clip: root.iconUrl !== ""
                    fillMode: Image.PreserveAspectCrop
                    // 图标加载失败（离线/URL 失效）→ 回退占位图
                    onStatusChanged: {
                        if (status === Image.Error) {
                            source = "icons/lucide/box.svg"
                            width = 18; height = 18
                        }
                    }
                }
            }

            // ── 2×4 信息网格 ──
            GridLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                columns: 2
                columnSpacing: 28
                rowSpacing: 5

                Repeater {
                    model: [
                        [qsTr("名称"), root.packName || "—"],
                        [qsTr("游戏版本"), root.mcVersion || "—"],
                        [qsTr("加载器"), root.loader || qsTr("无")],
                        [qsTr("来源"), root.format || "—"],
                        [qsTr("版本号"), root.packVersion || "—"],
                        [qsTr("在线模组"), qsTr("%1 个").arg(root.modCount)],
                        [qsTr("内置文件"), root.fileCount === "" ? "—" : qsTr("%1 个").arg(root.fileCount)],
                        [qsTr("目标版本"), root.targetName || "—"]
                    ]

                    delegate: RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: modelData[0]
                            font.pixelSize: StyleTokens.fontSizeXs
                            color: StyleTokens.textSubtle
                            Layout.preferredWidth: 48
                        }
                        Text {
                            text: modelData[1]
                            font.pixelSize: StyleTokens.fontSizeSm
                            font.bold: modelData[0] === qsTr("名称")
                            color: modelData[0] === qsTr("名称") ? StyleTokens.textPrimary : StyleTokens.textSecondary
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                }
            }
        }
    }
}
