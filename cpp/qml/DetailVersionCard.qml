import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

// DetailVersionCard
// Unified version card used by ModDetailPage and ResourcePackDetailPage
// Features: version label, tags row, info lines, download button, L3 expand

Rectangle {
    id: card

    Layout.fillWidth: true
    implicitHeight: Math.max(52, mainRow.implicitHeight + 16)
    radius: 8
    color: cardHov.containsMouse ? "#161a26" : "#11141c"
    border.color: cardHov.containsMouse ? "#4068c8" : "#1e2230"
    border.width: cardHov.containsMouse ? 1.5 : 1

    // ── Opacity fade-in entrance ──
    opacity: 0
    Component.onCompleted: { opacity = 1 }

    // ── Elastic click feedback ──
    property real _clickScale: 1.0
    scale: _clickScale
    Timer { id: clickRestoreTimer; interval: 100
        onTriggered: { card._clickScale = 1.0 }
    }

    // ── Data properties ──
    property string versionLabel: ""        // version number string
    property var tags: []                  // [{text, color, bg}] — loader/prerelease tags
    property var infoLines: []             // [{label, value}] — info rows below version
    property bool hasDownload: false       // show download button
    property string downloadLabel: "下载"   // button text (Mod=下载, RP=安装)

    // ── L3 expand (RP detail) ──
    property bool showExpand: false        // show expand toggle
    property bool expanded: false          // current expand state
    property var l3Detail: null            // expanded content data

    signal downloadClicked()
    signal expandToggled()

    // ── Animations ──
    Behavior on color { ColorAnimation { duration: 200 } }
    Behavior on border.color { ColorAnimation { duration: 200 } }
    Behavior on border.width { NumberAnimation { duration: 150 } }
    Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
    Behavior on implicitHeight { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
    Behavior on _clickScale {
        SpringAnimation { spring: 1.8; damping: 0.25; epsilon: 0.01 }
    }

    // ── Hover gradient overlay ──
    Rectangle {
        anchors.fill: parent; radius: parent.radius
        opacity: cardHov.containsMouse ? 0.12 : 0
        gradient: Gradient {
            GradientStop { position: 0; color: "#5068c8" }
            GradientStop { position: 1; color: "#6080d8" }
        }
        Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
    }

    MouseArea {
        id: cardHov
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: hasDownload ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: {
            if (hasDownload) {
                card._clickScale = 0.92
                clickRestoreTimer.restart()
                downloadClicked()
            }
            if (showExpand) {
                expandToggled()
            }
        }
    }

    // ── Layout ──
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 4

        // Main row: version label + tags + download button
        RowLayout {
            id: mainRow
            Layout.fillWidth: true
            spacing: 6

            // Version label
            Text {
                text: card.versionLabel
                color: "#d0d4e0"
                font.pixelSize: 12; font.weight: Font.DemiBold
                Layout.alignment: Qt.AlignVCenter
            }

            // Tags
            Repeater {
                model: card.tags
                delegate: Rectangle {
                    id: tagRect
                    width: tagText.implicitWidth + 8; height: 16
                    radius: 3; color: modelData.bg || "#1a2848"
                    Text {
                        id: tagText
                        anchors.centerIn: parent
                        text: modelData.text
                        color: modelData.color || "#8aaeff"
                        font.pixelSize: 8
                    }
                }
            }

            Item { Layout.fillWidth: true }

            // Download button
            Rectangle {
                id: dlBtn
                visible: card.hasDownload
                implicitWidth: Math.max(dlBtnText.implicitWidth + 20, 56)
                implicitHeight: 28; radius: 6
                color: dlBtnHov.containsMouse ? "#3a5ed0" : "#2a4590"

                property real _btnScale: 1.0
                scale: _btnScale
                Behavior on color { ColorAnimation { duration: 150 } }
                Behavior on _btnScale {
                    SpringAnimation { spring: 2.0; damping: 0.3; epsilon: 0.01 }
                }

                Text {
                    id: dlBtnText
                    anchors.centerIn: parent
                    text: card.downloadLabel
                    color: "#e8ecf8"
                    font.pixelSize: 12; font.weight: Font.DemiBold
                }
                MouseArea {
                    id: dlBtnHov
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: function(mouse) {
                        mouse.accepted = true      // prevent card click
                        dlBtn._btnScale = 0.92
                        dlBtnScaleRestore.restart()
                        card.downloadClicked()
                    }
                    Timer { id: dlBtnScaleRestore; interval: 100
                        onTriggered: { dlBtn._btnScale = 1.0 }
                    }
                }
            }
        }

        // Info lines (e.g. "MC: 1.21, 1.21.1", "2024-01-15  |  下载量 12.5K")
        Repeater {
            model: card.infoLines
            delegate: RowLayout {
                Layout.fillWidth: true
                spacing: 3
                Text {
                    text: modelData.label + " "
                    color: "#606478"; font.pixelSize: 9
                    visible: modelData.label !== ""
                }
                Text {
                    text: modelData.value
                    color: "#788090"; font.pixelSize: 9
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
            }
        }
    }

    // ── L3 Detail (expandable, for RP) ──
    // Injected via default property alias from parent page
    // Usage: DetailVersionCard { ... L3Content { ... } }
    default property alias expandedContent: l3Container.data
    Item {
        id: l3Container
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: mainRow.bottom
        anchors.topMargin: 8
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        visible: card.showExpand && card.expanded
        height: visible ? l3Container.implicitHeight : 0
        Behavior on height { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
    }
}
