import QtQuick
import QtQuick.Controls

// 2D Minecraft Head Avatar — GPU-accelerated Canvas with nearest-neighbour sampling
// Same sharpness as 3D, zero extra dependencies.
Rectangle {
    id: root
    property url skinSource: ""
    width: 48; height: 48
    color: "transparent"

    // ── Invisible preloader (handles file:/// URLs correctly) ──
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

    // ── GPU Canvas (RHI → Direct3D, nearest-neighbour) ──
    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: false
        renderStrategy: Canvas.Threaded

        onPaint: {
            var ctx = getContext("2d")
            var w = width, h = height

            // Clear
            ctx.clearRect(0, 0, w, h)

            if (skin.status !== Image.Ready) {
                // Draw placeholder circle
                ctx.fillStyle = "#1a1f2e"
                ctx.beginPath()
                ctx.arc(w / 2, h / 2, Math.min(w, h) / 2 - 2, 0, Math.PI * 2)
                ctx.fill()
                return
            }

            // ── Pixel-art sharpness: nearest-neighbour ──
            ctx.imageSmoothingEnabled = false

            // Face: skin atlas region (8, 8) → 8×8 → scale to canvas
            ctx.drawImage(skin, 8, 8, 8, 8, 0, 0, w, h)

            // Hat overlay: skin atlas region (40, 8) → 8×8 → alpha-blend on top
            ctx.drawImage(skin, 40, 8, 8, 8, 0, 0, w, h)
        }
    }

    // ── Circular mask clip ──
    layer.enabled: true
    layer.effect: ShaderEffect {
        property size size: Qt.size(root.width, root.height)
        fragmentShader: "
            varying highp vec2 qt_TexCoord0;
            uniform highp vec2 size;
            void main() {
                vec2 center = vec2(0.5, 0.5);
                float dist = distance(qt_TexCoord0, center);
                float radius = 0.46;
                float alpha = 1.0 - smoothstep(radius - 0.02, radius + 0.02, dist);
                gl_FragColor = vec4(texture2D(source, qt_TexCoord0).rgb, alpha);
            }"
    }
}
