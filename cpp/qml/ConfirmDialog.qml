import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: root
    anchors.fill: parent

    property string title: ""
    property string message: ""
    property var onAccept: null
    property bool opened: false

    signal closed()

    onOpenedChanged: {
        if (!opened) closed()
    }

    // Dim overlay
    Rectangle {
        anchors.fill: parent; z: 0; color: "#000000"
        opacity: 0.5
        Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
        MouseArea { anchors.fill: parent; onClicked: root.opened = false }
    }

    // Dialog box
    Rectangle {
        anchors.centerIn: parent; width: 360; height: 180; radius: 10; z: 1
        color: "#141820"; border.color: "#2a1f24"; border.width: 1
        Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

        ColumnLayout {
            anchors.fill: parent; anchors.margins: 20; spacing: 12
            Text { text: root.title; font.pixelSize: 15; font.weight: Font.Bold; color: "#e4e8f2" }
            Text { text: root.message; font.pixelSize: 12; color: "#b0b8c8"; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            Item { Layout.fillHeight: true }
            RowLayout {
                Layout.alignment: Qt.AlignRight; spacing: 10
                Rectangle {
                    width: 80; height: 32; radius: 5; color: "transparent"; border.color: "#2a2e3c"
                    scale: cancelDlgMa.pressed ? 0.9 : 1.0
                    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                    Text { anchors.centerIn: parent; text: "取消"; font.pixelSize: 12; color: "#b0b8c8" }
                    MouseArea {
                        id: cancelDlgMa
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: root.opened = false
                    }
                }
                Rectangle {
                    width: 80; height: 32; radius: 5; color: "#c05050"
                    scale: confirmDlgMa.pressed ? 0.9 : 1.0
                    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                    Text { anchors.centerIn: parent; text: "确认"; font.pixelSize: 12; color: "#e8ecf8" }
                    MouseArea {
                        id: confirmDlgMa
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.opened = false
                            if (root.onAccept) root.onAccept()
                        }
                    }
                }
            }
        }
    }
}
