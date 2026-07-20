import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

Flickable {
    anchors.fill: parent
    contentWidth: width
    contentHeight: expCol.implicitHeight
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    property bool hasBg: backend ? (typeof backend.customBgPath === "string" && backend.customBgPath.length > 0) : false
    ColumnLayout {
        id: expCol
        width: parent.width
        spacing: 12
        Text { text: qsTr("实验性功能"); font.pixelSize: StyleTokens.fontSizeXl; font.bold: true; color: StyleTokens.textPrimary }
        Text { text: qsTr("以下功能处于实验阶段，可能存在不稳定的情况。"); font.pixelSize: StyleTokens.fontSizeSm; color: "#707888"; wrapMode: Text.WordWrap; Layout.fillWidth: true }

        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 110; radius: StyleTokens.radiusLg; color: "#11141c"; border.color: StyleTokens.bgInput; clip: true
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 14; spacing: 8
                RowLayout {
                    Layout.fillWidth: true; spacing: 6
                    Text { text: qsTr("嵌入式窗口登录"); font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.DemiBold; color: StyleTokens.textPrimary }
                    Rectangle { radius: StyleTokens.radiusSm; height: 18; width: tagText.implicitWidth + 10; color: StyleTokens.bgHover
                        Text { id: tagText; anchors.centerIn: parent; text: qsTr("实验性"); font.pixelSize: StyleTokens.fontSizeXs; color: "#a098e0" }
                    }
                    Item { Layout.fillWidth: true }
                    Text { text: embeddedSwitch.checked ? qsTr("已开启") : qsTr("已关闭"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary }
                    Switch {
                        id: embeddedSwitch; Layout.alignment: Qt.AlignVCenter
                        checked: backend ? backend.embeddedLoginEnabled : false
                        onToggled: { if (backend) backend.setEmbeddedLoginEnabled(checked) }
                        palette { mid: "#3a4eb8"; dark: "#252835"; light: "#5d6fe0"; button: checked ? "#5d6fe0" : "#353845" }
                        Behavior on checked { NumberAnimation { duration: 200 } }
                    }
                }
                Text {
                    text: qsTr("在启动器内嵌窗口中完成 Microsoft 登录，无需跳转外部浏览器。\n输入 Windows 已登录的 Microsoft 账户后，会自动唤起系统 PIN/指纹验证窗口，可快捷登录。")
                    font.pixelSize: StyleTokens.fontSizeSm; color: "#707888"; wrapMode: Text.WordWrap; Layout.fillWidth: true
                }
            }
        }

        // ── Language selector ──
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 110; radius: StyleTokens.radiusLg; color: "#11141c"; border.color: StyleTokens.bgInput; clip: true
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 14; spacing: 8
                RowLayout {
                    Layout.fillWidth: true; spacing: 6
                    Text { text: qsTr("界面语言"); font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.DemiBold; color: StyleTokens.textPrimary }
                    Rectangle { radius: StyleTokens.radiusSm; height: 18; width: langTag.implicitWidth + 10; color: StyleTokens.bgHover
                        Text { id: langTag; anchors.centerIn: parent; text: qsTr("实验性"); font.pixelSize: StyleTokens.fontSizeXs; color: "#a098e0" }
                    }
                    Item { Layout.fillWidth: true }
                    ComboBox {
                        id: langCombo
                        model: ["简体中文（中国大陆）", "繁體中文（香港特別行政區 / 澳門特別行政區）", "繁體中文（中國台灣）"]
                        property bool _syncBack: false
                        Component.onCompleted: {
                            _syncBack = true
                            currentIndex = (backend && backend.readLanguageFile) ? backend.readLanguageFile() : 0
                            _syncBack = false
                        }
                        onActivated: {
                            if (_syncBack) return
                            if (backend) backend.switchLanguage(currentIndex)
                        }
                        Layout.preferredWidth: 280

                        // ── Custom dark style ──
                        background: Rectangle {
                            radius: StyleTokens.radiusMd; color: langCombo.hovered ? "#252a38" : "#161a24"
                            border.color: langCombo.hovered ? "#3a4eb8" : "#1e2230"
                            border.width: 1
                            Behavior on color { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                        }
                        contentItem: Text {
                            leftPadding: 12; verticalAlignment: Text.AlignVCenter
                            text: langCombo.displayText; color: "#d0d4e0"; font.pixelSize: StyleTokens.fontSizeSm
                            elide: Text.ElideRight
                        }
                        indicator: Canvas {
                            width: 12; height: 12; anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.strokeStyle = langCombo.hovered ? "#8088f0" : "#606478"
                                ctx.lineWidth = 1.5
                                ctx.beginPath(); ctx.moveTo(0, 3); ctx.lineTo(6, 9); ctx.lineTo(12, 3); ctx.stroke()
                            }
                        }
                        delegate: ItemDelegate {
                            width: parent.width
                            contentItem: Text {
                                text: modelData; color: "#d0d4e0"; font.pixelSize: StyleTokens.fontSizeSm
                                verticalAlignment: Text.AlignVCenter; leftPadding: 12; rightPadding: 12
                                elide: Text.ElideRight
                            }
                            background: Rectangle {
                                color: highlighted ? "#252a38" : "transparent"
                                Behavior on color { ColorAnimation { duration: 150 } }
                            }
                            highlighted: langCombo.highlightedIndex === index
                        }
                        popup: Popup {
                            y: langCombo.height + 4
                            width: langCombo.width
                            implicitHeight: contentItem.implicitHeight + 8
                            padding: 4
                            enter: Transition {
                                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 140; easing.type: Easing.OutCubic }
                                NumberAnimation { property: "y"; from: langCombo.height + 2; duration: 140; easing.type: Easing.OutCubic }
                            }
                            exit: Transition {
                                NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 80; easing.type: Easing.InCubic }
                            }
                            contentItem: ListView {
                                clip: true; implicitHeight: contentHeight
                                model: langCombo.popup.visible ? langCombo.delegateModel : null
                                currentIndex: langCombo.highlightedIndex
                            }
                            background: Rectangle {
                                radius: StyleTokens.radiusMd; color: "#161a24"; border.color: StyleTokens.bgElevated
                            }
                        }
                    }
                }
                Text {
                    text: qsTr("选择语言后即时生效，无需重启启动器。")
                    font.pixelSize: StyleTokens.fontSizeSm; color: "#707888"; wrapMode: Text.WordWrap; Layout.fillWidth: true
                }
            }
        }

        // ── MC auto-language ──
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: langModeCard.implicitHeight + 28
            radius: StyleTokens.radiusLg; color: "#11141c"; border.color: StyleTokens.bgInput
            ColumnLayout {
                id: langModeCard
                anchors.fill: parent; anchors.margins: 14; spacing: 6
                RowLayout {
                    Layout.fillWidth: true; spacing: 6
                    Text { text: qsTr("游戏语言自动调整"); font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.DemiBold; color: StyleTokens.textPrimary }
                    Rectangle { radius: StyleTokens.radiusSm; height: 18; width: langNewTag.implicitWidth + 10; color: "#2a3a5c"
                        Text { id: langNewTag; anchors.centerIn: parent; text: qsTr("新增"); font.pixelSize: StyleTokens.fontSizeXs; color: "#70b8ff" }
                    }
                    Item { Layout.fillWidth: true }
                    ComboBox {
                        id: langModeCombo
                        model: [qsTr("系统区域（启动时自动检测）"), qsTr("IP 属地（安装时自动调整）"), qsTr("关闭（手动设置）")]
                        property int _initLangModeIdx: 0
                        property bool _syncLang: false
                        Component.onCompleted: {
                            _syncLang = true
                            currentIndex = page._initLangModeIdx
                            _syncLang = false
                        }
                        onActivated: {
                            if (_syncLang) return
                            backend.setAutoLangModeFromCombo(currentIndex)
                        }
                        Layout.preferredWidth: 280

                        background: Rectangle {
                            radius: StyleTokens.radiusMd; color: langModeCombo.hovered ? "#252a38" : "#161a24"
                            border.color: langModeCombo.hovered ? "#3a4eb8" : "#1e2230"
                            border.width: 1
                            Behavior on color { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                        }
                        contentItem: Text {
                            leftPadding: 12; verticalAlignment: Text.AlignVCenter
                            text: langModeCombo.displayText; color: "#d0d4e0"; font.pixelSize: StyleTokens.fontSizeSm
                            elide: Text.ElideRight
                        }
                        indicator: Canvas {
                            width: 12; height: 12; anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.strokeStyle = langModeCombo.hovered ? "#8088f0" : "#606478"
                                ctx.lineWidth = 1.5
                                ctx.beginPath(); ctx.moveTo(0, 3); ctx.lineTo(6, 9); ctx.lineTo(12, 3); ctx.stroke()
                            }
                        }
                        delegate: ItemDelegate {
                            width: parent.width
                            contentItem: Text {
                                text: modelData; color: "#d0d4e0"; font.pixelSize: StyleTokens.fontSizeSm
                                verticalAlignment: Text.AlignVCenter; leftPadding: 12; rightPadding: 12
                                elide: Text.ElideRight
                            }
                            background: Rectangle {
                                color: highlighted ? "#252a38" : "transparent"
                                Behavior on color { ColorAnimation { duration: 150 } }
                            }
                            highlighted: langModeCombo.highlightedIndex === index
                        }
                        popup: Popup {
                            y: langModeCombo.height + 4
                            width: langModeCombo.width
                            implicitHeight: contentItem.implicitHeight + 8
                            padding: 4
                            enter: Transition {
                                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 140; easing.type: Easing.OutCubic }
                                NumberAnimation { property: "y"; from: langModeCombo.height + 2; duration: 140; easing.type: Easing.OutCubic }
                            }
                            exit: Transition {
                                NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 80; easing.type: Easing.InCubic }
                            }
                            contentItem: ListView {
                                clip: true; implicitHeight: contentHeight
                                model: langModeCombo.popup.visible ? langModeCombo.delegateModel : null
                                currentIndex: langModeCombo.highlightedIndex
                            }
                            background: Rectangle {
                                radius: StyleTokens.radiusMd; color: "#161a24"; border.color: StyleTokens.bgElevated
                            }
                        }
                    }
                }
                Text {
                    text: {
                        switch (langModeCombo.currentIndex) {
                            case 0: return qsTr("每次启动游戏时根据电脑系统区域自动写入 options.txt") + "（默认识别 zh_CN / zh_HK / zh_TW / ja_JP / ko_KR）"
                            case 1: return qsTr("安装版本后根据 IP 属地自动写入 options.txt，启动时不覆盖")
                            case 2: return qsTr("不自动调整游戏语言，可在 Minecraft 游戏设置中手动选择")
                            default: return ""
                        }
                    }
                    font.pixelSize: StyleTokens.fontSizeSm; color: "#707888"; wrapMode: Text.WordWrap; Layout.fillWidth: true
                }
            }
        }

        // ── Custom background ──
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: bgCardContent.implicitHeight + 28
            radius: StyleTokens.radiusLg; color: "#11141c"; border.color: StyleTokens.bgInput
            ColumnLayout {
                id: bgCardContent
                anchors.fill: parent; anchors.margins: 14; spacing: 8
                RowLayout {
                    Layout.fillWidth: true; spacing: 6
                    Text { text: qsTr("自定义背景"); font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.DemiBold; color: StyleTokens.textPrimary }
                    Rectangle { radius: StyleTokens.radiusSm; height: 18; width: bgTag.implicitWidth + 10; color: StyleTokens.bgHover
                        Text { id: bgTag; anchors.centerIn: parent; text: qsTr("实验性"); font.pixelSize: StyleTokens.fontSizeXs; color: "#a098e0" }
                    }
                    Item { Layout.fillWidth: true }
                    Text { text: hasBg ? qsTr("已设置") : qsTr("未设置"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary }
                    Rectangle { width: 60; height: 28; radius: StyleTokens.radiusSm; color: bgBrowseHov.pressed ? "#1e2a40" : (bgBrowseHov.containsMouse ? "#252a38" : "#161a24"); border.color: bgBrowseHov.containsMouse ? "#4a5ed0" : StyleTokens.bgHover
                        Behavior on color { ColorAnimation { duration: 150 } }
                        Behavior on border.color { ColorAnimation { duration: 150 } }
                        scale: bgBrowseHov.containsMouse ? (bgBrowseHov.pressed ? 0.94 : 1.04) : 1.0
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        Text { anchors.centerIn: parent; text: qsTr("浏览"); font.pixelSize: StyleTokens.fontSizeSm; color: "#d0d4e0" }
                        MouseArea { id: bgBrowseHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            onClicked: { if (backend) backend.pickBackgroundImage() }
                        }
                    }
                    Rectangle { visible: hasBg; width: 60; height: 28; radius: StyleTokens.radiusSm; color: bgClearHov.pressed ? "#3a1818" : (bgClearHov.containsMouse ? "#302020" : "#161a24"); border.color: bgClearHov.containsMouse ? "#d06060" : StyleTokens.bgHover
                        Behavior on color { ColorAnimation { duration: 150 } }
                        Behavior on border.color { ColorAnimation { duration: 150 } }
                        scale: bgClearHov.containsMouse ? (bgClearHov.pressed ? 0.94 : 1.04) : 1.0
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        Text { anchors.centerIn: parent; text: qsTr("清除"); font.pixelSize: StyleTokens.fontSizeSm; color: "#d08080" }
                        MouseArea { id: bgClearHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            onClicked: { if (backend) backend.setCustomBgPath("") }
                        }
                    }
                }

                // Preview thumbnail (click to adjust crop)
                Image {
                    id: bgPreview
                    Layout.fillWidth: true; Layout.preferredHeight: 100
                    visible: hasBg
                    source: hasBg ? backend.customBgPath : ""
                    fillMode: Image.PreserveAspectFit
                    cache: false; asynchronous: true
                }
                MouseArea {
                    anchors.fill: bgPreview; visible: hasBg
                    hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: { bgCropOverlay.active = true }
                    scale: containsMouse ? (pressed ? 0.95 : 1.05) : 1.0
                    Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                }
                Rectangle {
                    opacity: hasBg ? 0 : 0
                    anchors.fill: bgPreview
                    color: "transparent"; border.color: "#3a5ed0"; border.width: 1.5; radius: StyleTokens.radiusSm
                }
                Rectangle { visible: hasBg; Layout.fillWidth: true; height: 1; color: StyleTokens.bgInput }

                // Hint text
                Text {
                    visible: hasBg; Layout.fillWidth: true; wrapMode: Text.WordWrap
                    font.pixelSize: StyleTokens.fontSizeXs; color: "#606480"
                    text: qsTr("当前预览的是所选中的整个图片，而非裁剪后的图片。点击图片，可手动更改选区范围。")
                }

                // Sliders
                RowLayout {
                    visible: hasBg
                    Layout.fillWidth: true; spacing: 16
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 4
                        Text { text: qsTr("菜单栏透明度"); font.pixelSize: StyleTokens.fontSizeSm; color: "#b0b8c8" }
                        RowLayout { Layout.fillWidth: true; spacing: 8
                            Slider {
                                id: sidebarSPSlider
                                Layout.fillWidth: true; from: 0.40; to: 1.0; stepSize: 0.05
                                value: backend ? backend.sidebarOpacity : 0.90
                                onMoved: { if (backend) backend.setSidebarOpacity(value) }
                                background: Rectangle {
                                    implicitHeight: 4
                                    x: sidebarSPSlider.leftPadding
                                    y: sidebarSPSlider.topPadding + sidebarSPSlider.availableHeight / 2 - height / 2
                                    width: sidebarSPSlider.availableWidth; height: 4; radius: StyleTokens.radiusXs; color: StyleTokens.bgHover
                                    Rectangle {
                                        width: sidebarSPSlider.visualPosition * parent.width; height: 4; radius: StyleTokens.radiusXs; color: "#3a4eb8"
                                    }
                                }
                                handle: Rectangle {
                                    implicitWidth: 12; implicitHeight: 12
                                    x: sidebarSPSlider.leftPadding + sidebarSPSlider.visualPosition * (sidebarSPSlider.availableWidth - width)
                                    y: sidebarSPSlider.topPadding + sidebarSPSlider.availableHeight / 2 - height / 2
                                    radius: StyleTokens.radiusMd; color: StyleTokens.accentLight
                                    Behavior on x { SmoothedAnimation { velocity: 600; duration: 200 } }
                                }
                            }
                            Text { text: (backend ? backend.sidebarOpacity : 0.90).toFixed(2); font.pixelSize: StyleTokens.fontSizeSm; color: "#a0a8c0"; Layout.preferredWidth: 32 }
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 4
                        Text { text: qsTr("背景可见度"); font.pixelSize: StyleTokens.fontSizeSm; color: "#b0b8c8" }
                        RowLayout { Layout.fillWidth: true; spacing: 8
                            Slider {
                                id: contentSPSlider
                                Layout.fillWidth: true; from: 0.40; to: 1.0; stepSize: 0.05
                                value: backend ? backend.contentOpacity : 0.70
                                onMoved: { if (backend) backend.setContentOpacity(value) }
                                background: Rectangle {
                                    implicitHeight: 4
                                    x: contentSPSlider.leftPadding
                                    y: contentSPSlider.topPadding + contentSPSlider.availableHeight / 2 - height / 2
                                    width: contentSPSlider.availableWidth; height: 4; radius: StyleTokens.radiusXs; color: StyleTokens.bgHover
                                    Rectangle {
                                        width: contentSPSlider.visualPosition * parent.width; height: 4; radius: StyleTokens.radiusXs; color: "#3a4eb8"
                                    }
                                }
                                handle: Rectangle {
                                    implicitWidth: 12; implicitHeight: 12
                                    x: contentSPSlider.leftPadding + contentSPSlider.visualPosition * (contentSPSlider.availableWidth - width)
                                    y: contentSPSlider.topPadding + contentSPSlider.availableHeight / 2 - height / 2
                                    radius: StyleTokens.radiusMd; color: StyleTokens.accentLight
                                    Behavior on x { SmoothedAnimation { velocity: 600; duration: 200 } }
                                }
                            }
                            Text { text: (backend ? backend.contentOpacity : 0.70).toFixed(2); font.pixelSize: StyleTokens.fontSizeSm; color: "#a0a8c0"; Layout.preferredWidth: 32 }
                        }
                    }
                }

                Text {
                    text: qsTr("选择一张图片作为启动器背景。菜单栏和背景可见度可分别调节。")
                    font.pixelSize: StyleTokens.fontSizeSm; color: "#707888"; wrapMode: Text.WordWrap; Layout.fillWidth: true
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
