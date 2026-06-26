import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: page
    color: "transparent"

    property int currentSection: 0

    opacity: 0; y: 10
    Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeInDuration; easing.type: AnimationTokens.itemFadeInEasing } }
    Behavior on y { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
    Component.onCompleted: { opacity = 1; y = 0; appWindow.pageLoading = false }

    // Called when switching sections — show loading bar
    function switchSection(idx) {
        if (idx === currentSection) return
        currentSection = idx
        if (appWindow) appWindow.pageLoading = true
        sectionLoadTimer.restart()
    }
    Timer {
        id: sectionLoadTimer
        interval: 60
        onTriggered: { appWindow.pageLoading = false }
    }

    RowLayout {
        anchors.fill: parent; spacing: 0

        // Left nav
        Rectangle {
            Layout.preferredWidth: 180; Layout.fillHeight: true; color: "transparent"

            ListView {
                id: nav; anchors.fill: parent; anchors.margins: 8

                function navLabel(key) {
                    switch (key) {
                        case "general": return qsTr("通用设置")
                        case "java": return qsTr("Java 设置")
                        case "memory": return qsTr("内存设置")
                        case "experimental": return qsTr("实验性功能")
                        case "about": return qsTr("关于")
                        default: return key
                    }
                }

                model: [
                    { label: "通用设置", icon: "settings", key: "general" },
                    { label: "Java 设置", icon: "terminal", key: "java" },
                    { label: "内存设置", icon: "cpu", key: "memory" },
                    { label: "实验性功能", icon: "flask-conical", key: "experimental" },
                    { label: "关于", icon: "info", key: "about" }
                ]
                currentIndex: 0; spacing: 2

                delegate: Rectangle {
                    width: nav.width - 16; height: 38; radius: 6
                    color: nav.currentIndex === index ? "#181c28" : (navMouse.containsMouse ? "#11141c" : "transparent")
                    scale: navMouse.containsMouse ? 1.02 : 1.0
                    Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }
                    Row {
                        anchors.left: parent.left; anchors.leftMargin: 14; anchors.verticalCenter: parent.verticalCenter
                        spacing: 8
                        Image {
                            anchors.verticalCenter: parent.verticalCenter
                            source: "icons/lucide/" + modelData.icon + ".svg"
                            width: 16; height: 16
                        }
                        Text {
                            text: nav.navLabel(modelData.key); color: nav.currentIndex === index ? "#e8ecf8" : "#8890a0"; font.pixelSize: 13
                            font.weight: nav.currentIndex === index ? Font.DemiBold : Font.Normal
                        }
                    }
                    MouseArea { id: navMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { nav.currentIndex = index; page.switchSection(index) } }
                }
            }

            Rectangle {
                id: settingsIndicator
                z: 10
                x: 8; y: 8 + nav.currentIndex * 40
                width: 2; height: 38; color: "#5d6fe0"; radius: 1
                Behavior on y { SmoothedAnimation { velocity: 200; duration: 300 } }
            }
        }

        // Divider
        Rectangle { Layout.preferredWidth: 1; Layout.fillHeight: true; color: "#1a1f2a" }

        // Right content — lazy-loaded sections
        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true; color: "transparent"

            // Section 0: General (loaded immediately)
            Loader {
                id: generalLoader
                anchors.fill: parent; anchors.margins: 24
                active: page.currentSection === 0
                visible: status === Loader.Ready
                sourceComponent: generalComponent
                asynchronous: true
            }

            // Section 1: Java (lazy loaded, cached after first load)
            Loader {
                id: javaLoader
                anchors.fill: parent; anchors.margins: 24
                active: page.currentSection === 1
                visible: status === Loader.Ready
                sourceComponent: javaComponent
                asynchronous: true
            }

            // Section 2: Memory (lazy loaded, cached after first load)
            Loader {
                id: memoryLoader
                anchors.fill: parent; anchors.margins: 24
                active: page.currentSection === 2
                visible: status === Loader.Ready
                sourceComponent: memoryComponent
                asynchronous: true
            }

            // Section 3: Experimental (lazy loaded)
            Loader {
                id: experimentalLoader
                anchors.fill: parent; anchors.margins: 24
                active: page.currentSection === 3
                visible: status === Loader.Ready
                sourceComponent: experimentalComponent
                asynchronous: true
            }

            // Section 4: About (lazy loaded)
            Loader {
                id: aboutLoader
                anchors.fill: parent; anchors.margins: 24
                active: page.currentSection === 4
                visible: status === Loader.Ready
                sourceComponent: aboutComponent
                asynchronous: true
            }
        }
    }

    // ────── COMPONENTS ──────

    Component {
        id: generalComponent
        Item {
            ColumnLayout {
                anchors.fill: parent; spacing: 12
                Text { text: qsTr("通用设置"); font.pixelSize: 18; font.bold: true; color: "#e8ecf8" }

                Rectangle { Layout.fillWidth: true; height: 52; radius: 7; color: "#11141c"; border.color: "#1a1f2a"
                    RowLayout {
                        anchors.left: parent.left; anchors.leftMargin: 14
                        anchors.right: parent.right; anchors.rightMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 8
                        Text { text: qsTr("版本隔离"); font.pixelSize: 13; font.weight: Font.Medium; color: "#e8ecf8"; Layout.fillWidth: true }
                        Text { text: backend && backend.isolationEnabled ? "已开启" : "已关闭"; font.pixelSize: 11; color: "#8088a0" }
                        Switch { id: isolationSwitch; Layout.alignment: Qt.AlignVCenter; checked: backend ? backend.isolationEnabled : false; onToggled: { if (backend) backend.setIsolationEnabled(checked) }
                            palette { mid: "#3a4eb8"; dark: "#252835"; light: "#5d6fe0"; button: checked ? "#5d6fe0" : "#353845" }
                        }
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 52; radius: 7; color: "#11141c"; border.color: "#1a1f2a"
                    RowLayout {
                        anchors.left: parent.left; anchors.leftMargin: 14
                        anchors.right: parent.right; anchors.rightMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 8
                        Text { text: qsTr("启动后关闭启动器"); font.pixelSize: 13; font.weight: Font.Medium; color: "#e8ecf8"; Layout.fillWidth: true }
                        Switch { Layout.alignment: Qt.AlignVCenter; checked: backend ? backend.closeAfterLaunch : false; onToggled: { if (backend) backend.setCloseAfterLaunch(checked) }
                            palette { mid: "#3a4eb8"; dark: "#252835"; light: "#5d6fe0"; button: checked ? "#5d6fe0" : "#353845" }
                        }
                    }
                }
                Item { Layout.fillHeight: true }
            }
        }
    }

    Component {
        id: javaComponent
        Item {
            ScrollView {
                anchors.fill: parent; clip: true
                ColumnLayout {
                    width: parent.width; spacing: 12
                    Text { text: qsTr("Java 设置"); font.pixelSize: 18; font.bold: true; color: "#e8ecf8" }

                    Rectangle { Layout.fillWidth: true; radius: 8; color: "#11141c"; border.color: "#1a1f2a"; Layout.preferredHeight: 90
                        ColumnLayout { anchors.fill: parent; anchors.margins: 16; spacing: 8
                            RowLayout { Layout.fillWidth: true; spacing: 8
                                Text { text: qsTr("当前 Java"); font.pixelSize: 13; color: "#e8ecf8" }
                                Rectangle { radius: 4; height: 22; width: statusLabel.implicitWidth + 14; color: backend && backend.javaInstalled ? "#1a3028" : "#301a1a"
                                    Text { id: statusLabel; anchors.centerIn: parent; text: backend && backend.javaInstalled ? "已检测" : "未检测到"; color: backend && backend.javaInstalled ? "#4bc870" : "#e05050"; font.pixelSize: 11 }
                                }
                                Item { Layout.fillWidth: true }
                            }
                            Text { text: backend ? (backend.javaVersion || "未知版本") : ""; font.pixelSize: 15; font.bold: true; color: "#e8ecf8"; Layout.fillWidth: true }
                            Text { text: backend ? (backend.javaPath || "未找到 Java 可执行文件") : "未找到 Java"; font.pixelSize: 11; font.family: "Consolas, monospace"; color: "#8890a0"; elide: Text.ElideMiddle; Layout.fillWidth: true }
                        }
                    }

                    Text { text: qsTr("可用 Java"); font.pixelSize: 13; font.weight: Font.DemiBold; color: "#b8c0d0"; visible: javaListView.visible }
                    ListView {
                        id: javaListView; Layout.fillWidth: true
                        layoutDirection: Qt.LeftToRight; orientation: ListView.Vertical
                        interactive: false
                        height: Math.min(count * 50, 200)
                        visible: count > 0
                        model: backend ? backend.availableJavaList : []
                        delegate: Rectangle {
                            width: javaListView.width; height: 46; radius: 6
                            color: javaRow.containsMouse ? "#151a28" : "transparent"
                            border.color: backend.javaPath === modelData.path ? "#5068d8" : "transparent"
                            RowLayout {
                                anchors.fill: parent; anchors.margins: 12; spacing: 10
                                Rectangle { width: 8; height: 8; radius: 4; color: backend.javaPath === modelData.path ? "#5d6fe0" : "#505468" }
                                ColumnLayout { spacing: 2; Layout.fillWidth: true
                                    Text { text: "Java " + modelData.major; font.pixelSize: 13; color: backend.javaPath === modelData.path ? "#e8ecf8" : "#b8c0d0"; font.weight: backend.javaPath === modelData.path ? Font.DemiBold : Font.Normal }
                                    Text { text: modelData.path; font.pixelSize: 10; font.family: "Consolas, monospace"; color: "#707888"; elide: Text.ElideMiddle; Layout.fillWidth: true }
                                }
                                Text { text: "v" + modelData.version; font.pixelSize: 10; color: "#606478" }
                            }
                            MouseArea { id: javaRow; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { if (backend) backend.selectJavaByIndex(index) }
                            }
                        }
                    }

                    RowLayout { Layout.fillWidth: true; spacing: 10
                        Rectangle { width: detectBtnText.implicitWidth + 24; height: 34; radius: 7; color: detectBtnArea.containsMouse ? "#6d7de8" : "#5068d8"
                            scale: detectBtnArea.pressed ? 0.92 : 1.0
                            Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }
                            Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                            Text { id: detectBtnText; anchors.centerIn: parent; text: qsTr("重新检测"); font.pixelSize: 13; font.weight: Font.DemiBold; color: "#ffffff" }
                            MouseArea { id: detectBtnArea; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { if (backend) { backend.detectJava(); toastManager.show("正在检测 Java 环境...") } } }
                        }
                        Rectangle { width: browseBtnText.implicitWidth + 24; height: 34; radius: 7; color: browseBtnArea.containsMouse ? "#1a1f2e" : "transparent"
                            scale: browseBtnArea.pressed ? 0.92 : 1.0
                            Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }
                            border.color: browseBtnArea.containsMouse ? "#6d7de8" : "#4a5ec8"; border.width: 1.5
                            Text { id: browseBtnText; anchors.centerIn: parent; text: qsTr("手动选择"); font.pixelSize: 13; font.weight: Font.Medium; color: browseBtnArea.containsMouse ? "#ffffff" : "#b0b8e0" }
                            MouseArea { id: browseBtnArea; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { if (backend) { var ok = backend.browseJava(); if (ok) toastManager.show("已选择 Java 路径") } } }
                        }
                        Item { Layout.fillWidth: true }
                    }
                }
            }
        }
    }

    Component {
        id: memoryComponent
        Item {
            ColumnLayout {
                anchors.fill: parent; spacing: 16
                Text { text: qsTr("内存设置"); font.pixelSize: 18; font.bold: true; color: "#e8ecf8" }

                Rectangle { Layout.fillWidth: true; height: 52; radius: 7; color: "#11141c"; border.color: "#1a1f2a"
                    RowLayout {
                        anchors.left: parent.left; anchors.leftMargin: 14
                        anchors.right: parent.right; anchors.rightMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 12
                        Text { text: qsTr("系统内存"); font.pixelSize: 13; color: "#8890a0" }
                        Rectangle { width: 1; height: 14; color: "#1a1f2a" }
                        Text {
                            font.pixelSize: 13; color: "#b8c0d0"; Layout.fillWidth: true
                            text: {
                                if (!backend) return ""
                                var info = backend.systemMemoryInfo
                                if (!info || !info.total) return "未知"
                                var gb = (info.total / 1024).toFixed(1)
                                var availGb = (info.available / 1024).toFixed(1)
                                return availGb + " GB 可用 / " + gb + " GB 总计"
                            }
                        }
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 52; radius: 7; color: "#11141c"; border.color: "#1a1f2a"
                    RowLayout {
                        anchors.left: parent.left; anchors.leftMargin: 14
                        anchors.right: parent.right; anchors.rightMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 8
                        Text { text: qsTr("自动调整"); font.pixelSize: 13; font.weight: Font.Medium; color: "#e8ecf8"; Layout.fillWidth: true }
                        Text { text: autoMemorySwitch.checked ? "系统将根据可用内存自动分配" : "手动设置"; font.pixelSize: 11; color: "#8088a0" }
                        Switch { id: autoMemorySwitch; Layout.alignment: Qt.AlignVCenter; checked: backend ? backend.autoMemoryEnabled : false; onToggled: { if (backend) backend.setAutoMemoryEnabled(checked) }
                            palette { mid: "#3a4eb8"; dark: "#252835"; light: "#5d6fe0"; button: checked ? "#5d6fe0" : "#353845" }
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true; Layout.preferredHeight: autoMemorySwitch.checked ? 0 : childrenRect.height
                    visible: !autoMemorySwitch.checked; clip: true
                    Behavior on Layout.preferredHeight { NumberAnimation { duration: 250 } }

                    ColumnLayout {
                        width: parent.width; spacing: 14
                        Text { text: qsTr("最大内存: ") + maxSlider.value + " MB"; font.pixelSize: 14; color: "#e8ecf8" }
                        Slider {
                            id: maxSlider; Layout.fillWidth: true; from: 1024; to: 16384; stepSize: 512
                            value: backend ? backend.maxMemoryMb : 4096; onMoved: { if (backend) backend.setMaxMemory(value) }
                            background: Rectangle { x: maxSlider.leftPadding; y: maxSlider.topPadding + maxSlider.availableHeight / 2 - 2; width: maxSlider.availableWidth; height: 4; radius: 2; color: "#1a1f2a"
                                Rectangle { width: maxSlider.visualPosition * parent.width; height: 4; radius: 2; color: "#5d6fe0" } }
                            handle: Rectangle { x: maxSlider.leftPadding + maxSlider.visualPosition * (maxSlider.availableWidth - width); y: maxSlider.topPadding + maxSlider.availableHeight / 2 - height / 2; width: 18; height: 18; radius: 9; color: "#5d6fe0" }
                        }
                        Text { text: qsTr("最小内存: ") + minSlider.value + " MB"; font.pixelSize: 14; color: "#e8ecf8" }
                        Slider {
                            id: minSlider; Layout.fillWidth: true; from: 512; to: 2048; stepSize: 256
                            value: backend ? backend.minMemoryMb : 512; onMoved: { if (backend) backend.setMinMemory(value) }
                            background: Rectangle { x: minSlider.leftPadding; y: minSlider.topPadding + minSlider.availableHeight / 2 - 2; width: minSlider.availableWidth; height: 4; radius: 2; color: "#1a1f2a"
                                Rectangle { width: minSlider.visualPosition * parent.width; height: 4; radius: 2; color: "#5d6fe0" } }
                            handle: Rectangle { x: minSlider.leftPadding + minSlider.visualPosition * (minSlider.availableWidth - width); y: minSlider.topPadding + minSlider.availableHeight / 2 - height / 2; width: 18; height: 18; radius: 9; color: "#5d6fe0" }
                        }

                        RowLayout { spacing: 6
                            Repeater { model: [{ label: "低配 (2G)", val: 2048 }, { label: "中配 (4G)", val: 4096 }, { label: "高配 (8G)", val: 8192 }]
                                Rectangle { width: presetLabel.implicitWidth + 20; height: 30; radius: 6; color: presetMouse.containsMouse ? "#5068d8" : "#1f2740"; border.color: presetMouse.containsMouse ? "#6d7de8" : "#2a3d60"
                                    scale: presetMouse.containsMouse ? 1.04 : 1.0
                                    Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }
                                    Text { id: presetLabel; anchors.centerIn: parent; text: modelData.label; color: presetMouse.containsMouse ? "#ffffff" : "#b0c0e8"; font.pixelSize: 11 }
                                    MouseArea { id: presetMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { maxSlider.value = modelData.val; if (backend) backend.setMaxMemory(modelData.val) } }
                                }
                            }
                        }
                    }
                }

                Item { Layout.fillWidth: true; Layout.fillHeight: true; visible: autoMemorySwitch.checked }
                Item { Layout.fillHeight: true }
            }
        }
    }

    Component {
        id: experimentalComponent
        Item {
            ColumnLayout {
                anchors.fill: parent; spacing: 12
                Text { text: qsTr("实验性功能"); font.pixelSize: 18; font.bold: true; color: "#e8ecf8" }
                Text { text: qsTr("以下功能处于实验阶段，可能存在不稳定的情况。"); font.pixelSize: 12; color: "#707888"; wrapMode: Text.WordWrap; Layout.fillWidth: true }

                Rectangle {
                    Layout.fillWidth: true; Layout.preferredHeight: 110; radius: 8; color: "#11141c"; border.color: "#1a1f2a"
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 14; spacing: 8
                        RowLayout {
                            Layout.fillWidth: true; spacing: 6
                            Text { text: qsTr("嵌入式窗口登录"); font.pixelSize: 14; font.weight: Font.DemiBold; color: "#e8ecf8" }
                            Rectangle { radius: 4; height: 18; width: tagText.implicitWidth + 10; color: "#2a2040"
                                Text { id: tagText; anchors.centerIn: parent; text: qsTr("实验性"); font.pixelSize: 10; color: "#a098e0" }
                            }
                            Item { Layout.fillWidth: true }
                            Text { text: embeddedSwitch.checked ? qsTr("已开启") : qsTr("已关闭"); font.pixelSize: 11; color: "#8088a0" }
                            Switch {
                                id: embeddedSwitch; Layout.alignment: Qt.AlignVCenter
                                checked: backend ? backend.embeddedLoginEnabled : false
                                onToggled: { if (backend) backend.setEmbeddedLoginEnabled(checked) }
                                palette { mid: "#3a4eb8"; dark: "#252835"; light: "#5d6fe0"; button: checked ? "#5d6fe0" : "#353845" }
                            }
                        }
                        Text {
                            text: qsTr("在启动器内嵌窗口中完成 Microsoft 登录，无需跳转外部浏览器。\n输入 Windows 已登录的 Microsoft 账户后，会自动唤起系统 PIN/指纹验证窗口，可快捷登录。")
                            font.pixelSize: 11; color: "#707888"; wrapMode: Text.WordWrap; Layout.fillWidth: true
                        }
                    }
                }

                // ── Language selector ──
                Rectangle {
                    Layout.fillWidth: true; Layout.preferredHeight: 110; radius: 8; color: "#11141c"; border.color: "#1a1f2a"
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 14; spacing: 8
                        RowLayout {
                            Layout.fillWidth: true; spacing: 6
                            Text { text: qsTr("界面语言"); font.pixelSize: 14; font.weight: Font.DemiBold; color: "#e8ecf8" }
                            Rectangle { radius: 4; height: 18; width: langTag.implicitWidth + 10; color: "#2a2040"
                                Text { id: langTag; anchors.centerIn: parent; text: qsTr("实验性"); font.pixelSize: 10; color: "#a098e0" }
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
                                    radius: 6; color: langCombo.hovered ? "#252a38" : "#161a24"
                                    border.color: langCombo.hovered ? "#3a4eb8" : "#1e2230"
                                    border.width: 1
                                    Behavior on color { ColorAnimation { duration: 200 } }
                                    Behavior on border.color { ColorAnimation { duration: 200 } }
                                }
                                contentItem: Text {
                                    leftPadding: 12; verticalAlignment: Text.AlignVCenter
                                    text: langCombo.displayText; color: "#d0d4e0"; font.pixelSize: 12
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
                                    width: langCombo.popup.width  // use popup width
                                    contentItem: Text {
                                        text: modelData; color: "#d0d4e0"; font.pixelSize: 12
                                        verticalAlignment: Text.AlignVCenter; leftPadding: 12
                                    }
                                    background: Rectangle {
                                        color: highlighted ? "#252a38" : "transparent"
                                        Behavior on color { ColorAnimation { duration: 150 } }
                                    }
                                    highlighted: langCombo.highlightedIndex === index
                                }
                                popup: Popup {
                                    y: langCombo.height + 4
                                    width: 380
                                    implicitHeight: contentItem.implicitHeight + 8
                                    padding: 4
                                    contentItem: ListView {
                                        clip: true; implicitHeight: contentHeight
                                        model: langCombo.popup.visible ? langCombo.delegateModel : null
                                        currentIndex: langCombo.highlightedIndex
                                    }
                                    background: Rectangle {
                                        radius: 6; color: "#161a24"; border.color: "#1e2230"
                                    }
                                }
                            }
                        }
                        Text {
                            text: qsTr("选择语言后即时生效，无需重启启动器。")
                            font.pixelSize: 11; color: "#707888"; wrapMode: Text.WordWrap; Layout.fillWidth: true
                        }
                    }
                }

                // ── Custom background ──
                Rectangle {
                    Layout.fillWidth: true; Layout.preferredHeight: bgCardContent.implicitHeight + 28
                    radius: 8; color: "#11141c"; border.color: "#1a1f2a"
                    ColumnLayout {
                        id: bgCardContent
                        anchors.fill: parent; anchors.margins: 14; spacing: 8
                        RowLayout {
                            Layout.fillWidth: true; spacing: 6
                            Text { text: qsTr("自定义背景"); font.pixelSize: 14; font.weight: Font.DemiBold; color: "#e8ecf8" }
                            Rectangle { radius: 4; height: 18; width: bgTag.implicitWidth + 10; color: "#2a2040"
                                Text { id: bgTag; anchors.centerIn: parent; text: qsTr("实验性"); font.pixelSize: 10; color: "#a098e0" }
                            }
                            Item { Layout.fillWidth: true }
                            Text { text: backend && backend.customBgPath !== "" ? qsTr("已设置") : qsTr("未设置"); font.pixelSize: 11; color: "#8088a0" }
                            Rectangle { width: 60; height: 28; radius: 5; color: bgBrowseHov.hovered ? "#252a38" : "#161a24"; border.color: "#2a2e3c"
                                scale: bgBrowseHov.containsMouse ? 1.05 : 1.0
                                Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                                Text { anchors.centerIn: parent; text: qsTr("浏览"); font.pixelSize: 11; color: "#d0d4e0" }
                                MouseArea { id: bgBrowseHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                    onClicked: { if (backend) backend.pickBackgroundImage() }
                                }
                            }
                            Rectangle { visible: backend ? backend.customBgPath !== "" : false; width: 60; height: 28; radius: 5; color: bgClearHov.hovered ? "#302020" : "#161a24"; border.color: "#2a2e3c"
                                scale: bgClearHov.containsMouse ? 1.05 : 1.0
                                Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                                Text { anchors.centerIn: parent; text: qsTr("清除"); font.pixelSize: 11; color: "#d08080" }
                                MouseArea { id: bgClearHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                    onClicked: { if (backend) backend.setCustomBgPath("") }
                                }
                            }
                        }

                        // Preview thumbnail
                        Image {
                            Layout.fillWidth: true; Layout.preferredHeight: 100
                            visible: backend ? backend.customBgPath !== "" : false
                            source: visible ? ("file:///" + backend.customBgPath) : ""
                            fillMode: Image.PreserveAspectFit
                            cache: false; asynchronous: true
                        }
                        Rectangle { visible: backend ? backend.customBgPath !== "" : false; Layout.fillWidth: true; height: 1; color: "#1a1f2a" }

                        // Sliders
                        RowLayout {
                            visible: backend ? backend.customBgPath !== "" : false
                            Layout.fillWidth: true; spacing: 16
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 4
                                Text { text: qsTr("菜单栏透明度"); font.pixelSize: 11; color: "#b0b8c8" }
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
                                            width: sidebarSPSlider.availableWidth; height: 4; radius: 2; color: "#252a38"
                                            Rectangle {
                                                width: sidebarSPSlider.visualPosition * parent.width; height: 4; radius: 2; color: "#3a4eb8"
                                            }
                                        }
                                        handle: Rectangle {
                                            implicitWidth: 12; implicitHeight: 12
                                            x: sidebarSPSlider.leftPadding + sidebarSPSlider.visualPosition * (sidebarSPSlider.availableWidth - width)
                                            y: sidebarSPSlider.topPadding + sidebarSPSlider.availableHeight / 2 - height / 2
                                            radius: 6; color: "#6080e8"
                                        }
                                    }
                                    Text { text: (backend ? backend.sidebarOpacity : 0.90).toFixed(2); font.pixelSize: 11; color: "#a0a8c0"; Layout.preferredWidth: 32 }
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 4
                                Text { text: qsTr("主页面透明度"); font.pixelSize: 11; color: "#b0b8c8" }
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
                                            width: contentSPSlider.availableWidth; height: 4; radius: 2; color: "#252a38"
                                            Rectangle {
                                                width: contentSPSlider.visualPosition * parent.width; height: 4; radius: 2; color: "#3a4eb8"
                                            }
                                        }
                                        handle: Rectangle {
                                            implicitWidth: 12; implicitHeight: 12
                                            x: contentSPSlider.leftPadding + contentSPSlider.visualPosition * (contentSPSlider.availableWidth - width)
                                            y: contentSPSlider.topPadding + contentSPSlider.availableHeight / 2 - height / 2
                                            radius: 6; color: "#6080e8"
                                        }
                                    }
                                    Text { text: (backend ? backend.contentOpacity : 0.70).toFixed(2); font.pixelSize: 11; color: "#a0a8c0"; Layout.preferredWidth: 32 }
                                }
                            }
                        }

                        Text {
                            text: qsTr("选择一张图片作为启动器背景。菜单栏和主页面可分别调节透明度。")
                            font.pixelSize: 11; color: "#707888"; wrapMode: Text.WordWrap; Layout.fillWidth: true
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }
    }

    Component {
        id: aboutComponent
        Item {
            ColumnLayout {
                anchors.fill: parent; spacing: 14
                Text { text: qsTr("关于"); font.pixelSize: 18; font.bold: true; color: "#e8ecf8" }
                Rectangle { Layout.fillWidth: true; radius: 8; color: "#11141c"; border.color: "#1a1f2a"; Layout.preferredHeight: 120
                    ColumnLayout { anchors.fill: parent; anchors.margins: 18; spacing: 8
                        Text { text: "Shadow Launcher"; font.pixelSize: 16; font.bold: true; color: "#e8ecf8" }
                        Text { text: qsTr("版本 v0.3.0"); font.pixelSize: 12; color: "#b8c0d0" }
                        Text { text: qsTr("基于 PySide6 + QML"); font.pixelSize: 12; color: "#8890a0" }
                        Text { text: qsTr("Minecraft 第三方启动器"); font.pixelSize: 12; color: "#707888" }
                    }
                }
                Item { Layout.fillHeight: true }
            }
        }
    }
}
