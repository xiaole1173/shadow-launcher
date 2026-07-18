// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    anchors.fill: parent
    color: "transparent"

    property var mp: backend ? backend.multiplayer : null
    property var toastManager: null

    opacity: 0
    Component.onCompleted: root.opacity = 1
    Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }

    Connections {
        target: mp
        function onErrorOccurred(msg) {
            if (root.toastManager && msg)
                root.toastManager.show(msg)
        }
    }

    Flickable {
        id: pageFlick
        anchors.fill: parent
        contentWidth: width
        contentHeight: contentCol.implicitHeight + 80
        clip: true
        interactive: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: contentCol
            width: pageFlick.width - 80
            x: 40
            spacing: 20

            // ── Header ──
            Text {
                text: "多人联机"
                font.pixelSize: 28; font.bold: true; color: StyleTokens.textSecondary
                Layout.topMargin: 40
            }

            // ── Info card ──
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: infoLabel.implicitHeight + 36
                color: StyleTokens.bgPrimary
                border.color: "#2a2a4a"; border.width: 1
                radius: StyleTokens.radiusLg

                Text {
                    id: infoLabel
                    anchors.left: parent.left; anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: 18
                    text: "欢迎您参与联机功能内测！\n如果你是房主：点击创建房间→复制房间号发给你的好友。\n如果你是加入者：点击加入房间→粘贴房间号→点击加入。\n考虑到正版验证可能干扰局域网联机，我们建议房主使用离线登录。\n（功能测试中）目前启动器仅支持对1.16+的版本关闭正版验证，旧版本无法关闭。\n注意：联机功能还处于内测，不稳定时有发生；如遭遇攻击掉线还请理解！"
                    font.pixelSize: StyleTokens.fontSizeMd; color: StyleTokens.textTertiary
                    lineHeight: 1.5
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignLeft
                }
            }

            // ── State display ──
            Rectangle {
                Layout.fillWidth: true; Layout.preferredHeight: 56
                color: "transparent"
                border.color: "#2a2a4a"; border.width: 1
                radius: StyleTokens.radiusLg
                visible: mp ? mp.state !== 0 : false
                opacity: visible ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }

                RowLayout {
                    anchors.fill: parent; anchors.margins: 16; spacing: 12
                    Rectangle {
                        width: 10; height: 10; radius: StyleTokens.radiusSm
                        color: mp && (mp.state === 5 || mp.state === 6) ? "#4ade80" :
                               mp && mp.state === 7 ? "#ef4444" : "#f59e0b"
                    }
                    Text {
                        Layout.fillWidth: true
                        text: mp ? mp.stateText : ""
                        font.pixelSize: StyleTokens.fontSizeMd; color: StyleTokens.textTertiary
                    }
                }
            }

            // ── Room code card (host only) ──
            Rectangle {
                Layout.fillWidth: true; Layout.preferredHeight: 64
                color: "transparent"
                border.color: "#2a2a4a"; border.width: 1
                radius: StyleTokens.radiusLg
                visible: mp ? (mp.role === 1 && mp.roomCode !== "") : false
                opacity: visible ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }

                RowLayout {
                    anchors.fill: parent; anchors.margins: 16; spacing: 12
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 2
                        Text { text: "房间码"; font.pixelSize: StyleTokens.fontSizeSm; color: "#666" }
                        Text {
                            text: mp ? mp.roomCode : ""
                            font.pixelSize: StyleTokens.fontSizeXl; font.bold: true
                            color: "#7ec8e3"
                            font.family: "Consolas, monospace"
                        }
                    }
                    Rectangle {
                        Layout.preferredWidth: 80; Layout.preferredHeight: 34; radius: StyleTokens.radiusMd
                        color: "transparent"; border.color: "#3a4eb8"; border.width: 1.5
                        scale: copyBtnHover.pressed ? 0.95 : 1
                        Behavior on scale { NumberAnimation { duration: 100; easing.type: Easing.OutBack } }
                        Text { anchors.centerIn: parent; text: "复制"; font.pixelSize: StyleTokens.fontSizeMd; color: StyleTokens.accentLight }
                        MouseArea {
                            id: copyBtnHover
                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (mp) {
                                    mp.copyRoomCode()
                                    if (root.toastManager)
                                        root.toastManager.show("房间码已复制到剪贴板")
                                }
                            }
                        }
                    }
                }
            }

            // ── Player list ──
            Rectangle {
                id: playerListCard
                Layout.fillWidth: true
                Layout.preferredHeight: playerCol.implicitHeight + 44
                color: StyleTokens.bgPrimary
                border.color: "#2a2a4a"; border.width: 1
                radius: StyleTokens.radiusLg
                visible: mp ? (mp.players && mp.players.length > 0) : false
                opacity: visible ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
                Behavior on Layout.preferredHeight { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

                ColumnLayout {
                    id: playerCol
                    width: parent.width - 24
                    x: 12; y: 12
                    spacing: 6

                    Text {
                        text: mp ? "在线成员 (" + mp.players.length + ")" : "在线成员"
                        font.pixelSize: StyleTokens.fontSizeMd; color: "#888"
                        Layout.leftMargin: 4
                    }

                    Repeater {
                        model: mp ? mp.players : []

                        Rectangle {
                            id: playerItem
                            width: playerCol.width
                            height: 48
                            color: StyleTokens.surfaceOverlay
                            radius: StyleTokens.radiusLg
                            border.color: StyleTokens.accentSubtle
                            border.width: 1

                            // ── Entrance animation ──
                            opacity: 0
                            scale: 0.9
                            Component.onCompleted: { playerItem.opacity = 1; playerItem.scale = 1 }
                            Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
                            Behavior on scale { NumberAnimation { duration: 350; easing.type: Easing.OutBack } }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12; anchors.rightMargin: 12
                                spacing: 10

                                // Computer name
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.hostname || modelData.machine_id || "Unknown"
                                    color: StyleTokens.textSecondary
                                    font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.Medium
                                    elide: Text.ElideRight
                                }

                                // IP address
                                Text {
                                    text: modelData.ip || "—"
                                    color: StyleTokens.textTertiary
                                    font.pixelSize: StyleTokens.fontSizeSm
                                    font.family: "Consolas, monospace"
                                    Layout.preferredWidth: ipText.implicitWidth
                                }
                                Text { id: ipText; visible: false; text: modelData.ip || "—"; font.pixelSize: StyleTokens.fontSizeSm; font.family: "Consolas, monospace" }

                                // Latency
                                Rectangle {
                                    visible: modelData.latency !== undefined && modelData.latency >= 0
                                    Layout.preferredWidth: latText.implicitWidth + 16
                                    Layout.preferredHeight: 22
                                    radius: StyleTokens.radiusMd
                                    color: {
                                        var lat = modelData.latency || 0
                                        if (lat < 50) return "#2060c060"
                                        if (lat < 150) return "#20f59e0b"
                                        return "#20ef4444"
                                    }

                                    Text {
                                        id: latText
                                        anchors.centerIn: parent
                                        text: (modelData.latency || 0) + "ms"
                                        font.pixelSize: StyleTokens.fontSizeSm
                                        color: {
                                            var lat = modelData.latency || 0
                                            if (lat < 50) return "#60c060"
                                            if (lat < 150) return "#f59e0b"
                                            return "#ef4444"
                                        }
                                    }
                                }

                                // Role badge
                                Rectangle {
                                    Layout.preferredWidth: roleLabel.implicitWidth + 16
                                    Layout.preferredHeight: 22
                                    radius: StyleTokens.radiusMd
                                    color: modelData.kind === "HOST" ? "#20f59e0b" : "#204ade80"

                                    Text {
                                        id: roleLabel
                                        anchors.centerIn: parent
                                        text: modelData.kind === "HOST" ? "房主" : "玩家"
                                        font.pixelSize: StyleTokens.fontSizeSm
                                        color: modelData.kind === "HOST" ? "#f59e0b" : "#4ade80"
                                    }
                                }

                                // Status dot
                                Rectangle {
                                    width: 10; height: 10; radius: StyleTokens.radiusSm
                                    color: modelData.kind === "HOST" ? "#f59e0b" : "#4ade80"
                                }
                            }
                        }
                    }
                }
            }

            // ── Idle: buttons ──
            RowLayout {
                Layout.alignment: Qt.AlignHCenter; spacing: 12
                visible: mp ? mp.state === 0 : true
                opacity: visible ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }

                Rectangle {
                    Layout.preferredWidth: 200; Layout.preferredHeight: 42; radius: StyleTokens.radiusLg
                    color: "#3a4eb8"
                    scale: createClick.pressed ? 0.95 : 1
                    Behavior on scale { NumberAnimation { duration: 100; easing.type: Easing.OutBack } }
                    Text {
                        anchors.centerIn: parent
                        text: "创建房间"; font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.Bold; color: StyleTokens.textPrimary
                    }
                    MouseArea {
                        id: createClick
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: { if (mp) mp.createRoom() }
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 200; Layout.preferredHeight: 42; radius: StyleTokens.radiusLg
                    color: "#3a4eb8"
                    scale: joinClick.pressed ? 0.95 : 1
                    Behavior on scale { NumberAnimation { duration: 100; easing.type: Easing.OutBack } }
                    Text {
                        anchors.centerIn: parent
                        text: "加入房间"; font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.Bold; color: StyleTokens.textPrimary
                    }
                    MouseArea {
                        id: joinClick
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: joinDialog.open()
                    }
                }
            }

            // ── Disconnect button ──
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                visible: mp ? mp.state > 0 : false
                opacity: visible ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }

                Rectangle {
                    Layout.preferredWidth: 200; Layout.preferredHeight: 42; radius: StyleTokens.radiusLg
                    color: "transparent"; border.color: "#ef4444"; border.width: 1.5
                    scale: dcPress.pressed ? 0.95 : 1
                    Behavior on scale { NumberAnimation { duration: 100; easing.type: Easing.OutBack } }

                    Rectangle {
                        anchors.fill: parent; radius: StyleTokens.radiusLg
                        color: StyleTokens.error
                        opacity: dcPress.containsMouse ? 1 : 0
                        Behavior on opacity { NumberAnimation { duration: 200 } }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: mp && mp.state <= 3 ? "取消" : "断开连接"
                        font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.Bold; color: StyleTokens.error
                    }
                    MouseArea {
                        id: dcPress
                        anchors.fill: parent; hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: { if (mp) mp.leaveRoom() }
                    }
                }
            }

            // Bottom padding
            Item { Layout.preferredHeight: 20 }
        }
    }

    // ── Join Room Dialog ──
    Popup {
        id: joinDialog
        anchors.centerIn: parent; width: 400; height: 260
        modal: true; closePolicy: Popup.CloseOnEscape
        background: Rectangle { color: "#1a1a2e"; radius: StyleTokens.radiusXl; border.color: "#333"; border.width: 1 }

        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 200; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.95; to: 1; duration: 200; easing.type: Easing.OutCubic }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 150; easing.type: Easing.InCubic }
        }

        ColumnLayout {
            anchors.fill: parent; anchors.margins: 24; spacing: 16
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "加入联机房间"; font.pixelSize: StyleTokens.fontSizeXl; font.bold: true; color: "#7ec8e3"
            }
            Rectangle {
                Layout.preferredWidth: parent.width; Layout.preferredHeight: 44
                color: "#0d0d1a"; radius: StyleTokens.radiusLg
                border.color: "#2a2a4a"; border.width: 1
                TextInput {
                    id: joinInput
                    anchors.fill: parent; anchors.margins: 12
                    verticalAlignment: TextInput.AlignVCenter
                    font.pixelSize: StyleTokens.fontSizeLg; font.family: "Consolas, monospace"
                    color: "#e0e0e0"; clip: true
                    Text {
                        anchors.centerIn: parent
                        text: "粘贴房间码 (U/XXXX-...)"
                        color: "#555"; font.pixelSize: StyleTokens.fontSizeLg
                        font.family: "Consolas, monospace"
                        visible: !joinInput.text
                    }
                }
            }
            RowLayout {
                Layout.alignment: Qt.AlignHCenter; spacing: 12
                Rectangle {
                    Layout.preferredWidth: 150; Layout.preferredHeight: 44; radius: StyleTokens.radiusLg
                    color: "transparent"; border.color: "#555"; border.width: 1.5
                    Text {
                        anchors.centerIn: parent
                        text: "返回"; font.pixelSize: StyleTokens.fontSizeLg; font.weight: Font.Bold; color: StyleTokens.textTertiary
                    }
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: joinDialog.close()
                    }
                }
                Rectangle {
                    Layout.preferredWidth: 150; Layout.preferredHeight: 44; radius: StyleTokens.radiusLg
                    color: "#3a4eb8"
                    Text {
                        anchors.centerIn: parent
                        text: "加入"; font.pixelSize: StyleTokens.fontSizeLg; font.weight: Font.Bold; color: StyleTokens.textPrimary
                    }
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (joinInput.text && mp) {
                                joinDialog.close()
                                mp.joinRoom(joinInput.text.trim())
                                joinInput.text = ""
                            }
                        }
                    }
                }
            }
        }
    }
}
