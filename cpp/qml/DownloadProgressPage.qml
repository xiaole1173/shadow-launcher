import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

// DownloadProgressPage — multi-card install progress view
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

    function fmtSpeed(speedBytes) {
        return fmtSize(speedBytes) + "/s"
    }

    // ── empty state ──
    Rectangle {
        anchors.fill: parent; color: "#0c0f16"
        visible: !cardsView.count
        ColumnLayout {
            anchors.centerIn: parent; spacing: 12
            Text { Layout.alignment: Qt.AlignHCenter; text: "\u2501\u2501"; font.pixelSize: 32; color: "#1e2230" }
            Text { Layout.alignment: Qt.AlignHCenter; text: "当前没有进行中的任务"; font.pixelSize: 14; color: "#505870" }
            Text { Layout.alignment: Qt.AlignHCenter; text: "从下载页或安装页启动安装后，进度将在此显示"; font.pixelSize: 11; color: "#3a4058" }
        }
    }

    // ── card list ──
    ListView {
        id: cardsView
        anchors.fill: parent; anchors.margins: 16
        model: {
            var ai = backend ? (backend.activeInstalls || []) : []
            console.log("[progress-ui] activeInstalls model has", ai.length, "cards")
            return ai
        }
        spacing: 12; clip: true

        delegate: Rectangle {
            width: cardsView.width; implicitHeight: cardContent.implicitHeight + 20
            radius: 10; color: "#11141c"
            border.color: "#1e2230"; border.width: 1

            ColumnLayout {
                id: cardContent
                anchors.fill: parent; anchors.margins: 14; spacing: 8
                property var card: modelData || {}

                // ── card header ──
                RowLayout { spacing: 8; Layout.fillWidth: true
                    // Type badge
                    Rectangle {
                        implicitWidth: typeText.implicitWidth + 12; implicitHeight: 22; radius: 4
                        color: {
                            var t = cardContent.card.installType || ""
                            if (t === "mod_loader") return "#1a1a3a"
                            if (t === "version") return "#1a2a1a"
                            if (t === "resource") return "#1a2a2a"
                            return "#1a1a20"
                        }
                        Text {
                            id: typeText; anchors.centerIn: parent; font.pixelSize: 10
                            text: {
                                var t = cardContent.card.installType || ""
                                if (t === "mod_loader") return "加载器"
                                if (t === "version") return "版本"
                                if (t === "resource") return "资源"
                                return "下载"
                            }
                            color: {
                                var t = cardContent.card.installType || ""
                                if (t === "mod_loader") return "#5d6fe0"
                                if (t === "version") return "#4bc870"
                                if (t === "resource") return "#40c8c8"
                                return "#888"
                            }
                        }
                    }
                    // Display name
                    Text {
                        Layout.fillWidth: true
                        text: cardContent.card.displayName || "--"
                        font.pixelSize: 14; font.weight: Font.Bold
                        color: cardContent.card.installFailed ? "#c06050" : "#e8ecf8"
                        elide: Text.ElideRight
                    }
                    // Speed
                    Text {
                        visible: (cardContent.card.speed || 0) > 0
                        text: fmtSpeed(cardContent.card.speed || 0)
                        font.pixelSize: 11; color: "#5d6fe0"; font.family: "Consolas, monospace"
                    }
                }

                // ── progress bar ──
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: 6; radius: 3; color: "#1e2230"
                    Rectangle {
                        height: parent.height; radius: 3
                        color: cardContent.card.installFailed ? "#802020" : "#3a5ecc"
                        width: parent.width * Math.min(1, (cardContent.card.totalProgress || 0))
                        Behavior on width { NumberAnimation { duration: 200 } }
                    }
                }

                // ── progress text ──
                RowLayout { Layout.fillWidth: true
                    Text {
                        Layout.fillWidth: true
                        text: cardContent.card.installFailed ? ("\u2717 " + (cardContent.card.installError || "\u5b89\u88c5\u5931\u8d25")) : ((cardContent.card.totalProgress || 0) * 100).toFixed(1) + "%"
                        font.pixelSize: 13; font.weight: Font.Bold
                        color: cardContent.card.installFailed ? "#e06050" : "#5d6fe0"
                    }
                    Text {
                        Layout.fillWidth: true
                        text: cardContent.card.installPhase || ""
                        font.pixelSize: 11; color: "#606888"
                        elide: Text.ElideRight
                    }
                    Text {
                        visible: (cardContent.card.remainingSteps || 0) > 0
                        text: "剩余 " + (cardContent.card.remainingSteps || 0) + " 步"
                        font.pixelSize: 11; color: "#c8a040"
                    }
                }

                // ── step list (mod_loader only) ──
                ColumnLayout {
                    visible: cardContent.card.installType === "mod_loader" && cardContent.card.steps
                             && (cardContent.card.steps.length || 0) > 0
                    spacing: 4; Layout.fillWidth: true

                    Repeater {
                        model: cardContent.card.steps || []
                        delegate: Rectangle {
                            visible: (modelData && modelData.show !== undefined) ? modelData.show : true
                            Layout.fillWidth: true; implicitHeight: 28; color: "transparent"
                            RowLayout {
                                anchors.fill: parent; anchors.margins: 4; spacing: 8
                                // Status dot
                                Rectangle {
                                    implicitWidth: 20; implicitHeight: 20; radius: 10
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
                                    Text {
                                        anchors.centerIn: parent; font.pixelSize: 10
                                        text: {
                                            var s = (modelData && modelData.status) ? modelData.status : "pending"
                                            if (s === "completed") return "\u2714"
                                            if (s === "active") return "\u22EF"
                                            if (s === "failed") return "\u2718"
                                            return ""
                                        }
                                        color: {
                                            var s = (modelData && modelData.status) ? modelData.status : "pending"
                                            if (s === "completed") return "#4bc870"
                                            if (s === "active") return "#5d6fe0"
                                            if (s === "failed") return "#e06060"
                                            return "#4a5060"
                                        }
                                    }
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData ? (modelData.name || "") : ""
                                    font.pixelSize: 12
                                    color: {
                                        var s = (modelData && modelData.status) ? modelData.status : "pending"
                                        if (s === "completed") return "#707888"
                                        if (s === "active") return "#c8d0e8"
                                        return "#4a5060"
                                    }
                                    elide: Text.ElideRight
                                }
                                Text {
                                    visible: modelData && modelData.status === "active" && (modelData.percentage || 0) > 0
                                    text: (modelData && modelData.status === "active") ? (modelData.percentage || 0) + "%" : ""
                                    font.pixelSize: 12; font.weight: Font.Bold; color: "#5d6fe0"
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ── done state (all cards gone, install finished) ──
    Rectangle {
        anchors.fill: parent; color: "#0c0f16"
        visible: backend && backend.installPhase === "done" && !cardsView.count
        ColumnLayout {
            anchors.centerIn: parent; spacing: 12
            Text { Layout.alignment: Qt.AlignHCenter; text: "\u2714"; font.pixelSize: 48; color: "#4bc870" }
            Text { Layout.alignment: Qt.AlignHCenter; text: "安装完成"; font.pixelSize: 15; font.weight: Font.Bold; color: "#4bc870" }
            Text { Layout.alignment: Qt.AlignHCenter; text: "可在下载页面启动"; font.pixelSize: 12; color: "#608868" }
        }
    }
}
