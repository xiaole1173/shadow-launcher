import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#dd0c0f16"
    visible: false

    property var backend: null
    property string versionId: ""
    property string versionType: "release"
    property int sourceIndex: -1
    property bool installOptifine: false
    property bool installFabric: false
    property bool installForge: false

    signal dismissed()
    signal startInstall(string versionId, int sourceIndex)

    opacity: 0
    Behavior on opacity { NumberAnimation { duration: 250 } }

    function show(vid, vtype) {
        versionId = vid
        versionType = vtype
        sourceIndex = -1  // default multi-source
        installOptifine = false
        installFabric = false
        installForge = false
        visible = true
        opacity = 1
    }

    function hide() {
        opacity = 0
        dismissTimer.start()
    }

    Timer { id: dismissTimer; interval: 260; onTriggered: { visible = false; root.dismissed() } }

    // Mouse-blocking background
    MouseArea { anchors.fill: parent; hoverEnabled: true }

    // ── Centered card ──
    Rectangle {
        width: 460; height: childrenRect.height + 40; radius: 12
        anchors.centerIn: parent
        color: "#121418"; border.color: "#1e2230"

        ColumnLayout {
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
            anchors.margins: 24; spacing: 18

            // Header
            RowLayout {
                Layout.fillWidth: true
                Text { text: "安装配置"; font.pixelSize: 18; font.bold: true; color: "#e8ecf8" }
                Item { Layout.fillWidth: true }
                Rectangle { width: 28; height: 28; radius: 14; color: closeMouse.containsMouse ? "#2a1a1a" : "transparent"
                    Text { anchors.centerIn: parent; text: "✕"; color: "#8890a0"; font.pixelSize: 13 }
                    MouseArea { id: closeMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.hide() }
                }
            }

            // Version info card
            Rectangle { Layout.fillWidth: true; height: 48; radius: 8; color: "#1a1f2e"; border.color: "#252835"
                RowLayout { anchors.fill: parent; anchors.margins: 14; spacing: 10
                    Text { text: root.versionId; font.pixelSize: 15; font.bold: true; color: "#e8ecf8" }
                    Rectangle { radius: 3; height: 18; width: typeLabel.implicitWidth + 10; color: "#1f2740"
                        Text { id: typeLabel; anchors.centerIn: parent; text: root.versionType; color: "#8090c0"; font.pixelSize: 10 }
                    }
                }
            }

            // ── Download source ──
            ColumnLayout { spacing: 8
                Text { text: "下载源"; font.pixelSize: 13; font.weight: Font.DemiBold; color: "#b8c0d0" }
                RowLayout { spacing: 6
                    Repeater {
                        model: [
                            { key: -1, label: "多源并行（推荐）", desc: "BMCLAPI+官方并行下载" },
                            { key: 0, label: "BMCLAPI", desc: "国内镜像，速度快" },
                            { key: 1, label: "Mojang 官方", desc: "需要良好网络" }
                        ]
                        Rectangle {
                            width: Math.min(srcBtn.implicitWidth + 20, 160); height: 36; radius: 7
                            color: root.sourceIndex === modelData.key ? "#5068d8" : "#1a1f2e"
                            border.color: root.sourceIndex === modelData.key ? "#5d6fe0" : "#252835"
                            clip: true
                            ColumnLayout {
                                anchors.centerIn: parent; spacing: 0
                                Text { id: srcBtn; text: modelData.label; color: root.sourceIndex === modelData.key ? "#ffffff" : "#b8c0d0"; font.pixelSize: 11; font.weight: root.sourceIndex === modelData.key ? Font.DemiBold : Font.Normal }
                            }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.sourceIndex = modelData.key }
                        }
                    }
                }
            }

            // ── Optional addons ──
            ColumnLayout { spacing: 8
                Text { text: "附加组件"; font.pixelSize: 13; font.weight: Font.DemiBold; color: "#b8c0d0" }
                Text { text: "Fabric / OptiFine / Forge 自动安装功能即将支持"; font.pixelSize: 11; color: "#505468" }
                RowLayout { spacing: 8; opacity: 0.4
                    Rectangle { width: 100; height: 34; radius: 6; color: "#1a1f2e"; border.color: "#252835"
                        Text { anchors.centerIn: parent; text: "Fabric"; color: "#505468"; font.pixelSize: 12 }
                        MouseArea { anchors.fill: parent; onClicked: {} }
                    }
                    Rectangle { width: 100; height: 34; radius: 6; color: "#1a1f2e"; border.color: "#252835"
                        Text { anchors.centerIn: parent; text: "OptiFine"; color: "#505468"; font.pixelSize: 12 }
                        MouseArea { anchors.fill: parent; onClicked: {} }
                    }
                    Rectangle { width: 100; height: 34; radius: 6; color: "#1a1f2e"; border.color: "#252835"
                        Text { anchors.centerIn: parent; text: "Forge"; color: "#505468"; font.pixelSize: 12 }
                        MouseArea { anchors.fill: parent; onClicked: {} }
                    }
                }
            }

            // ── Action buttons ──
            RowLayout { spacing: 10; Layout.alignment: Qt.AlignRight
                Rectangle { width: 80; height: 34; radius: 7; color: cancelMouse.containsMouse ? "#1a1f2e" : "transparent"
                    scale: cancelMouse.pressed ? 0.9 : 1.0
                    Behavior on scale { NumberAnimation { duration: 100; easing.type: Easing.OutCubic } }
                    border.color: cancelMouse.containsMouse ? "#5d6fe0" : "#3a4050"; border.width: 1
                    Text { anchors.centerIn: parent; text: "取消"; color: cancelMouse.containsMouse ? "#ffffff" : "#8890a0"; font.pixelSize: 13 }
                    MouseArea { id: cancelMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.hide() }
                }
                Rectangle { width: 120; height: 36; radius: 7; color: installMouse.containsMouse ? "#6d7de8" : "#5068d8"
                    scale: installMouse.pressed ? 0.92 : 1.0
                    Behavior on scale { NumberAnimation { duration: 100; easing.type: Easing.OutCubic } }
                    Text { anchors.centerIn: parent; text: "开始安装"; font.pixelSize: 13; font.weight: Font.DemiBold; color: "#ffffff" }
                    MouseArea { id: installMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (!root.versionId) {
                                console.warn("[InstallConfig] versionId is empty, cannot install")
                                return
                            }
                            root.hide()
                            if (backend) {
                                console.log("[InstallConfig] Calling installVersion: version=" + root.versionId + " source=" + root.sourceIndex)
                                backend.installVersion(root.versionId, root.sourceIndex)
                            }
                        }
                    }
                }
            }
        }
    }
}
