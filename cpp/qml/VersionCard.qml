// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026  Shadow Team
import QtQuick
import QtQuick.Layouts

// VersionCard — MC 版本列表卡片
// 基于 DownloadCard 统一架构，更扁（42px），只显示版本号 + 类型标签
//
// 用法：
//   VersionCard {
//       versionId: model.versionId
//       versionType: model.vtype
//       isSelected: page.selectedVersionId === model.versionId
//       onClicked: { /* 打开安装页 */ }
//   }

Rectangle {
    id: root

    implicitWidth: parent ? parent.width : 200
    height: 42
    radius: StyleTokens.radiusMd
    color: root.transparent ? "transparent" : (hovered ? StyleTokens.accentSubtle : StyleTokens.bgPrimary)
    border.color: isSelected ? StyleTokens.accent :
                  (hovered ? StyleTokens.accentHover : "transparent")
    border.width: isSelected ? 1.5 : (hovered ? 1 : 0)

    opacity: 0
    Component.onCompleted: opacity = 1

    Behavior on color { ColorAnimation { duration: 150 } }
    Behavior on border.color { ColorAnimation { duration: 150 } }
    Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }

    // ── 公共 API ──
    property string versionId: ""
    property string versionType: ""
    property bool isSelected: false
    property bool transparent: false  // 自定义背景时透明

    signal clicked()

    // ── 内部状态 ──
    property bool hovered: false

    // ── 悬停辉光 ──
    // 正常模式：渐变光泽
    Rectangle {
        anchors.fill: parent; radius: parent.radius
        visible: !root.transparent
        gradient: Gradient {
            GradientStop { position: 0; color: StyleTokens.accentLight }
            GradientStop { position: 1; color: StyleTokens.accentLight }
        }
        opacity: root.hovered ? 0.06 : 0
        Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
    }
    // 透明模式：半透暗底
    Rectangle {
        anchors.fill: parent; radius: parent.radius
        visible: root.transparent
        color: Qt.rgba(0.09, 0.1, 0.13, 0.45)
        opacity: root.hovered ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
    }

    // ── 主布局 ──
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 10

        // ── 版本号 ──
        Text {
            text: root.versionId
            color: StyleTokens.textSecondary
            font.pixelSize: StyleTokens.fontSizeMd
            font.weight: root.isSelected ? Font.DemiBold : Font.Medium
            Layout.fillWidth: true
            elide: Text.ElideRight
        }

        // ── 类型标签 ──
        Rectangle {
            radius: StyleTokens.radiusXs
            height: 18
            width: typeTag.implicitWidth + 12
            color: root.versionType === "release" ? StyleTokens.successBg :
                   (root.versionType === "snapshot" ? StyleTokens.warningBg :
                   (root.versionType === "april_fools" ? StyleTokens.warningBg : StyleTokens.bgElevated))

            Text {
                id: typeTag
                anchors.centerIn: parent
                text: root.versionType === "release" ? "正式版" :
                      (root.versionType === "snapshot" ? "快照" :
                      (root.versionType === "april_fools" ? "愚人节" : "旧版"))
                color: root.versionType === "release" ? "#4a8" :
                       (root.versionType === "snapshot" ? "#b84" :
                       (root.versionType === "april_fools" ? "#e9a" : "#999"))
                font.pixelSize: StyleTokens.fontSizeXs
                font.family: StyleTokens.fontFamilyMono
            }
        }
    }

    // ── 鼠标区域 ──
    MouseArea {
        anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
        onEntered: root.hovered = true
        onExited: root.hovered = false
        onClicked: root.clicked()
    }
}
