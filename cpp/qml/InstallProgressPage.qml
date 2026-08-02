// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ═══════════════════════════════════════════════════════════════════
// InstallProgressPage — 全局下载进度页（原生纯净卡片列表）
//
// 整合包导入已原生接入 InstallCardModel：任务启动即注册一张标准
// DownloadQueueCard（type=modpack），6 阶段映射为卡片子步骤点阵，
// 模组明细 / 实时日志 / 解析信息面板全部经 cardData 轮询通道挂载在
// 卡片内部附属区。本页面与普通 MC 下载完全同构，无任何自研适配层。
// ═══════════════════════════════════════════════════════════════════
Item {
    id: root

    property var mainWindow: null
    property var backend: null

    Rectangle {
        anchors.fill: parent
        color: StyleTokens.bgPrimary
    }

    ListView {
        id: cardsView
        anchors.fill: parent
        anchors.topMargin: 16
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 8
        clip: true

        // 消除滚动条
        ScrollBar.vertical: null

        model: backend ? backend.installCardsModel : null
        delegate: DownloadQueueCard {
            width: cardsView.width
        }

        footer: Item {
            width: ListView.view.width
            height: ListView.view.height > 0 ? Math.max(ListView.view.height - 60, 0) : 250
            // 空状态仅当无任何卡片时显示（资源文件下载走卡片通道，installing 恒 false）
            visible: cardsView.count === 0

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

    // 过渡动画由 MainWindow 的 Rectangle 淡入处理，页面自身不额外动画
}
