// SPDX-License-Identifier: AGPL-3.0-or-later
// 联机主页面 — 集成所有联机组件的全功能页面
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
        function onMinecraftPortReady(port) {
            if (root.toastManager)
                root.toastManager.show("MC 端口已就绪: " + port)
        }
        function onScanResultsChanged() {
            // Scan results available — could show in page if needed
        }
        function onConnectionDifficultyChanged() {
            // Handled via binding on connectionDifficulty property
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
            spacing: 16

            // ── Page header ──
            Text {
                text: "多人联机"
                font.pixelSize: 28; font.bold: true; color: StyleTokens.textSecondary
                Layout.topMargin: 40
            }

            // ── Help panel (collapsed by default) ──
            MultiplayerHelpPanel {
                Layout.fillWidth: true
                expanded: false
            }

            // ── State indicator ──
            MultiplayerStateIndicator {
                Layout.fillWidth: true
                mp: root.mp
            }

            // ── Room code card (host only) ──
            MultiplayerRoomCodeCard {
                Layout.fillWidth: true
                mp: root.mp
                toastManager: root.toastManager
            }

            // ── Network monitoring panel ──
            MultiplayerNetworkPanel {
                Layout.fillWidth: true
                mp: root.mp
            }

            // ── Player list ──
            Rectangle {
                id: playerListCard
                Layout.fillWidth: true
                Layout.preferredHeight: playerCol.implicitHeight + 44
                color: StyleTokens.bgPrimary
                border.color: StyleTokens.bgElevated; border.width: 1
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
                        text: {
                            if (!mp) return "在线成员"
                            var count = mp.players ? mp.players.length : 0
                            var kind = mp.role === 1 ? "房主" : "访客"
                            return "在线成员 (" + count + "人) — " + kind + "模式"
                        }
                        font.pixelSize: StyleTokens.fontSizeMd
                        color: StyleTokens.textTertiary
                        Layout.leftMargin: 4
                    }

                    Repeater {
                        model: mp ? mp.players : []

                        MultiplayerPlayerCard {
                            width: playerCol.width
                            playerData: modelData
                            entryIndex: index
                        }
                    }
                }
            }

            // ── Action buttons (idle state) ──
            RowLayout {
                Layout.alignment: Qt.AlignHCenter; spacing: 12
                visible: mp ? mp.state === 0 : true
                opacity: visible ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }

                ShadowButton {
                    Layout.preferredWidth: 220; Layout.preferredHeight: 44
                    text: "创建房间"
                    bold: true
                    btnRadius: StyleTokens.radiusLg
                    iconSource: "icons/lucide/gamepad-2.svg"
                    onClicked: { if (mp) mp.createRoom() }
                }

                ShadowButton {
                    Layout.preferredWidth: 220; Layout.preferredHeight: 44
                    text: "加入房间"
                    bold: true
                    btnRadius: StyleTokens.radiusLg
                    iconSource: "icons/lucide/log-in.svg"
                    onClicked: joinDialog.open()
                }
            }

            // ── Disconnect / cancel button (active states) ──
            RowLayout {
                Layout.alignment: Qt.AlignHCenter; spacing: 12
                visible: mp ? mp.state > 0 : false
                opacity: visible ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }

                ShadowButton {
                    Layout.preferredWidth: 220; Layout.preferredHeight: 44
                    text: mp && mp.state <= 4 ? "取消" : "断开连接"
                    bold: true
                    accentColor: StyleTokens.error
                    btnRadius: StyleTokens.radiusLg
                    iconSource: "icons/lucide/log-out.svg"
                    onClicked: { if (mp) mp.leaveRoom() }
                }
            }

            // Bottom padding
            Item { Layout.preferredHeight: 20 }
        }
    }

    // ── Join Room Dialog ──
    Popup {
        id: joinDialog
        anchors.centerIn: parent; width: 420; height: 280
        modal: true; closePolicy: Popup.CloseOnEscape
        background: Rectangle { color: StyleTokens.bgPrimary; radius: StyleTokens.radiusXl; border.color: StyleTokens.bgElevated; border.width: 1 }

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
                text: "加入联机房间"
                font.pixelSize: StyleTokens.fontSizeXl; font.bold: true; color: StyleTokens.accentLight
            }

            // ── Player name input ──
            ColumnLayout {
                Layout.fillWidth: true; spacing: 4
                Text {
                    text: "你的昵称"
                    font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textSubtle
                }
                InputBox {
                    id: nameInput
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    placeholderText: "输入昵称（可选）"
                    text: mp ? mp.playerName : ""
                    onTextChanged: { if (mp) mp.setPlayerName(text) }
                }
            }

            // ── Room code input ──
            ColumnLayout {
                Layout.fillWidth: true; spacing: 4
                Text {
                    text: "房间码"
                    font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textSubtle
                }
                InputBox {
                    id: joinInput
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    placeholderText: "U/XXXX-XXXX-XXXX-XXXX"
                }
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter; spacing: 12
                ShadowButton {
                    Layout.preferredWidth: 140; Layout.preferredHeight: 40
                    text: "返回"
                    bold: true
                    accentColor: StyleTokens.textTertiary
                    btnRadius: StyleTokens.radiusLg
                    iconSource: "icons/lucide/arrow-left.svg"
                    onClicked: joinDialog.close()
                }
                ShadowButton {
                    Layout.preferredWidth: 140; Layout.preferredHeight: 40
                    text: "加入"
                    bold: true
                    btnRadius: StyleTokens.radiusLg
                    iconSource: "icons/lucide/log-in.svg"
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
