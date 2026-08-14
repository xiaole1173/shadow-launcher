// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

Rectangle {
    id: page
    color: "transparent"

    // Set by MainWindow loader onLoaded — carries the loaded autoLangMode combo index
    property int _initLangModeIdx: 0

    property int currentSection: 0

    // ── 字节格式化（Java 下载进度显示，2026-08-08 修复跨作用域不可见 + 无效输入防御）──
    function _fmtBytes(bytes) {
        if (bytes === undefined || bytes === null || isNaN(bytes) || bytes <= 0) return "0 B"
        var units = ["B", "KB", "MB", "GB"]
        var i = 0
        var v = bytes
        while (v >= 1024 && i < units.length - 1) { v /= 1024; i++ }
        return v.toFixed(v >= 10 || i === 0 ? 0 : 1) + " " + units[i]
    }

    opacity: 0; y: 10
    Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeInDuration; easing.type: AnimationTokens.itemFadeInEasing } }
    Behavior on y { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
    Component.onCompleted: {
        opacity = 1; y = 0; appWindow.pageLoading = false
        // 触发一次 Java 前置检测（幂等：重复调用直接返回缓存）
        if (typeof backend !== "undefined" && backend && backend.javaBackend) {
            backend.javaBackend.scanSystemJavas()
        }
    }

    // Called when switching sections — show loading bar
    function switchSection(idx) {
        if (idx === currentSection) return
        currentSection = idx
        if (appWindow) appWindow.pageLoading = true
        sectionLoadTimer.restart()
        console.info("[UI] 设置 切段 section=" + idx)
    }
    // External entry (used by --navigate settings:about screenshot mode)
    function selectSection(idx) {
        currentSection = idx
        if (appWindow) appWindow.pageLoading = true
        sectionLoadTimer.restart()
        console.info("[UI] 设置 外部切段 section=" + idx)
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
                    width: nav.width - 16; height: 38; radius: StyleTokens.radiusMd
                    color: nav.currentIndex === index ? "#181c28" : (navMouse.containsMouse ? StyleTokens.bgSecondary : "transparent")
                    Behavior on color { ColorAnimation { duration: 200; easing.type: Easing.OutCubic } }
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
                            text: nav.navLabel(modelData.key); color: nav.currentIndex === index ? StyleTokens.textPrimary : "#8890a0"; font.pixelSize: StyleTokens.fontSizeMd
                            Behavior on color { ColorAnimation { duration: 200; easing.type: Easing.OutCubic } }
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
                width: 2; height: 38; color: "#5d6fe0"; radius: StyleTokens.radiusXs
                Behavior on y { SmoothedAnimation { velocity: 200; duration: 300 } }
            }
        }

        // Divider
        Rectangle { Layout.preferredWidth: 1; Layout.fillHeight: true; color: StyleTokens.bgInput }

        // Right content — lazy-loaded sections
        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true; color: "transparent"

            // Section 0: General (loaded immediately, stays cached)
            Loader {
                id: generalLoader
                anchors.fill: parent; anchors.margins: 24
                active: true
                opacity: page.currentSection === 0 ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                sourceComponent: generalComponent
                asynchronous: true
            }

            // Section 1: Java (external file)
            Loader {
                id: javaLoader
                anchors.fill: parent; anchors.margins: 24
                active: true
                source: "SettingsJavaPage.qml"
                opacity: page.currentSection === 1 ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                asynchronous: false
                onItemChanged: {
                    if (item) {
                        item.backend = Qt.binding(function() { return backend })
                        item.toastManager = toastManager
                        item.refreshAll()
                    }
                }
            }

            // Section 2: Memory (lazy loaded, cached after first load)
            Loader {
                id: memoryLoader
                anchors.fill: parent; anchors.margins: 24
                active: true
                opacity: page.currentSection === 2 ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                sourceComponent: memoryComponent
                asynchronous: true
            }

            // Section 3: Experimental (lazy loaded, cached after first load)
            Loader {
                id: experimentalLoader
                anchors.fill: parent; anchors.margins: 24
                active: true
                opacity: page.currentSection === 3 ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                source: "SettingsExperimentalPage.qml"
                asynchronous: true
            }

            // Section 4: About (lazy loaded, cached after first load)
            Loader {
                id: aboutLoader
                anchors.fill: parent; anchors.margins: 24
                active: true
                opacity: page.currentSection === 4 ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                sourceComponent: aboutComponent
                asynchronous: true
            }
        }
    }

    // ────── COMPONENTS ──────

    Component {
        id: generalComponent
        Flickable {
            id: generalFlick
            anchors.fill: parent
            contentHeight: generalCol.childrenRect.height + 40
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: generalCol
                width: parent.width
                spacing: 12
                Text { text: qsTr("通用设置"); font.pixelSize: StyleTokens.fontSizeXl; font.bold: true; color: StyleTokens.textPrimary }

                Rectangle { Layout.fillWidth: true; height: 52; radius: StyleTokens.radiusMd; color: StyleTokens.bgSecondary; border.color: StyleTokens.bgInput
                    RowLayout {
                        anchors.left: parent.left; anchors.leftMargin: 14
                        anchors.right: parent.right; anchors.rightMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 8
                        Text { text: qsTr("版本隔离"); font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.Medium; color: StyleTokens.textPrimary; Layout.fillWidth: true }
                        Text { text: "已开启"; font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary }
                        ShadowSwitch { id: isolationSwitch; Layout.alignment: Qt.AlignVCenter; checked: true; enabled: false }
                    }
                }

                // ═══ 下载设置 ═══
                Text { text: qsTr("下载"); font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.DemiBold; color: "#b8c0d0"; Layout.topMargin: 8 }

                // ── 文件下载源 ──
                Text { text: qsTr("文件下载源"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary }
                ShadowDropdown {
                    Layout.fillWidth: true
                    model: [
                        { text: qsTr("尽量使用镜像源"), value: 0 },
                        { text: qsTr("尽量使用官方源"), value: 1 },
                        { text: qsTr("尽量使用官方源，速度过慢切换镜像源"), value: 2 }
                    ]
                    labelKey: "text"
                    currentValue: (backend) ? backend.fileDownloadSource : 0
                    onValueSelected: function(v) { if (backend) backend.fileDownloadSource = v }
                }

                // ── 版本列表源 ──
                Text { text: qsTr("版本列表源"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textSubtle; Layout.topMargin: 8 }
                ShadowDropdown {
                    Layout.fillWidth: true
                    model: [
                        { text: qsTr("尽量使用镜像源"), value: 0 },
                        { text: qsTr("尽量使用官方源"), value: 1 },
                        { text: qsTr("尽量使用官方源，速度过慢切换镜像源"), value: 2 }
                    ]
                    labelKey: "text"
                    currentValue: (backend) ? backend.listDownloadSource : 0
                    onValueSelected: function(v) { if (backend) backend.listDownloadSource = v }
                }

                // ── 最大线程数 ──
                Text { id:thrLab; text:qsTr("最大线程数")+": "+thrSld._v; font.pixelSize:StyleTokens.fontSizeSm; color:StyleTokens.textSubtle; Layout.topMargin:8 }
                Item {
                    id:thrSld
                    Layout.fillWidth:true; Layout.preferredHeight:28
                    property int _v: (backend) ? backend.maxDownloadThreads : 64
                    Rectangle { id:thrTrk; height:5; radius: StyleTokens.radiusXs; color:StyleTokens.bgElevated
                        anchors.verticalCenter:parent.verticalCenter; anchors.left:parent.left; anchors.leftMargin:8; anchors.right:parent.right; anchors.rightMargin:8
                        Rectangle { anchors.left:parent.left; anchors.top:parent.top; anchors.bottom:parent.bottom
                            width:thrTrk.width*((thrSld._v-1)/127); radius: StyleTokens.radiusXs; color:StyleTokens.accentHover }
                    }
                    Rectangle { id:thrHnd; width:14; height:14; radius: StyleTokens.radiusMd; color:StyleTokens.textInverse; border.width:2; border.color:StyleTokens.accentHover
                        anchors.verticalCenter:thrTrk.verticalCenter
                        x:thrTrk.x+thrTrk.width*((thrSld._v-1)/127)-7 }
                    MouseArea { anchors.fill:parent
                        function snp(v){return Math.max(1,Math.min(128,Math.round(v)))}
                        onPositionChanged:function(m){if(pressed){thrSld._v=snp(1+(m.x-thrTrk.x)/thrTrk.width*127);if(backend)backend.maxDownloadThreads=thrSld._v}}
                        onClicked:function(m){thrSld._v=snp(1+(m.x-thrTrk.x)/thrTrk.width*127);if(backend)backend.maxDownloadThreads=thrSld._v}
                    }
                }

                // ── 速度限制 ──
                Text { id:spdLab; text:qsTr("速度限制")+": "+(spdSld._s>=0?spdSld._s.toFixed(1)+" MB/s":qsTr("无限制")); font.pixelSize:StyleTokens.fontSizeSm; color:StyleTokens.textSubtle; Layout.topMargin:8 }
                Item {
                    id:spdSld
                    Layout.fillWidth:true; Layout.preferredHeight:28
                    property double _s: (backend) ? backend.downloadSpeedLimitMB : -1
                    function s2i(v){if(v<0)return 21; if(v<=5)return Math.round(v*2); return 10+Math.round((v-5)/1.5)}
                    function i2s(i){if(i>=21)return -1; if(i<=10)return Math.max(0.1,i*0.5); return 5.0+(i-10)*1.5}
                    property int _i:s2i(_s)
                    Rectangle { id:spdTrk; height:5; radius: StyleTokens.radiusXs; color:StyleTokens.bgElevated
                        anchors.verticalCenter:parent.verticalCenter; anchors.left:parent.left; anchors.leftMargin:8; anchors.right:parent.right; anchors.rightMargin:8
                        Rectangle { anchors.left:parent.left; anchors.top:parent.top; anchors.bottom:parent.bottom
                            width:spdTrk.width*(spdSld._i/21); radius: StyleTokens.radiusXs; color:StyleTokens.accentHover }
                    }
                    Rectangle { id:spdHnd; width:14; height:14; radius: StyleTokens.radiusMd; color:StyleTokens.textInverse; border.width:2; border.color:StyleTokens.accentHover
                        anchors.verticalCenter:spdTrk.verticalCenter
                        x:spdTrk.x+spdTrk.width*(spdSld._i/21)-7 }
                    MouseArea { anchors.fill:parent
                        function snp(v){return Math.max(0,Math.min(21,Math.round(v)))}
                        onPositionChanged:function(m){if(pressed){spdSld._i=snp((m.x-spdTrk.x)/spdTrk.width*21);spdSld._s=spdSld.i2s(spdSld._i);if(backend)backend.downloadSpeedLimitMB=spdSld._s}}
                        onClicked:function(m){spdSld._i=snp((m.x-spdTrk.x)/spdTrk.width*21);spdSld._s=spdSld.i2s(spdSld._i);if(backend)backend.downloadSpeedLimitMB=spdSld._s}
                    }
                }

                // ═══ 启动细节（2026-08-08 低垂果实批）═══
                Text { text: qsTr("启动"); font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.DemiBold; color: "#b8c0d0"; Layout.topMargin: 8 }

                // ── 进程优先级 ──
                Text { text: qsTr("进程优先级"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    // 高
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 32
                        radius: StyleTokens.radiusMd
                        color: (backend && backend.processPriority === 0) ? StyleTokens.accentLight : StyleTokens.bgSecondary
                        border.color: (backend && backend.processPriority === 0) ? StyleTokens.accent : StyleTokens.bgInput
                        border.width: 1
                        scale: priHighMa.pressed ? 0.94 : 1.0
                        Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                        Behavior on color { ColorAnimation { duration: 150 } }
                        Text {
                            anchors.centerIn: parent
                            text: qsTr("高")
                            font.pixelSize: StyleTokens.fontSizeSm
                            color: (backend && backend.processPriority === 0) ? StyleTokens.textPrimary : StyleTokens.textTertiary
                        }
                        MouseArea {
                            id: priHighMa
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: { if (backend) backend.setProcessPriority(0) }
                        }
                    }

                    // 正常
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 32
                        radius: StyleTokens.radiusMd
                        color: (backend && backend.processPriority === 1) ? StyleTokens.accentLight : StyleTokens.bgSecondary
                        border.color: (backend && backend.processPriority === 1) ? StyleTokens.accent : StyleTokens.bgInput
                        border.width: 1
                        scale: priNormMa.pressed ? 0.94 : 1.0
                        Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                        Behavior on color { ColorAnimation { duration: 150 } }
                        Text {
                            anchors.centerIn: parent
                            text: qsTr("正常")
                            font.pixelSize: StyleTokens.fontSizeSm
                            color: (backend && backend.processPriority === 1) ? StyleTokens.textPrimary : StyleTokens.textTertiary
                        }
                        MouseArea {
                            id: priNormMa
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: { if (backend) backend.setProcessPriority(1) }
                        }
                    }

                    // 低
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 32
                        radius: StyleTokens.radiusMd
                        color: (backend && backend.processPriority === 2) ? StyleTokens.accentLight : StyleTokens.bgSecondary
                        border.color: (backend && backend.processPriority === 2) ? StyleTokens.accent : StyleTokens.bgInput
                        border.width: 1
                        scale: priLowMa.pressed ? 0.94 : 1.0
                        Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                        Behavior on color { ColorAnimation { duration: 150 } }
                        Text {
                            anchors.centerIn: parent
                            text: qsTr("低")
                            font.pixelSize: StyleTokens.fontSizeSm
                            color: (backend && backend.processPriority === 2) ? StyleTokens.textPrimary : StyleTokens.textTertiary
                        }
                        MouseArea {
                            id: priLowMa
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: { if (backend) backend.setProcessPriority(2) }
                        }
                    }
                }

                // ── GC 策略（全局默认）──
                Text { text: qsTr("GC 策略"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary; Layout.topMargin: 8 }
                ShadowDropdown {
                    Layout.fillWidth: true
                    model: [
                        { value: 0, label: qsTr("自动（推荐）") },
                        { value: 1, label: qsTr("分代 ZGC 优先") },
                        { value: 2, label: qsTr("仅 G1GC") },
                        { value: 3, label: qsTr("不指定（跟随自定义参数）") }
                    ]
                    valueKey: "value"
                    currentValue: (backend) ? backend.gcMode : 0
                    onValueSelected: function(v) { if (backend) backend.setGcMode(Number(v)) }
                }
                Text { text: qsTr("可在版本设置-启动配置中为单个版本单独覆盖"); font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.textMuted }

                // ── 自动进服（全局默认）──
                Text { text: qsTr("自动进服（全局默认）"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary; Layout.topMargin: 8 }
                InputBox {
                    Layout.fillWidth: true
                    placeholderText: qsTr("服务器地址，如 play.example.com:25565（留空不自动进服）")
                    text: (backend) ? (backend.autoJoinServer || "") : ""
                    onAccepted: {
                        if (backend) backend.setAutoJoinServer(text.trim())
                    }
                }

                // ── 全屏启动（全局默认）──
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("全屏启动"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary; Layout.topMargin: 8; Layout.fillWidth: true }
                    ShadowSwitch {
                        Layout.topMargin: 8
                        checked: (backend) ? !!backend.fullscreenEnabled : false
                        onToggled: { if (backend) backend.setFullscreenEnabled(checked) }
                    }
                }

                // ── 窗口标题（全局默认）──
                Text { text: qsTr("窗口标题"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary; Layout.topMargin: 8 }
                InputBox {
                    Layout.fillWidth: true
                    placeholderText: qsTr("启动后修改游戏窗口标题（留空不修改）")
                    text: (backend) ? (backend.windowTitleOverride || "") : ""
                    onAccepted: {
                        if (backend) backend.setWindowTitleOverride(text.trim())
                    }
                }

                // ── 启动前/退出后命令（全局默认）──
                Text { text: qsTr("启动前命令"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary; Layout.topMargin: 8 }
                InputBox {
                    Layout.fillWidth: true
                    placeholderText: qsTr("如：start D:\\tools\\sync.bat（留空不执行）")
                    text: (backend) ? (backend.preLaunchCommand || "") : ""
                    onAccepted: {
                        if (backend) backend.setPreLaunchCommand(text.trim())
                    }
                }
                Text { text: qsTr("退出后命令"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary; Layout.topMargin: 8 }
                InputBox {
                    Layout.fillWidth: true
                    placeholderText: qsTr("如：start D:\\tools\\backup.bat（留空不执行）")
                    text: (backend) ? (backend.postExitCommand || "") : ""
                    onAccepted: {
                        if (backend) backend.setPostExitCommand(text.trim())
                    }
                }
                Text { text: qsTr("以上默认值可在版本设置-启动配置中按版本单独覆盖"); font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.textMuted }

                // ═══ 配置管理（设置导入导出）═══
                Text { text: qsTr("配置管理"); font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.DemiBold; color: "#b8c0d0"; Layout.topMargin: 8 }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    ShadowButton {
                        text: qsTr("导出设置")
                        Layout.preferredWidth: 120; Layout.preferredHeight: 32
                        accentColor: StyleTokens.accentSubtle
                        font.pixelSize: StyleTokens.fontSizeSm
                        onClicked: settingsSaveDialog.open()
                    }
                    ShadowButton {
                        text: qsTr("导入设置")
                        Layout.preferredWidth: 120; Layout.preferredHeight: 32
                        accentColor: StyleTokens.accentSubtle
                        font.pixelSize: StyleTokens.fontSizeSm
                        onClicked: settingsOpenDialog.open()
                    }
                }
                Text { text: qsTr("令牌、密钥、Beta 密钥等敏感信息不会导出"); font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.textMuted }

                Item { Layout.fillHeight: true }
            }
        }

    }

    // ── 设置文件对话框（导出/导入）──
    FileDialog {
        id: settingsSaveDialog
        title: qsTr("导出设置")
        fileMode: FileDialog.SaveFile
        nameFilters: ["设置文件 (*.ini)"]
        defaultSuffix: "ini"
        currentFile: "shadow_settings.ini"
        onAccepted: {
            if (!backend) return
            var sel = settingsSaveDialog.selectedFile
            var path = ""
            if (typeof sel === "string") {
                path = sel
            } else if (sel && typeof sel.toString === "function") {
                path = sel.toString()
            }
            if (path.indexOf("file:///") === 0) path = path.substring(8)
            if (!path.toLowerCase().endsWith(".ini")) path = path + ".ini"
            var ok = backend.exportSettingsToFile(path)
            toastManager.show(ok ? qsTr("设置已导出") : qsTr("导出失败"))
        }
    }
    FileDialog {
        id: settingsOpenDialog
        title: qsTr("导入设置")
        fileMode: FileDialog.OpenFile
        nameFilters: ["设置文件 (*.ini)"]
        onAccepted: {
            if (!backend) return
            var sel = settingsOpenDialog.selectedFile
            var path = ""
            if (typeof sel === "string") {
                path = sel
            } else if (sel && typeof sel.toString === "function") {
                path = sel.toString()
            }
            if (path.indexOf("file:///") === 0) path = path.substring(8)
            var ok = backend.importSettingsFromFile(path)
            toastManager.show(ok ? qsTr("设置已导入") : qsTr("导入失败或文件为空"))
        }
    }

    Component {
        id: memoryComponent
        Loader {
            anchors.fill: parent
            source: "SettingsMemorySection.qml"
            onItemChanged: {
                if (item) {
                    item.backend = backend
                    item.refreshAll()
                }
            }
        }
    }


    // ── 鸣谢条目组件：名字（可点击开网页）+ 徽标 + 描述；风格与原卡片一致 ──
    // 注意：纯 anchors 布局（勿引入 Row/Column Positioner——其子项用 anchors 属未定义行为，
    // 曾导致描述文字与名字行重叠，2026-08-12 修复）
    Component {
        id: ackItemComp
        Item {
            id: ackItem
            width: parent.width
            height: ackDesc.y + ackDesc.height

            property bool ackHovered: false

            Text {
                id: ackNameText
                text: modelData.name
                font.pixelSize: StyleTokens.fontSizeMd
                font.bold: true
                color: ackItem.ackHovered ? StyleTokens.accent : StyleTokens.textPrimary
                font.underline: ackItem.ackHovered
                Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
            }
            Rectangle {
                visible: modelData.badge !== ""
                color: StyleTokens.accentSubtle; radius: StyleTokens.radiusSm
                implicitHeight: ackBadgeText.implicitHeight + 6
                implicitWidth: ackBadgeText.implicitWidth + 10
                anchors.left: ackNameText.right
                anchors.leftMargin: 8
                anchors.verticalCenter: ackNameText.verticalCenter
                Text {
                    id: ackBadgeText
                    anchors.centerIn: parent
                    text: modelData.badge
                    font.pixelSize: StyleTokens.fontSizeXs
                    color: StyleTokens.accentLink
                }
            }
            MouseArea {
                id: ackNameMouse
                anchors.fill: ackNameText
                hoverEnabled: modelData.url !== ""
                cursorShape: modelData.url !== "" ? Qt.PointingHandCursor : Qt.ArrowCursor
                onEntered: ackItem.ackHovered = true
                onExited: ackItem.ackHovered = false
                onClicked: {
                    if (modelData.url !== "") Qt.openUrlExternally(modelData.url)
                }
            }
            Text {
                id: ackDesc
                text: modelData.desc
                font.pixelSize: StyleTokens.fontSizeSm
                color: "#8890a0"
                width: parent.width
                wrapMode: Text.WordWrap
                lineHeight: 1.35
                anchors.top: ackNameText.bottom
                anchors.topMargin: 3
            }
        }
    }

    Component {
        id: aboutComponent
        Flickable {
            id: aboutFlick
            anchors.fill: parent
            contentHeight: aboutColumn.implicitHeight
            flickableDirection: Flickable.VerticalFlick
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ColumnLayout {
                id: aboutColumn
                anchors.left: parent.left; anchors.right: parent.right
                spacing: 14
                Text { text: qsTr("关于"); font.pixelSize: StyleTokens.fontSizeXl; font.bold: true; color: StyleTokens.textPrimary }

                // About card
                Rectangle {
                    Layout.fillWidth: true; radius: StyleTokens.radiusLg; color: StyleTokens.bgSecondary; border.color: StyleTokens.bgInput
                    Layout.preferredHeight: aboutCardContent.height + 34
                    ColumnLayout {
                        id: aboutCardContent
                        anchors.left: parent.left; anchors.right: parent.right
                        anchors.top: parent.top; anchors.margins: 17; spacing: 8
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "Shadow Launcher"; font.pixelSize: StyleTokens.fontSizeLg; font.bold: true; color: StyleTokens.textPrimary }
                            Item { Layout.fillWidth: true }
                            Text { text: backend ? backend.appVersion : ""; font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textMuted }
                        }
                        RowLayout {
                            Layout.fillWidth: true; spacing: 8
                            ShadowButton {
                                accentColor: "#2d3748"
                                text: qsTr("GitHub")
                                Layout.preferredWidth: 74; Layout.preferredHeight: 26
                                font.pixelSize: StyleTokens.fontSizeSm
                                onClicked: Qt.openUrlExternally("https://github.com/xiaole1173/shadow-launcher")
                            }
                            ShadowButton {
                                accentColor: StyleTokens.accentSubtle
                                text: backend && backend.updateChecking ? qsTr("检查中...") : qsTr("检查更新")
                                Layout.preferredWidth: 74; Layout.preferredHeight: 26
                                font.pixelSize: StyleTokens.fontSizeSm
                                enabled: backend && !backend.updateChecking
                                onClicked: {
                                    if (backend) backend.checkForUpdate()
                                }
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("(C) 2025-2026 影 / Shadow / xiaole1173")
                            font.pixelSize: StyleTokens.fontSizeSm; color: "#707888"
                        }
                        ShadowButton {
                            accentColor: "transparent"
                            text: qsTr("GNU AGPL v3 许可证")
                            Layout.preferredWidth: 130; Layout.preferredHeight: 24
                            font.pixelSize: StyleTokens.fontSizeSm
                            onClicked: Qt.openUrlExternally("https://github.com/xiaole1173/shadow-launcher/blob/master/LICENSE")
                        }
                        Text {
                            Layout.fillWidth: true; wrapMode: Text.WordWrap
                            text: qsTr("非 Minecraft 官方产品。未经 Mojang 或 Microsoft 批准，也不与 Mojang 或 Microsoft 关联。")
                            font.pixelSize: StyleTokens.fontSizeSm; color: "#707888"; lineHeight: 1.4
                        }
                    }
                }



                // Acknowledgments card
                Rectangle {
                    Layout.fillWidth: true; radius: StyleTokens.radiusLg; color: StyleTokens.bgSecondary; border.color: StyleTokens.bgInput
                    Layout.preferredHeight: ackContent.height + 34
                    ColumnLayout {
                        id: ackContent
                        anchors.left: parent.left; anchors.right: parent.right
                        anchors.top: parent.top; anchors.margins: 17; spacing: 12
                        Text { text: qsTr("鸣谢"); font.pixelSize: StyleTokens.fontSizeXl; font.bold: true; color: StyleTokens.textPrimary }
                        Column { Layout.fillWidth: true; spacing: 14
                            Repeater {
                                width: parent.width
                                model: [
                                    { name: "bangbang93", url: "https://afdian.com/a/bangbang93", badge: "",
                                      desc: qsTr("提供了镜像源和Forge安装工具。点击名字直达镜像源赞助页。") },
                                    { name: "z0z0r4", url: "https://www.mcimirror.top/", badge: "",
                                      desc: qsTr("提供了MCIM镜像源，主要用于Mod等资源的下载，虽然不大稳定（也可能是我测试太多给我限速了），但好歹不用死守着有时非常逆天的官方源了。") },
                                    { name: "Lucide", url: "https://lucide.dev/", badge: "",
                                      desc: qsTr("提供了启动器目前所有可见的图标！点击名字直达网页。") },
                                    { name: "ChunMoMo", url: "", badge: qsTr("内测人员"),
                                      desc: qsTr("直接提供了一台能远程操控的电脑给开发者测试初代联机功能（而且是整整一个下午）！虽然初代联机功能已经废弃，但还是得感谢。") }
                                ]
                                delegate: ackItemComp
                            }
                        }
                    }
                }

                // Internal testing supporters card
                Rectangle {
                    Layout.fillWidth: true; radius: StyleTokens.radiusLg; color: StyleTokens.bgSecondary; border.color: StyleTokens.bgInput
                    Layout.preferredHeight: testerContent.height + 34
                    ColumnLayout {
                        id: testerContent
                        anchors.left: parent.left; anchors.right: parent.right
                        anchors.top: parent.top; anchors.margins: 17; spacing: 12
                        Text { text: qsTr("内测人员的支持"); font.pixelSize: StyleTokens.fontSizeXl; font.bold: true; color: StyleTokens.textPrimary }
                        Text {
                            Layout.fillWidth: true; wrapMode: Text.WordWrap
                            text: qsTr("这里非常荣幸地宣布：jh_hello、XChenyYa二人以神奇的bug体制挖掘出了启动器的很多莫名其妙的bug，包括但不限于下载、UI显示、联机等多个方面。")
                            font.pixelSize: StyleTokens.fontSizeSm; color: "#8890a0"; lineHeight: 1.35
                        }
                        Text {
                            Layout.fillWidth: true; wrapMode: Text.WordWrap
                            text: qsTr("非常感谢jh_hello、XChenyYa、渡、LUVlhr等人对联机功能后续测试的全方位支持！")
                            font.pixelSize: StyleTokens.fontSizeSm; color: "#8890a0"; lineHeight: 1.35
                        }
                        Text {
                            Layout.fillWidth: true; wrapMode: Text.WordWrap
                            text: qsTr("以及其他参与内测的小伙伴们也十分感谢！")
                            font.pixelSize: StyleTokens.fontSizeSm; color: "#8890a0"; lineHeight: 1.35
                        }
                    }
                }

                // Launcher Logs card
                Rectangle {
                    Layout.fillWidth: true; radius: StyleTokens.radiusLg; color: StyleTokens.bgSecondary; border.color: StyleTokens.bgInput
                    Layout.preferredHeight: logsRow.height + 24
                    RowLayout {
                        id: logsRow
                        anchors.left: parent.left; anchors.right: parent.right
                        anchors.top: parent.top; anchors.margins: 12
                        spacing: 10
                        Image {
                            source: "icons/lucide/folder-open.svg"; width: 18; height: 18
                        }
                        Text {
                            text: qsTr("启动器日志"); font.pixelSize: StyleTokens.fontSizeMd; color: StyleTokens.textPrimary
                            Layout.fillWidth: true
                        }
                        ShadowButton {
                            accentColor: "#2d3748"; text: qsTr("打开文件夹")
                            Layout.preferredHeight: 26; font.pixelSize: StyleTokens.fontSizeSm
                            onClicked: backend.openLauncherLogsFolder()
                        }
                    }
                }

                // ═══ One-click Java install card ═══
                Rectangle {
                    Layout.fillWidth: true; radius: StyleTokens.radiusLg; color: StyleTokens.bgSecondary; border.color: StyleTokens.bgInput
                    Layout.preferredHeight: javaCardContent.height + 34

                    ColumnLayout {
                        id: javaCardContent
                        anchors.left: parent.left; anchors.right: parent.right
                        anchors.top: parent.top; anchors.margins: 17; spacing: 8

                        RowLayout {
                            Layout.fillWidth: true; spacing: 8
                            Image { source: "icons/lucide/download-cloud.svg"; width: 18; height: 18 }
                            Text {
                                text: qsTr("一键安装所需 Java"); font.pixelSize: StyleTokens.fontSizeMd; font.bold: true
                                color: StyleTokens.textPrimary; Layout.fillWidth: true
                            }
                            // 架构徽标
                            Rectangle {
                                visible: backend && backend.javaBackend && backend.javaBackend.cpuArch
                                color: StyleTokens.accentSubtle; radius: StyleTokens.radiusSm
                                implicitHeight: archTagText.implicitHeight + 6
                                implicitWidth: archTagText.implicitWidth + 10
                                Text {
                                    id: archTagText
                                    anchors.centerIn: parent
                                    text: backend && backend.javaBackend && backend.javaBackend.cpuArch
                                        ? backend.javaBackend.cpuArch : ""
                                    font.pixelSize: StyleTokens.fontSizeXs
                                    color: StyleTokens.accentLink
                                }
                            }
                        }

                        Text {
                            Layout.fillWidth: true; wrapMode: Text.WordWrap
                            text: qsTr("包括游戏所需 Java 和启动器所需 Java（Java 8 / 17 / 25 JRE，下载到启动器目录，不写入注册表）")
                            font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textSubtle; lineHeight: 1.4
                        }

                        // ── 前置检测状态（已检测到 → 绿色跳过 / 缺失 → 将安装） ──
                        ColumnLayout {
                            id: javaStatusRow
                            visible: backend && backend.javaBackend
                            Layout.fillWidth: true; spacing: 5

                            // root._javaScanTick 递增时触发 detected 重新求值（detectedSystemJavas 是函数）
                            property var detected: (page._javaScanTick >= 0 && backend && backend.javaBackend)
                                ? (backend.javaBackend.detectedSystemJavas() || []) : []

                            function hasMajor(major) {
                                for (var i = 0; i < detected.length; i++) {
                                    if (detected[i].major === major) return true
                                }
                                return false
                            }
                            function labelFor(major) {
                                for (var i = 0; i < detected.length; i++) {
                                    if (detected[i].major === major) {
                                        return "Java " + major + " (" + (detected[i].isJdk ? "JDK" : "JRE") + ")"
                                    }
                                }
                                return "Java " + major
                            }

                            Repeater {
                                model: [
                                    { major: 8,  label: "Java 8" },
                                    { major: 17, label: "Java 17" },
                                    { major: 25, label: "Java 25" }
                                ]
                                delegate: RowLayout {
                                    Layout.fillWidth: true; spacing: 6
                                    Rectangle {
                                        width: 6; height: 6; radius: 3
                                        color: parent.parent.hasMajor(modelData.major) ? StyleTokens.success : StyleTokens.textMuted
                                    }
                                    Text {
                                        text: modelData.label
                                        font.pixelSize: StyleTokens.fontSizeXs
                                        color: parent.parent.hasMajor(modelData.major) ? StyleTokens.success : StyleTokens.textTertiary
                                        Layout.preferredWidth: 58
                                    }
                                    Text {
                                        Layout.fillWidth: true; elide: Text.ElideRight
                                        text: parent.parent.hasMajor(modelData.major)
                                            ? (qsTr("已检测到 ") + parent.parent.labelFor(modelData.major))
                                            : qsTr("未检测到，将自动安装")
                                        font.pixelSize: StyleTokens.fontSizeXs
                                        color: parent.parent.hasMajor(modelData.major) ? "#7ec8a0" : StyleTokens.textMuted
                                    }
                                }
                            }
                        }

                        // 安装进度 / 状态行
                        RowLayout {
                            visible: backend && backend.javaBackend && backend.javaBackend.javaInstalling
                            Layout.fillWidth: true; spacing: 8
                            LoadingSpinner {
                                width: 16; height: 16
                                running: backend.javaBackend.javaInstalling
                            }
                            Text {
                                Layout.fillWidth: true; elide: Text.ElideRight
                                text: backend.javaBackend.javaInstallStatus || ""
                                font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary
                            }
                            Text {
                                text: "%1/%2".arg(backend.javaBackend.javaInstallStep).arg(backend.javaBackend.javaInstallTotal)
                                font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.textMuted
                            }
                        }

                        // ── 下载进度条（下载中显示百分比 + 速度） ──
                        ColumnLayout {
                            visible: backend && backend.javaBackend && backend.javaBackend.javaInstalling
                                && backend.javaBackend.javaDownloadTotal > 0
                            Layout.fillWidth: true; spacing: 4

                            Rectangle {
                                Layout.fillWidth: true; height: 6; radius: 3
                                color: StyleTokens.bgInput
                                Rectangle {
                                    width: parent.width * (backend.javaBackend.javaDownloadPercent / 100.0)
                                    height: parent.height; radius: 3
                                    color: StyleTokens.accent
                                    Behavior on width { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true; spacing: 6
                                Text {
                                    Layout.fillWidth: true
                                    text: (backend.javaBackend.javaDownloadTotal > 0)
                                        ? "%1%  %2 / %3".arg(
                                            backend.javaBackend.javaDownloadPercent,
                                            _fmtBytes(backend.javaBackend.javaDownloadBytes),
                                            _fmtBytes(backend.javaBackend.javaDownloadTotal))
                                        : ""
                                    font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.textMuted
                                }
                                Text {
                                    text: backend.javaBackend.javaDownloadSpeedMBps > 0
                                        ? backend.javaBackend.javaDownloadSpeedMBps.toFixed(1) + " MB/s" : ""
                                    font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.accentLink
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true; spacing: 8
                            ShadowButton {
                                accentColor: backend && backend.javaBackend && backend.javaBackend.javaInstalling
                                    ? "#4a3a20" : StyleTokens.accent
                                text: backend && backend.javaBackend && backend.javaBackend.javaInstalling
                                    ? qsTr("安装中…") : qsTr("一键安装")
                                Layout.preferredWidth: 110; Layout.preferredHeight: 28
                                font.pixelSize: StyleTokens.fontSizeSm
                                enabled: backend && backend.javaBackend && !backend.javaBackend.javaInstalling
                                onClicked: {
                                    if (backend && backend.javaBackend) backend.javaBackend.installRequiredJavas()
                                }
                            }
                            ShadowButton {
                                accentColor: "#3a2020"
                                text: qsTr("取消")
                                Layout.preferredWidth: 70; Layout.preferredHeight: 28
                                font.pixelSize: StyleTokens.fontSizeSm
                                visible: backend && backend.javaBackend && backend.javaBackend.javaInstalling
                                onClicked: {
                                    if (backend && backend.javaBackend) backend.javaBackend.cancelJavaInstall()
                                }
                            }
                            Item { Layout.fillWidth: true }
                        }
                    }
                }
            }
        }
    }

    // ── Background crop overlay ──
    Loader {
        id: bgCropOverlay
        active: false
        sourceComponent: bgCropComp
        anchors.fill: parent; z: 10
    }
    Component {
        id: bgCropComp
        BackgroundCropOverlay {
            anchors.fill: parent
            cropAR: appWindow ? (appWindow.width / appWindow.height) : 1.778
            onClosed: bgCropOverlay.active = false
        }
    }

    // ── Update check connections ──
    // 注：toastMessage 全局监听已移到 MainWindow（onToastMessage），
    // 此处不再重复监听，避免同一条消息弹两次。

    // ── One-click Java install feedback ──
    // root 级扫描计数器：Connections 无法直接访问 aboutComponent 内的组件，
    // 通过这个计数器驱动 javaStatusRow 的 detected 重绑定
    property int _javaScanTick: 0
    Connections {
        target: (typeof backend !== "undefined" && backend && backend.javaBackend) ? backend.javaBackend : null
        function onSystemJavaScanFinished() {
            _javaScanTick++
        }
        function onJavaInstalled(label, path, skipped) {
            if (toastManager) {
                toastManager.show(skipped ? (label + " 已存在，跳过") : (label + " 安装完成"), 3500)
            }
        }
        function onJavaInstallFinished(ok, error) {
            if (toastManager) {
                if (ok) toastManager.show(qsTr("全部所需 Java 就绪"), 4500)
                else toastManager.show(qsTr("Java 安装失败: %1").arg(error || qsTr("未知错误")), 6000)
            }
        }
        function onLogMessage(msg) {
            // 只弹关键信息（ARM64 降级/扫描完成/锁定/异常），过滤常规进度文字
            if (!msg || !toastManager) return
            if (msg.indexOf("正在") === 0 || msg.indexOf("前置检测完成：系统中已有") === 0) return
            toastManager.show(msg, 4000)
        }
    }

} // end SettingsPage
