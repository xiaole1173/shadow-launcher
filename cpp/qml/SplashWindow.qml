import QtQuick

Rectangle {
    anchors.fill: parent
    color: "#0c0f16"

    Item {
        anchors.centerIn: parent
        width: 280; height: 80

        // Loading bar — thin shimmer effect
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width; height: 2
            radius: 1
            color: "#1e2433"

            Rectangle {
                height: parent.height; width: parent.width * 0.35; radius: 1
                color: "#3B82F6"
                SequentialAnimation on x {
                    running: true; loops: Animation.Infinite
                    NumberAnimation { from: 0; to: 280 * 0.65; duration: 1800; easing.type: Easing.InOutCubic }
                    NumberAnimation { from: 280 * 0.65; to: 0; duration: 1800; easing.type: Easing.InOutCubic }
                }
            }
        }

        // App name
        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom; anchors.bottomMargin: 12
            spacing: 6
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Shadow Launcher"
                font.pixelSize: 22; font.bold: true
                color: "#e0e6f0"
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("正在启动...")
                font.pixelSize: 11
                color: "#5a647a"
            }
        }
    }
}
