import QtQuick

Rectangle {
    id: root
    property url skinSource: ""
    width: 48; height: 48
    color: "transparent"

    // ── Google-style Spinner ──
    property real _spinnerAngle: 0
    property real _spinnerArcLen: 60
    property bool _spinning: img.status === Image.Loading
        || img.status === Image.Error
        || (img.status === Image.Null && root.skinSource.toString())

    RotationAnimation on _spinnerAngle {
        running: root._spinning
        from: 0; to: 360; duration: 1200; loops: Animation.Infinite
        easing.type: Easing.InOutCubic
    }

    SequentialAnimation on _spinnerArcLen {
        running: root._spinning
        loops: Animation.Infinite
        NumberAnimation { from: 20; to: 280; duration: 650; easing.type: Easing.InOutCubic }
        NumberAnimation { from: 280; to: 20; duration: 650; easing.type: Easing.InOutCubic }
    }

    Canvas {
        id: spinner
        anchors.fill: parent
        visible: root._spinning
        onPaint: {
            var ctx = getContext("2d");
            var cw = width, ch = height;
            ctx.clearRect(0, 0, cw, ch);
            if (img.status === Image.Ready) return;
            var cx = cw / 2, cy = ch / 2, r = Math.min(cx, cy) - 5;
            ctx.lineWidth = 2.5;
            ctx.lineCap = "round";

            // Draw faint track circle
            ctx.strokeStyle = "rgba(91, 141, 239, 0.15)";
            ctx.beginPath();
            ctx.arc(cx, cy, r, 0, Math.PI * 2);
            ctx.stroke();

            // Draw animated arc
            var angRad = root._spinnerAngle * Math.PI / 180;
            var lenRad = (root._spinnerArcLen * Math.PI / 180) / 2;
            ctx.strokeStyle = "#5b8def";
            ctx.beginPath();
            ctx.arc(cx, cy, r, angRad - lenRad, angRad + lenRad);
            ctx.stroke();
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

    // ── Fallback when no source / error ──
    Rectangle {
        anchors.fill: parent; radius: 4
        visible: !root._spinning && img.status !== Image.Ready
        color: "#1a1e28"
        Text {
            anchors.centerIn: parent
            text: "?"; font.pixelSize: 18; color: "#4a5068"
        }
    }
}
