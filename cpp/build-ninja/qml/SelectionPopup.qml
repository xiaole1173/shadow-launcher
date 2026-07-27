// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

// ═══ SelectionPopup — 通用选择弹窗 ═══
//
// 特点：
//   - 背景遮罩 + 居中卡片
//   - 标题 + 关闭按钮（ShadowIconButton）
//   - 可自定义内容区（default property）
//   - 弹入/弹出动画（opacity + scale）
//   - 点遮罩关闭
//
// 用法：
//   SelectionPopup {
//       id: popup
//       title: "选择角色"
//       opened: showPopup
//       onClosed: showPopup = false
//
//       ListView {
//           model: myModel
//           delegate: ItemDelegate { ... }
//       }
//   }
//
// ──────────────────────────────────────────────

Item {
    id: root
    anchors.fill: parent
    clip: true

    // ── Public API ──
    property string title: ""
    property string subtitle: ""
    property bool opened: false

    // Card sizing
    property int cardWidth: 360
    property int cardMaxHeight: parent ? parent.height - 80 : 600

    // Animation tweaks
    property real animDuration: 200
    property real animScale: 0.92

    // Custom close button source (SVG path). Empty = uses Lucide X
    property string closeIcon: ""

    // ── Signals ──
    signal closed()
    signal accepted()

    // ── Content area — default property ──
    default property alias content: contentContainer.children

    // ── Opacity sync ──
    visible: root.opened || root.opacity > 0
    opacity: root.opened ? 1 : 0
    Behavior on opacity {
        NumberAnimation { duration: root.animDuration; easing.type: Easing.OutCubic }
    }

    // ── Dim overlay ──
    Rectangle {
        id: dimBg
        anchors.fill: parent
        color: "#80000000"
        opacity: root.opened ? 1 : 0
        z: 0

        Behavior on opacity {
            NumberAnimation { duration: root.animDuration; easing.type: Easing.OutCubic }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onClicked: {
                root.opened = false
                root.closed()
            }
        }
    }

    // ── Card wrapper (centered) ──
    Rectangle {
        id: card
        width: root.cardWidth
        height: Math.min(contentColumn.implicitHeight + 16, root.cardMaxHeight)
        anchors.centerIn: parent
        z: 1

        radius: StyleTokens.radiusLg
        color: StyleTokens.bgSecondary
        border.color: StyleTokens.border; border.width: 1

        // ── Entrance / exit animation ──
        opacity: root.opened ? 1 : 0
        scale: root.opened ? 1 : root.animScale

        Behavior on opacity {
            NumberAnimation { duration: root.animDuration; easing.type: Easing.OutCubic }
        }
        Behavior on scale {
            NumberAnimation { duration: root.animDuration + 100; easing.type: Easing.OutBack }
        }

        // ── Layout ──
        ColumnLayout {
            id: contentColumn
            anchors.fill: parent
            anchors.margins: 0
            spacing: 0

            // ── Header ──
            Rectangle {
                id: headerBar
                Layout.fillWidth: true
                Layout.preferredHeight: root.subtitle.length > 0 ? 56 : 48
                color: "transparent"

                // Title
                Text {
                    anchors.left: parent.left; anchors.leftMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.title
                    color: StyleTokens.textPrimary
                    font.pixelSize: StyleTokens.fontSizeLg
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                // Subtitle (if any, below title)
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

                // Close button
                ShadowIconButton {
                    id: closeBtn
                    anchors.right: parent.right; anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    source: root.closeIcon.length > 0 ? root.closeIcon : "icons/lucide/x.svg"
                    sourceWidth: 14; sourceHeight: 14
                    type: "close"
                    onClicked: {
                        root.opened = false
                        root.closed()
                    }
                }
            }

            // ── Divider ──
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: StyleTokens.bgElevated
            }

            // ── Scrollable content area ──
            ScrollView {
                id: scrollView
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.topMargin: 4
                Layout.bottomMargin: 4
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

    // ── Keyboard: Escape to close ──
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape && root.opened) {
            root.opened = false
            root.closed()
            event.accepted = true
        }
    }

    // ── Focus management ──
    focus: true
    onOpenedChanged: {
        if (root.opened) {
            forceActiveFocus()
        }
    }

    // Prevent event leakage (capture all clicks beneath the overlay)
    MouseArea {
        anchors.fill: parent
        z: -1
        propagateComposedEvents: false
        enabled: root.opened
    }
}
