import QtQuick

Canvas {
    id: root
    property url skinSource: ""
    width: 48; height: 48
    antialiasing: false
    renderStrategy: Canvas.Threaded

    onSkinSourceChanged: requestPaint()

    onPaint: {
        var ctx = getContext("2d")
        var w = width, h = height
        ctx.clearRect(0, 0, w, h)

        var url = skinSource.toString()
        if (!url) {
            ctx.fillStyle = "#2a3040"
            ctx.fillRect(0, 0, w, h)
            return
        }

        ctx.imageSmoothingEnabled = false

        // drawImage(url, sx, sy, sw, sh, dx, dy, dw, dh)
        // Face: (8,8) 8×8 from skin atlas
        ctx.drawImage(url, 8, 8, 8, 8, 0, 0, w, h)

        // Hat: (40,8) 8×8, alpha-blend
        ctx.drawImage(url, 40, 8, 8, 8, 0, 0, w, h)
    }
}
