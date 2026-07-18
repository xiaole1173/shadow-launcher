// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: overlay
    readonly property bool hasBg: backend && typeof backend.customBgPath === "string" && backend.customBgPath.length > 0
    color: hasBg ? Qt.rgba(0.047, 0.059, 0.086, 0.92) : "#0c0f16"
    property var backend: null
    property var toastManager: null

    // Block all mouse events from passing through to underlying UI
    MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons }

    // ── Visibility ──
    property bool _dismissed: false
    property bool _animatingOut: false
    visible: ((backend ? backend.launching : false) || checkFailed || _animatingOut) && !_dismissed

    // ── Flip animation (entry + exit) ──
    property bool flipped: false
    transform: Rotation {
        id: flipRotation
        origin.x: overlay.width / 2
        origin.y: overlay.height / 2
        axis { x: 0; y: 1; z: 0 }
        angle: flipped ? 0 : 90
        Behavior on angle { NumberAnimation { duration: 500; easing.type: Easing.OutBack } }
    }
    opacity: flipped ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeInDuration; easing.type: AnimationTokens.itemFadeInEasing } }

    onVisibleChanged: {
        if (visible) {
            _animatingOut = false
            hideTimer.stop()
            showTimer.stop()
            flipped = false
            showTimer.start()
        }
    }

    Timer { id: showTimer; interval: 50; onTriggered: { flipped = true } }

    function hide() {
        console.log("[overlay] hide()")
        checkFailed = false
        checkFailedPhase = ""
        checkFailedDetails = ""
        flipped = false
        _animatingOut = true
        hideTimer.start()
    }

    Timer { id: hideTimer; interval: 550; onTriggered: { _dismissed = true; _animatingOut = false } }

    Timer { id: closeTimer; interval: 1500; onTriggered: { if (!checkFailed) hide() } }

    // ── Progress state ──
    property int progressValue: 0
    property string statusText: "准备启动..."
    property string versionId: ""
    property string username: ""
    property int memory: 0

    // ── Check failure state ──
    property bool checkFailed: false
    property string checkFailedPhase: ""
    property string checkFailedDetails: ""
    property var missingFilesList: []
    property string checkWarning: ""

    onCheckFailedChanged: {
        if (checkFailed) {
            _animatingOut = false
            hideTimer.stop()
        }
    }

    // ── Backend signal handlers (single handler — no duplicates) ──
    Connections {
        target: backend
        enabled: backend !== null

        function onLaunchStateChanged() {
            console.log("[overlay] onLaunchStateChanged: launching=" + (backend ? backend.launching : "null"))
            if (backend && backend.launching) {
                _dismissed = false
                _animatingOut = false
                progressValue = 0
                statusText = "准备启动..."
                versionId = backend.launchVersion || ""
                username = backend.launchUsername || ""
                memory = backend.resolvedMemoryMB(versionId || "")
                checkFailed = false
                checkFailedPhase = ""
                checkFailedDetails = ""
                missingFilesList = []
                checkWarning = ""
            }
        }

        function onLaunchProgressChanged(pct, status) {
            console.log("[overlay] progress: " + pct + "% - " + status)
            progressValue = pct
            statusText = status
            if (pct === 0 && status && status.indexOf("失败") >= 0) {
                checkFailed = true
            }
            if (pct === 100 && !checkFailed) {
                closeTimer.start()
            }
        }

        function onLaunchCheckFailed(phase, details) {
            checkFailed = true
            checkFailedPhase = phase || ""
            checkFailedDetails = details || ""
        }

        function onLaunchCheckMissingFiles(files) {
            missingFilesList = files || []
        }

        function onLaunchCheckWarning(warning) {
            checkWarning = warning || ""
        }
    }

    // ── UI ──
    ColumnLayout {
        anchors.centerIn: parent
        spacing: 16
        width: Math.min(parent.width * 0.6, 480)

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: checkFailed ? "启动检查失败" : "正在启动 Minecraft"
            font.pixelSize: StyleTokens.fontSizeXl
            font.bold: true
            color: checkFailed ? "#ff6060" : "#d0d4e0"
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: versionId ? ("版本 " + versionId) : ""
            color: StyleTokens.textMuted
            font.pixelSize: StyleTokens.fontSizeSm
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: username ? ("玩家: " + username + "  |  内存: " + memory + " MB") : ""
            color: "#606478"
            font.pixelSize: StyleTokens.fontSizeSm
            visible: !checkFailed
        }

        // Failure state
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: errorColumn.implicitHeight + 24
            color: StyleTokens.bgSecondary
            radius: StyleTokens.radiusLg
            border.color: StyleTokens.errorBg
            visible: checkFailed
            opacity: checkFailed ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeInDuration; easing.type: AnimationTokens.itemFadeInEasing } }

            ColumnLayout {
                id: errorColumn
                anchors.fill: parent
                anchors.margins: 12
                spacing: 6

                Text {
                    text: checkFailedPhase ? ("问题: " + checkFailedPhase) : "启动检查未通过"
                    color: "#ff8080"
                    font.pixelSize: StyleTokens.fontSizeMd
                    font.weight: Font.DemiBold
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }

                Text {
                    text: checkFailedDetails
                    color: "#c08080"
                    font.pixelSize: StyleTokens.fontSizeSm
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    visible: checkFailedDetails !== ""
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(missingFilesList.length * 22 + 8, 200)
                    color: StyleTokens.bgPrimary
                    radius: StyleTokens.radiusSm
                    visible: missingFilesList.length > 0
                    clip: true

                    ListView {
                        id: missingListView
                        anchors.fill: parent
                        anchors.margins: 4
                        model: missingFilesList.slice(0, 10)
                        spacing: 2
                        delegate: Text {
                            text: "- " + modelData
                            color: "#c06060"
                            font.pixelSize: StyleTokens.fontSizeSm
                            font.family: "Consolas, monospace"
                            width: missingListView.width
                            elide: Text.ElideRight
                        }
                    }

                    Text {
                        anchors.bottom: parent.bottom
                        anchors.right: parent.right
                        anchors.margins: 4
                        text: missingFilesList.length > 10 ? ("... 等共 " + missingFilesList.length + " 个文件") : ""
                        color: "#806060"
                        font.pixelSize: StyleTokens.fontSizeXs
                        visible: missingFilesList.length > 10
                    }
                }

                Text {
                    text: qsTr("建议: 请重新下载该版本以恢复缺失文件")
                    color: "#807880"
                    font.pixelSize: StyleTokens.fontSizeSm
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    visible: missingFilesList.length > 0
                }
            }
        }

        // Warning
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: warnText.implicitHeight + 16
            color: "#1a1800"
            radius: StyleTokens.radiusMd
            border.color: StyleTokens.warningBg
            visible: checkWarning !== ""
            opacity: checkWarning !== "" ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeOutDuration; easing.type: AnimationTokens.itemFadeOutEasing } }

            Text {
                id: warnText
                anchors.centerIn: parent
                text: checkWarning
                color: "#e0c040"
                font.pixelSize: StyleTokens.fontSizeSm
                width: parent.width - 20
                wrapMode: Text.WordWrap
            }
        }

        // Progress (normal state)
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: checkFailed ? 0 : 50
            visible: !checkFailed
            opacity: checkFailed ? 0 : 1
            Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeOutDuration; easing.type: AnimationTokens.itemFadeOutEasing } }

            ColumnLayout {
                anchors.fill: parent
                spacing: 10

                Rectangle {
                    Layout.fillWidth: true
                    height: 6
                    radius: StyleTokens.radiusSm
                    color: StyleTokens.bgInput

                    Rectangle {
                        height: 6; radius: StyleTokens.radiusSm
                        color: "#3a4eb8"
                        width: parent.width * (progressValue / 100)
                        Behavior on width { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                    }
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: progressValue + "%"
                    font.pixelSize: StyleTokens.fontSize2xl
                    font.bold: true
                    color: "#d0d4e0"
                }
            }
        }

        // Status text
        Text {
            id: statusLabel
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            Layout.maximumHeight: 48
            text: statusText
            color: checkFailed ? "#a08080" : "#9094a8"
            font.pixelSize: StyleTokens.fontSizeSm
            elide: Text.ElideRight
            maximumLineCount: 3
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            clip: true
            visible: statusText !== ""
        }

        // Action buttons
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 12

            Rectangle {
                width: 100; height: 34; radius: StyleTokens.radiusMd
                color: logMouse ? (logMouse.containsMouse ? "#1a2838" : "transparent") : "transparent"
                border.color: logMouse ? (logMouse.containsMouse ? "#2858a0" : "#283850") : "#283850"
                scale: logMouse ? (logMouse.pressed ? 0.9 : (logMouse.containsMouse ? 1.04 : 1.0)) : 1.0
                visible: checkFailed
                opacity: checkFailed ? 1 : 0
                Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }
                Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                Behavior on border.color { ColorAnimation { duration: 150 } }
                Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeOutDuration; easing.type: AnimationTokens.itemFadeOutEasing } }

                Text {
                    anchors.centerIn: parent
                    text: qsTr("查看日志")
                    font.pixelSize: StyleTokens.fontSizeSm
                    color: logMouse ? (logMouse.containsMouse ? "#80a0ff" : "#6090d0") : "#6090d0"
                }

                MouseArea {
                    id: logMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (backend) {
                            var ok = backend.openLatestLog(versionId)
                            if (!ok && toastManager) toastManager.show("日志文件尚未生成", "游戏可能崩溃过快，未能创建日志。可打开日志目录查看是否有旧日志。", 5000)
                        }
                    }
                }
            }

            // ── Relogin button (login check failures) ──
            Rectangle {
                width: 120; height: 34; radius: StyleTokens.radiusMd
                color: reloginMouse ? (reloginMouse.containsMouse ? "#1a1a38" : "transparent") : "transparent"
                border.color: reloginMouse ? (reloginMouse.containsMouse ? "#3868c0" : "#304070") : "#304070"
                scale: reloginMouse ? (reloginMouse.pressed ? 0.9 : (reloginMouse.containsMouse ? 1.04 : 1.0)) : 1.0
                visible: checkFailed && checkFailedPhase === "登录状态"
                opacity: (checkFailed && checkFailedPhase === "登录状态") ? 1 : 0
                Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }
                Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                Behavior on border.color { ColorAnimation { duration: 150 } }
                Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeInDuration; easing.type: AnimationTokens.itemFadeInEasing } }

                Text {
                    anchors.centerIn: parent
                    text: qsTr("重新登录")
                    font.pixelSize: StyleTokens.fontSizeSm
                    font.weight: Font.DemiBold
                    color: reloginMouse ? (reloginMouse.containsMouse ? "#a0c8ff" : "#80a0e0") : "#80a0e0"
                }

                MouseArea {
                    id: reloginMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        overlay.hide()
                        if (backend) {
                            backend.logout()  // clear expired MS session before re-login
                            backend.microsoftLogin()
                        }
                    }
                }
            }

            Rectangle {
                width: checkFailed ? 140 : 120; height: 34; radius: StyleTokens.radiusMd
                color: checkFailed ? (actionMouse ? (actionMouse.containsMouse ? "#1a2a18" : "transparent") : "transparent")
                                   : (actionMouse ? (actionMouse.containsMouse ? "#2a1518" : "transparent") : "transparent")
                border.color: checkFailed ? (actionMouse ? (actionMouse.containsMouse ? "#286028" : "#284028") : "#284028")
                                          : (actionMouse ? (actionMouse.containsMouse ? "#602828" : "#402428") : "#402428")
                scale: actionMouse ? (actionMouse.pressed ? 0.9 : (actionMouse.containsMouse ? 1.04 : 1.0)) : 1.0
                Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }
                Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                Behavior on border.color { ColorAnimation { duration: 150 } }

                Text {
                    anchors.centerIn: parent
                    text: checkFailed ? "返回启动页" : "取消启动"
                    font.pixelSize: StyleTokens.fontSizeSm
                    color: checkFailed ? (actionMouse ? (actionMouse.containsMouse ? "#80ff80" : "#60c060") : "#60c060")
                                       : (actionMouse ? (actionMouse.containsMouse ? "#ff6060" : "#c05050") : "#c05050")
                }

                MouseArea {
                    id: actionMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (checkFailed) {
                            overlay.hide()
                        } else {
                            overlay.hide()
                            if (backend) backend.cancelLaunch()
                        }
                    }
                }
            }
        }
    }
}
