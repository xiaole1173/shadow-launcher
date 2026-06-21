import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

// ========== VERSION SETTINGS OVERLAY ==========
Rectangle {
    id: versionSettingsOverlay
    anchors.fill: parent; color: "#0c0f16"; z: 5
    onVisibleChanged: {
        if (visible && backend) {
            // 版本切换时刷新所有数据列表（跟随版本隔离）
            backend.refreshVersionDetails()
            modListModel.clear(); var m = backend.listMods(); for (var i = 0; i < m.length; i++) modListModel.append(m[i])
            rpListModel.clear(); var p = backend.listResourcePacks(); for (var i = 0; i < p.length; i++) rpListModel.append(p[i])
            saveListModel.clear(); var s = backend.listSaves(); for (var i = 0; i < s.length; i++) saveListModel.append(s[i])
            // 重置校验状态
            _verifyRunning = false
            _verifyProgressDone = 0
            _verifyProgressTotal = 0
            _verifyResultText = ""
            _verifyResultOk = false
        }
    }

    // ── 校验状态（本地追踪，不依赖后端不触发的NOTIFY信号） ──
    property bool _verifyRunning: false
    property int _verifyProgressDone: 0
    property int _verifyProgressTotal: 0
    property string _verifyResultText: ""
    property bool _verifyResultOk: false
    property var _verifyFailedFiles: []
    property bool _verifyHasFailed: false

    // ── 信号连接 ──
    Connections {
        target: backend
        enabled: versionSettingsOverlay.visible

        function onVerifyStarted() {
            versionSettingsOverlay._verifyRunning = true
            versionSettingsOverlay._verifyProgressDone = 0
            versionSettingsOverlay._verifyProgressTotal = 0
            versionSettingsOverlay._verifyResultText = ""
            versionSettingsOverlay._verifyResultOk = false
            versionSettingsOverlay._verifyFailedFiles = []
            versionSettingsOverlay._verifyHasFailed = false
        }

        function onVerifyProgress(checked, total) {
            versionSettingsOverlay._verifyProgressDone = checked
            versionSettingsOverlay._verifyProgressTotal = total
        }

        function onVerifyFinished(allPassed) {
            versionSettingsOverlay._verifyRunning = false
            versionSettingsOverlay._verifyResultOk = allPassed
            // 不覆盖 logMessage 已积累的详细结果，仅在结果为空时显示默认文本
            if (versionSettingsOverlay._verifyResultText === "") {
                var total = versionSettingsOverlay._verifyProgressTotal
                versionSettingsOverlay._verifyResultText = allPassed
                    ? ("✅ 校验完成: " + total + " 个文件全部通过")
                    : ("❌ 校验完成: " + total + " 个文件全部通过。")
            }
        }

        function onVerifyFailedFiles(files) {
            versionSettingsOverlay._verifyFailedFiles = files || []
            versionSettingsOverlay._verifyHasFailed = (files && files.length > 0)
        }

        function onLogMessage(msg) {
            // 如果正在校验并且收到日志，追加到结果文本
            if (versionSettingsOverlay._verifyRunning || msg.indexOf("校验") >= 0) {
                if (versionSettingsOverlay._verifyResultText !== "") {
                    versionSettingsOverlay._verifyResultText += "\n" + msg
                } else {
                    versionSettingsOverlay._verifyResultText = msg
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent; anchors.margins: 16; spacing: 0

        // TOP BAR: version info + actions ===
        Rectangle {
            Layout.fillWidth: true; height: 56; radius: 8
            color: "#11141c"; border.color: "#1a1e28"
            RowLayout {
                anchors.fill: parent; anchors.margins: 14; spacing: 12

                // Back button
                Rectangle {
                    width: 60; height: 28; radius: 6; color: "transparent"; border.color: "#1a1f2e"
                    scale: backHover.containsMouse ? 1.06 : 1.0
                    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                    Text { anchors.centerIn: parent; text: "← 返回"; font.pixelSize: 11; color: "#989cb0" }
                    MouseArea { id: backHover; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { showVersionSettings = false } }
                }

                // Version label
                Text {
                    Layout.fillWidth: true
                    text: currentSelectedVersion || "未选择版本"
                    font.pixelSize: 16; font.weight: Font.Bold; color: "#e4e8f2"
                }

                // Loader tag
                Rectangle {
                    visible: {
                        if (!backend || !backend.versionDetails) return false
                        for (var i = 0; i < backend.versionDetails.length; i++)
                            if (backend.versionDetails[i].id === currentSelectedVersion) return true
                        return false
                    }
                    height: 20; implicitWidth: settingsLoaderTag.implicitWidth + 10; radius: 4
                    color: {
                        if (!backend || !backend.versionDetails) return "#4a6a8a"
                        for (var i = 0; i < backend.versionDetails.length; i++) {
                            if (backend.versionDetails[i].id === currentSelectedVersion) {
                                var t = backend.versionDetails[i].loaderType
                                if (t === "Forge") return "#c05050"
                                if (t === "Fabric") return "#50a0c0"
                                if (t === "NeoForge") return "#c08050"
                                if (t === "Quilt") return "#50c0a0"
                                return "#4a6a8a"
                            }
                        }
                        return "#4a6a8a"
                    }
                    Text {
                        id: settingsLoaderTag
                        anchors.centerIn: parent
                        text: {
                            if (!backend || !backend.versionDetails) return ""
                            for (var i = 0; i < backend.versionDetails.length; i++) {
                                if (backend.versionDetails[i].id === currentSelectedVersion)
                                    return backend.versionDetails[i].loaderType || ""
                            }
                            return ""
                        }
                        font.pixelSize: 10; font.weight: Font.Medium; color: "#e8ecf8"
                    }
                }

                // Spacer
                Item { Layout.fillWidth: true }

                // Launch button
                Rectangle {
                    width: 100; height: 32; radius: 6; color: "#3a4eb8"
                    Text { anchors.centerIn: parent; text: "启动"; font.pixelSize: 13; font.weight: Font.Bold; color: "#e8ecf8" }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (!backend) return
                            if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }
                            backend.launch(currentSelectedVersion)
                        }
                    }
                }
            }
        }

        Item { Layout.preferredHeight: 12 }

        // BODY: sidebar + content ═══
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 16
        Rectangle {
            Layout.preferredWidth: 170; Layout.fillHeight: true; color: "transparent"
            property var sectionModel: [
                { text: "概览", icon: "" },
                { text: "启动配置", icon: "" },
                { text: "Mod 管理", icon: "" },
                { text: "资源包管理", icon: "" },
                { text: "存档管理", icon: "" },
                { text: "工具与维护", icon: "" }
            ]
            ListView {
                id: settingsNav
                anchors.fill: parent
                model: parent.sectionModel
                delegate: Rectangle {
                    width: settingsNav.width; height: 36; radius: 6
                    color: ListView.isCurrentItem ? "#162040" : (mouseArea2.containsMouse ? "#11141c" : "transparent")
                    scale: mouseArea2.containsMouse ? 1.03 : 1.0
                    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                    Rectangle { anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 3; color: ListView.isCurrentItem ? "#5080e8" : "transparent" }
                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 16; spacing: 10
                        Text {
                            text: modelData.text
                            font.pixelSize: 13
                            color: ListView.isCurrentItem ? "#e0e4f8" : (mouseArea2.containsMouse ? "#e4e8fc" : "#9498ac")
                            font.weight: ListView.isCurrentItem ? Font.Bold : Font.Normal
                        }
                        Item { Layout.fillWidth: true }
                    }
                    MouseArea { id: mouseArea2; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: settingsNav.currentIndex = index }
                }
                currentIndex: 0
            }
        }
        Rectangle {
        }  // close sidebar Rectangle
        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true
            color: "#11141c"; radius: 8; border.color: "#1a1e28"

            // Section 0: 概览 ===
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 24; spacing: 12
                visible: settingsNav.currentIndex === 0

                // Quick info
                Rectangle {
                    Layout.fillWidth: true; height: 52; radius: 6; color: "#0d1018"
                    RowLayout {
                        anchors.fill: parent; anchors.margins: 14; spacing: 12
                        ColumnLayout { Layout.fillWidth: true; spacing: 2
                            Text { text: "占用空间"; font.pixelSize: 10; color: "#a8b0c0" }
                            Text { text: backend && backend.currentVersionSummary ? backend.currentVersionSummary.sizeDisplay : "-"; font.pixelSize: 14; font.weight: Font.Medium; color: "#d4dcf0" }
                        }
                        Rectangle { width: 1; height: 32; color: "#1a1f2e" }
                        ColumnLayout { Layout.fillWidth: true; spacing: 2
                            Text { text: "已装 Mod"; font.pixelSize: 10; color: "#a8b0c0" }
                            Text { text: (backend && backend.currentVersionSummary ? backend.currentVersionSummary.modCount : 0) + " 个"; font.pixelSize: 14; font.weight: Font.Medium; color: "#d4dcf0" }

                        }
                        Rectangle { width: 1; height: 32; color: "#1a1f2e" }
                        ColumnLayout { Layout.fillWidth: true; spacing: 2
                            Text { text: "版本隔离"; font.pixelSize: 10; color: "#a8b0c0" }
                            Text { text: backend && backend.isolationEnabled ? "已开启" : "未开启"; font.pixelSize: 14; font.weight: Font.Medium; color: backend && backend.isolationEnabled ? "#4bc870" : "#707088" }
                        }
                    }
                }

                // Shortcuts
                Text { text: "快捷入口"; font.pixelSize: 10; color: "#9ca0b4"; font.letterSpacing: 1.5 }
                Flow {
                    Layout.fillWidth: true; spacing: 8

                    // Always visible
                    Rectangle { width: 120; height: 32; radius: 6; color: shortcutHover0.hovered ? "#3a5ed0" : "#2a4590"
                        scale: shMouse0.pressed ? 0.92 : 1.0
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        Text { anchors.centerIn: parent; text: "版本文件夹"; font.pixelSize: 11; color: "#e8ecf8" }
                        HoverHandler { id: shortcutHover0 }
                        MouseArea { id: shMouse0; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }
                                if (backend) {
                                    backend.openVersionDir(currentSelectedVersion)
                                    toastManager.show("已打开版本文件夹")
                                }
                            }
                        }
                    }
                    Rectangle { width: 120; height: 32; radius: 6; color: shortcutHover1.hovered ? "#3a5ed0" : "#2a4590"
                        scale: shMouse1.pressed ? 0.92 : 1.0
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        Text { anchors.centerIn: parent; text: "存档文件夹"; font.pixelSize: 11; color: "#e8ecf8" }
                        HoverHandler { id: shortcutHover1 }
                        MouseArea { id: shMouse1; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: { if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }; if (backend) { if (backend.openSavesFolder(currentSelectedVersion)) { toastManager.show("已打开存档文件夹") } else { toastManager.show("无存档文件夹") } } }
                        }
                    }
                    Rectangle { width: 120; height: 32; radius: 6; color: shortcutHover2.hovered ? "#3a5ed0" : "#2a4590"
                        scale: shMouse2.pressed ? 0.92 : 1.0
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        Text { anchors.centerIn: parent; text: "截图文件夹"; font.pixelSize: 11; color: "#e8ecf8" }
                        HoverHandler { id: shortcutHover2 }
                        MouseArea { id: shMouse2; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: { if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }; if (backend) { if (backend.openScreenshotsFolder(currentSelectedVersion)) { toastManager.show("已打开截图文件夹") } else { toastManager.show("无截图文件夹") } } }
                        }
                    }
                    Rectangle { width: 120; height: 32; radius: 6; color: shortcutHover6.hovered ? "#3a5ed0" : "#2a4590"
                        scale: shMouse6.pressed ? 0.92 : 1.0
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        Text { anchors.centerIn: parent; text: "logs 日志"; font.pixelSize: 11; color: "#e8ecf8" }
                        HoverHandler { id: shortcutHover6 }
                        MouseArea { id: shMouse6; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: { if (backend) { if (backend.openLogsFolder(currentSelectedVersion)) { toastManager.show("已打开日志文件夹") } else { toastManager.show("无日志文件夹") } } }
                        }
                    }
                    Rectangle { width: 130; height: 32; radius: 6; color: shortcutHover7.hovered ? "#3a5ed0" : "#2a4590"
                        scale: shMouse7.pressed ? 0.92 : 1.0
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        Text { anchors.centerIn: parent; text: "最新启动日志"; font.pixelSize: 11; color: "#e8ecf8" }
                        HoverHandler { id: shortcutHover7 }
                        MouseArea { id: shMouse7; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: { if (backend) { if (backend.openLatestLog(currentSelectedVersion)) { toastManager.show("已打开最新日志") } else { toastManager.show("无日志文件") } } }
                        }
                    }
                    Rectangle { width: 130; height: 32; radius: 6; color: shortcutHover8.hovered ? "#c85050" : "#9a3838"
                        scale: shMouse8.pressed ? 0.92 : 1.0
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        Text { anchors.centerIn: parent; text: "崩溃日志"; font.pixelSize: 11; color: "#e8ecf8" }
                        HoverHandler { id: shortcutHover8 }
                        MouseArea { id: shMouse8; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: { if (backend) { if (backend.openCrashLog(currentSelectedVersion)) { toastManager.show("已打开崩溃日志") } else { toastManager.show("无崩溃报告") } } }
                        }
                    }

                    // Copy path
                    Rectangle { width: 130; height: 32; radius: 6; color: shortcutHoverCp.hovered ? "#3a5ed0" : "#2a4590"
                        scale: shMouseCp.pressed ? 0.92 : 1.0
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        Text { anchors.centerIn: parent; text: "复制版本路径"; font.pixelSize: 11; color: "#e8ecf8" }
                        HoverHandler { id: shortcutHoverCp }
                        MouseArea { id: shMouseCp; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }
                                if (backend) { backend.copyVersionPath(currentSelectedVersion); toastManager.show("已复制版本路径") }
                            }
                        }
                    }

                    // Mod-only: visible only for modded versions
                    Rectangle { width: 120; height: 32; radius: 6; color: shortcutHover3.hovered ? "#3a5ed0" : "#3a4a90"
                        visible: backend ? backend.isModdedVersion(currentSelectedVersion) : false
                        scale: shMouse3.pressed ? 0.92 : 1.0
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        Text { anchors.centerIn: parent; text: "Mod 文件夹"; font.pixelSize: 11; color: "#e8ecf8" }
                        HoverHandler { id: shortcutHover3 }
                        MouseArea { id: shMouse3; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: { if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }; if (backend) { if (backend.openModsFolder(currentSelectedVersion)) { toastManager.show("已打开 Mod 文件夹") } else { toastManager.show("无 Mod 文件夹") } } }
                        }
                    }
                    Rectangle { width: 120; height: 32; radius: 6; color: shortcutHover4.hovered ? "#3a5ed0" : "#3a4a90"
                        visible: backend ? backend.isModdedVersion(currentSelectedVersion) : false
                        scale: shMouse4.pressed ? 0.92 : 1.0
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        Text { anchors.centerIn: parent; text: "config 文件夹"; font.pixelSize: 11; color: "#e8ecf8" }
                        HoverHandler { id: shortcutHover4 }
                        MouseArea { id: shMouse4; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: { if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }; if (backend) { backend.openConfigFolder(); toastManager.show("已打开 config 文件夹") } }
                        }
                    }
                    Rectangle { width: 120; height: 32; radius: 6; color: shortcutHover5.hovered ? "#3a5ed0" : "#3a4a90"
                        visible: backend ? backend.isModdedVersion(currentSelectedVersion) : false
                        scale: shMouse5.pressed ? 0.92 : 1.0
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        Text { anchors.centerIn: parent; text: "光影包"; font.pixelSize: 11; color: "#e8ecf8" }
                        HoverHandler { id: shortcutHover5 }
                        MouseArea { id: shMouse5; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: { if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }; if (backend) { if (backend.openShaderPacksFolder(currentSelectedVersion)) { toastManager.show("已打开光影包文件夹") } else { toastManager.show("无光影包文件夹") } } }
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }

            // Section 1: 启动配置 ===
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 24; spacing: 12
                visible: settingsNav.currentIndex === 1

                Flickable {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    contentHeight: launchSettingsContent.implicitHeight
                    clip: true
                    ColumnLayout {
                        id: launchSettingsContent
                        width: parent.width; spacing: 14

                        // Java 环境
                        Text { text: "Java 环境"; font.pixelSize: 12; font.weight: Font.DemiBold; color: "#e4e8f2" }
                        // Recommended Java hint
                        Text {
                            text: {
                                if (!backend || !currentSelectedVersion) return ""
                                var vid = currentSelectedVersion
                                var parts = vid.split(".")
                                if (parts[0] === "1" && parts.length > 1) {
                                    var minor = parseInt(parts[1]) || 0
                                    if (minor >= 18) return "推荐: Java 17+"
                                    if (minor === 17) return "推荐: Java 16+"
                                    if (minor >= 12) return "推荐: Java 8"
                                    return "推荐: Java 8"
                                }
                                return ""
                            }
                            font.pixelSize: 10; color: "#4bc870"
                        }

                        Rectangle {
                            Layout.fillWidth: true; height: 36; radius: 6; color: "transparent"; border.color: "#1a1f2e"
                            Text { anchors.left: parent.left; anchors.leftMargin: 12; anchors.verticalCenter: parent.verticalCenter
                                text: backend ? (backend.javaPath || "未找到 Java 可执行文件") : "未找到 Java 可执行文件"
                                font.pixelSize: 12; color: "#b0b8c8"; elide: Text.ElideMiddle; width: parent.width - 90 }
                            Rectangle { anchors.right: parent.right; anchors.rightMargin: 6; anchors.verticalCenter: parent.verticalCenter
                                width: 90; height: 26; radius: 4; color: "#3a4eb8"
                                scale: autoDetectMa.pressed ? 0.9 : 1.0
                                Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                                Text { anchors.centerIn: parent; text: "自动检测"; font.pixelSize: 11; color: "#e8ecf8" }
                                MouseArea { id: autoDetectMa; anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { if (backend) { backend.detectJava(); toastManager.show("正在检测 Java 环境...") } } }
                            }
                            Rectangle { anchors.right: parent.right; anchors.rightMargin: 102; anchors.verticalCenter: parent.verticalCenter
                                width: 50; height: 26; radius: 4; color: "transparent"; border.color: "#1a1f2e"
                                scale: browseJavaMa.pressed ? 0.9 : 1.0
                                Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                                Text { anchors.centerIn: parent; text: "浏览"; font.pixelSize: 11; color: "#b0b8c8" }
                                MouseArea { id: browseJavaMa; anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { if (backend) { var ok = backend.pickJava(); if (ok) toastManager.show("已选择 Java 路径") } } }
                            }
                        }
                        Text {
                            text: {
                                if (!backend || !backend.javaVersion) return ""
                                var compat = backend.javaCompatibility
                                var base = backend.javaVersion
                                if (compat === "recommended") return base + "  推荐"
                                if (compat === "compatible") return base + "  兼容"
                                if (compat === "incompatible") return base + "  不兼容"

                                return base
                            }
                            font.pixelSize: 10
                            color: {
                                var compat = backend ? backend.javaCompatibility : "unknown"
                                if (compat === "recommended") return "#4bc870"
                                if (compat === "compatible") return "#e0a040"
                                if (compat === "incompatible") return "#c05050"
                                return "#888ca0"
                            }
                        }

                        // 鍐呭瓨设置
                        Text { text: "内存分配"; font.pixelSize: 12; font.weight: Font.DemiBold; color: "#e4e8f2"; Layout.topMargin: 8 }
                        RowLayout {
                            Layout.fillWidth: true; spacing: 12
                            ColumnLayout { Layout.fillWidth: true; spacing: 4
                                Text { text: "最大内存 " + (backend ? backend.maxMemoryMb + " MB" : "-"); font.pixelSize: 12; color: "#d4dcf0" }
                                Slider {
                                    id: maxMemSlider; Layout.fillWidth: true
                                    from: 512; to: 16384; stepSize: 512
                                    value: backend ? backend.maxMemoryMb : 2048
                                    enabled: backend ? !backend.autoMemoryEnabled : true
                                    onMoved: { if (backend) backend.setMaxMemory(value) }
                                }
                            }
                            Text { text: "最小" + (backend ? backend.minMemoryMb + " MB" : "-"); font.pixelSize: 11; color: "#989cb0" }
                        }
                        Text { text: backend ? (JSON.stringify(backend.systemMemoryInfo) || "") : ""; font.pixelSize: 9; color: "#a8b0c0" }

                        // 自动内存
                        RowLayout {
                            Text { text: "自动内存"; font.pixelSize: 12; color: "#d4dcf0" }
                            Item { Layout.fillWidth: true }
                            Switch {
                                checked: backend ? backend.autoMemoryEnabled : true
                                onToggled: { if (backend) backend.setAutoMemoryEnabled(checked) }
                            }
                        }

                        // JVM 参数
                        Text { text: "JVM 参数"; font.pixelSize: 12; font.weight: Font.DemiBold; color: "#e4e8f2"; Layout.topMargin: 8 }
                        Rectangle { Layout.fillWidth: true; height: 60; radius: 6; color: "#0d1018"; border.color: "#1a1f2e"
                            TextInput {
                                id: jvmArgsInput; anchors.fill: parent; anchors.margins: 8
                                color: "#b0b8c8"; font.pixelSize: 11; font.family: "Consolas, monospace"
                                text: backend ? backend.jvmArgs : ""
                                onEditingFinished: { if (backend) backend.setJvmArgs(text) }
                            }
                        }

                        // GC 预设
                        Text { text: "GC 预设"; font.pixelSize: 10; color: "#a8b0c0" }
                        Flow {
                            Layout.fillWidth: true; spacing: 8
                            Repeater {
                                model: [
                                    { label: "G1GC 平衡", args: "-XX:+UseG1GC -XX:G1NewSizePercent=20 -XX:MaxGCPauseMillis=50" },
                                    { label: "ZGC 低延迟", args: "-XX:+UseZGC -XX:+UnlockExperimentalVMOptions" },
                                    { label: "Shenandoah", args: "-XX:+UseShenandoahGC" },
                                    { label: "Parallel", args: "-XX:+UseParallelGC" },
                                    { label: "Serial", args: "-XX:+UseSerialGC" },
                                    { label: "清空自定", args: "" }
                                ]
                                Rectangle { width: 90; height: 26; radius: 4; color: presetHover.hovered ? "#1a2848" : "#0d1018"; border.color: "#1a1f2e"
                                    Text { anchors.centerIn: parent; text: modelData.label; font.pixelSize: 11; color: "#b0b8c8" }
                                    HoverHandler { id: presetHover }
                                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            jvmArgsInput.text = modelData.args
                                            if (backend) {
                                                console.log("[jvm] GC preset:", modelData.label, modelData.args)
                                                backend.setJvmArgs(modelData.args)
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // 版本隔离
                        Text { text: "版本隔离"; font.pixelSize: 12; font.weight: Font.DemiBold; color: "#e4e8f2"; Layout.topMargin: 8 }
                        RowLayout {
                            Text { text: "版本隔离（模组配置/存档独立）"; font.pixelSize: 12; color: "#d4dcf0" }
                            Item { Layout.fillWidth: true }
                            Switch {
                                checked: backend ? backend.isolationEnabled : false
                                onToggled: { if (backend) backend.setIsolationEnabled(checked) }
                            }
                        }

                        // 启动后关闭启动器
                        RowLayout {
                            Text { text: "启动后关闭启动器"; font.pixelSize: 12; color: "#d4dcf0" }
                            Item { Layout.fillWidth: true }
                            Switch {
                                checked: backend ? backend.closeOnLaunch : false
                                onToggled: { if (backend) backend.setCloseOnLaunch(checked) }
                            }
                        }

                        // 游戏附加参数
                        Text { text: "游戏附加参数"; font.pixelSize: 12; font.weight: Font.DemiBold; color: "#e4e8f2"; Layout.topMargin: 10 }
                        Text { text: "（示例 --width 1920 --height 1080 --fullscreen）"; font.pixelSize: 9; color: "#a8b0c0" }
                        Rectangle { Layout.fillWidth: true; height: 44; radius: 6; color: "#0d1018"; border.color: "#1a1f2e"
                            TextInput {
                                id: gameArgsInput; anchors.fill: parent; anchors.margins: 8
                                color: "#b0b8c8"; font.pixelSize: 11; font.family: "Consolas, monospace"
                                text: backend ? backend.gameArgs : ""
                                onEditingFinished: { if (backend) backend.setGameArgs(text) }
                            }
                        }
                    }
                }
            }

            // Section 2: Mod 管理 ===
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 24; spacing: 8
                visible: settingsNav.currentIndex === 2

                Text { text: "Mod 管理"; font.pixelSize: 14; font.bold: true; color: "#e4e8f2" }
                Text { text: "管理已安装的 Mod，查看版本和兼容性。"; font.pixelSize: 11; color: "#9498a8" }

                // Refresh + search
                RowLayout {
                    spacing: 8
                    Rectangle {
                        width: 80; height: 28; radius: 4; color: "#3a4eb8"
                        scale: modRefreshMa.pressed ? 0.9 : 1.0
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        Text { anchors.centerIn: parent; text: "刷新列表"; font.pixelSize: 11; color: "#e8ecf8" }
                        MouseArea { id: modRefreshMa; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: { modListModel.clear(); if (backend) { var m = backend.listMods(); for (var i = 0; i < m.length; i++) modListModel.append(m[i]); toastManager.show("Mod 列表已刷新") } }
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true; height: 28; radius: 4; color: "#0d1018"; border.color: modSearchField.activeFocus ? "#5068c8" : "#1a1f2e"
                        TextInput {
                            id: modSearchField; anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 10
                            color: "#e4e8f2"; font.pixelSize: 12; verticalAlignment: TextInput.AlignVCenter
                            onTextChanged: {
                                modListModel.clear()
                                if (backend) {
                                    var allMods = backend.listMods()
                                    var query = text.toLowerCase()
                                    for (var i = 0; i < allMods.length; i++) {
                                        if (!query || allMods[i].name.toLowerCase().indexOf(query) >= 0)
                                            modListModel.append(allMods[i])
                                    }
                                }
                            }
                            Text {
                                anchors.left: parent.left; anchors.leftMargin: 10
                                anchors.verticalCenter: parent.verticalCenter
                                text: "搜索 Mod..."; color: "#9ca0b4"; font.pixelSize: 12
                                visible: !modSearchField.text
                            }
                        }
                    }
                }

                // Mod list
                ListView {
                    id: modListView
                    Layout.fillWidth: true; Layout.fillHeight: true
                    model: ListModel { id: modListModel }
                    clip: true; spacing: 4
                    delegate: Rectangle {
                        width: modListView.width; height: 36; radius: 4; color: modRowHover.hovered ? "#11141c" : "transparent"
                        RowLayout {
                            anchors.fill: parent; anchors.margins: 10; spacing: 10
                            Text { text: name; font.pixelSize: 12; color: "#d4d8e8"; Layout.fillWidth: true; elide: Text.ElideRight }
                            Text { text: sizeDisplay; font.pixelSize: 10; color: "#9498a8" }
                            Rectangle { width: 60; height: 24; radius: 3; color: delBtnHover.hovered ? "#6b2020" : "transparent"; border.color: "#4a2828"
                                Text { anchors.centerIn: parent; text: "删除"; font.pixelSize: 10; color: "#c05050" }
                                MouseArea { id: delBtnHover; anchors.fill: parent; cursorShape: Qt.PointingHandCursor; hoverEnabled: true
                                    onClicked: { if (backend) { backend.deleteMod(name); modListModel.remove(index); toastManager.show("已删除 Mod: " + name) } }
                                }
                            }
                        }
                        MouseArea { id: modRowHover; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton }
                    }

                    Component.onCompleted: {
                        if (backend) { var m = backend.listMods(); for (var i = 0; i < m.length; i++) modListModel.append(m[i]) }
                    }
                }
            }

            // Section 3: 资源包管理 ===
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 24; spacing: 8
                visible: settingsNav.currentIndex === 3

                Text { text: "资源包管理"; font.pixelSize: 14; font.bold: true; color: "#e4e8f2" }
                Text { text: "管理已安装的资源包和材质包"; font.pixelSize: 11; color: "#9498a8" }

                RowLayout {
                    spacing: 8
                    Rectangle { height: 28; radius: 4; color: "#3a4eb8"; implicitWidth: rpRefreshText.implicitWidth + 20
                        scale: rpRefreshMa.pressed ? 0.9 : 1.0
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        Text { id: rpRefreshText; anchors.centerIn: parent; text: "刷新列表"; font.pixelSize: 11; color: "#e8ecf8" }
                        MouseArea { id: rpRefreshMa; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: { rpListModel.clear(); if (backend) { var p = backend.listResourcePacks(); for (var i = 0; i < p.length; i++) rpListModel.append(p[i]); toastManager.show("资源包列表已刷新") } }
                        }
                    }
                    Rectangle { height: 28; radius: 4; color: "transparent"; border.color: "#1a1f2e"; implicitWidth: rpOpenText.implicitWidth + 20
                        Text { id: rpOpenText; anchors.centerIn: parent; text: "打开文件夹"; font.pixelSize: 11; color: "#b0b8c8" }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { if (backend) { if (backend.openResourcePacksFolder(currentSelectedVersion)) { toastManager.show("已打开资源包文件夹") } else { toastManager.show("无资源包文件夹") } } } }
                    }
                }

                ListView {
                    id: rpListView
                    Layout.fillWidth: true; Layout.fillHeight: true
                    model: ListModel { id: rpListModel }
                    clip: true; spacing: 4
                    delegate: Rectangle {
                        width: rpListView.width; height: 36; radius: 4; color: rpRowHover.hovered ? "#11141c" : "transparent"
                        RowLayout {
                            anchors.fill: parent; anchors.margins: 10; spacing: 10
                            Rectangle { width: 20; height: 20; radius: 3; color: isDir ? "#5060a0" : "#3a6040"
                                Text { anchors.centerIn: parent; text: isDir ? "文件夹" : "文件"; font.pixelSize: 10 }
                            }
                            Text { text: name; font.pixelSize: 12; color: "#d4d8e8"; Layout.fillWidth: true; elide: Text.ElideRight }
                            Text { text: sizeDisplay; font.pixelSize: 10; color: "#9498a8" }
                            Rectangle { width: 60; height: 24; radius: 3; color: "transparent"; border.color: "#4a2828"
                                Text { anchors.centerIn: parent; text: "删除"; font.pixelSize: 10; color: "#c05050" }
                                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                    onClicked: { if (backend) { backend.deleteResourcePack(name); rpListModel.remove(index); toastManager.show("已删除资源包: " + name) } }
                                }
                            }
                        }
                        MouseArea { id: rpRowHover; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton }
                    }

                    Component.onCompleted: {
                        if (backend) { var p = backend.listResourcePacks(); for (var i = 0; i < p.length; i++) rpListModel.append(p[i]) }
                    }
                }
            }

            // Section 4: 存档管理 ===
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 24; spacing: 8
                visible: settingsNav.currentIndex === 4

                Text { text: "存档管理"; font.pixelSize: 14; font.bold: true; color: "#e4e8f2" }
                Text { text: "管理已保存的世界存档，可备份或迁移存档文件。"; font.pixelSize: 11; color: "#9498a8" }

                Rectangle {
                    width: 80; height: 28; radius: 4; color: "#3a4eb8"
                    scale: saveRefreshMa.pressed ? 0.9 : 1.0
                    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                    Text { anchors.centerIn: parent; text: "刷新列表"; font.pixelSize: 11; color: "#e8ecf8" }
                    MouseArea { id: saveRefreshMa; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: { saveListModel.clear(); if (backend) { var s = backend.listSaves(); for (var i = 0; i < s.length; i++) saveListModel.append(s[i]); toastManager.show("存档列表已刷新") } }
                    }
                }

                ListView {
                    id: saveListView
                    Layout.fillWidth: true; Layout.fillHeight: true
                    model: ListModel { id: saveListModel }
                    clip: true; spacing: 4
                    delegate: Rectangle {
                        width: saveListView.width; height: 36; radius: 4; color: saveRowHover.hovered ? "#11141c" : "transparent"
                        RowLayout {
                            anchors.fill: parent; anchors.margins: 10; spacing: 10
                            Text { text: name; font.pixelSize: 12; color: "#d4d8e8"; Layout.fillWidth: true; elide: Text.ElideRight }
                            Text { text: sizeDisplay; font.pixelSize: 10; color: "#9498a8" }
                            Rectangle { width: 60; height: 24; radius: 3; color: "transparent"; border.color: "#4a2828"
                                Text { anchors.centerIn: parent; text: "删除"; font.pixelSize: 10; color: "#c05050" }
                                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                    onClicked: { if (backend) { backend.deleteSave(name); saveListModel.remove(index); toastManager.show("已删除存档: " + name) } }
                                }
                            }
                        }
                        MouseArea { id: saveRowHover; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton }
                    }

                    Component.onCompleted: {
                        if (backend) { var s = backend.listSaves(); for (var i = 0; i < s.length; i++) saveListModel.append(s[i]) }
                    }
                }
            }

            // Section 5: 工具与维护
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 24; spacing: 12
                visible: settingsNav.currentIndex === 5

                Text { text: "游戏完整性校验"; font.pixelSize: 14; font.bold: true; color: "#e4e8f2" }
                Text { text: "扫描选定版本的游戏文件完整性，检查损坏或缺失的文件。"; font.pixelSize: 11; color: "#9498a8"; wrapMode: Text.WordWrap; Layout.fillWidth: true }

                // Start button
                Rectangle {
                    width: 140; height: 36; radius: 6
                    color: versionSettingsOverlay._verifyRunning ? "#2a3040" : (verifyBtnMouse.containsMouse ? "#2563EB" : "#3a4eb8")
                    scale: verifyBtnMouse.containsMouse && !versionSettingsOverlay._verifyRunning ? 1.04 : 1.0
                    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                    Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
                    Text { anchors.centerIn: parent; text: versionSettingsOverlay._verifyRunning ? "校验中..." : "开始校验"; font.pixelSize: 12; color: "#e8ecf8" }

                    MouseArea {
                        id: verifyBtnMouse; anchors.fill: parent; cursorShape: Qt.PointingHandCursor; hoverEnabled: true
                        enabled: !versionSettingsOverlay._verifyRunning
                        onClicked: { if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }; if (backend) backend.verifyVersion(currentSelectedVersion) }
                    }
                }

                // Progress bar
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 6
                    visible: versionSettingsOverlay._verifyRunning || versionSettingsOverlay._verifyProgressTotal > 0
                    Rectangle {
                        Layout.fillWidth: true; height: 8; radius: 4; color: "#1a1e28"
                        Rectangle {
                            height: 8; radius: 4
                            width: versionSettingsOverlay._verifyProgressTotal > 0 ? parent.width * (versionSettingsOverlay._verifyProgressDone / versionSettingsOverlay._verifyProgressTotal) : 0
                            color: versionSettingsOverlay._verifyRunning ? "#6080e8" : (versionSettingsOverlay._verifyProgressDone === versionSettingsOverlay._verifyProgressTotal ? "#4bc870" : "#c05050")
                            Behavior on width { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        }
                    }
                    Text {
                        text: {
                            if (versionSettingsOverlay._verifyRunning && versionSettingsOverlay._verifyProgressTotal > 0) {
                                var pct = Math.round(versionSettingsOverlay._verifyProgressDone / versionSettingsOverlay._verifyProgressTotal * 100)
                                return "校验中 " + versionSettingsOverlay._verifyProgressDone + "/" + versionSettingsOverlay._verifyProgressTotal + " (" + pct + "%)"
                            }
                            return versionSettingsOverlay._verifyRunning ? "校验中..." : ""
                        }
                        font.pixelSize: 11; color: "#989cb0"
                    }
                }

                Item { height: 12; width: 1 }

                // Red failure notification
                Rectangle {
                    Layout.fillWidth: true
                    height: 40; radius: 6
                    color: "#2A1518"
                    border.color: "#804040"
                    border.width: 1
                    visible: versionSettingsOverlay._verifyHasFailed && versionSettingsOverlay._verifyFailedFiles.length > 0
                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 8
                        Text {
                            text: "❌ 检测到 " + versionSettingsOverlay._verifyFailedFiles.length + " 个文件异常"
                            font.pixelSize: 12; color: "#ff8080"
                        }
                    }
                }

                // Action buttons
                RowLayout {
                    spacing: 10
                    visible: versionSettingsOverlay._verifyHasFailed && versionSettingsOverlay._verifyFailedFiles.length > 0

                    // Repair button (hollow orange)
                    Rectangle {
                        width: 140; height: 36; radius: 6
                        color: "transparent"
                        border.color: repairBtnHover.hovered ? "#ff8c42" : "#c06420"
                        border.width: 1.5
                        Text { anchors.centerIn: parent; text: "🔧 一键修复"; font.pixelSize: 12; color: repairBtnHover.hovered ? "#ff8c42" : "#e08050" }
                        HoverHandler { id: repairBtnHover }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }
                                if (backend) {
                                    backend.repairVersion(currentSelectedVersion)
                                    versionSettingsOverlay._verifyHasFailed = false
                                    versionSettingsOverlay._verifyFailedFiles = []
                                }
                            }
                        }
                    }

                    // View report button
                    Rectangle {
                        width: 140; height: 36; radius: 6
                        color: "transparent"
                        border.color: reportBtnHover.hovered ? "#ff8080" : "#804040"
                        border.width: 1.5
                        Text { anchors.centerIn: parent; text: "📋 查看异常详情"; font.pixelSize: 12; color: reportBtnHover.hovered ? "#ff8080" : "#e07070" }
                        HoverHandler { id: reportBtnHover }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (backend) backend.openVerifyReport()
                            }
                        }
                    }
                }

                Item { height: 12; width: 1 }

                // Version tools
                Text { text: "版本工具"; font.pixelSize: 10; color: "#9ca0b4"; font.letterSpacing: 1.5 }
                Flow {
                    Layout.fillWidth: true; spacing: 8

                    // Clone
                    Rectangle { width: 110; height: 32; radius: 5; color: cloneHover.hovered ? "#1a2848" : "#0d1018"; border.color: "#1a1f2e"
                        Text { anchors.centerIn: parent; text: "克隆版本"; font.pixelSize: 11; color: "#d4dcf0" }
                        HoverHandler { id: cloneHover }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }

                                if (backend) backend.cloneVersion(currentSelectedVersion)

                                toastManager.show("已克隆版本:  " + currentSelectedVersion) }


                        }
                    }

                    // Rename
                    Rectangle { width: 110; height: 32; radius: 5; color: renameHover.hovered ? "#1a2848" : "#0d1018"; border.color: "#1a1f2e"
                        Text { anchors.centerIn: parent; text: "重命名版本"; font.pixelSize: 11; color: "#d4dcf0" }
                        HoverHandler { id: renameHover }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }

                                if (backend) backend.renameVersion(currentSelectedVersion)

                            }
                        }
                    }

                    // Migrate
                    Rectangle { width: 110; height: 32; radius: 5; color: migrateHover.hovered ? "#1a2848" : "#0d1018"; border.color: "#1a1f2e"
                        Text { anchors.centerIn: parent; text: "迁移目录"; font.pixelSize: 11; color: "#d4dcf0" }
                        HoverHandler { id: migrateHover }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (!currentSelectedVersion) return
                                if (backend) backend.migrateVersion(currentSelectedVersion)
                            }
                        }
                    }
                }

                Item { height: 12; width: 1 }

                // Delete version
                Text { text: "危险操作"; font.pixelSize: 10; color: "#c05050"; font.letterSpacing: 1.5 }
                Rectangle {
                    Layout.fillWidth: true; height: 36; radius: 6; color: "transparent"; border.color: "#2a1f24"
                    scale: delVerHover.containsMouse ? 1.02 : 1.0
                    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                    Text { anchors.left: parent.left; anchors.leftMargin: 14; anchors.verticalCenter: parent.verticalCenter; text: "删除此版本"; font.pixelSize: 13; color: delVerHover.containsMouse ? "#f05050" : "#c05050" }
                    MouseArea {
                        id: delVerHover
                        anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }
                            confirmDialog.title = "删除版本"
                            confirmDialog.message = "确认要删除版本 " + currentSelectedVersion + " 吗？\n此操作不可撤销，版本的所有文件将被删除。"

                            confirmDialog.onAccept = function() {
                                backend.deleteVersion(currentSelectedVersion)
                                backend.refreshVersionDetails()
                                showVersionSettings = false
                            }
                            confirmDialog.visible = true
                        }
                    }
                }
            }
        }
    }
}
