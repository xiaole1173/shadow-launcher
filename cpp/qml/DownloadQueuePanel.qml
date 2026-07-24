// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

// DownloadQueuePanel — 浮动下载队列面板
// 固定在 MainWindow 右侧，通过右下角 FAB 按钮控制展开/折叠
// 使用 Lucide 图标，无 emoji

Rectangle {
    id: root

    // ── Public API ──
    property bool expanded: true
    property var backendRef: null

    // ── Panel state tracking ──
    property bool _autoExpandedOnce: false  // 是否已经自动展开过
    property int _cardCount: 0

    // ── Layout ──
    anchors.right: parent.right
    anchors.top: parent.top; anchors.bottom: parent.bottom
    anchors.topMargin: 8; anchors.bottomMargin: 8
    z: 4

    width: root.expanded ? 320 : 0
    clip: true
    visible: _cardCount > 0

    Behavior on width {
        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
    }

    color: "#0d1117"
    radius: StyleTokens.radiusMd
    border { color: "#1a202c"; width: 1 }

    // ── 内容区域 (仅在展开时可见) ──
    opacity: root.expanded ? 1 : 0
    Behavior on opacity {
        NumberAnimation { duration: 150 }
    }

    // ── 同步卡片数量 ──
    function syncCardCount() {
        var m = backendRef ? backendRef.installCardsModel : null
        _cardCount = m ? m.count : 0
        // 首次有卡片时自动展开
        if (_cardCount > 0 && !_autoExpandedOnce) {
            root.expanded = true
            _autoExpandedOnce = true
        }
    }

    // ── 内容 ──
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 0; spacing: 0
        visible: root.expanded

        // ── Header ──
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            color: "transparent"

            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 8
                spacing: 4

                Text {
                    text: "下载中"
                    font.pixelSize: StyleTokens.fontSizeSm
                    color: StyleTokens.textSecondary
                }

                Text {
                    text: "(" + (backendRef && backendRef.installCardsModel ? backendRef.installCardsModel.count : 0) + ")"
                    font.pixelSize: StyleTokens.fontSizeSm
                    color: StyleTokens.textMuted
                }

                Item { Layout.fillWidth: true }

                // ── 收起按钮 (Lucide chevron-down.svg) ──
                Rectangle {
                    width: 24; height: 24; radius: 4
                    color: collapseMouse.containsMouse ? "#1a2030" : "transparent"
                    Behavior on color { ColorAnimation { duration: 120 } }

                    Image {
                        anchors.centerIn: parent
                        source: "icons/lucide/chevron-down.svg"
                        width: 14; height: 14
                        sourceSize.width: 14; sourceSize.height: 14
                    }

                    MouseArea {
                        id: collapseMouse
                        anchors.fill: parent; hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.expanded = false
                    }
                }
            }
        }

        // ── 分隔线 ──
        Rectangle {
            Layout.fillWidth: true; height: 1
            color: "#1a202c"
        }

        // ── 卡片列表 ──
        ListView {
            id: cardsView
            Layout.fillWidth: true; Layout.fillHeight: true
            Layout.topMargin: 6; Layout.bottomMargin: 6
            spacing: 6
            clip: true

            model: backendRef ? backendRef.installCardsModel : null

            delegate: DownloadQueueCard {
                width: cardsView.width - 8
                anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
            }

            // ── 空状态 ──
            Rectangle {
                anchors.centerIn: parent
                visible: cardsView.count === 0
                color: "transparent"
                ColumnLayout {
                    anchors.centerIn: parent; spacing: 8
                    Image {
                        Layout.alignment: Qt.AlignHCenter
                        source: "icons/lucide/package.svg"
                        width: 32; height: 32
                        sourceSize.width: 32; sourceSize.height: 32
                        opacity: 0.3
                    }
                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "当前没有下载任务"
                        font.pixelSize: StyleTokens.fontSizeSm
                        color: StyleTokens.textMuted
                    }
                }
            }

            // 底部留白
            footer: Item { height: 8 }
        }
    }

    // ── 初始化同步 ──
    Component.onCompleted: root.syncCardCount()

    // ── 监听卡片模型变化 ──
    Connections {
        target: backendRef ? backendRef.installCardsModel : null
        enabled: target !== null
        function onGenerationChanged() {
            root.syncCardCount()
        }
    }
}
