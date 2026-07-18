// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// ═══ 衣帽间 — 皮肤 & 披风管理 ═══
// 从左侧滑入，占满整个启动器窗口
// 布局: 左1/3 当前皮肤, 右2/3 披风选择
Item {
    id: root
    anchors.fill: parent
    z: 999

    property var backend: null
    property var account: null  // AccountBackend (set via backend.account)
    property var toastManager: null
    property var appWindow: null

    // loginMode mirrors HomePage: 0 = Microsoft (online), 1 = offline
    readonly property int loginMode: backend ? backend.lastLoginMode : -1
    readonly property bool loggedIn: {
        if (!account) return false
        if (loginMode === 0) return account.isOnline  // Microsoft: true only after successful login
        return account.offlineUsername !== ""         // Offline: has username set
    }
    readonly property bool isOnline: loginMode === 0 && account && account.isOnline
    readonly property string accountLabel: {
        if (!loggedIn) return qsTr("未登录")
        if (isOnline) return qsTr("正版用户：") + account.username
        return qsTr("离线用户：") + account.offlineUsername
    }
    readonly property string activeSkinPath: {
        if (!loggedIn) return ""
        if (isOnline) return account.skinPath || ""
        return account.offlineSkinPath || ""
    }
    readonly property string skinVariant: account ? (account.skinVariant || "CLASSIC") : "CLASSIC"

    signal closeRequested()

    // ─── Overlay backdrop (darkens main window) ───
    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: _visible ? 0.55 : 0
        Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
        MouseArea {
            anchors.fill: parent
            onClicked: close()
        }
    }

    // ─── Main panel: slides from left ───
    property bool _visible: false
    function open() { _visible = true }
    function close() { _visible = false; closeTimer.start() }

    Timer {
        id: closeTimer
        interval: 350
        onTriggered: root.closeRequested()
    }

    Rectangle {
        id: panel
        width: parent.width
        height: parent.height
        color: StyleTokens.bgPrimary
        x: _visible ? 0 : -parent.width
        Behavior on x { SmoothedAnimation { duration: 380; velocity: -1 } }

        // 拦截面板空白区域点击，防止穿透到 backdrop 触发关闭
        MouseArea {
            anchors.fill: parent
            z: -2
        }

        // ══════════════════════════════════════
        // 顶部栏
        // ══════════════════════════════════════
        Rectangle {
            id: topBar
            anchors { left: parent.left; right: parent.right; top: parent.top }
            height: 44
            color: StyleTokens.bgPrimary
            z: 10

            RowLayout {
                anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                spacing: 8

                // ── 返回按钮 ──
                Rectangle {
                    Layout.alignment: Qt.AlignVCenter
                    width: 32; height: 32; radius: StyleTokens.radiusSm
                    color: backHover.containsMouse ? "#1a2a40" : "#0a0c14"
                    Image {
                        anchors.centerIn: parent
                        source: "../icons/lucide/arrow-left.svg"
                        width: 16; height: 16
                        sourceSize: Qt.size(16, 16)
                        opacity: backHover.containsMouse ? 0.9 : 0.45
                    }
                    MouseArea {
                        id: backHover
                        anchors.fill: parent; hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.close()
                    }
                }

                // ── 账号标签 ──
                Text {
                    text: root.accountLabel
                    color: "#6880a0"
                    font.pixelSize: StyleTokens.fontSizeSm
                    font.family: "Microsoft YaHei"
                    Layout.alignment: Qt.AlignVCenter
                }

                Item { Layout.fillWidth: true }
            }

            // 窗口拖拽 (放在 RowLayout 后面，z:-1 避免挡住按钮)
            MouseArea {
                z: -1
                anchors.fill: parent
                property real lastX: 0; property real lastY: 0
                cursorShape: Qt.ArrowCursor
                onPressed: { lastX = mouseX; lastY = mouseY }
                onPositionChanged: {
                    var win = root.appWindow
                    if (win) { win.x += mouseX - lastX; win.y += mouseY - lastY }
                }
            }
        }

        // ══════════════════════════════════════
        // 主体内容
        // ══════════════════════════════════════
        RowLayout {
            anchors {
                top: topBar.bottom; left: parent.left
                right: parent.right; bottom: parent.bottom
            }
            spacing: 0

            // ── 左侧呼吸区域 ──
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 50
            }

            // ═══════════════════ 3D 皮肤预览 ═══════════════════
            ColumnLayout {
                Layout.preferredWidth: 200
                Layout.fillHeight: true
                Layout.topMargin: 24
                Layout.bottomMargin: 24
                spacing: 12

                // ── 当前皮肤 ──
                Text {
                    text: {
                        if (!root.loggedIn) return qsTr("当前皮肤")
                        if (root.isOnline) return qsTr("当前皮肤 · 正版")
                        return qsTr("当前皮肤 · 离线")
                    }
                    color: "#90a8c8"
                    font.pixelSize: StyleTokens.fontSizeLg
                    font.weight: Font.Bold
                    font.family: "Microsoft YaHei"
                }

                // ── 皮肤预览框 ──
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: StyleTokens.radiusLg
                    color: StyleTokens.surfaceOverlay
                    border.color: StyleTokens.bgHover
                    border.width: 1

                    // 3D 皮肤预览
                    SkinPreview3D {
                        anchors.fill: parent
                        anchors.margins: 4
                        skinSource: root.activeSkinPath
                        capeSource: account && account.capePath ? account.capePath : ""
                    }
                }

                // ── 上传按钮（仅正版） ──
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    radius: StyleTokens.radiusMd
                    color: root.isOnline && uploadHover.containsMouse ? "#2a3a50" : "#1a2a40"
                    border.color: root.isOnline && uploadHover.containsMouse ? "#4a6a90" : "#2a3a50"
                    opacity: root.isOnline ? 1.0 : 0.4
                    Row {
                        anchors.centerIn: parent
                        spacing: 8
                        Image {
                            anchors.verticalCenter: parent.verticalCenter
                            source: "../icons/lucide/download.svg"
                            rotation: 180
                            width: 14; height: 14
                            sourceSize: Qt.size(14, 14)
                        }
                        Text {
                            text: qsTr("上传")
                            color: "#c0d0e0"
                            font.pixelSize: StyleTokens.fontSizeMd
                            font.family: "Microsoft YaHei"
                        }
                    }

                    MouseArea {
                        id: uploadHover
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: root.isOnline ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: {
                            if (!root.isOnline) return
                            // TODO: call account.uploadSkin()
                            if (root.toastManager)
                                root.toastManager.show(qsTr("上传功能开发中"), "info")
                        }
                    }
                }
            }

            // ── 中间间隙 ──
            Item { Layout.preferredWidth: 60; Layout.fillHeight: true }

            // ═══════════════════ 披风选择（仅正版） ═══════════════════
            ColumnLayout {
                Layout.preferredWidth: 340
                Layout.fillHeight: true
                Layout.topMargin: 24
                Layout.bottomMargin: 24
                spacing: 12
                visible: root.isOnline

                // ── "选择你的披风" 标签 ──
                Text {
                    text: qsTr("选择你的披风")
                    color: "#90a8c8"
                    font.pixelSize: StyleTokens.fontSizeLg
                    font.weight: Font.Bold
                    font.family: "Microsoft YaHei"
                }

                // ── 披风列表（可滚动） ──
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: StyleTokens.radiusLg
                    color: StyleTokens.surfaceOverlay
                    border.color: StyleTokens.bgHover
                    border.width: 1
                    clip: true

                    ScrollView {
                        id: capeScroll
                        anchors.fill: parent
                        anchors.margins: 12
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                        GridLayout {
                            width: capeScroll.availableWidth
                            columns: 2
                            columnSpacing: 14
                            rowSpacing: 14

                            // ── "无披风" 永远是第一个 ──
                            CapeCard {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 160
                                capeAlias: ""
                                capeLabel: qsTr("无披风")
                                capePath: ""
                                isActive: !account || !account.capePath || account.capePath.length === 0
                                onSelected: {
                                    if (account) account.selectCape("")
                                }
                            }

                            // ── 动态披风列表 ──
                            Repeater {
                                model: root.account ? root.account.capes : []
                                delegate: CapeCard {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 160
                                    capeAlias: modelData.alias || ""
                                    capeLabel: modelData.alias || ""
                                    capePath: modelData.localPath || ""
                                    isActive: modelData.active === true
                                    onSelected: {
                                        if (root.account) root.account.selectCape(modelData.alias || "")
                                    }
                                }
                            }
                        }
                    }
                }

                // ── 提示文字 ──
                Text {
                    Layout.fillWidth: true
                    text: qsTr("注：您选择的披风/更换皮肤会实时生效。")
                    color: StyleTokens.textMuted
                    font.pixelSize: StyleTokens.fontSizeSm
                    font.family: "Microsoft YaHei"
                    horizontalAlignment: Text.AlignLeft
                }
            }

            // ── 右侧呼吸区域 ──
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 50
            }
        }
    }
}
