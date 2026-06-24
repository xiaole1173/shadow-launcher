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
    onBackendChanged: {
        if (backend && backend.installCardsModel)
            cardsView.model = backend.installCardsModel
    }
    property var toastManager: null

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
        anchors.fill: parent; color: "#0c0f16"
        visible: backend.installCardsModel.count === 0
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
        spacing: 12; clip: true

        delegate: Rectangle {
            width: cardsView.width; implicitHeight: cardContent.implicitHeight + 20
            radius: 10; color: "#11141c"
            border.color: "#1e2230"; border.width: 1

            ColumnLayout {
                id: cardContent
                anchors.fill: parent; anchors.margins: 14; spacing: 8
                property var card: modelData || {}
                property var _parsedSteps: model.steps || []  // QVariantList → already JS array, no JSON.parse needed
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
                        font.pixelSize: 14; font.weight: Font.Bold
                        color: model.failed ? "#c06050" : "#e8ecf8"
                        elide: Text.ElideRight
                    }
                    // Speed
                    Text {
                        visible: (model.speed || 0) > 0
                        text: fmtSpeed(model.speed || 0)
                        font.pixelSize: 11; color: "#5d6fe0"; font.family: "Consolas, monospace"
                    }
                }

                // ── progress bar ──
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: 6; radius: 3; color: "#1e2230"
                    visible: model.totalProgressVisible !== undefined ? model.totalProgressVisible : true
                    Rectangle {
                        height: parent.height; radius: 3
                        color: model.failed ? "#802020" : "#3a5ecc"
                        width: parent.width * Math.min(1, (model.progress || 0))
                        Behavior on width { NumberAnimation { duration: 1200; easing.type: Easing.OutCubic } }
                    }
                }

                // ── progress text ──
                RowLayout { Layout.fillWidth: true
                    Text {
                        Layout.fillWidth: true
                        text: model.failed ? ("\u2717 " + (model.error || "\u5b89\u88c5\u5931\u8d25")) : ((model.progress || 0) * 100).toFixed(1) + "%"
                        font.pixelSize: 13; font.weight: Font.Bold
                        color: model.failed ? "#e06050" : "#5d6fe0"
                    }
                    Text {
                        Layout.fillWidth: true
                        text: model.phase || ""
                        font.pixelSize: 11; color: "#606888"
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

                                    // Completed: checkmark
                                    Text {
                                        visible: modelData && modelData.status === "completed"
                                        anchors.centerIn: parent; font.pixelSize: 11
                                        text: "✔"; color: "#4bc870"
                                    }
                                    // Failed: cross
                                    Text {
                                        visible: modelData && modelData.status === "failed"
                                        anchors.centerIn: parent; font.pixelSize: 11
                                        text: "✘"; color: "#e06060"
                                    }
                                    // Active: rotating arc (pure QML, shared Timer driven)
                                    Item {
                                        visible: modelData && modelData.status === "active"
                                        anchors.centerIn: parent; width: 16; height: 16
                                        rotation: cardContent._spinAngle
                                        // 270-degree arc: full circle + mask to hide 90-degree sector
                                        Rectangle {
                                            anchors.centerIn: parent; width: 14; height: 14; radius: 7
                                            color: "transparent"
                                            border.width: 2; border.color: "#5d6fe0"
                                        }
                                        // Mask covers top-right quadrant to create arc gap
                                        Rectangle {
                                            width: 8; height: 16
                                            anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                                            color: "#1a1a3a"  // matches active status dot background
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
                                    visible: modelData && modelData.status === "active"
                                    text: (modelData && modelData.status === "active") ? (modelData.percentage || 0) + "%" : ""
                                    font.pixelSize: 12; font.weight: Font.Bold; color: "#5d6fe0"
                                }
                            }
                        }
                    }
                }

                // ── Control buttons ──
                RowLayout {
                    visible: {
                        var ph = model.phase || ""
                        return !model.failed && ph !== "" && ph !== "idle"
                            && ph !== "完成" && ph !== "空闲"
                    }
                    spacing: 6; Layout.fillWidth: true; Layout.alignment: Qt.AlignRight

                    Item { Layout.fillWidth: true }  // spacer

                    // Cancel — greyed out during SHA1 verify (instant, can't interrupt)
                    property bool verifying: {
                        var ph = model.phase || ""
                        return ph.includes("校验")
                    }
                    Rectangle {
                        implicitWidth: cancelBtnText.implicitWidth + 20; implicitHeight: 28
                        radius: 6
                        color: parent.verifying ? "#141720" : "#191c28"
                        border.color: parent.verifying ? "#1e2230" : "#2a3040"
                        border.width: 1
                        opacity: parent.verifying ? 0.4 : 1.0
                        Text {
                            id: cancelBtnText
                            anchors.centerIn: parent
                            text: parent.verifying ? "校验中..." : "✕ 取消"
                            font.pixelSize: 12
                            color: parent.verifying ? "#404860" : "#c06050"
                        }
                        MouseArea {
                            anchors.fill: parent
                            enabled: !parent.verifying
                            onClicked: {
                                if (!backend) return
                                // Cancel BOTH: VersionDownloader if active, ModLoaderInstaller if running
                                var iid = model.iid || ""
                                backend.cancelModLoaderInstall()
                                if (iid) backend.cancelInstall(iid)
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
