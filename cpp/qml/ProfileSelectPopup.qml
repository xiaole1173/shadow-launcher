import QtQuick
import QtQuick.Layouts

Item {
    id: root
    anchors.fill: parent

    property bool opened: false
    signal accepted(int profileIndex)
    signal cancelled()

    // ── 触发头像预加载 ──
    onOpenedChanged: {
        if (opened && backend && backend.yggdrasil)
            backend.yggdrasil.preloadProfileSkins()
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

    // ── 主卡片 ──
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

            ShadowIconButton {
                anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
                icon: "\u2715"; type: "close"
                onClicked: root.cancelled()
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
                model: backend && backend.yggdrasil ? backend.yggdrasil.profiles : []

                delegate: Rectangle {
                    x: 4
                    y: 4 + index * 56
                    width: parent ? parent.width - 8 : 372
                    height: 52
                    radius: StyleTokens.radiusMd

                    // 选中 ＋ 悬停 底色
                    color: {
                        var sel = (backend && backend.yggdrasil) && index === backend.yggdrasil.profileIndex
                        if (sel) return StyleTokens.accentSubtle
                        return ma.containsMouse ? StyleTokens.bgHover : StyleTokens.bgCard
                    }
                    Behavior on color { ColorAnimation { duration: 120 } }

                    // 选中左边条
                    Rectangle {
                        width: 3; height: parent.height - 10
                        anchors { left: parent.left; leftMargin: 5; verticalCenter: parent.verticalCenter }
                        radius: 2
                        visible: (backend && backend.yggdrasil) && index === backend.yggdrasil.profileIndex
                        color: StyleTokens.accent
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 14; anchors.rightMargin: 12
                        spacing: 12

                        // ── 头像 ──
                        Item {
                            Layout.preferredWidth: 34; Layout.preferredHeight: 34

                            Image {
                                anchors.fill: parent
                                source: backend && backend.yggdrasil
                                       ? backend.yggdrasil.profileHeadUrls[index] : ""
                                fillMode: Image.PreserveAspectFit
                                asynchronous: true; cache: true; smooth: false
                            }

                            Rectangle {
                                anchors.fill: parent; radius: StyleTokens.radiusSm
                                color: "#0e1018"
                                border { color: StyleTokens.borderLight; width: 1 }
                                visible: {
                                    var img = parent.children[0]
                                    return !img.source
                                           || img.status === Image.Null
                                           || img.status === Image.Error
                                }
                                Text {
                                    anchors.centerIn: parent
                                    text: {
                                        if (typeof modelData == "undefined" || !modelData || !modelData.name) return "?"
                                        return modelData.name.charAt(0).toUpperCase()
                                    }
                                    color: StyleTokens.textTertiary
                                    font { pixelSize: 16; weight: Font.Bold }
                                }
                            }

                            // 加载进度指示
                            Rectangle {
                                anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter
                                width: 6; height: 6; radius: 3
                                color: {
                                    var img = parent.children[0]
                                    if (img.source && img.status === Image.Loading) return StyleTokens.warning
                                    if (img.source && img.status === Image.Ready) return StyleTokens.success
                                    return "transparent"
                                }
                                visible: parent.children[0].source ? true : false
                            }
                        }

                        // ── 名字 ──
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
                                text: "\u2713"; color: StyleTokens.textInverse
                                font { pixelSize: 12; weight: Font.Bold }
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
