// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls.Basic

// ═══════════════════════════════════════════════════════════════
//  ShadowSwitch — 统一开关组件
//
//  用法:
//    ShadowSwitch {
//        checked: backend.yggdrasil.autoJoinServer
//        onToggled: backend.yggdrasil.autoJoinServer = checked
//    }
//
//  属性:
//    checked   — 当前开关状态
//    enabled   — 是否可交互（默认 true）
//    label     — 左侧标签文本（为空则不显示）
// ═══════════════════════════════════════════════════════════════

Switch {
    id: control

    // ── 公开属性 ──
    property string label: ""

    // ── 自定义样式 ──
    indicator: Rectangle {
        implicitWidth: 40
        implicitHeight: 22
        radius: height / 2
        color: control.checked ? StyleTokens.accentLight : StyleTokens.bgHover
        opacity: control.enabled ? 1.0 : 0.4

        Behavior on color {
            ColorAnimation { duration: 150; easing.type: Easing.OutCubic }
        }

        // 滑块
        Rectangle {
            width: 16
            height: 16
            radius: height / 2
            color: control.enabled ? "#e4e8f2" : "#707080"
            anchors.verticalCenter: parent.verticalCenter
            x: control.checked ? parent.width - width - 3 : 3

            Behavior on x {
                NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
            }
            Behavior on color {
                ColorAnimation { duration: 150; easing.type: Easing.OutCubic }
            }
        }
    }

    // ── 可选标签 ──
    contentItem: Text {
        text: control.label
        color: control.enabled ? StyleTokens.textSecondary : StyleTokens.textMuted
        font.pixelSize: StyleTokens.fontSizeMd
        verticalAlignment: Text.AlignVCenter
    }
}
