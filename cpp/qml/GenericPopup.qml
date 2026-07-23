// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

/// 通用弹窗组件
/// 使用方式：
///   GenericPopup {
///       title: qsTr("选择角色")
///       opened: showPopup
///       onClosed: showPopup = false
///       onRejected: backend.yggdrasil.cancelLogin()
///
///       ListView { model: ... }
///   }
Item {
    id: root
    anchors.fill: parent
    clip: true

    // ── 公开 API ──
    property string title: ""
    property string subtitle: ""
    property bool opened: false
    property int cardWidth: 360
    property string closeIcon: "icons/lucide/x.svg"

    // ── 信号 ──
    /// 关闭弹窗（点遮罩、按 Esc、点 X）
    signal closed()
    /// 确认操作
    signal accepted()
    /// 拒绝/取消（点 X 专用，与 closed 的区别是你可以知道用户是"取消"还是"点外部关闭"）
    signal rejected()

    // ── 内容区（默认属性） ──
    default property alias content: contentContainer.children

    // ── 可见性 ──
    visible: root.opened || root.opacity > 0
    opacity: root.opened ? 1 : 0
    Behavior on opacity {
        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
    }

    // 放最底层，阻止事件穿透但 z=-1 不遮挡正常内容
    MouseArea {
        anchors.fill: parent; z: -1
        enabled: root.opened
        propagateComposedEvents: false
    }

    // ── 遮罩层 ──
    Rectangle {
        anchors.fill: parent
        color: "#80000000"
        opacity: root.opened ? 1 : 0
        Behavior on opacity {
            NumberAnimation { duration: root.opened ? 150 : 200; easing.type: Easing.OutCubic }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                root.closed()
            }
        }
    }

    // ── 卡片 ──
    Rectangle {
        id: card
        width: root.cardWidth
        height: Math.min(contentColumn.implicitHeight + 16, parent ? parent.height - 80 : 600)
        anchors.centerIn: parent

        radius: StyleTokens.radiusLg
        color: StyleTokens.bgSecondary
        border.color: StyleTokens.border; border.width: 1

        opacity: root.opened ? 1 : 0
        scale: root.opened ? 1 : 0.92
        Behavior on opacity {
            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
        }
        Behavior on scale {
            NumberAnimation { duration: 300; easing.type: Easing.OutBack }
        }

        ColumnLayout {
            id: contentColumn
            anchors.fill: parent
            anchors.margins: 0
            spacing: 0

            // ── 标题栏 ──
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: root.subtitle.length > 0 ? 56 : 48
                color: "transparent"

                Text {
                    anchors.left: parent.left; anchors.leftMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.title
                    color: StyleTokens.textPrimary
                    font.pixelSize: StyleTokens.fontSizeLg
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    width: parent.width - 80
                }

                Text {
                    anchors.left: parent.left; anchors.leftMargin: 16
                    anchors.top: parent.top; anchors.topMargin: 28
                    visible: root.subtitle.length > 0
                    text: root.subtitle
                    color: StyleTokens.textTertiary
                    font.pixelSize: StyleTokens.fontSizeXs
                    elide: Text.ElideRight
                    width: parent.width - 80
                }

                // 关闭按钮（X）
                ShadowIconButton {
                    anchors.right: parent.right; anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    source: root.closeIcon
                    sourceWidth: 14; sourceHeight: 14
                    type: "close"
                    onClicked: {
                        root.closed()
                        root.rejected()
                    }
                }
            }

            // ── 分割线 ──
            Rectangle {
                Layout.fillWidth: true; Layout.preferredHeight: 1
                color: StyleTokens.bgElevated
            }

            // ── 可滚动内容区 ──
            ScrollView {
                Layout.fillWidth: true; Layout.fillHeight: true
                Layout.topMargin: 4; Layout.bottomMargin: 4
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ScrollBar.vertical.policy: ScrollBar.AsNeeded
                ScrollBar.vertical.interactive: true

                Item {
                    id: contentContainer
                    width: scrollView.availableWidth
                    height: childrenRect.height
                }
            }
        }
    }

    // ── Esc 键关闭 ──
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape && root.opened) {
            root.closed()
            root.rejected()
            event.accepted = true
        }
    }

    focus: true
    onOpenedChanged: {
        if (root.opened) forceActiveFocus()
    }
}
