import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: overlay
    color: "#0c0f16"
    visible: _shouldShow

    // Internal visibility — follows backend.launching unless error
    property bool _shouldShow: (backend ? backend.launching : false) || checkFailed
    on_ShouldShowChanged: {
        if (_shouldShow) {
            // Reset animation state
            flipped = false
            hideTimer.stop()
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
    Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }

    onVisibleChanged: {
        if (visible) {
            hideTimer.stop()  // 取消旧关闭定时器
            showTimer.stop()
            flipped = false
            showTimer.start()
            // Reset failure state when re-opening
            checkFailed = false
            checkFailedPhase = ""
            checkFailedDetails = ""
            missingFilesList = []
        }
    }

    Timer { id: showTimer; interval: 50; onTriggered: { flipped = true } }

    // Hide with flip-out animation
    function hide() {
        checkFailed = false
        checkFailedPhase = ""
        checkFailedDetails = ""
        flipped = false  // triggers reverse flip
        hideTimer.start()
    }

    Timer {
        id: hideTimer
        interval: 550  // slightly longer than animation (500ms)
        onTriggered: { _shouldShow = false }
    }

    // Progress state
    property int progressValue: 0
    property string statusText: "准备启动..."
    property string versionId: ""
    property string username: ""
    property int memory: 0

    // Update progress state from backend on change
    Timer {
        id: progressPoller
        interval: 200
        running: overlay.visible
        repeat: true
        onTriggered: {
            if (backend) {
                progressValue = backend.launchProgress || 0
                statusText = backend.launchStatus || "准备启动..."
                versionId = backend.launchVersion || ""
                username = backend.launchUsername || ""
                memory = 0
            }
        }
    }

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

        function onLaunchProgressChanged(pct, status) {
            progressValue = pct
            statusText = status
            // If progress resets to 0 with an error status, treat as failure
            if (pct === 0 && status && status.indexOf("失败") >= 0) {
                checkFailed = true
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
            Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }

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
            Behavior on opacity { NumberAnimation { duration: 200 } }

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
            Behavior on opacity { NumberAnimation { duration: 200 } }

            ColumnLayout {
                anchors.fill: parent
                spacing: 10

                // Progress bar
                Rectangle {
                    Layout.fillWidth: true
                    height: 6
                    radius: 3
                    color: "#1a1f2a"

                    Rectangle {
                        height: 6; radius: 3
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
                Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                Behavior on color { ColorAnimation { duration: 150 } }
                Behavior on border.color { ColorAnimation { duration: 150 } }
                Behavior on opacity { NumberAnimation { duration: 200 } }

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
                        if (backend) backend.openLatestLog()
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
                Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                Behavior on color { ColorAnimation { duration: 150 } }
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
