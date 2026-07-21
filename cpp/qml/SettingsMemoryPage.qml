// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

// ═══ 游戏内存 — 严格对齐 SettingsMemorySection 视觉 + 新增"跟随全局" ═══
Rectangle {
    id: root
    color: StyleTokens.bgSecondary

    property var backend: null
    signal goBack()

    // ── 分配模式: 0=跟随全局, 1=自动, 2=自定义 ──
    property int allocMode: 0
    property int customMB: 2048

    // ── 全局快照（跟随全局时的显示用）──
    property int  globalMaxMB: 2048
    property bool globalAuto: true

    // ── 系统内存 ──
    property real sysTotalMB: 4096
    property real sysAvailMB: 4096
    property real sysUsedMB: 0
    property int  autoRecommendedMB: 2048

    function gameAllocMB() {
        if (allocMode === 2) return customMB
        if (allocMode === 1) return autoRecommendedMB
        return globalAuto ? autoRecommendedMB : globalMaxMB
    }
    function gameAllocGB() { return (gameAllocMB() / 1024).toFixed(1) }
    function sysAvailGB()  { return (sysAvailMB / 1024).toFixed(1) }
    function sysTotalGB()  { return (sysTotalMB / 1024).toFixed(1) }
    function sliderMax()   { return Math.max(1024, Math.floor(sysAvailMB / 256) * 256) }

    Component.onCompleted: {}  // init 由外部注入 backend 后调用

    function refreshAll() {
        if (!backend) return
        globalMaxMB = backend.maxMemoryMb
        globalAuto  = backend.autoMemoryEnabled
        customMB = Math.max(512, globalMaxMB)
        var mem = backend.getMemoryStatus()
        if (mem) {
            sysTotalMB = mem.total || 4096
            sysAvailMB = mem.available || 2048
            sysUsedMB = sysTotalMB - sysAvailMB
            autoRecommendedMB = mem.recommended || 2048
        }
        var max = sliderMax()
        if (customMB > max) { customMB = max; if (backend) backend.setMaxMemory(customMB) }
        memPoller.start()
    }

    Timer {
        id: memPoller; interval: 2000; running: false; repeat: true
        onTriggered: {
            if (!backend) return
            var mem = backend.getMemoryStatus()
            if (mem) { sysTotalMB = mem.total || 4096; sysAvailMB = mem.available || 2048; sysUsedMB = sysTotalMB - sysAvailMB }
        }
    }

    Connections {
        target: backend; enabled: backend !== null
        function onMemorySettingsChanged() { if (allocMode === 0) refreshAll() }
    }

    // ── 入场动画 ──
    opacity: 0; y: 10
    Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
    Behavior on y       { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
    Component.onDestruction: memPoller.stop()

    // ── 内容 ──
    Flickable {
        anchors.fill: parent; contentHeight: col.implicitHeight + 40; boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: col
            width: parent.width; spacing: 12

            // 标题 + 返回
            RowLayout {
                Layout.fillWidth: true; Layout.topMargin: 16; Layout.leftMargin: 20; Layout.rightMargin: 20
                Text { id: backBtn; text: "← 返回"; font.pixelSize: StyleTokens.fontSizeMd; color: StyleTokens.textTertiary
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.goBack() } }
                Item { Layout.fillWidth: true }
            }

            Text {
                Layout.leftMargin: 20
                text: qsTr("内存设置"); font.pixelSize: StyleTokens.fontSizeXl; font.bold: true; color: StyleTokens.textPrimary
            }

            // ── 选项容器（对齐 SettingsMemorySection 边距）──
            Item { Layout.fillWidth: true; implicitHeight: optionsCol.implicitHeight
                ColumnLayout {
                    id: optionsCol
                    anchors.left: parent.left; anchors.leftMargin: 20
                    anchors.right: parent.right; anchors.rightMargin: 20
                    spacing: 8

                    // ═══ 跟随全局 ═══
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 40; radius: StyleTokens.radiusMd
                        color: allocMode === 0 ? "#181F30" : "#0f111a"
                        border.color: allocMode === 0 ? "#4A8FE7" : "#1a1f2a"
                        border.width: allocMode === 0 ? 1.5 : 1
                        Behavior on color { ColorAnimation { duration: 200 } }
                        Behavior on border.color { ColorAnimation { duration: 200 } }
                        RowLayout {
                            anchors.fill: parent; anchors.margins: 12; spacing: 8
                            Rectangle { width: 16; height: 16; radius: StyleTokens.radiusLg; color: "transparent"; border.color: allocMode === 0 ? "#4A8FE7" : "#5A6173"; border.width: 2
                                Rectangle { anchors.centerIn: parent; width: 7; height: 7; radius: StyleTokens.radiusXs; color: allocMode === 0 ? "#4A8FE7" : "transparent" } }
                            Text { text: qsTr("跟随全局设置"); font.pixelSize: StyleTokens.fontSizeMd; color: allocMode === 0 ? "#e8ecf8" : "#8890a0"; Layout.fillWidth: true }
                            Text { font.pixelSize: StyleTokens.fontSizeXs; color: "#5A6173"; text: globalAuto ? qsTr("自动") : globalMaxMB + " MB" }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                onClicked: { allocMode = 0; if (allocMode === 0) refreshAll() } }
                        }
                    }

                    // ═══ 自动配置 ═══
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 40; radius: StyleTokens.radiusMd
                        color: allocMode === 1 ? "#181F30" : "#0f111a"
                        border.color: allocMode === 1 ? "#4A8FE7" : "#1a1f2a"
                        border.width: allocMode === 1 ? 1.5 : 1
                        Behavior on color { ColorAnimation { duration: 200 } }
                        Behavior on border.color { ColorAnimation { duration: 200 } }
                        RowLayout {
                            anchors.fill: parent; anchors.margins: 12; spacing: 8
                            Rectangle { width: 16; height: 16; radius: StyleTokens.radiusLg; color: "transparent"; border.color: allocMode === 1 ? "#4A8FE7" : "#5A6173"; border.width: 2
                                Rectangle { anchors.centerIn: parent; width: 7; height: 7; radius: StyleTokens.radiusXs; color: allocMode === 1 ? "#4A8FE7" : "transparent" } }
                            Text { text: qsTr("自动配置"); font.pixelSize: StyleTokens.fontSizeMd; color: allocMode === 1 ? "#e8ecf8" : "#8890a0"; Layout.fillWidth: true }
                            Text { text: qsTr("启动时动态分配"); font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.textMuted }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                onClicked: { allocMode = 1; if (backend) backend.setAutoMemoryEnabled(true) } }
                        }
                    }

                    // ═══ 自定义 ═══
                    Rectangle {
                        id: customRow
                        Layout.fillWidth: true; Layout.preferredHeight: allocMode === 2 ? 78 : 40; radius: StyleTokens.radiusMd
                        color: allocMode === 2 ? "#181F30" : "#0f111a"
                        border.color: allocMode === 2 ? "#4A8FE7" : "#1a1f2a"
                        border.width: allocMode === 2 ? 1.5 : 1
                        Behavior on color { ColorAnimation { duration: 200 } }
                        Behavior on border.color { ColorAnimation { duration: 200 } }
                        Behavior on Layout.preferredHeight { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
                        clip: true

                        ColumnLayout {
                            anchors.left: parent.left; anchors.leftMargin: 12
                            anchors.right: parent.right; anchors.rightMargin: 12
                            anchors.top: parent.top; anchors.topMargin: 8
                            spacing: allocMode === 2 ? 8 : 0

                            RowLayout { spacing: 8
                                Rectangle { width: 16; height: 16; radius: StyleTokens.radiusLg; color: "transparent"; border.color: allocMode === 2 ? "#4A8FE7" : "#5A6173"; border.width: 2
                                    Rectangle { anchors.centerIn: parent; width: 7; height: 7; radius: StyleTokens.radiusXs; color: allocMode === 2 ? "#4A8FE7" : "transparent" } }
                                Text { text: qsTr("自定义"); font.pixelSize: StyleTokens.fontSizeMd; color: allocMode === 2 ? "#e8ecf8" : "#8890a0" }
                                Item { Layout.fillWidth: true }
                                Text { visible: allocMode === 2; text: customMB + " MB"; font.pixelSize: StyleTokens.fontSizeMd; font.bold: true; color: StyleTokens.accentLink }
                            }

                            Item { Layout.fillWidth: true; Layout.preferredHeight: 30; visible: allocMode === 2
                                // track
                                Rectangle { id: mpTrack; anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left; anchors.right: parent.right; height: 4; radius: StyleTokens.radiusXs; color: StyleTokens.bgInput
                                    Rectangle { anchors.verticalCenter: parent.verticalCenter; height: 4; radius: StyleTokens.radiusXs; color: StyleTokens.info
                                        width: ((customMB - 512) / (sliderMax() - 512)) * parent.width
                                        Behavior on width { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } } } }
                                // knob
                                Rectangle { id: mpKnob; width: 18; height: 18; radius: StyleTokens.radiusLg; anchors.verticalCenter: parent.verticalCenter
                                    color: mpDrag.drag.active ? "#7BA8F0" : "#5A9CF0"; border.color: "#FFFFFF"; border.width: 1.5
                                    x: ((customMB - 512) / (sliderMax() - 512)) * (mpTrack.width - width)
                                    Behavior on x { enabled: !mpDrag.drag.active; NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                                    MouseArea { id: mpDrag; anchors.fill: parent; drag.target: parent; drag.axis: Drag.XAxis
                                        drag.minimumX: 0; drag.maximumX: mpTrack.width - parent.width; cursorShape: Qt.PointingHandCursor
                                        onPositionChanged: {
                                            var r = mpKnob.x / (mpTrack.width - mpKnob.width)
                                            customMB = Math.round((512 + r * (sliderMax() - 512)) / 128) * 128
                                            customMB = Math.max(512, Math.min(sliderMax(), customMB))
                                            if (backend) backend.setMaxMemory(customMB)
                                        } }
                                }
                                MouseArea { anchors.fill: parent; z: -1; cursorShape: Qt.PointingHandCursor
                                    onClicked: function(m) {
                                        var r = Math.max(0, Math.min(1, m.x / mpTrack.width))
                                        customMB = Math.round((512 + r * (sliderMax() - 512)) / 128) * 128
                                        customMB = Math.max(512, Math.min(sliderMax(), customMB))
                                        if (backend) backend.setMaxMemory(customMB)
                                    } }
                            }
                        }

                        MouseArea { anchors.fill: parent; z: -1; cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                allocMode = 2
                                if (backend) backend.setAutoMemoryEnabled(false)
                                customMB = autoRecommendedMB
                                var max = sliderMax(); if (customMB > max) customMB = max
                                if (backend) backend.setMaxMemory(customMB)
                            } }
                    }
                }
            }

            // ═══ 内存可视化条（双条，对齐 SettingsMemorySection） ═══
            Rectangle {
                Layout.fillWidth: true; Layout.margins: 20; Layout.preferredHeight: 56; radius: StyleTokens.radiusMd
                color: "#11141c"; border.color: StyleTokens.bgInput
                property real bw: width - 28
                property real usedFrac: sysTotalMB > 0 ? sysUsedMB / sysTotalMB : 0
                property real gameFrac: sysTotalMB > 0 ? gameAllocMB() / sysTotalMB : 0

                Text { x: 14; y: 6; text: qsTr("已使用内存"); font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.textTertiary }
                Text { id: gameLabel; x: 14 + bw * usedFrac; y: 6
                    text: qsTr("游戏分配"); font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.textTertiary
                    Behavior on x { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } } }

                Item { x: 14; y: 20; width: bw; height: 6
                    Rectangle { anchors.fill: parent; radius: StyleTokens.radiusXs; color: StyleTokens.surfaceLight }
                    Rectangle { id: usedBar; anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
                        height: 6; radius: StyleTokens.radiusXs; color: "#4E88C8"
                        width: Math.max(0, parent.width * usedFrac)
                        Behavior on width { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } } }
                    Rectangle { id: gameBar; anchors.left: usedBar.right; anchors.leftMargin: 3
                        anchors.verticalCenter: parent.verticalCenter; height: 6; radius: StyleTokens.radiusXs; color: "#88B8E0"
                        width: Math.max(0, parent.width * gameFrac - 3)
                        Behavior on width { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } } }
                }

                Text { x: 14; y: 32; text: sysAvailGB() + " GB / " + sysTotalGB() + " GB"
                    font.pixelSize: StyleTokens.fontSizeSm; font.weight: Font.Medium; color: "#C0C8D8" }
                Text { id: gameNum; x: 14 + bw * usedFrac; y: 32
                    text: gameAllocGB() + " GB"; font.pixelSize: StyleTokens.fontSizeSm; font.weight: Font.Medium; color: "#9CB8E0"
                    Behavior on x { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } } }
            }
        }
    }
}
