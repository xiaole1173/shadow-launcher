import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ============================================================
// AgreementOverlay.qml — 协议同意弹窗
// ============================================================

Rectangle {
    id: overlay
    anchors.fill: parent
    color: "#0c0f16"
    radius: 12
    z: 999

    // ── Precompute HTML properties at overlay scope ──
    // (component AgreementRow has its own scope and can't see `backend` directly)
    readonly property string betaHtml: typeof backend !== "undefined" && backend ? backend.betaAgreementHtml : ""
    readonly property string privacyHtml: typeof backend !== "undefined" && backend ? backend.privacyAgreementHtml : ""
    readonly property string termsHtml: typeof backend !== "undefined" && backend ? backend.termsAgreementHtml : ""

    // Block clicks from passing through, but allow drag on title area
    MouseArea {
        id: clickBlocker
        anchors.fill: parent
        // Don't accept all buttons — let the title area handle drag
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
            onPressed: function(mouse) {
                lastPos = Qt.point(mouse.x, mouse.y)
            }
            onPositionChanged: function(mouse) {
                if (overlay.Window && overlay.Window.window) {
                    overlay.Window.window.x += mouse.x - lastPos.x
                    overlay.Window.window.y += mouse.y - lastPos.y
                }
            }
            onReleased: cursorShape = Qt.OpenHandCursor
        }
    }

    // ── Close button ──
    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 12
        width: 32
        height: 32
        radius: 6
        color: "#1a1e32"
        z: 10
        Text {
            anchors.centerIn: parent
            text: "\u2715"
            font.pixelSize: 14
            color: "#7880a0"
        }
        MouseArea {
            anchors.fill: parent
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

        // Logo / Title area
        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 4

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "SHADOW LAUNCHER"
                font.pixelSize: 28
                font.weight: Font.Bold
                font.letterSpacing: 4
                color: "#ffffff"
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: typeof backend !== "undefined" && backend ? "v" + backend.appVersion : ""
                font.pixelSize: 11
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
                    onLinkClicked: overlay.showAgreement("内测人员协议", overlay.betaHtml)
                }
                AgreementRow {
                    id: privacyRow; Layout.fillWidth: true
                    labelText: "我已阅读并同意《Shadow Launcher 隐私协议》"
                    checked: overlay.privacyChecked
                    onToggled: overlay.privacyChecked = checked
                    onLinkClicked: overlay.showAgreement("隐私协议", overlay.privacyHtml)
                }
                AgreementRow {
                    id: termsRow; Layout.fillWidth: true
                    labelText: "我已阅读并同意《Shadow Launcher 用户协议》"
                    checked: overlay.termsChecked
                    onToggled: overlay.termsChecked = checked
                    onLinkClicked: overlay.showAgreement("用户协议", overlay.termsHtml)
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
                        if (typeof backend !== "undefined" && backend) backend.acceptAgreements()
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
                radius: 12

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
                    text: currentAgreementHtml || "<p style='color:#7880a0'>无法加载协议文件</p>"
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
