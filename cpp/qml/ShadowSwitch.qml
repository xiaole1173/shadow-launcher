// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls.Basic

// ═══════════════════════════════════════════════════════════════
//  ShadowSwitch — 统一开关组件（纯 indicator，无自带 label）
//
//  用法（无标签）:
//    ShadowSwitch {
//        checked: backend.someProp
//        onToggled: backend.someProp = checked
//    }
//
//  用法（带标签 — 在外面用 RowLayout + Text）:
//    RowLayout {
//        Text { text: "选项名"; Layout.fillWidth: true }
//        ShadowSwitch { checked: ...; onToggled: ... }
//    }
//
//  属性:
//    checked   — 当前开关状态
//    enabled   — 是否可交互（默认 true）
// ═══════════════════════════════════════════════════════════════

Switch {
    id: control

    // ── 自定义 indicator 样式 ──
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

    // ── 不覆写 contentItem：由使用者在外部放 Text ──
    // （原生 Switch 默认无 content，indicator 自然居中/靠右）
}
