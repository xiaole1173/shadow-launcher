// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// LabeledDropdown — "标签文本 + 下拉筛选" 组合（FilterCard 12 处重复结构的公共抽取，2026-08-02）
RowLayout {
    id: root

    // ── 公共 API（透传给内部 ShadowDropdown）──
    property string label: ""
    property var model: []
    property string valueKey: ""
    property var labelFn: null
    property string currentValue: ""
    signal valueSelected(var v)

    spacing: 8

    Text {
        text: root.label
        color: "#9094a8"
        font.pixelSize: StyleTokens.fontSizeSm
        Layout.preferredWidth: Math.max(28, implicitWidth)
    }
    ShadowDropdown {
        Layout.fillWidth: true
        Layout.minimumWidth: 80
        model: root.model
        valueKey: root.valueKey
        labelFn: root.labelFn
        currentValue: root.currentValue
        onValueSelected: function(v) { root.valueSelected(v) }
    }
}
