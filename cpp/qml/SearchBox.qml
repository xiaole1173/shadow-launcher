// SearchBox.qml — 通用搜索框组件
// 最简版本：28px 高度、bgInput 背景、activeFocus 高亮
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    property alias text: searchInput.text
    property alias placeholderText: placeholder.text

    signal accepted()

    Layout.fillWidth: true
    height: 28
    radius: StyleTokens.radiusSm
    color: StyleTokens.bgInput
    border.color: searchInput.activeFocus ? StyleTokens.accentHover : StyleTokens.borderLight
    border.width: 1

    Behavior on color { ColorAnimation { duration: 200 } }
    Behavior on border.color { ColorAnimation { duration: 200 } }

    TextInput {
        id: searchInput
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        color: StyleTokens.textPrimary
        verticalAlignment: TextInput.AlignVCenter
        font.pixelSize: StyleTokens.fontSizeSm
        selectByMouse: true

        Keys.onReturnPressed: root.accepted()

        Text {
            id: placeholder
            anchors.fill: parent
            verticalAlignment: Text.AlignVCenter
            color: StyleTokens.textMuted
            font.pixelSize: StyleTokens.fontSizeSm
            visible: !searchInput.text
        }
    }
}
