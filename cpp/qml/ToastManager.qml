// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic

// ToastManager — 独立可复用的通知组件
// 用法: toastManager.show("消息内容", 持续毫秒)
//       toastManager.show("消息内容")  // 默认 3000ms
//       toastManager.showAction("消息", onAction)  // 2026-08-15：常驻可点击
// 特性: 右下角弹出、天蓝色主题、弹性滑入、淡出消失、多条自动堆叠
//       showAction：duration=0 常驻（不自动消失），【整个 toast 作为点击区域】
//       点击任意位置触发 onAction 后自动移除
//
// 2026-08-15 简化（用户实测按钮组件交互异常，弃用修复改为整条可点）：
//   - 删除"立即重启"按钮与 ✕ 关闭按钮（此前多级 MouseArea 分层仍异常）
//   - 整条 toast 即点击区（MouseArea 挂 toastRect 内，动画跟随），hover 整条
//     轻微高亮作为可点击反馈
Item {
    id: root

    // ═══ 公开 API ═══
    property int defaultDuration: 3000
    property int toastIdCounter: 0
    // 2026-08-15：showAction 的动作回调表（toastId -> function）
    property var _actionHandlers: ({})

    function show(message, duration) {
        if (!message || message === "") return
        if (!duration || duration <= 0) duration = root.defaultDuration
        var tid = toastIdCounter++
        // 插入到开头 → 新 toast 出现在最上方
        toastModel.insert(0, {
            "msg": message,
            "duration": duration,
            "toastId": tid,
            "isAction": false
        })
    }

    // ── 2026-08-15：常驻可点击 toast（duration=0 不自动消失）──
    // 整个 toast 为点击区域，点击任意位置触发 onAction。
    function showAction(message, onAction) {
        if (!message || message === "") return
        var tid = toastIdCounter++
        toastModel.insert(0, {
            "msg": message,
            "duration": 0,
            "toastId": tid,
            "isAction": true
        })
        if (onAction) root._actionHandlers[tid] = onAction
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
                width: toastRect.width
                height: 34
                clip: false

                Rectangle {
                    id: toastRect
                    height: 34
                    width: Math.min(toastLabel.implicitWidth + 24, 420)
                    radius: StyleTokens.radiusSm
                    // 整条可点：hover 轻微高亮作为反馈（2026-08-15 简化）
                    color: model.isAction && actionMouse.containsMouse ? "#1d2a3a" : StyleTokens.infoBg
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
                        color: StyleTokens.textSecondary
                        font.pixelSize: StyleTokens.fontSizeSm
                        elide: Text.ElideRight
                        maximumLineCount: 1
                    }

                    // ── 整个 toast 即点击区（2026-08-15 简化；挂 toastRect 内动画跟随）──
                    MouseArea {
                        id: actionMouse
                        anchors.fill: parent
                        visible: model.isAction
                        hoverEnabled: true   // ⚠ 必须！containsMouse 依赖它，否则 hover 高亮恒不触发
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            var h = root._actionHandlers[model.toastId]
                            if (h) {
                                delete root._actionHandlers[model.toastId]
                                h()
                            }
                            toastRect.removeSelf()
                        }
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
                        delete root._actionHandlers[model.toastId]
                        for (var i = 0; i < toastModel.count; i++) {
                            if (toastModel.get(i).toastId === model.toastId) {
                                toastModel.remove(i)
                                break
                            }
                        }
                    }

                    // ── 自动消失计时器（duration=0 常驻，不启动）──
                    Timer {
                        id: dismissTimer
                        interval: model.duration > 0 ? model.duration : root.defaultDuration
                        repeat: false
                        running: model.duration > 0
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
                        if (model.duration > 0) dismissTimer.start()
                    }
                }
            }
        }
    }
}
