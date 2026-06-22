import QtQuick

Rectangle {
    id: root
    property url skinSource: ""
    width: 48; height: 48
    color: "transparent"

    // ── Google Material Spinner (stroke-dasharray method) ──
    property real _dashOffset: 0
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

    // Rotation: dashOffset decreases → visible arc moves clockwise on screen
    NumberAnimation on _dashOffset {
        running: root._spinning
        from: 0; to: -_circumference; duration: 2400; loops: Animation.Infinite
    }

    // 3-phase: stretch (fast) → crawl (slow) → catch-up (tail chases)
    SequentialAnimation on _dashLen {
        running: root._spinning
        loops: Animation.Infinite
        NumberAnimation { from: 0.01; to: _circumference * 0.62; duration: 700; easing.type: Easing.OutCubic }
        NumberAnimation { from: _circumference * 0.62; to: _circumference * 0.80; duration: 900; easing.type: Easing.InQuart }
        NumberAnimation { from: _circumference * 0.80; to: 0.01; duration: 800; easing.type: Easing.OutExpo }
    }

    Canvas {
        id: spinner
        anchors.fill: parent
        visible: root._spinning

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
            ctx.lineDashOffset = root._dashOffset

            ctx.beginPath()
            ctx.arc(cx, cy, r, 0, Math.PI * 2)
            ctx.stroke()
        }
    }

    Connections {
        target: root
        function on_DashOffsetChanged() { spinner.requestPaint() }
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
