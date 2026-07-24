// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// InstallProgressPage — 全屏下载进度页
// 复用侧边栏的 installCardsModel + DownloadQueueCard 组件
// 数据与浮动面板完全同源，专为内测查看多任务进度提供完整视图

Item {
    id: root

    property var mainWindow: null

    // ── 顶部渐变装饰 ──
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 120
        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop { position: 0.0; color: Qt.rgba(0.4, 0.6, 1.0, 0.08) }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    // ── 顶部标题栏 ──
    Rectangle {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 56
        color: "transparent"

        // 左侧标题区
        RowLayout {
            anchors.left: parent.left; anchors.leftMargin: 24
            anchors.verticalCenter: parent.verticalCenter
            spacing: 12

            // 图标背景
            Rectangle {
                width: 32; height: 32; radius: 8
                color: Qt.rgba(0.4, 0.6, 1.0, 0.15)
                Image {
                    anchors.centerIn: parent
                    source: "icons/lucide/download-cloud.svg"
                    width: 18; height: 18
                    sourceSize.width: 18; sourceSize.height: 18
                }
            }

            Text {
                text: qsTr("下载进度")
                font.pixelSize: StyleTokens.fontSizeLg
                font.bold: true
                color: StyleTokens.textPrimary
            }

            // 任务计数
            Rectangle {
                visible: backend && backend.activeCount > 0
                height: 20
                radius: 10
                color: Qt.rgba(0.4, 0.6, 1.0, 0.2)
                Layout.alignment: Qt.AlignVCenter

                Text {
                    anchors.centerIn: parent
                    anchors.leftMargin: 8; anchors.rightMargin: 8
                    text: backend ? backend.activeCount + " " + qsTr("个任务") : ""
                    font.pixelSize: StyleTokens.fontSizeXs
                    color: StyleTokens.accent
                    padding: 4
                }
            }
        }

        // 右侧提示
        Text {
            anchors.right: parent.right; anchors.rightMargin: 24
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("右下角 FAB 可收起/展开侧边面板")
            font.pixelSize: StyleTokens.fontSizeXs
            color: StyleTokens.textMuted
        }

        // 底部细线
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left; anchors.leftMargin: 24
            anchors.right: parent.right; anchors.rightMargin: 24
            height: 1
            color: Qt.rgba(1, 1, 1, 0.06)
        }
    }

    // ── 卡片列表 ──
    ScrollView {
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.topMargin: 8
        clip: true
        ScrollBar.vertical.policy: ScrollBar.AsNeeded
        ScrollBar.vertical.interactive: true

        Column {
            id: cardColumn
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: 8
            padding: 16
            bottomPadding: 32

            // ── 下载中任务 ──
            Repeater {
                id: cardsRepeater
                model: backend ? backend.installCardsModel : null

                delegate: DownloadQueueCard {
                    width: cardColumn.width - cardColumn.padding * 2
                    anchors.horizontalCenter: undefined
                }
            }

            // ── 无任务占位 ──
            Item {
                width: parent.width - parent.padding * 2
                height: parent.height > 0 ? Math.max(parent.height - 80, 0) : 250
                visible: !backend || !backend.installing

                Column {
                    anchors.centerIn: parent
                    spacing: 16

                    Image {
                        anchors.horizontalCenter: parent.horizontalCenter
                        source: "icons/lucide/download-cloud.svg"
                        width: 64; height: 64
                        sourceSize.width: 64; sourceSize.height: 64
                        opacity: 0.3
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("暂无下载任务")
                        font.pixelSize: StyleTokens.fontSizeMd
                        color: StyleTokens.textMuted
                        opacity: 0.6
                    }
                }
            }
        }
    }

    // ── 页面进入动画 ──
    opacity: 0
    Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

    states: State {
        name: "visible"
        when: root.visible
        PropertyChanges { root.opacity: 1 }
    }
}
