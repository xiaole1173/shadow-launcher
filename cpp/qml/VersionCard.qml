import QtQuick

Rectangle {
    id: root

    implicitWidth: parent ? parent.width : 300
    height: 42
    color: "#0e1018"
    radius: 8

    property string versionId: ""
    property string versionType: ""
    property bool isSelected: false

    signal clicked()

    Text {
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left; anchors.leftMargin: 12
        text: root.versionId
        color: "#d0d4e0"
        font.pixelSize: 13
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
