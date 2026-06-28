import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ============================================================
// AgreementOverlay.qml — 协议同意弹窗
// 首次运行时强制覆盖一切，用户须勾选三份协议后才能使用。
// 协议状态持久化到 QSettings，版本号关联。
// ============================================================

Rectangle {
    id: overlay
    anchors.fill: parent
    color: "#0c0f16"
    z: 999

    Component.onCompleted: console.log("[AgreementOverlay] loaded, backend=", typeof backend !== "undefined" ? backend : "UNDEFINED", "agreementAccepted=", typeof backend !== "undefined" && backend ? backend.agreementAccepted : "N/A")

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
    }

    // ── Inline component: AgreementRow ──
    component AgreementRow: RowLayout {
        id: row
        spacing: 2
        property bool checked: false
        property string labelText: ""
        property string linkText: "[查看]"
        signal toggled()
        signal linkClicked()
        Layout.preferredHeight: 28

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
                onClicked: { row.checked = !row.checked; row.toggled() }
            }
        }
        Text {
            text: row.labelText
            font.pixelSize: 12
            color: "#b0b8d0"
            Layout.leftMargin: 6
        }
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

    // ── State ──
    property bool betaChecked: false
    property bool privacyChecked: false
    property bool termsChecked: false
    property bool allChecked: betaChecked && privacyChecked && termsChecked
    property bool showingAgreement: false
    property string currentAgreementTitle: ""
    property string currentAgreementHtml: ""

    // ── Main content ──
    ColumnLayout {
        id: mainContent
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.55, 560)
        spacing: 0
        visible: !showingAgreement

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
                text: "Launcher  v" + (backend ? backend.appVersion : "")
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
                    id: betaRow; Layout.fillWidth: true
                    labelText: "我已阅读并同意《Shadow Launcher 内测人员协议》"
                    checked: overlay.betaChecked
                    onToggled: overlay.betaChecked = checked
                    onLinkClicked: overlay.showAgreement("内测人员协议",
                        backend ? backend.betaAgreementHtml : "")
                }
                AgreementRow {
                    id: privacyRow; Layout.fillWidth: true
                    labelText: "我已阅读并同意《Shadow Launcher 隐私协议》"
                    checked: overlay.privacyChecked
                    onToggled: overlay.privacyChecked = checked
                    onLinkClicked: overlay.showAgreement("隐私协议",
                        backend ? backend.privacyAgreementHtml : "")
                }
                AgreementRow {
                    id: termsRow; Layout.fillWidth: true
                    labelText: "我已阅读并同意《Shadow Launcher 用户协议》"
                    checked: overlay.termsChecked
                    onToggled: overlay.termsChecked = checked
                    onLinkClicked: overlay.showAgreement("用户协议",
                        backend ? backend.termsAgreementHtml : "")
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
                        if (backend) backend.acceptAgreements()
                    }
                }
            }
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

    // ── RichText reading view ──
    Rectangle {
        anchors.fill: parent
        color: "#0c0f16"
        visible: showingAgreement

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // Header
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

            // Scrollable content
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 20
                clip: true

                Text {
                    id: agreementText
                    width: overlay.width - 40
                    text: currentAgreementHtml
                    textFormat: Text.RichText
                    font.pixelSize: 13
                    color: "#b0b8d0"
                    wrapMode: Text.WordWrap
                    onLinkActivated: function(url) {
                        if (typeof Qt !== "undefined") Qt.openUrlExternally(url)
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
