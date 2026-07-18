// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ============================================================
// AgreementOverlay.qml — 协议同意弹窗
// ============================================================

Rectangle {
    id: overlay
    anchors.fill: parent
    color: StyleTokens.bgPrimary
    radius: StyleTokens.radiusXl
    z: 999

    // ── Precompute HTML properties at overlay scope ──
    readonly property string betaHtml: typeof backend !== "undefined" && backend ? (backend.betaAgreementHtml || "") : ""
    readonly property string privacyHtml: typeof backend !== "undefined" && backend ? (backend.privacyAgreementHtml || "") : ""
    readonly property string termsHtml: typeof backend !== "undefined" && backend ? (backend.termsAgreementHtml || "") : ""

    // Pass-through MouseArea — blocks click-through but allows title drag
    MouseArea {
        anchors.fill: parent
    }

    // ── AgreementRow — inline component (scope: overlay) ──
    component AgreementRow: RowLayout {
        id: row
        spacing: 2
        property bool checked: false
        property string labelText: ""
        property string linkText: "[查看]"
        property var onView: null  // callback to open agreement
        signal toggled()
        Layout.preferredHeight: 28

        Rectangle {
            id: cb
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
            radius: StyleTokens.radiusXs
            color: row.checked ? "#6090d0" : "transparent"
            border.color: row.checked ? "#6090d0" : "#3a3e52"
            border.width: 1.5
            Text {
                anchors.centerIn: parent
                text: row.checked ? "\u2713" : ""
                font.pixelSize: StyleTokens.fontSizeXs
                color: StyleTokens.textInverse
                visible: row.checked
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: { row.checked = !row.checked; row.toggled() }
            }
        }
        Text {
            text: row.labelText
            font.pixelSize: StyleTokens.fontSizeSm
            color: "#b0b8d0"
            Layout.leftMargin: 6
        }
        Text {
            text: row.linkText
            font.pixelSize: StyleTokens.fontSizeSm
            color: "#6090d0"
            font.underline: true
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (row.onView) row.onView()
                }
            }
        }
    }

    // ── State ──
    property bool betaChecked: false
    property bool privacyChecked: false
    property bool termsChecked: false
    property bool allChecked: betaChecked && privacyChecked && termsChecked
    property bool showingAgreement: false
    property string currentAgreementTitle: ""
    property string currentAgreementHtml: ""

    // ── Title bar (draggable) ──
    Item {
        id: titleBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 64

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            cursorShape: Qt.OpenHandCursor
            property point lastPos: Qt.point(0, 0)
            onPressed: function(mouse) { lastPos = Qt.point(mouse.x, mouse.y) }
            onPositionChanged: function(mouse) {
                if (overlay.Window && overlay.Window.window) {
                    overlay.Window.window.x += mouse.x - lastPos.x
                    overlay.Window.window.y += mouse.y - lastPos.y
                }
            }
        }
    }

    // ── Close button (matches main launcher style) ──
    Rectangle {
        id: closeBtn
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 12
        width: 28; height: 28
        radius: StyleTokens.radiusLg
        color: closeMA.containsMouse ? (closeMA.pressed ? "#e06060" : "#c05050") : "transparent"
        scale: closeMA.pressed ? 0.85 : (closeMA.containsMouse ? 1.12 : 1.0)
        Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
        Text {
            anchors.centerIn: parent
            text: "\u2715"
            color: closeMA.containsMouse ? "#fff" : "#505568"
            font.pixelSize: StyleTokens.fontSizeSm
            font.weight: Font.Bold
        }
        MouseArea {
            id: closeMA
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: Qt.quit()
        }
    }

    // ── Main content ──
    ColumnLayout {
        id: mainContent
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.55, 560)
        spacing: 0
        visible: !showingAgreement

        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 4
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "SHADOW LAUNCHER"
                font.pixelSize: 28
                font.weight: Font.Bold
                font.letterSpacing: 4
                color: StyleTokens.textInverse
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: typeof backend !== "undefined" && backend ? "v" + backend.appVersion : ""
                font.pixelSize: StyleTokens.fontSizeSm
                color: StyleTokens.textMuted
            }
        }

        Item { Layout.preferredHeight: 24 }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "欢迎使用 Shadow Launcher 内测版！"
            font.pixelSize: StyleTokens.fontSizeMd
            font.weight: Font.Medium
            color: "#d0d4e8"
        }
        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 4
            text: "请仔细阅读并同意以下协议后开始使用。"
            font.pixelSize: StyleTokens.fontSizeSm
            color: "#7880a0"
        }

        Item { Layout.preferredHeight: 20 }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: agreementColumn.implicitHeight + 30
            color: StyleTokens.surfaceOverlay
            radius: StyleTokens.radiusLg
            border.color: StyleTokens.accentSubtle
            border.width: 1

            ColumnLayout {
                id: agreementColumn
                anchors.fill: parent
                anchors.margins: 15
                spacing: 4
                AgreementRow {
                    id: betaRow; Layout.fillWidth: true
                    labelText: "我已阅读并同意《Shadow Launcher 内测人员协议》"
                    checked: overlay.betaChecked
                    onToggled: overlay.betaChecked = checked
                    onView: function() { overlay.showAgreement("内测人员协议", overlay.betaHtml) }
                }
                AgreementRow {
                    id: privacyRow; Layout.fillWidth: true
                    labelText: "我已阅读并同意《Shadow Launcher 隐私协议》"
                    checked: overlay.privacyChecked
                    onToggled: overlay.privacyChecked = checked
                    onView: function() { overlay.showAgreement("隐私协议", overlay.privacyHtml) }
                }
                AgreementRow {
                    id: termsRow; Layout.fillWidth: true
                    labelText: "我已阅读并同意《Shadow Launcher 用户协议》"
                    checked: overlay.termsChecked
                    onToggled: overlay.termsChecked = checked
                    onView: function() { overlay.showAgreement("用户协议", overlay.termsHtml) }
                }
            }
        }

        Item { Layout.preferredHeight: 24 }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 16
            Rectangle {
                id: agreeBtn
                property bool btnEnabled: overlay.allChecked
                Layout.preferredWidth: 160
                Layout.preferredHeight: 40
                radius: StyleTokens.radiusLg
                color: btnEnabled ? "#6090d0" : "#2a2e42"
                Text {
                    anchors.centerIn: parent
                    text: "同意并继续"
                    font.pixelSize: StyleTokens.fontSizeMd
                    font.weight: Font.Medium
                    color: agreeBtn.btnEnabled ? "#ffffff" : "#505878"
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: agreeBtn.btnEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                    enabled: agreeBtn.btnEnabled
                    onClicked: {
                        if (typeof backend !== "undefined" && backend) backend.markAgreed = true
                    }
                }
            }
            Rectangle {
                Layout.preferredWidth: 160
                Layout.preferredHeight: 40
                radius: StyleTokens.radiusLg
                color: "transparent"
                border.color: "#3a3e52"
                border.width: 1
                Text {
                    anchors.centerIn: parent
                    text: "不同意并退出"
                    font.pixelSize: StyleTokens.fontSizeMd
                    color: "#7880a0"
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: Qt.quit()
                }
            }
        }
    }

    // ── RichText reading view ──
    Rectangle {
        anchors.fill: parent
        color: StyleTokens.bgPrimary
        visible: showingAgreement

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                color: StyleTokens.bgSecondary

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 8
                    Text {
                        text: currentAgreementTitle
                        font.pixelSize: StyleTokens.fontSizeMd
                        font.weight: Font.Medium
                        color: "#d0d4e8"
                        Layout.fillWidth: true
                    }
                    Rectangle {
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        radius: StyleTokens.radiusMd
                        color: StyleTokens.bgCard
                        Text {
                            anchors.centerIn: parent
                            text: "\u2715"
                            font.pixelSize: StyleTokens.fontSizeMd
                            color: "#7880a0"
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: overlay.showingAgreement = false
                        }
                    }
                }
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 20
                clip: true

                Text {
                    id: agreementText
                    width: overlay.width - 40
                    text: currentAgreementHtml || "<p style='color:#7880a0'>无法加载协议文件</p>"
                    textFormat: Text.RichText
                    font.pixelSize: StyleTokens.fontSizeMd
                    color: "#b0b8d0"
                    wrapMode: Text.WordWrap
                    onLinkActivated: function(url) {
                        Qt.openUrlExternally(url)
                    }
                }
            }
        }
    }

    function showAgreement(title, html) {
        currentAgreementTitle = title
        currentAgreementHtml = html
        showingAgreement = true
    }
}
