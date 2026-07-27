// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick
import QtQuick.Layouts

// PaginationFooter — 统一翻页控制条
//
// 布局：首页 | 上一页 —— 第 X 页 —— 下一页
// 「第 X 页」文字居中在整行，按钮靠文字两侧排列
// 按钮仅悬停灰色反馈 + 按下短暂加深
//
// 用法：
//   PaginationFooter {
//       currentPage: page.rpPage
//       hasNext: page.rpHasMore
//       onFirstClicked: searchPage(0)
//       onPrevClicked: searchPage(currentPage - 1)
//       onNextClicked: searchPage(currentPage + 1)
//   }

Rectangle {
    id: root

    implicitWidth: parent ? parent.width : 200
    implicitHeight: 32
    color: "transparent"
    visible: !root.loading

    property int currentPage: 0
    property bool hasNext: false
    property bool hasPrev: currentPage > 0
    property bool loading: false

    signal firstClicked()
    signal prevClicked()
    signal nextClicked()

    // ── 居中容器 ──
    Item {
        anchors.fill: parent

        // ── 首页按钮 ──
        PaginationBtn {
            id: firstBtn
            iconPoints: [
                {x1:10,y1:7, x2:5,y2:12, x3:10,y3:17},
                {x1:18,y1:7, x2:13,y2:12, x3:18,y3:17}
            ]
            disabled: !root.hasPrev
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: prevBtn.left
            anchors.rightMargin: 2
            onPressed: if (root.hasPrev) root.firstClicked()
        }

        // ── 上一页按钮 ──
        PaginationBtn {
            id: prevBtn
            iconPoints: [
                {x1:14,y1:7, x2:8,y2:12, x3:14,y3:17}
            ]
            disabled: !root.hasPrev
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: pageText.left
            anchors.rightMargin: 6
            onPressed: if (root.hasPrev) root.prevClicked()
        }

        // ── 页码文字 ──
        Text {
            id: pageText
            anchors.centerIn: parent
            anchors.verticalCenterOffset: 1
            text: "第 " + (root.currentPage + 1) + " 页"
            color: "#b0b8c8"
            font.pixelSize: 13
            font.weight: Font.Medium
        }

        // ── 下一页按钮 ──
        PaginationBtn {
            id: nextBtn
            iconPoints: [
                {x1:10,y1:7, x2:16,y2:12, x3:10,y3:17}
            ]
            disabled: !root.hasNext
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: pageText.right
            anchors.leftMargin: 6
            onPressed: if (root.hasNext) root.nextClicked()
        }
    }

    // ── 翻页按钮组件 ──
    component PaginationBtn: Rectangle {
        property var iconPoints: []
        property bool disabled: false
        signal pressed()

        implicitWidth: 28; implicitHeight: 28; radius: 6
        color: disabled ? "transparent"
              : btnHover.pressed ? "#252a38"
              : btnHover.containsMouse ? "#1a1e2a"
              : "transparent"
        border.width: 0
        opacity: disabled ? 0.25 : 1.0
        Behavior on color { ColorAnimation { duration: 80 } }

        Canvas {
            id: ico
            anchors.centerIn: parent
            width: 24; height: 24
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.strokeStyle = parent.disabled ? "#606478" : "#c0c8d8"
                ctx.lineWidth = 2
                ctx.lineCap = "round"
                ctx.lineJoin = "round"
                for (var s = 0; s < parent.iconPoints.length; s++) {
                    var p = parent.iconPoints[s]
                    ctx.beginPath()
                    ctx.moveTo(p.x1, p.y1)
                    ctx.lineTo(p.x2, p.y2)
                    ctx.lineTo(p.x3, p.y3)
                    ctx.stroke()
                }
            }
        }
        onDisabledChanged: ico.requestPaint()

        MouseArea {
            id: btnHover; anchors.fill: parent; hoverEnabled: true
            cursorShape: disabled ? Qt.ArrowCursor : Qt.PointingHandCursor
            onClicked: if (!disabled) parent.pressed()
        }
    }
}
