// SearchBox.qml — 通用搜索框组件
// 统一组件：28px 高度、bgInput 背景、activeFocus 高亮
// 支持 showIcon 图标模式 + textChanged 信号
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    property alias text: searchInput.text
    property alias placeholderText: placeholder.text
    property bool showIcon: false

    signal accepted()

    Layout.fillWidth: true
    height: 28
    radius: StyleTokens.radiusSm
    color: StyleTokens.bgInput
    border.color: searchInput.activeFocus ? StyleTokens.accentHover : StyleTokens.borderLight
    border.width: 1

    Behavior on color { ColorAnimation { duration: 200 } }
    Behavior on border.color { ColorAnimation { duration: 200 } }

    // 搜索图标
    Image {
        visible: root.showIcon
        source: "icons/lucide/search.svg"
        width: 14; height: 14
        anchors { left: parent.left; leftMargin: 10; verticalCenter: parent.verticalCenter }
    }

    TextInput {
        id: searchInput
        anchors.fill: parent
        anchors.leftMargin: root.showIcon ? 32 : 8
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
