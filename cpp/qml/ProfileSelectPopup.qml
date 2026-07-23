import QtQuick
import QtQuick.Layouts

Item {
    id: root
    anchors.fill: parent

    property bool opened: false

    signal accepted(int profileIndex)
    signal cancelled()

    Rectangle {
        anchors.fill: parent; z: 100
        color: "#80000000"
        opacity: root.opened ? 1 : 0
        visible: root.opened || opacity > 0
        Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
        MouseArea { anchors.fill: parent }
    }

    Rectangle {
        id: card; z: 101
        width: 360
        height: Math.min(48 + 1 + 8 + Math.max(col.implicitHeight + 8, 140), parent ? parent.height - 80 : 600)
        anchors.centerIn: parent
        radius: 12
        color: "#11141c"
        border { color: "#2a3040"; width: 1 }

        scale: root.opened ? 1 : 0.9
        opacity: root.opened ? 1 : 0
        Behavior on scale { NumberAnimation { duration: 350; easing.type: Easing.OutBack } }
        Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

        Rectangle {
            width: parent.width; height: 48
            color: "transparent"

            Text {
                anchors { left: parent.left; leftMargin: 16; verticalCenter: parent.verticalCenter }
                text: qsTr("选择角色")
                color: "#e8ecf8"
                font { pixelSize: 16; weight: Font.DemiBold }
            }

            Rectangle {
                anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
                width: 32; height: 32; radius: 6
                color: xarea.containsMouse ? "#2a3040" : "transparent"
                Behavior on color { ColorAnimation { duration: 120 } }
                Text {
                    anchors.centerIn: parent
                    text: "\u2715"; color: "#8088a0"; font.pixelSize: 16
                }
                MouseArea {
                    id: xarea
                    anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.cancelled()
                }
            }
        }

        Rectangle {
            anchors.top: parent.top; anchors.topMargin: 48
            width: parent.width; height: 1
            color: "#2a3040"
        }

        Item {
            id: clipArea
            anchors { top: parent.top; topMargin: 48 + 1 + 8; left: parent.left; right: parent.right; bottom: parent.bottom; bottomMargin: 8 }
            clip: true

            Column {
                id: col
                width: parent.width
                spacing: 2
                topPadding: 4
                bottomPadding: 4

                // 诊断
                Rectangle {
                    width: parent.width; height: 20
                    color: "#2a3040"; radius: 4
                    Text {
                        anchors.centerIn: parent
                        color: "#ffaa00"
                        font.pixelSize: 10; font.family: "monospace"
                        text: "[D] bk=" + (typeof backend != "undefined" && backend ? "1" : "0")
                              + " yg=" + (backend && backend.yggdrasil ? "1" : "0")
                              + " pr=" + (backend && backend.yggdrasil ? backend.yggdrasil.profiles.length : "?")
                    }
                }

                Repeater {
                    id: profRep
                    model: backend && backend.yggdrasil ? backend.yggdrasil.profiles : []

                    delegate: Rectangle {
                        width: parent ? parent.width : 360
                        height: 44
                        radius: 6

                        // 默认可见底色
                        color: ma.containsMouse ? "#2a3040" : "#1a1f2e"
                        Behavior on color { ColorAnimation { duration: 100 } }
                        border { color: index === (backend && backend.yggdrasil ? backend.yggdrasil.profileIndex : -1)
                                 ? "#3b82f6" : "transparent"; width: 1 }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12; anchors.rightMargin: 12
                            spacing: 10

                            Rectangle {
                                Layout.preferredWidth: 28; Layout.preferredHeight: 28; radius: 6
                                color: "#0e1018"; border { color: "#2a3040"; width: 1 }
                                Text {
                                    anchors.centerIn: parent
                                    text: (typeof modelData != "undefined" && modelData && modelData.name)
                                          ? modelData.name.charAt(0).toUpperCase() : "?"
                                    color: "#a8b0c0"
                                    font { pixelSize: 14; weight: Font.Bold }
                                }
                            }

                            Text {
                                text: (typeof modelData != "undefined" && modelData && modelData.name)
                                      ? modelData.name : ""
                                color: "#e8ecf8"
                                font.pixelSize: 14
                                Layout.fillWidth: true; elide: Text.ElideRight
                            }

                            Rectangle {
                                width: 8; height: 8; radius: 4
                                visible: index === (backend && backend.yggdrasil ? backend.yggdrasil.profileIndex : -1)
                                color: "#3b82f6"
                            }
                        }

                        MouseArea {
                            id: ma; anchors.fill: parent; hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (backend && backend.yggdrasil) {
                                    backend.yggdrasil.selectProfile(index)
                                    root.accepted(index)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
