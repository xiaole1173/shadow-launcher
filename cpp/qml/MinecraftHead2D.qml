import QtQuick

Rectangle {
    id: root
    property url skinSource: ""
    width: 48; height: 48
    color: "transparent"

    // ── Spinner ──
    property real _angle: 0
    RotationAnimation on _angle {
        running: img.status === Image.Loading || (img.status === Image.Null && root.skinSource.toString())
        from: 0; to: 360; duration: 1000; loops: Animation.Infinite
    }

    Canvas {
        id: spinner
        anchors.fill: parent
        visible: img.status !== Image.Ready
        onPaint: {
            var ctx = getContext("2d"), cw = width, ch = height
            ctx.clearRect(0, 0, cw, ch)
            if (img.status === Image.Ready) return
            var cx = cw / 2, cy = ch / 2, r = Math.min(cx, cy) - 5
            ctx.strokeStyle = "#5b8def"
            ctx.lineWidth = 3; ctx.lineCap = "round"
            ctx.beginPath()
            ctx.arc(cx, cy, r - 2, root._angle * Math.PI / 180, root._angle * Math.PI / 180 + Math.PI * 1.3)
            ctx.stroke()
        }
    }
    Connections { target: root; function on_SangleChanged() { spinner.requestPaint() } }

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
}
