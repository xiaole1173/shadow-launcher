// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: root
    color: StyleTokens.bgPrimary
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
    Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeInDuration; easing.type: AnimationTokens.itemFadeInEasing } }

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
        width: 460; height: childrenRect.height + 40; radius: StyleTokens.radiusXl
        anchors.centerIn: parent
        color: "#121418"; border.color: StyleTokens.bgElevated

        ColumnLayout {
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
            anchors.margins: 24; spacing: 18

            // Header
            RowLayout {
                Layout.fillWidth: true
                Text { text: qsTr("安装配置"); font.pixelSize: StyleTokens.fontSizeXl; font.bold: true; color: StyleTokens.textPrimary }
                Item { Layout.fillWidth: true }
                Rectangle { width: 28; height: 28; radius: StyleTokens.radiusXl; color: closeMouse.containsMouse ? "#2a1a1a" : "transparent"
                    Text { anchors.centerIn: parent; text: "[失败]"; color: "#8890a0"; font.pixelSize: StyleTokens.fontSizeMd }
                    MouseArea { id: closeMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.hide() }
                }
            }

            // Version info card
            Rectangle { Layout.fillWidth: true; height: 48; radius: StyleTokens.radiusLg; color: "#1a1f2e"; border.color: StyleTokens.bgElevated
                RowLayout { anchors.fill: parent; anchors.margins: 14; spacing: 10
                    Text { text: root.versionId; font.pixelSize: StyleTokens.fontSizeLg; font.bold: true; color: StyleTokens.textPrimary }
                    Rectangle { radius: StyleTokens.radiusXs; height: 18; width: typeLabel.implicitWidth + 10; color: StyleTokens.accentSubtle
                        Text { id: typeLabel; anchors.centerIn: parent; text: root.versionType; color: "#8090c0"; font.pixelSize: StyleTokens.fontSizeXs }
                    }
                }
            }

            // ── Download source ──
            ColumnLayout { spacing: 8
                Text { text: qsTr("下载源"); font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.DemiBold; color: "#b8c0d0" }
                RowLayout { spacing: 6
                    Repeater {
                        model: [
                            { key: -1, label: "多源并行（推荐）", desc: "BMCLAPI+官方并行下载" },
                            { key: 0, label: "BMCLAPI", desc: "国内镜像，速度快" },
                            { key: 1, label: "Mojang 官方", desc: "需要良好网络" }
                        ]
                        Rectangle {
                            width: Math.min(srcBtn.implicitWidth + 20, 160); height: 36; radius: StyleTokens.radiusMd
                            color: root.sourceIndex === modelData.key ? "#5068d8" : "#1a1f2e"
                            border.color: root.sourceIndex === modelData.key ? "#5d6fe0" : "#252835"
                            clip: true
                            ColumnLayout {
                                anchors.centerIn: parent; spacing: 0
                                Text { id: srcBtn; text: modelData.label; color: root.sourceIndex === modelData.key ? "#ffffff" : "#b8c0d0"; font.pixelSize: StyleTokens.fontSizeSm; font.weight: root.sourceIndex === modelData.key ? Font.DemiBold : Font.Normal }
                            }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.sourceIndex = modelData.key }
                        }
                    }
                }
            }

            // ── Optional addons ──
            ColumnLayout { spacing: 8
                Text { text: qsTr("附加组件"); font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.DemiBold; color: "#b8c0d0" }
                Text { text: qsTr("Fabric / OptiFine / Forge 自动安装功能即将支持"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textMuted }
                RowLayout { spacing: 8; opacity: 0.4
                    Rectangle { width: 100; height: 34; radius: StyleTokens.radiusMd; color: "#1a1f2e"; border.color: StyleTokens.bgElevated
                        Text { anchors.centerIn: parent; text: "Fabric"; color: "#505468"; font.pixelSize: StyleTokens.fontSizeSm }
                        MouseArea { anchors.fill: parent; onClicked: {} }
                    }
                    Rectangle { width: 100; height: 34; radius: StyleTokens.radiusMd; color: "#1a1f2e"; border.color: StyleTokens.bgElevated
                        Text { anchors.centerIn: parent; text: "OptiFine"; color: "#505468"; font.pixelSize: StyleTokens.fontSizeSm }
                        MouseArea { anchors.fill: parent; onClicked: {} }
                    }
                    Rectangle { width: 100; height: 34; radius: StyleTokens.radiusMd; color: "#1a1f2e"; border.color: StyleTokens.bgElevated
                        Text { anchors.centerIn: parent; text: "Forge"; color: "#505468"; font.pixelSize: StyleTokens.fontSizeSm }
                        MouseArea { anchors.fill: parent; onClicked: {} }
                    }
                }
            }

            // ── Action buttons ──
            RowLayout { spacing: 10; Layout.alignment: Qt.AlignRight
                Rectangle { width: 80; height: 34; radius: StyleTokens.radiusMd; color: cancelMouse.containsMouse ? "#1a1f2e" : "transparent"
                    scale: cancelMouse.pressed ? 0.9 : 1.0
                    Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }
                    border.color: cancelMouse.containsMouse ? "#5d6fe0" : "#3a4050"; border.width: 1
                    Text { anchors.centerIn: parent; text: qsTr("取消"); color: cancelMouse.containsMouse ? "#ffffff" : "#8890a0"; font.pixelSize: StyleTokens.fontSizeMd }
                    MouseArea { id: cancelMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.hide() }
                }
                Rectangle { width: 120; height: 36; radius: StyleTokens.radiusMd; color: installMouse.containsMouse ? "#6d7de8" : "#5068d8"
                    scale: installMouse.pressed ? 0.92 : 1.0
                    Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }
                    Text { anchors.centerIn: parent; text: qsTr("开始安装"); font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.DemiBold; color: StyleTokens.textInverse }
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
