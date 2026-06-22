import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: overlay
    color: "#0c0f16"
    property var backend: null
    property var toastManager: null

    Component.onCompleted: console.log("[overlay] LOADED backend=" + (backend ? "valid" : "null"))

    // Visibility: dismiss flag overrides all other conditions
    property bool _dismissed: false
    property bool _animatingOut: false
    visible: ((backend ? backend.launching : false) || checkFailed || _animatingOut) && !_dismissed

    onCheckFailedChanged: {
        if (checkFailed) {
            console.log("[overlay] checkFailed=true phase=" + checkFailedPhase + " details=" + checkFailedDetails)
            // Show error: keep visible, stop any closing
            _animatingOut = false
            hideTimer.stop()
        } else if (!_animatingOut && !(backend && backend.launching)) {
            console.log("[overlay] checkFailed cleared + not launching → should dismiss")
            // No reason to stay visible — start exit animation if this was a normal dismiss
            // (handled by hide() function)
        }
    }

    // Flip-page animation on appearance
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

    // Hide with flip-out animation — sets _dismissed so binding allows close
    function hide() {
        console.log("[overlay] hide() called: checkFailed=" + checkFailed)
        checkFailed = false
        checkFailedPhase = ""
        checkFailedDetails = ""
        flipped = false
        _animatingOut = true
        hideTimer.start()
    }

    Timer {
        id: hideTimer
        interval: 550
        onTriggered: {
            console.log("[overlay] hideTimer fired -> _dismissed=true")
            _dismissed = true; _animatingOut = false
        }
    }

    Timer {
        id: closeTimer
        interval: 1500
        onTriggered: {
            console.log("[overlay] closeTimer fired -> calling hide()")
            if (!checkFailed) hide()
        }
    }

    // Progress state
    property int progressValue: 0
    property string statusText: "准备启动..."
    property string versionId: ""
    property string username: ""
    property int memory: 0

    // Check failure state
    property bool checkFailed: false
    property string checkFailedPhase: ""
    property string checkFailedDetails: ""
    property var missingFilesList: []
    property string checkWarning: ""

    // Track progress and check results from backend
    Connections {
        target: backend
        enabled: backend !== null

        function onLaunchStateChanged() {
            if (versionId === "") versionId = backend.launchVersion
        }

        function onLaunchProgressChanged(pct, status) {
            console.log("[overlay] onLaunchProgressChanged: " + pct + "% - " + status)
            progressValue = pct
            statusText = status
            if (pct === 0 && status && status.indexOf("失败") >= 0) {
                checkFailed = true
            }
            if (pct === 100 && !checkFailed) {
                console.log("[overlay] 100% detected -> starting closeTimer (1500ms)")
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

        function onLaunchStateChanged() {
            console.log("[overlay] onLaunchStateChanged: backend.launching=" + (backend ? backend.launching : "null"))
            if (backend && backend.launching) {
                console.log("[overlay] New launch -> reset _dismissed + progress")
                _dismissed = false
                progressValue = 0
                statusText = "准备启动..."
                checkFailed = false
                checkFailedPhase = ""
                checkFailedDetails = ""
            }
        }

        function onLaunchCheckWarning(warning) {
            checkWarning = warning || ""
        }
    }

    // Auto-clear warning after 5 seconds
    Timer {
        id: warningTimer
        interval: 5000
        onTriggered: checkWarning = ""
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 16
        width: Math.min(parent.width * 0.6, 480)

        // Header — changes based on state
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: checkFailed ? "启动检查失败" : "正在启动 Minecraft"
            font.pixelSize: 20
            font.bold: true
            color: checkFailed ? "#ff6060" : "#d0d4e0"
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: versionId ? ("版本 " + versionId) : ""
            color: "#505468"
            font.pixelSize: 12
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: username ? ("玩家: " + username + "  |  内存: " + memory + " MB") : ""
            color: "#606478"
            font.pixelSize: 11
            visible: !checkFailed
        }

        // ── Failure state display ──
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: errorColumn.implicitHeight + 24
            color: "#1a1216"
            radius: 8
            border.color: "#402428"
            visible: checkFailed
            opacity: checkFailed ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeInDuration; easing.type: AnimationTokens.itemFadeInEasing } }

            ColumnLayout {
                id: errorColumn
                anchors.fill: parent
                anchors.margins: 12
                spacing: 6

                // Failure phase
                Text {
                    text: checkFailedPhase ? ("问题: " + checkFailedPhase) : "启动检查未通过"
                    color: "#ff8080"
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }

                // Failure details
                Text {
                    text: checkFailedDetails
                    color: "#c08080"
                    font.pixelSize: 12
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    visible: checkFailedDetails !== ""
                }

                // Missing files list (scrollable if many)
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(missingFilesList.length * 22 + 8, 200)
                    color: "#0e0a0c"
                    radius: 4
                    visible: missingFilesList.length > 0
                    clip: true

                    ListView {
                        id: missingListView
                        anchors.fill: parent
                        anchors.margins: 4
                        model: missingFilesList.slice(0, 10)  // Show max 10
                        spacing: 2
                        delegate: Text {
                            text: "• " + modelData
                            color: "#c06060"
                            font.pixelSize: 11
                            font.family: "Consolas, monospace"
                            width: missingListView.width
                            elide: Text.ElideRight
                        }
                    }

                    // "and N more" indicator
                    Text {
                        anchors.bottom: parent.bottom
                        anchors.right: parent.right
                        anchors.margins: 4
                        text: missingFilesList.length > 10 ? ("... 等共 " + missingFilesList.length + " 个文件") : ""
                        color: "#806060"
                        font.pixelSize: 10
                        visible: missingFilesList.length > 10
                    }
                }

                // Recovery hint
                Text {
                    text: "💡 建议: 请重新下载该版本以恢复缺失文件"
                    color: "#807880"
                    font.pixelSize: 11
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    visible: missingFilesList.length > 0
                }
            }
        }

        // ── Warning display ──
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: warnText.implicitHeight + 16
            color: "#1a1800"
            radius: 6
            border.color: "#403800"
            visible: checkWarning !== ""
            opacity: checkWarning !== "" ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeOutDuration; easing.type: AnimationTokens.itemFadeOutEasing } }

            Text {
                id: warnText
                anchors.centerIn: parent
                text: checkWarning
                color: "#e0c040"
                font.pixelSize: 12
                width: parent.width - 20
                wrapMode: Text.WordWrap
            }
        }

        // ── Normal progress display ──
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: checkFailed ? 0 : 50
            visible: !checkFailed
            opacity: checkFailed ? 0 : 1
            Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeOutDuration; easing.type: AnimationTokens.itemFadeOutEasing } }

            ColumnLayout {
                anchors.fill: parent
                spacing: 10

                // Progress bar
                Rectangle {
                    Layout.fillWidth: true
                    height: 6
                    radius: 4
                    color: "#1a1f2a"

                    Rectangle {
                        height: 6; radius: 4
                        color: "#3a4eb8"
                        width: parent.width * (progressValue / 100)
                        Behavior on width { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                    }
                }

                // Percentage
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: progressValue + "%"
                    font.pixelSize: 24
                    font.bold: true
                    color: "#d0d4e0"
                }
            }
        }

        // Status text (max 3 lines to prevent layout deformation)
        Text {
            id: statusLabel
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            Layout.maximumHeight: 48
            text: statusText
            color: checkFailed ? "#a08080" : "#9094a8"
            font.pixelSize: 12
            elide: Text.ElideRight
            maximumLineCount: 3
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            clip: true
            visible: statusText !== ""
        }

        // Action button row (changes based on state)
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 12

            // "查看日志" button — visible when check fails
            Rectangle {
                width: 100
                height: 34
                radius: 6
                color: logMouse.containsMouse ? "#1a2838" : "transparent"
                border.color: logMouse.containsMouse ? "#2858a0" : "#283850"
                scale: logMouse.pressed ? 0.9 : (logMouse.containsMouse ? 1.04 : 1.0)
                visible: checkFailed
                opacity: checkFailed ? 1 : 0
                Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }
                Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                Behavior on border.color { ColorAnimation { duration: 150 } }
                Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeOutDuration; easing.type: AnimationTokens.itemFadeOutEasing } }

                Text {
                    anchors.centerIn: parent
                    text: "📋 查看日志"
                    font.pixelSize: 12
                    color: logMouse.containsMouse ? "#80a0ff" : "#6090d0"
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

            // Main action button
            Rectangle {
                width: checkFailed ? 140 : 120
                height: 34
                radius: 6
                color: checkFailed ? (actionMouse.containsMouse ? "#1a2a18" : "transparent")
                                   : (actionMouse.containsMouse ? "#2a1518" : "transparent")
                border.color: checkFailed ? (actionMouse.containsMouse ? "#286028" : "#284028")
                                          : (actionMouse.containsMouse ? "#602828" : "#402428")
                scale: actionMouse.pressed ? 0.9 : (actionMouse.containsMouse ? 1.04 : 1.0)
                Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }
                Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                Behavior on border.color { ColorAnimation { duration: 150 } }

                Text {
                    anchors.centerIn: parent
                    text: checkFailed ? "返回启动页" : "取消启动"
                    font.pixelSize: 12
                    color: checkFailed ? (actionMouse.containsMouse ? "#80ff80" : "#60c060")
                                       : (actionMouse.containsMouse ? "#ff6060" : "#c05050")
                }

                MouseArea {
                    id: actionMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (checkFailed) {
                            // Failed — just hide overlay to return to home
                            overlay.hide()
                        } else {
                            // In progress — cancel launch
                            overlay.hide()
                            if (backend) backend.cancelLaunch()
                        }
                    }
                }
            }
        }
    }
}
