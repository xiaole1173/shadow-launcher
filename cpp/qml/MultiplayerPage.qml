// SPDX-License-Identifier: AGPL-3.0-or-later
// 联机主页面 — 全功能集成 + 逐层入场动画 + 弹窗过渡
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    anchors.fill: parent
    color: "transparent"

    property var mp: backend ? backend.multiplayer : null
    property var toastManager: null

    // ── Page entrance animation ──
    opacity: 0
    Component.onCompleted: pageEnterAnim.start()
    NumberAnimation { id: pageEnterAnim; target: root; property: "opacity"; to: 1; duration: AnimationTokens.pageDuration; easing.type: AnimationTokens.pageEasing }

    // Track whether backend initialization is complete (room ready for sharing)
    property bool _backendReady: false

    Connections {
        target: mp
        function onStateChanged() {
            // Aligned with C++ enum: 0=Idle, 6=WaitingForGuests (host ready), 9=Error
            if (mp) {
                if (mp.state === 0) {
                    root._backendReady = false;
                } else if (mp.state === 6 || mp.state === 8) {
                    root._backendReady = true;
                } else if (mp.state === 9) {
                    root._backendReady = false;
                }
            }
        }
        function onErrorOccurred(msg) {
            root._backendReady = false;
            if (root.toastManager && msg)
                root.toastManager.show(msg)
            // Error shake feedback
            errorShakeAnim.restart()
        }
        function onMinecraftPortReady(port) {
            if (root.toastManager)
                root.toastManager.show("MC 端口已就绪: " + port)
        }
        function onConnectionDifficultyChanged() {}
    }

    // Error shake animation
    SequentialAnimation {
        id: errorShakeAnim
        NumberAnimation { target: root; property: "x"; to: -4; duration: 40 }
        NumberAnimation { target: root; property: "x"; to: 4; duration: 40 }
        NumberAnimation { target: root; property: "x"; to: -2; duration: 40 }
        NumberAnimation { target: root; property: "x"; to: 2; duration: 40 }
        NumberAnimation { target: root; property: "x"; to: 0; duration: 40 }
    }

    Flickable {
        id: pageFlick
        anchors.fill: parent
        contentWidth: width
        contentHeight: contentCol.implicitHeight + 80
        clip: true
        interactive: true
        boundsBehavior: Flickable.StopAtBounds
        // Smooth scroll inertia
        flickDeceleration: 1500
        maximumFlickVelocity: 3000

        ColumnLayout {
            id: contentCol
            width: pageFlick.width - 80
            x: 40
            spacing: 16

            // ── Page header with entry delay ──
            Text {
                text: "多人联机"
                font.pixelSize: 28; font.bold: true; color: StyleTokens.textSecondary
                Layout.topMargin: 40
                opacity: 0
                Component.onCompleted: opacity = 1
                Behavior on opacity { NumberAnimation { duration: 350; easing.type: Easing.OutCubic } }
            }

            // ── Help panel (collapsed) ──
            MultiplayerHelpPanel { Layout.fillWidth: true; expanded: false }

            // ── State indicator ──
            MultiplayerStateIndicator { Layout.fillWidth: true; mp: root.mp }

            // ── Room code card (visible only after backend signals room is ready) ──
            MultiplayerRoomCodeCard {
                Layout.fillWidth: true
                mp: root.mp
                toastManager: root.toastManager
                visible: mp && mp.role === 1 && mp.roomCode !== "" && root._backendReady
            }

            // ── Network monitoring panel (visible only during/after active session) ──
            MultiplayerNetworkPanel { Layout.fillWidth: true; mp: root.mp }

            // ── Player list with grouped animation ──
            Rectangle {
                id: playerListCard
                Layout.fillWidth: true
                Layout.preferredHeight: playerCol.implicitHeight + 44
                color: StyleTokens.bgPrimary
                border.color: StyleTokens.bgElevated; border.width: 1
                radius: StyleTokens.radiusLg
                visible: mp ? (mp.players && mp.players.length > 0) : false

                opacity: visible ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: AnimationTokens.panelEnterDuration; easing.type: AnimationTokens.panelEnterEasing } }
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
                        delegate: MultiplayerPlayerCard {
                            width: playerCol.width
                            playerData: modelData
                            entryIndex: index
                        }
                    }
                }
            }

            // ── Loading state during backend initialization ──
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 80
                color: "transparent"
                visible: mp && (mp.state === 1 || mp.state === 7) && mp.role === 1
                opacity: visible ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 8
                    Image {
                        Layout.alignment: Qt.AlignHCenter
                        source: "icons/lucide/refresh-cw.svg"
                        width: 24; height: 24
                        NumberAnimation on rotation {
                            from: 0; to: 360
                            duration: 1200
                            loops: Animation.Infinite
                        }
                    }
                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "正在启动局域网、初始化联机服务..."
                        font.pixelSize: StyleTokens.fontSizeMd
                        color: StyleTokens.textTertiary
                    }
                }
            }

            // ── Action buttons with press feedback ──
            RowLayout {
                Layout.alignment: Qt.AlignHCenter; spacing: 16
                visible: mp ? mp.state === 0 : true
                opacity: visible ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }

                ShadowButton {
                    Layout.preferredWidth: 220; Layout.preferredHeight: 44
                    text: "创建房间"
                    bold: true
                    btnRadius: StyleTokens.radiusLg
                    iconSource: "icons/lucide/gamepad-2.svg"
                    hoverScale: 1.05; pressScale: 0.93
                    onClicked: { if (mp) mp.createRoom() }
                }

                ShadowButton {
                    Layout.preferredWidth: 220; Layout.preferredHeight: 44
                    text: "加入房间"
                    bold: true
                    btnRadius: StyleTokens.radiusLg
                    iconSource: "icons/lucide/log-in.svg"
                    hoverScale: 1.05; pressScale: 0.93
                    onClicked: joinDialog.open()
                }
            }

            // ── Disconnect button ──
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                visible: mp ? mp.state > 0 : false
                opacity: visible ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }

                ShadowButton {
                    Layout.preferredWidth: 220; Layout.preferredHeight: 44
                    text: mp && (mp.state <= 4 || mp.state === 7) ? "取消" : "断开连接"
                    bold: true
                    accentColor: StyleTokens.error
                    btnRadius: StyleTokens.radiusLg
                    iconSource: "icons/lucide/log-out.svg"
                    hoverScale: 1.05; pressScale: 0.93
                    onClicked: { if (mp) mp.leaveRoom() }
                }
            }

            // ── 联机服务合规声明（显眼白色，置于页面底部）──
            Text {
                Layout.fillWidth: true
                Layout.topMargin: 12
                text: "使用联机服务，即表示您承诺，在多人联机全过程中，您将严格遵守您所在国家或地区的全部法律法规。您不得将多人联机功能用于除多人联机外的其他用途。"
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                color: StyleTokens.textInverse
                font.pixelSize: StyleTokens.fontSizeSm
                lineHeight: 1.5
                opacity: 0.9
            }

            Item { Layout.preferredHeight: 20 }
        }
    }

    // ── Join Room Dialog with smooth open/close ──
    Popup {
        id: joinDialog
        anchors.centerIn: parent; width: 420; height: 280
        modal: true; closePolicy: Popup.CloseOnEscape
        background: Rectangle {
            color: StyleTokens.bgPrimary; radius: StyleTokens.radiusXl
            border.color: StyleTokens.bgElevated; border.width: 1
        }

        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 220; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.92; to: 1; duration: 250; easing.type: Easing.OutBack }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 140; easing.type: Easing.InCubic }
            NumberAnimation { property: "scale"; from: 1; to: 0.95; duration: 140; easing.type: Easing.InCubic }
        }

        // Overlay dim with fade
        Overlay.modal: Rectangle {
            color: "#60000000"
            Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
        }

        ColumnLayout {
            anchors.fill: parent; anchors.margins: 24; spacing: 16

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "加入联机房间"
                font.pixelSize: StyleTokens.fontSizeXl; font.bold: true; color: StyleTokens.accentLight
            }

            ColumnLayout {
                Layout.fillWidth: true; spacing: 4
                Text {
                    text: "你的昵称"
                    font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textSubtle
                }
                InputBox {
                    id: nameInput
                    Layout.fillWidth: true; Layout.preferredHeight: 40
                    placeholderText: "输入昵称（可选）"
                    text: mp ? mp.playerName : ""
                    onTextChanged: { if (mp) mp.setPlayerName(text) }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true; spacing: 4
                Text {
                    text: "房间码"
                    font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textSubtle
                }
                InputBox {
                    id: joinInput
                    Layout.fillWidth: true; Layout.preferredHeight: 40
                    placeholderText: "U/XXXX-XXXX-XXXX-XXXX"
                }
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter; spacing: 14
                ShadowButton {
                    Layout.preferredWidth: 140; Layout.preferredHeight: 40
                    text: "返回"
                    bold: true
                    accentColor: StyleTokens.textTertiary
                    btnRadius: StyleTokens.radiusLg
                    iconSource: "icons/lucide/arrow-left.svg"
                    hoverScale: 1.05; pressScale: 0.93
                    onClicked: joinDialog.close()
                }
                ShadowButton {
                    Layout.preferredWidth: 140; Layout.preferredHeight: 40
                    text: "加入"
                    bold: true
                    btnRadius: StyleTokens.radiusLg
                    iconSource: "icons/lucide/log-in.svg"
                    hoverScale: 1.05; pressScale: 0.93
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
