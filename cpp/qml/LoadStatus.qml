// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick
import QtQuick.Layouts

// LoadStatus — 统一加载/空状态指示器
// 用法：
//   LoadStatus {
//       loading: page.modSearching
//       emptyText: "输入关键词搜索 Mod"
//       count: modResultsModel.count
//   }

Rectangle {
    id: root

    implicitWidth: parent ? parent.width : 200
    implicitHeight: root.count > 0 ? 0 : 140
    height: root.count > 0 ? 0 : implicitHeight
    color: "transparent"
    visible: root.count === 0
    clip: true

    property bool loading: false
    property string emptyText: ""
    property int count: 0

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 10
        visible: root.count === 0

        // ── 加载环形弧（仅 loading 时显示） ──
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            width: 28; height: 28
            color: "transparent"
            visible: root.loading
            LoadingSpinner {
                anchors.centerIn: parent
                width: 24; height: 24
                running: true
            }
        }

        // ── 文字 ──
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: root.loading ? "正在搜索..." : root.emptyText
            color: root.loading ? StyleTokens.accentHover : "#606478"
            font.pixelSize: StyleTokens.fontSizeSm
        }
    }
}
