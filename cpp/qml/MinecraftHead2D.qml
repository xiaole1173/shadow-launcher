import QtQuick

// 2D Minecraft face avatar — GPU Canvas, nearest-neighbour, zero fluff.
Rectangle {
    id: root
    property url skinSource: ""
    width: 48; height: 48
    color: "transparent"

    Image {
        id: skin
        source: root.skinSource
        visible: false
        cache: false
        asynchronous: true
        onStatusChanged: {
            if (status === Image.Ready || status === Image.Error)
                canvas.requestPaint()
        }
    }

    Connections {
        target: root
        function onSkinSourceChanged() { canvas.requestPaint() }
    }

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: false
        renderStrategy: Canvas.Threaded

        onPaint: {
            var ctx = getContext("2d")
            var w = width, h = height
            ctx.clearRect(0, 0, w, h)

            if (skin.status !== Image.Ready) {
                // Loading spinner placeholder
                ctx.fillStyle = "#2a3040"
                ctx.fillRect(0, 0, w, h)
                return
            }

            ctx.imageSmoothingEnabled = false

            // Face: (8,8) 8×8 from skin atlas
            ctx.drawImage(skin, 8, 8, 8, 8, 0, 0, w, h)

            // Hat overlay: (40,8) 8×8, alpha-blend
            ctx.drawImage(skin, 40, 8, 8, 8, 0, 0, w, h)
        }
    }
}
