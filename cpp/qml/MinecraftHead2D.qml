import QtQuick

// Avatar with spinner during load. Uses both Image (reliable display)
// and Canvas (planned pixel-art face rendering, WIP).
Rectangle {
    id: root
    property url skinSource: ""
    width: 48; height: 48
    color: "#1e2430"

    // ── Spinner ring ──
    property real _angle: 0
    RotationAnimation on _angle {
        running: img.status === Image.Loading || img.status === Image.Null
        from: 0; to: 360; duration: 1000; loops: Animation.Infinite
    }

    Canvas {
        id: spinner
        anchors.fill: parent
        visible: img.status === Image.Loading || img.status === Image.Null
        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            if (img.status === Image.Ready) return

            var cx = width/2, cy = height/2, r = Math.min(cx, cy) - 5
            ctx.strokeStyle = "#5b8def"
            ctx.lineWidth = 3; ctx.lineCap = "round"
            ctx.beginPath()
            var a = root._angle * Math.PI / 180
            ctx.arc(cx, cy, r - 2, a, a + Math.PI * 1.3)
            ctx.stroke()
        }
    }
    Connections {
        target: root
        function on_SangleChanged() { spinner.requestPaint() }
    }

    // ── Reliable image display (proof the URL arrives) ──
    Image {
        id: img
        anchors.fill: parent
        source: root.skinSource
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        cache: false
        visible: status === Image.Ready
    }
}
