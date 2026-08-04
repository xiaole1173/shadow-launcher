// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

// ═══════════════════════════════════════════════════════════
// CrashDialog — 崩溃分析弹窗（v2 完整版）
//
// 两种状态：
//   1. analyzing: 游戏启动失败后正在分析日志（转圈 + 提示）
//   2. result:    分析完成，展示原因 / 建议 / 嫌疑模组
//
// 用法（由 MainWindow 驱动）：
//   crashDialogLoader.item.beginAnalyzing()          // 进入分析中状态
//   crashDialogLoader.item.crashData = report        // 分析完成，展示结果
// ═══════════════════════════════════════════════════════════

Popup {
    id: dialog
    modal: true
    anchors.centerIn: Overlay.overlay
    width: 520
    height: Math.min(Math.max(implicitHeight + 40, 320), 640)
    closePolicy: Popup.CloseOnEscape
    padding: 0

    // 分析结果数据 {type, reason, description, suspectedMods, suggestions,
    //              matchedRules, collectedLogs, reportFilePath, exportDir,
    //              reportTooLong, filePath, timestamp, isValid}
    property var crashData: ({})
    // 后端引用（MainWindow 注入），用于重新分析 / 导出日志 / 打开路径
    property var backend: null
    // Toast 引用（MainWindow 注入），导出日志后反馈
    property var toastManager: null
    // 是否处于分析中
    property bool analyzing: false

    // ── 由 MainWindow 调用 ──
    function beginAnalyzing() {
        analyzing = true
        crashData = ({})
        open()
    }

    onCrashDataChanged: {
        if (crashData && crashData.isValid) {
            analyzing = false
            console.log("[crash] dialog result type=", crashData.type,
                        "reason=", crashData.reason,
                        "suggestions=", (crashData.suggestions || []).length,
                        "reportTooLong=", crashData.reportTooLong)
            open()
        }
    }

    // ── 弹出动画 ──
    enter: Transition {
        NumberAnimation { property: "scale"; from: 0.92; to: 1.0; duration: AnimationTokens.pageDuration; easing.type: AnimationTokens.pageEasing }
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: AnimationTokens.itemFadeInDuration; easing.type: AnimationTokens.itemFadeInEasing }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0; duration: AnimationTokens.itemFadeOutDuration; easing.type: AnimationTokens.itemFadeOutEasing }
    }
    scale: 1.0
    opacity: 1.0

    background: Rectangle {
        radius: StyleTokens.radiusXl
        color: StyleTokens.surfaceOverlay
        border.color: StyleTokens.surfaceLight
        border.width: 1
    }

    contentItem: ColumnLayout {
        spacing: 12
        anchors.fill: parent
        anchors.margins: 20

        // ══════ Header ══════
        RowLayout {
            spacing: 10

            Rectangle {
                width: 36; height: 36; radius: 18
                color: analyzing ? StyleTokens.accentSubtle
                     : (crashData.type === "jvm" ? "#382020" : "#382828")
                Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.colorEasing } }

                Text {
                    anchors.centerIn: parent
                    text: analyzing ? "…" : "[警告]"
                    font.pixelSize: StyleTokens.fontSizeXl
                    color: analyzing ? StyleTokens.accentLight : StyleTokens.textDanger
                }
            }

            ColumnLayout { spacing: 2
                Text {
                    text: analyzing ? "正在分析崩溃日志…"
                         : (crashData.type === "jvm" ? "JVM 崩溃" :
                            crashData.type === "minecraft" ? "Minecraft 崩溃" :
                            crashData.type === "log" ? "游戏异常退出" : "崩溃诊断")
                    font.pixelSize: StyleTokens.fontSizeLg
                    font.bold: true
                    color: analyzing ? StyleTokens.textSecondary : "#e8a0a0"
                }
                Text {
                    text: crashData.timestamp
                        ? new Date(crashData.timestamp).toLocaleString(Qt.locale(), "yyyy-MM-dd hh:mm:ss")
                        : ""
                    font.pixelSize: StyleTokens.fontSizeXs
                    color: StyleTokens.textSubtle
                    visible: !!crashData.timestamp
                }
            }

            Item { Layout.fillWidth: true }

            // 关闭按钮
            Rectangle {
                width: 24; height: 24; radius: StyleTokens.radiusMd
                color: closeBtnHov.hovered ? StyleTokens.bgHover : "transparent"
                Text {
                    anchors.centerIn: parent; text: "✕"
                    color: StyleTokens.textTertiary; font.pixelSize: StyleTokens.fontSizeMd
                }
                MouseArea {
                    id: closeBtnHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: dialog.close()
                }
            }
        }

        // ══════ Body ══════
        // ── 分析中状态 ──
        ColumnLayout {
            visible: dialog.analyzing
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 16

            Item { Layout.fillHeight: true; Layout.preferredHeight: 40 }

            LoadingSpinner {
                Layout.alignment: Qt.AlignHCenter
                width: 44; height: 44
                running: dialog.analyzing
                arcDegrees: 120; periodMs: 1000
                arcColor: StyleTokens.accentLight
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "正在收集并分析崩溃日志…"
                font.pixelSize: StyleTokens.fontSizeMd
                color: StyleTokens.textSecondary
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "将扫描崩溃报告、游戏日志与启动器日志，识别崩溃原因与相关模组"
                font.pixelSize: StyleTokens.fontSizeXs
                color: StyleTokens.textSubtle
            }

            Item { Layout.fillHeight: true; Layout.preferredHeight: 40 }
        }

        // ── 结果状态 ──
        ColumnLayout {
            visible: !dialog.analyzing
            spacing: 10
            Layout.fillWidth: true

            // 原因
            Text {
                Layout.fillWidth: true
                font.pixelSize: StyleTokens.fontSizeMd
                font.bold: true
                color: "#e8a0a0"
                text: crashData.reason || "未知原因"
                wrapMode: Text.Wrap
            }
            Text {
                Layout.fillWidth: true
                font.pixelSize: StyleTokens.fontSizeSm
                color: StyleTokens.textTertiary
                text: crashData.description || ""
                wrapMode: Text.Wrap
                visible: !!crashData.description
            }

            // ── 建议列表 ──
            ColumnLayout {
                visible: crashData.suggestions && crashData.suggestions.length > 0
                spacing: 6
                Layout.fillWidth: true

                Text {
                    text: "[提示] 建议处理方式"
                    font.pixelSize: StyleTokens.fontSizeSm
                    font.bold: true
                    color: StyleTokens.warning
                }

                Repeater {
                    model: crashData.suggestions || []
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: sugText.implicitHeight + 12
                        radius: StyleTokens.radiusMd
                        color: "#1a1810"
                        border.color: "#3a3020"

                        Text {
                            id: sugText
                            anchors.fill: parent
                            anchors.margins: 8
                            text: modelData
                            font.pixelSize: StyleTokens.fontSizeSm
                            color: "#d8c090"
                            wrapMode: Text.Wrap
                        }
                    }
                }
            }

            // ── 嫌疑模组 ──
            ColumnLayout {
                visible: crashData.suspectedMods && crashData.suspectedMods.length > 0
                spacing: 6
                Layout.fillWidth: true

                Text {
                    text: "[警告] 疑似相关模组"
                    font.pixelSize: StyleTokens.fontSizeSm
                    font.bold: true
                    color: StyleTokens.textDanger
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 6

                    Repeater {
                        model: crashData.suspectedMods || []
                        delegate: Rectangle {
                            width: modTagText.implicitWidth + 16
                            height: 24
                            radius: StyleTokens.radiusSm
                            color: "#1a1420"
                            border.color: StyleTokens.bgHover

                            Text {
                                id: modTagText
                                anchors.centerIn: parent
                                text: modelData
                                font.pixelSize: StyleTokens.fontSizeXs
                                color: "#c8a0c8"
                            }
                        }
                    }
                }
            }

            // ── 完整报告已写入文件提示 ──
            Rectangle {
                visible: !!crashData.reportFilePath
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                radius: StyleTokens.radiusMd
                color: StyleTokens.accentSubtle
                border.color: StyleTokens.borderFocus

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        text: crashData.reportTooLong
                            ? "分析内容较长，完整报告已保存为独立文件"
                            : "完整分析报告已生成"
                        font.pixelSize: StyleTokens.fontSizeSm
                        color: StyleTokens.accentLink
                        elide: Text.ElideRight
                    }
                    Rectangle {
                        width: 72; height: 24; radius: StyleTokens.radiusSm
                        color: repBtnHov.hovered ? StyleTokens.accentHover : StyleTokens.accent
                        Text {
                            anchors.centerIn: parent; text: "打开报告"
                            font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.textInverse
                        }
                        MouseArea {
                            id: repBtnHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (backend) backend.openPath(crashData.reportFilePath)
                                else Qt.openUrlExternally("file:///" + crashData.reportFilePath)
                            }
                        }
                    }
                }
            }
        }

        // ══════ Footer buttons ══════
        RowLayout {
            spacing: 8
            Layout.fillWidth: true
            Layout.topMargin: 4

            // 重新分析
            Rectangle {
                Layout.preferredWidth: 90; Layout.preferredHeight: 32
                radius: StyleTokens.radiusMd
                color: reHov.hovered ? StyleTokens.bgHover : "transparent"
                border.color: StyleTokens.borderLight
                Text {
                    anchors.centerIn: parent; text: "重新分析"
                    font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary
                }
                MouseArea {
                    id: reHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (backend) {
                            dialog.analyzing = true
                            backend.analyzeCrashNow()
                        }
                    }
                }
            }

            // 导出全部日志
            Rectangle {
                Layout.preferredWidth: 110; Layout.preferredHeight: 32
                radius: StyleTokens.radiusMd
                color: expHov.hovered ? "#202840" : "#151828"
                border.color: StyleTokens.textMuted
                Text {
                    anchors.centerIn: parent; text: "导出全部日志"
                    font.pixelSize: StyleTokens.fontSizeSm; color: "#a0b0d0"
                }
                MouseArea {
                    id: expHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (backend) {
                            var dir = backend.exportCrashLogs("")
                            if (dir && toastManager) toastManager.show("日志已导出到: " + dir, 5000)
                        }
                    }
                }
            }

            Item { Layout.fillWidth: true }

            // 打开崩溃报告
            Rectangle {
                Layout.preferredWidth: 110; Layout.preferredHeight: 32
                radius: StyleTokens.radiusMd
                color: openHov.hovered ? "#202840" : "#151828"
                border.color: StyleTokens.textMuted
                visible: !!crashData.filePath
                Text {
                    anchors.centerIn: parent; text: "打开崩溃报告"
                    font.pixelSize: StyleTokens.fontSizeSm; color: "#a0b0d0"
                }
                MouseArea {
                    id: openHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (crashData.filePath) {
                            if (backend) backend.openPath(crashData.filePath)
                            else Qt.openUrlExternally("file:///" + crashData.filePath)
                        }
                    }
                }
            }

            // 关闭
            Rectangle {
                Layout.preferredWidth: 80; Layout.preferredHeight: 32
                radius: StyleTokens.radiusMd
                color: closeHov.hovered ? "#382020" : "#201818"
                border.color: "#503030"
                Text {
                    anchors.centerIn: parent; text: "关闭"
                    font.pixelSize: StyleTokens.fontSizeSm; color: "#c0a0a0"
                }
                MouseArea {
                    id: closeHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: { dialog.close() }
                }
            }
        }
    }
}
