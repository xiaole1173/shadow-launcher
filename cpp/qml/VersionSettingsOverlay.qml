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
    anchors.fill: parent; color: hasBg ? "transparent" : "#0c0f16"; z: 5
    property var backend: null
    property var toastManager: null
    property var confirmDialog: null
    readonly property bool hasBg: backend && typeof backend.customBgPath === "string" && backend.customBgPath.length > 0

    // Export progress state
    property bool isExporting: false
    property int exportPct: 0
    property string exportStatus: ""

    Connections {
        target: backend && backend.userDataBackend ? backend.userDataBackend : null
        function onExportProgress(pct, status) {
            isExporting = true
            exportPct = pct
            exportStatus = status
        }
        function onExportFinished(success, path, error) {
            isExporting = false
            exportPct = 0
            exportStatus = ""
            if (success) {
                _showToast("用户数据已导出")
            } else {
                _showToast("导出失败: " + (error || "未知错误"))
            }
        }
    }
    onVisibleChanged: {
        if (visible && backend) {
            // 版本切换时刷新所有数据列表（跟随版本隔离）
            backend.refreshVersionDetails()
            modListModel.clear(); var m = backend.listMods(currentSelectedVersion); for (var i = 0; i < m.length; i++) modListModel.append(m[i])
            rpListModel.clear(); var p = backend.listResourcePacks(); for (var i = 0; i < p.length; i++) rpListModel.append(p[i])
            saveListModel.clear(); var s = backend.listSaves(currentSelectedVersion); for (var i = 0; i < s.length; i++) saveListModel.append(s[i])
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
                    ? ("[完成] 校验完成: " + total + " 个文件全部通过")
                    : ("[失败] 校验完成: " + total + " 个文件全部通过。")
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
            color: "#11141c"; border.color: StyleTokens.bgInput
            RowLayout {
                anchors.fill: parent; anchors.margins: 14; spacing: 12

                // Back button
                Rectangle {
                    width: 60; height: 28; radius: StyleTokens.radiusMd; color: "transparent"; border.color: StyleTokens.bgCard
                    scale: backHover.containsMouse ? 1.06 : 1.0
                    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                    Text { anchors.centerIn: parent; text: qsTr("← 返回"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary }
                    MouseArea { id: backHover; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { showVersionSettings = false } }
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
                        font.pixelSize: StyleTokens.fontSizeXs; font.weight: Font.Medium; color: StyleTokens.textPrimary
                    }
                }

                // Spacer
                Item { Layout.fillWidth: true }

                // Launch button
                Rectangle {
                    id: topLaunchBtn
                    width: 100; height: 32; radius: StyleTokens.radiusMd
                    color: topLaunchHover.containsMouse ? (topLaunchHover.pressed ? "#2a3a90" : "#4a5ec8") : "#3a4eb8"
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
                { text: qsTr("概览"), icon: "" },
                { text: qsTr("启动配置"), icon: "" },
                { text: qsTr("内存设置"), icon: "" },
                { text: qsTr("Mod 管理"), icon: "" },
                { text: qsTr("资源包管理"), icon: "" },
                { text: qsTr("存档管理"), icon: "" },
                { text: qsTr("工具与维护"), icon: "" }
            ]

            // Check if current version has a mod loader
            function isModdedVersion() {
                if (!backend || !backend.versionDetails || !currentSelectedVersion) return false
                for (var i = 0; i < backend.versionDetails.length; i++) {
                    if (backend.versionDetails[i].id === currentSelectedVersion) {
                        var lt = backend.versionDetails[i].loaderType || ""
                        return (lt !== "原版" && lt !== "")
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
                                return (lt !== "原版" && lt !== "") ? 36 : 0
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
                                return lt !== "原版" && lt !== ""
                            }
                        }
                        return false
                    }
                    color: ListView.isCurrentItem ? "#162040" : (mouseArea2.containsMouse ? "#11141c" : "transparent")
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
            color: "#11141c"; radius: StyleTokens.radiusLg; border.color: StyleTokens.bgInput

            // Section 0: 概览 ===
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 24; spacing: 12
                opacity: settingsNav.currentIndex === 0 ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

                // Quick info
                Rectangle {
                    Layout.fillWidth: true; height: 52; radius: StyleTokens.radiusMd; color: StyleTokens.bgPrimary
                    RowLayout {
                        anchors.fill: parent; anchors.margins: 14; spacing: 12
                        ColumnLayout { Layout.fillWidth: true; spacing: 2
                            Text { text: qsTr("占用空间"); font.pixelSize: StyleTokens.fontSizeXs; color: "#a8b0c0" }
                            Text { text: backend && backend.currentVersionSummary ? backend.currentVersionSummary.sizeDisplay : "-"; font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.Medium; color: StyleTokens.textSecondary }
                        }
                        Rectangle { width: 1; height: 32; color: StyleTokens.bgCard }
                        ColumnLayout { Layout.fillWidth: true; spacing: 2
                            Text { text: qsTr("已装 Mod"); font.pixelSize: StyleTokens.fontSizeXs; color: "#a8b0c0" }
                            Text { text: (backend && backend.currentVersionSummary ? backend.currentVersionSummary.modCount : 0) + " 个"; font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.Medium; color: StyleTokens.textSecondary }

                        }
                        Rectangle { width: 1; height: 32; color: StyleTokens.bgCard }
                        ColumnLayout { Layout.fillWidth: true; spacing: 2
                            Text { text: qsTr("版本隔离"); font.pixelSize: StyleTokens.fontSizeXs; color: "#a8b0c0" }
                            Text { text: backend && backend.isolationEnabled ? "已开启" : "未开启"; font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.Medium; color: backend && backend.isolationEnabled ? "#4bc870" : "#707088" }
                        }
                    }
                }

                // Shortcuts
                Text { text: qsTr("快捷入口"); font.pixelSize: StyleTokens.fontSizeXs; color: "#9ca0b4"; font.letterSpacing: 1.5 }
                Flow {
                    Layout.fillWidth: true; spacing: 8

                    // Always visible
                    Rectangle { width: 120; height: 32; radius: StyleTokens.radiusMd; color: shortcutHover0.hovered ? "#3a5ed0" : "#2a4590"
                        scale: shMouse0.pressed ? 0.92 : 1.0
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        Row { anchors.centerIn: parent; spacing: 5
                            Image { source: "icons/lucide/folder.svg"; width: 14; height: 14; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: qsTr("版本文件夹"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textPrimary }
                        }
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
                    Rectangle { width: 120; height: 32; radius: StyleTokens.radiusMd; color: shortcutHover1.hovered ? "#3a5ed0" : "#2a4590"
                        scale: shMouse1.pressed ? 0.92 : 1.0
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        Row { anchors.centerIn: parent; spacing: 5
                            Image { source: "icons/lucide/map.svg"; width: 14; height: 14; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: qsTr("存档文件夹"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textPrimary }
                        }
                        HoverHandler { id: shortcutHover1 }
                        MouseArea { id: shMouse1; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: { if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }; if (backend) { if (backend.openSavesFolder(currentSelectedVersion)) { toastManager.show("已打开存档文件夹") } else { toastManager.show("无存档文件夹") } } }
                        }
                    }
                    Rectangle { width: 120; height: 32; radius: StyleTokens.radiusMd; color: shortcutHover2.hovered ? "#3a5ed0" : "#2a4590"
                        scale: shMouse2.pressed ? 0.92 : 1.0
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        Row { anchors.centerIn: parent; spacing: 5
                            Image { source: "icons/lucide/camera.svg"; width: 14; height: 14; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: qsTr("截图文件夹"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textPrimary }
                        }
                        HoverHandler { id: shortcutHover2 }
                        MouseArea { id: shMouse2; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: { if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }; if (backend) { if (backend.openScreenshotsFolder(currentSelectedVersion)) { toastManager.show("已打开截图文件夹") } else { toastManager.show("无截图文件夹") } } }
                        }
                    }
                    Rectangle { width: 120; height: 32; radius: StyleTokens.radiusMd; color: shortcutHover6.hovered ? "#3a5ed0" : "#2a4590"
                        scale: shMouse6.pressed ? 0.92 : 1.0
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        Row { anchors.centerIn: parent; spacing: 5
                            Image { source: "icons/lucide/file-text.svg"; width: 14; height: 14; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: qsTr("logs 日志"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textPrimary }
                        }
                        HoverHandler { id: shortcutHover6 }
                        MouseArea { id: shMouse6; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: { if (backend) { if (backend.openLogsFolder(currentSelectedVersion)) { toastManager.show("已打开日志文件夹") } else { toastManager.show("无日志文件夹") } } }
                        }
                    }
                    Rectangle { width: 130; height: 32; radius: StyleTokens.radiusMd; color: shortcutHover7.hovered ? "#3a5ed0" : "#2a4590"
                        scale: shMouse7.pressed ? 0.92 : 1.0
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        Row { anchors.centerIn: parent; spacing: 5
                            Image { source: "icons/lucide/file.svg"; width: 14; height: 14; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: qsTr("最新启动日志"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textPrimary }
                        }
                        HoverHandler { id: shortcutHover7 }
                        MouseArea { id: shMouse7; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: { if (backend) { if (backend.openLatestLog(currentSelectedVersion)) { toastManager.show("已打开最新日志") } else { toastManager.show("无日志文件") } } }
                        }
                    }
                    Rectangle { width: 130; height: 32; radius: StyleTokens.radiusMd; color: shortcutHover8.hovered ? "#c85050" : "#9a3838"
                        scale: shMouse8.pressed ? 0.92 : 1.0
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        Row { anchors.centerIn: parent; spacing: 5
                            Image { source: "icons/lucide/alert-octagon.svg"; width: 14; height: 14; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: qsTr("崩溃日志"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textPrimary }
                        }
                        HoverHandler { id: shortcutHover8 }
                        MouseArea { id: shMouse8; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: { if (backend) { if (backend.openCrashLog(currentSelectedVersion)) { toastManager.show("已打开崩溃日志") } else { toastManager.show("无崩溃报告") } } }
                        }
                    }

                    // Copy path
                    Rectangle { width: 130; height: 32; radius: StyleTokens.radiusMd; color: shortcutHoverCp.hovered ? "#3a5ed0" : "#2a4590"
                        scale: shMouseCp.pressed ? 0.92 : 1.0
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        Row { anchors.centerIn: parent; spacing: 5
                            Image { source: "icons/lucide/clipboard-copy.svg"; width: 14; height: 14; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: qsTr("复制版本路径"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textPrimary }
                        }
                        HoverHandler { id: shortcutHoverCp }
                        MouseArea { id: shMouseCp; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }
                                if (backend) { backend.copyVersionPath(currentSelectedVersion); toastManager.show("已复制版本路径") }
                            }
                        }
                    }

                    // Mod-only: visible only for modded versions
                    Rectangle { width: 120; height: 32; radius: StyleTokens.radiusMd; color: shortcutHover3.hovered ? "#3a5ed0" : "#3a4a90"
                        visible: backend ? backend.isModdedVersion(currentSelectedVersion) : false
                        scale: shMouse3.pressed ? 0.92 : 1.0
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        Row { anchors.centerIn: parent; spacing: 5
                            Image { source: "icons/lucide/puzzle.svg"; width: 14; height: 14; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: qsTr("Mod 文件夹"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textPrimary }
                        }
                        HoverHandler { id: shortcutHover3 }
                        MouseArea { id: shMouse3; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: { if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }; if (backend) { if (backend.openModsFolder(currentSelectedVersion)) { toastManager.show("已打开 Mod 文件夹") } else { toastManager.show("无 Mod 文件夹") } } }
                        }
                    }
                    Rectangle { width: 120; height: 32; radius: StyleTokens.radiusMd; color: shortcutHover4.hovered ? "#3a5ed0" : "#3a4a90"
                        visible: backend ? backend.isModdedVersion(currentSelectedVersion) : false
                        scale: shMouse4.pressed ? 0.92 : 1.0
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        Row { anchors.centerIn: parent; spacing: 5
                            Image { source: "icons/lucide/settings.svg"; width: 14; height: 14; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: qsTr("config 文件夹"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textPrimary }
                        }
                        HoverHandler { id: shortcutHover4 }
                        MouseArea { id: shMouse4; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: { if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }; if (backend) { backend.openConfigFolder(); toastManager.show("已打开 config 文件夹") } }
                        }
                    }
                    Rectangle { width: 120; height: 32; radius: StyleTokens.radiusMd; color: shortcutHover5.hovered ? "#3a5ed0" : "#3a4a90"
                        visible: backend ? backend.isModdedVersion(currentSelectedVersion) : false
                        scale: shMouse5.pressed ? 0.92 : 1.0
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        Row { anchors.centerIn: parent; spacing: 5
                            Image { source: "icons/lucide/sparkles.svg"; width: 14; height: 14; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: qsTr("光影包"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textPrimary }
                        }
                        HoverHandler { id: shortcutHover5 }
                        MouseArea { id: shMouse5; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: { if (!currentSelectedVersion) { toastManager.show("请先选择一个版本"); return }; if (backend) { if (backend.openShaderPacksFolder(currentSelectedVersion)) { toastManager.show("已打开光影包文件夹") } else { toastManager.show("无光影包文件夹") } } }
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

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10

                    // Header
                    RowLayout {
                        Text { text: qsTr("Mod 管理"); font.pixelSize: StyleTokens.fontSizeLg; font.bold: true; color: StyleTokens.textSecondary }
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
                                    if (backend) backend.openModsFolder(currentSelectedVersion)
                                }
                            }
                            HoverHandler { id: modFolderBtnH }
                        }

                        // Refresh button
                        Rectangle {
                            width: 30; height: 30; radius: StyleTokens.radiusMd; color: modRefreshBtnH.hovered ? "#222a3a" : "#141820"
                            border.color: StyleTokens.bgHover
                            scale: modRefreshBtnM.pressed ? 0.88 : 1.0
                            Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                            Behavior on color { ColorAnimation { duration: 200 } }
                            Image {
                                anchors.centerIn: parent
                                source: "icons/lucide/refresh-cw.svg"
                                width: 14; height: 14
                            }
                            MouseArea {
                                id: modRefreshBtnM; anchors.fill: parent; cursorShape: Qt.PointingHandCursor; hoverEnabled: true
                                onClicked: { modSection.refreshModList(); toastManager.show("Mod 列表已刷新") }
                            }
                            HoverHandler { id: modRefreshBtnH }
                        }
                    }

                    Text { text: qsTr("管理已安装的 Mod，拖拽 JAR 文件到此区域快捷导入。内置支持 Fabric/NeoForge/Forge 元数据识别。\n"); font.pixelSize: StyleTokens.fontSizeSm; color: "#9498a8"; wrapMode: Text.WordWrap; Layout.fillWidth: true }

                    // Search
                    Rectangle {
                        Layout.fillWidth: true; height: 30; radius: StyleTokens.radiusMd; color: StyleTokens.bgPrimary
                        border.color: modSearchField.activeFocus ? "#5068c8" : "#1a1f2e"
                        Behavior on border.color { ColorAnimation { duration: 200 } }
                        TextInput {
                            id: modSearchField; anchors.fill: parent; anchors.leftMargin: 32; anchors.rightMargin: 10
                            color: "#e4e8f2"; font.pixelSize: StyleTokens.fontSizeSm; verticalAlignment: TextInput.AlignVCenter
                            onTextChanged: modSection.filterModList()
                        }
                        Image {
                            source: "icons/lucide/search.svg"
                            width: 14; height: 14
                            anchors { left: parent.left; leftMargin: 10; verticalCenter: parent.verticalCenter }
                        }
                        Text {
                            anchors { left: parent.left; leftMargin: 32; verticalCenter: parent.verticalCenter }
                            text: qsTr("搜索 Mod 名称..."); color: "#9ca0b4"; font.pixelSize: StyleTokens.fontSizeSm
                            visible: !modSearchField.text
                        }
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
                            cellWidth: Math.min(260, (modScroll.width - 16) / Math.max(1, Math.floor((modScroll.width - 16) / 260)))
                            cellHeight: 136
                            clip: true

                            delegate: Rectangle {
                                id: card
                                width: modGrid.cellWidth - 12
                                height: 128
                                radius: StyleTokens.radiusLg
                                color: cardHover.hovered ? "#1a1f2e" : "#10131c"
                                border { width: 1; color: cardHover.hovered ? "#3a4eb8" : "#1e2430" }

                                Behavior on color { ColorAnimation { duration: 200 } }
                                Behavior on border.color { ColorAnimation { duration: 200 } }

                                // ── Staggered entrance animation ──
                                property int _entranceDelay: index * 60
                                opacity: 0
                                Component.onCompleted: entranceTimer.start()
                                Timer {
                                    id: entranceTimer
                                    interval: card._entranceDelay
                                    onTriggered: card.opacity = 1
                                }
                                Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }

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
                                            text: model.modName || model.fileName
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
                                                color: model.loader === "fabric" ? "#1a3620" : (model.loader === "neoforge" ? "#2a2020" : "#202036")
                                                Text {
                                                    id: loaderText
                                                    anchors.centerIn: parent
                                                    text: model.loader || ""
                                                    font.pixelSize: StyleTokens.fontSizeXs; color: model.loader === "fabric" ? "#4cc94c" : (model.loader === "neoforge" ? "#cc6644" : "#4466cc")
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

                                            // Delete button (visible on hover)
                                            Rectangle {
                                                width: 24; height: 24; radius: StyleTokens.radiusSm
                                                color: delBtnH.hovered ? "#401818" : "transparent"
                                                opacity: cardHover.hovered ? 1.0 : 0.0
                                                Behavior on opacity { NumberAnimation { duration: 200 } }
                                                Behavior on color { ColorAnimation { duration: 200 } }
                                                Image {
                                                    anchors.centerIn: parent
                                                    source: "icons/lucide/trash-2.svg"
                                                    width: 13; height: 13
                                                }
                                                MouseArea {
                                                    id: delBtnM; anchors.fill: parent; cursorShape: Qt.PointingHandCursor; hoverEnabled: true
                                                    onClicked: {
                                                        if (backend) {
                                                            var fn = model.fileName || ""
                                                            backend.deleteMod(fn, currentSelectedVersion)
                                                            modListModel.remove(index)
                                                            toastManager.show("已删除: " + fn)
                                                        }
                                                    }
                                                }
                                                HoverHandler { id: delBtnH }
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
                    if (backend) {
                        var m = backend.listMods(currentSelectedVersion)
                        for (var i = 0; i < m.length; i++) modListModel.append(m[i])
                    }
                }

                function filterModList() {
                    modListModel.clear()
                    if (backend) {
                        var allMods = backend.listMods(currentSelectedVersion)
                        var query = modSearchField.text.toLowerCase()
                        for (var i = 0; i < allMods.length; i++) {
                            var name = (allMods[i].modName || allMods[i].fileName || "").toLowerCase()
                            if (!query || name.indexOf(query) >= 0)
                                modListModel.append(allMods[i])
                        }
                    }
                }

                // Drop area — put a transparent visual child to receive drag events
                DropArea {
                    id: modDropArea
                    anchors.fill: parent
                    onEntered: {
                        if (drag.hasUrls) {
                            drag.accept(Qt.CopyAction)
                        }
                    }
                    onDropped: {
                        if (drop.hasUrls && backend) {
                            for (var i = 0; i < drop.urls.length; i++) {
                                var path = drop.urls[i].toString()
                                if (path.startsWith("file:///")) path = path.substring(8)
                                if (path.endsWith(".jar") || path.endsWith(".JAR")) {
                                    if (backend.importMod(path, currentSelectedVersion))
                                        toastManager.show("已导入: " + path.split("/").pop())
                                }
                            }
                            modSection.refreshModList()
                        }
                    }
                    Rectangle {
                        anchors.fill: parent
                        color: "transparent"
                        border { width: 2; color: modDropArea.containsDrag ? "#5080e8" : "transparent" }
                        radius: StyleTokens.radiusLg
                        opacity: modDropArea.containsDrag ? 1 : 0
                    }
                }
            }

            // Section 4: 资源包管理 ===
            Item {
                id: rpSection
                anchors.fill: parent; anchors.margins: 24
                opacity: settingsNav.currentIndex === 4 ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

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
                                    if (backend) backend.openResourcePacksFolder(currentSelectedVersion)
                                }
                            }
                            HoverHandler { id: rpFolderBtnH }
                        }

                        // Refresh button
                        Rectangle {
                            width: 30; height: 30; radius: StyleTokens.radiusMd; color: rpRefreshBtnH.hovered ? "#222a3a" : "#141820"
                            border.color: StyleTokens.bgHover
                            scale: rpRefreshBtnM.pressed ? 0.88 : 1.0
                            Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                            Behavior on color { ColorAnimation { duration: 200 } }
                            Image {
                                anchors.centerIn: parent
                                source: "icons/lucide/refresh-cw.svg"
                                width: 14; height: 14
                            }
                            MouseArea {
                                id: rpRefreshBtnM; anchors.fill: parent; cursorShape: Qt.PointingHandCursor; hoverEnabled: true
                                onClicked: { rpSection.refreshRPList(); toastManager.show("资源包列表已刷新") }
                            }
                            HoverHandler { id: rpRefreshBtnH }
                        }
                    }

                    Text { text: qsTr("管理已安装的资源包和材质包。支持 pack.mcmeta 元数据解析和 pack.png 图标提取。\n"); font.pixelSize: StyleTokens.fontSizeSm; color: "#9498a8"; wrapMode: Text.WordWrap; Layout.fillWidth: true }

                    // Search
                    Rectangle {
                        Layout.fillWidth: true; height: 30; radius: StyleTokens.radiusMd; color: StyleTokens.bgPrimary
                        border.color: rpSearchField.activeFocus ? "#5068c8" : "#1a1f2e"
                        Behavior on border.color { ColorAnimation { duration: 200 } }
                        TextInput {
                            id: rpSearchField; anchors.fill: parent; anchors.leftMargin: 32; anchors.rightMargin: 10
                            color: "#e4e8f2"; font.pixelSize: StyleTokens.fontSizeSm; verticalAlignment: TextInput.AlignVCenter
                            onTextChanged: rpSection.filterRPList()
                        }
                        Image {
                            source: "icons/lucide/search.svg"
                            width: 14; height: 14
                            anchors { left: parent.left; leftMargin: 10; verticalCenter: parent.verticalCenter }
                        }
                        Text {
                            anchors { left: parent.left; leftMargin: 32; verticalCenter: parent.verticalCenter }
                            text: qsTr("搜索资源包名称..."); color: "#9ca0b4"; font.pixelSize: StyleTokens.fontSizeSm
                            visible: !rpSearchField.text
                        }
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
                            cellWidth: Math.min(260, (rpScroll.width - 16) / Math.max(1, Math.floor((rpScroll.width - 16) / 260)))
                            cellHeight: 136
                            clip: true

                            delegate: Rectangle {
                                id: rpCard
                                width: rpGrid.cellWidth - 12
                                height: 128
                                radius: StyleTokens.radiusLg
                                color: rpCardHover.hovered ? "#1a1f2e" : "#10131c"
                                border { width: 1; color: rpCardHover.hovered ? "#3a4eb8" : "#1e2430" }

                                Behavior on color { ColorAnimation { duration: 200 } }
                                Behavior on border.color { ColorAnimation { duration: 200 } }

                                // ── Staggered entrance animation ──
                                property int _entranceDelay: index * 60
                                opacity: 0
                                Component.onCompleted: entranceTimer.start()
                                Timer {
                                    id: entranceTimer
                                    interval: rpCard._entranceDelay
                                    onTriggered: rpCard.opacity = 1
                                }
                                Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }

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
                                            Rectangle {
                                                width: 24; height: 24; radius: StyleTokens.radiusSm
                                                color: rpDelBtnH.hovered ? "#401818" : "transparent"
                                                opacity: rpCardHover.hovered ? 1.0 : 0.0
                                                Behavior on opacity { NumberAnimation { duration: 200 } }
                                                Behavior on color { ColorAnimation { duration: 200 } }
                                                Image {
                                                    anchors.centerIn: parent
                                                    source: "icons/lucide/trash-2.svg"
                                                    width: 13; height: 13
                                                }
                                                MouseArea {
                                                    id: rpDelBtnM; anchors.fill: parent; cursorShape: Qt.PointingHandCursor; hoverEnabled: true
                                                    onClicked: {
                                                        if (backend) {
                                                            var fn = model.fileName || ""
                                                            backend.deleteResourcePack(fn, currentSelectedVersion)
                                                            rpListModel.remove(index)
                                                            toastManager.show("已删除: " + fn)
                                                        }
                                                    }
                                                }
                                                HoverHandler { id: rpDelBtnH }
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
                    if (backend) {
                        var p = backend.listResourcePacks(currentSelectedVersion)
                        for (var i = 0; i < p.length; i++) rpListModel.append(p[i])
                    }
                }

                function filterRPList() {
                    rpListModel.clear()
                    if (backend) {
                        var allPacks = backend.listResourcePacks(currentSelectedVersion)
                        var query = rpSearchField.text.toLowerCase()
                        for (var i = 0; i < allPacks.length; i++) {
                            var name = (allPacks[i].name || allPacks[i].fileName || "").toLowerCase()
                            if (!query || name.indexOf(query) >= 0)
                                rpListModel.append(allPacks[i])
                        }
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
                        if (backend) {
                            var s = backend.listSaves(currentSelectedVersion)
                            for (var i = 0; i < s.length; i++) saveListModel.append(s[i])
                        }
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
                                if (backend) backend.openSavesFolder(currentSelectedVersion)
                            }
                        }
                        HoverHandler { id: saveFolderBtnH }
                    }

                    // Refresh button
                    Rectangle {
                        width: 30; height: 30; radius: StyleTokens.radiusMd; color: saveRefreshBtnH.hovered ? "#222a3a" : "#141820"
                        border.color: StyleTokens.bgHover
                        scale: saveRefreshBtnM.pressed ? 0.88 : 1.0
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        Behavior on color { ColorAnimation { duration: 200 } }
                        Image {
                            anchors.centerIn: parent
                            source: "icons/lucide/refresh-cw.svg"
                            width: 14; height: 14
                        }
                        MouseArea {
                            id: saveRefreshBtnM; anchors.fill: parent; cursorShape: Qt.PointingHandCursor; hoverEnabled: true
                            onClicked: {
                                saveListModel.clear()
                                if (backend) {
                                    var s = backend.listSaves(currentSelectedVersion)
                                    for (var i = 0; i < s.length; i++) saveListModel.append(s[i])
                                }
                                toastManager.show("存档列表已刷新")
                            }
                        }
                        HoverHandler { id: saveRefreshBtnH }
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
                        width: saveListView.width; height: 36; radius: StyleTokens.radiusSm; color: saveRowHover.hovered ? "#11141c" : "transparent"

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
                                Text { text: qsTr("删除"); font.pixelSize: StyleTokens.fontSizeXs; color: "#c05050" }
                            }
                                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                    onClicked: { if (backend) { _showConfirm("确认删除", "删除存档 \"" + name + "\"？\n此操作不可撤销。", function() { backend.deleteSave(name, currentSelectedVersion); saveListModel.remove(index); toastManager.show("已删除存档: " + name) }) } }
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

            // Section 6: 工具与维护
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 24; spacing: 12
                opacity: settingsNav.currentIndex === 6 ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

                Text { text: qsTr("游戏完整性校验"); font.pixelSize: StyleTokens.fontSizeMd; font.bold: true; color: StyleTokens.textSecondary }
                Text { text: qsTr("扫描选定版本的游戏文件完整性，检查损坏或缺失的文件。"); font.pixelSize: StyleTokens.fontSizeSm; color: "#9498a8"; wrapMode: Text.WordWrap; Layout.fillWidth: true }

                // Start button
                Rectangle {
                    width: 140; height: 36; radius: StyleTokens.radiusMd
                    color: versionSettingsOverlay._verifyRunning ? "#2a3040" : (verifyBtnMouse.containsMouse ? "#2563EB" : "#3a4eb8")
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
                            text: qsTr("[失败] 检测到 ") + versionSettingsOverlay._verifyFailedFiles.length + " 个文件异常"
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
                        Text { anchors.centerIn: parent; text: qsTr("[修复] 一键修复"); font.pixelSize: StyleTokens.fontSizeSm; color: repairBtnHover.hovered ? "#ff8c42" : "#e08050" }
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
                    Rectangle { width: 110; height: 32; radius: StyleTokens.radiusSm; color: cloneHover.hovered ? "#1a2848" : "#0d1018"; border.color: StyleTokens.bgCard
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
                    Rectangle { width: 110; height: 32; radius: StyleTokens.radiusSm; color: renameHover.hovered ? "#1a2848" : "#0d1018"; border.color: StyleTokens.bgCard
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
                            color: exportHover.containsMouse ? "#1a2848" : "#0d1018"
                            border.color: exportHover.containsMouse ? "#7c3aed" : "#1a1f2e"
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
                Text { text: qsTr("危险操作"); font.pixelSize: StyleTokens.fontSizeXs; color: "#c05050"; font.letterSpacing: 1.5 }
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
            color: "#222840"; border.color: "#3a4eb8"; border.width: 1
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
                color: "#141820"; border.color: "#2a1f24"; border.width: 1
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 20; spacing: 12
                    Text { text: _confirmTitle; font.pixelSize: StyleTokens.fontSizeLg; font.weight: Font.Bold; color: StyleTokens.textSecondary }
                    Text { text: _confirmMessage; font.pixelSize: StyleTokens.fontSizeSm; color: "#b0b8c8"; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                    Item { Layout.fillHeight: true }
                    RowLayout {
                        Layout.alignment: Qt.AlignRight; spacing: 10
                        Rectangle { width: 80; height: 32; radius: StyleTokens.radiusSm; color: "transparent"; border.color: StyleTokens.bgHover
                            Text { anchors.centerIn: parent; text: "取消"; font.pixelSize: StyleTokens.fontSizeSm; color: "#b0b8c8" }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: _confirmVisible = false }
                        }
                        Rectangle { width: 80; height: 32; radius: StyleTokens.radiusSm; color: "#c05050"
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
                color: "#141820"; border.color: StyleTokens.bgHover; border.width: 1
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 20; spacing: 12
                    Text { text: "重命名版本"; font.pixelSize: StyleTokens.fontSizeLg; font.weight: Font.Bold; color: StyleTokens.textSecondary }
                    Text { text: "请输入新的版本名称"; font.pixelSize: StyleTokens.fontSizeSm; color: "#b0b8c8" }
                    Rectangle {
                        Layout.fillWidth: true; height: 36; radius: StyleTokens.radiusMd; color: "#1a1d28"; border.color: "#2a3040"
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
                            Text { anchors.centerIn: parent; text: "取消"; font.pixelSize: StyleTokens.fontSizeSm; color: "#b0b8c8" }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: _renameVisible = false }
                        }
                        Rectangle { width: 80; height: 32; radius: StyleTokens.radiusSm; color: "#3a4eb8"
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

    // Export FileDialog
    FileDialog {
        id: exportFileDialog
        title: qsTr("导出用户数据")
        fileMode: FileDialog.SaveFile
        nameFilters: ["ZIP 文件 (*.zip)"]
        defaultSuffix: "zip"
        currentFile: currentSelectedVersion ? (currentSelectedVersion + "_userdata.zip") : ""
        onAccepted: {
            var path = exportFileDialog.selectedFile
            if (!path.toString().toLowerCase().endsWith(".zip")) {
                path = path + ".zip"
            }
            if (backend && backend.userDataBackend) {
                backend.userDataBackend.exportUserData(backend.gameDir, currentSelectedVersion, path.toString())
            }
        }
    }
}
