// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Layouts

// DownloadQueueCard — 下载队列面板中的单张卡片
// 紧凑布局：名称行 + 进度条 + 信息行
// 取消按钮始终可见，使用 Lucide x.svg 图标

Rectangle {
    id: root

    implicitWidth: parent ? parent.width : 300
    implicitHeight: (stepsList.visible ? stepsList.y + stepsList.height : infoRow.y + infoRow.height) + 12
    radius: StyleTokens.radiusLg
    color: "#141a24"

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
            color: model.failed ? StyleTokens.errorLight
                 : model.progress >= 1.0 ? "#3fb950"
                 : StyleTokens.accent
            width: parent.width * Math.min(model.progress || 0, 1.0)
        }
    }

    // ── 第一行：名称 + 取消按钮 ──
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
            text: model.name || ""
            font.pixelSize: StyleTokens.fontSizeCaption
            color: StyleTokens.textPrimary
            elide: Text.ElideRight
            width: parent.width - 30  // leave space for cancel button
        }

        // ── 取消按钮 (Lucide x.svg) ──
        Rectangle {
            id: cancelBtn
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: 20; height: 20; radius: 10
            visible: model.canCancel !== false
            color: cancelMouse.containsMouse ? "#4a1a1a" : "transparent"

            Behavior on color { ColorAnimation { duration: 120 } }

            Image {
                anchors.centerIn: parent
                source: "icons/lucide/x.svg"
                width: 12; height: 12
                sourceSize.width: 12; sourceSize.height: 12
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
        anchors.top: progressTrack.bottom; anchors.topMargin: 6
        anchors.left: parent.left; anchors.leftMargin: 12
        anchors.right: parent.right; anchors.rightMargin: 12
        spacing: 4

        Text {
            text: {
                if (model.failed) return ""
                if (model.progress >= 1.0) return " "  // placeholder, icon replaces
                var speed = model.speed || 0
                if (speed <= 0) return ""
                return fmtSpeed(speed)
            }
            font.pixelSize: StyleTokens.fontSizeXs
            color: model.failed ? StyleTokens.errorLight
                 : model.progress >= 1.0 ? "#3fb950"
                 : StyleTokens.textMuted
            visible: text !== ""
        }

        // ── 完成图标 (Lucide check-circle.svg) ──
        Image {
            visible: !model.failed && model.progress >= 1.0
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
        // Only show sub-steps during active download, not when done/failed
        visible: model.progress < 1.0 && !model.failed
        // Capture steps BEFORE the Repeater shadows the 'model' identifier
        property var __steps: model && model.steps ? model.steps : []

        Repeater {
            model: stepsList.__steps
            delegate: RowLayout {
                width: parent.width
                spacing: 4
                visible: modelData && modelData.show !== false
                height: 16

                // ── 状态指示器 (圆点) ──
                Rectangle {
                    width: 6; height: 6; radius: 3
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
                    font.pixelSize: StyleTokens.fontSizeXs
                    color: StyleTokens.textSecondary
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                // ── 活跃步骤的百分比 (仅 active) ──
                Text {
                    text: modelData.status === "active" ? (modelData.percentage + "%") : ""
                    font.pixelSize: StyleTokens.fontSizeXs
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
