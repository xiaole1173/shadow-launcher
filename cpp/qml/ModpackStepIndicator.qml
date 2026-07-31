// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick

// ModpackStepIndicator — 整合包导入分步指示器（5 阶段横向）
// 状态：pending（灰）/ active（主题色高亮 + 加载动画）/ done（对勾）/ failed（红叉）
// 顶部轨道线随完成进度填充；全部完成时全绿，失败时轨道线变红。
//
// 用法：
//   ModpackStepIndicator {
//       steps: ["解析校验", "资源解压", "模组下载", "安装游戏", "收尾注册"]
//       activeStep: 2        // 当前阶段（1..n；0 = 无）
//       doneCount: 1         // 已完成阶段数（0..n）
//       failedStep: 0        // 失败阶段（0 = 无）
//       running: true        // 进行中 → 当前阶段显示加载动画
//   }
Item {
    id: root

    implicitWidth: 480
    implicitHeight: 58

    property var steps: []
    property int activeStep: 0
    property int doneCount: 0
    property int failedStep: 0
    property bool running: true
    // 总进度（0..1）：>=0 时轨道填充与后端总进度同步（按阶段边界归一，见 _fill）
    property real progress: -1

    readonly property int _n: steps.length

    // 轨道填充比例：
    //  - 失败：填充到失败阶段前（红色）
    //  - 传入总进度：按阶段边界（解析5%/解压10%/下载55%/安装25%/收尾5%）分阶段归一
    //  - 兜底：按 doneCount 分段填充
    readonly property real _fill: {
        if (root.failedStep > 0)
            return root._n > 1 ? Math.min(1.0, root.doneCount / (root._n - 1)) : 1.0
        if (root.progress >= 0.0 && root.progress <= 1.0) {
            var bounds = [0.0, 0.05, 0.15, 0.70, 0.95, 1.0]
            var p = root.progress
            var stage = 1
            for (var i = 1; i < bounds.length; ++i) {
                if (p <= bounds[i]) { stage = i; break }
            }
            var s = bounds[stage - 1]
            var e = bounds[stage]
            var frac = (e > s) ? Math.max(0.0, Math.min(1.0, (p - s) / (e - s))) : 1.0
            return root._n > 1 ? Math.min(1.0, ((stage - 1) + frac) / (root._n - 1)) : 1.0
        }
        return root._n > 1 ? Math.min(1.0, root.doneCount / (root._n - 1)) : 1.0
    }

    // ── 轨道线（圆环中心高度，位于圆下方经背景贯穿）──
    Rectangle {
        id: track
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: 15
        height: 2
        radius: 1
        color: StyleTokens.bgElevated

        Rectangle {
            id: trackFill
            height: 2
            radius: 1
            width: track.width * root._fill
            color: root.failedStep > 0 ? StyleTokens.errorLight : StyleTokens.success
            Behavior on width { NumberAnimation { duration: 350; easing.type: Easing.OutCubic } }
            Behavior on color { ColorAnimation { duration: 200; easing.type: Easing.OutCubic } }
        }
    }

    // ── 步骤节点 ──
    Row {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top

        Repeater {
            model: root._n

            delegate: Column {
                width: root.width / root._n
                spacing: 6

                // ── 圆环 ──
                Item {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 30
                    height: 30

                    Rectangle {
                        id: circle
                        anchors.fill: parent
                        radius: 15
                        color: _bg
                        border.color: _border
                        border.width: 1
                        scale: _isActive ? 1.0 : 0.92

                        readonly property bool _isDone: index < root.doneCount && root.failedStep !== index + 1
                        readonly property bool _isActive: root.activeStep === index + 1
                        readonly property bool _isFailed: root.failedStep === index + 1
                        readonly property color _bg: _isFailed ? StyleTokens.errorBg :
                                                     _isActive ? StyleTokens.accentSubtle :
                                                     _isDone ? StyleTokens.successBg : StyleTokens.bgElevated
                        readonly property color _border: _isFailed ? StyleTokens.errorLight :
                                                         _isActive ? StyleTokens.accent :
                                                         _isDone ? StyleTokens.success : StyleTokens.borderLight

                        Behavior on color { ColorAnimation { duration: 200; easing.type: Easing.OutCubic } }
                        Behavior on border.color { ColorAnimation { duration: 200; easing.type: Easing.OutCubic } }
                        Behavior on scale { NumberAnimation { duration: 250; easing.type: Easing.OutBack } }

                        // 序号（pending）
                        Text {
                            anchors.centerIn: parent
                            text: index + 1
                            color: StyleTokens.textMuted
                            font.pixelSize: StyleTokens.fontSizeSm
                            visible: !circle._isDone && !circle._isActive && !circle._isFailed
                        }
                        // 对勾（done）
                        Image {
                            anchors.centerIn: parent
                            source: "icons/lucide/check.svg"
                            width: 14
                            height: 14
                            visible: circle._isDone
                        }
                        // 红叉（failed）
                        Image {
                            anchors.centerIn: parent
                            source: "icons/lucide/x.svg"
                            width: 12
                            height: 12
                            visible: circle._isFailed
                        }
                        // 加载动画（active）
                        LoadingSpinner {
                            anchors.centerIn: parent
                            width: 16
                            height: 16
                            running: circle._isActive && root.running
                            arcColor: StyleTokens.accentLight
                        }
                    }
                }

                // ── 标签 ──
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.steps[index] || ""
                    font.pixelSize: StyleTokens.fontSizeXs
                    color: circle._isFailed ? StyleTokens.errorLight :
                           circle._isActive ? StyleTokens.textSecondary :
                           circle._isDone ? StyleTokens.success : StyleTokens.textMuted
                    Behavior on color { ColorAnimation { duration: 200; easing.type: Easing.OutCubic } }
                }
            }
        }
    }
}
