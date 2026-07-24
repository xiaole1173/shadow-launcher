// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// InstallProgressPage — 全屏下载进度页
// 简洁风格，与启动器其他页面一致

Item {
    id: root

    property var mainWindow: null
    property var backend: null

    // ── 简单背景 ──
    Rectangle {
        anchors.fill: parent
        color: StyleTokens.bgPrimary
    }

    // ── 卡片列表 ──
    ScrollView {
        anchors.fill: parent
        anchors.topMargin: 16
        clip: true
        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
            contentItem: Rectangle {
                implicitWidth: 4
                radius: StyleTokens.radiusXs
                color: StyleTokens.textMuted
            }
        }

        // 使用 ListView 而非 Column+Repeater
        // ListView 原生支持滚动 + 复用委托，与侧边栏一致
        ListView {
            id: cardsView
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 8

            model: backend ? backend.installCardsModel : null
            delegate: DownloadQueueCard {
                width: cardsView.width - 32
            }

            // ── 顶部留白 ──
            headerPositioning: ListView.OverlayHeader
            header: Item { width: 1; height: 4 }

            // ── 无任务占位 ──
            footer: Item {
                width: ListView.view.width
                height: ListView.view.height > 0 ? Math.max(ListView.view.height - 60, 0) : 250
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
