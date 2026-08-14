// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic

// ToastManager — 独立可复用的通知组件
// 用法: toastManager.show("消息内容", 持续毫秒)
//       toastManager.show("消息内容")  // 默认 3000ms
//       toastManager.showAction("消息", onAction, "按钮文字")  // 2026-08-15：常驻可点击
// 特性: 右下角弹出、天蓝色主题、弹性滑入、淡出消失、多条自动堆叠
//       showAction：duration=0 常驻（不自动消失），整条可点击触发 onAction，
//       右上角 ✕ 手动关闭；点击后自动移除
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
    // 用于"更新已就绪，点击立即重启安装"等需要用户决策的场景。
    function showAction(message, onAction, actionText) {
        if (!message || message === "") return
        var tid = toastIdCounter++
        toastModel.insert(0, {
            "msg": message,
            "duration": 0,
            "toastId": tid,
            "isAction": true,
            "actionText": actionText || "立即处理"
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
                // 宽度绑定到 toastRect 的最终宽度
                width: toastRect.width
                height: 34
                clip: false

                Rectangle {
                    id: toastRect
                    height: 34
                    width: Math.min(toastLabel.implicitWidth + (model.isAction ? 132 : 24), model.isAction ? 480 : 380)
                    radius: StyleTokens.radiusSm
                    // hover 高亮（仅可点击条）
                    color: model.isAction && actionMouse.containsMouse ? "#1d2a3a"
                         : StyleTokens.infoBg
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

                    // ── 消息文字（action toast 右侧预留按钮区）──
                    Text {
                        id: toastLabel
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        anchors.right: parent.right
                        anchors.rightMargin: model.isAction ? 118 : 8
                        anchors.verticalCenter: parent.verticalCenter
                        text: model.msg || ""
                        color: StyleTokens.textSecondary
                        font.pixelSize: StyleTokens.fontSizeSm
                        elide: Text.ElideRight
                        maximumLineCount: 1
                    }

                    // ── 操作按钮文字（showAction）──
                    Text {
                        id: actionLabel
                        anchors.right: parent.right
                        anchors.rightMargin: 30
                        anchors.verticalCenter: parent.verticalCenter
                        visible: model.isAction
                        text: model.actionText || ""
                        color: StyleTokens.info
                        font.pixelSize: StyleTokens.fontSizeSm
                        font.weight: Font.DemiBold
                    }

                    // ── 关闭按钮（showAction 常驻条）──
                    Text {
                        id: closeMark
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        visible: model.isAction
                        text: "✕"
                        color: closeHover.containsMouse ? StyleTokens.textPrimary : StyleTokens.textTertiary
                        font.pixelSize: StyleTokens.fontSizeXs
                        MouseArea {
                            id: closeHover
                            anchors.fill: parent; hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: { toastRect.removeSelf() }
                        }
                    }

                    // ── 整条点击（showAction）──
                    MouseArea {
                        id: actionMouse
                        anchors.fill: parent
                        enabled: model.isAction
                        hoverEnabled: model.isAction
                        cursorShape: model.isAction ? Qt.PointingHandCursor : Qt.ArrowCursor
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
