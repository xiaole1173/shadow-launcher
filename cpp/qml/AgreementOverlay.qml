import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtWebEngine

// ============================================================
// AgreementOverlay.qml — 协议同意弹窗
//
// 启动器首次运行时强制覆盖一切，用户必须勾选三份协议后才能使用。
// 协议存储在 QSettings 中，版本号关联，协议更新时重新弹窗。
// ============================================================

// ── AgreementRow reusable component ──
component AgreementRow: RowLayout {
    id: row
    spacing: 2

    property bool checked: false
    property string labelText: ""
    property string linkText: "[查看]"

    signal toggled()
    signal linkClicked()

    Layout.preferredHeight: 28

    // Checkbox (custom styled)
    Rectangle {
        id: cb
        Layout.preferredWidth: 16
        Layout.preferredHeight: 16
        radius: 3
        color: row.checked ? "#6090d0" : "transparent"
        border.color: row.checked ? "#6090d0" : "#3a3e52"
        border.width: 1.5

        Text {
            anchors.centerIn: parent
            text: row.checked ? "\u2713" : ""
            font.pixelSize: 10
            color: "#ffffff"
            visible: row.checked
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                row.checked = !row.checked
                row.toggled()
            }
        }
    }

    // Label
    Text {
        text: row.labelText
        font.pixelSize: 12
        color: "#b0b8d0"
        Layout.leftMargin: 6
    }

    // [查看] link
    Text {
        text: row.linkText
        font.pixelSize: 12
        color: "#6090d0"
        font.underline: true

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: row.linkClicked()
        }
    }
}

// ── Main overlay ──
Rectangle {
    id: overlay
    anchors.fill: parent
    color: "#0c0f16"
    z: 999

    // Block clicks from passing through to main UI
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
    }

    // ── View state ──
    property bool betaChecked: false
    property bool privacyChecked: false
    property bool termsChecked: false
    property bool allChecked: betaChecked && privacyChecked && termsChecked

    // WebView state
    property bool showingAgreement: false
    property string currentAgreementTitle: ""
    property string currentAgreementUrl: ""

    // ── Main content ──
    ColumnLayout {
        id: mainContent
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.55, 560)
        spacing: 0
        visible: !showingAgreement

        // Logo / Title area
        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 6

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "SHADOW"
                font.pixelSize: 28
                font.weight: Font.Bold
                font.letterSpacing: 6
                color: "#ffffff"
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: overlay.parent && overlay.parent.backend ? "Launcher  v" + overlay.parent.backend.appVersion : ""
                font.pixelSize: 12
                color: "#505878"
            }
        }

        Item { Layout.preferredHeight: 24 }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "欢迎使用 Shadow Launcher 内测版！"
            font.pixelSize: 14
            font.weight: Font.Medium
            color: "#d0d4e8"
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 4
            text: "请仔细阅读并同意以下协议后开始使用。"
            font.pixelSize: 12
            color: "#7880a0"
        }

        Item { Layout.preferredHeight: 20 }

        // ── Agreement rows ──
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: agreementColumn.implicitHeight + 30
            color: "#121622"
            radius: 10
            border.color: "#1e2240"
            border.width: 1

            ColumnLayout {
                id: agreementColumn
                anchors.fill: parent
                anchors.margins: 15
                spacing: 4

                AgreementRow {
                    id: betaRow
                    Layout.fillWidth: true
                    labelText: "我已阅读并同意《Shadow Launcher 内测人员协议》"
                    linkText: "[ 查看 ]"
                    checked: overlay.betaChecked
                    onToggled: overlay.betaChecked = checked
                    onLinkClicked: overlay.showAgreement(
                        "内测人员协议",
                        "qrc:/qt/qml/ShadowLauncher/agreements/beta_agreement.html"
                    )
                }

                AgreementRow {
                    id: privacyRow
                    Layout.fillWidth: true
                    labelText: "我已阅读并同意《Shadow Launcher 隐私协议》"
                    linkText: "[ 查看 ]"
                    checked: overlay.privacyChecked
                    onToggled: overlay.privacyChecked = checked
                    onLinkClicked: overlay.showAgreement(
                        "隐私协议",
                        "qrc:/qt/qml/ShadowLauncher/agreements/privacy_policy.html"
                    )
                }

                AgreementRow {
                    id: termsRow
                    Layout.fillWidth: true
                    labelText: "我已阅读并同意《Shadow Launcher 用户协议》"
                    linkText: "[ 查看 ]"
                    checked: overlay.termsChecked
                    onToggled: overlay.termsChecked = checked
                    onLinkClicked: overlay.showAgreement(
                        "用户协议",
                        "qrc:/qt/qml/ShadowLauncher/agreements/terms_of_service.html"
                    )
                }
            }
        }

        Item { Layout.preferredHeight: 24 }

        // ── Buttons ──
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 16

            // Agree button
            Rectangle {
                id: agreeBtn
                property bool btnEnabled: overlay.allChecked
                Layout.preferredWidth: 160
                Layout.preferredHeight: 40
                radius: 8
                color: btnEnabled ? "#6090d0" : "#2a2e42"

                Text {
                    anchors.centerIn: parent
                    text: "同意并继续"
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    color: agreeBtn.btnEnabled ? "#ffffff" : "#505878"
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: agreeBtn.btnEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                    enabled: agreeBtn.btnEnabled
                    onClicked: {
                        if (overlay.parent && overlay.parent.backend) {
                            overlay.parent.backend.acceptAgreements()
                        }
                    }
                }
            }

            // Decline button
            Rectangle {
                Layout.preferredWidth: 160
                Layout.preferredHeight: 40
                radius: 8
                color: "transparent"
                border.color: "#3a3e52"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "不同意并退出"
                    font.pixelSize: 13
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

    // ── WebView for agreement content ──
    Rectangle {
        id: webViewContainer
        anchors.fill: parent
        color: "#0c0f16"
        visible: showingAgreement

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // Header bar
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                color: "#0e1120"
                border.color: "#1e2240"
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 8

                    Text {
                        text: currentAgreementTitle
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        color: "#d0d4e8"
                        Layout.fillWidth: true
                    }

                    Rectangle {
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        radius: 6
                        color: "#1a1e32"

                        Text {
                            anchors.centerIn: parent
                            text: "\u2715"
                            font.pixelSize: 14
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

            // WebView
            WebEngineView {
                id: agreementWebView
                Layout.fillWidth: true
                Layout.fillHeight: true
                url: showingAgreement ? currentAgreementUrl : ""
                settings.localContentCanAccessFileUrls: true
            }
        }
    }

    // ── Functions ──
    function showAgreement(title, url) {
        currentAgreementTitle = title
        currentAgreementUrl = url
        showingAgreement = true
    }
}
