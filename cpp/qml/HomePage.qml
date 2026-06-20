import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "qrc:/ShadowLauncher/qml"

Rectangle {
    id: homePage
    color: "#121418"

    property bool isLoggedIn: false
    property string displayName: ""

    signal launchGame()
    signal openVersionSelect()
    signal openVersionSettings()

    Connections {
        target: backend
        enabled: backend !== null
        function onUsernameChanged() {
            homePage.isLoggedIn = backend.username !== ""
            homePage.displayName = backend.username
        }
        function onSkinReady() {
            // 皮肤就绪
        }
        function onOfflineLoginResult(success) {
            if (success) {
                homePage.isLoggedIn = true
                homePage.displayName = backend.username
            }
        }
    }

    Component.onCompleted: {
        if (backend) {
            homePage.isLoggedIn = backend.username !== ""
            homePage.displayName = backend.username
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 16

        // 离线登录区域（未登录时显示）
        ColumnLayout {
            visible: !homePage.isLoggedIn
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 12

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                width: 60; height: 60; radius: 30
                color: "#2A2F3A"
                Text {
                    anchors.centerIn: parent
                    text: "👤"; font.pixelSize: 28
                }
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "离线模式"; color: "#F1F3F6"; font.pixelSize: 18; font.bold: true
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "无需正版账号，输入用户名即可开始游戏"
                color: "#7E8596"; font.pixelSize: 12
            }

            Item { height: 8; width: 1 }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 8

                Rectangle {
                    width: 220; height: 40; radius: 8
                    color: "#1A1D24"; border.color: "#2A2F3A"
                    TextInput {
                        id: nameInput
                        anchors.fill: parent
                        anchors.margins: 12
                        color: "#F1F3F6"; font.pixelSize: 14
                        verticalAlignment: Text.AlignVCenter
                        Text {
                            anchors.fill: parent
                            anchors.margins: 12
                            text: "输入用户名..."; color: "#4a4d5a"
                            font.pixelSize: 14; verticalAlignment: Text.AlignVCenter
                            visible: !nameInput.text
                        }
                    }
                }

                Rectangle {
                    width: 60; height: 40; radius: 8
                    color: "#3B82F6"
                    Text {
                        anchors.centerIn: parent
                        text: "登录"; color: "#FFF"; font.pixelSize: 14
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            if (backend) {
                                var name = nameInput.text.trim() || "Player"
                                backend.setLastLoginMode(0)
                                backend.offlineLogin(name)
                            }
                        }
                    }
                }
            }
        }

        // 登录后显示
        ColumnLayout {
            visible: homePage.isLoggedIn
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 16

            // 用户信息
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 12

                Rectangle {
                    width: 48; height: 48; radius: 24
                    color: "#2A2F3A"
                    Image {
                        anchors.fill: parent
                        anchors.margins: 2
                        source: backend ? (backend.skinHeadUrl || "") : ""
                        fillMode: Image.PreserveAspectFit
                    }
                    Text {
                        anchors.centerIn: parent
                        text: "👤"; font.pixelSize: 20
                        visible: !backend || !backend.skinHeadUrl
                    }
                }

                ColumnLayout {
                    spacing: 2
                    Text {
                        text: homePage.displayName || "Player"
                        color: "#F1F3F6"; font.pixelSize: 16; font.bold: true
                    }
                    Text {
                        text: "离线模式"; color: "#7E8596"; font.pixelSize: 11
                    }
                }
            }

            // 启动按钮
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                width: 280; height: 52; radius: 8
                color: "#10B981"
                Text {
                    anchors.centerIn: parent
                    text: "🚀 启动 Minecraft"; color: "#FFF"; font.pixelSize: 16; font.bold: true
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: homePage.launchGame()
                }
            }

            // 版本管理按钮
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                width: 280; height: 40; radius: 8
                color: "#1A1D24"; border.color: "#2A2F3A"
                Text {
                    anchors.centerIn: parent
                    text: "⚙ 版本管理"; color: "#F1F3F6"; font.pixelSize: 14
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: homePage.openVersionSelect()
                }
            }
        }
    }
}
