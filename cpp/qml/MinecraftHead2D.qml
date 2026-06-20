import QtQuick

Rectangle {
    id: root
    property url skinSource: ""
    width: 48; height: 48
    color: "#1e2430"  // visible even without Canvas

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: false
        renderStrategy: Canvas.Threaded
        property bool _loaded: false

        Connections {
            target: root
            function onSkinSourceChanged() {
                canvas._loaded = false
                var u = root.skinSource.toString()
                if (u) canvas.loadImage(u)
                canvas.requestPaint()
            }
        }

        onImageLoaded: { _loaded = true; requestPaint() }

        onPaint: {
            var ctx = getContext("2d"), w = width, h = height
            ctx.clearRect(0, 0, w, h)

            if (!root.skinSource.toString()) {
                ctx.fillStyle = "#2a3040"
                ctx.fillRect(0, 0, w, h)
                return
            }

            if (!_loaded) {
                // Spinner — blue ring
                var cx = w/2, cy = h/2, r = Math.min(cx, cy) - 5
                ctx.strokeStyle = "#5b8def"
                ctx.lineWidth = 3; ctx.lineCap = "round"
                var a = (Date.now() % 2000) / 2000 * Math.PI * 2
                ctx.beginPath()
                ctx.arc(cx, cy, r - 2, a, a + Math.PI * 1.3)
                ctx.stroke()

                // Repaint for animation
                canvas.requestPaint()
                return
            }

            ctx.imageSmoothingEnabled = false
            var url = root.skinSource.toString()
            ctx.drawImage(url, 8, 8, 8, 8, 0, 0, w, h)
            ctx.drawImage(url, 40, 8, 8, 8, 0, 0, w, h)
        }
    }

    Component.onCompleted: {
        if (root.skinSource.toString()) {
            canvas.loadImage(root.skinSource)
        }
    }
}
