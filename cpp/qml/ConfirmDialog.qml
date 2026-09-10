// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: root
    anchors.fill: parent

    property string title: ""
    property string message: ""
    property var onAccept: null
    property bool opened: false

    signal closed()

    onOpenedChanged: {
        if (!opened) closed()
    }

    // ── 2026-08-15：opened 控制整体可见性 ──
    // 原设计依赖 Loader active:false 卸载来隐藏（常驻加载后对话框无条件显示，
    // 启动即弹出空窗口只有取消/确认）。opened=false 时必须整体不可见。
    visible: opened
    opacity: opened ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

    // Dim overlay
    Rectangle {
        anchors.fill: parent; z: 0; color: StyleTokens.scrim
        opacity: 0.5
        Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
        MouseArea { anchors.fill: parent; onClicked: root.opened = false }
    }

    // Dialog box
    Rectangle {
        anchors.centerIn: parent; width: 360; height: 180; radius: StyleTokens.radiusLg; z: 1
        color: StyleTokens.surfaceOverlay; border.color: StyleTokens.errorBg; border.width: 1
        Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

        ColumnLayout {
            anchors.fill: parent; anchors.margins: 20; spacing: 12
            Text { text: root.title; font.pixelSize: StyleTokens.fontSizeLg; font.weight: Font.Bold; color: StyleTokens.textSecondary }
            Text { text: root.message; font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            Item { Layout.fillHeight: true }
            RowLayout {
                Layout.alignment: Qt.AlignRight; spacing: 10
                Rectangle {
                    width: 80; height: 32; radius: StyleTokens.radiusSm; color: "transparent"; border.color: StyleTokens.bgHover
                    scale: cancelDlgMa.pressed ? 0.9 : 1.0
                    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                    Text { anchors.centerIn: parent; text: qsTr("取消"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary }
                    MouseArea {
                        id: cancelDlgMa
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: root.opened = false
                    }
                }
                Rectangle {
                    width: 80; height: 32; radius: StyleTokens.radiusSm; color: StyleTokens.textDanger
                    scale: confirmDlgMa.pressed ? 0.9 : 1.0
                    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                    Text { anchors.centerIn: parent; text: qsTr("确认"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textPrimary }
                    MouseArea {
                        id: confirmDlgMa
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.opened = false
                            if (root.onAccept) root.onAccept()
                        }
                    }
                }
            }
        }
    }
}
