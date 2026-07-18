// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick

// ═══ 衣帽间入口 Toast — 参考 InlineToast 设计 ═══
// 从菜单左边缘向外弹性伸展，蓝色主题
// 内容: [Lucide图标] + "衣帽间"，无其他文字
Item {
    id: root
    signal clicked()

    width: 140
    height: 40
    clip: false

    // ── 背景 + 内容 (InlineToast 结构: 背景包住色条和文字) ──
    Rectangle {
        id: bar
        width: parent.width
        radius: StyleTokens.radiusMd
        color: StyleTokens.infoBg            // info 蓝色背景, 同 InlineToast
        implicitHeight: label.implicitHeight + 20

        // ── 左侧蓝色色条 (同 InlineToast info bar) ──
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 3
            color: StyleTokens.info        // info 蓝色, 同 InlineToast
            radius: StyleTokens.radiusXs
        }

        // ── 内容: Lucide图标 + "衣帽间" ──
        Row {
            id: contentRow
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 12
            spacing: 8

            // Lucide 图标: box (储物箱 = 衣帽间)
            Image {
                anchors.verticalCenter: parent.verticalCenter
                source: "../icons/lucide/box.svg"
                width: 14; height: 14
                sourceSize: Qt.size(14, 14)
            }

            Text {
                id: label
                text: qsTr("衣帽间")
                color: "#c8d8f0"    // info 文字色, 同 InlineToast
                font.pixelSize: StyleTokens.fontSizeSm
                font.weight: Font.Medium
                font.family: "Microsoft YaHei"
            }
        }

        // ── 弹性从左侧滑入 (InlineToast 是从右滑, 这里改成从左) ──
        transform: Translate {
            x: _visible ? 0 : -bar.width + 4   // 未显示时留 4px 边缘
            Behavior on x {
                NumberAnimation { duration: 350; easing.type: Easing.OutBack }
            }
        }
    }

    property bool _visible: false
    opacity: _visible ? 1 : 0
    Behavior on opacity {
        NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
    }

    scale: mouseArea.containsMouse ? 1.04 : 1.0
    Behavior on scale { NumberAnimation { duration: 150 } }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }

    Component.onCompleted: { _visible = true }
}
