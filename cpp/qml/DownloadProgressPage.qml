// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

// DownloadProgressPage — multi-card install progress view
Rectangle {
    id: root
    readonly property bool hasBg: backend && typeof backend.customBgPath === "string" && backend.customBgPath.length > 0
    anchors.fill: parent
    color: hasBg ? "transparent" : "#0c0f16"
    property var backend: null
    onBackendChanged: {
        if (backend && backend.installCardsModel)
            cardsView.model = backend.installCardsModel
    }
    property var toastManager: null

    // ── Cancel button state ──
    property string _selectedIid: ""
    property string _selectedName: ""
    property bool _showCancel: false

    // ── Local refresh timer (200ms) — avoids C++ model-rebuild storm ──
    property int _refreshTick: 0
    Timer { interval: 200; running: true; repeat: true; onTriggered: _refreshTick++ }

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

    // ── empty state ──
    Rectangle {
        anchors.fill: parent; color: hasBg ? "transparent" : "#0c0f16"
        visible: backend.installCardsModel.count === 0
        ColumnLayout {
            anchors.centerIn: parent; spacing: 12
            Text { Layout.alignment: Qt.AlignHCenter; text: "\u2501\u2501"; font.pixelSize: 32; color: StyleTokens.bgElevated }
            Text { Layout.alignment: Qt.AlignHCenter; text: qsTr("当前没有进行中的任务"); font.pixelSize: StyleTokens.fontSizeMd; color: StyleTokens.textMuted }
            Text { Layout.alignment: Qt.AlignHCenter; text: qsTr("从下载页或安装页启动安装后，进度将在此显示"); font.pixelSize: StyleTokens.fontSizeSm; color: "#3a4058" }
        }
    }

    // ── card list ──
    ListView {
        id: cardsView
        anchors.fill: parent; anchors.margins: 16
        spacing: 12

        delegate: Rectangle {
            id: delegateRoot
            width: cardsView.width; implicitHeight: cardContent.implicitHeight + 20
            radius: StyleTokens.radiusLg; color: StyleTokens.bgSecondary
            border.color: "#1e2230"; border.width: 1

            // ── Card click → toggle cancel button ──
            MouseArea {
                anchors.fill: parent
                z: -1
                enabled: (model.progress || 0) > 0 && (model.progress || 0) < 1 && !model.failed
                         && (model.canCancel === undefined || model.canCancel !== false)
                onClicked: {
                    var clickedIid = cardsView.model.data(cardsView.model.index(index, 0), 0x101) || ""
                    if (root._showCancel) {
                        if (root._selectedIid === clickedIid) {
                            // Click same card → hide cancel
                            console.log("[anim] cancelBounceOut starting (same card)")
                            cancelBounceOut.start()
                            root._showCancel = false
                        } else {
                            // Click different card → slide-out then slide-in with new name
                            console.log("[anim] switch cancel to " + clickedIid + ", root.height=" + root.height)
                            cancelSwitchTimer._pendingIid = clickedIid
                            cancelBounceOut.start()
                            cancelSwitchTimer.start()
                        }
                    } else {
                        root._selectedIid = clickedIid
                        root._showCancel = root._selectedIid !== ""
                        if (root._showCancel) {
                            console.log("[anim] cancelBounceIn starting, root.height=" + root.height)
                            cancelBounceIn.start()
                        }
                    }
                }
            }

            ColumnLayout {
                id: cardContent
                anchors.fill: parent; anchors.margins: 14; spacing: 8
                property var card: modelData || {}
                property var _parsedSteps: model.steps || []  // QVariantList → already JS array, no JSON.parse needed
                readonly property bool _isDownload: {
                    var p = model.phase || "";
                    return p.includes("下载") || p.includes("校验") || p.includes("Download") || p.includes("Verify")
                        || p.includes("连通") || p.includes("获取") || p.includes("准备") || p.includes("测");
                }
                // Shared spin angle — survives Repeater delegate recreation
                property real _spinAngle: 0
                Timer {
                    interval: 16; running: true; repeat: true
                    onTriggered: cardContent._spinAngle = (cardContent._spinAngle + 6) % 360
                }

                // ── card header ──
                RowLayout { spacing: 8; Layout.fillWidth: true
                    // Display name
                    Text {
                        Layout.fillWidth: true
                        text: model.name || "--"
                        font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.Bold
                        color: model.failed ? "#c06050" : "#e8ecf8"
                        elide: Text.ElideRight
                    }
                    // Speed
                    Text {
                        text: fmtSpeed(model.speed || 0)
                        font.pixelSize: StyleTokens.fontSizeSm; color: "#5d6fe0"; font.family: "Consolas, monospace"
                    }
                }

                // ── progress bar ──
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: 6; radius: StyleTokens.radiusXs; color: StyleTokens.bgElevated
                    // Always visible — color distinguishes download vs install phase
                    Rectangle {
                        height: parent.height; radius: StyleTokens.radiusXs
                        color: {
                            if (model.failed) return "#802020"
                            if (cardContent._isDownload) return "#3a5ecc"
                            return "#2a3050"  // install phase: muted
                        }
                        width: parent.width * Math.min(1, (model.progress || 0))
                        Behavior on width { NumberAnimation { duration: 800; easing.type: Easing.OutCubic } }
                    }
                }

                // ── progress text ──
                RowLayout { Layout.fillWidth: true
                    Text {
                        Layout.fillWidth: true
                        text: model.failed ? ("\u2717 " + (model.error || "\u5b89\u88c5\u5931\u8d25")) : ((model.progress || 0) * 100).toFixed(1) + "%"
                        font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.Bold
                        color: model.failed ? "#e06050" : (cardContent._isDownload ? "#5d6fe0" : "#1084b8")
                    }
                    Text {
                        Layout.fillWidth: true
                        text: cardContent._isDownload ? (model.phase || "") : qsTr("\u5904\u7406\u672c\u5730\u6587\u4ef6")
                        font.pixelSize: StyleTokens.fontSizeSm; color: "#5070a0"
                        elide: Text.ElideRight
                    }

                }

                // ── step list ──
                ColumnLayout {
                    visible: cardContent._parsedSteps.length > 0
                    spacing: 4; Layout.fillWidth: true

                    Repeater {
                        model: cardContent._parsedSteps
                        delegate: Rectangle {
                            visible: (modelData && modelData.show !== undefined) ? modelData.show : true
                            Layout.fillWidth: true; implicitHeight: 28; color: "transparent"
                            RowLayout {
                                anchors.fill: parent; anchors.margins: 4; spacing: 8
                                // Status dot — animated for active state
                                Rectangle {
                                    implicitWidth: 20; implicitHeight: 20; radius: StyleTokens.radiusLg
                                    color: {
                                        var s = (modelData && modelData.status) ? modelData.status : "pending"
                                        if (s === "completed") return "#1a3a1a"
                                        if (s === "active") return "#1a1a3a"
                                        if (s === "failed") return "#3a1a1a"
                                        return "#1a1a20"
                                    }
                                    border.color: {
                                        var s = (modelData && modelData.status) ? modelData.status : "pending"
                                        if (s === "completed") return "#4bc870"
                                        if (s === "active") return "#5d6fe0"
                                        if (s === "failed") return "#e06060"
                                        return "#2a2a3a"
                                    }
                                    border.width: 1

                                    // Completed: checkmark
                                    Text {
                                        visible: modelData && modelData.status === "completed"
                                        anchors.centerIn: parent; font.pixelSize: StyleTokens.fontSizeSm
                                        text: "[完成]"; color: StyleTokens.success
                                    }
                                    // Failed: cross
                                    Text {
                                        visible: modelData && modelData.status === "failed"
                                        anchors.centerIn: parent; font.pixelSize: StyleTokens.fontSizeSm
                                        text: "[失败]"; color: StyleTokens.errorLight
                                    }
                                    // Active: rotating arc (pure QML, shared Timer driven)
                                    Item {
                                        visible: modelData && modelData.status === "active"
                                        anchors.centerIn: parent; width: 16; height: 16
                                        rotation: cardContent._spinAngle
                                        // 270-degree arc: full circle + mask to hide 90-degree sector
                                        Rectangle {
                                            anchors.centerIn: parent; width: 14; height: 14; radius: StyleTokens.radiusMd
                                            color: "transparent"
                                            border.width: 2; border.color: StyleTokens.accentLight
                                        }
                                        // Mask covers top-right quadrant to create arc gap
                                        Rectangle {
                                            width: 8; height: 16
                                            anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                                            color: StyleTokens.bgCard  // matches active status dot background
                                        }
                                    }
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData ? (modelData.name || "") : ""
                                    font.pixelSize: StyleTokens.fontSizeSm
                                    color: {
                                        var s = (modelData && modelData.status) ? modelData.status : "pending"
                                        if (s === "completed") return "#707888"
                                        if (s === "active") return "#c8d0e8"
                                        return "#4a5060"
                                    }
                                    elide: Text.ElideRight
                                }
                                Text {
                                    visible: modelData && modelData.status === "active"
                                    text: (modelData && modelData.status === "active") ? (modelData.percentage || 0) + "%" : ""
                                    font.pixelSize: StyleTokens.fontSizeSm; font.weight: Font.Bold; color: StyleTokens.accentLight
                                }
                            }
                        }
                    }
                }

                // ── Import failed countdown (60s auto-dismiss) ──
                Rectangle {
                    visible: (model.hasUserDataImport || false) && (model.failed || false) && (model.importFailedAtMs || 0) > 0
                    implicitHeight: 30; Layout.fillWidth: true
                    radius: StyleTokens.radiusSm; color: "#1a1000"
                    RowLayout {
                        anchors.fill: parent; anchors.margins: 6; spacing: 4
                        Image {
                            source: "icons/lucide/info.svg"
                            width: 14; height: 14
                            fillMode: Image.PreserveAspectFit
                        }
                        Text {
                            id: countdownLabel
                            font.pixelSize: StyleTokens.fontSizeSm; color: "#e0a040"
                            Layout.fillWidth: true
                            text: countdownTimer.remaining > 0
                                ? "卡片将在 " + countdownTimer.remaining + " 秒后自动关闭"
                                : "卡片即将关闭..."
                            property int remaining: 60
                        }
                        Timer {
                            id: countdownTimer
                            interval: 1000; repeat: true
                            running: countdownLabel.visible
                            property int remaining: 60
                            onTriggered: {
                                var now = Date.now()
                                var ms = (model.importFailedAtMs || 0)
                                remaining = Math.max(0, Math.ceil((ms + 60000 - now) / 1000))
                                countdownLabel.remaining = remaining
                                if (remaining <= 0 && backend) {
                                    var iid = model.iid || ""
                                    if (iid) backend.dismissCard(iid)
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ── Cancel button (elastic bounce from bottom) ──
    Rectangle {
        id: cancelBtn
        visible: true
        anchors.horizontalCenter: parent.horizontalCenter
        width: 260; height: 44; radius: StyleTokens.radiusLg
        color: cancelHover.containsMouse ? "#2a1820" : "#1a1420"
        border.color: cancelHover.containsMouse ? "#e06060" : "#a04040"
        border.width: 1

        // Static initial — off-screen. Animation explicitly controls position
        // via PropertyAction + NumberAnimation, no binding to interfere.
        opacity: 0.0
        y: -100

        // Hover scale (1.03 on hover, 1.0 idle)
        property real _hoverScale: cancelHover.containsMouse ? 1.03 : 1.0
        // Press bounce (animated, reset to 1.0 after bounce)
        property real _bounceScale: 1.0
        scale: _hoverScale * _bounceScale

        // Entrance: slide up from below with elastic bounce
        SequentialAnimation {
            id: cancelBounceIn
            PropertyAction { target: cancelBtn; property: "y"; value: root.height + 60 }
            PropertyAction { target: cancelBtn; property: "opacity"; value: 0.0 }
            ParallelAnimation {
                NumberAnimation { target: cancelBtn; property: "y"; to: root.height - cancelBtn.height - 24; duration: 550; easing.type: Easing.OutBack; easing.overshoot: 2.5 }
                NumberAnimation { target: cancelBtn; property: "opacity"; to: 1.0; duration: 300 }
            }
        }

        // Exit: slide down and fade out
        SequentialAnimation {
            id: cancelBounceOut
            ParallelAnimation {
                NumberAnimation { target: cancelBtn; property: "y"; to: root.height + 60; duration: 250; easing.type: Easing.InCubic }
                NumberAnimation { target: cancelBtn; property: "opacity"; to: 0.0; duration: 200 }
            }
        }

        // Timer for switch animation: hide → update ID → show
        Timer {
            id: cancelSwitchTimer
            interval: 280
            onTriggered: {
                root._selectedIid = cancelSwitchTimer._pendingIid || ""
                cancelBounceIn.start()
            }
            property string _pendingIid: ""
        }

        Behavior on color { ColorAnimation { duration: 200 } }
        Behavior on border.color { ColorAnimation { duration: 200 } }
        Behavior on _hoverScale { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }

        // Elastic press feedback on click
        SequentialAnimation {
            id: pressBounce
            PropertyAction { target: cancelBtn; property: "_bounceScale"; value: 0.92 }
            NumberAnimation { target: cancelBtn; property: "_bounceScale"; to: 1.0; duration: 450; easing.type: Easing.OutBack; easing.overshoot: 2.5 }
        }

        Row {
            anchors.centerIn: parent; spacing: 10
            // Red circle + white X — custom icon matching button theme
            Rectangle {
                width: 20; height: 20; radius: StyleTokens.radiusLg; anchors.verticalCenter: parent.verticalCenter
                color: "#cc4040"
                Text {
                    anchors.centerIn: parent
                    text: "[失败]"
                    font.pixelSize: StyleTokens.fontSizeSm; font.weight: Font.Bold; color: StyleTokens.textInverse
                }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("取消 %1").arg(root._selectedIid || "(empty)")
                font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.DemiBold
                color: cancelHover.containsMouse ? "#ff8080" : "#c05050"
            }
        }

        MouseArea {
            id: cancelHover
            anchors.fill: parent; hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                console.log("[cancel-btn] clicked, backend=" + (typeof backend) + " selectedIid=" + root._selectedIid)
                pressBounce.start()
                if (root._selectedIid) {
                    console.log("[cancel-btn] calling cancelVersionInstall(" + root._selectedIid + ")")
                    backend.cancelVersionInstall(root._selectedIid)
                } else {
                    console.log("[cancel-btn] SKIP: _selectedIid is empty")
                }
                root._showCancel = false
                // Immediately hide progress page and go back to home
                var win = root.Window.window
                if (win && win.hideDownloadNav) win.hideDownloadNav()
            }
        }
    }

    // ── done state (all cards gone, install finished) ──
    Rectangle {
        anchors.fill: parent; color: hasBg ? "transparent" : "#0c0f16"
        visible: backend && backend.installPhase === "done" && !cardsView.count
        ColumnLayout {
            anchors.centerIn: parent; spacing: 12
            Text { Layout.alignment: Qt.AlignHCenter; text: "\u2714"; font.pixelSize: 48; color: StyleTokens.success }
            Text { Layout.alignment: Qt.AlignHCenter; text: qsTr("安装完成"); font.pixelSize: StyleTokens.fontSizeLg; font.weight: Font.Bold; color: StyleTokens.success }
            Text { Layout.alignment: Qt.AlignHCenter; text: qsTr("可在下载页面启动"); font.pixelSize: StyleTokens.fontSizeSm; color: "#608868" }
        }
    }
}
