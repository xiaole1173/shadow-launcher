// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

// ========== 版本级登录配置 ==========
Item {
    id: root
    property var backend: null
    property var toastManager: null
    property string currentSelectedVersion: ""

    ColumnLayout {
        anchors.fill: parent
        spacing: 16

        Item { Layout.preferredHeight: 8 }

        // ── Header ──
        Text {
            text: qsTr("登录")
            font.pixelSize: StyleTokens.fontSizeLg
            font.weight: Font.SemiBold
            color: StyleTokens.textPrimary
        }

        Text {
            text: qsTr("选择此版本的登录方式。第三方登录将在启动页面提供账号密码输入框，启动游戏时自动验证。")
            font.pixelSize: StyleTokens.fontSizeSm
            color: "#7E8596"
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            lineHeight: 1.5
        }

        // ── 登录方式 下拉框 ──
        RowLayout {
            spacing: 12

            Text {
                text: qsTr("登录方式")
                font.pixelSize: StyleTokens.fontSizeMd
                color: "#B4BAC6"
                Layout.preferredWidth: 80
            }

            Rectangle {
                Layout.preferredWidth: 220
                height: 36
                radius: StyleTokens.radiusLg
                color: StyleTokens.surfaceLight
                border.color: loginComboHover.containsMouse ? "#3B82F6" : "#2A2F3A"
                border.width: 1

                Behavior on border.color { ColorAnimation { duration: 200 } }

                ComboBox {
                    id: loginModeCombo
                    anchors.fill: parent
                    anchors.margins: 1
                    model: ["正版登录或离线登录", "Authlib-Injector", "统一通行证"]
                    currentIndex: 0
                    flat: true

                    background: Item {}
                    contentItem: Text {
                        leftPadding: 10
                        text: loginModeCombo.displayText
                        font.pixelSize: StyleTokens.fontSizeMd
                        color: "#B4BAC6"
                        verticalAlignment: Text.AlignVCenter
                    }
                    indicator: Text {
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        text: "\u25bc"
                        font.pixelSize: StyleTokens.fontSizeXs
                        color: "#7E8596"
                    }

                    delegate: ItemDelegate {
                        width: loginModeCombo.width
                        height: 36
                        contentItem: Text {
                            text: modelData
                            font.pixelSize: StyleTokens.fontSizeMd
                            color: hovered ? "#F1F3F6" : "#B4BAC6"
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 10
                        }
                        background: Rectangle {
                            color: hovered ? "#252A35" : "#1A1D24"
                            radius: StyleTokens.radiusSm
                        }
                    }

                    popup: Popup {
                        y: loginModeCombo.height
                        width: loginModeCombo.width
                        height: implicitHeight
                        padding: 4
                        background: Rectangle {
                            color: StyleTokens.surfaceLight
                            radius: StyleTokens.radiusLg
                            border.color: StyleTokens.bgHover
                        }
                        contentItem: ListView {
                            clip: true
                            implicitHeight: contentHeight
                            model: loginModeCombo.popup.visible ? loginModeCombo.delegateModel : null
                            currentIndex: loginModeCombo.highlightedIndex
                        }
                    }

                    Component.onCompleted: {
                        if (root.backend && root.currentSelectedVersion) {
                            var savedType = root.backend.versionLoginType(root.currentSelectedVersion)
                            loginModeCombo.currentIndex = savedType
                            _updateFields(savedType)
                        }
                    }

                    onCurrentIndexChanged: {
                        if (root.backend && root.currentSelectedVersion) {
                            root.backend.setVersionLoginType(root.currentSelectedVersion, currentIndex)
                            _updateFields(currentIndex)
                        }
                    }

                    function _updateFields(type) {
                        authServerField.visible = type > 0
                        registerUrlField.visible = type === 1
                        if (type > 0 && root.backend && root.currentSelectedVersion) {
                            var saved = root.backend.versionAuthServer(root.currentSelectedVersion)
                            if (saved)
                                authServerField.text = saved
                            else
                                authServerField.text = type === 1 ? "https://littleskin.cn/api/yggdrasil" : ""
                            var savedReg = root.backend.versionAuthRegisterUrl(root.currentSelectedVersion)
                            if (savedReg)
                                registerUrlField.text = savedReg
                            else
                                registerUrlField.text = ""
                        }
                    }
                }

                MouseArea {
                    id: loginComboHover
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                }
            }
        }

        // ── 认证服务器地址 ──
        TextField {
            id: authServerField
            Layout.fillWidth: true
            Layout.topMargin: 4
            visible: false
            placeholderText: qsTr("认证服务器地址（例如 https://littleskin.cn/api/yggdrasil）")
            placeholderTextColor: "#5A6173"
            color: StyleTokens.textPrimary
            font.pixelSize: StyleTokens.fontSizeMd
            leftPadding: 12
            topPadding: 10
            bottomPadding: 10

            background: Rectangle {
                radius: StyleTokens.radiusLg
                color: StyleTokens.surfaceLight
                border.color: authServerField.activeFocus ? "#3B82F6" : "#2A2F3A"
                border.width: 1
                Behavior on border.color { ColorAnimation { duration: 200 } }
            }

            onEditingFinished: {
                if (root.backend && root.currentSelectedVersion)
                    root.backend.setVersionAuthServer(root.currentSelectedVersion, text)
            }
        }

        // ── 认证注册链接（仅 Authlib-Injector） ──
        TextField {
            id: registerUrlField
            Layout.fillWidth: true
            Layout.topMargin: 8
            visible: false
            placeholderText: qsTr("认证注册链接（可选）")
            placeholderTextColor: "#5A6173"
            color: StyleTokens.textPrimary
            font.pixelSize: StyleTokens.fontSizeMd
            leftPadding: 12
            topPadding: 10
            bottomPadding: 10

            background: Rectangle {
                radius: StyleTokens.radiusLg
                color: StyleTokens.surfaceLight
                border.color: registerUrlField.activeFocus ? "#3B82F6" : "#2A2F3A"
                border.width: 1
                Behavior on border.color { ColorAnimation { duration: 200 } }
            }

            onEditingFinished: {
                if (root.backend && root.currentSelectedVersion)
                    root.backend.setVersionAuthRegisterUrl(root.currentSelectedVersion, text)
            }
        }

        Item { Layout.fillHeight: true }
    }
}
