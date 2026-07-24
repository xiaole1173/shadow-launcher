// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Layouts

// DownloadQueueCard — 下载队列面板中的单张卡片
// 紧凑布局：名称行 + 进度条 + 信息行
// 取消按钮始终可见，使用 Lucide x.svg 图标

Rectangle {
    id: root

    property bool compact: true  // 紧凑模式（侧边栏），false 为大卡片（全屏页）

    // 字号映射
    readonly property real _titleSize: compact ? StyleTokens.fontSizeCaption : StyleTokens.fontSizeMd
    readonly property real _bodySize: compact ? StyleTokens.fontSizeXs : StyleTokens.fontSizeSm
    readonly property real _stepSize: compact ? StyleTokens.fontSizeXs : StyleTokens.fontSizeXs
    // 间距映射
    readonly property real _padT: compact ? 10 : 16
    readonly property real _padH: compact ? 12 : 20
    readonly property real _progressHeight: compact ? 4 : 6
    readonly property real _progressTop: compact ? 32 : 40
    readonly property real _spacingMd: compact ? 6 : 10
    readonly property real _spacingSm: compact ? 4 : 6
    readonly property real _spacingStep: compact ? 3 : 5
    readonly property real _stepHeight: compact ? 16 : 20
    readonly property real _cancelBtnSize: compact ? 20 : 28
    readonly property real _cancelIconSize: compact ? 12 : 16
    readonly property real _stepDotSize: compact ? 6 : 8
    readonly property real _doneIconSize: compact ? 14 : 18

    implicitWidth: parent ? parent.width : 300
    implicitHeight: (stepsList.visible ? stepsList.y + stepsList.height : infoRow.y + infoRow.height) + _padT
    radius: StyleTokens.radiusLg
    color: "#141a24"

    // ── 进度条 (4px) ──
    Rectangle {
        id: progressTrack
        anchors.top: parent.top; anchors.topMargin: _progressTop
        anchors.left: parent.left; anchors.leftMargin: _padH
        anchors.right: parent.right; anchors.rightMargin: _padH
        height: _progressHeight; radius: 2
        color: "#1e2a3a"

        Rectangle {
            height: parent.height; radius: 2
            color: model.failed ? StyleTokens.errorLight
                 : model.progress >= 1.0 ? "#3fb950"
                 : StyleTokens.accent
            width: parent.width * Math.min(model.progress || 0, 1.0)
            Behavior on width {
                SmoothedAnimation { velocity: 0.5; duration: 300 }
            }
        }
    }

    // ── 第一行：名称 + 取消按钮 ──
    Item {
        id: nameRow
        anchors.top: parent.top; anchors.topMargin: _padT
        anchors.left: parent.left; anchors.leftMargin: _padH
        anchors.right: parent.right; anchors.rightMargin: _padH
        height: _cancelBtnSize > 20 ? _cancelBtnSize : 18  // 按取消按钮大小自适应行高

        Text {
            id: nameText
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            text: model.name || ""
            font.pixelSize: _titleSize
            color: StyleTokens.textPrimary
            elide: Text.ElideRight
            width: parent.width - _cancelBtnSize - 8
        }

        // ── 取消按钮 (Lucide x.svg) ──
        Rectangle {
            id: cancelBtn
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: _cancelBtnSize; height: _cancelBtnSize; radius: _cancelBtnSize / 2
            visible: model.canCancel !== false
            color: cancelMouse.containsMouse ? "#4a1a1a" : "transparent"

            Behavior on color { ColorAnimation { duration: 120 } }

            Image {
                anchors.centerIn: parent
                source: "icons/lucide/x.svg"
                width: _cancelIconSize; height: _cancelIconSize
                sourceSize.width: _cancelIconSize; sourceSize.height: _cancelIconSize
            }

            MouseArea {
                id: cancelMouse
                anchors.fill: parent; hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (backend && model.iid)
                        backend.cancelVersionInstall(model.iid)
                }
            }
        }
    }

    // ── 第三行：速度/完成标记 ──
    RowLayout {
        id: infoRow
        anchors.top: progressTrack.bottom; anchors.topMargin: _spacingMd
        anchors.left: parent.left; anchors.leftMargin: _padH
        anchors.right: parent.right; anchors.rightMargin: _padH
        spacing: _spacingSm

        Text {
            text: {
                if (model.failed) return ""
                if (model.progress >= 1.0) return " "  // placeholder, icon replaces
                return fmtSpeed(model.speed || 0)
            }
            font.pixelSize: _bodySize
            color: model.failed ? StyleTokens.errorLight
                 : model.progress >= 1.0 ? "#3fb950"
                 : StyleTokens.textMuted
            // 始终可见 — 防止子步骤因速度消失而抖动
            visible: true
        }

        // ── 弹簧：将完成图标推到最右侧 ──
        Item { Layout.fillWidth: true }

        // ── 完成图标 (Lucide check-circle.svg) ──
        Image {
            visible: !model.failed && model.progress >= 1.0
            source: "icons/lucide/check-circle.svg"
            width: _doneIconSize; height: _doneIconSize
            sourceSize.width: _doneIconSize; sourceSize.height: _doneIconSize
        }
    }

    // ── 子步骤列表 ──
    Column {
        id: stepsList
        anchors.top: infoRow.bottom; anchors.topMargin: _spacingMd
        anchors.left: parent.left; anchors.leftMargin: _padH + 4
        anchors.right: parent.right; anchors.rightMargin: _padH
        spacing: _spacingStep
        // Only show sub-steps during active download, not when done/failed
        visible: model.progress < 1.0 && !model.failed
        // Capture steps BEFORE the Repeater shadows the 'model' identifier
        property var __steps: model && model.steps ? model.steps : []

        Repeater {
            model: stepsList.__steps
            delegate: RowLayout {
                width: parent.width
                spacing: _spacingSm
                visible: modelData && modelData.show !== false
                height: _stepHeight

                // ── 状态指示器 (圆点) ──
                Rectangle {
                    width: _stepDotSize; height: _stepDotSize; radius: _stepDotSize / 2
                    anchors.verticalCenter: parent.verticalCenter
                    color: {
                        var s = modelData.status || "pending"
                        if (s === "completed") return "#3fb950"
                        if (s === "active") return StyleTokens.accent
                        if (s === "failed") return StyleTokens.errorLight
                        return "#2a3a4a"
                    }
                }

                // ── 步骤名 ──
                Text {
                    text: modelData.name || ""
                    font.pixelSize: _stepSize
                    color: StyleTokens.textSecondary
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                // ── 活跃步骤的百分比 (仅 active) ──
                Text {
                    text: modelData.status === "active" ? (modelData.percentage + "%") : ""
                    font.pixelSize: _stepSize
                    color: StyleTokens.textMuted
                    visible: modelData.status === "active"
                }
            }
        }
    }

    // ── 辅助函数 (从 DownloadProgressPage 移植) ──
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
