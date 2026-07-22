// SearchBox.qml — 通用搜索框组件
// 样式：28px 高度、bgInput 背景、activeFocus 高亮边框、右侧可选图标
// 使用：
//   SearchBox {
//       id: mySearch
//       placeholderText: qsTr("搜索...")
//       onAccepted: doSearch()
//   }
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    // ── 公开属性 ──
    property alias text: searchInput.text
    property alias placeholderText: placeholder.text
    property alias input: searchInput

    // 可选右侧图标（如搜索图标），为 null 时不显示
    property var icon: null

    // ── 信号 ──
    signal accepted()           // 回车键按下
    signal textChanged(string newText)

    // ── 样式 ──
    Layout.fillWidth: true
    height: 28
    radius: StyleTokens.radiusSm
    color: StyleTokens.bgInput
    border.color: searchInput.activeFocus ? StyleTokens.accentHover : StyleTokens.borderLight
    border.width: 1

    Behavior on color { ColorAnimation { duration: 200 } }
    Behavior on border.color { ColorAnimation { duration: 200 } }

    // ── 文本输入 ──
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

        onTextChanged: root.textChanged(searchInput.text)

        // ── 占位文字 ──
        Text {
            id: placeholder
            anchors.fill: parent
            verticalAlignment: Text.AlignVCenter
            color: StyleTokens.textMuted
            font.pixelSize: StyleTokens.fontSizeSm
            visible: !searchInput.text
        }
    }

    // ── 可选右侧图标（如搜索图标） ──
    Loader {
        anchors.right: parent.right
        anchors.rightMargin: 4
        anchors.verticalCenter: parent.verticalCenter
        sourceComponent: iconComponent
        active: icon !== null
    }

    Component {
        id: iconComponent
        Rectangle {
            width: 20; height: 20; radius: 4
            color: "transparent"
            Text {
                anchors.centerIn: parent
                text: root.icon || ""
                color: StyleTokens.textMuted
                font.pixelSize: StyleTokens.fontSizeMd
            }
        }
    }
}
