// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick

Rectangle {
    id: root
    property url skinSource: ""
    property bool showSpinner: true
    width: 48; height: 48
    color: "transparent"

    // ── Simple spinner: continuous rotating arc ──
    property real _angle: 0
    property bool _spinning: root.showSpinner && (
        img.status !== Image.Ready
        || _spinnerMinVisible
        || img.status === Image.Loading
        || img.status === Image.Error
        || (img.status === Image.Null && root.skinSource.toString())
    )
    property bool _spinnerMinVisible: false
    Timer { id: spinnerMinTimer; interval: 300; onTriggered: _spinnerMinVisible = false }

    NumberAnimation on _angle {
        running: root._spinning
        from: 0; to: 360; duration: 1000; loops: Animation.Infinite
    }

    Canvas {
        id: spinner
        anchors.fill: parent
        visible: root._spinning

        onPaint: {
            var ctx = getContext("2d")
            var cw = width, ch = height
            ctx.clearRect(0, 0, cw, ch)

            var cx = cw / 2, cy = ch / 2, r = Math.min(cx, cy) - 4
            if (r <= 0) return

            // Draw arc from _angle to _angle + 270
            var startRad = (root._angle - 90) * Math.PI / 180
            var endRad   = (root._angle + 180) * Math.PI / 180

            ctx.strokeStyle = "#5b8def"
            ctx.lineWidth = 2.5
            ctx.lineCap = "round"

            ctx.beginPath()
            ctx.arc(cx, cy, r, startRad, endRad)
            ctx.stroke()
        }
    }

    Connections {
        target: root
        function on_AngleChanged() { spinner.requestPaint() }
        function on_SpinningChanged() {
            if (root._spinning) spinner.requestPaint()
        }
    }

    // ── Face Image ──
    Image {
        id: img
        anchors.fill: parent
        source: root.skinSource
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        cache: false
        visible: status === Image.Ready
        mipmap: false
        smooth: false
    }

    onSkinSourceChanged: {
        if (skinSource.toString()) {
            _spinnerMinVisible = true
            spinnerMinTimer.restart()
        }
    }
}
