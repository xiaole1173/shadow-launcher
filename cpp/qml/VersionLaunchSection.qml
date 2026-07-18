// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
// Version Launch Section — per-version Java / JVM / game args configuration
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

// ═══ 版本级启动配置 ═══
Item {
    id: root
    property var backend: null
    property var toastManager: null
    property string currentSelectedVersion: ""

    // ── Mode: 0 = follow global, 1 = custom (per-version) ──
    property int _mode: 0
    // ── Java mode: 0 = auto, 1 = version folder, 2 = specified ──
    property int _javaMode: 0
    property int _selectedJavaIndex: -1
    property string _jvmWarning: ""

    // ── Cached values ──
    property string _globalJvmArgs: ""
    property string _globalGameArgs: ""
    property bool _globalHighPerfGpu: false
    property string _perVerJvmArgs: ""
    property string _perVerGameArgs: ""
    property bool _perVerHighPerfGpu: false
    property var _javaList: []

    function refreshAll() {
        if (!backend) return
        _globalJvmArgs = backend.jvmArgs || ""
        _globalGameArgs = backend.gameArgs || ""
        _globalHighPerfGpu = backend.highPerfGpu || false

        _mode = backend.versionJvmArgsMode(currentSelectedVersion)
        _perVerJvmArgs = backend.versionJvmArgs(currentSelectedVersion) || ""
        _perVerGameArgs = backend.versionGameArgs(currentSelectedVersion) || ""
        _perVerHighPerfGpu = backend.versionHighPerfGpu(currentSelectedVersion) || false

        _javaMode = backend.versionJavaMode(currentSelectedVersion)
        _javaList = backend.availableJavaList || []
        _updateJavaIndex()

        // Init from global if custom but empty
        if (_mode === 1 && _perVerJvmArgs === "") {
            _perVerJvmArgs = _globalJvmArgs
            backend.setVersionJvmArgs(currentSelectedVersion, _perVerJvmArgs)
        }
        if (_mode === 1 && _perVerGameArgs === "") {
            _perVerGameArgs = _globalGameArgs
            backend.setVersionGameArgs(currentSelectedVersion, _perVerGameArgs)
        }
    }

    function _updateJavaIndex() {
        if (!backend || _javaMode !== 2) { _selectedJavaIndex = -1; return }
        var path = backend.javaPath
        var list = _javaList || []
        for (var i = 0; i < list.length; i++) {
            if (list[i].path === path) { _selectedJavaIndex = i; return }
        }
        _selectedJavaIndex = -1
    }

    property string _defaultJvmArgs: "-XX:+UseG1GC -XX:+UnlockExperimentalVMOptions -XX:G1NewSizePercent=20 -XX:MaxGCPauseMillis=50"

    function _effectiveJvmArgs() {
        var val = _mode === 1 ? _perVerJvmArgs : _globalJvmArgs
        return val || _defaultJvmArgs
    }
    function _effectiveGameArgs() { return _mode === 1 ? _perVerGameArgs : _globalGameArgs }
    function _effectiveHighPerfGpu() { return _mode === 1 ? _perVerHighPerfGpu : _globalHighPerfGpu }

    function _validateJvmArgs(args) {
        var trimmed = args ? args.trim() : ""

        if (!trimmed) {
            root._jvmWarning = qsTr("JVM 参数为空，启动时将加载默认参数")
            return
        }

        var lines = trimmed.split(/[\r\n]+/)
        for (var l = 0; l < lines.length; l++) {
            var line = lines[l].trim()
            if (!line) continue
            if (line[0] !== '-' && line[0] !== '@') {
                root._jvmWarning = qsTr("检测到无效参数: \"") + line + qsTr("\"，JVM 参数应以 -D/-X/-XX: 开头")
                return
            }
        }

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

    // ── Listen for global changes ──
    Connections {
        target: backend
        enabled: root._mode === 0
                        }

    Flickable {
        anchors.fill: parent
        contentHeight: contentColumn.implicitHeight + 48
        clip: true

        ColumnLayout {
            id: contentColumn
            y: 24
            width: parent.width - 48
            x: 24
            spacing: 14

            // ═══════════════════════════════════════
            // 1. Mode toggle
            // ═══════════════════════════════════════
            Text {
                text: qsTr("配置模式")
                font.pixelSize: StyleTokens.fontSizeSm; font.weight: Font.DemiBold; color: StyleTokens.textSecondary
            }
            RowLayout {
                spacing: 10
                Rectangle {
                    id: followGlobalBtn
                    width: 130; height: 36; radius: StyleTokens.radiusLg
                    color: root._mode === 0 ? "#2a3a6a" : (fgHover.hovered ? "#151c30" : "transparent")
                    border.color: root._mode === 0 ? "#3a5ab8" : "#1a1f2e"
                    scale: followGlobalMa.pressed ? 0.94 : 1.0
                    Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                    Behavior on color { ColorAnimation { duration: 150 } }
                    HoverHandler { id: fgHover }
                    Text {
                        anchors.centerIn: parent
                        text: qsTr("跟随全局设置")
                        font.pixelSize: StyleTokens.fontSizeSm
                        color: root._mode === 0 ? "#e8ecf8" : "#808aa0"
                    }
                    MouseArea {
                        id: followGlobalMa
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (root._mode === 0) return
                            root._mode = 0
                            if (backend) backend.setVersionJvmArgsMode(currentSelectedVersion, 0)
                        }
                    }
                }
                Rectangle {
                    id: customBtn
                    width: 130; height: 36; radius: StyleTokens.radiusLg
                    color: root._mode === 1 ? "#2a3a6a" : (customHover.hovered ? "#151c30" : "transparent")
                    border.color: root._mode === 1 ? "#3a5ab8" : "#1a1f2e"
                    scale: customMa.pressed ? 0.94 : 1.0
                    Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                    Behavior on color { ColorAnimation { duration: 150 } }
                    HoverHandler { id: customHover }
                    Text {
                        anchors.centerIn: parent
                        text: qsTr("独立配置")
                        font.pixelSize: StyleTokens.fontSizeSm
                        color: root._mode === 1 ? "#e8ecf8" : "#808aa0"
                    }
                    MouseArea {
                        id: customMa
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (root._mode === 1) return
                            root._mode = 1
                            if (backend) {
                                backend.setVersionJvmArgsMode(currentSelectedVersion, 1)
                                if (_perVerJvmArgs === "") {
                                    _perVerJvmArgs = _globalJvmArgs
                                    backend.setVersionJvmArgs(currentSelectedVersion, _perVerJvmArgs)
                                }
                                if (_perVerGameArgs === "") {
                                    _perVerGameArgs = _globalGameArgs
                                    backend.setVersionGameArgs(currentSelectedVersion, _perVerGameArgs)
                                }
                            }
                        }
                    }
                }
                Text {
                    text: root._mode === 0
                        ? qsTr("当前使用全局 Java 设置")
                        : qsTr("此版本使用独立配置，不受全局设置影响")
                    font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.textTertiary
                }
                Item { Layout.fillWidth: true }
            }

            // ═══════════════════════════════════════
            // 2. Java environment — dropdown selector
            // ═══════════════════════════════════════
            Text {
                text: qsTr("Java 环境")
                font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.Medium; color: StyleTokens.textSecondary
                Layout.topMargin: 12
            }

            // Recommended Java hint
            Text {
                text: {
                    if (!currentSelectedVersion) return ""
                    var vid = currentSelectedVersion
                    var parts = vid.split(".")
                    if (parts[0] === "1" && parts.length > 1) {
                        var minor = parseInt(parts[1]) || 0
                        if (minor >= 18) return qsTr("MC %1 → 推荐 Java 17+").arg(vid)
                        if (minor === 17) return qsTr("MC %1 → 推荐 Java 16+").arg(vid)
                        if (minor >= 12) return qsTr("MC %1 → 推荐 Java 8").arg(vid)
                        return qsTr("MC %1 → 推荐 Java 8").arg(vid)
                    }
                    return ""
                }
                font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary
            }

            // Java mode dropdown (row 1)
            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: qsTr("Java")
                    font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textSecondary
                    Layout.preferredWidth: 40
                }

                ComboBox {
                    id: javaModeCombo
                    Layout.fillWidth: true
                    model: [qsTr("自动选择"), qsTr("使用版本文件夹中的 Java"), qsTr("使用指定的 Java")]
                    currentIndex: root._javaMode
                    enabled: root._mode === 1
                    opacity: root._mode === 0 ? 0.6 : 1.0
                    implicitHeight: 34

                    background: Rectangle {
                        radius: StyleTokens.radiusMd
                        color: StyleTokens.bgPrimary
                        border.color: javaModeCombo.hovered ? "#3a5ab8" : "#1a1f2e"
                    }
                    contentItem: Text {
                        text: javaModeCombo.displayText
                        font.pixelSize: StyleTokens.fontSizeSm; color: "#b0b8c8"
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 10
                    }
                    indicator: Text {
                        text: "▼"
                        font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.textTertiary
                        anchors.right: parent.right; anchors.rightMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    delegate: ItemDelegate {
                        width: javaModeCombo.width
                        contentItem: Text {
                            text: modelData
                            font.pixelSize: StyleTokens.fontSizeSm
                            color: highlighted ? "#e8ecf8" : "#b0b8c8"
                        }
                        background: Rectangle {
                            color: highlighted ? "#253555" : "#0d1018"
                            Behavior on color { ColorAnimation { duration: 100 } }
                        }
                        highlighted: javaModeCombo.highlightedIndex === index
                    }

                    popup: Popup {
                        y: javaModeCombo.height
                        width: javaModeCombo.width
                        padding: 2
                        topMargin: 4
                        enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 120 } }
                        exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 80 } }
                        contentItem: ListView {
                            clip: true
                            implicitHeight: contentHeight
                            model: javaModeCombo.popup.visible ? javaModeCombo.delegateModel : null
                            currentIndex: javaModeCombo.highlightedIndex
                        }
                        background: Rectangle { radius: StyleTokens.radiusMd; color: StyleTokens.bgPrimary; border.color: StyleTokens.bgCard }
                    }

                    onCurrentIndexChanged: {
                        if (currentIndex === root._javaMode) return
                        root._javaMode = currentIndex
                        if (backend) backend.setVersionJavaMode(currentSelectedVersion, currentIndex)
                        root._updateJavaIndex()
                    }
                }
            }

            // Specified Java dropdown + refresh (row 2 — visible only when javaMode === 2)
            Item {
                Layout.fillWidth: true
                clip: true
                implicitHeight: root._javaMode === 2 ? javaRow2.implicitHeight : 0
                Behavior on implicitHeight { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }

                RowLayout {
                    id: javaRow2
                    width: parent.width
                    opacity: root._javaMode === 2 ? 1 : 0
                    Behavior on opacity { NumberAnimation { duration: 200 } }

                Text {
                    text: ""
                    Layout.preferredWidth: 40
                }

                ComboBox {
                    id: javaListCombo
                    Layout.fillWidth: true
                    textRole: "label"
                    valueRole: "path"
                    implicitHeight: 34
                    model: {
                        var items = []
                        var list = root._javaList || []
                        for (var i = 0; i < list.length; i++) {
                            items.push({
                                label: "Java " + list[i].major + "  (" + list[i].version + ")",
                                path: list[i].path,
                                major: list[i].major
                            })
                        }
                        items.push({ label: qsTr("导入电脑中已有的 Java"), path: "__browse__", major: 0 })
                        return items
                    }
                    currentIndex: root._selectedJavaIndex
                    enabled: root._mode === 1

                    background: Rectangle {
                        radius: StyleTokens.radiusMd
                        color: "#0d1018"
                        border.color: javaListCombo.hovered ? "#3a5ab8" : "#1a1f2e"
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
                        color: javaListCombo.currentIndex < 0 ? "#808aa0" : "#b0b8c8"
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 10
                    }
                    indicator: Text {
                        text: "▼"
                        font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.textTertiary
                        anchors.right: parent.right; anchors.rightMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    delegate: ItemDelegate {
                        width: javaListCombo.width
                        contentItem: RowLayout {
                            Text {
                                text: modelData.label
                                font.pixelSize: StyleTokens.fontSizeSm
                                color: highlighted ? "#e8ecf8" : "#b0b8c8"
                                Layout.fillWidth: true
                            }
                            Text {
                                text: modelData.major > 0 ? qsTr("推荐") : ""
                                font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.success
                                visible: modelData.major > 0
                            }
                        }
                        background: Rectangle {
                            color: highlighted ? "#253555" : "#0d1018"
                            Behavior on color { ColorAnimation { duration: 100 } }
                        }
                        highlighted: javaListCombo.highlightedIndex === index
                    }

                    popup: Popup {
                        y: javaListCombo.height
                        width: javaListCombo.width
                        padding: 2
                        topMargin: 4
                        enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 120 } }
                        exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 80 } }
                        contentItem: ListView {
                            clip: true
                            implicitHeight: contentHeight
                            model: javaListCombo.popup.visible ? javaListCombo.delegateModel : null
                            currentIndex: javaListCombo.highlightedIndex
                        }
                        background: Rectangle { radius: StyleTokens.radiusMd; color: "#0d1018"; border.color: StyleTokens.bgCard }
                    }

                    onCurrentIndexChanged: {
                        if (currentIndex < 0) return
                        var m = model
                        if (!m || currentIndex >= m.length) return
                        var item = m[currentIndex]
                        if (item.path === "__browse__") {
                            if (backend) backend.pickJava()
                            javaListCombo.currentIndex = root._selectedJavaIndex // restore
                            return
                        }
                        root._selectedJavaIndex = currentIndex
                        if (backend && currentIndex < (root._javaList || []).length) {
                            backend.selectJavaByIndex(currentIndex)
                        }
                    }
                }

                // Refresh icon button
                Rectangle {
                    id: refreshBtn
                    width: 34; height: 34; radius: StyleTokens.radiusMd
                    color: refreshJavaHover.hovered ? "#1a2848" : "transparent"
                    border.color: StyleTokens.bgCard
                    scale: refreshMa.pressed ? 0.88 : 1.0
                    enabled: root._mode === 1
                    Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                    Behavior on color { ColorAnimation { duration: 120 } }
                    Text {
                        id: refreshIcon
                        anchors.centerIn: parent
                        text: "↻"
                        font.pixelSize: StyleTokens.fontSizeLg; color: StyleTokens.accentLight
                        rotation: 0
                    }
                    HoverHandler { id: refreshJavaHover }
                    MouseArea {
                        id: refreshMa
                        anchors.fill: parent
                        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: {
                            if (!enabled || !backend) return
                            toastManager.show(qsTr("正在扫描 Java 环境..."))
                            backend.scanJavaInstallations()
                            _javaList = backend.availableJavaList || []
                            _updateJavaIndex()
                            var count = _javaList.length
                            toastManager.show(count > 0
                                ? qsTr("扫描完成，共检出 ") + count + qsTr(" 个 Java")
                                : qsTr("未检测到 Java 环境，请手动导入或安装 Java"))
                        }
                    }
                }
            }

            }  // end outer Item (javaRow2 wrapper)

            // Java version display
            Text {
                visible: {
                    var v = backend ? backend.javaVersion || "" : ""
                    return v.length > 0 && v.indexOf("Java") >= 0
                }
                text: backend ? (backend.javaVersion || "") : ""
                font.pixelSize: StyleTokens.fontSizeSm
                color: "#b0b8c8"
                Layout.leftMargin: 48
            }

            // ═══════════════════════════════════════
            // 3. JVM parameters
            // ═══════════════════════════════════════
            Text {
                text: qsTr("JVM 参数")
                font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.Medium; color: StyleTokens.textSecondary
                Layout.topMargin: 12
            }
            Rectangle {
                Layout.fillWidth: true
                height: 80
                radius: StyleTokens.radiusMd; color: "#0d1018"; border.color: StyleTokens.bgCard
                opacity: root._mode === 0 ? 0.6 : 1.0

                TextEdit {
                    id: jvmArgsInput
                    anchors.fill: parent; anchors.margins: 8
                    color: "#b0b8c8"; font.pixelSize: StyleTokens.fontSizeSm
                    font.family: "Consolas, monospace"
                    text: root._effectiveJvmArgs()
                    readOnly: root._mode === 0
                    activeFocusOnPress: root._mode === 1
                    wrapMode: TextEdit.Wrap
                    selectByMouse: true
                    onTextChanged: root._validateJvmArgs(text)
                    onEditingFinished: {
                        if (root._mode !== 1) return
                        var newVal = text
                        root._perVerJvmArgs = newVal
                        if (backend) backend.setVersionJvmArgs(currentSelectedVersion, newVal)
                    }
                }
            }

            // JVM GC 冲突警告
            Item {
                Layout.fillWidth: true
                clip: true
                implicitHeight: root._jvmWarning ? jvmWarnContent.implicitHeight + 20 : 0
                Behavior on implicitHeight {
                    NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
                }

                Rectangle {
                    id: jvmWarnContent
                    width: parent.width
                    opacity: root._jvmWarning ? 1 : 0
                    Behavior on opacity { NumberAnimation { duration: 250 } }
                    radius: StyleTokens.radiusLg; color: StyleTokens.errorBg
                    border.color: "#d04040"; border.width: 1
                    implicitHeight: warnLabel.implicitHeight + 24

                    RowLayout {
                        anchors.fill: parent; anchors.margins: 12; spacing: 8
                        Text {
                            text: "[警告]"; font.pixelSize: StyleTokens.fontSizeLg; color: StyleTokens.errorLight
                        }
                        Text {
                            id: warnLabel
                            text: root._jvmWarning
                            font.pixelSize: StyleTokens.fontSizeSm; color: "#e8a0a0"
                            Layout.fillWidth: true; wrapMode: Text.WordWrap
                        }
                    }

                    transform: Translate {
                        y: root._jvmWarning ? 0 : -jvmWarnContent.implicitHeight
                        Behavior on y { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
                    }
                }
            }

            // JVM 参数警告栏
            InlineToast {
                Layout.fillWidth: true
                type: "warning"
                message: root._jvmWarning
            }

            // GC presets
            Text { text: qsTr("GC 预设"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary }
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
                        id: gcChip
                        implicitWidth: gcLabel.implicitWidth + 20; height: 30; radius: StyleTokens.radiusMd
                        color: gcHover.hovered || gcFlash.running ? "#1a2848" : "#0d1018"
                        border.color: StyleTokens.bgCard
                        scale: gcMa.pressed ? 0.92 : 1.0
                        opacity: root._mode === 0 ? 0.5 : 1.0
                        Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                        Behavior on color { ColorAnimation { duration: 120 } }
                        Text {
                            id: gcLabel
                            anchors.centerIn: parent
                            text: modelData.label
                            font.pixelSize: StyleTokens.fontSizeSm; color: "#b0b8c8"
                        }
                        HoverHandler { id: gcHover }
                        MouseArea {
                            id: gcMa
                            anchors.fill: parent
                            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: {
                                if (!enabled) return
                                gcFlash.start()
                                if (backend) toastManager.show(qsTr("已应用 ") + modelData.label + qsTr(" 预设"))
                                jvmArgsInput.text = modelData.args
                                root._perVerJvmArgs = modelData.args
                                if (backend) backend.setVersionJvmArgs(currentSelectedVersion, modelData.args)
                                root._validateJvmArgs(modelData.args)
                            }
                        }
                        SequentialAnimation on color {
                            id: gcFlash
                            running: false
                            ColorAnimation { to: "#305080"; duration: 80 }
                            ColorAnimation { to: "#1a2848"; duration: 300 }
                            ColorAnimation { to: "#0d1018"; duration: 200 }
                        }
                    }
                }
            }


            RowLayout {
                Layout.alignment: Qt.AlignRight
                Rectangle {
                    id: resetBtn
                    width: 110; height: 28; radius: StyleTokens.radiusMd
                    color: resetJvmHover.hovered ? "#151c30" : "transparent"
                    border.color: StyleTokens.bgCard
                    scale: resetMa.pressed ? 0.92 : 1.0
                    opacity: root._mode === 0 ? 0.5 : 1.0
                    Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                    Behavior on color { ColorAnimation { duration: 120 } }
                    Text { anchors.centerIn: parent; text: qsTr("恢复默认 JVM"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary }
                    HoverHandler { id: resetJvmHover }
                    MouseArea {
                        id: resetMa
                        anchors.fill: parent
                        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: {
                            if (!enabled) return
                            var defVal = "-XX:+UseG1GC -XX:+UnlockExperimentalVMOptions -XX:G1NewSizePercent=20 -XX:MaxGCPauseMillis=50"
                            jvmArgsInput.text = defVal
                            root._perVerJvmArgs = defVal
                            if (backend) backend.setVersionJvmArgs(currentSelectedVersion, defVal)
                        }
                    }
                }
            }

            // ═══════════════════════════════════════
            // 4. Game additional args
            // ═══════════════════════════════════════
            Text {
                text: qsTr("游戏附加参数")
                font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.Medium; color: StyleTokens.textSecondary
                Layout.topMargin: 12
            }
            Text {
                text: qsTr("示例：--width 1920 --height 1080 --fullscreen")
                font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.textTertiary
            }
            Rectangle {
                Layout.fillWidth: true; height: 44
                radius: StyleTokens.radiusMd; color: "#0d1018"; border.color: StyleTokens.bgCard
                opacity: root._mode === 0 ? 0.6 : 1.0
                TextInput {
                    id: gameArgsInput
                    anchors.fill: parent; anchors.margins: 8
                    color: "#b0b8c8"; font.pixelSize: StyleTokens.fontSizeSm
                    font.family: "Consolas, monospace"
                    text: root._effectiveGameArgs()
                    readOnly: root._mode === 0
                    activeFocusOnPress: root._mode === 1
                    onEditingFinished: {
                        if (root._mode !== 1) return
                        var newVal = text
                        root._perVerGameArgs = newVal
                        if (backend) backend.setVersionGameArgs(currentSelectedVersion, newVal)
                    }
                }
            }

            // ═══════════════════════════════════════
            // 5. Launch options
            // ═══════════════════════════════════════
            Text {
                text: qsTr("启动选项")
                font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.Medium; color: StyleTokens.textSecondary
                Layout.topMargin: 12
            }
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: optionsContent.implicitHeight + 16
                radius: StyleTokens.radiusLg
                color: StyleTokens.bgSecondary
                border.color: StyleTokens.bgCard

                ColumnLayout {
                    id: optionsContent
                    anchors.left: parent.left; anchors.right: parent.right
                    anchors.top: parent.top; anchors.margins: 8
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: qsTr("要求 Java 使用高性能显卡"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textSecondary }
                        Item { Layout.fillWidth: true }
                        Switch {
                            checked: root._effectiveHighPerfGpu()
                            enabled: root._mode === 1
                            opacity: root._mode === 0 ? 0.5 : 1.0
                            onToggled: {
                                if (root._mode !== 1) return
                                root._perVerHighPerfGpu = checked
                                if (backend) backend.setVersionHighPerfGpu(currentSelectedVersion, checked)
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: qsTr("版本隔离（模组/存档独立存储）"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textSecondary }
                        Item { Layout.fillWidth: true }
                        Switch { checked: true; enabled: false; opacity: 0.5 }
                        Text { text: qsTr("始终开启"); font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.textTertiary }
                    }
                }
            }

            Item { Layout.fillHeight: true; implicitHeight: 24 }
        }
    }
}
