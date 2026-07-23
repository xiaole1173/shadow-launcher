import QtQuick
import QtQuick.Layouts

Item {
    id: root
    anchors.fill: parent

    property bool opened: false
    signal accepted(int profileIndex)
    signal cancelled()

    Rectangle {
        anchors.fill: parent; z: 100
        color: "#80000000"
        opacity: root.opened ? 1 : 0
        visible: root.opened || opacity > 0
        Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
        MouseArea { anchors.fill: parent }
    }

    Rectangle {
        id: card; z: 101
        width: 360

        // Height = title bar(48) + separator(1) + pad(8) + max(content, 140) + bottomPad(8)
        property real _contentH: Math.max(140, (profilesCount > 0 ? profilesCount * 46 - 2 : 0))
        height: Math.min(48 + 1 + 8 + _contentH + 8, parent ? parent.height - 80 : 600)
        anchors.centerIn: parent
        radius: 12
        color: StyleTokens.bgSecondary
        border { color: StyleTokens.borderLight; width: 1 }

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

            Rectangle {
                anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
                width: 32; height: 32; radius: 6
                color: xarea.containsMouse ? StyleTokens.bgHover : "transparent"
                Behavior on color { ColorAnimation { duration: 120 } }
                Text {
                    anchors.centerIn: parent
                    text: "\u2715"; color: StyleTokens.textSubtle; font.pixelSize: 16
                }
                MouseArea {
                    id: xarea
                    anchors.fill: parent; hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.cancelled()
                }
            }
        }

        // ── 分割线 ──
        Rectangle {
            anchors.top: parent.top; anchors.topMargin: 48
            width: parent.width; height: 1
            color: StyleTokens.borderLight
        }

        // ── 内容区域 ──
        Item {
            anchors { top: parent.top; topMargin: 48 + 1 + 10; left: parent.left; right: parent.right; bottom: parent.bottom; bottomMargin: 10 }
            clip: true

            // 角色列表 — 手动 y 定位
            Repeater {
                id: profRep
                model: backend && backend.yggdrasil ? backend.yggdrasil.profiles : []

                delegate: Rectangle {
                    x: 0
                    y: index * 46
                    width: parent ? parent.width : 360
                    height: 44
                    radius: 6

                    color: {
                        var sel = (backend && backend.yggdrasil) && index === backend.yggdrasil.profileIndex
                        if (sel) return StyleTokens.accentSubtle
                        return ma.containsMouse ? StyleTokens.bgHover : StyleTokens.bgCard
                    }
                    Behavior on color { ColorAnimation { duration: 120 } }
                    border { color: index === (backend && backend.yggdrasil ? backend.yggdrasil.profileIndex : -1)
                             ? StyleTokens.accentLight : "transparent"; width: 1 }

                    // 选中左边条
                    Rectangle {
                        width: 3; height: parent.height - 8
                        anchors { left: parent.left; leftMargin: 4; verticalCenter: parent.verticalCenter }
                        radius: 2
                        visible: (backend && backend.yggdrasil) && index === backend.yggdrasil.profileIndex
                        color: StyleTokens.accent
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12; anchors.rightMargin: 12
                        spacing: 10

                        Item {
                            Layout.preferredWidth: 28; Layout.preferredHeight: 28

                            // 实况头
                            Image {
                                id: headImg
                                anchors.fill: parent; visible: status === Image.Ready
                                source: backend && backend.yggdrasil ? backend.yggdrasil.profileHeadUrls[index] : ""
                                fillMode: Image.PreserveAspectFit
                                asynchronous: true; cache: true; smooth: false; mipmap: false
                            }

                            // fallback：首字母
                            Rectangle {
                                anchors.fill: parent; radius: 6
                                visible: headImg.status !== Image.Ready
                                color: "#0e1018"; border { color: StyleTokens.borderLight; width: 1 }
                                Text {
                                    anchors.centerIn: parent
                                    text: (typeof modelData != "undefined" && modelData && modelData.name)
                                          ? modelData.name.charAt(0).toUpperCase() : "?"
                                    color: StyleTokens.textTertiary; font { pixelSize: 14; weight: Font.Bold }
                                }
                            }
                        }

                        Text {
                            text: (typeof modelData != "undefined" && modelData && modelData.name)
                                  ? modelData.name : ""
                            color: StyleTokens.textPrimary; font.pixelSize: 14
                            Layout.fillWidth: true; elide: Text.ElideRight
                        }

                        Rectangle {
                            width: 18; height: 18; radius: 9
                            visible: index === (backend && backend.yggdrasil ? backend.yggdrasil.profileIndex : -1)
                            color: StyleTokens.accent
                            Text {
                                anchors.centerIn: parent
                                text: "\u2713"; color: StyleTokens.textInverse
                                font { pixelSize: 11; weight: Font.Bold }
                            }
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
        }

        // ── 计算 profiles 数量（用于高度） ──
        readonly property int profilesCount: backend && backend.yggdrasil ? backend.yggdrasil.profiles.length : 0

    onOpenedChanged: {
        if (opened && backend && backend.yggdrasil)
            backend.yggdrasil.preloadProfileSkins()
    }
    }
}
