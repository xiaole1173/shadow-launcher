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

    // ── Viewport aspect ratio (matches the actual window) ──
    property real cropAR: 1.778

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

    NumberAnimation {
        id: snapAnimX; target: root; property: "_cropX"
        duration: 600; easing.type: Easing.OutBack
    }
    NumberAnimation {
        id: snapAnimY; target: root; property: "_cropY"
        duration: 600; easing.type: Easing.OutBack
    }

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

    // ── Viewport area (same aspect ratio as actual window) ──
    Item {
        id: viewport
        clip: true
        
        // Calculate size: fill available space while maintaining cropAR
        readonly property real _availW: root.width - 64
        readonly property real _availH: root.height - titleBar.height - btnBar.height - 24
        readonly property real _availAR: _availW / Math.max(_availH, 1)
        
        width: cropAR >= _availAR ? _availW : _availH * cropAR
        height: cropAR >= _availAR ? _availW / cropAR : _availH
        anchors.centerIn: parent

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

            x: (_vpW - _displayW) / 2 - (root._cropX - 0.5) * _displayW
            y: (_vpH - _displayH) / 2 - (root._cropY - 0.5) * _displayH
        }
    }  // viewport

    // Drag handler — on full overlay so mouse can travel beyond viewport bounds
    MouseArea {
        id: dragArea
        anchors.fill: parent
        hoverEnabled: true
        z: 6
        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.ArrowCursor

        onPressed: function(mouse) {
            // Only start drag if press is inside viewport
            var vpGlobal = mapToItem(root, mouse.x, mouse.y)
            var vpRect = viewport.mapToItem(root, 0, 0)
            if (vpGlobal.x < vpRect.x || vpGlobal.x > vpRect.x + viewport.width
             || vpGlobal.y < vpRect.y || vpGlobal.y > vpRect.y + viewport.height) {
                mouse.accepted = false
                return
            }
            root._dragStartX = mouse.x
            root._dragStartY = mouse.y
            root._dragCropX = root._cropX
            root._dragCropY = root._cropY
        }
        onPositionChanged: function(mouse) {
            if (!pressed) return
            var dx = mouse.x - root._dragStartX
            var dy = mouse.y - root._dragStartY
            root._cropX = root._dragCropX - dx / Math.max(_displayW, _vpW)
            root._cropY = root._dragCropY - dy / Math.max(_displayH, _vpH)
        }
        onReleased: {
            var tx = Math.max(0, Math.min(1, root._cropX))
            var ty = Math.max(0, Math.min(1, root._cropY))
            snapAnimX.from = root._cropX; snapAnimX.to = tx; snapAnimX.start()
            snapAnimY.from = root._cropY; snapAnimY.to = ty; snapAnimY.start()
        }
    }

    // Cursor-only area over viewport — shows hand cursor without consuming events
    MouseArea {
        id: vpCursor
        anchors.fill: viewport
        hoverEnabled: true
        cursorShape: Qt.OpenHandCursor
        acceptedButtons: Qt.NoButton  // never consumes press, passes to dragArea below
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
                    onClicked: {
                        snapAnimX.from = root._cropX; snapAnimX.to = 0.5; snapAnimX.start()
                        snapAnimY.from = root._cropY; snapAnimY.to = 0.5; snapAnimY.start()
                    }
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
