import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

// DownloadProgressPage — shows install progress with step checklist + aggregate stats
Rectangle {
    id: root
    anchors.fill: parent
    color: "#0c0f16"
    property var backend: null
    property var toastManager: null

    function fmtSize(bytes) {
        if (!bytes || bytes < 0) return "0 B"
        var units = ["B", "KB", "MB", "GB"]
        var u = 0
        var v = bytes
        while (v >= 1024 && u < units.length - 1) { v /= 1024; u++ }
        return v.toFixed(u === 0 ? 0 : 1) + " " + units[u]
    }

    // ── empty state ──
    Rectangle {
        anchors.fill: parent
        color: "#0c0f16"
        visible: !backend || !backend.installing
        ColumnLayout {
            anchors.centerIn: parent; spacing: 12
            Text { Layout.alignment: Qt.AlignHCenter; text: "\u2501\u2501"; font.pixelSize: 32; color: "#1e2230" }
            Text { Layout.alignment: Qt.AlignHCenter; text: "当前没有进行中的任务"; font.pixelSize: 14; color: "#505870" }
            Text { Layout.alignment: Qt.AlignHCenter; text: "从下载页或安装页启动安装后，进度将在此显示"; font.pixelSize: 11; color: "#3a4058" }
        }
    }

    // ── install active state ──
    Rectangle {
        anchors.fill: parent
        color: "#0c0f16"
        visible: backend && backend.installing

        RowLayout {
            anchors.fill: parent; anchors.margins: 16; spacing: 16

            // ── Left: aggregate stats panel ──
            Rectangle {
                Layout.preferredWidth: 200; Layout.fillHeight: true
                radius: 10; color: "#11141c"
                border.color: "#1e2230"; border.width: 1

                ColumnLayout {
                    anchors.centerIn: parent; spacing: 20
                    width: parent.width - 24

                    // Total progress
                    ColumnLayout { spacing: 4; Layout.fillWidth: true
                        Text { text: "总进度"; font.pixelSize: 12; color: "#5d6fe0"; font.weight: Font.DemiBold }
                        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: "#1e2230" }
                        Text {
                            text: (backend && backend.installTotalProgress > 0)
                                  ? (backend.installTotalProgress * 100).toFixed(1) + " %"
                                  : "0 %"
                            font.pixelSize: 28; font.weight: Font.Bold; color: "#e8ecf8"
                        }
                        Rectangle {
                            Layout.fillWidth: true; implicitHeight: 4; radius: 2; color: "#1e2230"
                            Rectangle {
                                width: (backend && backend.installTotalProgress > 0)
                                       ? parent.width * backend.installTotalProgress : 0
                                height: parent.height; radius: 2; color: "#5d6fe0"
                                Behavior on width { NumberAnimation { duration: 200 } }
                            }
                        }
                    }

                    // Speed
                    ColumnLayout { spacing: 4; Layout.fillWidth: true
                        Text { text: "下载速度"; font.pixelSize: 12; color: "#5d6fe0"; font.weight: Font.DemiBold }
                        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: "#1e2230" }
                        Text {
                            text: backend ? fmtSize(backend.installSpeed) + "/s" : "0 B/s"
                            font.pixelSize: 22; font.weight: Font.Bold; color: "#e8ecf8"
                        }
                    }

                    // Remaining files
                    ColumnLayout { spacing: 4; Layout.fillWidth: true
                        Text { text: "剩余步骤"; font.pixelSize: 12; color: "#5d6fe0"; font.weight: Font.DemiBold }
                        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: "#1e2230" }
                        Text {
                            text: backend ? String(backend.installRemainingSteps) : "0"
                            font.pixelSize: 22; font.weight: Font.Bold; color: "#e8ecf8"
                        }
                    }

                    // Pause / Cancel buttons
                    RowLayout { spacing: 8; Layout.fillWidth: true
                        Rectangle {
                            Layout.fillWidth: true; implicitHeight: 32; radius: 6
                            color: pauseMouse.containsMouse ? "#3a5ed0" : "#2a4590"
                            Text { anchors.centerIn: parent; text: backend && backend.installPaused ? "继续" : "暂停"; font.pixelSize: 12; color: "#fff" }
                            MouseArea { id: pauseMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (backend) {
                                        if (backend.installPaused) backend.resumeInstall()
                                        else backend.pauseInstall()
                                    }
                                }
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true; implicitHeight: 32; radius: 6
                            color: cancelMouse2.containsMouse ? "#5a2a2a" : "#3a1a1a"
                            Text { anchors.centerIn: parent; text: "取消"; font.pixelSize: 12; color: "#e06060" }
                            MouseArea { id: cancelMouse2; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (backend) {
                                        backend.cancelInstall()
                                        if (toastManager) toastManager.show("已取消安装")
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ── Right: version info + step checklist ──
            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true
                radius: 10; color: "#11141c"
                border.color: "#1e2230"; border.width: 1

                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 16; spacing: 8

                    // Version header
                    RowLayout { spacing: 8; Layout.fillWidth: true
                        Text {
                            text: (backend ? backend.installVersionId : "--") + " 安装"
                            font.pixelSize: 15; font.weight: Font.Bold; color: "#e8ecf8"
                            Layout.fillWidth: true; elide: Text.ElideRight
                        }
                        Rectangle {
                            implicitWidth: statusLabel.implicitWidth + 16; implicitHeight: 24; radius: 4
                            color: (backend && backend.installPhase === "verifying") ? "#3a3a10"
                                   : (backend && backend.installPhase === "done") ? "#1a3028" : "#1a1a30"
                            Text {
                                id: statusLabel; anchors.centerIn: parent
                                text: (backend && backend.installPhase === "verifying") ? "校验中"
                                      : (backend && backend.installPhase === "done") ? "已完成" : "下载中"
                                font.pixelSize: 11; color: (backend && backend.installPhase === "done") ? "#4bc870"
                                                          : (backend && backend.installPhase === "verifying") ? "#c8c840" : "#5d6fe0"
                            }
                        }
                    }

                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: "#1e2230" }

                    // Step checklist
                    ListView {
                        id: stepList
                        Layout.fillWidth: true; Layout.fillHeight: true
                        model: backend ? (backend.installSteps || []) : []
                        clip: true; spacing: 6

                        delegate: Rectangle {
                            width: stepList.width; implicitHeight: 36; radius: 6
                            color: "transparent"

                            RowLayout {
                                anchors.fill: parent; anchors.margins: 6; spacing: 10

                                // Status icon
                                Rectangle {
                                    implicitWidth: 28; implicitHeight: 28; radius: 14
                                    color: {
                                        var st = (modelData && modelData.status) ? modelData.status : "pending"
                                        if (st === "completed") return "#1a3a1a"
                                        if (st === "active") return "#1a1a3a"
                                        if (st === "failed") return "#3a1a1a"
                                        return "#1a1a20"
                                    }
                                    border.color: {
                                        var st = (modelData && modelData.status) ? modelData.status : "pending"
                                        if (st === "completed") return "#4bc870"
                                        if (st === "active") return "#5d6fe0"
                                        if (st === "failed") return "#e06060"
                                        return "#2a2a3a"
                                    }
                                    border.width: 1.5

                                    Text {
                                        anchors.centerIn: parent
                                        text: {
                                            var st = (modelData && modelData.status) ? modelData.status : "pending"
                                            if (st === "completed") return "\u2714"
                                            if (st === "active") return "\u22EF"
                                            if (st === "failed") return "\u2718"
                                            return "\u2022"
                                        }
                                        font.pixelSize: st === "completed" ? 14 : 12
                                        color: {
                                            var st = (modelData && modelData.status) ? modelData.status : "pending"
                                            if (st === "completed") return "#4bc870"
                                            if (st === "active") return "#5d6fe0"
                                            if (st === "failed") return "#e06060"
                                            return "#4a5060"
                                        }
                                    }
                                }

                                // Step name
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData ? (modelData.name || "") : ""
                                    font.pixelSize: 13
                                    color: {
                                        var st = (modelData && modelData.status) ? modelData.status : "pending"
                                        if (st === "completed") return "#8890a8"
                                        if (st === "active") return "#e8ecf8"
                                        return "#505870"
                                    }
                                    elide: Text.ElideRight
                                }

                                // Active step: show percentage + byte progress
                                ColumnLayout {
                                    visible: modelData && modelData.status === "active" && (modelData.bytesTotal || 0) > 0
                                    spacing: 2
                                    Layout.preferredWidth: 140
                                    Text {
                                        Layout.fillWidth: true
                                        text: (modelData && modelData.status === "active")
                                              ? String(modelData.percentage || 0) + "%"
                                              : ""
                                        font.pixelSize: 13; font.weight: Font.Bold; color: "#5d6fe0"
                                        horizontalAlignment: Text.AlignRight
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: (modelData && modelData.status === "active" && modelData.bytesTotal > 0)
                                              ? fmtSize(modelData.bytesReceived || 0) + " / " + fmtSize(modelData.bytesTotal || 0)
                                              : ""
                                        font.pixelSize: 9; color: "#607088"; font.family: "Consolas, monospace"
                                        horizontalAlignment: Text.AlignRight
                                    }
                                }
                                // Active step without byte data: just percentage
                                Text {
                                    visible: modelData && modelData.status === "active" && (modelData.bytesTotal || 0) === 0
                                    text: (modelData && modelData.status === "active")
                                          ? String(modelData.percentage || 0) + "%"
                                          : ""
                                    font.pixelSize: 13; font.weight: Font.Bold; color: "#5d6fe0"
                                }
                            }

                            // Progress bar for active step with byte data
                            Rectangle {
                                anchors.bottom: parent.bottom; anchors.left: parent.left
                                anchors.leftMargin: 44; anchors.right: parent.right
                                implicitHeight: 2; color: "#1e2230"
                                visible: modelData && modelData.status === "active" && (modelData.bytesTotal || 0) > 0
                                Rectangle {
                                    height: parent.height
                                    width: (modelData && modelData.bytesTotal > 0)
                                           ? parent.width * Math.min(1, (modelData.bytesReceived || 0) / modelData.bytesTotal)
                                           : 0
                                    color: "#3a5ecc"
                                    Behavior on width { NumberAnimation { duration: 200 } }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ── done state overlay ──
    Rectangle {
        anchors.fill: parent; color: "#0c0f16"
        visible: backend && backend.installPhase === "done" && !backend.installing
        ColumnLayout {
            anchors.centerIn: parent; spacing: 12
            Text { Layout.alignment: Qt.AlignHCenter; text: "\u2714"; font.pixelSize: 48; color: "#4bc870" }
            Text { Layout.alignment: Qt.AlignHCenter; text: "版本 " + (backend ? backend.installVersionId : "") + " 已就绪"; font.pixelSize: 15; font.weight: Font.Bold; color: "#4bc870" }
            Text {
                Layout.alignment: Qt.AlignHCenter; text: "安装完成，可在下载页面启动"
                font.pixelSize: 12; color: "#608868"
            }
        }
    }
}
