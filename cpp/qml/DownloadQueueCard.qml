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
    implicitHeight: infoRow.y + infoRow.height + 12
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
            Behavior on width {
                SmoothedAnimation { velocity: 0.5; duration: 300 }
            }
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

    // ── 第三行：状态文本 + 速度/完成标记 ──
    RowLayout {
        id: infoRow
        anchors.top: progressTrack.bottom; anchors.topMargin: 6
        anchors.left: parent.left; anchors.leftMargin: 12
        anchors.right: parent.right; anchors.rightMargin: 12
        spacing: 4

        Text {
            id: phaseText
            text: {
                if (model.failed) return "失败"
                if (model.progress >= 1.0) return "完成"
                return model.phase || ""
            }
            font.pixelSize: StyleTokens.fontSizeXs
            color: model.failed ? StyleTokens.errorLight
                 : model.progress >= 1.0 ? "#3fb950"
                 : StyleTokens.textMuted
            elide: Text.ElideRight
            Layout.fillWidth: true
        }

        Text {
            text: {
                if (model.failed) return ""
                if (model.progress >= 1.0) return "✓"
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
