// Test font.family on Item
import QtQuick

Item {
    width: 200; height: 200
    font.family: "Arial"
    
    Text {
        anchors.centerIn: parent
        text: "Test"
        font.pixelSize: 20
    }
}
