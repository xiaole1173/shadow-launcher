import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: page
    color: "transparent"

    property int currentSection: 0

    opacity: 0; y: 10
    Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
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
                model: ["通用设置", "Java 设置", "内存设置", "关于"]
                currentIndex: 0; spacing: 2

                delegate: Rectangle {
                    width: nav.width - 16; height: 38; radius: 6
                    color: nav.currentIndex === index ? "#181c28" : (navMouse.containsMouse ? "#11141c" : "transparent")
                    scale: navMouse.containsMouse ? 1.02 : 1.0
                    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                    Text {
                        anchors.left: parent.left; anchors.leftMargin: 14; anchors.verticalCenter: parent.verticalCenter
                        text: modelData; color: nav.currentIndex === index ? "#e8ecf8" : "#8890a0"; font.pixelSize: 13
                        font.weight: nav.currentIndex === index ? Font.DemiBold : Font.Normal
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

            // Section 3: About (lazy loaded)
            Loader {
                id: aboutLoader
                anchors.fill: parent; anchors.margins: 24
                active: page.currentSection === 3
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
                Text { text: "通用设置"; font.pixelSize: 18; font.bold: true; color: "#e8ecf8" }

                Rectangle { Layout.fillWidth: true; height: 52; radius: 7; color: "#11141c"; border.color: "#1a1f2a"
                    RowLayout {
                        anchors.left: parent.left; anchors.leftMargin: 14
                        anchors.right: parent.right; anchors.rightMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 8
                        Text { text: "版本隔离"; font.pixelSize: 13; font.weight: Font.Medium; color: "#e8ecf8"; Layout.fillWidth: true }
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
                        Text { text: "启动后关闭启动器"; font.pixelSize: 13; font.weight: Font.Medium; color: "#e8ecf8"; Layout.fillWidth: true }
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
                    Text { text: "Java 设置"; font.pixelSize: 18; font.bold: true; color: "#e8ecf8" }

                    Rectangle { Layout.fillWidth: true; radius: 8; color: "#11141c"; border.color: "#1a1f2a"; Layout.preferredHeight: 90
                        ColumnLayout { anchors.fill: parent; anchors.margins: 16; spacing: 8
                            RowLayout { Layout.fillWidth: true; spacing: 8
                                Text { text: "当前 Java"; font.pixelSize: 13; color: "#e8ecf8" }
                                Rectangle { radius: 4; height: 22; width: statusLabel.implicitWidth + 14; color: backend && backend.javaInstalled ? "#1a3028" : "#301a1a"
                                    Text { id: statusLabel; anchors.centerIn: parent; text: backend && backend.javaInstalled ? "已检测" : "未检测到"; color: backend && backend.javaInstalled ? "#4bc870" : "#e05050"; font.pixelSize: 11 }
                                }
                                Item { Layout.fillWidth: true }
                            }
                            Text { text: backend ? (backend.javaVersion || "未知版本") : ""; font.pixelSize: 15; font.bold: true; color: "#e8ecf8"; Layout.fillWidth: true }
                            Text { text: backend ? (backend.javaPath || "未找到 Java 可执行文件") : "未找到 Java"; font.pixelSize: 11; font.family: "Consolas, monospace"; color: "#8890a0"; elide: Text.ElideMiddle; Layout.fillWidth: true }
                        }
                    }

                    Text { text: "可用 Java"; font.pixelSize: 13; font.weight: Font.DemiBold; color: "#b8c0d0"; visible: javaListView.visible }
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
                            Behavior on scale { NumberAnimation { duration: 100; easing.type: Easing.OutCubic } }
                            Behavior on color { ColorAnimation { duration: 150 } }
                            Text { id: detectBtnText; anchors.centerIn: parent; text: "重新检测"; font.pixelSize: 13; font.weight: Font.DemiBold; color: "#ffffff" }
                            MouseArea { id: detectBtnArea; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { if (backend) { backend.detectJava(); toastManager.show("正在检测 Java 环境...") } } }
                        }
                        Rectangle { width: browseBtnText.implicitWidth + 24; height: 34; radius: 7; color: browseBtnArea.containsMouse ? "#1a1f2e" : "transparent"
                            scale: browseBtnArea.pressed ? 0.92 : 1.0
                            Behavior on scale { NumberAnimation { duration: 100; easing.type: Easing.OutCubic } }
                            border.color: browseBtnArea.containsMouse ? "#6d7de8" : "#4a5ec8"; border.width: 1.5
                            Text { id: browseBtnText; anchors.centerIn: parent; text: "手动选择"; font.pixelSize: 13; font.weight: Font.Medium; color: browseBtnArea.containsMouse ? "#ffffff" : "#b0b8e0" }
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
                Text { text: "内存设置"; font.pixelSize: 18; font.bold: true; color: "#e8ecf8" }

                Rectangle { Layout.fillWidth: true; height: 52; radius: 7; color: "#11141c"; border.color: "#1a1f2a"
                    RowLayout {
                        anchors.left: parent.left; anchors.leftMargin: 14
                        anchors.right: parent.right; anchors.rightMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 12
                        Text { text: "系统内存"; font.pixelSize: 13; color: "#8890a0" }
                        Rectangle { width: 1; height: 14; color: "#1a1f2a" }
                        Text { text: backend ? backend.systemMemoryInfo || "未知" : ""; font.pixelSize: 13; color: "#b8c0d0"; Layout.fillWidth: true }
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 52; radius: 7; color: "#11141c"; border.color: "#1a1f2a"
                    RowLayout {
                        anchors.left: parent.left; anchors.leftMargin: 14
                        anchors.right: parent.right; anchors.rightMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 8
                        Text { text: "自动调整"; font.pixelSize: 13; font.weight: Font.Medium; color: "#e8ecf8"; Layout.fillWidth: true }
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
                        Text { text: "最大内存: " + maxSlider.value + " MB"; font.pixelSize: 14; color: "#e8ecf8" }
                        Slider {
                            id: maxSlider; Layout.fillWidth: true; from: 1024; to: 16384; stepSize: 512
                            value: backend ? backend.maxMemoryMb : 4096; onMoved: { if (backend) backend.setMaxMemory(value) }
                            background: Rectangle { x: maxSlider.leftPadding; y: maxSlider.topPadding + maxSlider.availableHeight / 2 - 2; width: maxSlider.availableWidth; height: 4; radius: 2; color: "#1a1f2a"
                                Rectangle { width: maxSlider.visualPosition * parent.width; height: 4; radius: 2; color: "#5d6fe0" } }
                            handle: Rectangle { x: maxSlider.leftPadding + maxSlider.visualPosition * (maxSlider.availableWidth - width); y: maxSlider.topPadding + maxSlider.availableHeight / 2 - height / 2; width: 18; height: 18; radius: 9; color: "#5d6fe0" }
                        }
                        Text { text: "最小内存: " + minSlider.value + " MB"; font.pixelSize: 14; color: "#e8ecf8" }
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
                                    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
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
        id: aboutComponent
        Item {
            ColumnLayout {
                anchors.fill: parent; spacing: 14
                Text { text: "关于"; font.pixelSize: 18; font.bold: true; color: "#e8ecf8" }
                Rectangle { Layout.fillWidth: true; radius: 8; color: "#11141c"; border.color: "#1a1f2a"; Layout.preferredHeight: 120
                    ColumnLayout { anchors.fill: parent; anchors.margins: 18; spacing: 8
                        Text { text: "Shadow Launcher"; font.pixelSize: 16; font.bold: true; color: "#e8ecf8" }
                        Text { text: "版本 v0.3.0"; font.pixelSize: 12; color: "#b8c0d0" }
                        Text { text: "基于 PySide6 + QML"; font.pixelSize: 12; color: "#8890a0" }
                        Text { text: "Minecraft 第三方启动器"; font.pixelSize: 12; color: "#707888" }
                    }
                }
                Item { Layout.fillHeight: true }
            }
        }
    }
}
