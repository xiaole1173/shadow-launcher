// InputBox.qml — 通用输入框组件
// 40px 高度、radiusLg、bgSecondary 背景、焦点高亮动画
// 支持密码模式、历史下拉、校验错误态、右侧操作按钮
// 与 SearchBox 互补：SearchBox→搜索筛选，InputBox→表单输入
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

    // ── 几何 ──
    Layout.fillWidth: true
    height: 40
    radius: StyleTokens.radiusLg
    color: StyleTokens.bgSecondary

    // 边框动画
    border.color: {
        if (hasError) return "#cc5555"
        if (textInput.activeFocus) return StyleTokens.accent
        return StyleTokens.bgElevated
    }
    border.width: 1
    Behavior on border.color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }

    // ── 初始默认值 ──
    Component.onCompleted: {
        if (defaultText !== "" && text === "") text = defaultText
    }

    // ── 内部布局 ──
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 6
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

            // 占位符
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
            color: historyPopup.visible ? "#1e2840" : "transparent"
            Text {
                anchors.centerIn: parent
                text: historyPopup.visible ? "▲" : "▼"
                color: StyleTokens.textTertiary
                font.pixelSize: StyleTokens.fontSizeXs
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (historyPopup.visible) historyPopup.close()
                    else historyPopup.open()
                }
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

    // ── 历史记录弹出 ──
    Popup {
        id: historyPopup
        y: parent.height + 4
        width: parent.width
        padding: 4
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

        background: Rectangle {
            color: StyleTokens.bgSecondary
            radius: StyleTokens.radiusLg
            border.color: StyleTokens.bgElevated
        }

        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180; easing.type: Easing.OutCubic }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120 }
        }

        contentItem: ColumnLayout {
            spacing: 2
            Repeater {
                model: root.historyModel
                delegate: Rectangle {
                    id: historyItem
                    required property var modelData
                    Layout.fillWidth: true
                    height: 32
                    radius: StyleTokens.radiusSm
                    color: histRowMouse.containsMouse ? "#1a2840" : "transparent"
                    Behavior on color { ColorAnimation { duration: 100 } }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 6
                        spacing: 4

                        Text {
                            Layout.fillWidth: true
                            text: historyItem.modelData
                            color: StyleTokens.textSecondary
                            font.pixelSize: StyleTokens.fontSizeMd
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }

                        // 删除按钮
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
                                onClicked: root.historyItemDeleted(historyItem.modelData)
                            }
                        }
                    }

                    MouseArea {
                        id: histRowMouse
                        anchors.left: parent.left
                        anchors.right: delBtn.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            textInput.text = historyItem.modelData
                            historyPopup.close()
                        }
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
