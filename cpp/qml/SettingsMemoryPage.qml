import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

// ═══ 游戏内存 — 实时内存状态 / 手动分配滑块 / 自动内存提示 ═══

Rectangle {
    id: memoryPage
    color: "#121418"

    property var backend: null

    property real sysTotalGB: 0
    property real sysAvailGB: 0
    property real sysUsedPercent: 0
    property int autoRecommendedMB: 0
    property int manualMemoryMB: 2048

    signal goBack()

    opacity: 0
    y: 10
    Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeInDuration; easing.type: AnimationTokens.itemFadeInEasing } }
    Behavior on y { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }

    Component.onCompleted: {
        memoryPage.opacity = 1
        memoryPage.y = 0
        refreshMemoryStatus()
    }

    function refreshMemoryStatus() {
        if (!backend) return
        var mem = backend.getMemoryStatus()
        if (mem) {
            sysTotalGB = mem.total / 1024.0
            sysAvailGB = mem.available / 1024.0
            sysUsedPercent = mem.percent
            autoRecommendedMB = mem.recommended
            if (manualMemoryMB < 512) manualMemoryMB = autoRecommendedMB
        }
    }

    Timer {
        id: memRefreshTimer
        interval: 3000
        running: memoryPage.visible
        repeat: true
        onTriggered: refreshMemoryStatus()
    }

    onVisibleChanged: {
        if (visible) {
            refreshMemoryStatus()
            memRefreshTimer.start()
        } else {
            memRefreshTimer.stop()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 16

        // ═══ Top bar ═══
        RowLayout {
            spacing: 12

            Rectangle {
                width: 60; height: 30; radius: 8
                color: "transparent"
                border.color: backMouse.containsMouse ? "#3B82F6" : "#2A2F3A"
                border.width: 1
                Text {
                    anchors.centerIn: parent
                    text: "← 设置"
                    color: backMouse.containsMouse ? "#3B82F6" : "#B4BAC6"
                    font.pixelSize: 13
                }
                MouseArea {
                    id: backMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: memoryPage.goBack()
                }
            }

            Text {
                text: "游戏内存"
                font.pixelSize: 22
                font.bold: true
                color: "#F1F3F6"
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#2A2F3A" }

        // ═══ 实时内存状态 ═══
        Text {
            text: "实时内存状态"
            font.pixelSize: 12; font.bold: true
            color: "#7E8596"
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 180
            radius: 8
            color: "#1A1D24"
            border.color: "#2A2F3A"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 14

                RowLayout {
                    spacing: 8
                    Text { text: "系统总内存:"; font.pixelSize: 14; color: "#B4BAC6" }
                    Text {
                        text: sysTotalGB.toFixed(1) + " GB"
                        font.pixelSize: 14; font.bold: true; color: "#F1F3F6"
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    spacing: 8
                    Text { text: "当前可用:"; font.pixelSize: 14; color: "#B4BAC6" }
                    Text {
                        text: sysAvailGB.toFixed(1) + " GB"
                        font.pixelSize: 14; font.bold: true
                        color: sysAvailGB < 2.0 ? "#F59E0B" : "#F1F3F6"
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    spacing: 8
                    Text { text: "使用率:"; font.pixelSize: 14; color: "#B4BAC6" }
                    Text {
                        text: sysUsedPercent.toFixed(0) + "%"
                        font.pixelSize: 14; font.bold: true; color: "#F1F3F6"
                    }
                    Item { Layout.fillWidth: true }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 18
                    radius: 9
                    color: "#2A2F3A"
                    Rectangle {
                        height: 18; radius: 9
                        width: parent.width * Math.min(1, sysUsedPercent / 100.0)
                        color: sysUsedPercent < 50 ? "#10B981" : (sysUsedPercent < 75 ? "#F59E0B" : "#EF4444")
                    }
                }

                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#2A2F3A" }

                RowLayout {
                    spacing: 8
                    Text { text: "预估分配:"; font.pixelSize: 14; color: "#B4BAC6" }
                    Text {
                        text: autoRecommendedMB + " MB"
                        font.pixelSize: 14; font.bold: true; color: "#3B82F6"
                    }
                    Item { Layout.fillWidth: true }
                }
            }
        }

        // ═══ 手动内存分配 ═══
        Text {
            text: "手动内存分配"
            font.pixelSize: 12; font.bold: true
            color: "#7E8596"
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 130
            radius: 8
            color: "#1A1D24"
            border.color: "#2A2F3A"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                RowLayout {
                    spacing: 6
                    Text { text: "分配大小:"; font.pixelSize: 14; color: "#B4BAC6" }
                    Text {
                        text: manualMemoryMB + " MB"
                        font.pixelSize: 18; font.bold: true; color: "#F1F3F6"
                    }
                    Item { Layout.fillWidth: true }
                }

                // Slider
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    color: "transparent"

                    Rectangle {
                        id: sliderTrack
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width; height: 6; radius: 3
                        color: "#2A2F3A"
                    }

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        height: 6; radius: 3
                        color: "#3B82F6"
                        width: {
                            var minVal = 512; var maxVal = 32768
                            return ((manualMemoryMB - minVal) / (maxVal - minVal)) * sliderTrack.width
                        }
                    }

                    Rectangle {
                        id: sliderHandle
                        width: 20; height: 20; radius: 10
                        anchors.verticalCenter: parent.verticalCenter
                        color: sliderDragArea.drag.active ? "#2563EB" : "#3B82F6"
                        border.color: "#FFFFFF"; border.width: 2
                        x: {
                            var minVal = 512; var maxVal = 32768
                            var ratio = (manualMemoryMB - minVal) / (maxVal - minVal)
                            return ratio * (sliderTrack.width - width)
                        }

                        MouseArea {
                            id: sliderDragArea
                            anchors.fill: parent
                            drag.target: parent
                            drag.axis: Drag.XAxis
                            drag.minimumX: 0
                            drag.maximumX: sliderTrack.width - parent.width
                            cursorShape: Qt.PointingHandCursor
                            onPositionChanged: {
                                var minVal = 512; var maxVal = 32768
                                var step = 256
                                var ratio = parent.x / (sliderTrack.width - parent.width)
                                var raw = minVal + ratio * (maxVal - minVal)
                                raw = Math.round(raw / step) * step
                                manualMemoryMB = Math.max(minVal, Math.min(maxVal, raw))
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: function(mouse) {
                            var minVal = 512; var maxVal = 32768; var step = 256
                            var clampedX = Math.max(0, Math.min(mouse.x - sliderHandle.width / 2, sliderTrack.width - sliderHandle.width))
                            var ratio = clampedX / (sliderTrack.width - sliderHandle.width)
                            var raw = minVal + ratio * (maxVal - minVal)
                            raw = Math.round(raw / step) * step
                            manualMemoryMB = Math.max(minVal, Math.min(maxVal, raw))
                        }
                    }
                }

                // Range labels
                RowLayout {
                    Text { text: "512 MB"; font.pixelSize: 11; color: "#5A6173" }
                    Item { Layout.fillWidth: true }
                    Text { text: "32 GB"; font.pixelSize: 11; color: "#5A6173" }
                }
            }
        }

        // ═══ 提示 ═══
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            radius: 8
            color: "#1A1D24"
            border.color: "#2A2F3A"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 8
                Text { text: "ℹ️"; font.pixelSize: 14; color: "#7E8596" }
                Text {
                    text: "游戏启动时将自动根据系统当前可用内存动态分配"
                    font.pixelSize: 12; color: "#7E8596"
                }
                Item { Layout.fillWidth: true }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
