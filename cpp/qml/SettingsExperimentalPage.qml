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
        Text { text: qsTr("以下功能处于实验阶段，可能存在不稳定的情况。"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textSubtle; wrapMode: Text.WordWrap; Layout.fillWidth: true }

        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 110; radius: StyleTokens.radiusLg; color: StyleTokens.bgSecondary; border.color: StyleTokens.bgInput; clip: true
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 14; spacing: 8
                RowLayout {
                    Layout.fillWidth: true; spacing: 6
                    Text { text: qsTr("嵌入式窗口登录"); font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.DemiBold; color: StyleTokens.textPrimary }
                    Rectangle { radius: StyleTokens.radiusSm; height: 18; width: tagText.implicitWidth + 10; color: StyleTokens.bgHover
                        Text { id: tagText; anchors.centerIn: parent; text: qsTr("实验性"); font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.accentLink }
                    }
                    Item { Layout.fillWidth: true }
                    Text { text: embeddedSwitch.checked ? qsTr("已开启") : qsTr("已关闭"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary }
                    ShadowSwitch {
                        id: embeddedSwitch; Layout.alignment: Qt.AlignVCenter
                        checked: backend ? backend.embeddedLoginEnabled : false
                        onToggled: { if (backend) backend.setEmbeddedLoginEnabled(checked) }
                    }
                }
                Text {
                    text: qsTr("在启动器内嵌窗口中完成 Microsoft 登录，无需跳转外部浏览器。\n输入 Windows 已登录的 Microsoft 账户后，会自动唤起系统 PIN/指纹验证窗口，可快捷登录。")
                    font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textSubtle; wrapMode: Text.WordWrap; Layout.fillWidth: true
                }
            }
        }

        // ── 2026-08-15：版本悬停前置依赖 tooltip（实验性）──
        Rectangle {
            Layout.fillWidth: true
            // 高度内容自适应（原固定 120 太大）
            Layout.preferredHeight: depsCard.implicitHeight + 28
            radius: StyleTokens.radiusLg; color: StyleTokens.bgSecondary; border.color: StyleTokens.bgInput; clip: true
            ColumnLayout {
                id: depsCard
                anchors.fill: parent; anchors.margins: 14; spacing: 8
                RowLayout {
                    Layout.fillWidth: true; spacing: 6
                    Text { text: qsTr("版本悬停显示前置模组"); font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.DemiBold; color: StyleTokens.textPrimary }
                    Rectangle { radius: StyleTokens.radiusSm; height: 18; width: depsTag.implicitWidth + 10; color: StyleTokens.bgHover
                        Text { id: depsTag; anchors.centerIn: parent; text: qsTr("实验性"); font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.accentLink }
                    }
                    Item { Layout.fillWidth: true }
                    Text { text: depsSwitch.checked ? qsTr("已开启") : qsTr("已关闭"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary }
                    ShadowSwitch {
                        id: depsSwitch; Layout.alignment: Qt.AlignVCenter
                        checked: backend ? backend.modDepsTooltipEnabled : false
                        onToggled: { if (backend) backend.setModDepsTooltipEnabled(checked) }
                    }
                }
                Text {
                    text: qsTr("在模组详情页将鼠标悬停在版本卡片上时，显示该版本的前置模组信息。")
                    font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textSubtle; wrapMode: Text.WordWrap; Layout.fillWidth: true
                }
                RowLayout {
                    Layout.fillWidth: true; spacing: 6
                    Image {
                        source: "icons/lucide/alert-triangle.svg"
                        // ⚠ RowLayout 里必须用 Layout.preferredWidth/Height（width/height
                        // 会被布局忽略 → 回退 SVG 固有 24px → 图标"很大一块"）
                        Layout.preferredWidth: StyleTokens.fontSizeSm
                        Layout.preferredHeight: StyleTokens.fontSizeSm
                        Layout.maximumWidth: StyleTokens.fontSizeSm
                        Layout.maximumHeight: StyleTokens.fontSizeSm
                        sourceSize.width: 24; sourceSize.height: 24
                        fillMode: Image.PreserveAspectFit
                        Layout.alignment: Qt.AlignVCenter
                    }
                    Text {
                        text: qsTr("显示信息可能有误，且开启时可能造成卡顿。")
                        font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.warning; wrapMode: Text.WordWrap; Layout.fillWidth: true
                    }
                }
            }
        }

        // ── Language selector ──
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 110; radius: StyleTokens.radiusLg; color: StyleTokens.bgSecondary; border.color: StyleTokens.bgInput; clip: true
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 14; spacing: 8
                RowLayout {
                    Layout.fillWidth: true; spacing: 6
                    Text { text: qsTr("界面语言"); font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.DemiBold; color: StyleTokens.textPrimary }
                    Rectangle { radius: StyleTokens.radiusSm; height: 18; width: langTag.implicitWidth + 10; color: StyleTokens.bgHover
                        Text { id: langTag; anchors.centerIn: parent; text: qsTr("实验性"); font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.accentLink }
                    }
                    Item { Layout.fillWidth: true }
                    ShadowDropdown {
                        id: langCombo
                        model: ["简体中文（中国大陆）", "繁體中文（香港特別行政區 / 澳門特別行政區）", "繁體中文（中國台灣）"]
                        Layout.preferredWidth: 280
                        placeholderText: ""
                        Component.onCompleted: {
                            var idx = (backend && backend.readLanguageFile) ? backend.readLanguageFile() : 0
                            currentValue = model[idx]
                        }
                        onValueSelected: function(v) {
                            if (backend) backend.switchLanguage(model.indexOf(v))
                        }
                    }
                }
                Text {
                    text: qsTr("选择语言后即时生效，无需重启启动器。")
                    font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textSubtle; wrapMode: Text.WordWrap; Layout.fillWidth: true
                }
            }
        }

        // ── MC auto-language ──
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: langModeCard.implicitHeight + 28
            radius: StyleTokens.radiusLg; color: StyleTokens.bgSecondary; border.color: StyleTokens.bgInput
            ColumnLayout {
                id: langModeCard
                anchors.fill: parent; anchors.margins: 14; spacing: 6
                RowLayout {
                    Layout.fillWidth: true; spacing: 6
                    Text { text: qsTr("游戏语言自动调整"); font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.DemiBold; color: StyleTokens.textPrimary }
                    Rectangle { radius: StyleTokens.radiusSm; height: 18; width: langNewTag.implicitWidth + 10; color: StyleTokens.infoBg
                        Text { id: langNewTag; anchors.centerIn: parent; text: qsTr("新增"); font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.accentLink }
                    }
                    Item { Layout.fillWidth: true }
                    ShadowDropdown {
                        id: langModeCombo
                        model: [qsTr("系统区域（启动时自动检测）"), qsTr("IP 属地（安装时自动调整）"), qsTr("关闭（手动设置）")]
                        Layout.preferredWidth: 280
                        placeholderText: ""
                        Component.onCompleted: {
                            currentValue = model[page._initLangModeIdx]
                        }
                        onValueSelected: function(v) {
                            backend.setAutoLangModeFromCombo(model.indexOf(v))
                        }
                    }
                }
                Text {
                    text: {
                        switch (langModeCombo.model.indexOf(langModeCombo.currentValue)) {
                            case 0: return qsTr("每次启动游戏时根据电脑系统区域自动写入 options.txt") + "（默认识别 zh_CN / zh_HK / zh_TW / ja_JP / ko_KR）"
                            case 1: return qsTr("安装版本后根据 IP 属地自动写入 options.txt，启动时不覆盖")
                            case 2: return qsTr("不自动调整游戏语言，可在 Minecraft 游戏设置中手动选择")
                            default: return ""
                        }
                    }
                    font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textSubtle; wrapMode: Text.WordWrap; Layout.fillWidth: true
                }
            }
        }

        // ── Custom background ──
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: bgCardContent.implicitHeight + 28
            radius: StyleTokens.radiusLg; color: StyleTokens.bgSecondary; border.color: StyleTokens.bgInput
            ColumnLayout {
                id: bgCardContent
                anchors.fill: parent; anchors.margins: 14; spacing: 8
                RowLayout {
                    Layout.fillWidth: true; spacing: 6
                    Text { text: qsTr("自定义背景"); font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.DemiBold; color: StyleTokens.textPrimary }
                    Rectangle { radius: StyleTokens.radiusSm; height: 18; width: bgTag.implicitWidth + 10; color: StyleTokens.bgHover
                        Text { id: bgTag; anchors.centerIn: parent; text: qsTr("实验性"); font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.accentLink }
                    }
                    Item { Layout.fillWidth: true }
                    Text { text: hasBg ? qsTr("已设置") : qsTr("未设置"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary }
                    Rectangle { width: 60; height: 28; radius: StyleTokens.radiusSm; color: bgBrowseHov.pressed ? StyleTokens.accentSubtle : (bgBrowseHov.containsMouse ? StyleTokens.bgHover : StyleTokens.bgSecondary); border.color: bgBrowseHov.containsMouse ? StyleTokens.accentHover : StyleTokens.bgHover
                        Behavior on color { ColorAnimation { duration: 150 } }
                        Behavior on border.color { ColorAnimation { duration: 150 } }
                        scale: bgBrowseHov.containsMouse ? (bgBrowseHov.pressed ? 0.94 : 1.04) : 1.0
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        Text { anchors.centerIn: parent; text: qsTr("浏览"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textSecondary }
                        MouseArea { id: bgBrowseHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            onClicked: { if (backend) backend.pickBackgroundImage() }
                        }
                    }
                    Rectangle { visible: hasBg; width: 60; height: 28; radius: StyleTokens.radiusSm; color: bgClearHov.pressed ? StyleTokens.errorBg : (bgClearHov.containsMouse ? StyleTokens.errorBg : StyleTokens.accentSubtle); border.color: bgClearHov.containsMouse ? StyleTokens.errorLight : StyleTokens.bgHover
                        Behavior on color { ColorAnimation { duration: 150 } }
                        Behavior on border.color { ColorAnimation { duration: 150 } }
                        scale: bgClearHov.containsMouse ? (bgClearHov.pressed ? 0.94 : 1.04) : 1.0
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        Text { anchors.centerIn: parent; text: qsTr("清除"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.errorLight }
                        MouseArea { id: bgClearHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            onClicked: { if (backend) backend.setCustomBgPath("") }
                        }
                    }
                }

                // Preview thumbnail (click to adjust crop)
                Item {
                    id: bgPreviewWrap
                    Layout.fillWidth: true; Layout.preferredHeight: 100
                    visible: hasBg
                    Image {
                        id: bgPreview
                        anchors.fill: parent; fillMode: Image.PreserveAspectFit
                        source: hasBg ? backend.customBgPath : ""
                        cache: false; asynchronous: true
                    }
                    MouseArea {
                        anchors.fill: parent; visible: hasBg
                        hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: { bgCropOverlay.active = true }
                        scale: containsMouse ? (pressed ? 0.95 : 1.05) : 1.0
                        Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                    }
                    Rectangle {
                        anchors.fill: parent
                        color: "transparent"; border.color: StyleTokens.accentHover; border.width: 1.5; radius: StyleTokens.radiusSm
                    }
                }
                Rectangle { visible: hasBg; Layout.fillWidth: true; height: 1; color: StyleTokens.bgInput }

                // Hint text
                Text {
                    visible: hasBg; Layout.fillWidth: true; wrapMode: Text.WordWrap
                    font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.textMuted
                    text: qsTr("当前预览的是所选中的整个图片，而非裁剪后的图片。点击图片，可手动更改选区范围。")
                }

                // Sliders
                ColumnLayout {
                    visible: hasBg
                    Layout.fillWidth: true; spacing: 10
                    RowLayout {
                        Layout.fillWidth: true; spacing: 16
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 4
                        Text { text: qsTr("菜单栏透明度"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary }
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
                                        width: sidebarSPSlider.visualPosition * parent.width; height: 4; radius: StyleTokens.radiusXs; color: StyleTokens.accent
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
                            Text { text: (backend ? backend.sidebarOpacity : 0.90).toFixed(2); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary; Layout.preferredWidth: 32 }
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 4
                        Text { text: qsTr("背景明暗度"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary }
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
                                        width: contentSPSlider.visualPosition * parent.width; height: 4; radius: StyleTokens.radiusXs; color: StyleTokens.accent
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
                            Text { text: (backend ? backend.contentOpacity : 0.70).toFixed(2); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary; Layout.preferredWidth: 32 }
                        }
                    }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 4
                        Text { text: qsTr("背景模糊度"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary }
                        RowLayout { Layout.fillWidth: true; spacing: 8
                            Slider {
                                id: blurSPSlider
                                Layout.fillWidth: true; from: 0.0; to: 1.0; stepSize: 0.05
                                value: backend ? backend.backgroundBlur : 0.0
                                onMoved: { if (backend) backend.setBackgroundBlur(value) }
                                background: Rectangle {
                                    implicitHeight: 4
                                    x: blurSPSlider.leftPadding
                                    y: blurSPSlider.topPadding + blurSPSlider.availableHeight / 2 - height / 2
                                    width: blurSPSlider.availableWidth; height: 4; radius: StyleTokens.radiusXs; color: StyleTokens.bgHover
                                    Rectangle {
                                        width: blurSPSlider.visualPosition * parent.width; height: 4; radius: StyleTokens.radiusXs; color: StyleTokens.accent
                                    }
                                }
                                handle: Rectangle {
                                    implicitWidth: 12; implicitHeight: 12
                                    x: blurSPSlider.leftPadding + blurSPSlider.visualPosition * (blurSPSlider.availableWidth - width)
                                    y: blurSPSlider.topPadding + blurSPSlider.availableHeight / 2 - height / 2
                                    radius: StyleTokens.radiusMd; color: StyleTokens.accentLight
                                    Behavior on x { SmoothedAnimation { velocity: 600; duration: 200 } }
                                }
                            }
                            Text { text: (backend ? backend.backgroundBlur : 0.0).toFixed(2); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary; Layout.preferredWidth: 32 }
                        }
                    }
                }

                Text {
                    text: qsTr("选择一张图片作为启动器背景。菜单栏透明度、背景明暗度和背景模糊度可分别调节。")
                    font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textSubtle; wrapMode: Text.WordWrap; Layout.fillWidth: true
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
