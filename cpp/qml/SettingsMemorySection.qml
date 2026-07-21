// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

// ═══ 全局内存设置 — 简洁两选项 + 蓝色细条可视化 ═══
Item {
    id: root

    property var backend: null

    // ── 分配模式: 0=自动, 1=自定义 ──
    property int allocMode: 0
    property int customMB: 2048

    // ── 系统内存 ──
    property real sysTotalMB: 4096
    property real sysAvailMB: 4096
    property real sysUsedMB: 0
    property int  autoRecommendedMB: 2048

    // ── 游戏分配值 ──
    function gameAllocMB() { return allocMode === 1 ? customMB : autoRecommendedMB }
    function gameAllocGB() { return (gameAllocMB() / 1024).toFixed(1) }
    function sysAvailGB()  { return (sysAvailMB / 1024).toFixed(1) }
    function sysTotalGB()  { return (sysTotalMB / 1024).toFixed(1) }

    // ── 滑块上限 = 剩余可用内存，但最低给 1024 ──
    function sliderMax() { return Math.max(1024, Math.floor(sysAvailMB / 256) * 256) }

    // ── 初始化 ──
    Component.onCompleted: {}  // Handled by Loader.onItemChanged

    // ── 实时轮询系统内存 ──
    Timer {
        id: memPoller
        interval: 2000; running: false; repeat: true
        onTriggered: {
            if (!backend) return
            var mem = backend.getMemoryStatus()
            if (mem) {
                sysTotalMB = mem.total || 4096
                sysAvailMB = mem.available || 2048
                sysUsedMB = sysTotalMB - sysAvailMB
            }
        }
    }

    function refreshAll() {
        if (!backend) return
        customMB = Math.max(512, backend.maxMemoryMb)
        allocMode = backend.autoMemoryEnabled ? 0 : 1
        var mem = backend.getMemoryStatus()
        if (mem) {
            sysTotalMB = mem.total || 4096
            sysAvailMB = mem.available || 2048
            sysUsedMB = sysTotalMB - sysAvailMB
            autoRecommendedMB = mem.recommended || 2048
        }
        // clamp customMB to sliderMax on init
        var max = sliderMax()
        if (customMB > max) {
            customMB = max
            if (backend) backend.setMaxMemory(customMB)
        }
        memPoller.start()
    }

    Connections {
        target: backend
        enabled: backend !== null
        function onMemorySettingsChanged() {
            var newMode = backend.autoMemoryEnabled ? 0 : 1
            if (newMode !== allocMode) {
                allocMode = newMode
                if (allocMode === 1) customMB = Math.max(512, backend.maxMemoryMb)
                refreshAll()
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        // ═══ 标题 ═══
        Text {
            text: qsTr("内存设置")
            font.pixelSize: StyleTokens.fontSizeXl; font.bold: true
            color: StyleTokens.textPrimary
        }

        // ═══ 选项 1: 自动配置 ═══
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 48; radius: StyleTokens.radiusMd
            color: allocMode === 0 ? "#181F30" : "#0f111a"
            border.color: allocMode === 0 ? "#4A8FE7" : "#1a1f2a"
            border.width: allocMode === 0 ? 1.5 : 1
            Behavior on color { ColorAnimation { duration: 200 } }
            Behavior on border.color { ColorAnimation { duration: 200 } }

            ColumnLayout {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left; anchors.leftMargin: 14
                anchors.right: parent.right; anchors.rightMargin: 14
                spacing: 2

                RowLayout {
                    Layout.fillWidth: true; spacing: 8
                    Rectangle {
                        width: 16; height: 16; radius: StyleTokens.radiusLg; color: "transparent"
                        border.color: allocMode === 0 ? "#4A8FE7" : "#5A6173"; border.width: 2
                        Rectangle {
                            anchors.centerIn: parent; width: 7; height: 7; radius: StyleTokens.radiusXs
                            color: allocMode === 0 ? "#4A8FE7" : "transparent"
                            Behavior on color { ColorAnimation { duration: 150 } }
                        }
                    }
                    Text {
                        text: qsTr("自动配置")
                        font.pixelSize: StyleTokens.fontSizeMd; font.weight: allocMode === 0 ? Font.Medium : Font.Normal
                        color: allocMode === 0 ? "#e8ecf8" : "#8890a0"
                    }
                }
                Text {
                    visible: allocMode === 0
                    text: qsTr("启动时根据剩余内存动态分配")
                    font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.textMuted
                    Layout.leftMargin: 24  // align with text after radio circle
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    allocMode = 0
                    if (backend) backend.setAutoMemoryEnabled(true)
                }
            }
        }

        // ═══ 选项 2: 自定义 ═══
        Rectangle {
            id: customRow
            Layout.fillWidth: true; Layout.preferredHeight: allocMode === 1 ? 84 : 48; radius: StyleTokens.radiusMd
            color: allocMode === 1 ? "#181F30" : "#0f111a"
            border.color: allocMode === 1 ? "#4A8FE7" : "#1a1f2a"
            border.width: allocMode === 1 ? 1.5 : 1
            Behavior on color { ColorAnimation { duration: 200 } }
            Behavior on border.color { ColorAnimation { duration: 200 } }
            Behavior on Layout.preferredHeight { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
            clip: true

            ColumnLayout {
                anchors.left: parent.left; anchors.leftMargin: 14
                anchors.right: parent.right; anchors.rightMargin: 14
                anchors.top: parent.top; anchors.topMargin: 10
                spacing: allocMode === 1 ? 8 : 0

                // 标签行
                RowLayout {
                    Layout.fillWidth: true; spacing: 8
                    Rectangle {
                        width: 16; height: 16; radius: StyleTokens.radiusLg; color: "transparent"
                        border.color: allocMode === 1 ? "#4A8FE7" : "#5A6173"; border.width: 2
                        Rectangle {
                            anchors.centerIn: parent; width: 7; height: 7; radius: StyleTokens.radiusXs
                            color: allocMode === 1 ? "#4A8FE7" : "transparent"
                            Behavior on color { ColorAnimation { duration: 150 } }
                        }
                    }
                    Text {
                        text: qsTr("自定义")
                        font.pixelSize: StyleTokens.fontSizeMd; font.weight: allocMode === 1 ? Font.Medium : Font.Normal
                        color: allocMode === 1 ? "#e8ecf8" : "#8890a0"
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        visible: allocMode === 1
                        text: customMB + " MB"
                        font.pixelSize: StyleTokens.fontSizeMd; font.bold: true
                        color: StyleTokens.accentLink
                    }
                }

                // 调节条 + 标尺（仅自定义模式）
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 30
                    visible: allocMode === 1; opacity: allocMode === 1 ? 1 : 0
                    Behavior on opacity { NumberAnimation { duration: 200 } }

                    // 轨道
                    Rectangle {
                        id: track
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left; anchors.right: parent.right
                        height: 4; radius: StyleTokens.radiusXs; color: StyleTokens.bgInput

                        // 已填充段
                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            height: 4; radius: StyleTokens.radiusXs
                            color: StyleTokens.info
                            width: ((customMB - 512) / (sliderMax() - 512)) * parent.width
                            Behavior on width { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
                        }

                        // 刻度标记
                        Repeater {
                            model: 5
                            Rectangle {
                                width: 1; height: 8; color: StyleTokens.bgHover
                                anchors.verticalCenter: track.verticalCenter
                                x: (index / 4) * (track.width - 1)
                            }
                        }
                    }

                    // 拖动 knob
                    Rectangle {
                        id: knob
                        width: 18; height: 18; radius: StyleTokens.radiusLg
                        anchors.verticalCenter: parent.verticalCenter
                        color: dragArea.drag.active ? "#7BA8F0" : "#5A9CF0"
                        border.color: "#FFFFFF"; border.width: 1.5
                        x: ((customMB - 512) / (sliderMax() - 512)) * (track.width - width)
                        visible: allocMode === 1; opacity: allocMode === 1 ? 1 : 0
                        Behavior on opacity { NumberAnimation { duration: 150 } }
                        Behavior on x { enabled: !dragArea.drag.active; NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

                        MouseArea {
                            id: dragArea
                            anchors.fill: parent
                            drag.target: parent
                            drag.axis: Drag.XAxis
                            drag.minimumX: 0
                            drag.maximumX: track.width - parent.width
                            cursorShape: Qt.PointingHandCursor
                            onPositionChanged: {
                                var r = knob.x / (track.width - knob.width)
                                customMB = Math.round((512 + r * (sliderMax() - 512)) / 128) * 128
                                customMB = Math.max(512, Math.min(sliderMax(), customMB))
                                if (backend) backend.setMaxMemory(customMB)
                            }
                        }
                    }

                    // 点击轨道跳转 (z:-1 → knob 的 drag MouseArea 优先)
                    MouseArea {
                        id: trackClick
                        anchors.fill: parent
                        z: -1
                        cursorShape: Qt.PointingHandCursor
                        onClicked: function(mouse) {
                            var r = Math.max(0, Math.min(1, mouse.x / track.width))
                            customMB = Math.round((512 + r * (sliderMax() - 512)) / 128) * 128
                            customMB = Math.max(512, Math.min(sliderMax(), customMB))
                            if (backend) backend.setMaxMemory(customMB)
                        }
                    }
                }
            }

            // 底层 MouseArea — 未选中时点击切换到自定义，继承自动推荐值
            MouseArea {
                anchors.fill: parent
                z: -1
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    allocMode = 1
                    if (backend) backend.setAutoMemoryEnabled(false)
                    // 继承自动推荐值而非旧 customMB
                    customMB = autoRecommendedMB
                    var max = sliderMax()
                    if (customMB > max) customMB = max
                    if (backend) backend.setMaxMemory(customMB)
                }
            }
        }

        // ═══ 内存可视化（双条布局） ═══
        Rectangle {
            id: barContainer
            Layout.fillWidth: true; Layout.preferredHeight: 64; radius: StyleTokens.radiusMd
            color: "#11141c"; border.color: StyleTokens.bgInput

            property real usedW: sysTotalMB > 0 ? sysUsedMB / sysTotalMB : 0
            property real gameW: sysTotalMB > 0 ? gameAllocMB() / sysTotalMB : 0

            // 可用宽度
            property real bw: width - 36  // margin

            // ── 标签行 ──
            Text {
                x: 18
                y: 10
                text: qsTr("已使用内存")
                font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary
            }
            Text {
                id: gameLabel
                x: 18 + barContainer.bw * barContainer.usedW
                y: 10
                text: qsTr("游戏分配")
                font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary
                Behavior on x { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }
            }

            // ── 双条（同基线）──
            Item {
                x: 18; y: 26
                width: barContainer.bw; height: 6

                // 背景
                Rectangle {
                    anchors.fill: parent; radius: StyleTokens.radiusXs; color: StyleTokens.surfaceLight
                }

                // 已使用
                Rectangle {
                    id: usedBar
                    anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
                    height: 6; radius: StyleTokens.radiusXs
                    width: Math.max(0, parent.width * barContainer.usedW)
                    color: "#4E88C8"
                    Behavior on width { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }
                }

                // 游戏分配（紧接已使用之后）
                Rectangle {
                    id: gameBar
                    anchors.left: usedBar.right
                    anchors.leftMargin: 3
                    anchors.verticalCenter: parent.verticalCenter
                    height: 6; radius: StyleTokens.radiusXs
                    width: Math.max(0, parent.width * barContainer.gameW - 3)
                    color: "#88B8E0"
                    Behavior on width { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }
                    Behavior on anchors.leftMargin { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }
                }
            }

            // ── 数字行 ──
            Text {
                x: 18
                y: 40
                text: sysAvailGB() + " GB / " + sysTotalGB() + " GB"
                font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.Medium; color: "#C0C8D8"
            }
            Text {
                id: gameNum
                x: 18 + barContainer.bw * barContainer.usedW
                y: 40
                text: gameAllocGB() + " GB"
                font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.Medium; color: "#9CB8E0"
                Behavior on x { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
