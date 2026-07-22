// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

// ──────────────────────────────────────────────
// ShadowDropdown — 通用下拉筛选组件
//
// 美学参考 ModLoaderCard：选中高亮 #3a50b0、悬停 #1a2440、
// 禁用 opacity 0.55、状态点、类型标签色等。
//
// 用法：
//   ShadowDropdown {
//       Layout.preferredWidth: 120
//       model: [{slug:"fabric", label:"Fabric"}, ...]
//       valueKey: "slug"
//       currentValue: page.modLoader
//       onValueSelected: function(v) { page.modLoader = v }
//   }
// ──────────────────────────────────────────────

Rectangle {
    id: root

    // ── Geometry ──
    height: 28
    radius: StyleTokens.radiusSm

    // ── Public API ──
    // Current selected value (bind to parent property)
    property var currentValue: ""

    // Model: array of items. Items can be:
    //   - plain strings: ["fabric","forge"]
    //   - objects:      [{slug:"fabric", label:"Fabric"}, ...]
    property var model: []

    // Key names for extracting value/label from object items
    property string valueKey: "value"
    property string labelKey: "label"

    // Optional: custom label function (value → display string).
    // When set, overrides auto-lookup from model.
    // Example: function(v) { return page.modLoaderLabels[v] || "全部" }
    property var labelFn: null

    // Optional: fully override the trigger display text.
    // When non-empty, takes priority over labelFn and auto-lookup.
    property string displayText: ""

    // Placeholder text when currentValue is empty/null
    property string placeholderText: "全部"

    // Read-only: whether the popup is currently open
    readonly property bool open: popupMenu.visible

    // Maximum height of the dropdown list before scrolling (0 = no limit)
    property int maxPopupHeight: 260

    // Emitted when user selects an item from the list
    signal valueSelected(var value)

    // ── Internal helpers ──
    function _itemValue(item) {
        if (typeof item === "object" && item !== null)
            return item[valueKey]
        return item
    }

    function _itemLabel(item) {
        if (typeof item === "object" && item !== null)
            return item[labelKey] !== undefined ? String(item[labelKey]) : String(item[valueKey])
        return String(item)
    }

    function _resolvedLabel() {
        if (displayText !== "")
            return displayText
        if (labelFn && typeof labelFn === "function")
            return labelFn(currentValue)
        for (var i = 0; i < model.length; i++) {
            if (_itemValue(model[i]) === currentValue)
                return _itemLabel(model[i])
        }
        return placeholderText
    }

    // ── Visual: trigger area ──
    color: (triggerMouse.containsMouse || popupMenu.visible) ? "#1e3260" : "#0c0e14"
    border.color: (triggerMouse.containsMouse || popupMenu.visible || (currentValue !== "" && currentValue !== null)) ? "#5078e0" : StyleTokens.borderLight
    border.width: 1

    Behavior on color { ColorAnimation { duration: 150 } }
    Behavior on border.color { ColorAnimation { duration: 150 } }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 4
        spacing: 4

        Text {
            Layout.fillWidth: true
            text: _resolvedLabel()
            color: (currentValue !== "" && currentValue !== null) ? StyleTokens.accentLink : "#788090"
            font.pixelSize: StyleTokens.fontSizeSm
            elide: Text.ElideRight
        }
        Text {
            text: "\u25BE"
            color: StyleTokens.textMuted
            font.pixelSize: StyleTokens.fontSizeXs
        }
    }

    MouseArea {
        id: triggerMouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            if (popupMenu.visible)
                popupMenu.close()
            else
                popupMenu.open()
        }
    }

    // ── Popup dropdown list ──
    Popup {
        id: popupMenu
        y: parent.height + 4
        width: Math.max(parent.width, 120)
        padding: 4
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

        background: Rectangle {
            color: "#151922"
            radius: StyleTokens.radiusLg
            border.color: StyleTokens.bgElevated
        }

        enter: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180; easing.type: Easing.OutCubic }
                NumberAnimation { property: "y"; from: popupMenu.y - 4; to: popupMenu.y; duration: 180; easing.type: Easing.OutCubic }
            }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120 }
        }

        contentItem: Flickable {
            id: popupFlick
            implicitHeight: Math.min(popupInner.implicitHeight + 8, root.maxPopupHeight)
            contentHeight: popupInner.implicitHeight + 4
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            ColumnLayout {
                id: popupInner
                width: popupFlick.width
                spacing: 2
                Repeater {
                    model: root.model
                    Rectangle {
                        Layout.fillWidth: true
                        height: 26
                        radius: StyleTokens.radiusSm
                        color: {
                            var val = root._itemValue(modelData)
                            if (val === root.currentValue)
                                return StyleTokens.accentSubtle
                            if (itemMouse.containsMouse)
                                return "#1a2440"
                            return "transparent"
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 10
                            text: root._itemLabel(modelData)
                            color: root._itemValue(modelData) === root.currentValue ? StyleTokens.accentHover : "#9094a8"
                            font.pixelSize: StyleTokens.fontSizeSm
                            font.weight: root._itemValue(modelData) === root.currentValue ? Font.DemiBold : Font.Normal
                        }

                        MouseArea {
                            id: itemMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                var val = root._itemValue(modelData)
                                root.currentValue = val
                                root.valueSelected(val)
                                popupMenu.close()
                            }
                        }
                    }
                }
            }
        }
    }
}
