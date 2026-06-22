import QtQuick

Rectangle {
    id: root
    property url skinSource: ""
    width: 48; height: 48
    color: "transparent"

    // ── Google Material Spinner ──
    // 3-phase cycle: fast grow → slow grow → snap to dot
    property real _spinnerAngle: 0
    property real _spinnerArcLen: 10
    property bool _spinning: img.status !== Image.Ready
        || _spinnerMinVisible
        || img.status === Image.Loading
        || img.status === Image.Error
        || (img.status === Image.Null && root.skinSource.toString())
    property bool _spinnerMinVisible: false
    Timer { id: spinnerMinTimer; interval: 300; onTriggered: _spinnerMinVisible = false }

    // Continuous rotation
    RotationAnimation on _spinnerAngle {
        running: root._spinning
        from: 0; to: 360; duration: 2000; loops: Animation.Infinite
        easing.type: Easing.Linear
    }

    // 4-phase cycle: stretch → crawl → catch-up (tail chases head)
    SequentialAnimation on _spinnerArcLen {
        running: root._spinning
        loops: Animation.Infinite
        NumberAnimation { from: 5;  to: 280; duration: 700; easing.type: Easing.OutCubic  }  // stretch
        NumberAnimation { from: 280; to: 340; duration: 900; easing.type: Easing.InQuart   }  // slow crawl
        NumberAnimation { from: 340; to: 5;   duration: 400; easing.type: Easing.OutExpo   }  // tail catches head
    }

    Canvas {
        id: spinner
        anchors.fill: parent
        visible: root._spinning
        onPaint: {
            var ctx = getContext("2d")
            var cw = width, ch = height
            ctx.clearRect(0, 0, cw, ch)
            var cx = cw / 2, cy = ch / 2, r = Math.min(cx, cy) - 5
            ctx.strokeStyle = "#5b8def"
            ctx.lineWidth = 2.5
            ctx.lineCap = "round"

            // Head leads, arc stretches forward in rotation direction
            var head = root._spinnerAngle * Math.PI / 180
            var tail = head + (root._spinnerArcLen * Math.PI / 180)   // tail trails FORWARD
            ctx.beginPath()
            ctx.arc(cx, cy, r, head, tail)
            ctx.stroke()
        }
    }
    Connections {
        target: root
        function on_SpinnerAngleChanged() { spinner.requestPaint() }
        function on_SpinnerArcLenChanged() { spinner.requestPaint() }
        function on_SpinningChanged() { if (root._spinning) spinner.requestPaint() }
    }

    // ── Face Image (pre-cropped by C++ to 128x128, file:// URL) ──
    Image {
        id: img
        anchors.fill: parent
        source: root.skinSource
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        cache: false
        visible: status === Image.Ready
        mipmap: false
        smooth: false  // keep pixel-art crisp
    }

    onSkinSourceChanged: {
        if (skinSource.toString()) {
            _spinnerMinVisible = true
            spinnerMinTimer.restart()
        }
    }


}
