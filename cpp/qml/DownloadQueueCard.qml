// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// DownloadQueueCard — 下载队列面板中的单张卡片
// 紧凑布局：名称行 + 进度条 + 信息行
// 采用 Poll 模式：Timer 每 200ms 从 C++ InstallCardModel::cardData() 获取数据
//
// 数据分离策略：
//   _hot  (progress, speed) — 每 200ms 变化，仅触发进度条/速度文本重评
//   _meta (name, steps, failed, error, phase, canCancel) — 仅状态变化时更新
//   步骤未变时 Repeater 不重建（通过 JSON 比较检测）

Rectangle {
    id: root

    implicitWidth: parent ? parent.width : 300
    implicitHeight: (stepsList.visible ? stepsList.y + stepsList.height : infoRow.y + infoRow.height) + 12
    radius: StyleTokens.radiusLg
    color: "#141a24"

    // ── Poll 变量 ──
    property var _hot: ({})     // 高频：progress, speed（每 200ms 变化）
    property var _meta: ({})    // 低频：name, steps, failed, error, phase, canCancel
    property bool _dismissed: false
    property string _stepsJson: ''  // 步骤缓存，用于检测变化
    // 从 panel 传入的模型引用 (防止 scope 问题)
    property var cardModel: null
    
    // ── 呼吸点动画（独立于 Repeater 生命周期，不受 poll 重建影响）──
    property real breathePhase: 0
    NumberAnimation on breathePhase {
        from: 0; to: 2 * Math.PI
        duration: 1200
        loops: Animation.Infinite
    }

    // 获取 C++ 模型对象的辅助函数
    // 优先级: cardModel > model.model > ListView.view.model
    function _getModel() {
        if (root.cardModel && typeof root.cardModel.cardData === 'function')
            return root.cardModel
        if (model && model.model && typeof model.model.cardData === 'function')
            return model.model
        if (ListView.view && ListView.view.model && typeof ListView.view.model.cardData === 'function')
            return ListView.view.model
        return null
    }

    // 初始加载
    Component.onCompleted: {
        var mdl = _getModel()
        if (mdl) {
            var d = mdl.cardData(index)
            if (d && d.iid) {
                _hot = { progress: d.progress, speed: d.speed }
                _meta = d
                if (d.steps) _stepsJson = JSON.stringify(d.steps)
            }
        }
    }

    // ── Poll 定时器 ──
    Timer {
        id: pollTimer
        interval: 200
        running: !_dismissed
        repeat: true
        onTriggered: {
            var mdl = root._getModel()
            if (mdl) {
                var nd = mdl.cardData(index)
                // cardData 返回空 map 时跳过 (row 无效)
                if (nd && nd.iid) {
                    // 临时：始终更新 _meta，测试步骤能否正常显示
                    _hot = { progress: nd.progress, speed: nd.speed }
                    _meta = nd
                    if (nd.steps) _stepsJson = JSON.stringify(nd.steps)
                }
            }
        }
    }

    // ── 自动消失计时器（完成后 3s 自动关闭卡片） ──
    Timer {
        id: dismissTimer
        interval: 3000
        repeat: false
        running: !_dismissed && (_meta.failed || _hot.progress >= 1.0)
        onTriggered: {
            _dismissed = true
            if (backend && _meta.iid)
                backend.dismissCard(_meta.iid)
        }
    }

    // ── 进度条 (4px) ──
    Rectangle {
        id: progressTrack
        anchors.top: parent.top; anchors.topMargin: 32
        anchors.left: parent.left; anchors.leftMargin: 12
        anchors.right: parent.right; anchors.rightMargin: 12
        height: 4; radius: 2
        color: "#1e2a3a"

        Rectangle {
            height: parent.height; radius: 2
            color: _meta.failed ? StyleTokens.errorLight
                 : _hot.progress >= 1.0 ? "#3fb950"
                 : StyleTokens.accent
            width: parent.width * Math.min(_hot.progress || 0, 1.0)
            Behavior on width {
                SmoothedAnimation { velocity: 0.5; duration: 300 }
            }
        }
    }

    // ── 第一行：名称 + 取消/关闭按钮 ──
    Item {
        id: nameRow
        anchors.top: parent.top; anchors.topMargin: 10
        anchors.left: parent.left; anchors.leftMargin: 12
        anchors.right: parent.right; anchors.rightMargin: 12
        height: 18

        Text {
            id: nameText
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            text: _meta.failed ? (_meta.name || "") + " — " + (_meta.error || "失败")
                  : (_meta.name || "")
            font.pixelSize: StyleTokens.fontSizeCaption
            color: _meta.failed ? StyleTokens.errorLight
                 : _hot.progress >= 1.0 ? "#3fb950"
                 : StyleTokens.textPrimary
            elide: Text.ElideRight
            width: parent.width - 30
        }

        // ── 操作按钮 ──
        Rectangle {
            id: actionBtn
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: 20; height: 20; radius: 10
            visible: _meta.canCancel !== false || _meta.failed || _hot.progress >= 1.0
            color: actionMouse.containsMouse ? "#4a1a1a" : "transparent"

            Behavior on color { ColorAnimation { duration: 120 } }

            Image {
                anchors.centerIn: parent
                source: "icons/lucide/x.svg"
                width: 12; height: 12
                sourceSize.width: 12; sourceSize.height: 12
            }

            MouseArea {
                id: actionMouse
                anchors.fill: parent; hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (_meta.failed || _hot.progress >= 1.0) {
                        if (backend && _meta.iid)
                            backend.dismissCard(_meta.iid)
                    } else {
                        if (backend && _meta.iid)
                            backend.cancelVersionInstall(_meta.iid)
                    }
                }
            }
        }
    }

    // ── 第三行：速度/完成标记 ──
    RowLayout {
        id: infoRow
        anchors.top: progressTrack.bottom; anchors.topMargin: 6
        anchors.left: parent.left; anchors.leftMargin: 12
        anchors.right: parent.right; anchors.rightMargin: 12
        spacing: 4

        Text {
            text: {
                if (_meta.failed) return _meta.error || "失败"
                if (_hot.progress >= 1.0) return "完成 ✓"
                return fmtSpeed(_hot.speed || 0)
            }
            font.pixelSize: StyleTokens.fontSizeXs
            color: _meta.failed ? StyleTokens.errorLight
                 : _hot.progress >= 1.0 ? "#3fb950"
                 : StyleTokens.textMuted
            visible: true
        }

        Item { Layout.fillWidth: true }

        Image {
            visible: !_meta.failed && _hot.progress >= 1.0
            source: "icons/lucide/check-circle.svg"
            width: 14; height: 14
            sourceSize.width: 14; sourceSize.height: 14
        }
    }

    // ── 子步骤列表 ──
    Column {
        id: stepsList
        anchors.top: infoRow.bottom; anchors.topMargin: 6
        anchors.left: parent.left; anchors.leftMargin: 16
        anchors.right: parent.right; anchors.rightMargin: 12
        spacing: 3
        visible: _hot.progress < 1.0 && !_meta.failed
        property var __steps: _meta.steps || []

        Repeater {
            model: stepsList.__steps
            delegate: RowLayout {
                width: parent.width
                spacing: 4
                visible: modelData && modelData.show !== false
                height: 16

                property real stepRawPct: modelData.percentage || 0
                property real stepSmoothPct: stepRawPct

                onStepRawPctChanged: stepSmoothPct = stepRawPct

                Behavior on stepSmoothPct {
                    SmoothedAnimation {
                        duration: 400
                        velocity: 5
                    }
                }

                Rectangle {
                    width: 6; height: 6; radius: 3
                    Layout.alignment: Qt.AlignVCenter
                    color: {
                        var s = modelData.status || "pending"
                        if (s === "completed") return "#3fb950"
                        if (s === "active") return StyleTokens.accent
                        if (s === "failed") return StyleTokens.errorLight
                        return "#2a3a4a"
                    }
                }

                Text {
                    text: modelData.name || ""
                    font.pixelSize: StyleTokens.fontSizeXs
                    color: StyleTokens.textSecondary
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                // Percentage text (only shown when there's actual progress)
                Text {
                    text: stepSmoothPct > 0 ? (Math.round(stepSmoothPct) + "%") : ""
                    font.pixelSize: StyleTokens.fontSizeXs
                    color: StyleTokens.textMuted
                    visible: modelData.status === "active" && stepSmoothPct > 0
                }
                // Breathing dots animation for loader install steps (active at 0%)
                Row {
                    visible: modelData.status === "active" && stepSmoothPct === 0
                    spacing: 3
                    Repeater {
                        model: 3
                        Rectangle {
                            width: 5; height: 5; radius: 2.5
                            color: "#788090"
                            // Continuous sine wave driven by root-level animation
                            // (independent of delegate lifecycle — survives poll recreations)
                            opacity: 0.3 + 0.7 * Math.abs(Math.sin(root.breathePhase + index * 2.094))
                        }
                    }
                }
            }
        }
    }

    // ── 辅助函数 ──
    function fmtSize(bytes) {
        if (!bytes || bytes < 0) return "0 B"
        var units = ["B", "KB", "MB", "GB"]
        var u = 0
        var v = bytes
        while (v >= 1024 && u < units.length - 1) { v /= 1024; u++ }
        return v.toFixed(u === 0 ? 0 : 1) + " " + units[u]
    }
    function fmtSpeed(speedBytes) {
        return fmtSize(speedBytes) + "/s"
    }
}
