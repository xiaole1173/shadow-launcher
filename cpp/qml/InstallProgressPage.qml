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

    // ── 背景 ──
    Rectangle {
        anchors.fill: parent
        color: StyleTokens.bgPrimary
    }

    // ── 顶部标题栏 ──
    Rectangle {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 52
        color: StyleTokens.bgSecondary

        Text {
            anchors.left: parent.left; anchors.leftMargin: 20
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("下载进度")
            font.pixelSize: StyleTokens.fontSizeLg
            font.bold: true
            color: StyleTokens.textPrimary
        }

        // ── 提示：侧边栏可同步查看 ──
        Text {
            anchors.right: parent.right; anchors.rightMargin: 20
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("右下角 FAB 可收起/展开侧边面板")
            font.pixelSize: StyleTokens.fontSizeXs
            color: StyleTokens.textMuted
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

        Column {
            id: cardColumn
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: 8
            padding: 12

            // ── 下载中任务 ──
            Repeater {
                model: backend ? backend.installCardsModel : null

                delegate: DownloadQueueCard {
                    width: cardColumn.width - cardColumn.padding * 2
                    anchors.horizontalCenter: undefined
                    // centerIn parent — anchors.horizontalCenter is set by parent Column
                }
            }

            // ── 无任务占位 ──
            Item {
                width: parent.width - parent.padding * 2
                height: parent.height > 0 ? Math.max(parent.height - 80, 0) : 200
                visible: !backend || !backend.installing

                Column {
                    anchors.centerIn: parent
                    spacing: 12

                    Image {
                        anchors.horizontalCenter: parent.horizontalCenter
                        source: "icons/lucide/download-cloud.svg"
                        width: 48; height: 48
                        sourceSize.width: 48; sourceSize.height: 48
                        opacity: 0.4
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("暂无下载任务")
                        font.pixelSize: StyleTokens.fontSizeMd
                        color: StyleTokens.textMuted
                    }
                }
            }
        }
    }

    // ── 页面过渡动画 ──
    opacity: 0
    Behavior on opacity { NumberAnimation { duration: 200 } }

    // 当页面变为当前页时淡入
    states: State {
        name: "visible"
        when: root.visible
        PropertyChanges { root.opacity: 1 }
    }
}
