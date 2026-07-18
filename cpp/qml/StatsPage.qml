// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// ═══ 游戏时长统计页 ═══

Item {
    id: root
    anchors.fill: parent

    // backend is explicitly injected from the Loader in MainWindow.qml
    // All stats properties are on backend directly (QML var can't access sub-objects)
    property var backend: null

    // ── Local mirrors driven by Connections ──
    property bool svcLoading: false
    property bool svcEmpty:   true

    Connections {
        target: backend
        enabled: backend !== null
        function onStatsLoadingChanged()  { if (backend) svcLoading = backend.statsLoading }
        function onStatsChanged()         { if (backend) svcEmpty   = backend.statsEmpty; svcLoading = backend.statsLoading }
    }

    // ── When backend arrives, trigger scan ──
    onBackendChanged: {
        console.log("[StatsPage] backend arrived, calling refreshGameStats")
        if (backend) {
            svcLoading = backend.statsLoading
            svcEmpty   = backend.statsEmpty
            backend.refreshGameStats()
        }
    }

    // ── Init ──
    opacity: 0
    Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
    Component.onCompleted: opacity = 1

    function onVisible() {
        console.log("[StatsPage] onVisible called")
        if (backend) backend.refreshGameStats()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 14

        Text {
            text: qsTr("统计")
            font.pixelSize: StyleTokens.fontSizeXl
            font.bold: true
            color: StyleTokens.textPrimary
        }

        // ── DIAGNOSTIC ──
        Text {
            visible: false
            text: "[DIAG] backend=" + (backend ? "OK" : "NULL")
                + " | load=" + svcLoading
                + " | empty=" + svcEmpty
                + " | cpp_load=" + (backend ? backend.statsLoading : "?")
                + " | cpp_empty=" + (backend ? backend.statsEmpty : "?")
                + " | hours=" + (backend ? backend.totalGameHours : "?")
                + " | count=" + (backend && backend.versionGameStats ? backend.versionGameStats.length : "?")
            font.pixelSize: StyleTokens.fontSizeSm
            color: "#ff6666"
            font.family: "Consolas"
        }

        // ── Content slot ──
        Item {
            id: contentSlot
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Not ready
            Rectangle {
                anchors.fill: parent
                visible: !backend
                color: "transparent"
                Text {
                    anchors.centerIn: parent
                    text: qsTr("统计模块初始化中……")
                    font.pixelSize: StyleTokens.fontSizeMd
                    color: StyleTokens.textMuted
                }
            }

            // Loading
            Item {
                id: loadingView
                anchors.fill: parent
                visible: backend !== null && svcLoading

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 12

                    Canvas {
                        id: spinner
                        Layout.preferredWidth: 32; Layout.preferredHeight: 32
                        property real angle: 0
                        onAngleChanged: requestPaint()
                        RotationAnimation on angle {
                            from: 0; to: 360
                            duration: 900
                            loops: Animation.Infinite
                            running: loadingView.visible
                        }
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            ctx.strokeStyle = "#5068d8"
                            ctx.lineWidth = 2.5
                            ctx.lineCap = "round"
                            ctx.beginPath()
                            ctx.arc(width/2, height/2, 10, angle * Math.PI / 180, (angle + 290) * Math.PI / 180)
                            ctx.stroke()
                        }
                    }

                    Text {
                        text: qsTr("加载中……")
                        font.pixelSize: StyleTokens.fontSizeMd
                        color: "#606478"
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }

            // Empty
            Item {
                id: emptyView
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 24
                visible: backend !== null && !svcLoading && svcEmpty

                Text {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("暂无时长统计，您还没有安装任何版本或没有存档！")
                    font.pixelSize: StyleTokens.fontSizeMd
                    color: StyleTokens.textMuted
                }
            }

            // Data
            ColumnLayout {
                id: dataView
                anchors.fill: parent
                visible: backend !== null && !svcLoading && !svcEmpty
                spacing: 16

                // Total hours
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 72
                    radius: StyleTokens.radiusLg
                    color: StyleTokens.bgSecondary
                    border.color: StyleTokens.bgInput

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 20

                        Text {
                            text: qsTr("总游戏时长")
                            font.pixelSize: StyleTokens.fontSizeMd
                            color: StyleTokens.textTertiary
                        }

                        Item { Layout.fillWidth: true }

                        Text {
                            text: (backend ? backend.totalGameHours.toFixed(2) : "0.0") + " " + qsTr("小时")
                            font.pixelSize: StyleTokens.fontSize2xl
                            font.bold: true
                            color: StyleTokens.textPrimary
                        }
                    }
                }

                // Bar chart
                Rectangle {
                    id: chartBox
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: StyleTokens.radiusLg
                    color: StyleTokens.bgSecondary
                    border.color: StyleTokens.bgInput
                    clip: true

                    ColumnLayout {
                        id: barColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.leftMargin: 16
                        anchors.topMargin: 16
                        anchors.rightMargin: 24
                        spacing: 10

                        Repeater {
                            model: backend ? backend.versionGameStats : []
                            delegate: Item {
                                id: barItem
                                Layout.fillWidth: true
                                Layout.preferredHeight: 28
                                property real w: barWidth(modelData.hours)

                                HoverHandler {
                                    id: rowHover
                                }

                                ToolTip {
                                    id: tip
                                    visible: rowHover.hovered && nameLabel.truncated
                                    text: modelData.displayName || modelData.versionId || ""
                                    delay: 0
                                    timeout: -1
                                    x: {
                                        var cursorX = rowHover.point.position.x
                                        var gap = 12
                                        var tipW = tip.width > 0 ? tip.width : 0
                                        return (cursorX + gap + tipW > barItem.width)
                                            ? cursorX - gap - tipW
                                            : cursorX + gap
                                    }
                                    y: rowHover.point.position.y - 24
                                }

                                Text {
                                    id: nameLabel
                                    anchors.left: parent.left
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: modelData.displayName || modelData.versionId || ""
                                    font.pixelSize: StyleTokens.fontSizeSm
                                    color: "#b0b8c8"
                                    width: 80
                                    elide: Text.ElideRight
                                }

                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 88
                                    anchors.verticalCenter: parent.verticalCenter
                                    height: 16
                                    radius: StyleTokens.radiusSm
                                    width: w
                                    color: barColor(index)
                                    Behavior on width {
                                        NumberAnimation { duration: 500; easing.type: Easing.OutCubic }
                                    }
                                }

                                Text {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 88 + w + 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: modelData.hours.toFixed(2) + "h"
                                    font.pixelSize: StyleTokens.fontSizeSm
                                    color: "#707888"
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    function barWidth(hours) {
        if (!backend || !backend.versionGameStats || backend.versionGameStats.length === 0) return 100
        var maxH = 0
        for (var i = 0; i < backend.versionGameStats.length; i++) {
            if (backend.versionGameStats[i].hours > maxH)
                maxH = backend.versionGameStats[i].hours
        }
        if (maxH <= 0) return 100
        var avail = chartBox.width - 88 - 60 - 40
        if (avail < 100) avail = 300
        return Math.max(8, (hours / maxH) * avail)
    }

    function barColor(index) {
        var colors = ["#5068d8", "#5588cc", "#5aa8c0", "#60c8b4", "#6fd4ac",
                      "#7ee0a0", "#8dec94", "#9cf888", "#abff7c", "#baff70"]
        return colors[index % colors.length]
    }
}
