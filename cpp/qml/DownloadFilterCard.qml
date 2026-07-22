// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

// DownloadFilterCard — 下载页统一筛选卡片组件
//
// 提供了搜索栏、内容区域和底部操作栏(搜索/重置按钮 + 测试版开关)
// 内容区域通过 content 属性注入筛选行
//
// 用法:
//   DownloadFilterCard {
//       searchPlaceholder: qsTr("输入 Mod 名称...")
//       onSearchRequested: doSearch()
//       onResetRequested: resetFilters()
//
//       content: ColumnLayout {
//           FilterRow {
//               FilterLabel { text: "加载器" }
//               ShadowDropdown { ... }
//               ...
//           }
//       }
//
//       showPreRelease: page.showPre
//       onPreReleaseToggled: page.showPre = !page.showPre
//   }

Rectangle {
    id: root

    Layout.fillWidth: true
    implicitHeight: mainCol.implicitHeight + 24
    radius: StyleTokens.radiusLg
    color: StyleTokens.bgSecondary
    border.color: StyleTokens.bgElevated

    // ── 公开 API ──
    property alias searchText: searchBox.text
    property alias searchPlaceholder: searchBox.placeholderText
    property alias content: contentArea
    property alias searchBox: searchBox

    property bool showPreRelease: false
    property bool preReleaseActive: false
    property string preReleaseOnText: qsTr("隐藏测试版")
    property string preReleaseOffText: qsTr("显示测试版")

    // 搜索/重置按钮位置: "top" (第1行) 或 "bottom" (最后1行)
    property int buttonsPosition: 0  // 0=top, 1=bottom

    // 来源下拉菜单 (RP 专用)
    property bool showSourceDropdown: false
    property var sourceModel: []
    property string sourceValue: ""
    property string sourceDisplayText: ""
    property string sourceValueKey: "value"

    // ── 信号 ──
    signal searchRequested()
    signal resetRequested()
    signal preReleaseToggled(bool active)

    // ── 布局 ──
    ColumnLayout {
        id: mainCol
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        // Row 1: 搜索栏 + 可选来源 + 按钮
        RowLayout {
            Layout.fillWidth: true; spacing: 10

            // 搜索框
            SearchBox {
                id: searchBox
                placeholderText: qsTr("搜索...")
                onAccepted: root.searchRequested()
            }

            // 来源下拉 (RP)
            ShadowDropdown {
                id: sourceDropdown
                Layout.preferredWidth: 140
                model: root.sourceModel
                valueKey: root.sourceValueKey
                displayText: root.sourceDisplayText
                currentValue: root.sourceValue
                visible: root.showSourceDropdown
            }

            // 搜索/重置按钮 (top 位置)
            RowLayout {
                id: topButtons
                spacing: 6
                visible: root.buttonsPosition === 0

                // ── 搜索按钮 ──
                Rectangle {
                    width: 50; height: 28; radius: StyleTokens.radiusSm
                    color: searchBtnMouse.containsMouse ? "#5a78e0" : StyleTokens.accentHover
                    scale: searchBtnMouse.pressed ? 0.92 : (searchBtnMouse.containsMouse ? 1.06 : 1.0)
                    Behavior on color { ColorAnimation { duration: 150 } }
                    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutBack } }

                    Text {
                        anchors.centerIn: parent
                        text: qsTr("搜索")
                        color: StyleTokens.textInverse
                        font.pixelSize: StyleTokens.fontSizeXs
                        font.bold: true
                    }
                    MouseArea {
                        id: searchBtnMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.searchRequested()
                    }
                }

                // ── 重置按钮 ──
                Rectangle {
                    width: 50; height: 28; radius: StyleTokens.radiusSm
                    color: resetBtnMouse.containsMouse ? "#2a2030" : "#1a1420"
                    border.color: "#3a2840"; border.width: 1
                    scale: resetBtnMouse.pressed ? 0.92 : (resetBtnMouse.containsMouse ? 1.06 : 1.0)
                    Behavior on color { ColorAnimation { duration: 150 } }
                    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutBack } }

                    Text {
                        anchors.centerIn: parent
                        text: qsTr("重置")
                        color: "#b090c8"
                        font.pixelSize: StyleTokens.fontSizeXs
                    }
                    MouseArea {
                        id: resetBtnMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.resetRequested()
                    }
                }
            }
        }

        // ── 内容区域 (筛选行) ──
        // 通过 content 属性注入 RowLayout 等筛选项
        ColumnLayout {
            id: contentArea
            Layout.fillWidth: true
            spacing: 8
        }

        // ── 底部操作栏: 测试版开关 + 搜索/重置按钮 (bottom 位置) ──
        RowLayout {
            Layout.fillWidth: true; spacing: 8
            visible: root.showPreRelease || root.buttonsPosition === 1

            // 测试版开关
            Rectangle {
                id: preReleaseToggle
                width: toggleText.implicitWidth + 16
                height: 24; radius: StyleTokens.radiusSm
                color: toggleMouse.containsMouse
                    ? (root.preReleaseActive ? "#282040" : "#1a1e28")
                    : (root.preReleaseActive ? "#1e1838" : "#12151c")
                border.color: root.preReleaseActive ? "#504080" : StyleTokens.borderLight
                border.width: 1
                Behavior on color { ColorAnimation { duration: 150 } }
                visible: root.showPreRelease

                Text {
                    id: toggleText
                    anchors.centerIn: parent
                    text: root.preReleaseActive ? root.preReleaseOnText : root.preReleaseOffText
                    color: root.preReleaseActive ? "#9088e0" : "#687080"
                    font.pixelSize: StyleTokens.fontSizeXs
                }
                MouseArea {
                    id: toggleMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.preReleaseToggled(!root.preReleaseActive)
                }
            }

            Item { Layout.fillWidth: true }

            // 搜索/重置按钮 (bottom 位置)
            RowLayout {
                id: bottomButtons
                spacing: 6
                visible: root.buttonsPosition === 1

                Rectangle {
                    width: 50; height: 28; radius: StyleTokens.radiusSm
                    color: sBtnMouse.containsMouse ? "#5a78e0" : StyleTokens.accentHover
                    scale: sBtnMouse.pressed ? 0.92 : (sBtnMouse.containsMouse ? 1.06 : 1.0)
                    Behavior on color { ColorAnimation { duration: 150 } }
                    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutBack } }
                    Text { anchors.centerIn: parent; text: qsTr("搜索"); color: StyleTokens.textInverse; font.pixelSize: StyleTokens.fontSizeXs; font.bold: true }
                    MouseArea {
                        id: sBtnMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: root.searchRequested()
                    }
                }
                Rectangle {
                    width: 50; height: 28; radius: StyleTokens.radiusSm
                    color: rBtnMouse.containsMouse ? "#2a2030" : "#1a1420"
                    border.color: "#3a2840"; border.width: 1
                    scale: rBtnMouse.pressed ? 0.92 : (rBtnMouse.containsMouse ? 1.06 : 1.0)
                    Behavior on color { ColorAnimation { duration: 150 } }
                    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutBack } }
                    Text { anchors.centerIn: parent; text: qsTr("重置"); color: "#b090c8"; font.pixelSize: StyleTokens.fontSizeXs }
                    MouseArea {
                        id: rBtnMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: root.resetRequested()
                    }
                }
            }
        }
    }

    // ── 动画 ──
    opacity: 0
    Component.onCompleted: opacity = 1
    Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
}
