import QtQuick

Rectangle {
    id: root
    property url skinSource: ""
    width: 48; height: 48
    color: "transparent"

    // ── Google Material Spinner ──
    // Two LAYERS (no conflicts):
    //   Layer 1 (GPU): Item + RotationAnimator → continuous clockwise rotation
    //   Layer 2 (CPU): Canvas + SequentialAnimation → dash stretch/hold/snap
    property real _dashLen: 0.01
    property real _circumference: 0

    property bool _spinning: img.status !== Image.Ready
        || _spinnerMinVisible
        || img.status === Image.Loading
        || img.status === Image.Error
        || (img.status === Image.Null && root.skinSource.toString())
    property bool _spinnerMinVisible: false
    Timer { id: spinnerMinTimer; interval: 300; onTriggered: _spinnerMinVisible = false }

    Component.onCompleted: {
        var r = Math.min(width, height) / 2 - 5
        _circumference = 2 * Math.PI * r
    }

    // ── Layer 1: GPU rotation (render thread, not main thread) ──
    Item {
        id: rotator
        anchors.fill: parent
        rotation: 0

        RotationAnimator on rotation {
            running: root._spinning
            from: 0; to: 360; duration: 2000; loops: Animation.Infinite
            direction: RotationAnimator.Clockwise
        }

        // ── Layer 2: dash animation on Canvas ──
        Canvas {
            id: spinner
            anchors.fill: parent
            visible: root._spinning

            // Dash cycle: stretch 0→44% in 700ms, hold 800ms, loop-snap = catch-up
            SequentialAnimation on _dashLen {
                running: root._spinning
                loops: Animation.Infinite
                NumberAnimation { from: 0.01; to: root._circumference * 0.44; duration: 700; easing.type: Easing.OutCubic }
                PauseAnimation { duration: 800 }
            }

            onPaint: {
                var ctx = getContext("2d")
                var cw = width, ch = height
                ctx.clearRect(0, 0, cw, ch)
                if (root._circumference <= 0) return

                var cx = cw / 2, cy = ch / 2, r = Math.min(cx, cy) - 5
                ctx.strokeStyle = "#5b8def"
                ctx.lineWidth = 2.5
                ctx.lineCap = "round"

                ctx.setLineDash([root._dashLen, root._circumference - root._dashLen])
                ctx.lineDashOffset = 0

                ctx.beginPath()
                ctx.arc(cx, cy, r, 0, Math.PI * 2)
                ctx.stroke()
            }
        }
    }

    Connections {
        target: root
        function on_DashLenChanged() { spinner.requestPaint() }
        function on_SpinningChanged() { if (root._spinning) spinner.requestPaint() }
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
