import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    width: parent ? parent.width : 400
    height: parent ? parent.height : 300
    color: "#121418"
    id: root

    property var backend: null
    signal goBack()

    // ── Color Palette ──
    readonly property string colorBg:          "#1A1D24"
    readonly property string colorBorder:      "#2A2F3A"
    readonly property string colorPrimary:     "#F1F3F6"
    readonly property string colorSecondary:   "#B4BAC6"
    readonly property string colorTertiary:    "#7E8596"
    readonly property string colorQuaternary:  "#5A6173"
    readonly property string colorAccent:      "#3B82F6"
    readonly property string colorAccentHover: "#2563EB"
    readonly property string colorError:       "#EF4444"
    readonly property string colorSuccess:     "#10B981"
    readonly property string colorWarning:     "#F59E0B"
    readonly property int    radius:           8

    // ── Default JVM args placeholder ──
    readonly property string defaultJvmArgs:
        "-XX:+UnlockExperimentalVMOptions -XX:+UseG1GC " +
        "-XX:G1NewSizePercent=20 -XX:G1ReservePercent=20 " +
        "-XX:MaxGCPauseMillis=50 -XX:G1HeapRegionSize=16M " +
        "-XX:ParallelGCThreads=4 -XX:ConcGCThreads=2 " +
        "-XX:+DisableExplicitGC -Dsun.rmi.dgc.server.gcInterval=2147483646"

    // ── State ──
    QtObject {
        id: d

        property string manualJavaPath: ""
        property string scannedJavaCount: ""
        property bool   requireHighPerfGpu: false
        property string jvmArgs: ""
        property bool   javaFieldFocused: false
    }

    // ── Bindings to backend properties ──
    Binding {
        target: d
        property: "manualJavaPath"
        value: backend ? backend.javaPath : ""
    }

    // ── Page enter animation ──
    states: State {
        name: "visible"
        PropertyChanges { target: root; opacity: 1 }
        PropertyChanges { target: root; y: 0 }
    }

    transitions: Transition {
        NumberAnimation { properties: "opacity,y"; duration: 300; easing.type: Easing.OutCubic }
    }

    opacity: 0
    y: 20

    Component.onCompleted: root.state = "visible"

    // ═══════════════════════════════════════════════════════════
    //  LAYOUT
    // ═══════════════════════════════════════════════════════════
    Flickable {
        anchors.fill: parent
        contentHeight: contentColumn.implicitHeight + 40
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: contentColumn
            width: parent.width - 40
            x: 20
            spacing: 24

            // ── Top Bar ──
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                Layout.topMargin: 12

                Rectangle {
                    id: backButton
                    width: backLabel.implicitWidth + 24
                    height: 36
                    radius: root.radius
                    color: "transparent"
                    anchors.verticalCenter: parent.verticalCenter

                    Label {
                        id: backLabel
                        anchors.centerIn: parent
                        text: qsTr("← 设置")
                        color: root.colorTertiary
                        font.pixelSize: 15
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        hoverEnabled: true
                        onClicked: root.goBack()

                        Rectangle {
                            anchors.fill: parent
                            radius: root.radius
                            color: root.colorBorder
                            opacity: parent.containsMouse ? 0.3 : 0
                            Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeOutDuration; easing.type: AnimationTokens.itemFadeOutEasing } }
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    text: qsTr("Java配置")
                    color: root.colorPrimary
                    font.pixelSize: 20
                    font.bold: true
                }
            }

            // ── Java选择 ──
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                Label {
                    text: qsTr("Java选择")
                    color: root.colorTertiary
                    font.pixelSize: 12
                    font.bold: true
                }

                // Read-only path field
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 42
                    radius: root.radius
                    color: root.colorBg
                    border.color: root.colorBorder
                    border.width: 1

                    Label {
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 14
                        verticalAlignment: Text.AlignVCenter
                        text: (backend && backend.javaPath ? backend.javaPath : d.manualJavaPath) || "未设置 — 将自动检测"
                        color: (backend && backend.javaPath ? true : d.manualJavaPath !== "") ? root.colorSecondary : root.colorQuaternary
                        font.pixelSize: 13
                        elide: Text.ElideMiddle
                    }
                }

                // Detected version
                // Detected version — visible when backend has a version
                Label {
                    visible: backend && backend.javaVersion !== ""
                    text: backend ? ("已检测版本: Java " + backend.javaMajor + " (" + backend.javaVersion + ")") : ""
                    color: root.colorSuccess
                    font.pixelSize: 12
                    Layout.leftMargin: 4
                }

                // Buttons row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    // ── 自动搜索 button ──
                    Rectangle {
                        id: autoSearchBtn
                        Layout.fillWidth: true
                        Layout.preferredHeight: 42
                        radius: root.radius
                        color: "transparent"
                        border.color: root.colorAccent
                        border.width: 1.5
                        opacity: 1

                        Label {
                            anchors.centerIn: parent
                            text: qsTr("自动搜索")
                            color: root.colorAccent
                            font.pixelSize: 14
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true
                            onClicked: {
                                if (backend) {
                                    var scanned = backend.scanJavaInstallations()
                                    console.log("[Java] Scanned, found:", scanned.length)
                                    if (scanned.length > 0) {
                                        d.scannedJavaCount = "找到 " + scanned.length + " 个 Java 安装"
                                        backend.autoSelectJava()
                                    } else {
                                        d.scannedJavaCount = "未找到 Java，请手动指定路径"
                                    }
                                }
                            }

                            Rectangle {
                                anchors.fill: parent
                                radius: root.radius
                                color: root.colorAccent
                                opacity: parent.containsMouse ? 0.1 : 0
                                Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeOutDuration; easing.type: AnimationTokens.itemFadeOutEasing } }
                            }
                        }
                    }

                    // ── 手动选择 button ──
                    Rectangle {
                        id: manualSelectBtn
                        Layout.fillWidth: true
                        Layout.preferredHeight: 42
                        radius: root.radius
                        color: "transparent"
                        border.color: root.colorAccent
                        border.width: 1.5

                        Label {
                            anchors.centerIn: parent
                            text: qsTr("手动选择")
                            color: root.colorAccent
                            font.pixelSize: 14
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true
                            onClicked: {
                                if (backend) { backend.openJavaFileDialog(); toastManager.show("正在打开文件选择器...") }
                            }

                            Rectangle {
                                anchors.fill: parent
                                radius: root.radius
                                color: root.colorAccent
                                opacity: parent.containsMouse ? 0.1 : 0
                                Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeOutDuration; easing.type: AnimationTokens.itemFadeOutEasing } }
                            }
                        }
                    }
                }

                // Info note
                Label {
                    visible: d.scannedJavaCount === ""
                    text: d.scannedJavaCount || "ⓘ 启动器会自动检测系统Java并选择最优版本"
                    color: root.colorTertiary
                    font.pixelSize: 11
                    Layout.leftMargin: 4
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }

            // ── 启动选项 ──
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                Label {
                    text: qsTr("启动选项")
                    color: root.colorTertiary
                    font.pixelSize: 12
                    font.bold: true
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(64, gpuRow.implicitHeight + 24)
                    radius: root.radius
                    color: root.colorBg
                    border.color: root.colorBorder
                    border.width: 1

                    RowLayout {
                        id: gpuRow
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 12

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            Label {
                                text: qsTr("要求Java使用高性能显卡")
                                color: root.colorSecondary
                                font.pixelSize: 14
                            }
                            Label {
                                text: qsTr("在双显卡笔记本上，强制Java使用独立显卡运行Minecraft")
                                color: root.colorTertiary
                                font.pixelSize: 11
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                        }

                        // Custom Switch
                        Rectangle {
                            id: gpuSwitch
                            Layout.preferredWidth: 44
                            Layout.preferredHeight: 24
                            Layout.alignment: Qt.AlignVCenter
                            radius: 12
                            color: d.requireHighPerfGpu ? root.colorAccent : root.colorQuaternary

                            Rectangle {
                                width: 18
                                height: 18
                                radius: 9
                                color: root.colorPrimary
                                anchors.verticalCenter: parent.verticalCenter
                                x: d.requireHighPerfGpu ? parent.width - width - 3 : 3

                                Behavior on x { NumberAnimation { duration: 250; easing.type: Easing.InOutCubic } }
                            }

                            Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: d.requireHighPerfGpu = !d.requireHighPerfGpu
                            }
                        }
                    }
                }
            }

            // ── Java虚拟机参数 ──
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                Label {
                    text: qsTr("Java虚拟机参数")
                    color: root.colorTertiary
                    font.pixelSize: 12
                    font.bold: true
                }

                // Multi-line TextArea
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 160
                    radius: root.radius
                    color: root.colorBg
                    border.color: d.javaFieldFocused ? root.colorAccent : root.colorBorder
                    border.width: 1

                    Behavior on border.color { ColorAnimation { duration: 200 } }

                    ScrollView {
                        anchors.fill: parent
                        anchors.margins: 2
                        clip: true

                        TextArea {
                            id: jvmArgsArea
                            width: parent.width
                            height: parent.height
                            padding: 14
                            color: root.colorPrimary
                            font.pixelSize: 12
                            font.family: "Consolas, monospace"
                            wrapMode: TextArea.Wrap
                            selectByMouse: true
                            persistentSelection: true

                            placeholderText: defaultJvmArgs
                            placeholderTextColor: root.colorQuaternary

                            background: Rectangle {
                                color: "transparent"
                            }

                            text: d.jvmArgs

                            onTextChanged: d.jvmArgs = text
                            onFocusChanged: d.javaFieldFocused = focus

                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.RightButton
                                onClicked: function(mouse) {
                                    contextMenu.popup()
                                }
                            }
                        }
                    }

                    // Context menu for the text area
                    Menu {
                        id: contextMenu
                        MenuItem {
                            text: qsTr("剪切")
                            onTriggered: jvmArgsArea.cut()
                        }
                        MenuItem {
                            text: qsTr("复制")
                            onTriggered: jvmArgsArea.copy()
                        }
                        MenuItem {
                            text: qsTr("粘贴")
                            onTriggered: jvmArgsArea.paste()
                        }
                        MenuItem {
                            text: qsTr("全选")
                            onTriggered: jvmArgsArea.selectAll()
                        }
                    }
                }

                // Restore default button
                Rectangle {
                    id: restoreDefaultsBtn
                    Layout.preferredWidth: restoreDefaultsLabel.implicitWidth + 32
                    Layout.preferredHeight: 36
                    Layout.alignment: Qt.AlignLeft
                    radius: root.radius
                    color: "transparent"
                    border.color: root.colorBorder
                    border.width: 1

                    Label {
                        id: restoreDefaultsLabel
                        anchors.centerIn: parent
                        text: qsTr("恢复默认")
                        color: root.colorSecondary
                        font.pixelSize: 13
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        hoverEnabled: true
                        onClicked: {
                            d.jvmArgs = defaultJvmArgs
                            jvmArgsArea.text = defaultJvmArgs
                        }

                        Rectangle {
                            anchors.fill: parent
                            radius: root.radius
                            color: root.colorBorder
                            opacity: parent.containsMouse ? 0.3 : 0
                            Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeOutDuration; easing.type: AnimationTokens.itemFadeOutEasing } }
                        }
                    }
                }
            }

            // Bottom spacer
            Item { Layout.preferredHeight: 24 }
        }
    }
}
