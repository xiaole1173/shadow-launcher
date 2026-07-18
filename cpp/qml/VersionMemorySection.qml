// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

// ═══ 版本级内存设置 — 三选项 + 双条 ═══
Item {
    id: root

    property var backend: null
    property string currentSelectedVersion: ""

    // ── 分配模式: 0=跟随全局, 1=自动, 2=自定义 ──
    property int allocMode: 0
    property int customMB: 2048

    // ── 全局快照 ──
    property int  globalMaxMB: 2048
    property bool globalAuto: true

    // ── 系统内存 (init with non-zero defaults so bars render immediately) ──
    property real sysTotalMB: 8192
    property real sysAvailMB: 4096
    property real sysUsedMB: 4096
    property int  autoRecMB: 2048

    function sliderMax() { return Math.max(1024, Math.floor(sysAvailMB / 256) * 256) }
    function gameAllocMB() {
        if (allocMode === 2) return customMB
        if (allocMode === 1) return autoRecMB
        return globalAuto ? autoRecMB : globalMaxMB
    }
    function gameAllocGB() { return (gameAllocMB() / 1024).toFixed(1) }
    function sysAvailGB()  { return (sysAvailMB / 1024).toFixed(1) }
    function sysTotalGB()  { return (sysTotalMB / 1024).toFixed(1) }

    Component.onCompleted: {}  // refreshAll() called by Loader

    Timer {
        id: memPoller; interval: 2000; running: false; repeat: true
        onTriggered: {
            if (!backend) return
            var mem = backend.getMemoryStatus()
            if (mem) { sysTotalMB = mem.total || 4096; sysAvailMB = mem.available || 2048; sysUsedMB = sysTotalMB - sysAvailMB }
        }
    }

    function refreshAll() {
        if (!backend) return
        globalMaxMB = backend.maxMemoryMb
        globalAuto  = backend.autoMemoryEnabled
        var mem = backend.getMemoryStatus()
        if (mem) {
            sysTotalMB = mem.total || 4096
            sysAvailMB = mem.available || 2048
            sysUsedMB = sysTotalMB - sysAvailMB
            autoRecMB = mem.recommended || 2048
        }
        if (currentSelectedVersion) {
            allocMode = backend.versionMemoryMode(currentSelectedVersion)
            customMB  = Math.max(512, backend.versionMemoryManualMB(currentSelectedVersion))
        }
        var sMax = sliderMax()
        if (customMB > sMax) customMB = sMax
        memPoller.start()
    }

    Connections {
        target: backend
        enabled: backend !== null
        function onMemorySettingsChanged() {
            if (!backend) return
            globalMaxMB = backend.maxMemoryMb
            globalAuto  = backend.autoMemoryEnabled
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 12

        // ═══ 标题 ═══
        Text { text: qsTr("内存设置"); font.pixelSize: StyleTokens.fontSizeXl; font.bold: true; color: StyleTokens.textPrimary }

        // ── 跟随全局设置 ──
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 40; radius: StyleTokens.radiusMd
            color: allocMode === 0 ? "#181F30" : "#0f111a"
            border.color: allocMode === 0 ? "#4A8FE7" : "#1a1f2a"; border.width: allocMode === 0 ? 1.5 : 1
            Behavior on color { ColorAnimation { duration: 200 } }
            Behavior on border.color { ColorAnimation { duration: 200 } }

            RowLayout {
                anchors.fill: parent; anchors.margins: 12; spacing: 8
                Rectangle { width: 16; height: 16; radius: StyleTokens.radiusLg; color: "transparent"; border.color: allocMode === 0 ? "#4A8FE7" : "#5A6173"; border.width: 2
                    Rectangle { anchors.centerIn: parent; width: 7; height: 7; radius: StyleTokens.radiusXs; color: allocMode === 0 ? "#4A8FE7" : "transparent" } }
                Text { text: qsTr("跟随全局设置"); font.pixelSize: StyleTokens.fontSizeMd; color: allocMode === 0 ? "#e8ecf8" : "#8890a0"; Layout.fillWidth: true }
                Text { font.pixelSize: StyleTokens.fontSizeXs; color: "#5A6173"; text: globalAuto ? qsTr("自动") : globalMaxMB + " MB" }
            }
            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                onClicked: { if (!backend || !currentSelectedVersion) return; allocMode = 0; backend.setVersionMemoryMode(currentSelectedVersion, 0); refreshAll() } }
        }

        // ── 自动配置 ──
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 48; radius: StyleTokens.radiusMd
            color: allocMode === 1 ? "#181F30" : "#0f111a"
            border.color: allocMode === 1 ? "#4A8FE7" : "#1a1f2a"; border.width: allocMode === 1 ? 1.5 : 1
            Behavior on color { ColorAnimation { duration: 200 } }
            Behavior on border.color { ColorAnimation { duration: 200 } }

            ColumnLayout {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left; anchors.leftMargin: 14
                anchors.right: parent.right; anchors.rightMargin: 14
                spacing: 2

                RowLayout { spacing: 8
                    Rectangle { width: 16; height: 16; radius: StyleTokens.radiusLg; color: "transparent"; border.color: allocMode === 1 ? "#4A8FE7" : "#5A6173"; border.width: 2
                        Rectangle { anchors.centerIn: parent; width: 7; height: 7; radius: StyleTokens.radiusXs; color: allocMode === 1 ? "#4A8FE7" : "transparent" } }
                    Text { text: qsTr("自动配置"); font.pixelSize: StyleTokens.fontSizeMd; color: allocMode === 1 ? "#e8ecf8" : "#8890a0" }
                }
                Text { text: qsTr("启动时根据剩余内存动态分配"); font.pixelSize: StyleTokens.fontSizeXs; color: "#5A6173"; Layout.leftMargin: 24 }
            }
            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                onClicked: { if (!backend || !currentSelectedVersion) return; allocMode = 1; backend.setVersionMemoryMode(currentSelectedVersion, 1); refreshAll() } }
        }

        // ── 自定义 ──
        Rectangle {
            id: customRow
            Layout.fillWidth: true; Layout.preferredHeight: allocMode === 2 ? 84 : 40; radius: StyleTokens.radiusMd
            color: allocMode === 2 ? "#181F30" : "#0f111a"
            border.color: allocMode === 2 ? "#4A8FE7" : "#1a1f2a"; border.width: allocMode === 2 ? 1.5 : 1
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
                    Text { text: qsTr("自定义"); font.pixelSize: StyleTokens.fontSizeMd; color: allocMode === 2 ? "#e8ecf8" : "#8890a0"; Layout.fillWidth: true }
                    Text { visible: allocMode === 2; text: customMB + " MB"; font.pixelSize: StyleTokens.fontSizeMd; font.bold: true; color: StyleTokens.accentLink }
                }

                Item { Layout.fillWidth: true; Layout.preferredHeight: 30; visible: allocMode === 2
                    Rectangle { id: track; anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left; anchors.right: parent.right; height: 4; radius: StyleTokens.radiusXs; color: StyleTokens.bgInput
                        Rectangle { anchors.verticalCenter: parent.verticalCenter; height: 4; radius: StyleTokens.radiusXs; color: StyleTokens.info
                            width: sliderMax() > 512 ? ((customMB - 512) / (sliderMax() - 512)) * parent.width : 0
                            Behavior on width { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } } } }
                    Rectangle { id: knob; width: 18; height: 18; radius: StyleTokens.radiusLg; anchors.verticalCenter: parent.verticalCenter
                        color: dragArea.drag.active ? "#7BA8F0" : "#5A9CF0"; border.color: "#FFFFFF"; border.width: 1.5
                        x: sliderMax() > 512 ? ((customMB - 512) / (sliderMax() - 512)) * (track.width - width) : 0
                        Behavior on x { enabled: !dragArea.drag.active; NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                        MouseArea { id: dragArea; anchors.fill: parent; drag.target: parent; drag.axis: Drag.XAxis
                            drag.minimumX: 0; drag.maximumX: track.width - parent.width; cursorShape: Qt.PointingHandCursor
                            onPositionChanged: {
                                var r = knob.x / Math.max(1, track.width - knob.width)
                                customMB = Math.round((512 + r * (sliderMax() - 512)) / 128) * 128
                                customMB = Math.max(512, Math.min(sliderMax(), customMB))
                                if (backend && currentSelectedVersion) backend.setVersionMemoryManualMB(currentSelectedVersion, customMB)
                            } }
                    }
                    MouseArea { anchors.fill: parent; z: -1; cursorShape: Qt.PointingHandCursor
                        onClicked: function(m) {
                            var r = Math.max(0, Math.min(1, m.x / track.width))
                            customMB = Math.round((512 + r * (sliderMax() - 512)) / 128) * 128
                            customMB = Math.max(512, Math.min(sliderMax(), customMB))
                            if (backend && currentSelectedVersion) backend.setVersionMemoryManualMB(currentSelectedVersion, customMB)
                        } }
                }
            }

            MouseArea { anchors.fill: parent; z: -1; cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (!backend || !currentSelectedVersion) return
                    allocMode = 2; backend.setVersionMemoryMode(currentSelectedVersion, 2)
                    customMB = autoRecMB; var max = sliderMax(); if (customMB > max) customMB = max
                    backend.setVersionMemoryManualMB(currentSelectedVersion, customMB)
                } }
        }

        // ═══ 内存可视化条 (Layout.fillWidth, 同 SettingsMemorySection) ═══
        Rectangle {
            id: barContainer
            Layout.fillWidth: true; Layout.preferredHeight: 64; radius: StyleTokens.radiusMd
            color: "#11141c"; border.color: StyleTokens.bgInput

            property real usedW: sysTotalMB > 0 ? sysUsedMB / sysTotalMB : 0
            property real gameW: sysTotalMB > 0 ? gameAllocMB() / sysTotalMB : 0
            property real bw: barContainer.width - 36

            Text { x: 18; y: 10; text: qsTr("已使用内存"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary }
            Text { x: 18 + barContainer.bw * barContainer.usedW; y: 10; text: qsTr("游戏分配"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary
                Behavior on x { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } } }

            Item {
                x: 18; y: 26
                width: barContainer.bw; height: 6

                Rectangle { anchors.fill: parent; radius: StyleTokens.radiusXs; color: StyleTokens.surfaceLight }

                Rectangle {
                    id: usedBar
                    anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
                    height: 6; radius: StyleTokens.radiusXs
                    width: Math.max(0, parent.width * barContainer.usedW)
                    color: "#4E88C8"
                    Behavior on width { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }
                }

                Rectangle {
                    id: gameBar
                    anchors.left: usedBar.right; anchors.leftMargin: 3
                    anchors.verticalCenter: parent.verticalCenter
                    height: 6; radius: StyleTokens.radiusXs
                    width: Math.max(0, parent.width * barContainer.gameW - 3)
                    color: "#88B8E0"
                    Behavior on width { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }
                    Behavior on anchors.leftMargin { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }
                }
            }

            Text { x: 18; y: 40; text: sysAvailGB() + " GB / " + sysTotalGB() + " GB"; font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.Medium; color: "#C0C8D8" }
            Text { x: 18 + barContainer.bw * barContainer.usedW; y: 40; text: gameAllocGB() + " GB"; font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.Medium; color: "#9CB8E0"
                Behavior on x { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } } }
        }
    }
}
