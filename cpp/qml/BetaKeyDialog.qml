// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Window {
    id: root
    width: 420
    height: 300
    visible: true
    flags: Qt.Dialog | Qt.FramelessWindowHint
    color: StyleTokens.bgCard
    title: "Shadow Launcher 内测版"

    // Dark rounded border
    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.color: StyleTokens.bgHover
        border.width: 1
        radius: StyleTokens.radiusXl
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 20

        // Logo / title
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "Shadow Launcher"
            font.pixelSize: StyleTokens.fontSize2xl
            font.bold: true
            color: "#7ec8e3"
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "内测版本 · 请输入密钥"
            font.pixelSize: StyleTokens.fontSizeMd
            color: "#888"
        }

        // Input
        Rectangle {
            Layout.preferredWidth: 320
            Layout.preferredHeight: 44
            Layout.alignment: Qt.AlignHCenter
            color: StyleTokens.bgPrimary
            radius: StyleTokens.radiusLg
            border.color: keyInput.activeFocus ? "#7ec8e3" : "#2a2a4a"
            border.width: 1

            TextInput {
                id: keyInput
                anchors.fill: parent
                anchors.margins: 12
                verticalAlignment: TextInput.AlignVCenter
                font.pixelSize: StyleTokens.fontSizeMd
                color: StyleTokens.textSecondary
                clip: true
                focus: true
                maximumLength: 50

                Text {
                    anchors.fill: parent
                    verticalAlignment: Text.AlignVCenter
                    text: "输入内测密钥..."
                    color: StyleTokens.textMuted
                    font.pixelSize: StyleTokens.fontSizeMd
                    visible: !keyInput.text
                }

                Keys.onReturnPressed: submit()
            }
        }

        // Status
        Text {
            id: statusText
            Layout.alignment: Qt.AlignHCenter
            font.pixelSize: StyleTokens.fontSizeSm
            visible: false
        }

        // Submit button
        Rectangle {
            Layout.preferredWidth: 320
            Layout.preferredHeight: 42
            Layout.alignment: Qt.AlignHCenter
            radius: StyleTokens.radiusLg
            color: verifyBtn.pressed ? "#3a7aa5" : "#4a9ac5"

            Text {
                anchors.centerIn: parent
                text: "验证"
                font.pixelSize: StyleTokens.fontSizeLg
                font.bold: true
                color: "white"
            }

            MouseArea {
                id: verifyBtn
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: submit()
            }
        }
    }

    function submit() {
        var k = keyInput.text.trim();
        if (!k) {
            showStatus("请输入密钥", "#ff8844");
            return;
        }
        showStatus("验证中...", "#888");
        backend.submitBetaKey(k);
    }

    function showStatus(msg, c) {
        statusText.text = msg;
        statusText.color = c;
        statusText.visible = true;
    }

    Connections {
        target: backend
        function onBetaKeyInvalid(reason) {
            showStatus(reason, "#ff5555");
        }
        function onBetaVerified() {
            root.close();
        }
    }
}
