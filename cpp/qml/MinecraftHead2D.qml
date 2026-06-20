import QtQuick

Canvas {
    id: root
    property url skinSource: ""
    width: 48; height: 48
    antialiasing: false
    renderStrategy: Canvas.Threaded

    property bool _loaded: false

    onSkinSourceChanged: {
        _loaded = false
        if (skinSource.toString()) {
            loadImage(skinSource)
        } else {
            requestPaint()
        }
    }

    onImageLoaded: {
        _loaded = true
        requestPaint()
    }

    onPaint: {
        var ctx = getContext("2d")
        var w = width, h = height
        ctx.clearRect(0, 0, w, h)

        if (!_loaded) {
            ctx.fillStyle = "#2a3040"
            ctx.fillRect(0, 0, w, h)
            return
        }

        ctx.imageSmoothingEnabled = false

        // Face: skin atlas (8,8) 8×8 → canvas
        ctx.drawImage(skinSource, 8, 8, 8, 8, 0, 0, w, h)

        // Hat: (40,8) 8×8, alpha-blended on top
        ctx.drawImage(skinSource, 40, 8, 8, 8, 0, 0, w, h)

        // Clip to square
        ctx.globalCompositeOperation = "source-atop"
    }
}
