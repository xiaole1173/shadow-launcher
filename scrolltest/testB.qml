import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 变体 B：有 height:max，无 Binding（909528e 之前的滚动行为）
Window {
    id: win
    width: 500
    height: 400
    visible: true
    title: "B: height=max only"

    ScrollView {
        id: sv
        anchors.fill: parent
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        ColumnLayout {
            id: col
            width: sv.availableWidth
            height: Math.max(sv.availableHeight, implicitHeight)
            spacing: 12
            Repeater {
                model: 30
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 50
                    color: Qt.hsla(index / 30, 0.6, 0.5, 1)
                    Text { text: "item " + index; anchors.centerIn: parent; color: "white" }
                }
            }
        }
    }

    Timer {
        interval: 800
        repeat: true
        running: true
        onTriggered: {
            var f = sv.contentItem
            console.log("T viewport=" + f.height + " contentHeight=" + f.contentHeight
                        + " maxY=" + Math.max(0, f.contentHeight - f.height)
                        + " colH=" + col.height + " colImplicit=" + col.implicitHeight
                        + " sbVisible=" + sv.ScrollBar.vertical.visible
                        + " sbW=" + sv.ScrollBar.vertical.width)
            f.contentY = 1e9
        }
    }
}
