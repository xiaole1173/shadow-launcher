// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

// DetailLinkBar — 详情页统一「转到 XX / 复制」胶囊按钮行。
// 复用项目通用 ShadowButton（hover 放大 / press 缩小 / OutBack 弹性缓动），
// 不再手搓 Rectangle；链接蓝边、复制棕边。
//
// 用法：
//   DetailLinkBar {
//       backend: backend
//       toastManager: toastManager
//       links: [ { label: "Modrinth", url: ".../mod/slug" }, ... ]       // 点击浏览器打开
//       copyItems: [ { label: "复制名称", text: "..." }, ... ]           // 点击复制并 toast
//   }

Flow {
    id: root
    Layout.fillWidth: true
    spacing: 8
    visible: (links.length > 0) || (copyItems.length > 0)

    property var backend: null
    property var toastManager: null
    property var links: []
    property var copyItems: []

    // 样式令牌：链接蓝边 / 复制棕边
    property color linkAccent: StyleTokens.accentHover
    property color linkTextColor: StyleTokens.accentLight
    property color copyAccent: StyleTokens.textMuted
    property color copyTextColor: StyleTokens.warning

    // ── 链接按钮（浏览器打开）──
    Repeater {
        model: root.links
        delegate: ShadowButton {
            text: modelData.label
            outlined: true
            accentColor: root.linkAccent
            textColor: root.linkTextColor
            font.pixelSize: StyleTokens.fontSizeSm
            implicitHeight: 30
            leftPadding: 14
            rightPadding: 14
            onClicked: {
                if (modelData.url) Qt.openUrlExternally(modelData.url)
            }
        }
    }

    // ── 复制按钮 ──
    Repeater {
        model: root.copyItems
        delegate: ShadowButton {
            text: modelData.label
            outlined: true
            accentColor: root.copyAccent
            textColor: root.copyTextColor
            font.pixelSize: StyleTokens.fontSizeSm
            implicitHeight: 30
            leftPadding: 14
            rightPadding: 14
            onClicked: {
                if (root.backend && modelData.text) root.backend.copyToClipboard(modelData.text)
                if (root.toastManager) root.toastManager.show("已" + modelData.label)
            }
        }
    }
}
