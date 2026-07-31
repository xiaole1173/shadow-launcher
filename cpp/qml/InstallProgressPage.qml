// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ═══════════════════════════════════════════════════════════════════
// InstallProgressPage — 全局下载进度页（导入全过程载体）
//
// 页面结构：
//   顶部：整合包导入条目（仅导入进行 / 有结果时可见）——分步指示器、
//         全局进度、解析信息预览、模组下载列表、实时日志、终止导入。
//   下方：原有下载任务卡片列表（installCardsModel），行为不变。
//
// ── 代码内必须写明的三处差异化说明 ──
// ① 阶段差异：后端原生 6 步执行链路（解析→解压→下载→安装游戏→收尾→完成），
//    UI 合并为 5 个展示阶段，并补充原文档缺失的「游戏本体安装」环节；
//    总进度由后端统一计算下发，QML 只做绑定渲染，禁止二次加权运算。
// ② 解析预览差异：后端无独立「只解析」接口，解析与导入流程一体化执行；
//    信息面板（ModpackInfoPanel）挂载在下载页顶部，解析完成即时渲染。
// ③ 展示载体差异：导入运行阶段完全使用全局下载进度页承载，导入弹窗
//    （ModpackImportOverlay）只做文件选择入口，流程启动后弹窗永久关闭，
//    结束无任何回显弹窗；成功/失败/终止均沿用普通 MC 下载任务收尾逻辑。
// ═══════════════════════════════════════════════════════════════════
Item {
    id: root

    property var mainWindow: null
    property var backend: null

    // ═══════════ 导入条目状态 ═══════════
    property int _entryState: 0            // 0 无 / 1 执行中 / 2 成功 / 3 失败 / 4 已取消
    property bool _entryDismissed: false   // 用户/自动关闭条目
    property bool _cancelRequested: false  // 已点终止导入（用于区分失败/取消）
    property string _statusText: ""
    property real _progress: 0.0
    property string _stepName: ""
    property int _activeStep: 0            // 1..5 当前阶段
    property int _doneCount: 0             // 已完成阶段数
    property int _failedStep: 0            // 失败阶段（0 = 无）
    property bool _infoRevealed: false     // 解析信息面板是否已填充
    property bool _parsed: false           // 解析是否完成
    property var _info: ({ name: "", version: "", mc: "", loader: "", modCount: 0, fileCount: "", format: "", targetName: "" })
    property string _resultName: ""
    property string _resultVersion: ""
    property string _errorText: ""
    property bool _logAutoScroll: true

    readonly property var _steps: [qsTr("解析校验"), qsTr("资源解压"), qsTr("模组下载"), qsTr("安装游戏"), qsTr("收尾注册")]

    // ═══════════ 导入条目逻辑（详见文件头三处差异化说明）═══════════
    function _beginImport() {
        modListModel.clear()
        logModel.clear()
        root._info = { name: "", version: "", mc: "", loader: "", modCount: 0, fileCount: "", format: "", targetName: "" }
        root._infoRevealed = false
        root._parsed = false
        root._activeStep = 1
        root._doneCount = 0
        root._failedStep = 0
        root._progress = 0.0
        root._statusText = ""
        root._stepName = ""
        root._logAutoScroll = true
        root._cancelRequested = false
        root._entryDismissed = false
        root._entryState = 1
        // 来源字段：弹窗根声明属性（mainWindow.modpackImportOverlay 公开引用），
        // 避免动态属性赋值（Qt6 qmlcachegen 下会抛 Cannot assign to non-existent property）
        root._setInfo("format",
            (root.mainWindow && root.mainWindow.modpackImportOverlay)
                ? root.mainWindow.modpackImportOverlay._modpackImportFormat : "")
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
        else if (s.indexOf(qsTr("完成")) >= 0) idx = 5
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
    // 已知 BUG 修复：后端不下发标准 downloading 状态（仅 pending/skipped/fail/done），
    // 采用【文件名称匹配 + 下载中状态兜底判定】，避免多文件并发刷新时条目绑定错乱。
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

    // ═══════════ 后端导入信号桥（页面常驻，跨页面保持捕获）═══════════
    Connections {
        target: backend && backend.modpackImporter ? backend.modpackImporter : null

        function onBusyChanged() {
            if (backend && backend.modpackImporter && backend.modpackImporter.busy)
                root._beginImport()
        }
        function onProgressChanged() {
            if (!backend || !backend.modpackImporter) return
            root._progress = backend.modpackImporter.progress || 0.0
            root._statusText = backend.modpackImporter.statusText || ""
            root._mapStep(backend.modpackImporter.currentStep || root._stepName)
        }
        function onStepChanged() {
            if (!backend || !backend.modpackImporter) return
            root._stepName = backend.modpackImporter.currentStep || ""
            root._mapStep(root._stepName)
        }
        function onLogLine(msg) { root._appendLog(msg) }
        function onModListChanged() { root._syncModList() }
        function onFileProgressChanged(fileName, fp) { root._onFileProgress(fileName, fp) }
        function onImportFinished(success, versionName, error) {
            if (!backend || !backend.modpackImporter) return
            if (success) {
                root._entryState = 2
                root._resultName = backend.modpackImporter.resultName || versionName
                root._resultVersion = backend.modpackImporter.resultVersionId || error || ""
                root._doneCount = root._steps.length
                importDoneTimer.restart()   // 成功：短暂展示后自动收拢（对齐普通下载卡片）
            } else {
                root._entryState = root._cancelRequested ? 4 : 3
                root._errorText = error || qsTr("未知错误")
            }
            root._cancelRequested = false
        }
    }

    // 成功收拢定时器
    Timer {
        id: importDoneTimer
        interval: 6000
        repeat: false
        onTriggered: {
            if (root._entryState === 2) root._entryDismissed = true
        }
    }

    Rectangle {
        anchors.fill: parent
        color: StyleTokens.bgPrimary
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ═══════ 整合包导入条目（执行 / 结果常驻载体）═══════
        Rectangle {
            id: importEntry
            Layout.fillWidth: true
            // ⚠ 修复：普通 Rectangle 的 implicitHeight=0，若不设高度外层 ColumnLayout
            // 会分配 0 高度 → 卡片不可见（导入在后台正常跑但页面无渲染）。
            // 高度绑定内容区隐式高度（+上下 margins 14*2），内容增减时自适应。
            Layout.preferredHeight: Math.min(560, entryBody.implicitHeight + 28)
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.topMargin: 12
            radius: StyleTokens.radiusLg
            color: StyleTokens.surfaceOverlay
            border.color: StyleTokens.borderLight
            border.width: 1
            clip: true

            visible: root._entryState !== 0 && !root._entryDismissed
            opacity: visible ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

            ColumnLayout {
                id: entryBody
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                // ── 头部：标题 + 状态徽章 + 终止 / 关闭 ──
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        text: qsTr("导入整合包")
                        font.pixelSize: StyleTokens.fontSizeMd
                        font.bold: true
                        color: StyleTokens.textPrimary
                    }

                    // 状态徽章
                    Rectangle {
                        Layout.preferredWidth: 64
                        Layout.preferredHeight: 20
                        radius: 10
                        color: root._entryState === 1 ? StyleTokens.accentSubtle
                             : root._entryState === 2 ? StyleTokens.successBg
                             : root._entryState === 4 ? StyleTokens.warningBg
                             : StyleTokens.errorBg
                        Text {
                            anchors.centerIn: parent
                            text: root._entryState === 1 ? qsTr("执行中")
                                 : root._entryState === 2 ? qsTr("已完成")
                                 : root._entryState === 4 ? qsTr("已取消")
                                 : qsTr("失败")
                            font.pixelSize: StyleTokens.fontSizeXs
                            color: root._entryState === 1 ? StyleTokens.accentLink
                                 : root._entryState === 2 ? StyleTokens.success
                                 : root._entryState === 4 ? StyleTokens.warning
                                 : StyleTokens.errorLight
                        }
                    }

                    Item { Layout.fillWidth: true }

                    // 终止导入（运行期唯一可用操作）
                    ShadowButton {
                        visible: root._entryState === 1
                        text: root._cancelRequested ? qsTr("正在取消…") : qsTr("终止导入")
                        enabled: !root._cancelRequested
                        accentColor: Qt.darker(StyleTokens.error, 1.15)
                        Layout.preferredWidth: 120
                        Layout.preferredHeight: 28
                        onClicked: {
                            if (!backend || !backend.modpackImporter) return
                            root._cancelRequested = true
                            backend.modpackImporter.cancelImport()  // 后端自动回滚已写入文件
                        }
                    }

                    // 关闭条目（结果态）
                    ShadowIconButton {
                        visible: root._entryState >= 2
                        source: "icons/lucide/x.svg"
                        sourceWidth: 12; sourceHeight: 12
                        onClicked: root._entryDismissed = true
                    }
                }

                // ── 分步指示器（① 阶段差异：后端 6 步合并 5 阶段展示）──
                ModpackStepIndicator {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 56
                    steps: root._steps
                    activeStep: root._entryState === 2 ? 0 : root._activeStep
                    doneCount: root._entryState === 2 ? root._steps.length : root._doneCount
                    failedStep: root._entryState === 3 ? root._failedStep : 0
                    running: root._entryState === 1
                    progress: root._progress   // 轨道填充与后端总进度同步
                }

                // ── 全局进度（总进度由后端计算下发，QML 仅绑定）──
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 5

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: root._stepName || qsTr("准备中…")
                            font.pixelSize: StyleTokens.fontSizeSm
                            font.bold: true
                            color: root._activeStep > 0 ? StyleTokens.accentLink : StyleTokens.textSecondary
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Text {
                            text: Math.round(root._progress * 100) + "%"
                            font.pixelSize: StyleTokens.fontSizeXs
                            color: StyleTokens.textTertiary
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 5
                        radius: 2
                        color: StyleTokens.bgElevated

                        Rectangle {
                            width: parent.width * Math.max(0.0, Math.min(1.0, root._progress))
                            height: 5
                            radius: 2
                            color: root._entryState === 3 ? StyleTokens.errorLight : StyleTokens.accent
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

                // ── 解析信息预览（② 解析与导入一体化，面板挂载页顶即时渲染）──
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

                // ── 模组下载列表（手写 Delegate 复刻 DownloadQueueCard 视觉）──
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 5

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: root._parsed
                                ? qsTr("在线模组（%1）").arg(modListModel.count)
                                : qsTr("在线模组")
                            font.pixelSize: StyleTokens.fontSizeXs
                            font.bold: true
                            color: StyleTokens.textTertiary
                        }
                        Text {
                            visible: root._failedCount() > 0
                            text: qsTr("%1 个失败").arg(root._failedCount())
                            font.pixelSize: StyleTokens.fontSizeXs
                            color: StyleTokens.errorLight
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: qsTr("悬停失败条目查看详细报错")
                            font.pixelSize: StyleTokens.fontSizeXs
                            color: StyleTokens.textMuted
                            visible: root._failedCount() > 0
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(150, Math.max(56, modListModel.count * 52))
                        radius: StyleTokens.radiusMd
                        color: "transparent"
                        clip: true
                        visible: modListModel.count > 0

                        ListView {
                            id: modListView
                            anchors.fill: parent
                            model: modListModel
                            spacing: 4
                            clip: true
                            delegate: modItemDelegate
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                        }
                    }

                    // 空态提示
                    RowLayout {
                        Layout.fillWidth: true
                        visible: modListModel.count === 0

                        LoadingSpinner {
                            Layout.preferredWidth: 14
                            Layout.preferredHeight: 14
                            running: root._entryState === 1 && !root._parsed
                        }
                        Text {
                            text: !root._parsed
                                ? qsTr("正在准备模组下载清单…")
                                : qsTr("该整合包不含需要在线下载的模组")
                            font.pixelSize: StyleTokens.fontSizeXs
                            color: StyleTokens.textMuted
                        }
                        Item { Layout.fillWidth: true }
                    }
                }

                // ── 实时日志容器（与页面卡片列表同风格，自动滚动可暂停）──
                ColumnLayout {
                    Layout.fillWidth: true
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
                            Layout.preferredWidth: 66
                            Layout.preferredHeight: 20
                            radius: 10
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
                                    importLogView.positionViewAtEnd()
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 104
                        radius: StyleTokens.radiusMd
                        color: StyleTokens.bgInput
                        border.color: StyleTokens.border
                        border.width: 1
                        clip: true
                        visible: logModel.count > 0

                        ListView {
                            id: importLogView
                            anchors.fill: parent
                            anchors.margins: 6
                            model: logModel
                            spacing: 3
                            clip: true
                            delegate: Text {
                                width: importLogView.width - 12
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

                // ── 失败 / 取消错误条（悬浮查看完整详情）──
                RowLayout {
                    Layout.fillWidth: true
                    visible: root._entryState >= 3
                    spacing: 8

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 30
                        radius: StyleTokens.radiusSm
                        color: root._entryState === 4 ? StyleTokens.warningBg : StyleTokens.errorBg
                        border.color: root._entryState === 4 ? StyleTokens.warning : StyleTokens.errorLight
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 6

                            Text {
                                text: root._entryState === 4 ? qsTr("已取消") : qsTr("导入失败")
                                font.pixelSize: StyleTokens.fontSizeXs
                                font.bold: true
                                color: root._entryState === 4 ? StyleTokens.warning : StyleTokens.errorLight
                            }
                            Text {
                                text: root._entryState === 4
                                    ? qsTr("本次写入的文件已自动清理")
                                    : (root._errorText || qsTr("未知错误"))
                                font.pixelSize: StyleTokens.fontSizeXs
                                color: StyleTokens.textSecondary
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Text {
                                visible: root._entryState === 3
                                text: qsTr("悬浮查看详情")
                                font.pixelSize: StyleTokens.fontSizeXs
                                color: StyleTokens.errorLight
                            }
                        }

                        // 悬浮查看完整错误（Popup 不参与布局，跟随鼠标显示）
                        ToolTip {
                            visible: root._entryState === 3 && errorHover.containsMouse && root._errorText.length > 0
                            text: root._errorText || ""
                            delay: 300
                            contentItem: Text {
                                text: root._errorText || ""
                                color: StyleTokens.textSecondary
                                font.pixelSize: StyleTokens.fontSizeXs
                                wrapMode: Text.Wrap
                                width: 400
                            }
                            background: Rectangle {
                                color: StyleTokens.surfaceLight
                                border.color: StyleTokens.borderLight
                                border.width: 1
                                radius: StyleTokens.radiusSm
                            }
                        }
                        MouseArea {
                            id: errorHover
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                        }
                    }
                }
            }
        }

        // ═══════ 原有下载任务卡片列表（行为不变）═══════
        ListView {
            id: cardsView
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.topMargin: root._entryState === 0 || root._entryDismissed ? 16 : 8
            Layout.bottomMargin: 8
            spacing: 8
            clip: true

            // 消除滚动条
            ScrollBar.vertical: null

            model: backend ? backend.installCardsModel : null
            delegate: DownloadQueueCard {
                width: cardsView.width
            }

            footer: Item {
                width: ListView.view.width
                height: ListView.view.height > 0 ? Math.max(ListView.view.height - 60, 0) : 250
                visible: !backend || !backend.installing

                Column {
                    anchors.centerIn: parent
                    spacing: 16

                    Image {
                        anchors.horizontalCenter: parent.horizontalCenter
                        source: "icons/lucide/download-cloud.svg"
                        width: 64; height: 64
                        sourceSize.width: 64; sourceSize.height: 64
                        opacity: 0.3
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("暂无下载任务")
                        font.pixelSize: StyleTokens.fontSizeMd
                        color: StyleTokens.textMuted
                    }
                }
            }
        }
    }

    // ═══════════ 模组下载条目（严格复刻 DownloadQueueCard 视觉语言）═══════════
    Component {
        id: modItemDelegate
        Rectangle {
            id: itemBox
            width: ListView.view.width
            height: 48
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
                    spacing: 4

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

                    // ── 进度条（4px，与 DownloadQueueCard 同款）──
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 4
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
