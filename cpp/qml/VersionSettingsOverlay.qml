// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

// ========== VERSION SETTINGS OVERLAY ==========
Rectangle {
    id: versionSettingsOverlay
    anchors.fill: parent; color: hasBg ? "transparent" : StyleTokens.bgPrimary; z: 5
    property var backend: null
    property var toastManager: null
    property var confirmDialog: null
    // 当前侧边栏分区（0概览 1启动配置 2内存设置 3Mod管理 4资源包 5存档 6工具）；供外部（全局拖拽路由/截图测试）读写
    property alias currentNavIndex: settingsNav.currentIndex
    readonly property bool hasBg: backend && typeof backend.customBgPath === "string" && backend.customBgPath.length > 0

    // 供全局拖拽路由（MainWindow.packDropArea）在导入完成后刷新列表
    function refreshModsUi() { if (modSection) modSection.refreshModList() }
    function refreshRpsUi() { if (rpSection) rpSection.refreshRPList() }

    // Export progress state
    property bool isExporting: false
    property int exportPct: 0
    property string exportStatus: ""

    Connections {
        target: backend ? backend.userDataBackend : null
        function onExportProgress(pct, status) {
            isExporting = true
            exportPct = pct
            exportStatus = status
        }
        function onExportFinished(success, path, error) {
            isExporting = false
            exportPct = 0
            exportStatus = ""
            // 通用蓝色 toast（ToastManager），不用本地 _showToast
            if (toastManager) {
                if (success) {
                    toastManager.show("用户数据已导出")
                } else {
                    toastManager.show("导出失败: " + (error || "未知错误"), 5000)
                }
            }
        }
    }
    onVisibleChanged: {
        if (visible && backend) {
            // 版本切换时刷新所有数据列表（跟随版本隔离）；列表扫描在 worker 线程异步执行
            backend.refreshVersionDetails()
            backend.listModsAsync(currentSelectedVersion)
            backend.listResourcePacksAsync(currentSelectedVersion)
            backend.listSavesAsync(currentSelectedVersion)
            // 重置校验状态
            _verifyRunning = false
            _verifyProgressDone = 0
            _verifyProgressTotal = 0
            _verifyResultText = ""
            _verifyResultOk = false
        }
    }

    // ── 异步列表加载完成 → 回填各分区列表（worker 线程扫描，不阻塞 UI）──
    Connections {
        target: backend
        enabled: backend !== null
        function onModsListReady(versionId, mods) {
            if (versionId !== currentSelectedVersion) return
            modSection._allMods = mods || []
            modSection.totalModCount = modSection._allMods.length
            modSection.applyModFilter()
        }
        function onResourcePacksListReady(versionId, packs) {
            if (versionId !== currentSelectedVersion) return
            rpSection._allPacks = packs || []
            rpSection.applyRpFilter()
        }
        function onSavesListReady(versionId, saves) {
            if (versionId !== currentSelectedVersion) return
            saveListModel.clear()
            var s = saves || []
            for (var i = 0; i < s.length; i++) saveListModel.append(s[i])
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
                    ? ("✓ 校验完成: " + total + " 个文件全部通过")
                    : ("✗ 校验完成: " + total + " 个文件全部通过。")
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
        id: overlayContent
        anchors.fill: parent; anchors.margins: 16; spacing: 0

        // ── Delayed content entrance (after overlay background fades in) ──
        opacity: 0
        Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
        Component.onCompleted: contentEntranceTimer.start()
        Timer { id: contentEntranceTimer; interval: 80; onTriggered: overlayContent.opacity = 1 }

        // TOP BAR: version info + actions ===
        Rectangle {
            Layout.fillWidth: true; height: 56; radius: StyleTokens.radiusLg
            color: StyleTokens.bgSecondary; border.color: StyleTokens.bgInput
            RowLayout {
                anchors.fill: parent; anchors.margins: 14; spacing: 12

                // Back button
                BackButton {
                    onClicked: { showVersionSettings = false }
                }

                // Version label
                Text {
                    Layout.fillWidth: true
                    text: currentSelectedVersion || "未选择版本"
                    font.pixelSize: StyleTokens.fontSizeLg; font.weight: Font.Bold; color: StyleTokens.textSecondary
                }

                // Loader tag
                Rectangle {
                    visible: {
                        if (!backend || !backend.versionDetails) return false
                        for (var i = 0; i < backend.versionDetails.length; i++)
                            if (backend.versionDetails[i].id === currentSelectedVersion) return true
                        return false
                    }
                    height: 20; implicitWidth: settingsLoaderTag.implicitWidth + 10; radius: StyleTokens.radiusSm
                    color: {
                        if (!backend || !backend.versionDetails) return "#4a6a8a"
                        for (var i = 0; i < backend.versionDetails.length; i++) {
                            if (backend.versionDetails[i].id === currentSelectedVersion) {
                                var t = backend.versionDetails[i].loaderType
                                if (t === "Forge") return "#c05050"
                                if (t === "Fabric") return "#3a7a9a"
                                if (t === "NeoForge") return "#c08050"
                                if (t === "Quilt") return "#3a8a7a"
                                if (t === "LiteLoader") return "#7070a0"
                                if (t === "OptiFine") return "#8a8a5a"
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
                        font.pixelSize: StyleTokens.fontSizeXs; font.weight: Font.Medium; color: StyleTokens.textPrimary
                    }
                }

                // Spacer
                Item { Layout.fillWidth: true }

                // Launch button
                Rectangle {
                    id: topLaunchBtn
                    width: 100; height: 32; radius: StyleTokens.radiusMd
                    color: topLaunchHover.containsMouse ? (topLaunchHover.pressed ? "#2a3a90" : "#4a5ec8") : StyleTokens.accent
                    scale: topLaunchHover.containsMouse ? (topLaunchHover.pressed ? 0.93 : 1.05) : 1.0
                    Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
                    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                    Row { anchors.centerIn: parent; spacing: 6
                    Image { source: "icons/lucide/play.svg"; width: 16; height: 16; anchors.verticalCenter: parent.verticalCenter }
                    Text { text: qsTr("启动"); font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.Bold; color: StyleTokens.textPrimary }
                }
                    MouseArea { id: topLaunchHover; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (!backend) return
                            if (!currentSelectedVersion) {
                                toastManager.show("请先选择版本")
                                return
                            }
                            // Offline mode: use stored username or default
                            if (loginMode === 1) {
                                // 地区受限（非中国大陆且未正版登录）时禁止离线启动
                                if (backend.isOfflineRestricted()) {
                                    toastManager.show("进行正版登录前，离线登录无法使用，请先完成正版登录。", 4000)
                                    return
                                }
                                var name = backend.offlineUsername || "Player"
                                backend.offlineLogin(name)
                            }
                            // Premium mode: must be logged in
                            if (loginMode === 0 && !backend.username) {
                                toastManager.show("请先完成正版登录")
                                return
                            }
                            // Yggdrasil mode: must be logged in
                            if (loginMode === 2 && !backend.yggdrasil.loggedIn) {
                                toastManager.show("请先完成外置登录")
                                return
                            }
                            backend.launch(currentSelectedVersion, loginMode === 0 || loginMode === 2)
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
                { text: qsTr("概览"), icon: "" },
                { text: qsTr("启动配置"), icon: "" },
                { text: qsTr("内存设置"), icon: "" },
                { text: qsTr("Mod 管理"), icon: "" },
                { text: qsTr("资源包管理"), icon: "" },
                { text: qsTr("存档管理"), icon: "" },
                { text: qsTr("工具与维护"), icon: "" },
                { text: qsTr("导出整合包"), icon: "" }
            ]

            // Check if current version has a mod loader（白名单判定，与版本选择 getBlockIcon 一致）
            function isModdedVersion() {
                if (!backend || !backend.versionDetails || !currentSelectedVersion) return false
                for (var i = 0; i < backend.versionDetails.length; i++) {
                    if (backend.versionDetails[i].id === currentSelectedVersion) {
                        var lt = backend.versionDetails[i].loaderType || ""
                        return (lt === "Forge" || lt === "Fabric" || lt === "NeoForge" || lt === "Quilt")
                    }
                }
                return false
            }
            ListView {
                id: settingsNav
                anchors.fill: parent
                model: parent.sectionModel
                delegate: Rectangle {
                    width: settingsNav.width
                    height: {
                        if (modelData.text !== qsTr("Mod 管理")) return 36
                        if (!backend || !backend.versionDetails || !currentSelectedVersion) return 0
                        for (var i = 0; i < backend.versionDetails.length; i++) {
                            if (backend.versionDetails[i].id === currentSelectedVersion) {
                                var lt = backend.versionDetails[i].loaderType || ""
                                return (lt === "Forge" || lt === "Fabric" || lt === "NeoForge" || lt === "Quilt") ? 36 : 0
                            }
                        }
                        return 0
                    }
                    radius: StyleTokens.radiusMd
                    visible: {
                        if (modelData.text !== qsTr("Mod 管理")) return true
                        if (!backend || !backend.versionDetails || !currentSelectedVersion) return false
                        for (var i = 0; i < backend.versionDetails.length; i++) {
                            if (backend.versionDetails[i].id === currentSelectedVersion) {
                                var lt = backend.versionDetails[i].loaderType || ""
                                return lt === "Forge" || lt === "Fabric" || lt === "NeoForge" || lt === "Quilt"
                            }
                        }
                        return false
                    }
                    color: ListView.isCurrentItem ? "#162040" : (mouseArea2.containsMouse ? StyleTokens.bgSecondary : "transparent")
                    scale: mouseArea2.containsMouse ? 1.03 : 1.0
                    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                    Rectangle { anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 3; color: ListView.isCurrentItem ? "#5080e8" : "transparent" }
                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 16; spacing: 10
                        Text {
                            text: modelData.text
                            font.pixelSize: StyleTokens.fontSizeMd
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
            color: StyleTokens.bgSecondary; radius: StyleTokens.radiusLg; border.color: StyleTokens.bgInput

            // Section 0: 概览 ===
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 24; spacing: 12
                opacity: settingsNav.currentIndex === 0 ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

                // ── 快捷入口（2026-08-03 分类重做：统一 ShadowButton 组件 + 分组排序）──
                Text { text: qsTr("快捷入口"); font.pixelSize: StyleTokens.fontSizeXs; color: "#9ca0b4"; font.letterSpacing: 1.5 }

                // ── 文件夹 ──
                Text { text: qsTr("文件夹"); font.pixelSize: StyleTokens.fontSizeXs; color: "#6a7088"; font.letterSpacing: 1.2 }
                Flow {
                    Layout.fillWidth: true; spacing: 8

                    ShadowButton {
                        Layout.preferredWidth: 130; Layout.preferredHeight: 32
                        text: qsTr("版本文件夹"); iconSource: "icons/lucide/folder.svg"; iconSize: 14
                        accentColor: "#2a4590"
                        font.pixelSize: StyleTokens.fontSizeSm
                        onClicked: {
                            if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }
                            if (backend) {
                                if (backend.openVersionDir(currentSelectedVersion)) { toastManager.show("已打开版本文件夹") }
                                else { toastManager.show("版本文件夹不存在") }
                            }
                        }
                    }
                    ShadowButton {
                        Layout.preferredWidth: 130; Layout.preferredHeight: 32
                        text: qsTr("存档文件夹"); iconSource: "icons/lucide/map.svg"; iconSize: 14
                        accentColor: "#2a4590"
                        font.pixelSize: StyleTokens.fontSizeSm
                        onClicked: {
                            if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }
                            if (backend) { if (backend.openSavesFolder(currentSelectedVersion)) { toastManager.show("已打开存档文件夹") } else { toastManager.show("版本不存在或文件夹未创建") } }
                        }
                    }
                    ShadowButton {
                        Layout.preferredWidth: 130; Layout.preferredHeight: 32
                        text: qsTr("截图文件夹"); iconSource: "icons/lucide/camera.svg"; iconSize: 14
                        accentColor: "#2a4590"
                        font.pixelSize: StyleTokens.fontSizeSm
                        onClicked: {
                            if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }
                            if (backend) { if (backend.openScreenshotsFolder(currentSelectedVersion)) { toastManager.show("已打开截图文件夹") } else { toastManager.show("版本不存在或文件夹未创建") } }
                        }
                    }
                    ShadowButton {
                        Layout.preferredWidth: 130; Layout.preferredHeight: 32
                        text: qsTr("Mod 文件夹"); iconSource: "icons/lucide/puzzle.svg"; iconSize: 14
                        accentColor: "#3a4a90"
                        // 白名单判定（与 sidebar“Mod 管理”同款内联写法）：lt ∈ Forge/Fabric/NeoForge/Quilt 才算加载器版
                        visible: {
                            if (!backend || !backend.versionDetails || !currentSelectedVersion) return false
                            for (var i = 0; i < backend.versionDetails.length; i++) {
                                if (backend.versionDetails[i].id === currentSelectedVersion) {
                                    var lt = backend.versionDetails[i].loaderType || ""
                                    return lt === "Forge" || lt === "Fabric" || lt === "NeoForge" || lt === "Quilt"
                                }
                            }
                            return false
                        }
                        font.pixelSize: StyleTokens.fontSizeSm
                        onClicked: {
                            if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }
                            if (backend) { if (backend.openModsFolder(currentSelectedVersion)) { toastManager.show("已打开 Mod 文件夹") } else { toastManager.show("版本不存在或文件夹未创建") } }
                        }
                    }
                }

                Item { Layout.preferredHeight: 8 }

                // ── 日志 ──
                Text { text: qsTr("日志"); font.pixelSize: StyleTokens.fontSizeXs; color: "#6a7088"; font.letterSpacing: 1.2 }
                Flow {
                    Layout.fillWidth: true; spacing: 8

                    ShadowButton {
                        Layout.preferredWidth: 130; Layout.preferredHeight: 32
                        text: qsTr("logs 日志"); iconSource: "icons/lucide/file-text.svg"; iconSize: 14
                        accentColor: "#2a4590"
                        font.pixelSize: StyleTokens.fontSizeSm
                        onClicked: {
                            if (backend) { if (backend.openLogsFolder(currentSelectedVersion)) { toastManager.show("已打开日志文件夹") } else { toastManager.show("版本不存在或文件夹未创建") } }
                        }
                    }
                    ShadowButton {
                        Layout.preferredWidth: 130; Layout.preferredHeight: 32
                        text: qsTr("最新启动日志"); iconSource: "icons/lucide/file.svg"; iconSize: 14
                        accentColor: "#2a4590"
                        font.pixelSize: StyleTokens.fontSizeSm
                        onClicked: {
                            if (backend) { if (backend.openLatestLog(currentSelectedVersion)) { toastManager.show("已打开最新日志") } else { toastManager.show("无日志文件") } }
                        }
                    }
                    ShadowButton {
                        Layout.preferredWidth: 130; Layout.preferredHeight: 32
                        text: qsTr("崩溃日志"); iconSource: "icons/lucide/alert-octagon.svg"; iconSize: 14
                        accentColor: "#9a3838"
                        font.pixelSize: StyleTokens.fontSizeSm
                        onClicked: {
                            if (backend) { if (backend.openCrashLog(currentSelectedVersion)) { toastManager.show("已打开崩溃日志") } else { toastManager.show("无崩溃报告") } }
                        }
                    }
                }

                Item { Layout.preferredHeight: 8 }

                // ── 其他 ──
                Text { text: qsTr("其他"); font.pixelSize: StyleTokens.fontSizeXs; color: "#6a7088"; font.letterSpacing: 1.2 }
                Flow {
                    Layout.fillWidth: true; spacing: 8

                    ShadowButton {
                        Layout.preferredWidth: 130; Layout.preferredHeight: 32
                        text: qsTr("复制版本路径"); iconSource: "icons/lucide/clipboard-copy.svg"; iconSize: 14
                        accentColor: "#2a4590"
                        font.pixelSize: StyleTokens.fontSizeSm
                        onClicked: {
                            if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }
                            if (backend) { backend.copyVersionPath(currentSelectedVersion); toastManager.show("已复制版本路径") }
                        }
                    }

                    ShadowButton {
                        Layout.preferredWidth: 130; Layout.preferredHeight: 32
                        text: qsTr("导出启动脚本"); iconSource: "icons/lucide/terminal.svg"; iconSize: 14
                        accentColor: "#2a5a40"
                        font.pixelSize: StyleTokens.fontSizeSm
                        onClicked: {
                            if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }
                            if (!backend) return
                            var jp = backend.javaPath || ""
                            if (!jp) { toastManager.show("未设置 Java 路径"); return }
                            launchScriptDialog.versionId = currentSelectedVersion
                            launchScriptDialog.javaPath = jp
                            launchScriptDialog.maxMemoryMb = backend.maxMemoryMb || 2048
                            launchScriptDialog.jvmArgs = backend.jvmArgs || ""
                            launchScriptDialog.gameArgs = backend.gameArgs || ""
                            launchScriptDialog.highPerfGpu = backend.highPerfGpu || false
                            launchScriptDialog.open()
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }

            // Section 1: 启动配置 ===
            Item {
                anchors.fill: parent
                opacity: settingsNav.currentIndex === 1 ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                Loader {
                    id: launchSettingsLoader
                    anchors.fill: parent
                    source: "VersionLaunchSection.qml"
                    asynchronous: false
                    active: settingsNav.currentIndex === 1
                    onLoaded: {
                        item.backend = backend
                        item.toastManager = toastManager
                        item.currentSelectedVersion = Qt.binding(function() { return currentSelectedVersion })
                        item.refreshAll()
                    }
                }
            }

            // Section 2: 内存设置 ===
            Item {
                anchors.fill: parent
                opacity: settingsNav.currentIndex === 2 ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                Loader {
                    id: memSettingsLoader
                    anchors.fill: parent
                    source: "VersionMemorySection.qml"
                    asynchronous: false
                    active: settingsNav.currentIndex === 2
                    onLoaded: {
                        item.backend = backend
                        item.currentSelectedVersion = Qt.binding(function() { return currentSelectedVersion })
                        item.refreshAll()
                    }
                }
            }


            // Section 3: Mod 管理 ===
            Item {
                id: modSection
                anchors.fill: parent; anchors.margins: 24
                opacity: settingsNav.currentIndex === 3 ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

                // 异步列表缓存：_allMods=全量，totalModCount=未过滤总数（标题“共？个模组”）
                property var _allMods: []
                property int totalModCount: 0

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10

                    // Header
                    RowLayout {
                        Text { text: qsTr("Mod 管理（共 %1 个模组）").arg(modSection.totalModCount); font.pixelSize: StyleTokens.fontSizeLg; font.bold: true; color: StyleTokens.textSecondary }
                        Item { Layout.fillWidth: true }

                        // Open folder button
                        Rectangle {
                            width: 30; height: 30; radius: StyleTokens.radiusMd; color: modFolderBtnH.hovered ? "#222a3a" : "#141820"
                            border.color: StyleTokens.bgHover
                            scale: modFolderBtnM.pressed ? 0.88 : 1.0
                            Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                            Behavior on color { ColorAnimation { duration: 200 } }
                            Image {
                                anchors.centerIn: parent
                                source: "icons/lucide/folder-open.svg"
                                width: 14; height: 14
                            }
                            MouseArea {
                                id: modFolderBtnM; anchors.fill: parent; cursorShape: Qt.PointingHandCursor; hoverEnabled: true
                                onClicked: {
                                    if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }
                                    if (backend) { if (backend.openModsFolder(currentSelectedVersion)) { /* no-op */ } else { toastManager.show("版本不存在或文件夹未创建") } }
                                }
                            }
                            HoverHandler { id: modFolderBtnH }
                        }

                        // Refresh button
                        RefreshButton {
                            onClicked: { modSection.refreshModList(); toastManager.show("Mod 列表已刷新") }
                        }
                    }

                    Text { text: qsTr("管理已安装的 Mod，拖拽 JAR 文件到此区域快捷导入。内置支持 Fabric/NeoForge/Forge 元数据识别。\n"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary; wrapMode: Text.WordWrap; Layout.fillWidth: true }

                    // Search
                    SearchBox {
                        id: modSearchField
                        Layout.fillWidth: true
                        showIcon: true
                        placeholderText: qsTr("搜索 Mod 名称...")
                        onTextChanged: modSection.applyModFilter()
                    }

                    // Grid of mod cards
                    ScrollView {
                        id: modScroll
                        Layout.fillWidth: true; Layout.fillHeight: true
                        clip: true
                        ScrollBar.vertical.policy: ScrollBar.AsNeeded

                        GridView {
                            id: modGrid
                            model: ListModel { id: modListModel }
                            // 固定每行两个卡片，卡片铺满半行宽（消除列数随窗口宽度变化 + 中间大空隙）
                            cellWidth: (modScroll.width - 16) / 2
                            cellHeight: 136
                            clip: true

                            delegate: Rectangle {
                                id: card
                                width: modGrid.cellWidth - 12
                                height: 128
                                radius: StyleTokens.radiusLg
                                color: cardHover.hovered ? "#121620" : "#0e1018"
                                border { width: 1; color: cardHover.hovered ? StyleTokens.accent : "#1e2430" }

                                Behavior on color { ColorAnimation { duration: 150 } }
                                Behavior on border.color { ColorAnimation { duration: 150 } }

                                Rectangle {
                                    anchors.fill: parent; radius: StyleTokens.radiusLg
                                    color: StyleTokens.accentLight
                                    opacity: cardHover.hovered ? 0.06 : 0.0
                                    Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                                }

                                opacity: model.enabled === false ? 0.55 : 0
                                Component.onCompleted: opacity = model.enabled === false ? 0.55 : 1
                                Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

                                RowLayout {
                                    anchors { fill: parent; margins: 10 }
                                    spacing: 10

                                    // Icon
                                    Rectangle {
                                        width: 48; height: 48; radius: StyleTokens.radiusMd; color: StyleTokens.surfaceOverlay
                                        Layout.alignment: Qt.AlignTop
                                        Image {
                                            anchors { fill: parent; margins: 4 }
                                            source: model.iconPath || "icons/lucide/package.svg"
                                            fillMode: Image.PreserveAspectFit
                                            sourceSize.width: 40; sourceSize.height: 40
                                            visible: true
                                            onStatusChanged: {
                                                if (status === Image.Error) source = "icons/lucide/package.svg"
                                            }
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true; spacing: 2

                                        // Name
                                        Text {
                                            text: (model.enabled === false ? (model.modName || model.fileName) + "（已禁用）" : (model.modName || model.fileName))
                                            font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.Medium; color: StyleTokens.textSecondary
                                            elide: Text.ElideRight; Layout.fillWidth: true
                                        }

                                        // Version + loader badge
                                        RowLayout {
                                            spacing: 4
                                            Text {
                                                text: "v" + (model.version || "?")
                                                font.pixelSize: StyleTokens.fontSizeXs; color: "#6ab04c"
                                            }
                                            Rectangle {
                                                visible: model.loader && model.loader !== "unknown"
                                                width: loaderText.implicitWidth + 10; height: 16; radius: StyleTokens.radiusXs
                                                property string _l: (model.loader || "").toLowerCase()
                                                color: _l === "forge" ? "#c05050" : (_l === "fabric" ? "#3a7a9a" : (_l === "neoforge" ? "#c08050" : (_l === "quilt" ? "#3a8a7a" : (_l === "liteloader" ? "#7070a0" : (_l === "optifine" ? "#8a8a5a" : "#4a6a8a")))))
                                                Text {
                                                    id: loaderText
                                                    anchors.centerIn: parent
                                                    property string _raw: model.loader || ""
                                                    text: _raw ? _raw.charAt(0).toUpperCase() + _raw.slice(1) : ""
                                                    font.pixelSize: StyleTokens.fontSizeXs
                                                    color: StyleTokens.textPrimary
                                                }
                                            }
                                        }

                                        // Description (2 lines)
                                        Text {
                                            text: model.description || ""
                                            font.pixelSize: StyleTokens.fontSizeXs; color: "#7880a0"
                                            elide: Text.ElideRight; maximumLineCount: 2; wrapMode: Text.WordWrap
                                            Layout.fillWidth: true; Layout.preferredHeight: 28
                                            visible: text !== ""
                                        }

                                        // Bottom row: file size + delete
                                        RowLayout {
                                            Layout.fillWidth: true
                                            Text {
                                                text: model.fileSizeText || ""
                                                font.pixelSize: StyleTokens.fontSizeXs; color: "#586080"
                                            }
                                            Item { Layout.fillWidth: true }

                                            // Toggle enable/disable (rename *.jar ↔ *.jar.disabled, 2026-08-07)
                                            Rectangle {
                                                width: 44; height: 22; radius: StyleTokens.radiusSm
                                                color: toggleBtnH.hovered ? "#2a3550" : "#1a2130"
                                                border.color: toggleBtnH.hovered ? StyleTokens.accent : "#2a3450"
                                                border.width: 1
                                                opacity: cardHover.hovered ? 1.0 : 0.0
                                                Behavior on opacity { NumberAnimation { duration: 200 } }
                                                Behavior on color { ColorAnimation { duration: 150 } }
                                                Behavior on border.color { ColorAnimation { duration: 150 } }
                                                scale: toggleBtnM.pressed ? 0.9 : 1.0
                                                Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                                                Text {
                                                    anchors.centerIn: parent
                                                    text: model.enabled === false ? qsTr("启用") : qsTr("禁用")
                                                    font.pixelSize: StyleTokens.fontSizeXs
                                                    color: model.enabled === false ? "#6ab04c" : "#a0a8c0"
                                                }
                                                MouseArea {
                                                    id: toggleBtnM; anchors.fill: parent; cursorShape: Qt.PointingHandCursor; hoverEnabled: true
                                                    onClicked: {
                                                        if (backend && currentSelectedVersion) {
                                                            var fn = model.fileName || ""
                                                            var en = model.enabled !== false
                                                            backend.setModEnabled(fn, currentSelectedVersion, !en)
                                                            // setProperty 只更新 enabled role（保留其他字段）→ 按钮文字/卡片样式即时刷新，无重建闪烁
                                                            modListModel.setProperty(index, "enabled", !en)
                                                            toastManager.show(en ? "已禁用: " + fn : "已启用: " + fn)
                                                        }
                                                    }
                                                }
                                                HoverHandler { id: toggleBtnH }
                                            }

                                            // Delete button (visible on hover)
                                            ShadowIconButton {
                                                width: 24; height: 24; icon: "\u2715"; type: "close"
                                                opacity: cardHover.hovered ? 1.0 : 0.0
                                                Behavior on opacity { NumberAnimation { duration: 200 } }
                                                onClicked: {
                                                    if (backend) {
                                                        var fn = model.fileName || ""
                                                        backend.deleteMod(fn, currentSelectedVersion)
                                                        modListModel.remove(index)
                                                        toastManager.show("已删除: " + fn)
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                HoverHandler { id: cardHover }
                            }

                            Component.onCompleted: { modSection.refreshModList() }
                        }
                    }
                }

                function refreshModList() {
                    modListModel.clear()
                    if (backend) backend.listModsAsync(currentSelectedVersion)
                }

                function applyModFilter() {
                    modListModel.clear()
                    var query = modSearchField.text.toLowerCase()
                    for (var i = 0; i < _allMods.length; i++) {
                        var name = (_allMods[i].modName || _allMods[i].fileName || "").toLowerCase()
                        if (!query || name.indexOf(query) >= 0)
                            modListModel.append(_allMods[i])
                    }
                }
            }

            // 拖入导入由 MainWindow 全局 DropArea（packDropArea）路由处理：
            // 本分区激活时拖入 .jar → importMod，完成后调用本 overlay 的 refreshModsUi() 刷新。
            // （Qt DnD 事件只投递给光标下最顶层 item，分区内 DropArea 会被全局 DropArea 拦截，故统一走全局路由）

            // Section 4: 资源包管理 ===
            Item {
                id: rpSection
                anchors.fill: parent; anchors.margins: 24
                opacity: settingsNav.currentIndex === 4 ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

                // 异步列表缓存（全量，过滤在 applyRpFilter 内进行）
                property var _allPacks: []

                onVisibleChanged: {
                    if (visible) refreshRPList()
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10

                    // Header
                    RowLayout {
                        Text { text: qsTr("资源包管理"); font.pixelSize: StyleTokens.fontSizeLg; font.bold: true; color: StyleTokens.textSecondary }
                        Item { Layout.fillWidth: true }

                        // Open folder button
                        Rectangle {
                            width: 30; height: 30; radius: StyleTokens.radiusMd; color: rpFolderBtnH.hovered ? "#222a3a" : "#141820"
                            border.color: StyleTokens.bgHover
                            scale: rpFolderBtnM.pressed ? 0.88 : 1.0
                            Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                            Behavior on color { ColorAnimation { duration: 200 } }
                            Image {
                                anchors.centerIn: parent
                                source: "icons/lucide/folder-open.svg"
                                width: 14; height: 14
                            }
                            MouseArea {
                                id: rpFolderBtnM; anchors.fill: parent; cursorShape: Qt.PointingHandCursor; hoverEnabled: true
                                onClicked: {
                                    if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }
                                    if (backend) { if (backend.openResourcePacksFolder(currentSelectedVersion)) { /* no-op */ } else { toastManager.show("版本不存在或文件夹未创建") } }
                                }
                            }
                            HoverHandler { id: rpFolderBtnH }
                        }

                        // Refresh button
                        RefreshButton {
                            onClicked: { rpSection.refreshRPList(); toastManager.show("资源包列表已刷新") }
                        }
                    }

                    Text { text: qsTr("管理已安装的资源包和材质包。支持 pack.mcmeta 元数据解析和 pack.png 图标提取。\n"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary; wrapMode: Text.WordWrap; Layout.fillWidth: true }

                    // Search
                    SearchBox {
                        id: rpSearchField
                        Layout.fillWidth: true
                        showIcon: true
                        placeholderText: qsTr("搜索资源包名称...")
                        onTextChanged: rpSection.applyRpFilter()
                    }

                    // Grid of resource pack cards
                    ScrollView {
                        id: rpScroll
                        Layout.fillWidth: true; Layout.fillHeight: true
                        clip: true
                        ScrollBar.vertical.policy: ScrollBar.AsNeeded

                        GridView {
                            id: rpGrid
                            model: ListModel { id: rpListModel }
                            // 固定每行两个卡片（与 Mod 管理一致）
                            cellWidth: (rpScroll.width - 16) / 2
                            cellHeight: 136
                            clip: true

                            delegate: Rectangle {
                                id: rpCard
                                width: rpGrid.cellWidth - 12
                                height: 128
                                radius: StyleTokens.radiusLg
                                color: rpCardHover.hovered ? "#121620" : "#0e1018"
                                border { width: 1; color: rpCardHover.hovered ? StyleTokens.accent : "#1e2430" }

                                Behavior on color { ColorAnimation { duration: 150 } }
                                Behavior on border.color { ColorAnimation { duration: 150 } }

                                Rectangle {
                                    anchors.fill: parent; radius: StyleTokens.radiusLg
                                    color: StyleTokens.accentLight
                                    opacity: rpCardHover.hovered ? 0.06 : 0.0
                                    Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                                }

                                opacity: 0
                                Component.onCompleted: opacity = 1
                                Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

                                RowLayout {
                                    anchors { fill: parent; margins: 10 }
                                    spacing: 10

                                    // Icon
                                    Rectangle {
                                        width: 48; height: 48; radius: StyleTokens.radiusMd; color: StyleTokens.surfaceOverlay
                                        Layout.alignment: Qt.AlignTop
                                        Image {
                                            anchors { fill: parent; margins: 4 }
                                            source: model.iconPath || "icons/lucide/palette.svg"
                                            fillMode: Image.PreserveAspectFit
                                            sourceSize.width: 40; sourceSize.height: 40
                                            visible: true
                                            onStatusChanged: {
                                                if (status === Image.Error) source = "icons/lucide/palette.svg"
                                            }
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true; spacing: 2

                                        // Name
                                        Text {
                                            text: model.name || model.fileName
                                            font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.Medium; color: StyleTokens.textSecondary
                                            elide: Text.ElideRight; Layout.fillWidth: true
                                        }

                                        // Version text (green, like Mod page v0.6.10)
                                        Text {
                                            text: model.versionText || ""
                                            font.pixelSize: StyleTokens.fontSizeXs; color: "#6ab04c"
                                            Layout.fillWidth: true; elide: Text.ElideRight
                                            visible: text !== ""
                                        }

                                        // Author
                                        Text {
                                            text: model.authorText || ""
                                            font.pixelSize: StyleTokens.fontSizeXs; color: "#7880a0"
                                            elide: Text.ElideRight; maximumLineCount: 2; wrapMode: Text.WordWrap
                                            Layout.fillWidth: true; Layout.preferredHeight: 28
                                            visible: text !== ""
                                        }

                                        // Bottom row: file size + delete
                                        RowLayout {
                                            Layout.fillWidth: true
                                            Text {
                                                text: model.fileSizeText || ""
                                                font.pixelSize: StyleTokens.fontSizeXs; color: "#586080"
                                            }
                                            Item { Layout.fillWidth: true }

                                            // Delete button (visible on hover)
                                            ShadowIconButton {
                                                width: 24; height: 24; icon: "\u2715"; type: "close"
                                                opacity: rpCardHover.hovered ? 1.0 : 0.0
                                                Behavior on opacity { NumberAnimation { duration: 200 } }
                                                onClicked: {
                                                    if (backend) {
                                                        var fn = model.fileName || ""
                                                        backend.deleteResourcePack(fn, currentSelectedVersion)
                                                        rpListModel.remove(index)
                                                        toastManager.show("已删除: " + fn)
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                HoverHandler { id: rpCardHover }
                            }

                        }
                    }
                }

                function refreshRPList() {
                    rpListModel.clear()
                    if (backend) backend.listResourcePacksAsync(currentSelectedVersion)
                }

                function applyRpFilter() {
                    rpListModel.clear()
                    var query = rpSearchField.text.toLowerCase()
                    for (var i = 0; i < _allPacks.length; i++) {
                        var name = (_allPacks[i].name || _allPacks[i].fileName || "").toLowerCase()
                        if (!query || name.indexOf(query) >= 0)
                            rpListModel.append(_allPacks[i])
                    }
                }
            }

            // Section 5: 存档管理 ===
            ColumnLayout {
                id: saveSection
                anchors.fill: parent; anchors.margins: 24; spacing: 8
                opacity: settingsNav.currentIndex === 5 ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

                onVisibleChanged: {
                    if (visible) {
                        saveListModel.clear()
                        if (backend) backend.listSavesAsync(currentSelectedVersion)
                    }
                }

                // Header
                RowLayout {
                    Text { text: qsTr("存档管理"); font.pixelSize: StyleTokens.fontSizeLg; font.bold: true; color: StyleTokens.textSecondary }
                    Item { Layout.fillWidth: true }

                    // Open folder button
                    Rectangle {
                        width: 30; height: 30; radius: StyleTokens.radiusMd; color: saveFolderBtnH.hovered ? "#222a3a" : "#141820"
                        border.color: StyleTokens.bgHover
                        scale: saveFolderBtnM.pressed ? 0.88 : 1.0
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        Behavior on color { ColorAnimation { duration: 200 } }
                        Image {
                            anchors.centerIn: parent
                            source: "icons/lucide/folder-open.svg"
                            width: 14; height: 14
                        }
                        MouseArea {
                            id: saveFolderBtnM; anchors.fill: parent; cursorShape: Qt.PointingHandCursor; hoverEnabled: true
                            onClicked: {
                                if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }
                                if (backend) { if (backend.openSavesFolder(currentSelectedVersion)) { /* no-op */ } else { toastManager.show("版本不存在或文件夹未创建") } }
                            }
                        }
                        HoverHandler { id: saveFolderBtnH }
                    }

                    // Refresh button
                    RefreshButton {
                        onClicked: {
                            saveListModel.clear()
                            if (backend) backend.listSavesAsync(currentSelectedVersion)
                            toastManager.show("存档列表已刷新")
                        }
                    }
                }

                Text { text: qsTr("管理已保存的世界存档，可备份或迁移存档文件。"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary }

                ListView {
                    id: saveListView
                    Layout.fillWidth: true; Layout.fillHeight: true
                    model: ListModel { id: saveListModel }
                    clip: true; spacing: 4
                    delegate: Rectangle {
                        id: saveRow
                        width: saveListView.width; height: 36; radius: StyleTokens.radiusSm; color: saveRowHover.hovered ? StyleTokens.bgSecondary : "transparent"

                        // ── Staggered entrance animation ──
                        property int _entranceDelay: index * 50
                        opacity: 0
                        Component.onCompleted: saveEntranceTimer.start()
                        Timer {
                            id: saveEntranceTimer
                            interval: saveRow._entranceDelay
                            onTriggered: saveRow.opacity = 1
                        }
                        Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
                        RowLayout {
                            anchors.fill: parent; anchors.margins: 10; spacing: 10
                            Text { text: name; font.pixelSize: StyleTokens.fontSizeSm; color: "#d4d8e8"; Layout.fillWidth: true; elide: Text.ElideRight }
                            Text { text: sizeDisplay; font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.textTertiary }
                            Rectangle { width: 60; height: 24; radius: StyleTokens.radiusXs; color: "transparent"; border.color: "#4a2828"
                                Row { anchors.centerIn: parent; spacing: 3
                                Image { source: "icons/lucide/trash-2.svg"; width: 12; height: 12; anchors.verticalCenter: parent.verticalCenter }
                                Text { text: qsTr("删除"); font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.textDanger }
                            }
                                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                    onClicked: { if (backend) { _showConfirm("确认删除", "删除存档 \"" + name + "\"？\n此操作不可撤销。", function() { backend.deleteSave(name, currentSelectedVersion); saveListModel.remove(index); toastManager.show("已删除存档: " + name) }) } }
                                }
                            }
                        }
                        MouseArea { id: saveRowHover; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton }
                    }

                    Component.onCompleted: {
                        if (backend) backend.listSavesAsync(currentSelectedVersion)
                    }
                }
            }

            // Section 6: 工具与维护
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 24; spacing: 12
                opacity: settingsNav.currentIndex === 6 ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

                Text { text: qsTr("游戏完整性校验"); font.pixelSize: StyleTokens.fontSizeMd; font.bold: true; color: StyleTokens.textSecondary }
                Text { text: qsTr("扫描选定版本的游戏文件完整性，检查损坏或缺失的文件。"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary; wrapMode: Text.WordWrap; Layout.fillWidth: true }

                // Start button
                Rectangle {
                    width: 140; height: 36; radius: StyleTokens.radiusMd
                    color: versionSettingsOverlay._verifyRunning ? StyleTokens.borderLight : (verifyBtnMouse.containsMouse ? "#2563EB" : StyleTokens.accent)
                    scale: verifyBtnMouse.containsMouse && !versionSettingsOverlay._verifyRunning ? 1.04 : 1.0
                    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                    Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
                    Text { anchors.centerIn: parent; text: versionSettingsOverlay._verifyRunning ? "校验中..." : "开始校验"; font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textPrimary }

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
                        Layout.fillWidth: true; height: 8; radius: StyleTokens.radiusSm; color: StyleTokens.bgInput
                        Rectangle {
                            height: 8; radius: StyleTokens.radiusSm
                            width: versionSettingsOverlay._verifyProgressTotal > 0 ? parent.width * (versionSettingsOverlay._verifyProgressDone / versionSettingsOverlay._verifyProgressTotal) : 0
                            color: versionSettingsOverlay._verifyRunning ? "#6080e8" : (versionSettingsOverlay._verifyProgressDone === versionSettingsOverlay._verifyProgressTotal ? StyleTokens.success : "#c05050")
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
                        font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary
                    }
                }

                Item { height: 12; width: 1 }

                // Red failure notification
                Rectangle {
                    Layout.fillWidth: true
                    height: 40; radius: StyleTokens.radiusMd
                    color: StyleTokens.errorBg
                    border.color: "#804040"
                    border.width: 1
                    visible: versionSettingsOverlay._verifyHasFailed && versionSettingsOverlay._verifyFailedFiles.length > 0
                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 8
                        Text {
                            text: "✗ " + qsTr("检测到 ") + versionSettingsOverlay._verifyFailedFiles.length + " 个文件异常"
                            font.pixelSize: StyleTokens.fontSizeSm; color: "#ff8080"
                        }
                    }
                }

                // Action buttons
                RowLayout {
                    spacing: 10
                    visible: versionSettingsOverlay._verifyHasFailed && versionSettingsOverlay._verifyFailedFiles.length > 0

                    // Repair button (hollow orange)
                    Rectangle {
                        width: 140; height: 36; radius: StyleTokens.radiusMd
                        color: "transparent"
                        border.color: repairBtnHover.hovered ? "#ff8c42" : "#c06420"
                        border.width: 1.5
                        Row {
                            anchors.centerIn: parent; spacing: 6
                            Image {
                                source: "icons/lucide/wrench.svg"
                                width: 14; height: 14
                                sourceSize.width: 14; sourceSize.height: 14
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text { text: qsTr("一键修复"); font.pixelSize: StyleTokens.fontSizeSm; color: repairBtnHover.hovered ? "#ff8c42" : "#e08050" }
                        }
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
                        width: 140; height: 36; radius: StyleTokens.radiusMd
                        color: "transparent"
                        border.color: reportBtnHover.hovered ? "#ff8080" : "#804040"
                        border.width: 1.5
                        Text { anchors.centerIn: parent; text: qsTr("[详情] 查看异常详情"); font.pixelSize: StyleTokens.fontSizeSm; color: reportBtnHover.hovered ? "#ff8080" : "#e07070" }
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
                Text { text: qsTr("版本工具"); font.pixelSize: StyleTokens.fontSizeXs; color: "#9ca0b4"; font.letterSpacing: 1.5 }
                Flow {
                    Layout.fillWidth: true; spacing: 8

                    // Clone
                    Rectangle { width: 110; height: 32; radius: StyleTokens.radiusSm; color: cloneHover.hovered ? StyleTokens.accentSubtle : "#0d1018"; border.color: StyleTokens.bgCard
                        Text { anchors.centerIn: parent; text: qsTr("克隆版本"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textSecondary }
                        HoverHandler { id: cloneHover }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (!currentSelectedVersion) { _showToast("请先选择一个版本"); return }
                                if (backend) {
                                    if (backend.cloneVersion(currentSelectedVersion)) {
                                        backend.refreshVersionDetails()
                                        _showToast("已克隆版本: " + currentSelectedVersion)
                                    } else {
                                        _showToast("克隆失败")
                                    }
                                }
                            }
                        }
                    }

                    // Rename
                    Rectangle { width: 110; height: 32; radius: StyleTokens.radiusSm; color: renameHover.hovered ? StyleTokens.accentSubtle : "#0d1018"; border.color: StyleTokens.bgCard
                        Text { anchors.centerIn: parent; text: qsTr("重命名版本"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textSecondary }
                        HoverHandler { id: renameHover }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (!currentSelectedVersion) { _showToast("请先选择一个版本"); return }
                                _showRenameDialog(currentSelectedVersion)
                            }
                        }
                    }

                    // Migrate (disabled until implemented properly)
                    Rectangle { width: 110; height: 32; radius: StyleTokens.radiusSm; color: "#0d1018"; border.color: StyleTokens.surfaceOverlay
                        Text { anchors.centerIn: parent; text: qsTr("迁移目录"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textMuted }
                    }

                    // Export user data — dual-state button/progress
                    Item {
                        width: exportProgress.visible ? 220 : 110; height: 32

                        // Idle: export button
                        Rectangle {
                            id: exportBtn
                            visible: !isExporting
                            width: 110; height: 32; radius: StyleTokens.radiusSm
                            color: exportHover.containsMouse ? StyleTokens.accentSubtle : "#0d1018"
                            border.color: exportHover.containsMouse ? "#7c3aed" : StyleTokens.bgCard
                            Behavior on color { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                            scale: exportHover.containsMouse ? 1.05 : 1.0
                            Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

                            property real _pressScale: 1.0
                            SequentialAnimation {
                                id: exportPressAnim
                                PropertyAction { target: exportBtn; property: "_pressScale"; value: 0.93 }
                                NumberAnimation { target: exportBtn; property: "_pressScale"; to: 1.0; duration: 300; easing.type: Easing.OutBack; easing.overshoot: 1.8 }
                            }
                            transform: Scale { origin.x: exportBtn.width / 2; origin.y: exportBtn.height / 2; xScale: exportBtn._pressScale; yScale: exportBtn._pressScale }

                            MouseArea {
                                id: exportHover
                                anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (backend) backend.logUiMsg("[export] btn clicked")
                                    exportPressAnim.start()
                                    if (!currentSelectedVersion) { _showToast("请先选择一个版本"); return }
                                    exportFileDialog.open()
                                }
                            }

                            Row {
                                anchors.centerIn: parent; spacing: 5
                                Image {
                                    source: "icons/lucide/package.svg"
                                    width: 14; height: 14; fillMode: Image.PreserveAspectFit
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    text: qsTr("导出用户数据")
                                    font.pixelSize: StyleTokens.fontSizeSm; color: "#b4a0f0"
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }
                        }

                        // Exporting: progress bar
                        Rectangle {
                            id: exportProgress
                            visible: isExporting
                            width: 220; height: 32; radius: StyleTokens.radiusSm
                            color: "#141028"; border.color: StyleTokens.bgHover
                            clip: true

                            // Fill bar
                            Rectangle {
                                anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
                                width: parent.width * exportPct / 100
                                radius: StyleTokens.radiusSm
                                color: StyleTokens.bgCard
                                Behavior on width { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                            }

                            RowLayout {
                                anchors.centerIn: parent; spacing: 6
                                Image {
                                    source: "icons/lucide/package.svg"
                                    width: 14; height: 14; fillMode: Image.PreserveAspectFit
                                }
                                Text {
                                    text: "导出中: " + exportStatus
                                    font.pixelSize: StyleTokens.fontSizeXs; color: "#b4a0f0"
                                    elide: Text.ElideRight
                                    Layout.maximumWidth: 130
                                }
                                Text {
                                    text: exportPct + "%"
                                    font.pixelSize: StyleTokens.fontSizeXs; font.weight: Font.Bold; color: "#a78bfa"
                                }
                            }
                        }
                    }
                }

                Item { height: 12; width: 1 }

                // Delete version
                Text { text: qsTr("危险操作"); font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.textDanger; font.letterSpacing: 1.5 }
                Rectangle {
                    Layout.fillWidth: true; height: 36; radius: StyleTokens.radiusMd; color: "transparent"; border.color: StyleTokens.surfaceLight
                    scale: delVerHover.containsMouse ? 1.02 : 1.0
                    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                    Text { anchors.left: parent.left; anchors.leftMargin: 14; anchors.verticalCenter: parent.verticalCenter; text: qsTr("删除此版本"); font.pixelSize: StyleTokens.fontSizeMd; color: delVerHover.containsMouse ? "#f05050" : "#c05050" }
                    MouseArea {
                        id: delVerHover
                        anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (!currentSelectedVersion) { _showToast("请先选择一个版本"); return }
                            _showConfirm("删除版本", "确认要删除版本 " + currentSelectedVersion + " 吗？\n此操作不可撤销，版本的所有文件将被删除。", function() {
                                if (backend) {
                                    backend.deleteVersion(currentSelectedVersion)
                                    backend.refreshVersionDetails()
                                }
                                showVersionSettings = false
                            })
                        }
                    }
                }
            }

            // Section 7: 导出整合包
            ExportModpackSection {
                id: exportSection
                anchors.fill: parent
                anchors.margins: 24
                opacity: settingsNav.currentIndex === 7 ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                backend: versionSettingsOverlay.backend
                toastManager: versionSettingsOverlay.toastManager
                versionId: currentSelectedVersion || ""
                versionName: currentSelectedVersion || ""
            }

        }
    }
    }

// ═══════════════════════════════════════════════════════════
//  LOCAL TOAST (replaces external toastManager dependency)
// ═══════════════════════════════════════════════════════════
function _showToast(msg) {
            _toastText = msg
            _toastVisible = true
            _toastTimer.restart()
        }
        property string _toastText: ""
        property bool _toastVisible: false
        Timer { id: _toastTimer; interval: 2500; onTriggered: _toastVisible = false }
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom; anchors.bottomMargin: 24
            width: _toastLabel.implicitWidth + 32; height: 36; radius: StyleTokens.radiusLg
            color: "#222840"; border.color: StyleTokens.accent; border.width: 1
            opacity: _toastVisible ? 1 : 0; z: 100
            visible: opacity > 0
            Behavior on opacity { NumberAnimation { duration: 200 } }
            Text {
                id: _toastLabel
                anchors.centerIn: parent
                text: _toastText; font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textSecondary
            }
        }

        // ═══════════════════════════════════════════════════════════
        //  LOCAL CONFIRM DIALOG (replaces external confirmDialog)
        // ═══════════════════════════════════════════════════════════
        function _showConfirm(title, msg, onOk) {
            _confirmTitle = title
            _confirmMessage = msg
            _confirmOnOk = onOk
            _confirmVisible = true
        }
        property string _confirmTitle: ""
        property string _confirmMessage: ""
        property var _confirmOnOk: null
        property bool _confirmVisible: false
        Rectangle {
            anchors.fill: parent; z: 200; color: "transparent"
            visible: _confirmVisible
            // Backdrop (semi-transparent)
            Rectangle {
                anchors.fill: parent
                color: "#000000"; opacity: _confirmVisible ? 0.55 : 0
                Behavior on opacity { NumberAnimation { duration: 150 } }
                MouseArea { anchors.fill: parent; onClicked: _confirmVisible = false }
            }
            // Panel (fully opaque sibling)
            Rectangle {
                anchors.centerIn: parent; width: 360; height: 190; radius: StyleTokens.radiusLg
                color: StyleTokens.surfaceOverlay; border.color: "#2a1f24"; border.width: 1
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 20; spacing: 12
                    Text { text: _confirmTitle; font.pixelSize: StyleTokens.fontSizeLg; font.weight: Font.Bold; color: StyleTokens.textSecondary }
                    Text { text: _confirmMessage; font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                    Item { Layout.fillHeight: true }
                    RowLayout {
                        Layout.alignment: Qt.AlignRight; spacing: 10
                        Rectangle { width: 80; height: 32; radius: StyleTokens.radiusSm; color: "transparent"; border.color: StyleTokens.bgHover
                            Text { anchors.centerIn: parent; text: "取消"; font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: _confirmVisible = false }
                        }
                        Rectangle { width: 80; height: 32; radius: StyleTokens.radiusSm; color: StyleTokens.textDanger
                            Text { anchors.centerIn: parent; text: "确认"; font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textPrimary }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    _confirmVisible = false
                                    if (_confirmOnOk) _confirmOnOk()
                                }
                            }
                        }
                    }
                }
            }
        }

        // ═══════════════════════════════════════════════════════════
        //  LOCAL RENAME DIALOG
        // ═══════════════════════════════════════════════════════════
        function _showRenameDialog(oldId) {
            _renameOldId = oldId
            _renameNewId = oldId
            _renameVisible = true
        }
        property string _renameOldId: ""
        property string _renameNewId: ""
        property bool _renameVisible: false
        Rectangle {
            anchors.fill: parent; z: 201; color: "transparent"
            visible: _renameVisible
            // Backdrop (semi-transparent)
            Rectangle {
                anchors.fill: parent
                color: "#000000"; opacity: _renameVisible ? 0.55 : 0
                Behavior on opacity { NumberAnimation { duration: 150 } }
                MouseArea { anchors.fill: parent; onClicked: _renameVisible = false }
            }
            // Panel (fully opaque sibling)
            Rectangle {
                anchors.centerIn: parent; width: 380; height: 190; radius: StyleTokens.radiusLg
                color: StyleTokens.surfaceOverlay; border.color: StyleTokens.bgHover; border.width: 1
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 20; spacing: 12
                    Text { text: "重命名版本"; font.pixelSize: StyleTokens.fontSizeLg; font.weight: Font.Bold; color: StyleTokens.textSecondary }
                    Text { text: "请输入新的版本名称"; font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary }
                    Rectangle {
                        Layout.fillWidth: true; height: 36; radius: StyleTokens.radiusMd; color: "#1a1d28"; border.color: StyleTokens.bgHover
                        TextInput {
                            anchors.fill: parent; anchors.margins: 10
                            text: _renameNewId; font.pixelSize: StyleTokens.fontSizeMd; color: StyleTokens.textSecondary
                            selectByMouse: true; clip: true; verticalAlignment: TextInput.AlignVCenter
                            onTextChanged: _renameNewId = text
                        }
                    }
                    Item { Layout.fillHeight: true }
                    RowLayout {
                        Layout.alignment: Qt.AlignRight; spacing: 10
                        Rectangle { width: 80; height: 32; radius: StyleTokens.radiusSm; color: "transparent"; border.color: StyleTokens.bgHover
                            Text { anchors.centerIn: parent; text: "取消"; font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: _renameVisible = false }
                        }
                        Rectangle { width: 80; height: 32; radius: StyleTokens.radiusSm; color: StyleTokens.accent
                            Text { anchors.centerIn: parent; text: "确认"; font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textPrimary }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    _renameVisible = false
                                    if (_renameNewId !== "" && _renameOldId !== _renameNewId) {
                                        if (backend && backend.renameVersion(_renameOldId, _renameNewId)) {
                                            backend.refreshVersionDetails()
                                            _showToast("已重命名: " + _renameOldId + " → " + _renameNewId)
                                        } else {
                                            _showToast("重命名失败")
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

    // 导出启动脚本 FileDialog（2026-08-07）
    FileDialog {
        id: launchScriptDialog
        title: qsTr("导出启动脚本")
        fileMode: FileDialog.SaveFile
        nameFilters: ["批处理脚本 (*.bat)"]
        defaultSuffix: "bat"
        currentFile: currentSelectedVersion ? (currentSelectedVersion + "_launch.bat") : ""
        property string versionId: ""
        property string javaPath: ""
        property int maxMemoryMb: 2048
        property string jvmArgs: ""
        property string gameArgs: ""
        property bool highPerfGpu: false
        onAccepted: {
            var sel = launchScriptDialog.selectedFile
            var path = ""
            if (typeof sel === "string") {
                path = sel
            } else if (sel && typeof sel.toString === "function") {
                path = sel.toString()
            }
            if (path.indexOf("file:///") === 0) path = path.substring(8)
            if (!path.toLowerCase().endsWith(".bat")) path = path + ".bat"
            if (!backend) return
            var script = backend.exportLaunchScript(launchScriptDialog.versionId,
                                                    launchScriptDialog.javaPath,
                                                    launchScriptDialog.maxMemoryMb,
                                                    launchScriptDialog.jvmArgs,
                                                    launchScriptDialog.gameArgs,
                                                    launchScriptDialog.highPerfGpu)
            if (!script) { toastManager.show("生成启动脚本失败"); return }
            if (backend.saveTextFile(path, script))
                toastManager.show("启动脚本已导出: " + path)
            else
                toastManager.show("写入启动脚本失败")
        }
    }

    // Export FileDialog
    FileDialog {
        id: exportFileDialog
        title: qsTr("导出用户数据")
        fileMode: FileDialog.SaveFile
        nameFilters: ["ZIP 文件 (*.zip)"]
        defaultSuffix: "zip"
        currentFile: currentSelectedVersion ? (currentSelectedVersion + "_userdata.zip") : ""
        onAccepted: {
            if (backend) backend.logUiMsg("[export] onAccepted fired")
            var sel = exportFileDialog.selectedFile
            // selectedFile 在不同 Qt 版本可能是 QUrl 或带 file:/// 前缀的字符串——统一防御式转本地路径
            var path = ""
            if (typeof sel === "string") {
                path = sel
            } else if (sel && typeof sel.toString === "function") {
                path = sel.toString()
            }
            if (path.indexOf("file:///") === 0) path = path.substring(8)
            if (!path.toLowerCase().endsWith(".zip")) {
                path = path + ".zip"
            }
            if (backend && backend.userDataBackend) {
                if (toastManager) toastManager.show("正在导出用户数据...")
                backend.userDataBackend.exportUserData(backend.gameDir, currentSelectedVersion, path)
            }
        }
        onRejected: {
            if (backend) backend.logUiMsg("[export] onRejected fired")
        }
    }


    // ── 导出联网失败确认（全屏弹窗，同主流启动器）──
    // 注意：opened 不能用绑定（绑定属性无法被按钮赋值关闭）——用信号驱动赋值。
    property bool _lookupAccepted: false
    // ConfirmDialog 组件靠外部 visible 控制（MainWindow Loader 同款模式）——
    // 实例化默认隐藏，信号到达才显示；按钮关闭走 closed → visible=false
    ConfirmDialog {
        id: exportLookupDialog
        title: qsTr("联网获取文件信息失败")
        message: ""
        visible: false
        onAccept: {
            // 确认按钮会先置 opened=false（触发 closed）再调 onAccept——
            // 用标志区分，防 onClosed 的 false 覆盖这里的 true
            versionSettingsOverlay._lookupAccepted = true
            if (exportSection && exportSection.backend && exportSection.backend.modpackExporter)
                exportSection.backend.modpackExporter.continueAfterLookupFailure(true)
        }
        onClosed: {
            exportLookupDialog.visible = false
            if (!versionSettingsOverlay._lookupAccepted
                && exportSection && exportSection.backend && exportSection.backend.modpackExporter)
                exportSection.backend.modpackExporter.continueAfterLookupFailure(false)
            versionSettingsOverlay._lookupAccepted = false
        }
    }
    // 导出分区联网失败 → 显示确认弹窗
    Connections {
        target: exportSection
        function onLookupDecisionRequested(message) {
            exportLookupDialog.message = message
            exportLookupDialog.visible = true
        }
    }
}
