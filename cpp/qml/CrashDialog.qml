// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

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
//
// 2026-08-04：复用 ShadowButton / StyleTokens，重写图标与颜色，
//             修复 visible 表达式 undefined→bool QML 报错
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
    // 导出对话框引用（MainWindow 注入——FileDialog 须挂在 Window 顶层，
    // 声明在 Popup 内部会导致 Qt6Core.dll 崩溃 0xc0000005）
    property var exportDialogRef: null
    // 是否处于分析中
    property bool analyzing: false
    // 抑制标志：beginAnalyzing 里 crashData=({}) 不应触发结果处理
    // （否则 analyzing 被立即改回 false + 空数据 open()，第二次弹窗时
    //  结果态→空态切换动画冲突导致 Qt6Core.dll 崩溃）
    property bool _suppressResult: false

    // ── 安全取值辅助（crashData 未就绪时不产生 undefined→bool 警告）──
    function _has(key) { return !!(crashData && crashData[key]) }
    function _len(key) { return (crashData && crashData[key]) ? crashData[key].length : 0 }

    // ── 由 MainWindow 调用 ──
    function beginAnalyzing() {
        _suppressResult = true
        analyzing = true
        crashData = ({})
        _suppressResult = false
        if (!opened) open()
    }

    onCrashDataChanged: {
        // beginAnalyzing 的内部置空不处理
        if (_suppressResult) return
        // 无论 isValid 与否都退出分析态（无效结果也显示“未找到崩溃报告”），
        // 否则重新分析/无日志场景会永远卡在加载圈。
        analyzing = false
        // 有内容（含 isValid=false 的“未找到崩溃报告”）才展示；空对象忽略
        if (crashData && Object.keys(crashData).length > 0) {
            console.log("[crash] dialog result type=", crashData.type,
                        "reason=", crashData.reason,
                        "suggestions=", (crashData.suggestions || []).length,
                        "reportTooLong=", crashData.reportTooLong)
            if (!opened) open()
        }
    }

    // 关闭弹窗即销毁启动器生成的分析产物（报告/日志副本；保留用户导出的 zip；
    // 不碰启动器 logs 与游戏侧日志）——用户要求
    onClosed: {
        if (backend) backend.cleanupCrashArtifacts()
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

            // 状态图标：分析中=转圈，结果=警示三角（用通用 LoadingSpinner 组件）
            Item {
                width: 36; height: 36
                LoadingSpinner {
                    anchors.fill: parent
                    running: dialog.analyzing
                    arcDegrees: 120; periodMs: 1000
                    arcColor: StyleTokens.accentLight
                    visible: dialog.analyzing
                }
                Rectangle {
                    anchors.fill: parent
                    radius: 18
                    visible: !dialog.analyzing
                    color: crashData.type === "jvm" ? StyleTokens.errorBg : StyleTokens.surfaceLight
                    Text {
                        anchors.centerIn: parent
                        text: "!"
                        font.pixelSize: StyleTokens.fontSizeXl
                        font.bold: true
                        color: StyleTokens.textDanger
                    }
                }
            }

            ColumnLayout { spacing: 2
                Text {
                    text: analyzing ? "正在分析崩溃日志…"
                         : (crashData.type === "jvm" ? "JVM 崩溃" :
                            crashData.type === "minecraft" ? "Minecraft 崩溃" :
                            crashData.type === "precheck" ? "启动检查失败" :
                            crashData.type === "log" ? "游戏异常退出" : "崩溃诊断")
                    font.pixelSize: StyleTokens.fontSizeLg
                    font.bold: true
                    color: analyzing ? StyleTokens.textSecondary : StyleTokens.textDanger
                }
                Text {
                    // timestamp 现在是 ISO 字符串（C++ 端格式化，避免 QML Date 转换崩溃）
                    text: crashData.timestamp ? String(crashData.timestamp).replace("T", " ").slice(0, 19) : ""
                    font.pixelSize: StyleTokens.fontSizeXs
                    color: StyleTokens.textSubtle
                    visible: _has("timestamp")
                }
            }

            Item { Layout.fillWidth: true }

            // 关闭按钮（复用通用图标按钮组件）
            ShadowIconButton {
                icon: "\u2715"
                type: "close"
                onClicked: dialog.close()
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
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: "正在收集并分析崩溃日志…"
                font.pixelSize: StyleTokens.fontSizeMd
                color: StyleTokens.textSecondary
            }
            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
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
                color: StyleTokens.textDanger
                text: crashData.reason || "未知原因"
                wrapMode: Text.Wrap
            }
            Text {
                Layout.fillWidth: true
                font.pixelSize: StyleTokens.fontSizeSm
                color: StyleTokens.textTertiary
                text: crashData.description || ""
                wrapMode: Text.Wrap
                visible: _has("description")
            }

            // ── 建议列表 ──
            ColumnLayout {
                visible: _len("suggestions") > 0
                spacing: 6
                Layout.fillWidth: true

                Text {
                    text: "建议处理方式"
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
                        color: StyleTokens.bgElevated
                        border.color: StyleTokens.border

                        Text {
                            id: sugText
                            anchors.fill: parent
                            anchors.margins: 8
                            text: modelData
                            font.pixelSize: StyleTokens.fontSizeSm
                            color: StyleTokens.textSecondary
                            wrapMode: Text.Wrap
                        }
                    }
                }
            }

            // ── 嫌疑模组 ──
            ColumnLayout {
                visible: _len("suspectedMods") > 0
                spacing: 6
                Layout.fillWidth: true

                Text {
                    text: "疑似相关模组"
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
                            color: StyleTokens.bgElevated
                            border.color: StyleTokens.border

                            Text {
                                id: modTagText
                                anchors.centerIn: parent
                                text: modelData
                                font.pixelSize: StyleTokens.fontSizeXs
                                color: StyleTokens.accentLink
                            }
                        }
                    }
                }
            }

            // ── JVM 输出（分析依据：游戏进程 stdout+stderr，主流启动器/同主流启动器）──
            ColumnLayout {
                id: jvmCol
                visible: _len("jvmOutput") > 0
                spacing: 6
                Layout.fillWidth: true

                property bool jvmExpanded: false

                // 点击展开/收起
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    Text {
                        text: "JVM 输出（最后 " + (crashData.jvmOutput ? crashData.jvmOutput.length : 0) + " 行）"
                        font.pixelSize: StyleTokens.fontSizeSm
                        font.bold: true
                        color: StyleTokens.textSecondary
                        Layout.fillWidth: true
                    }
                    Text {
                        text: jvmCol.jvmExpanded ? "收起 ▾" : "展开 ▸"
                        font.pixelSize: StyleTokens.fontSizeXs
                        color: StyleTokens.accentLink
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: jvmCol.jvmExpanded = !jvmCol.jvmExpanded
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: jvmCol.jvmExpanded ? Math.min(180, jvmFlick.contentHeight + 16) : 0
                    radius: StyleTokens.radiusMd
                    color: StyleTokens.bgPrimary
                    border.color: StyleTokens.border
                    visible: jvmCol.jvmExpanded
                    clip: true

                    Flickable {
                        id: jvmFlick
                        anchors.fill: parent
                        anchors.margins: 8
                        contentHeight: jvmText.implicitHeight
                        clip: true
                        ScrollBar.vertical: ScrollBar {}

                        Text {
                            id: jvmText
                            width: jvmFlick.width - 4
                            text: crashData.jvmOutput ? crashData.jvmOutput.join("\n") : ""
                            font.pixelSize: StyleTokens.fontSizeXs
                            font.family: "Consolas, monospace"
                            color: StyleTokens.textTertiary
                            wrapMode: Text.WrapAnywhere
                        }
                    }
                }
            }

            // ── 完整报告已写入文件提示 ──
            Rectangle {
                visible: _has("reportFilePath")
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
                    ShadowButton {
                        text: "打开报告"
                        accentColor: StyleTokens.accent
                        textColor: StyleTokens.textInverse
                        btnRadius: StyleTokens.radiusSm
                        Layout.preferredHeight: 24
                        onClicked: {
                            if (backend) backend.openPath(crashData.reportFilePath)
                            else Qt.openUrlExternally("file:///" + crashData.reportFilePath)
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

            ShadowButton {
                text: "重新分析"
                outlined: true
                Layout.preferredWidth: 90; Layout.preferredHeight: 32
                onClicked: {
                    if (backend) {
                        dialog.analyzing = true
                        backend.analyzeCrashNow()
                    }
                }
            }

            ShadowButton {
                text: "导出全部日志"
                outlined: true
                accentColor: StyleTokens.accentLink
                Layout.preferredWidth: 110; Layout.preferredHeight: 32
                onClicked: {
                    if (exportDialogRef) {
                        // 动态填充默认文件名（带时间戳），用户可直接保存
                        var ts = new Date()
                        var pad = function(n){ return n < 10 ? "0" + n : "" + n }
                        var name = "crash-logs-" + ts.getFullYear() + pad(ts.getMonth()+1) + pad(ts.getDate())
                                + "-" + pad(ts.getHours()) + pad(ts.getMinutes()) + pad(ts.getSeconds()) + ".zip"
                        exportDialogRef.currentFile = name
                        exportDialogRef.open()
                    } else if (backend) {
                        // 兜底：无对话框引用时直接导出到默认位置
                        var dir = backend.exportCrashLogs("")
                        if (dir && toastManager) toastManager.show("日志已导出到: " + dir, 5000)
                    }
                }
            }

            Item { Layout.fillWidth: true }

            ShadowButton {
                text: "打开崩溃报告"
                outlined: true
                accentColor: StyleTokens.accentLink
                Layout.preferredWidth: 110; Layout.preferredHeight: 32
                visible: _has("filePath")
                onClicked: {
                    if (crashData.filePath) {
                        if (backend) backend.openPath(crashData.filePath)
                        else Qt.openUrlExternally("file:///" + crashData.filePath)
                    }
                }
            }

            ShadowButton {
                text: "关闭"
                outlined: true
                accentColor: StyleTokens.textDanger
                Layout.preferredWidth: 80; Layout.preferredHeight: 32
                onClicked: { dialog.close() }
            }
        }
    }
}
