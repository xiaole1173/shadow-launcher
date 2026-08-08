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
    property string _selectedJavaPath: ""
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
        if (!backend || _javaMode !== 2) { _selectedJavaIndex = -1; _selectedJavaPath = ""; return }
        var path = backend.javaPath
        var list = _javaList || []
        for (var i = 0; i < list.length; i++) {
            if (list[i].path === path) { _selectedJavaIndex = i; _selectedJavaPath = path; return }
        }
        _selectedJavaIndex = -1; _selectedJavaPath = ""
    }

    property string _defaultJvmArgs: "-XX:+UseG1GC -XX:+UnlockExperimentalVMOptions -XX:G1NewSizePercent=20 -XX:MaxGCPauseMillis=50"

    function _effectiveJvmArgs() {
        var val = _mode === 1 ? _perVerJvmArgs : _globalJvmArgs
        return val || _defaultJvmArgs
    }
    function _effectiveGameArgs() { return _mode === 1 ? _perVerGameArgs : _globalGameArgs }
    function _effectiveHighPerfGpu() { return _mode === 1 ? _perVerHighPerfGpu : _globalHighPerfGpu }

    // ── 启动细节（2026-08-08 低垂果实批）：版本级优先 → 全局 ──
    function _resolvedGcMode() {
        if (!backend) return 0
        return backend.resolvedGcMode ? backend.resolvedGcMode(currentSelectedVersion) : (backend.gcMode || 0)
    }
    function _gcModeLabel() {
        var m = backend ? (backend.gcMode || 0) : 0
        if (m === 1) return qsTr("分代 ZGC 优先")
        if (m === 2) return qsTr("仅 G1GC")
        if (m === 3) return qsTr("不指定")
        return qsTr("自动")
    }
    function _resolvedAutoJoinServer() {
        if (!backend) return ""
        return backend.resolvedAutoJoinServer ? backend.resolvedAutoJoinServer(currentSelectedVersion) : (backend.autoJoinServer || "")
    }
    function _resolvedFullscreen() {
        if (!backend) return false
        return backend.resolvedFullscreen ? backend.resolvedFullscreen(currentSelectedVersion) : !!backend.fullscreenEnabled
    }
    function _resolvedWindowTitle() {
        if (!backend) return ""
        return backend.resolvedWindowTitle ? backend.resolvedWindowTitle(currentSelectedVersion) : (backend.windowTitleOverride || "")
    }
    function _resolvedPreLaunchCommand() {
        if (!backend) return ""
        return backend.resolvedPreLaunchCommand ? backend.resolvedPreLaunchCommand(currentSelectedVersion) : (backend.preLaunchCommand || "")
    }
    function _resolvedPostExitCommand() {
        if (!backend) return ""
        return backend.resolvedPostExitCommand ? backend.resolvedPostExitCommand(currentSelectedVersion) : (backend.postExitCommand || "")
    }

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
        contentWidth: contentColumn.width   // 显式限定内容宽，任何子元素超出都被裁剪而非“向右溢出”
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
                Layout.fillWidth: true
                spacing: 10
                Rectangle {
                    id: followGlobalBtn
                    width: 130; height: 36; radius: StyleTokens.radiusLg
                    color: root._mode === 0 ? "#2a3a6a" : (fgHover.hovered ? "#151c30" : "transparent")
                    border.color: root._mode === 0 ? "#3a5ab8" : StyleTokens.bgCard
                    scale: followGlobalMa.pressed ? 0.94 : 1.0
                    Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                    Behavior on color { ColorAnimation { duration: 150 } }
                    HoverHandler { id: fgHover }
                    Text {
                        anchors.centerIn: parent
                        text: qsTr("跟随全局设置")
                        font.pixelSize: StyleTokens.fontSizeSm
                        color: root._mode === 0 ? StyleTokens.textPrimary : "#808aa0"
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
                    border.color: root._mode === 1 ? "#3a5ab8" : StyleTokens.bgCard
                    scale: customMa.pressed ? 0.94 : 1.0
                    Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                    Behavior on color { ColorAnimation { duration: 150 } }
                    HoverHandler { id: customHover }
                    Text {
                        anchors.centerIn: parent
                        text: qsTr("独立配置")
                        font.pixelSize: StyleTokens.fontSizeSm
                        color: root._mode === 1 ? StyleTokens.textPrimary : "#808aa0"
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
                    // 提示文字可伸缩且超长省略，保证整行不会超出列宽（独立配置文案较长，曾导致向右溢出）
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    maximumLineCount: 1
                    text: root._mode === 0
                        ? qsTr("当前使用全局 Java 设置")
                        : qsTr("此版本使用独立配置，不受全局设置影响")
                    font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.textTertiary
                }
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

                ShadowDropdown {
                    id: javaModeDropdown
                    Layout.fillWidth: true
                    model: [
                        {value: 0, label: qsTr("自动选择")},
                        {value: 1, label: qsTr("使用版本文件夹中的 Java")},
                        {value: 2, label: qsTr("使用指定的 Java")}
                    ]
                    valueKey: "value"
                    currentValue: root._javaMode
                    enabled: root._mode === 1
                    opacity: root._mode === 0 ? 0.6 : 1.0
                    onValueSelected: function(v) {
                        var newMode = Number(v)
                        if (newMode === root._javaMode) return
                        root._javaMode = newMode
                        if (backend) backend.setVersionJavaMode(currentSelectedVersion, newMode)
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

                ShadowDropdown {
                    id: javaListDropdown
                    Layout.fillWidth: true
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
                    valueKey: "path"
                    labelKey: "label"
                    currentValue: root._selectedJavaPath
                    enabled: root._mode === 1
                    placeholderText: qsTr("点击选择 Java...")
                    onValueSelected: function(path) {
                        if (path === "__browse__") {
                            if (backend) backend.pickJava()
                            return
                        }
                        var list = root._javaList || []
                        for (var i = 0; i < list.length; i++) {
                            if (list[i].path === path) {
                                root._selectedJavaIndex = i
                                root._selectedJavaPath = path
                                if (backend) backend.selectJavaByIndex(i)
                                break
                            }
                        }
                    }
                }

                // Refresh icon button
                RefreshButton {
                    onClicked: {
                        if (!backend) return
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

            }  // end outer Item (javaRow2 wrapper)

            // Java version display
            Text {
                visible: {
                    var v = backend ? backend.javaVersion || "" : ""
                    return v.length > 0 && v.indexOf("Java") >= 0
                }
                text: backend ? (backend.javaVersion || "") : ""
                font.pixelSize: StyleTokens.fontSizeSm
                color: StyleTokens.textTertiary
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
                    color: StyleTokens.textTertiary; font.pixelSize: StyleTokens.fontSizeSm
                    font.family: StyleTokens.fontFamilyMono
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
                        color: gcHover.hovered || gcFlash.running ? StyleTokens.accentSubtle : "#0d1018"
                        border.color: StyleTokens.bgCard
                        scale: gcMa.pressed ? 0.92 : 1.0
                        opacity: root._mode === 0 ? 0.5 : 1.0
                        Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                        Behavior on color { ColorAnimation { duration: 120 } }
                        Text {
                            id: gcLabel
                            anchors.centerIn: parent
                            text: modelData.label
                            font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary
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
                            ColorAnimation { to: StyleTokens.accentSubtle; duration: 300 }
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
                    color: StyleTokens.textTertiary; font.pixelSize: StyleTokens.fontSizeSm
                    font.family: StyleTokens.fontFamilyMono
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
            // 2026-08-08 重构 v2：全部版本级覆盖（全局默认在设置-通用页），
            // 说明小字以（）形式放右侧；无「全局设置」分组；删版本隔离行
            // ═══════════════════════════════════════
            Text {
                text: qsTr("启动选项")
                font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.Medium; color: StyleTokens.textSecondary
                Layout.topMargin: 12
            }
            Text {
                visible: root._mode === 0
                text: qsTr("当前跟随全局默认，切换上方「独立配置」后可单独设置本版本")
                font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.textTertiary
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: optContent.implicitHeight + 16
                radius: StyleTokens.radiusLg
                color: StyleTokens.bgSecondary
                border.color: StyleTokens.bgCard

                ColumnLayout {
                    id: optContent
                    anchors.left: parent.left; anchors.right: parent.right
                    anchors.top: parent.top; anchors.margins: 12
                    spacing: 12

                    // ── 强制使用高性能显卡（版本级）──
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("强制使用高性能显卡")
                            font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textSecondary
                            elide: Text.ElideRight
                        }
                        Text {
                            text: qsTr("（要求 Java 使用独立显卡运行）")
                            font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.textTertiary
                        }
                        ShadowSwitch {
                            checked: root._effectiveHighPerfGpu()
                            enabled: root._mode === 1
                            onToggled: {
                                if (root._mode !== 1) return
                                root._perVerHighPerfGpu = checked
                                if (backend) backend.setVersionHighPerfGpu(currentSelectedVersion, checked)
                            }
                        }
                    }

                    // ── GC 策略（版本级）──
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("GC 策略")
                            font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textSecondary
                        }
                        Text {
                            text: qsTr("（垃圾回收器选择）")
                            font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.textTertiary
                        }
                        ShadowDropdown {
                            id: gcModeDropdown
                            Layout.preferredWidth: 190
                            height: 28
                            model: [
                                { value: 0, label: qsTr("自动（推荐）") },
                                { value: 1, label: qsTr("分代 ZGC 优先") },
                                { value: 2, label: qsTr("仅 G1GC") },
                                { value: 3, label: qsTr("不指定") }
                            ]
                            valueKey: "value"
                            currentValue: root._resolvedGcMode()
                            enabled: root._mode === 1
                            opacity: root._mode === 0 ? 0.6 : 1.0
                            onValueSelected: function(v) {
                                if (root._mode !== 1) return
                                var newMode = Number(v)
                                if (backend) backend.setVersionGcMode(currentSelectedVersion, newMode)
                            }
                        }
                    }

                    // ── 自动进服（版本级）──
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("自动进服")
                            font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textSecondary
                        }
                        Text {
                            text: qsTr("（启动后自动连接服务器）")
                            font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.textTertiary
                        }
                    }
                    InputBox {
                        Layout.fillWidth: true
                        placeholderText: qsTr("服务器地址，如 play.example.com:25565（留空不自动进服）")
                        text: root._resolvedAutoJoinServer()
                        readOnly: root._mode === 0
                        enabled: root._mode === 1
                        onAccepted: {
                            if (root._mode !== 1) return
                            if (backend) backend.setVersionAutoJoinServer(currentSelectedVersion, text.trim())
                        }
                    }

                    // ── 全屏启动（版本级）──
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("全屏启动")
                            font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textSecondary
                        }
                        Text {
                            text: qsTr("（以全屏方式进入游戏）")
                            font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.textTertiary
                        }
                        ShadowSwitch {
                            checked: root._resolvedFullscreen()
                            enabled: root._mode === 1
                            onToggled: {
                                if (root._mode !== 1) return
                                if (backend) {
                                    backend.setVersionFullscreenMode(currentSelectedVersion, 1)
                                    backend.setVersionFullscreen(currentSelectedVersion, checked)
                                }
                            }
                        }
                    }

                    // ── 窗口标题（版本级）──
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("窗口标题")
                            font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textSecondary
                        }
                        Text {
                            text: qsTr("（启动后修改游戏窗口标题，留空不修改）")
                            font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.textTertiary
                        }
                    }
                    InputBox {
                        Layout.fillWidth: true
                        placeholderText: qsTr("如：我的世界 生存服")
                        text: root._resolvedWindowTitle()
                        readOnly: root._mode === 0
                        enabled: root._mode === 1
                        onAccepted: {
                            if (root._mode !== 1) return
                            if (backend) {
                                backend.setVersionWindowTitleMode(currentSelectedVersion, 1)
                                backend.setVersionWindowTitle(currentSelectedVersion, text.trim())
                            }
                        }
                    }

                    // ── 启动前命令（版本级）──
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("启动前命令")
                            font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textSecondary
                        }
                        Text {
                            text: qsTr("（游戏启动前执行，异步不阻塞）")
                            font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.textTertiary
                        }
                    }
                    InputBox {
                        Layout.fillWidth: true
                        placeholderText: qsTr("如：start D:\\tools\\sync.bat（留空不执行）")
                        text: root._resolvedPreLaunchCommand()
                        readOnly: root._mode === 0
                        enabled: root._mode === 1
                        onAccepted: {
                            if (root._mode !== 1) return
                            if (backend) {
                                backend.setVersionPreLaunchMode(currentSelectedVersion, 1)
                                backend.setVersionPreLaunchCommand(currentSelectedVersion, text.trim())
                            }
                        }
                    }

                    // ── 退出后命令（版本级）──
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("退出后命令")
                            font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textSecondary
                        }
                        Text {
                            text: qsTr("（游戏退出后执行，异步不阻塞）")
                            font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.textTertiary
                        }
                    }
                    InputBox {
                        Layout.fillWidth: true
                        placeholderText: qsTr("如：start D:\\tools\\backup.bat（留空不执行）")
                        text: root._resolvedPostExitCommand()
                        readOnly: root._mode === 0
                        enabled: root._mode === 1
                        onAccepted: {
                            if (root._mode !== 1) return
                            if (backend) {
                                backend.setVersionPostExitMode(currentSelectedVersion, 1)
                                backend.setVersionPostExitCommand(currentSelectedVersion, text.trim())
                            }
                        }
                    }
                }
            }

            Item { Layout.fillHeight: true; implicitHeight: 24 }
        }
    }
}
