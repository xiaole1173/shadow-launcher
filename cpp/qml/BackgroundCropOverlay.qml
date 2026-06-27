import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

// BackgroundCropOverlay
// Full-screen overlay for adjusting background image position
// User drags the image to choose which area is visible

Rectangle {
    id: root
    color: "#000000"; opacity: 0
    Behavior on opacity { NumberAnimation { duration: 200 } }

    property bool _started: false
    signal closed()

    Component.onCompleted: {
        // Copy crop to local state
        _cropX = (backend && typeof backend.cropX === "number") ? backend.cropX : 0.5
        _cropY = (backend && typeof backend.cropY === "number") ? backend.cropY : 0.5
        opacity = 1
        _started = true
    }

    // ── Local crop state ──
    property real _cropX: 0.5
    property real _cropY: 0.5

    // ── Drag state ──
    property real _dragStartX: 0
    property real _dragStartY: 0
    property real _dragCropX: 0
    property real _dragCropY: 0

    // ── Image dimensions ──
    readonly property real _imgW: bgFull.implicitWidth || 1
    readonly property real _imgH: bgFull.implicitHeight || 1
    readonly property real _vpW: viewport.width
    readonly property real _vpH: viewport.height
    readonly property real _scale: Math.max(_vpW / _imgW, _vpH / _imgH)
    readonly property real _displayW: _imgW * _scale
    readonly property real _displayH: _imgH * _scale
    readonly property real _overX: Math.max(0, _displayW - _vpW)
    readonly property real _overY: Math.max(0, _displayH - _vpH)

    // ── Title bar ──
    Rectangle {
        id: titleBar
        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: 48; color: "#0c0f16"
        z: 5

        RowLayout {
            anchors { fill: parent; leftMargin: 16; rightMargin: 8 }
            Text {
                text: "调整背景位置"; font.pixelSize: 15; font.weight: Font.DemiBold; color: "#e8ecf8"
            }
            Item { Layout.fillWidth: true }
            Text {
                text: "拖拽图片选择可视区域"
                font.pixelSize: 11; color: "#606480"
            }
            Item { Layout.fillWidth: true }
            Rectangle {
                width: 32; height: 32; radius: 16; color: closeHov.hovered ? "#282838" : "transparent"
                Text { anchors.centerIn: parent; text: "\u2715"; font.pixelSize: 16; color: "#8890a8" }
                MouseArea {
                    id: closeHov; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: { root.opacity = 0; root.closed() }
                }
            }
        }
    }

    // ── Viewport area (where the background will be shown) ──
    Item {
        id: viewport
        anchors {
            top: titleBar.bottom; bottom: btnBar.top
            left: parent.left; right: parent.right
            margins: 32
        }
        clip: true

        // Checkerboard behind the image (to show transparent/cropped areas)
        Rectangle {
            anchors.fill: parent; color: "#0a0a12"
            // Dimmed area indicator (outside viewport)
            border.color: "#2a2a3a"; border.width: 1
        }

        // The full image, displayed at crop scale
        Image {
            id: bgFull
            source: backend ? (backend.customBgPath || "") : ""
            fillMode: Image.PreserveAspectFit
            transformOrigin: Item.TopLeft
            scale: root._scale
            asynchronous: true; cache: false

            x: (_vpW - _displayW) / 2 + _overX * (0.5 - root._cropX)
            y: (_vpH - _displayH) / 2 + _overY * (0.5 - root._cropY)

            // Drag handler
            TapHandler {
                id: dragHandler
                acceptedButtons: Qt.LeftButton
                onPressedChanged: {
                    if (pressed) {
                        root._dragStartX = point.position.x
                        root._dragStartY = point.position.y
                        root._dragCropX = root._cropX
                        root._dragCropY = root._cropY
                    }
                }
                onPointChanged: {
                    if (!pressed) return
                    var dx = point.position.x - root._dragStartX
                    var dy = point.position.y - root._dragStartY
                    if (_overX > 0)
                        root._cropX = Math.max(0, Math.min(1, root._dragCropX - dx / _overX))
                    if (_overY > 0)
                        root._cropY = Math.max(0, Math.min(1, root._dragCropY - dy / _overY))
                }
            }
        }
    }

    // ── Bottom button bar ──
    Rectangle {
        id: btnBar
        anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
        height: 56; color: "#0c0f16"; z: 5

        RowLayout {
            anchors.centerIn: parent; spacing: 12

            // Reset button
            Rectangle {
                width: 100; height: 34; radius: 6
                color: resetHov.hovered ? "#252a38" : "#161a24"; border.color: "#2a2e3c"
                Text {
                    anchors.centerIn: parent; text: "重置居中"
                    font.pixelSize: 12; color: "#b0b8c8"
                }
                MouseArea {
                    id: resetHov; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: { root._cropX = 0.5; root._cropY = 0.5 }
                }
            }

            // Cancel button
            Rectangle {
                width: 80; height: 34; radius: 6
                color: cancelHov.hovered ? "#302020" : "#161a24"; border.color: "#2a2e3c"
                Text {
                    anchors.centerIn: parent; text: "取消"
                    font.pixelSize: 12; color: "#d08080"
                }
                MouseArea {
                    id: cancelHov; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: { root.opacity = 0; root.closed() }
                }
            }

            // Confirm button
            Rectangle {
                width: 80; height: 34; radius: 6
                color: confirmHov.hovered ? "#2a3a68" : "#192650"; border.color: "#304898"
                Text {
                    anchors.centerIn: parent; text: "确认"
                    font.pixelSize: 12; color: "#70a0ff"; font.weight: Font.DemiBold
                }
                MouseArea {
                    id: confirmHov; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (backend) {
                            backend.setCropX(root._cropX)
                            backend.setCropY(root._cropY)
                        }
                        root.opacity = 0
                        root.closed()
                    }
                }
            }
        }
    }
}
