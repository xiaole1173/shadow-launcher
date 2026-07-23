// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Layouts

/// 外置登录多角色选择弹窗 — 独立组件
Item {
    id: root
    anchors.fill: parent

    // ── 公开 API ──
    property bool opened: false
    property var backend

    // ── 信号 ──
    signal accepted(int profileIndex)
    signal cancelled()

    // ── 遮罩（z=100 保证覆盖其他内容）──
    Rectangle {
        anchors.fill: parent
        z: 100
        color: "#80000000"
        opacity: root.opened ? 1 : 0
        visible: root.opened || opacity > 0
        Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

        // 仅拦截事件防止穿透，不处理点击（只有 X 能关闭）
        MouseArea { anchors.fill: parent }
    }

    // ── 卡片 ──
    Rectangle {
        id: card
        z: 101
        width: 360
        height: Math.min(48 + 1 + 12 + Math.max(contentCol.implicitHeight + 16, 160), parent ? parent.height - 80 : 600)
        anchors.centerIn: parent
        radius: StyleTokens.radiusLg
        color: StyleTokens.bgSecondary
        border { color: StyleTokens.border; width: 1 }

        scale: root.opened ? 1 : 0.9
        opacity: root.opened ? 1 : 0
        Behavior on scale { NumberAnimation { duration: 350; easing.type: Easing.OutBack } }
        Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

        // ── Header ──
        Rectangle {
            id: header
            width: parent.width; height: 48
            color: "transparent"

            Text {
                text: qsTr("选择角色")
                anchors { left: parent.left; leftMargin: 16; verticalCenter: parent.verticalCenter }
                color: StyleTokens.textPrimary
                font { pixelSize: StyleTokens.fontSizeLg; weight: Font.DemiBold }
            }

            // 关闭按钮（X），跟披风选择弹窗统一风格
            Rectangle {
                id: closeBtn
                anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
                width: 32; height: 32; radius: StyleTokens.radiusSm
                color: closeArea.containsMouse ? StyleTokens.bgHover : "transparent"
                Behavior on color { ColorAnimation { duration: 120 } }

                Image {
                    source: "icons/lucide/x.svg"
                    width: 16; height: 16
                    anchors.centerIn: parent
                    sourceSize: Qt.size(16, 16)
                }

                MouseArea {
                    id: closeArea
                    anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.cancelled()
                }
            }
        }

        // ── 分割线 ──
        Rectangle {
            width: parent.width; height: 1
            anchors.top: header.bottom
            color: StyleTokens.borderLight
        }

        // ── 可滚动内容（Flickable，跟披风弹窗一致）──
        Flickable {
            id: flick
            width: parent.width
            anchors { top: header.bottom; topMargin: 1; bottom: parent.bottom }
            contentHeight: contentCol.implicitHeight + 16
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: contentCol
                width: parent.width - 16
                anchors.horizontalCenter: parent.horizontalCenter
                y: 8
                spacing: 4

                Repeater {
                    id: profileRep
                    model: backend && backend.yggdrasil ? backend.yggdrasil.profiles : []

                    delegate: Rectangle {
                        required property int index
                        required property var modelData

                        Layout.fillWidth: true
                        height: 44
                        radius: StyleTokens.radiusSm

                        color: index === (backend && backend.yggdrasil ? backend.yggdrasil.profileIndex : -1)
                               ? StyleTokens.accentSubtle
                               : (rowArea.containsMouse ? StyleTokens.bgHover : "transparent")
                        Behavior on color { ColorAnimation { duration: 120 } }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 10

                            // 头像占位
                            Rectangle {
                                Layout.preferredWidth: 28
                                Layout.preferredHeight: 28
                                radius: StyleTokens.radiusSm
                                color: StyleTokens.bgCard
                                border { color: StyleTokens.borderLight; width: 1 }

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.name ? modelData.name.charAt(0).toUpperCase() : "?"
                                    color: StyleTokens.textTertiary
                                    font { pixelSize: StyleTokens.fontSizeMd; weight: Font.Bold }
                                }
                            }

                            // 角色名
                            Text {
                                text: modelData.name || ""
                                color: index === (backend && backend.yggdrasil ? backend.yggdrasil.profileIndex : -1)
                                       ? StyleTokens.accentLight : StyleTokens.textPrimary
                                font.pixelSize: StyleTokens.fontSizeMd
                                font.weight: index === (backend && backend.yggdrasil ? backend.yggdrasil.profileIndex : -1)
                                             ? Font.DemiBold : Font.Normal
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }

                            // 选中指示
                            Rectangle {
                                width: 8; height: 8; radius: 4
                                visible: index === (backend && backend.yggdrasil ? backend.yggdrasil.profileIndex : -1)
                                color: StyleTokens.accent
                            }
                        }

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

}
