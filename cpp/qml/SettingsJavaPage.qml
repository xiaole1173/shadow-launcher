// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// ═══ 全局 Java 设置 — Section 嵌入 SettingsPage ═══
Item {
    id: root
    property var backend: null
    property var toastManager: null

    // ── State ──
    property int _selectedJavaIndex: -1
    property var _javaList: []
    property bool _jvmFocused: false
    property string _jvmWarning: ""

    // ── Default JVM args ──
    readonly property string defaultJvmArgs:
        "-XX:+UseG1GC -XX:+UnlockExperimentalVMOptions " +
        "-XX:G1NewSizePercent=20 -XX:MaxGCPauseMillis=50 " +
        "-XX:G1HeapRegionSize=16M -XX:+DisableExplicitGC"

    function _updateJavaIndex() {
        var path = backend ? backend.javaPath : ""
        var list = _javaList || []
        for (var i = 0; i < list.length; i++) {
            if (list[i].path === path) { _selectedJavaIndex = i; return }
        }
        _selectedJavaIndex = -1
    }

    function refreshAll() {
        if (!backend) return
        _javaList = backend.availableJavaList || []
        _updateJavaIndex()
    }

    // Real-time JVM arg validation
    function _validateJvmArgs(args) {
        var trimmed = args ? args.trim() : ""

        // Empty → warn, fallback silently handled by _effectiveJvmArgs()
        if (!trimmed) {
            root._jvmWarning = qsTr("JVM 参数为空，启动时将加载默认参数")
            return
        }

        // Garbage detection: lines not starting with -D/-X/-XX:/--
        var lines = trimmed.split(/[\r\n]+/)
        for (var l = 0; l < lines.length; l++) {
            var line = lines[l].trim()
            if (!line) continue
            if (line[0] !== '-' && line[0] !== '@') {
                root._jvmWarning = qsTr("检测到无效参数: \"") + line + qsTr("\"，JVM 参数应以 -D/-X/-XX: 开头")
                return
            }
        }

        // GC conflict
        var gcFlags = []
        if (trimmed.indexOf("-XX:+UseG1GC") >= 0) gcFlags.push("G1GC")
        if (trimmed.indexOf("-XX:+UseZGC") >= 0) gcFlags.push("ZGC")
        if (trimmed.indexOf("-XX:+UseShenandoahGC") >= 0) gcFlags.push("Shenandoah")
        if (trimmed.indexOf("-XX:+UseParallelGC") >= 0) gcFlags.push("Parallel")
        if (trimmed.indexOf("-XX:+UseSerialGC") >= 0) gcFlags.push("Serial")
        if (gcFlags.length > 1) {
            root._jvmWarning = qsTr("检测到多个 GC 冲突: ") + gcFlags.join(" + ") + qsTr("，仅最后一个生效")
            return
        }

        root._jvmWarning = ""
    }

    Connections {
        target: backend
    }

    Component.onCompleted: refreshAll()

    // ═══════════════════════════════════════════════════════════
    ScrollView {
        id: scrollView
        anchors.fill: parent; clip: true

        ColumnLayout {
            width: scrollView.width; spacing: 14

            // ── Title ──
            Text {
                text: qsTr("Java 设置")
                font.pixelSize: StyleTokens.fontSizeXl; font.bold: true; color: StyleTokens.textPrimary
            }

            // ══ 1. 当前 Java 信息卡片 ══
            Rectangle {
                Layout.fillWidth: true; implicitHeight: cardContent.implicitHeight + 32
                radius: StyleTokens.radiusLg; color: StyleTokens.bgSecondary; border.color: StyleTokens.bgInput

                ColumnLayout {
                    id: cardContent
                    anchors.fill: parent; anchors.margins: 16; spacing: 10

                    // Status row
                    RowLayout {
                        Layout.fillWidth: true; spacing: 8
                        Text {
                            text: qsTr("当前 Java")
                            font.pixelSize: StyleTokens.fontSizeMd; color: StyleTokens.textPrimary
                        }
                        Rectangle {
                            radius: StyleTokens.radiusSm; height: 22
                            width: statusLabel.implicitWidth + 14
                            color: backend && backend.javaInstalled ? "#1a3028" : "#301a1a"
                            Text {
                                id: statusLabel
                                anchors.centerIn: parent
                                text: backend && backend.javaInstalled ? "已检测" : "未检测到"
                                color: backend && backend.javaInstalled ? "#4bc870" : "#e05050"
                                font.pixelSize: StyleTokens.fontSizeSm
                            }
                        }
                        Item { Layout.fillWidth: true }
                    }

                    // Version
                    Text {
                        text: backend ? (backend.javaVersion ? "Java " + backend.javaMajor + " (" + backend.javaVersion + ")" : qsTr("未知版本")) : ""
                        font.pixelSize: StyleTokens.fontSizeLg; font.bold: true; color: StyleTokens.textPrimary
                        Layout.fillWidth: true
                    }

                    // Path
                    Text {
                        text: backend && backend.javaPath ? backend.javaPath : qsTr("未找到 Java 可执行文件 — 请检测或手动指定")
                        font.pixelSize: StyleTokens.fontSizeSm; font.family: "Consolas, monospace"; color: StyleTokens.textTertiary
                        elide: Text.ElideMiddle; Layout.fillWidth: true
                    }

                    // Java list ComboBox + refresh
                    RowLayout {
                        Layout.fillWidth: true; spacing: 8

                        ComboBox {
                            id: javaListCombo
                            Layout.fillWidth: true
                            textRole: "label"; valueRole: "path"
                            implicitHeight: 34

                            model: {
                                var items = []
                                var list = root._javaList || []
                                for (var i = 0; i < list.length; i++) {
                                    items.push({
                                        label: "Java " + list[i].major + " (" + list[i].version + ")",
                                        path: list[i].path, major: list[i].major
                                    })
                                }
                                items.push({ label: qsTr("导入电脑中已有的 Java..."), path: "__browse__", major: 0 })
                                return items
                            }
                            currentIndex: root._selectedJavaIndex

                            background: Rectangle {
                                radius: StyleTokens.radiusMd; color: StyleTokens.surfaceLight
                                border.color: javaListCombo.hovered ? "#3B82F6" : "#2a2f3a"
                            }
                            contentItem: Text {
                                text: {
                                    var idx = javaListCombo.currentIndex
                                    if (idx < 0) return qsTr("点击选择 Java...")
                                    var m = javaListCombo.model
                                    if (m && idx < m.length) return m[idx].label
                                    return qsTr("点击选择 Java...")
                                }
                                font.pixelSize: StyleTokens.fontSizeSm
                                color: javaListCombo.currentIndex < 0 ? "#5A6173" : "#B4BAC6"
                                verticalAlignment: Text.AlignVCenter; leftPadding: 10
                            }
                            indicator: Text {
                                text: "▼"; font.pixelSize: StyleTokens.fontSizeXs; color: "#7E8596"
                                anchors.right: parent.right; anchors.rightMargin: 10
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            delegate: ItemDelegate {
                                width: javaListCombo.width
                                contentItem: RowLayout {
                                    Text {
                                        text: modelData.label; font.pixelSize: StyleTokens.fontSizeSm
                                        color: highlighted ? "#e8ecf8" : "#B4BAC6"
                                        Layout.fillWidth: true
                                    }
                                    Text {
                                        text: modelData.major > 0 ? qsTr("推荐") : ""
                                        font.pixelSize: StyleTokens.fontSizeXs; color: "#10B981"
                                        visible: modelData.major > 0
                                    }
                                }
                                background: Rectangle {
                                    color: highlighted ? "#253555" : "#1A1D24"
                                    Behavior on color { ColorAnimation { duration: 100 } }
                                }
                                highlighted: javaListCombo.highlightedIndex === index
                            }
                            popup: Popup {
                                y: javaListCombo.height; width: javaListCombo.width
                                padding: 2; topMargin: 4
                                enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 120 } }
                                exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 80 } }
                                contentItem: ListView {
                                    clip: true; implicitHeight: contentHeight
                                    model: javaListCombo.popup.visible ? javaListCombo.delegateModel : null
                                    currentIndex: javaListCombo.highlightedIndex
                                }
                                background: Rectangle { radius: StyleTokens.radiusMd; color: "#1A1D24"; border.color: StyleTokens.bgHover }
                            }

                            onCurrentIndexChanged: {
                                if (currentIndex < 0) return
                                var m = model
                                if (!m || currentIndex >= m.length) return
                                var item = m[currentIndex]
                                if (item.path === "__browse__") {
                                    if (backend) backend.openJavaFileDialog()
                                    javaListCombo.currentIndex = root._selectedJavaIndex
                                    return
                                }
                                root._selectedJavaIndex = currentIndex
                                if (backend && currentIndex < (root._javaList || []).length)
                                    backend.selectJavaByIndex(currentIndex)
                            }
                        }

                        // Refresh
                        Rectangle {
                            id: refreshBtn
                            width: 34; height: 34; radius: StyleTokens.radiusMd
                            color: refreshHover.hovered ? "#1a2848" : "transparent"
                            border.color: StyleTokens.bgHover
                            scale: refreshMa.pressed ? 0.88 : 1.0
                            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                            Behavior on color { ColorAnimation { duration: 120 } }
                            HoverHandler { id: refreshHover }
                            Text {
                                id: refreshIcon
                                anchors.centerIn: parent
                                text: "↻"; font.pixelSize: StyleTokens.fontSizeLg
                                color: StyleTokens.accentHover
                                rotation: 0
                            }
                            MouseArea {
                                id: refreshMa
                                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (!backend) return
                                    if (toastManager) toastManager.show(qsTr("正在扫描 Java 环境..."))
                                    backend.scanJavaInstallations()
                                    root._javaList = backend.availableJavaList || []
                                    root._updateJavaIndex()
                                    var count = root._javaList.length
                                    if (toastManager) {
                                        var msg = count > 0
                                            ? qsTr("扫描完成，共检出 ") + count + qsTr(" 个 Java")
                                            : qsTr("未检测到 Java 环境，请手动导入或安装 Java")
                                        toastManager.show(msg)
                                    }
                                }
                            }
                        }
                    }

                    // Buttons
                    RowLayout {
                        Layout.fillWidth: true; spacing: 10
                        Rectangle {
                            width: detectLabel.implicitWidth + 24; height: 34; radius: StyleTokens.radiusMd
                            color: detectHover.hovered ? "#6d7de8" : "#5068d8"
                            scale: detectMa.pressed ? 0.92 : 1.0
                            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                            Behavior on color { ColorAnimation { duration: 150 } }
                            Text {
                                id: detectLabel
                                anchors.centerIn: parent
                                text: qsTr("重新检测")
                                font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.DemiBold; color: StyleTokens.textInverse
                            }
                            HoverHandler { id: detectHover }
                            MouseArea {
                                id: detectMa
                                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (backend) {
                                        backend.detectJava()
                                        if (toastManager) toastManager.show(qsTr("正在检测 Java 环境..."))
                                    }
                                }
                            }
                        }
                        Rectangle {
                            width: browseLabel.implicitWidth + 24; height: 34; radius: StyleTokens.radiusMd
                            color: browseHover.hovered ? "#1a1f2e" : "transparent"
                            scale: browseMa.pressed ? 0.92 : 1.0
                            border.color: browseHover.hovered ? "#6d7de8" : "#4a5ec8"; border.width: 1.5
                            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                            Text {
                                id: browseLabel
                                anchors.centerIn: parent
                                text: qsTr("手动选择")
                                font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.Medium
                                color: browseHover.hovered ? "#ffffff" : "#b0b8e0"
                            }
                            HoverHandler { id: browseHover }
                            MouseArea {
                                id: browseMa
                                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (backend) {
                                        backend.browseJava()
                                        if (toastManager) toastManager.show(qsTr("已选择 Java 路径"))
                                    }
                                }
                            }
                        }
                        Item { Layout.fillWidth: true }
                    }
                }
            }

            // ══ 2. JVM 参数 ══
            Text {
                text: qsTr("JVM 参数")
                font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.Medium; color: StyleTokens.textSecondary
                Layout.topMargin: 8
            }
            Rectangle {
                Layout.fillWidth: true; implicitHeight: 120
                radius: StyleTokens.radiusLg; color: "#11141c"
                border.color: root._jvmFocused ? "#5068d8" : "#1a1f2a"
                Behavior on border.color { ColorAnimation { duration: 200 } }

                TextEdit {
                    id: jvmArgsInput
                    anchors.fill: parent; anchors.margins: 10
                    color: "#e8ecf8"; font.pixelSize: StyleTokens.fontSizeSm
                    font.family: "Consolas, monospace"
                    text: backend ? backend.jvmArgs || "" : ""
                    wrapMode: TextEdit.Wrap; selectByMouse: true
                    activeFocusOnPress: true
                    onFocusChanged: root._jvmFocused = focus
                    onTextChanged: root._validateJvmArgs(text)
                    onEditingFinished: {
                        if (backend) backend.setJvmArgs(text)
                    }

                    // Placeholder when empty
                    Label {
                        anchors.fill: parent; anchors.margins: 2
                        text: defaultJvmArgs
                        color: "#5A6173"; font.pixelSize: StyleTokens.fontSizeSm
                        font.family: "Consolas, monospace"
                        visible: jvmArgsInput.text === "" && !jvmArgsInput.activeFocus
                        wrapMode: Text.WordWrap
                    }
                }
            }

            // JVM 参数警告栏
            InlineToast {
                Layout.fillWidth: true
                type: "warning"
                message: root._jvmWarning
            }

            // GC 预设
            Text { text: qsTr("GC 预设"); font.pixelSize: StyleTokens.fontSizeSm; color: "#7E8596" }
            Flow {
                Layout.fillWidth: true; spacing: 8
                Repeater {
                    model: [
                        { label: qsTr("G1GC 平衡"), args: "-XX:+UseG1GC -XX:G1NewSizePercent=20 -XX:MaxGCPauseMillis=50" },
                        { label: qsTr("ZGC 低延迟"), args: "-XX:+UseZGC -XX:+UnlockExperimentalVMOptions" },
                        { label: "Shenandoah", args: "-XX:+UseShenandoahGC" },
                        { label: qsTr("Parallel 吞吐"), args: "-XX:+UseParallelGC" },
                        { label: "Serial", args: "-XX:+UseSerialGC" },
                        { label: qsTr("清空"), args: "" }
                    ]
                    Rectangle {
                        implicitWidth: gcLabel.implicitWidth + 20; height: 30; radius: StyleTokens.radiusMd
                        color: gcHover.hovered || gcFlash.running ? "#1a2848" : "#1A1D24"
                        border.color: StyleTokens.bgHover
                        scale: gcMa.pressed ? 0.92 : 1.0
                        Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                        Behavior on color { ColorAnimation { duration: 120 } }
                        Text {
                            id: gcLabel
                            anchors.centerIn: parent
                            text: modelData.label; font.pixelSize: StyleTokens.fontSizeSm; color: "#B4BAC6"
                        }
                        HoverHandler { id: gcHover }
                        MouseArea {
                            id: gcMa
                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                gcFlash.start()
                                jvmArgsInput.text = modelData.args
                                if (backend) backend.setJvmArgs(modelData.args)
                                if (toastManager) toastManager.show(qsTr("已应用 ") + modelData.label + qsTr(" 预设"))
                                root._validateJvmArgs(modelData.args)
                            }
                        }
                        SequentialAnimation on color {
                            id: gcFlash; running: false
                            ColorAnimation { to: "#305080"; duration: 80 }
                            ColorAnimation { to: "#1a2848"; duration: 300 }
                            ColorAnimation { to: "#1A1D24"; duration: 200 }
                        }
                    }
                }
            }

            // 恢复默认
            Rectangle {
                width: 120; height: 28; radius: StyleTokens.radiusMd
                color: resHover.hovered ? "#1a1f2e" : "transparent"; border.color: StyleTokens.bgHover
                scale: resMa.pressed ? 0.92 : 1.0
                Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                Behavior on color { ColorAnimation { duration: 120 } }
                HoverHandler { id: resHover }
                Text {
                    anchors.centerIn: parent
                    text: qsTr("恢复默认 JVM"); font.pixelSize: StyleTokens.fontSizeSm; color: "#7E8596"
                }
                MouseArea {
                    id: resMa
                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        jvmArgsInput.text = defaultJvmArgs
                        if (backend) backend.setJvmArgs(defaultJvmArgs)
                        if (toastManager) toastManager.show(qsTr("已恢复默认 JVM 参数"))
                    }
                }
            }

            // ══ 3. 游戏附加参数 ══
            Text {
                text: qsTr("游戏附加参数")
                font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.Medium; color: StyleTokens.textSecondary
                Layout.topMargin: 8
            }
            Text {
                text: qsTr("示例：--width 1920 --height 1080 --fullscreen")
                font.pixelSize: StyleTokens.fontSizeXs; color: "#7E8596"
            }
            Rectangle {
                Layout.fillWidth: true; implicitHeight: 44
                radius: StyleTokens.radiusLg; color: "#11141c"; border.color: StyleTokens.bgInput
                TextInput {
                    id: gameArgsInput
                    anchors.fill: parent; anchors.margins: 10
                    color: "#e8ecf8"; font.pixelSize: StyleTokens.fontSizeSm
                    font.family: "Consolas, monospace"
                    text: backend ? backend.gameArgs || "" : ""
                    activeFocusOnPress: true
                    onEditingFinished: { if (backend) backend.setGameArgs(text) }
                }
            }

            // ══ 4. 启动选项 ══
            Text {
                text: qsTr("启动选项")
                font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.Medium; color: StyleTokens.textSecondary
                Layout.topMargin: 8
            }
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: optionsColumn.implicitHeight + 16
                radius: StyleTokens.radiusLg; color: "#11141c"; border.color: StyleTokens.bgInput

                ColumnLayout {
                    id: optionsColumn
                    anchors.left: parent.left; anchors.right: parent.right
                    anchors.top: parent.top; anchors.margins: 8
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: qsTr("要求 Java 使用高性能显卡"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textSecondary }
                        Item { Layout.fillWidth: true }
                        Switch {
                            checked: backend ? backend.highPerfGpu : false
                            onToggled: { if (backend) backend.setHighPerfGpu(checked) }
                        }
                    }

                }
            }
        }
    }
}
