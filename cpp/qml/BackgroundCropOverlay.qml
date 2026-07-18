// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
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
    property real _zoom: 1.0  // viewport zoom factor

    // JS-driven snap-back (bypasses Qt animation unreliability)
    property real _snapFromX: 0; property real _snapToX: 0; property real _snapT0X: 0
    property real _snapFromY: 0; property real _snapToY: 0; property real _snapT0Y: 0
    property bool _snappingX: false; property bool _snappingY: false
    readonly property real _snapDuration: 500

    Timer {
        id: snapDriver
        interval: 16; repeat: false
        onTriggered: {
            var now = Date.now()
            var active = false
            if (_snappingX) {
                var t = Math.min(1, (now - _snapT0X) / _snapDuration)
                root._cropX = _snapFromX + (_snapToX - _snapFromX) * (1 - (1 - t) * (1 - t) * (1 - t))
                if (t >= 1) { root._cropX = _snapToX; _snappingX = false }
                else active = true
            }
            if (_snappingY) {
                var tY = Math.min(1, (now - _snapT0Y) / _snapDuration)
                root._cropY = _snapFromY + (_snapToY - _snapFromY) * (1 - (1 - tY) * (1 - tY) * (1 - tY))
                if (tY >= 1) { root._cropY = _snapToY; _snappingY = false }
                else active = true
            }
            if (active) snapDriver.restart()
        }
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
        height: 48; color: StyleTokens.bgPrimary
        z: 5

        RowLayout {
            anchors { fill: parent; leftMargin: 16; rightMargin: 8 }
            Text {
                text: "调整背景位置"; font.pixelSize: StyleTokens.fontSizeLg; font.weight: Font.DemiBold; color: StyleTokens.textPrimary
            }
            Item { Layout.fillWidth: true }
            Text {
                text: "拖拽图片选择可视区域"
                font.pixelSize: StyleTokens.fontSizeSm; color: "#606480"
            }
            Item { Layout.fillWidth: true }
            Rectangle {
                width: 32; height: 32; radius: 16; color: closeHov.hovered ? "#282838" : "transparent"
                Text { anchors.centerIn: parent; text: "\u2715"; font.pixelSize: StyleTokens.fontSizeLg; color: StyleTokens.textTertiary }
                MouseArea {
                    id: closeHov; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: { root.opacity = 0; closeTimer.start() }
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
            anchors.fill: parent; color: StyleTokens.bgPrimary
            // Dimmed area indicator (outside viewport)
            border.color: "#2a2a3a"; border.width: 1
        }

        // The full image, displayed at crop scale
        Image {
            id: bgFull
            source: backend ? (backend.customBgPath || "") : ""
            fillMode: Image.PreserveAspectFit
            transformOrigin: Item.TopLeft
            scale: root._scale * root._zoom
            asynchronous: true; cache: false

            x: _overX > 0 ? -_overX * root._cropX : -_displayW * (root._cropX - 0.5)
            y: _overY > 0 ? -_overY * root._cropY : -_displayH * (root._cropY - 0.5)
        }
    }  // viewport

    // Drag handler — on full overlay so mouse can travel beyond viewport bounds
    MouseArea {
        id: dragArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.ArrowCursor

        onPressed: function(mouse) {
            // Ignore clicks in title bar or button bar
            var tbGlobal = titleBar.mapToItem(root, 0, 0)
            var bbGlobal = btnBar.mapToItem(root, 0, 0)
            if (mouse.y < tbGlobal.y + titleBar.height || mouse.y > bbGlobal.y) {
                mouse.accepted = false; return
            }
            var vpGlobal = mapToItem(root, mouse.x, mouse.y)
            var vpRect = viewport.mapToItem(root, 0, 0)
            if (vpGlobal.x < vpRect.x || vpGlobal.x > vpRect.x + viewport.width
             || vpGlobal.y < vpRect.y || vpGlobal.y > vpRect.y + viewport.height) {
                mouse.accepted = false
                return
            }
            _snappingX = false; _snappingY = false; snapDriver.stop()
            root._dragStartX = mouse.x
            root._dragStartY = mouse.y
            root._dragCropX = root._cropX
            root._dragCropY = root._cropY
        }
        onPositionChanged: function(mouse) {
            if (!pressed) return
            var dx = mouse.x - root._dragStartX
            var dy = mouse.y - root._dragStartY
            root._cropX = root._dragCropX - dx / Math.max(_vpW, 1)
            root._cropY = root._dragCropY - dy / Math.max(_vpH, 1)
        }
        onReleased: {
            var tx = (_overX > 0) ? Math.max(0, Math.min(1, root._cropX)) : 0.5
            var ty = (_overY > 0) ? Math.max(0, Math.min(1, root._cropY)) : 0.5
            var now = Date.now()
            if (Math.abs(root._cropX - tx) > 0.001) {
                _snapFromX = root._cropX; _snapToX = tx; _snapT0X = now; _snappingX = true
            }
            if (Math.abs(root._cropY - ty) > 0.001) {
                _snapFromY = root._cropY; _snapToY = ty; _snapT0Y = now; _snappingY = true
            }
            if (_snappingX || _snappingY) snapDriver.restart()
        }
    }

    // Cursor-only area over viewport — shows hand cursor without consuming events
    MouseArea {
        id: vpCursor
        anchors.fill: viewport
        hoverEnabled: true
        cursorShape: Qt.OpenHandCursor
        acceptedButtons: Qt.NoButton
        onWheel: function(wheel) {
            root._zoom = Math.max(0.5, Math.min(3.0, root._zoom - wheel.angleDelta.y / 600))
        }  // never consumes press, passes to dragArea below
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
                width: 100; height: 34; radius: StyleTokens.radiusMd
                color: resetHov.hovered ? "#252a38" : "#161a24"; border.color: StyleTokens.bgHover
                Text {
                    anchors.centerIn: parent; text: "重置居中"
                    font.pixelSize: StyleTokens.fontSizeSm; color: "#b0b8c8"
                }
                MouseArea {
                    id: resetHov; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        var now = Date.now()
                        if (Math.abs(root._cropX - 0.5) > 0.001) {
                            _snapFromX = root._cropX; _snapToX = 0.5; _snapT0X = now; _snappingX = true
                        }
                        if (Math.abs(root._cropY - 0.5) > 0.001) {
                            _snapFromY = root._cropY; _snapToY = 0.5; _snapT0Y = now; _snappingY = true
                        }
                        if (_snappingX || _snappingY) snapDriver.restart()
                    }
                }
            }

            // Zoom controls
            Rectangle {
                width: 28; height: 28; radius: StyleTokens.radiusMd
                color: zoomOutHov.hovered ? "#252a38" : "#161a24"; border.color: StyleTokens.bgHover
                Text { anchors.centerIn: parent; text: "−"; font.pixelSize: StyleTokens.fontSizeMd; color: "#b0b8c8" }
                MouseArea {
                    id: zoomOutHov; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root._zoom = Math.max(0.5, root._zoom - 0.25)
                }
            }
            Text {
                text: Math.round(root._zoom * 100) + "%"
                font.pixelSize: StyleTokens.fontSizeSm; color: "#707890"
                Layout.preferredWidth: 36; horizontalAlignment: Text.AlignHCenter
            }
            Rectangle {
                width: 28; height: 28; radius: StyleTokens.radiusMd
                color: zoomInHov.hovered ? "#252a38" : "#161a24"; border.color: StyleTokens.bgHover
                Text { anchors.centerIn: parent; text: "+"; font.pixelSize: StyleTokens.fontSizeMd; color: "#b0b8c8" }
                MouseArea {
                    id: zoomInHov; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root._zoom = Math.min(3.0, root._zoom + 0.25)
                }
            }

            // Cancel button
            Rectangle {
                width: 80; height: 34; radius: StyleTokens.radiusMd
                color: cancelHov.hovered ? "#302020" : "#161a24"; border.color: StyleTokens.bgHover
                Text {
                    anchors.centerIn: parent; text: "取消"
                    font.pixelSize: StyleTokens.fontSizeSm; color: "#d08080"
                }
                MouseArea {
                    id: cancelHov; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: { root.opacity = 0; closeTimer.start() }
                }
            }

            // Confirm button
            Rectangle {
                width: 80; height: 34; radius: StyleTokens.radiusMd
                color: confirmHov.hovered ? "#2a3a68" : "#192650"; border.color: "#304898"
                Text {
                    anchors.centerIn: parent; text: "确认"
                    font.pixelSize: StyleTokens.fontSizeSm; color: "#70a0ff"; font.weight: Font.DemiBold
                }
                MouseArea {
                    id: confirmHov; anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        backend.updateCrop(root._cropX, root._cropY)
                        root.opacity = 0
                        closeTimer.start()
                    }
                }
            }
        }
    }

    Timer {
        id: closeTimer
        interval: 250
        onTriggered: root.closed()
    }
}
