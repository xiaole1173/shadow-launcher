import QtQuick

// 2D Minecraft Head Avatar — GPU Canvas with nearest-neighbour sampling.
// Pixel-art sharpness, zero extra dependencies.
Item {
    id: root
    property url skinSource: ""
    width: 48; height: 48

    // ── Invisible preloader ──
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
    onSkinSourceChanged: canvas.requestPaint()

    // ── Circular clip mask ──
    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: "#1a1f2e"
        clip: true

        // ── GPU Canvas (RHI → Direct3D, nearest-neighbour) ──
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
                    ctx.fillStyle = "#1a1f2e"
                    ctx.fillRect(0, 0, w, h)
                    return
                }

                // Pixel-art sharpness
                ctx.imageSmoothingEnabled = false

                // Face: (8, 8) 8×8 from 64×64 skin atlas → scale to canvas
                ctx.drawImage(skin, 8, 8, 8, 8, 0, 0, w, h)

                // Hat overlay: (40, 8) 8×8 → alpha-blend
                ctx.drawImage(skin, 40, 8, 8, 8, 0, 0, w, h)
            }
        }
    }
}
