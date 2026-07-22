// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

// SearchBox — 统一下载页/设置页搜索框组件
//
// 用法:
//   SearchBox {
//       id: mySearch
//       placeholderText: qsTr("输入名称...")
//       onAccepted: doSearch()
//   }
//
// 可选:
//   SearchBox {
//       showIcon: true   // 左侧放大镜图标
//       width: 200       // 固定宽度 (默认 Layout.fillWidth: true)
//       text: bindingToSomeText
//   }

Rectangle {
    id: root

    // ── 公开 API ──
    property alias text: input.text
    property alias placeholderText: placeholder.text
    property alias input: input
    property bool showIcon: false

    // ── 外观 ──
    Layout.fillWidth: true
    implicitHeight: 28
    radius: StyleTokens.radiusSm
    color: StyleTokens.bgInput
    border.color: input.activeFocus ? StyleTokens.accentHover : StyleTokens.borderLight
    border.width: 1
    Behavior on border.color { ColorAnimation { duration: 200 } }

    // ── 搜索图标 (可选) ──
    Image {
        id: iconImg
        source: "icons/lucide/search.svg"
        width: 14; height: 14
        anchors { left: parent.left; leftMargin: 10; verticalCenter: parent.verticalCenter }
        visible: root.showIcon
    }

    // ── 输入框 ──
    TextInput {
        id: input
        anchors.fill: parent
        anchors.leftMargin: root.showIcon ? 32 : 8
        anchors.rightMargin: 8
        color: StyleTokens.textPrimary
        verticalAlignment: TextInput.AlignVCenter
        font.pixelSize: StyleTokens.fontSizeSm
        Keys.onReturnPressed: root.accepted()
    }

    // ── 占位符 ──
    Text {
        id: placeholder
        anchors.left: parent.left
        anchors.leftMargin: root.showIcon ? 32 : 8
        anchors.verticalCenter: parent.verticalCenter
        text: qsTr("搜索...")
        color: StyleTokens.textMuted
        font.pixelSize: StyleTokens.fontSizeSm
        visible: !input.text
    }

    // ── 信号 ──
    signal accepted()
}
