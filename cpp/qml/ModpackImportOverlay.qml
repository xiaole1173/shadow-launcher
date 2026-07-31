// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

// ═══════════════════════════════════════════════════════════════════
// ModpackImportOverlay — 整合包导入弹窗（重构版）
//
// 流程（对标 主流启动器 导入链路，后端已完成全部逻辑）：
//   视图 0 文件选择：拖拽 / 按钮选择 .zip（CurseForge）或 .mrpack（Modrinth）
//   视图 1 执行：分步指示器（解析校验 → 资源解压 → 模组下载 → 安装游戏 → 收尾注册）
//              + 全局进度条 + 解析信息预览面板 + 模组下载列表 + 实时日志
//   视图 2 结果：成功 / 失败（含错误日志）/ 已取消
//
// 全部数据来自 backend.modpackImporter 信号（busy/progress/stepChanged/
// logLine/modItems/fileProgressChanged/importFinished），终止导入调用
// cancelImport()，后端自动回滚本次写入的文件。
// 运行期间弹窗不可关闭，仅允许「终止导入」主动停止。
// ═══════════════════════════════════════════════════════════════════
Rectangle {
    id: root
    anchors.fill: parent
    color: "transparent"
    visible: false
    z: 300

    property var toastManager: null
    property var appWindow: null

    // ── 视图状态 ──
    property int _view: 0              // 0 文件选择 / 1 执行 / 2 结果
    property bool _busy: false
    property bool _cancelling: false
    property string _filePath: ""
    property string _fileName: ""
    property string _statusText: ""
    property real _progress: 0.0
    property string _stepName: ""
    property int _activeStep: 0        // 1..5 当前阶段
    property int _doneCount: 0         // 已完成阶段数
    property int _failedStep: 0        // 失败阶段（0 = 无）
    property bool _finished: false
    property bool _infoRevealed: false // 解析信息面板是否已填充
    property bool _parsed: false       // 解析是否完成
    property var _info: ({ name: "", version: "", mc: "", loader: "", modCount: 0, fileCount: "", format: "", targetName: "" })
    property int _resultKind: 0        // 1 成功 / 2 失败 / 3 已取消
    property string _resultName: ""
    property string _resultVersion: ""
    property string _errorText: ""
    property bool _logAutoScroll: true // 日志是否自动滚动到底部

    readonly property var _steps: [qsTr("解析校验"), qsTr("资源解压"), qsTr("模组下载"), qsTr("安装游戏"), qsTr("收尾注册")]

    // ═══════════ 公共 API ═══════════
    function show() {
        if (!backend || !backend.modpackImporter) {
            if (toastManager) toastManager.show(qsTr("后端未就绪"))
            return
        }
        if (root._busy) return
        root.visible = true
        root._view = 0
        forceActiveFocus()
    }

    function hide() {
        root.visible = false
        root._logAutoScroll = true
    }

    // ═══════════ 内部逻辑 ═══════════
    function _beginRun() {
        modListModel.clear()
        logModel.clear()
        root._info = { name: "", version: "", mc: "", loader: "", modCount: 0, fileCount: "", format: "", targetName: "" }
        root._infoRevealed = false
        root._parsed = false
        root._activeStep = 1
        root._doneCount = 0
        root._failedStep = 0
        root._finished = false
        root._progress = 0.0
        root._statusText = ""
        root._stepName = ""
        root._logAutoScroll = true
        root._cancelling = false
        root._view = 1
    }

    function _setFile(path) {
        path = path.replace(/\\/g, "/")
        if (!/\.(zip|mrpack)$/i.test(path)) {
            if (toastManager) toastManager.show(qsTr("不支持的文件格式，请选择 .zip（CurseForge）或 .mrpack（Modrinth）"))
            return false
        }
        root._filePath = path
        root._fileName = path.split("/").pop()
        return true
    }

    function _startImport() {
        if (!root._filePath || root._busy) return
        backend.modpackImporter.startImport(root._filePath)  // busyChanged 同步触发 → _beginRun
    }

    function _cancelImport() {
        if (!root._busy || root._cancelling) return
        root._cancelling = true
        backend.modpackImporter.cancelImport()
    }

    function _closeResult() {
        if (backend && backend.modpackImporter) backend.modpackImporter.dismissResult()
        root.hide()
    }

    function _retry() {
        if (backend && backend.modpackImporter) backend.modpackImporter.dismissResult()
        root._cancelling = false
        root._view = 0   // 保留已选文件，可直接再次开始导入
    }

    function _goVersionPage() {
        if (backend && backend.modpackImporter) backend.modpackImporter.dismissResult()
        root.hide()
        if (root.appWindow) root.appWindow.showVersionSelect = true
    }

    // 后端步骤名 → 阶段序号（未知时按全局进度区间兜底）
    function _mapStep(name) {
        var s = name || ""
        var idx = root._activeStep
        if (s.indexOf(qsTr("解析")) >= 0) idx = 1
        else if (s.indexOf(qsTr("解压")) >= 0) idx = 2
        else if (s.indexOf(qsTr("下载")) >= 0) idx = 3
        else if (s.indexOf(qsTr("安装")) >= 0) idx = 4
        else if (s.indexOf(qsTr("整理")) >= 0) idx = 5
        else if (s.indexOf(qsTr("完成")) >= 0) { idx = 5; root._finished = true }
        else if (s.indexOf(qsTr("失败")) >= 0) { root._failedStep = Math.max(1, root._activeStep); root._doneCount = root._failedStep - 1; idx = root._failedStep }
        else if (s.indexOf(qsTr("取消")) >= 0) idx = 5
        else idx = root._stepFromProgress()
        if (idx > root._activeStep) root._doneCount = idx - 1
        root._activeStep = idx
    }

    function _stepFromProgress() {
        var p = root._progress
        if (p < 0.05) return 1
        if (p < 0.15) return 2
        if (p < 0.70) return 3
        if (p < 0.95) return 4
        return 5
    }

    function _appendLog(msg) {
        if (!msg) return
        var color = StyleTokens.textSubtle
        if (msg.indexOf("❌") >= 0) color = StyleTokens.errorLight
        else if (msg.indexOf("⚠") >= 0) color = StyleTokens.warning
        else if (msg.indexOf("✅") >= 0) color = StyleTokens.success
        logModel.append({ text: msg, color: color })
        while (logModel.count > 500) logModel.remove(0)
        root._parseInfo(msg)
    }

    // 从后端日志行提取整合包基础信息（无后端改动，纯 QML 侧展示用）
    function _parseInfo(msg) {
        // 整合包「名称」 版本 V  MC 1.20.1  加载器 forge 47.2.0
        var m = msg.match(/整合包「(.+?)」 版本 (.+?)  MC (.+?)  加载器 (.+)/)
        if (m) {
            root._setInfo("name", m[1])
            root._setInfo("version", m[2])
            root._setInfo("mc", m[3])
            var loader = m[4].trim()
            if (loader === "无") loader = ""
            root._setInfo("loader", loader)
            root._setInfo("format", /\.mrpack$/i.test(root._filePath) ? "Modrinth" : "CurseForge")
            root._infoRevealed = true
            root._parsed = true
            return
        }
        m = msg.match(/目标版本目录: (.+)/)
        if (m) {
            root._setInfo("targetName", m[1].replace(/\/+$/, "").split("/").pop())
            return
        }
        m = msg.match(/共 (\d+) 个文件/)
        if (m) { root._setInfo("fileCount", m[1]); return }
        if (msg.indexOf(qsTr("不含覆写资源")) >= 0) { root._setInfo("fileCount", "0"); return }
    }

    function _setInfo(key, value) {
        var o = {}
        var keys = Object.keys(root._info)
        for (var i = 0; i < keys.length; i++) o[keys[i]] = root._info[keys[i]]
        o[key] = value
        root._info = o
    }

    // 模组列表同步：按 index 增量合并（保留运行期 progress/speed 字段，避免重建动画）
    function _syncModList() {
        if (!backend || !backend.modpackImporter) return
        var arr = backend.modpackImporter.modItems || []
        for (var i = 0; i < arr.length; i++) {
            var m = arr[i]
            var idx = (m.index !== undefined && m.index !== null) ? m.index : i
            var found = -1
            for (var j = 0; j < modListModel.count; j++) {
                if (modListModel.get(j).index === idx) { found = j; break }
            }
            var name = m.name || ""
            var size = m.size || 0
            var status = m.status || "pending"
            var error = m.error || ""
            if (found >= 0) {
                var old = modListModel.get(found)
                if (old.status !== status) {
                    modListModel.setProperty(found, "status", status)
                    if (status === "done") modListModel.setProperty(found, "prog", 1.0)
                    else if (status === "fail") { modListModel.setProperty(found, "prog", 0.0); modListModel.setProperty(found, "speed", 0) }
                    else if (status === "pending" || status === "skipped") { modListModel.setProperty(found, "prog", 0.0); modListModel.setProperty(found, "speed", 0) }
                }
                if (old.name !== name) modListModel.setProperty(found, "name", name)
                if (old.size !== size) modListModel.setProperty(found, "size", size)
                if (old.error !== error) modListModel.setProperty(found, "error", error)
            } else {
                modListModel.append({
                    index: idx, name: name, size: size, status: status, error: error,
                    prog: status === "done" ? 1.0 : 0.0, speed: 0, lastFp: 0, lastTs: 0
                })
            }
        }
        root._setInfo("modCount", modListModel.count)
    }

    // 单文件进度：更新条目进度条 + 依据 size 增量估算实时速度
    // 注意：后端不产生 "downloading" 状态，条目进入下载中由本函数置位；
    // 匹配优先按文件名，未命中时回退到当前下载中条目（文件名可能与展示名不同）。
    function _onFileProgress(fileName, fp) {
        if (!fileName) return
        var found = -1
        for (var i = 0; i < modListModel.count; i++) {
            if (modListModel.get(i).name === fileName) { found = i; break }
        }
        if (found < 0) {
            for (i = 0; i < modListModel.count; i++) {
                if (modListModel.get(i).status === "downloading") { found = i; break }
            }
        }
        if (found < 0) return
        {
            var it = modListModel.get(found)
            var now = Date.now()
            var dt = now - (it.lastTs || now)
            var size = it.size || 0
            if (it.status !== "downloading") modListModel.setProperty(found, "status", "downloading")
            if (dt >= 400 && size > 0) {
                var dFrac = fp - (it.lastFp || 0)
                modListModel.setProperty(found, "speed", Math.max(0, dFrac * size * 1000.0 / dt))
            }
            modListModel.setProperty(found, "lastFp", fp)
            modListModel.setProperty(found, "lastTs", now)
            modListModel.setProperty(found, "prog", Math.max(0, Math.min(1, fp)))
        }
    }

    function _failedCount() {
        var c = 0
        for (var i = 0; i < modListModel.count; i++)
            if (modListModel.get(i).status === "fail") c++
        return c
    }

    function fmtSize(bytes) {
        if (!bytes || bytes < 0) return "0 B"
        var units = ["B", "KB", "MB", "GB"]
        var u = 0, v = bytes
        while (v >= 1024 && u < units.length - 1) { v /= 1024; u++ }
        return v.toFixed(u === 0 ? 0 : 1) + " " + units[u]
    }
    function fmtSpeed(speedBytes) { return fmtSize(speedBytes) + "/s" }

    // ═══════════ 后端信号桥 ═══════════
    Connections {
        target: backend && backend.modpackImporter ? backend.modpackImporter : null
        enabled: root.visible

        function onBusyChanged() {
            root._busy = backend.modpackImporter.busy
            if (root._busy) root._beginRun()
        }
        function onProgressChanged() {
            root._progress = backend.modpackImporter.progress || 0.0
            root._statusText = backend.modpackImporter.statusText || ""
            root._mapStep(backend.modpackImporter.currentStep || root._stepName)
        }
        function onStepChanged() {
            root._stepName = backend.modpackImporter.currentStep || ""
            root._mapStep(root._stepName)
        }
        function onLogLine(msg) { root._appendLog(msg) }
        function onModListChanged() { root._syncModList() }
        function onFileProgressChanged(fileName, fp) { root._onFileProgress(fileName, fp) }
        function onImportFinished(success, versionName, error) {
            root._busy = false
            if (success) {
                root._resultKind = 1
                root._resultName = backend.modpackImporter.resultName || versionName
                root._resultVersion = backend.modpackImporter.resultVersionId || error || ""
                root._finished = true
                root._doneCount = root._steps.length
                root._view = 2
                if (toastManager) toastManager.show(qsTr("整合包「%1」导入成功").arg(root._resultName))
            } else {
                root._resultKind = root._cancelling ? 3 : 2
                root._errorText = error || qsTr("未知错误")
                root._view = 2
            }
            root._cancelling = false
        }
    }

    // ═══════════ 文件选择对话框 ═══════════
    FileDialog {
        id: fileDialog
        title: qsTr("选择整合包文件")
        fileMode: FileDialog.OpenFile
        currentFolder: backend ? "file:///" + backend.gameDir.replace(/\\/g, "/") : ""
        nameFilters: [
            qsTr("整合包文件 (*.zip *.mrpack)"),
            qsTr("所有文件 (*)")
        ]
        onAccepted: {
            var p = fileDialog.selectedFile.toString()
            if (p.startsWith("file:///")) p = p.substring(8)
            root._setFile(p)
        }
    }

    // ═══════════ 遮罩层 ═══════════
    Rectangle {
        anchors.fill: parent
        color: "#80000000"
        opacity: root.visible ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                // 运行中不可关闭；结果页请走底部按钮
                if (root._view === 0) root.hide()
            }
        }
    }

    // ═══════════ 弹窗窗体 ═══════════
    Rectangle {
        id: card
        anchors.centerIn: parent
        width: 640
        height: Math.min(700, parent.height - 60)
        radius: StyleTokens.radiusWindow
        color: StyleTokens.surfaceOverlay
        border.color: StyleTokens.borderLight
        border.width: 1
        clip: true

        scale: root.visible ? 1.0 : 0.92
        opacity: root.visible ? 1 : 0
        Behavior on scale { NumberAnimation { duration: 300; easing.type: Easing.OutBack } }
        Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // ── 顶部标题栏 ──
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 48

                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 1
                    color: StyleTokens.bgElevated
                }

                Text {
                    anchors.left: parent.left; anchors.leftMargin: 20
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("导入整合包")
                    color: StyleTokens.textPrimary
                    font.pixelSize: StyleTokens.fontSizeLg
                    font.bold: true
                }

                Text {
                    anchors.left: parent.left; anchors.leftMargin: 20
                    anchors.top: parent.top; anchors.topMargin: 28
                    visible: root._view === 1
                    text: root._stepName || qsTr("执行中")
                    color: StyleTokens.textTertiary
                    font.pixelSize: StyleTokens.fontSizeXs
                }

                // 关闭按钮（运行期间隐藏）
                ShadowIconButton {
                    anchors.right: parent.right; anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    source: "icons/lucide/x.svg"
                    sourceWidth: 14; sourceHeight: 14
                    type: "close"
                    visible: root._view !== 1
                    onClicked: {
                        if (root._view === 2) root._closeResult()
                        else root.hide()
                    }
                }
            }

            // ── 主体内容区 ──
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                // ═══════ 视图 0：文件选择 ═══════
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 24
                    spacing: 16
                    opacity: root._view === 0 ? 1 : 0
                    visible: opacity > 0
                    Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }

                    Item { Layout.fillHeight: true }

                    // 拖拽 / 点击选择区
                    Rectangle {
                        id: dropBox
                        Layout.fillWidth: true
                        Layout.preferredHeight: 180
                        radius: StyleTokens.radiusXl
                        color: root._filePath ? StyleTokens.bgCard : StyleTokens.bgInput
                        border.color: root._filePath ? StyleTokens.accentHover
                                     : dropArea.containsDrag ? StyleTokens.accent : StyleTokens.borderLight
                        border.width: dropArea.containsDrag ? 2 : 1
                        Behavior on border.color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 10

                            Rectangle {
                                Layout.alignment: Qt.AlignHCenter
                                width: 48; height: 48; radius: 24
                                color: dropArea.containsDrag ? StyleTokens.accentSubtle : StyleTokens.bgElevated
                                Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
                                Image {
                                    anchors.centerIn: parent
                                    source: "icons/lucide/folder-open.svg"
                                    width: 22; height: 22
                                }
                            }
                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: root._filePath ? root._fileName
                                     : dropArea.containsDrag ? qsTr("松开以选择该文件")
                                     : qsTr("拖拽整合包到此处，或点击选择文件")
                                font.pixelSize: StyleTokens.fontSizeMd
                                font.bold: root._filePath
                                color: root._filePath ? StyleTokens.textPrimary : StyleTokens.textSecondary
                                elide: Text.ElideRight
                                Layout.maximumWidth: dropBox.width - 60
                            }
                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: qsTr("支持 CurseForge (.zip) 与 Modrinth (.mrpack)")
                                font.pixelSize: StyleTokens.fontSizeXs
                                color: StyleTokens.textMuted
                            }
                        }

                        // 清除已选文件
                        ShadowIconButton {
                            anchors.top: parent.top; anchors.topMargin: 10
                            anchors.right: parent.right; anchors.rightMargin: 10
                            source: "icons/lucide/x.svg"
                            sourceWidth: 12; sourceHeight: 12
                            visible: root._filePath.length > 0
                            onClicked: { root._filePath = ""; root._fileName = "" }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: fileDialog.open()
                        }

                        DropArea {
                            id: dropArea
                            anchors.fill: parent
                            onEntered: { if (drag.hasUrls) drag.accept(Qt.CopyAction) }
                            onDropped: {
                                if (drop.hasUrls && drop.urls.length > 0) {
                                    var p = drop.urls[0].toString()
                                    if (p.startsWith("file:///")) p = p.substring(8)
                                    root._setFile(p)
                                }
                            }
                        }
                    }

                    // 提示
                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("导入过程将自动解析压缩包、写入资源文件并下载模组，完成后刷新版本列表")
                        font.pixelSize: StyleTokens.fontSizeXs
                        color: StyleTokens.textMuted
                    }

                    Item { Layout.fillHeight: true }
                }

                // ═══════ 视图 1：执行 ═══════
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 12
                    opacity: root._view === 1 ? 1 : 0
                    visible: opacity > 0
                    Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }

                    // 分步指示器
                    ModpackStepIndicator {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 56
                        steps: root._steps
                        activeStep: root._activeStep
                        doneCount: root._doneCount
                        failedStep: root._failedStep
                        running: root._busy
                    }

                    // 全局进度
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Text {
                                text: root._stepName || qsTr("准备中…")
                                font.pixelSize: StyleTokens.fontSizeMd
                                font.bold: true
                                color: root._activeStep > 0 ? StyleTokens.accentLink : StyleTokens.textSecondary
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Text {
                                text: Math.round(root._progress * 100) + "%"
                                font.pixelSize: StyleTokens.fontSizeSm
                                color: StyleTokens.textTertiary
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 6
                            radius: 3
                            color: StyleTokens.bgElevated

                            Rectangle {
                                width: parent.width * Math.max(0.0, Math.min(1.0, root._progress))
                                height: 6
                                radius: 3
                                color: StyleTokens.accent
                                Behavior on width { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: root._statusText || ""
                            font.pixelSize: StyleTokens.fontSizeXs
                            color: StyleTokens.textSubtle
                            elide: Text.ElideRight
                            visible: text.length > 0
                        }
                    }

                    // 解析信息预览面板
                    ModpackInfoPanel {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 112
                        revealed: root._infoRevealed
                        packName: root._info.name
                        packVersion: root._info.version
                        mcVersion: root._info.mc
                        loader: root._info.loader
                        modCount: root._info.modCount
                        fileCount: root._info.fileCount
                        format: root._info.format
                        targetName: root._info.targetName
                    }

                    // ── 模组下载列表（复用 DownloadQueueCard 视觉语言）──
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: 90
                        spacing: 6

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Text {
                                text: root._parsed
                                    ? qsTr("在线模组（%1）").arg(modListModel.count)
                                    : qsTr("在线模组")
                                font.pixelSize: StyleTokens.fontSizeSm
                                font.bold: true
                                color: StyleTokens.textSecondary
                            }
                            Text {
                                visible: root._failedCount() > 0
                                text: qsTr("%1 个失败").arg(root._failedCount())
                                font.pixelSize: StyleTokens.fontSizeXs
                                color: StyleTokens.errorLight
                            }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: qsTr("点击「终止导入」可停止并自动清理")
                                font.pixelSize: StyleTokens.fontSizeXs
                                color: StyleTokens.textMuted
                                visible: root._busy && modListModel.count > 0
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: StyleTokens.radiusLg
                            color: "transparent"
                            clip: true

                            ListView {
                                id: modListView
                                anchors.fill: parent
                                model: modListModel
                                spacing: 6
                                clip: true
                                delegate: modDelegate
                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                            }

                            // 空态提示
                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 6
                                visible: modListModel.count === 0

                                LoadingSpinner {
                                    Layout.alignment: Qt.AlignHCenter
                                    Layout.preferredWidth: 18
                                    Layout.preferredHeight: 18
                                    running: root._busy && !root._parsed
                                }
                                Text {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: !root._parsed
                                        ? qsTr("正在准备模组下载清单…")
                                        : qsTr("该整合包不含需要在线下载的模组")
                                    font.pixelSize: StyleTokens.fontSizeXs
                                    color: StyleTokens.textMuted
                                }
                            }
                        }
                    }

                    // ── 实时日志 ──
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 116
                        spacing: 4

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Text {
                                text: qsTr("运行日志")
                                font.pixelSize: StyleTokens.fontSizeXs
                                color: StyleTokens.textTertiary
                            }
                            Item { Layout.fillWidth: true }
                            Rectangle {
                                visible: !root._logAutoScroll && logModel.count > 0
                                width: 66; height: 20; radius: 10
                                color: StyleTokens.bgHover
                                Text {
                                    anchors.centerIn: parent
                                    text: "↓ " + qsTr("最新")
                                    font.pixelSize: StyleTokens.fontSizeXs
                                    color: StyleTokens.textSecondary
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        root._logAutoScroll = true
                                        logView.positionViewAtEnd()
                                    }
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: StyleTokens.radiusLg
                            color: StyleTokens.bgInput
                            border.color: StyleTokens.border
                            border.width: 1
                            clip: true

                            ListView {
                                id: logView
                                anchors.fill: parent
                                anchors.margins: 6
                                model: logModel
                                spacing: 3
                                clip: true
                                delegate: Text {
                                    width: logView.width - 12
                                    text: model.text
                                    font.pixelSize: StyleTokens.fontSizeXs
                                    font.family: StyleTokens.fontFamilyMono
                                    color: model.color
                                    wrapMode: Text.WrapAnywhere
                                    lineHeight: 1.2
                                }
                                // 自动滚动：用户上翻时暂停，回到底部恢复
                                onCountChanged: { if (root._logAutoScroll) positionViewAtEnd() }
                                onContentYChanged: {
                                    if (contentY < contentHeight - height - 24) root._logAutoScroll = false
                                }
                                onMovementEnded: { if (atYEnd) root._logAutoScroll = true }
                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                            }
                        }
                    }
                }

                // ═══════ 视图 2：结果 ═══════
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 24
                    spacing: 14
                    opacity: root._view === 2 ? 1 : 0
                    visible: opacity > 0
                    Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }

                    // 结果图标
                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        width: 56; height: 56; radius: 28
                        color: root._resultKind === 1 ? StyleTokens.successBg
                             : root._resultKind === 3 ? StyleTokens.warningBg
                             : StyleTokens.errorBg
                        scale: root._view === 2 ? 1 : 0.6
                        Behavior on scale { NumberAnimation { duration: 350; easing.type: Easing.OutBack } }

                        Image {
                            anchors.centerIn: parent
                            width: 26; height: 26
                            source: root._resultKind === 1 ? "icons/lucide/check-circle.svg"
                                 : root._resultKind === 3 ? "icons/lucide/x-circle.svg"
                                 : "icons/lucide/alert-triangle-red.svg"
                        }
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: root._resultKind === 1 ? qsTr("导入成功")
                             : root._resultKind === 3 ? qsTr("导入已取消")
                             : qsTr("导入失败")
                        font.pixelSize: StyleTokens.fontSizeXl
                        font.bold: true
                        color: root._resultKind === 1 ? StyleTokens.success
                             : root._resultKind === 3 ? StyleTokens.warning
                             : StyleTokens.errorLight
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.fillWidth: true
                        text: root._resultKind === 1
                            ? qsTr("整合包「%1」已导入为版本「%2」，本地版本列表已刷新").arg(root._resultName).arg(root._resultVersion)
                            : root._resultKind === 3
                            ? qsTr("导入已取消，本次写入的文件已自动清理")
                            : root._errorText
                        font.pixelSize: StyleTokens.fontSizeSm
                        color: root._resultKind === 1 ? StyleTokens.textSecondary
                             : root._resultKind === 3 ? StyleTokens.textSecondary
                             : StyleTokens.errorLight
                        wrapMode: Text.Wrap
                        horizontalAlignment: Text.AlignHCenter
                    }

                    // 失败/取消：分步指示器定位失败阶段
                    ModpackStepIndicator {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 56
                        visible: root._resultKind >= 2
                        steps: root._steps
                        activeStep: root._resultKind === 3 ? 0 : root._activeStep
                        doneCount: root._doneCount
                        failedStep: root._resultKind === 3 ? 0 : root._failedStep
                        running: false
                    }

                    // 失败/取消：错误日志
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: root._resultKind >= 2
                        radius: StyleTokens.radiusLg
                        color: StyleTokens.bgInput
                        border.color: StyleTokens.border
                        border.width: 1
                        clip: true

                        ListView {
                            anchors.fill: parent
                            anchors.margins: 6
                            model: logModel
                            spacing: 3
                            clip: true
                            delegate: Text {
                                width: parent.width - 12
                                text: model.text
                                font.pixelSize: StyleTokens.fontSizeXs
                                font.family: StyleTokens.fontFamilyMono
                                color: model.color
                                wrapMode: Text.WrapAnywhere
                                lineHeight: 1.2
                            }
                            onCountChanged: positionViewAtEnd()
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                        }
                    }

                    Item { Layout.fillHeight: true }  // 成功时填充剩余空间
                }
            }

            // ── 底部按钮栏 ──
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 52

                Rectangle {
                    anchors.top: parent.top
                    width: parent.width
                    height: 1
                    color: StyleTokens.bgElevated
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 10

                    Item { Layout.fillWidth: true }

                    // ── 视图 0 ──
                    ShadowButton {
                        visible: root._view === 0
                        text: qsTr("取消")
                        accentColor: StyleTokens.bgElevated
                        textColor: StyleTokens.textSecondary
                        Layout.preferredWidth: 96
                        Layout.preferredHeight: 32
                        onClicked: root.hide()
                    }
                    ShadowButton {
                        visible: root._view === 0
                        text: qsTr("开始导入")
                        enabled: root._filePath.length > 0
                        accentColor: StyleTokens.accent
                        Layout.preferredWidth: 128
                        Layout.preferredHeight: 32
                        onClicked: root._startImport()
                    }

                    // ── 视图 1 ──
                    ShadowButton {
                        visible: root._view === 1
                        text: root._cancelling ? qsTr("正在取消…") : qsTr("终止导入")
                        enabled: !root._cancelling
                        accentColor: Qt.darker(StyleTokens.error, 1.15)
                        Layout.preferredWidth: 128
                        Layout.preferredHeight: 32
                        onClicked: root._cancelImport()
                    }

                    // ── 视图 2 成功 ──
                    ShadowButton {
                        visible: root._view === 2 && root._resultKind === 1
                        text: qsTr("前往版本主页")
                        accentColor: StyleTokens.accent
                        Layout.preferredWidth: 140
                        Layout.preferredHeight: 32
                        onClicked: root._goVersionPage()
                    }
                    ShadowButton {
                        visible: root._view === 2 && root._resultKind === 1
                        text: qsTr("关闭")
                        accentColor: StyleTokens.bgElevated
                        textColor: StyleTokens.textSecondary
                        Layout.preferredWidth: 96
                        Layout.preferredHeight: 32
                        onClicked: root._closeResult()
                    }

                    // ── 视图 2 失败 / 取消 ──
                    ShadowButton {
                        visible: root._view === 2 && root._resultKind >= 2
                        text: qsTr("重新导入")
                        accentColor: StyleTokens.accent
                        Layout.preferredWidth: 128
                        Layout.preferredHeight: 32
                        onClicked: root._retry()
                    }
                    ShadowButton {
                        visible: root._view === 2 && root._resultKind >= 2
                        text: qsTr("关闭")
                        accentColor: StyleTokens.bgElevated
                        textColor: StyleTokens.textSecondary
                        Layout.preferredWidth: 96
                        Layout.preferredHeight: 32
                        onClicked: root._closeResult()
                    }
                }
            }
        }
    }

    // ═══════════ Esc 键关闭（运行中禁用）══════════
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape && root.visible) {
            if (root._view === 0) { root.hide(); event.accepted = true }
            else if (root._view === 2) { root._closeResult(); event.accepted = true }
        }
    }

    focus: true

    // ═══════════ 模组下载列表条目（复用 DownloadQueueCard 视觉语言）═══════════
    Component {
        id: modDelegate
        Rectangle {
            id: itemBox
            width: ListView.view.width
            height: 50
            radius: StyleTokens.radiusLg
            color: itemBoxHover.containsMouse ? "#181e2a" : "#141a24"
            border.color: model.status === "fail" ? Qt.rgba(0.85, 0.35, 0.35, 0.4) : "transparent"
            border.width: 1
            Behavior on color { ColorAnimation { duration: 120; easing.type: Easing.OutCubic } }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 10

                // ── 状态图标 ──
                Item {
                    Layout.preferredWidth: 18
                    Layout.preferredHeight: 18

                    LoadingSpinner {
                        anchors.centerIn: parent
                        width: 14; height: 14
                        running: model.status === "downloading"
                    }
                    Rectangle {  // pending 灰点
                        anchors.centerIn: parent
                        width: 8; height: 8; radius: 4
                        color: StyleTokens.textMuted
                        visible: model.status === "pending" || model.status === ""
                    }
                    Rectangle {  // skipped 短横
                        anchors.centerIn: parent
                        width: 15; height: 15; radius: 8
                        color: StyleTokens.bgElevated
                        visible: model.status === "skipped"
                        Text {
                            anchors.centerIn: parent
                            text: "–"
                            color: StyleTokens.textMuted
                            font.pixelSize: 10
                        }
                    }
                    Rectangle {  // done / fail 圆底图标
                        anchors.centerIn: parent
                        width: 16; height: 16; radius: 8
                        color: model.status === "done" ? StyleTokens.successBg : StyleTokens.errorBg
                        visible: model.status === "done" || model.status === "fail"
                        Image {
                            anchors.centerIn: parent
                            width: 12; height: 12
                            source: model.status === "done" ? "icons/lucide/check.svg" : "icons/lucide/x.svg"
                        }
                    }
                }

                // ── 名称 + 状态文字 ──
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 5

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: model.name || ""
                            font.pixelSize: StyleTokens.fontSizeSm
                            font.bold: model.status === "downloading"
                            color: model.status === "fail" ? StyleTokens.errorLight
                                 : model.status === "done" ? StyleTokens.textSecondary
                                 : StyleTokens.textPrimary
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Text {
                            text: {
                                if (model.status === "downloading") return root.fmtSpeed(model.speed)
                                if (model.status === "done") return qsTr("完成")
                                if (model.status === "fail") return qsTr("失败")
                                if (model.status === "skipped") return qsTr("已跳过")
                                return ""
                            }
                            font.pixelSize: StyleTokens.fontSizeXs
                            color: model.status === "fail" ? StyleTokens.errorLight
                                 : model.status === "done" ? "#3fb950"
                                 : model.status === "downloading" ? StyleTokens.textTertiary
                                 : StyleTokens.textMuted
                        }
                    }

                    // ── 进度条（与 DownloadQueueCard 同款 4px）──
                    Rectangle {
                        Layout.fillWidth: true
                        height: 4
                        radius: 2
                        color: "#1e2a3a"
                        visible: model.status !== "pending" && model.status !== "skipped"

                        Rectangle {
                            width: parent.width * Math.min(model.prog, 1.0)
                            height: 4
                            radius: 2
                            color: model.status === "fail" ? StyleTokens.errorLight
                                 : model.prog >= 1.0 ? "#3fb950"
                                 : StyleTokens.accent
                            Behavior on width { SmoothedAnimation { velocity: 0.8; duration: 300 } }
                        }
                    }
                }
            }

            // ── 失败详情：悬停时覆盖显示完整报错 ──
            Rectangle {
                anchors.fill: parent
                radius: StyleTokens.radiusLg
                color: StyleTokens.errorBg
                border.color: StyleTokens.errorLight
                border.width: 1
                z: 5
                visible: itemBoxHover.containsMouse && model.status === "fail" && model.error.length > 0

                Text {
                    anchors.fill: parent
                    anchors.margins: 8
                    text: model.error || ""
                    font.pixelSize: StyleTokens.fontSizeXs
                    color: StyleTokens.errorLight
                    wrapMode: Text.Wrap
                    verticalAlignment: Text.AlignVCenter
                }
            }

            MouseArea {
                id: itemBoxHover
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
            }
        }
    }

    // ═══════════ 数据模型 ═══════════
    ListModel {
        id: modListModel
    }
    ListModel {
        id: logModel
    }
}
