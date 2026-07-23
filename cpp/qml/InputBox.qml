// InputBox.qml — 通用输入框组件
// 40px 高度、radiusLg、bgSecondary 背景、焦点高亮动画
// 支持密码模式、历史下拉内联展开（高度动画）、校验错误态、右侧操作按钮
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: root

    // ── 通用属性 ──
    property alias text: textInput.text
    property alias placeholderText: placeholder.text
    property string defaultText: ""
    property bool readOnly: false
    property bool enabled: true

    // ── 密码模式 ──
    property bool passwordMode: false

    // ── 历史下拉 ──
    property bool historyEnabled: false
    property var historyModel: []
    signal historyItemDeleted(var item)

    // ── 校验状态 ──
    property bool hasError: false
    property string errorMessage: ""

    // ── 右侧操作 ──
    property string rightIconSource: ""
    signal rightClicked()

    // ── 信号 ──
    signal accepted()
    signal textEdited(string text)

    // ── 内部状态 ──
    property bool _historyOpen: false
    readonly property int _historyItemH: 32
    readonly property int _historyMaxVisible: 5
    readonly property int _historyListH: Math.min(historyModel.length, _historyMaxVisible) * _historyItemH

    // ── 几何 ──
    Layout.fillWidth: true
    height: 40 + (_historyOpen ? _historyListH + 1 : 0)
    radius: StyleTokens.radiusLg
    color: StyleTokens.bgSecondary
    clip: true  // clip 下拉子项到圆角边界，避免尖角溢出边框
    border.width: 1
    border.color: {
        if (root.hasError) return "#cc5555"
        if (textInput.activeFocus) return StyleTokens.borderFocus
        return StyleTokens.bgElevated
    }
    Behavior on border.color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
    Behavior on height { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

    // ── 初始默认值 ──
    Component.onCompleted: {
        if (defaultText !== "" && text === "") text = defaultText
    }

    // ── 输入行 ──
    RowLayout {
        id: inputRow
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
        anchors.leftMargin: 12; anchors.rightMargin: 6
        height: 40
        spacing: 4

        TextInput {
            id: textInput
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: hasError && text !== "" ? "#ff9999" : StyleTokens.textSecondary
            font.pixelSize: StyleTokens.fontSizeMd
            verticalAlignment: TextInput.AlignVCenter
            selectByMouse: true
            readOnly: root.readOnly
            echoMode: root.passwordMode ? TextInput.Password : TextInput.Normal
            clip: true

            Keys.onReturnPressed: root.accepted()
            onTextChanged: root.textEdited(text)

            // ── 占位符 ──
            Text {
                id: placeholder
                anchors.fill: parent
                verticalAlignment: Text.AlignVCenter
                text: root.placeholderText
                color: StyleTokens.textTertiary
                font.pixelSize: StyleTokens.fontSizeMd
                visible: !textInput.text
                Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
            }
        }

        // ── 历史下拉按钮 ──
        Rectangle {
            id: historyBtn
            visible: root.historyEnabled && root.historyModel.length > 0
            width: 28; height: 28
            radius: StyleTokens.radiusSm
            color: _historyOpen ? "#1e2840" : "transparent"
            Text {
                anchors.centerIn: parent
                text: _historyOpen ? "▲" : "▼"
                color: StyleTokens.textTertiary
                font.pixelSize: StyleTokens.fontSizeXs
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: _historyOpen = !_historyOpen
            }
        }

        // ── 密码可见切换 ──
        Rectangle {
            id: eyeBtn
            visible: root.passwordMode
            width: 28; height: 28
            radius: StyleTokens.radiusSm
            color: eyeMouse.containsMouse ? "#1e2840" : "transparent"
            Behavior on color { ColorAnimation { duration: 120 } }
            Image {
                anchors.centerIn: parent
                source: "icons/lucide/eye.svg"
                width: 14; height: 14
                opacity: textInput.echoMode === TextInput.Password ? 0.4 : 1.0
                Behavior on opacity { NumberAnimation { duration: 120 } }
            }
            MouseArea {
                id: eyeMouse
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                hoverEnabled: true
                onClicked: {
                    textInput.echoMode = (textInput.echoMode === TextInput.Password)
                        ? TextInput.Normal
                        : TextInput.Password
                }
            }
        }

        // ── 右侧自定义图标 ──
        Rectangle {
            visible: root.rightIconSource !== ""
            width: 28; height: 28
            radius: StyleTokens.radiusSm
            color: rightMouse.containsMouse ? "#1e2840" : "transparent"
            Behavior on color { ColorAnimation { duration: 120 } }
            Image {
                anchors.centerIn: parent
                source: root.rightIconSource
                width: 14; height: 14
            }
            MouseArea {
                id: rightMouse
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                hoverEnabled: true
                onClicked: root.rightClicked()
            }
        }
    }

    // ── 分隔线（展开时可见）──
    Rectangle {
        id: popupSeparator
        anchors.top: inputRow.bottom
        anchors.left: parent.left; anchors.right: parent.right
        anchors.leftMargin: 8; anchors.rightMargin: 8
        height: 1
        visible: _historyOpen && _historyListH > 0
        color: StyleTokens.bgElevated
    }

    // ── 历史下拉（内联展开 + 独立 clip，不影响根边框）──
    Rectangle {
        id: dropdownArea
        anchors.top: popupSeparator.bottom
        anchors.left: parent.left; anchors.right: parent.right
        height: _historyOpen ? _historyListH : 0
        color: "transparent"
        clip: true
        Behavior on height { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

        ListView {
            id: historyList
            anchors.fill: parent
            model: root.historyModel
            interactive: false

            delegate: Rectangle {
                required property var modelData
                width: historyList.width; height: root._historyItemH
                color: selectMouse.containsMouse ? StyleTokens.bgElevated : "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12; anchors.rightMargin: 6
                    spacing: 4

                    Text {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        verticalAlignment: Text.AlignVCenter
                        text: modelData
                        color: StyleTokens.textSecondary
                        font.pixelSize: StyleTokens.fontSizeMd
                        elide: Text.ElideRight
                    }

                    Rectangle {
                        id: delBtn
                        width: 22; height: 22
                        radius: StyleTokens.radiusSm
                        color: delMouse.containsMouse
                            ? (delMouse.pressed ? "#882020" : "#551818")
                            : "transparent"
                        scale: delMouse.pressed ? 0.85 : 1.0
                        Behavior on color { ColorAnimation { duration: 120; easing.type: Easing.OutCubic } }
                        Behavior on scale { NumberAnimation { duration: 100; easing.type: Easing.OutCubic } }

                        Image {
                            anchors.centerIn: parent
                            source: "icons/lucide/x.svg"
                            width: 12; height: 12
                            opacity: delMouse.containsMouse ? 1.0 : 0.6
                        }
                        MouseArea {
                            id: delMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.historyItemDeleted(modelData)
                        }
                    }
                }

                MouseArea {
                    id: selectMouse
                    anchors.left: parent.left; anchors.leftMargin: 4
                    anchors.right: parent.right; anchors.rightMargin: 32
                    anchors.top: parent.top; anchors.bottom: parent.bottom
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        textInput.text = modelData
                        _historyOpen = false
                    }
                }
            }
        }
    }

    // ── 错误提示 ──
    Text {
        visible: root.hasError && root.errorMessage !== ""
        anchors.top: parent.bottom
        anchors.topMargin: 4
        anchors.left: parent.left
        anchors.leftMargin: 4
        text: root.errorMessage
        color: "#cc5555"
        font.pixelSize: StyleTokens.fontSizeXs
        opacity: root.hasError ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 200 } }
    }
}
