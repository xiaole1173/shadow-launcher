// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Layouts

/// 外置登录多角色选择弹窗 — 独立组件，不依赖 GenericPopup
Item {
    id: root
    anchors.fill: parent

    // ── 公开 API ──
    property bool opened: false
    property var backend

    // ── 信号 ──
    signal accepted(int profileIndex)
    signal cancelled()

    // ── 内部状态 ──
    property int _hoveredIndex: -1

    // ── 遮罩 ──
    Rectangle {
        id: dim
        anchors.fill: parent
        color: "#80000000"
        opacity: root.opened ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

        // 遮罩点击 = 取消
        MouseArea {
            anchors.fill: parent
            enabled: root.opened
            onClicked: root.cancelled()
            hoverEnabled: false
        }
    }

    // ── 卡片 ──
    Rectangle {
        id: card
        width: 340
        anchors.centerIn: parent

        // 高度由内容撑开，至少 200px，最多不超过父容器减去边距
        height: Math.min(48 + 1 + 8 + Math.max(contentColumn.implicitHeight, 120), parent ? parent.height - 80 : 600)

        radius: 12
        color: "#1a1a2e"
        border { color: "#2a2a4e"; width: 1 }

        opacity: root.opened ? 1 : 0
        scale: root.opened ? 1 : 0.92
        Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
        Behavior on scale { NumberAnimation { duration: 300; easing.type: Easing.OutBack } }

        // ── 标题 ──
        Text {
            id: titleText
            anchors { top: parent.top; left: parent.left; topMargin: 14; leftMargin: 16 }
            text: qsTr("选择角色")
            color: "#e0e4f0"
            font { pixelSize: 15; weight: Font.DemiBold }
        }

        // X 关闭按钮
        Rectangle {
            anchors { top: parent.top; right: parent.right; topMargin: 10; rightMargin: 10 }
            width: 28; height: 28; radius: 6
            color: xMarea.containsMouse ? "#ffffff12" : "transparent"
            Text {
                anchors.centerIn: parent
                text: "✕"; color: "#8888aa"; font.pixelSize: 14
            }

            MouseArea {
                id: xMarea; anchors.fill: parent; hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.cancelled()
            }
        }

        // ── 分割线 ──
        Rectangle {
            anchors { top: titleText.bottom; topMargin: 12 }
            width: parent.width; height: 1
            color: "#2a2a4e"
        }

        // ── 角色列表（不用 ScrollView，Column 直接撑开） ──
        Column {
            id: contentColumn
            anchors { top: parent.top; topMargin: 48 + 1 + 4; left: parent.left; right: parent.right }
            spacing: 2
            topPadding: 6
            bottomPadding: 6

            Repeater {
                id: profileRep
                model: backend && backend.yggdrasil ? backend.yggdrasil.profiles : []

                delegate: Rectangle {
                    required property int index
                    required property var modelData

                    width: parent ? parent.width : 320
                    height: 44
                    radius: 8

                    color: index === (backend && backend.yggdrasil ? backend.yggdrasil.profileIndex : -1)
                           ? "#1a3068" : (rowArea.containsMouse ? "#ffffff12" : "transparent")
                    Behavior on color { ColorAnimation { duration: 100 } }

                    // 每行布局
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 10

                        // 头像占位
                        Rectangle {
                            Layout.preferredWidth: 28
                            Layout.preferredHeight: 28
                            radius: 6
                            color: "#16162a"
                            border { color: "#3a3a5e"; width: 1 }

                            Text {
                                anchors.centerIn: parent
                                text: modelData.name ? modelData.name.charAt(0).toUpperCase() : "?"
                                color: "#a0a8c8"
                                font { pixelSize: 13; weight: Font.Bold }
                            }
                        }

                        // 角色名
                        Text {
                            text: modelData.name || ""
                            color: index === (backend && backend.yggdrasil ? backend.yggdrasil.profileIndex : -1)
                                   ? "#e0e4f0" : "#b0b4c8"
                            font.pixelSize: 13
                            font.weight: index === (backend && backend.yggdrasil ? backend.yggdrasil.profileIndex : -1)
                                         ? Font.DemiBold : Font.Normal
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        // 选中指示
                        Rectangle {
                            width: 8; height: 8; radius: 4
                            color: "#6080e8"
                            visible: index === (backend && backend.yggdrasil ? backend.yggdrasil.profileIndex : -1)
                        }
                    }

                    // 点击
                    MouseArea {
                        id: rowArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (backend && backend.yggdrasil) {
                                backend.yggdrasil.selectProfile(index)
                                root.accepted(index)
                            }
                        }
                    }
                }
            }
        }
    }
}
