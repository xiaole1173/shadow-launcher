import QtQuick
import QtQuick.Layouts

Item {
    id: root
    anchors.fill: parent

    property bool opened: false
    signal accepted(int profileIndex)
    signal cancelled()

    // ── 从名字生成一致的像素风配色 ──
    function avatarColor(name) {
        if (!name) return "#3b82f6"
        var h = 0
        for (var i = 0; i < name.length; ++i)
            h = ((h << 5) - h) + name.charCodeAt(i)
        h = Math.abs(h)
        var colors = ["#3b82f6","#8b5cf6","#ec4899","#f59e0b","#10b981","#06b6d4","#6366f1","#d946ef"]
        return colors[h % colors.length]
    }

    // ── 遮罩 ──
    Rectangle {
        anchors.fill: parent; z: 100
        color: "#80000000"
        opacity: root.opened ? 1 : 0
        visible: root.opened || opacity > 0
        Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
        MouseArea { anchors.fill: parent }
    }

    // ── 卡片 ──
    Rectangle {
        id: card; z: 101
        width: 380
        property real _contentH: Math.max(140, (profilesCount > 0 ? profilesCount * 56 - 4 : 0))
        height: Math.min(48 + 1 + 8 + _contentH + 8, parent ? parent.height - 80 : 600)
        anchors.centerIn: parent
        radius: StyleTokens.radiusLg
        color: StyleTokens.bgSecondary
        border { color: StyleTokens.bgElevated; width: 1 }

        scale: root.opened ? 1 : 0.9
        opacity: root.opened ? 1 : 0
        Behavior on scale { NumberAnimation { duration: 350; easing.type: Easing.OutBack } }
        Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

        // ── 标题栏 ──
        Rectangle {
            width: parent.width; height: 48
            color: "transparent"

            Text {
                anchors { left: parent.left; leftMargin: 16; verticalCenter: parent.verticalCenter }
                text: qsTr("选择角色")
                color: StyleTokens.textPrimary
                font { pixelSize: 16; weight: Font.DemiBold }
            }

            Row {
                anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
                spacing: 4

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("%1 个角色可用").arg(profilesCount)
                    color: StyleTokens.textSubtle
                    font.pixelSize: 12
                    visible: profilesCount > 0
                }

                Rectangle {
                    width: 32; height: 32; radius: StyleTokens.radiusSm
                    color: xarea.containsMouse ? StyleTokens.bgHover : "transparent"
                    Behavior on color { ColorAnimation { duration: 120 } }
                    Text {
                        anchors.centerIn: parent
                        text: "\u2715"; color: StyleTokens.textSubtle; font.pixelSize: 16
                    }
                    MouseArea {
                        id: xarea; anchors.fill: parent; hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.cancelled()
                    }
                }
            }
        }

        // ── 分割线 ──
        Rectangle {
            anchors.top: parent.top; anchors.topMargin: 48
            width: parent.width; height: 1
            color: StyleTokens.border
        }

        // ── 内容区域 ──
        Item {
            anchors { top: parent.top; topMargin: 48 + 1 + 8; left: parent.left; right: parent.right; bottom: parent.bottom; bottomMargin: 8 }
            clip: true

            Repeater {
                id: profRep
                model: backend && backend.yggdrasil ? backend.yggdrasil.profiles : []

                delegate: Rectangle {
                    id: row
                    x: 4
                    y: 4 + index * 56
                    width: parent ? parent.width - 8 : 372
                    height: 52
                    radius: StyleTokens.radiusMd

                    // 底色：选中 → accentSubtle, 悬停 → bgHover, 默认 → bgCard
                    color: {
                        var sel = (backend && backend.yggdrasil) && index === backend.yggdrasil.profileIndex
                        if (sel) return StyleTokens.accentSubtle
                        return ma.containsMouse ? StyleTokens.bgHover : StyleTokens.bgCard
                    }
                    Behavior on color { ColorAnimation { duration: 120 } }

                    // 选中：accent 色左边条
                    Rectangle {
                        width: 3; height: parent.height - 10
                        anchors { left: parent.left; leftMargin: 5; verticalCenter: parent.verticalCenter }
                        radius: 2
                        visible: (backend && backend.yggdrasil) && index === backend.yggdrasil.profileIndex
                        color: StyleTokens.accent
                        Behavior on color { ColorAnimation { duration: 120 } }
                    }

                    // 淡入动画
                    opacity: 0
                    NumberAnimation on opacity { from: 0; to: 1; duration: 250; easing.type: Easing.OutCubic; delay: index * 60 }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 14; anchors.rightMargin: 12
                        spacing: 12

                        // ── 头像：基于名字颜色的像素方块 ──
                        Rectangle {
                            Layout.preferredWidth: 34; Layout.preferredHeight: 34; radius: StyleTokens.radiusSm
                            color: {
                                var name = (typeof modelData != "undefined" && modelData && modelData.name)
                                           ? modelData.name : ""
                                return avatarColor(name)
                            }

                            // 像素风纹理：两条交叉线模拟 Minecraft 头部的眼睛线
                            Rectangle {
                                width: parent.width; height: 3
                                anchors.centerIn: parent
                                color: Qt.rgba(0,0,0,0.2); radius: 1
                            }
                            Rectangle {
                                width: 3; height: parent.height
                                anchors.centerIn: parent
                                color: Qt.rgba(0,0,0,0.15); radius: 1
                            }

                            Text {
                                anchors.centerIn: parent
                                text: {
                                    var name = (typeof modelData != "undefined" && modelData && modelData.name)
                                               ? modelData.name : ""
                                    return name ? name.charAt(0).toUpperCase() : "?"
                                }
                                color: StyleTokens.textInverse; font { pixelSize: 16; weight: Font.Bold }
                                opacity: 0.85
                            }
                        }

                        // ── 名字 + 副标题 ──
                        ColumnLayout {
                            spacing: 2
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter

                            Text {
                                text: (typeof modelData != "undefined" && modelData && modelData.name)
                                      ? modelData.name : ""
                                color: {
                                    var sel = (backend && backend.yggdrasil) && index === backend.yggdrasil.profileIndex
                                    return sel ? StyleTokens.accentLight : StyleTokens.textPrimary
                                }
                                font { pixelSize: 14; weight: Font.Medium }
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            Text {
                                text: qsTr("角色 #%1").arg(index + 1)
                                color: StyleTokens.textSubtle
                                font.pixelSize: 11
                                visible: typeof modelData != "undefined" && modelData && modelData.name
                            }
                        }

                        // ── 选中标记 ──
                        Rectangle {
                            width: 22; height: 22; radius: 11
                            visible: (backend && backend.yggdrasil) && index === backend.yggdrasil.profileIndex
                            color: StyleTokens.accent
                            Text {
                                anchors.centerIn: parent
                                text: "\u2713"; color: StyleTokens.textInverse; font.pixelSize: 12; weight: Font.Bold
                            }
                            Behavior on color { ColorAnimation { duration: 120 } }
                        }
                    }

                    MouseArea {
                        id: ma; anchors.fill: parent; hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (backend && backend.yggdrasil) {
                                backend.yggdrasil.selectProfile(index)
                                root.accepted(index)
                            }
                        }
                    }
                }
            }

            // 空状态
            Text {
                anchors.centerIn: parent
                text: qsTr("暂无可用角色")
                color: StyleTokens.textSubtle; font.pixelSize: 14
                visible: (typeof backend != "undefined" && backend && backend.yggdrasil
                         && backend.yggdrasil.profiles.length === 0)
            }
        }

        readonly property int profilesCount: backend && backend.yggdrasil ? backend.yggdrasil.profiles.length : 0
    }
}
