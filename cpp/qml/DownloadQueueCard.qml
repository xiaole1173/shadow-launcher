// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// DownloadQueueCard — 下载队列面板中的单张卡片
// 紧凑布局：名称行 + 进度条 + 信息行
// 取消按钮在可取消时显示 x.svg，完成后自动 3s 消失

Rectangle {
    id: root

    implicitWidth: parent ? parent.width : 300
    implicitHeight: (stepsList.visible ? stepsList.y + stepsList.height : infoRow.y + infoRow.height) + 12
    radius: StyleTokens.radiusLg
    color: "#141a24"

    // ── 自动消失计时器（完成后 3s 自动关闭卡片） ──
    // 只触发一次，触发后由 running 绑定控制不再重复
    Timer {
        id: dismissTimer
        interval: 3000
        repeat: false
        running: !_dismissed && (model.failed || model.progress >= 1.0)
        property bool _dismissed: false
        onTriggered: {
            _dismissed = true
            if (backend && model.iid)
                backend.dismissCard(model.iid)
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
            color: model.failed ? StyleTokens.errorLight
                 : model.progress >= 1.0 ? "#3fb950"
                 : StyleTokens.accent
            width: parent.width * Math.min(model.progress || 0, 1.0)
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
            text: model.failed ? (model.name || "") + " — " + (model.error || "失败")
                  : (model.name || "")
            font.pixelSize: StyleTokens.fontSizeCaption
            color: model.failed ? StyleTokens.errorLight
                 : model.progress >= 1.0 ? "#3fb950"
                 : StyleTokens.textPrimary
            elide: Text.ElideRight
            width: parent.width - 30
        }

        // ── 操作按钮 ──
        // 失败/完成后：关闭图标 (x.svg)，取消后或已终态：关闭
        // 活跃下载中：如果可取消则显示 x.svg 取消按钮
        Rectangle {
            id: actionBtn
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: 20; height: 20; radius: 10
            visible: model.canCancel !== false || model.failed || model.progress >= 1.0
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
                    if (model.failed || model.progress >= 1.0) {
                        // 完成态：关闭卡片
                        if (backend && model.iid)
                            backend.dismissCard(model.iid)
                    } else {
                        // 下载中：取消任务
                        if (backend && model.iid)
                            backend.cancelVersionInstall(model.iid)
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
                if (model.failed) return model.error || "失败"
                if (model.progress >= 1.0) return "完成 ✓"
                return fmtSpeed(model.speed || 0)
            }
            font.pixelSize: StyleTokens.fontSizeXs
            color: model.failed ? StyleTokens.errorLight
                 : model.progress >= 1.0 ? "#3fb950"
                 : StyleTokens.textMuted
            visible: true
        }

        Item { Layout.fillWidth: true }

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
        visible: model.progress < 1.0 && !model.failed
        property var __steps: model && model.steps ? model.steps : []

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
                    anchors.verticalCenter: parent.verticalCenter
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

                Text {
                    text: modelData.status === "active" ? (Math.round(stepSmoothPct) + "%") : ""
                    font.pixelSize: StyleTokens.fontSizeXs
                    color: StyleTokens.textMuted
                    visible: modelData.status === "active"
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
