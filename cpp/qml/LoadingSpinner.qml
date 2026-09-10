// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick

// LoadingSpinner — 环形加载弧指示器
// 基于 Canvas 绘制，旋转动画驱动弧段转动
//
// 用法：
//   LoadingSpinner {
//       width: 24; height: 24
//       running: page.modSearching
//   }

Canvas {
    id: root

    implicitWidth: 24
    implicitHeight: 24

    property color trackColor: StyleTokens.accentSubtle
    property color arcColor: StyleTokens.accentLight
    property real arcDegrees: 110   // 弧段张角（角度制）
    property int periodMs: 1200     // 完整旋转周期
    property bool running: true

    visible: running

    onPaint: {
        var ctx = getContext("2d")
        var s = Math.min(width, height)
        var cx = s / 2
        var cy = s / 2
        var r = cx - 3       // 外圈半径
        var sw = 2.5         // 描边宽度

        ctx.clearRect(0, 0, width, height)

        // 背景圆环（轨道）
        ctx.beginPath()
        ctx.arc(cx, cy, r, 0, Math.PI * 2)
        ctx.strokeStyle = root.trackColor
        ctx.lineWidth = sw
        ctx.stroke()

        // 活动弧段 — 从正上方 ( -π/2 ) 开始
        var arcRad = root.arcDegrees * Math.PI / 180
        var startAngle = -Math.PI / 2 - arcRad / 2  // 居中在顶部
        var endAngle = startAngle + arcRad
        ctx.beginPath()
        ctx.arc(cx, cy, r, startAngle, endAngle)
        ctx.strokeStyle = root.arcColor
        ctx.lineWidth = sw
        ctx.lineCap = "round"
        ctx.stroke()
    }

    RotationAnimation on rotation {
        from: 0; to: 360
        duration: root.periodMs
        loops: Animation.Infinite
        running: root.running
        easing.type: Easing.Linear
    }

    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onArcColorChanged: requestPaint()
    onTrackColorChanged: requestPaint()
    onArcDegreesChanged: requestPaint()
}
