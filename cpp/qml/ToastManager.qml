// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic

// ToastManager — 独立可复用的通知组件
// 用法: toastManager.show("消息内容", 持续毫秒)
//       toastManager.show("消息内容")  // 默认 3000ms
// 特性: 右下角弹出、天蓝色主题、弹性滑入、淡出消失、多条自动堆叠
Item {
    id: root

    // ═══ 公开 API ═══
    property int defaultDuration: 3000
    property int toastIdCounter: 0

    function show(message, duration) {
        if (!message || message === "") return
        if (!duration || duration <= 0) duration = root.defaultDuration
        var tid = toastIdCounter++
        // 插入到开头 → 新 toast 出现在最上方
        toastModel.insert(0, {
            "msg": message,
            "duration": duration,
            "toastId": tid
        })
    }

    ListModel {
        id: toastModel
    }

    // ═══ Toast 容器 — 右下角堆叠 ═══
    Column {
        id: toastColumn
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12
        spacing: 6

        Repeater {
            model: toastModel

            delegate: Item {
                id: delegateItem
                // 宽度绑定到 toastRect 的最终宽度
                width: toastRect.width
                height: 32
                clip: false

                Rectangle {
                    id: toastRect
                    height: 32
                    width: Math.min(toastLabel.implicitWidth + 24, 380)
                    radius: StyleTokens.radiusSm
                    color: StyleTokens.infoBg
                    // 起始位置: 在 delegate 右侧外部（用于弹性滑入动画）
                    x: toastRect.width + 80

                    // ── 左侧蓝色细条 ──
                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: 3
                        color: StyleTokens.info
                        radius: StyleTokens.radiusXs
                    }

                    // ── 消息文字 ──
                    Text {
                        id: toastLabel
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        text: model.msg || ""
                        color: "#c8d8f0"
                        font.pixelSize: StyleTokens.fontSizeSm
                        elide: Text.ElideRight
                        maximumLineCount: 1
                    }

                    // ── 弹性滑入动画 ──
                    NumberAnimation {
                        id: slideIn
                        target: toastRect
                        property: "x"
                        to: 0
                        duration: 420
                        easing.type: Easing.OutBack
                    }

                    // ── 淡出动画 ──
                    NumberAnimation {
                        id: fadeOut
                        target: toastRect
                        property: "opacity"
                        to: 0
                        duration: 280
                        easing.type: Easing.OutCubic
                    }

                    // ── 消失后从 model 中移除 ──
                    function removeSelf() {
                        for (var i = 0; i < toastModel.count; i++) {
                            if (toastModel.get(i).toastId === model.toastId) {
                                toastModel.remove(i)
                                break
                            }
                        }
                    }

                    // ── 自动消失计时器 ──
                    Timer {
                        id: dismissTimer
                        interval: model.duration || root.defaultDuration
                        repeat: false
                        onTriggered: {
                            fadeOut.start()
                        }
                    }

                    // 动画结束后清理
                    Connections {
                        target: fadeOut
                        function onStopped() {
                            toastRect.removeSelf()
                        }
                    }

                    // 组件创建时启动滑入 + 计时器
                    Component.onCompleted: {
                        slideIn.start()
                        dismissTimer.start()
                    }
                }
            }
        }
    }
}
