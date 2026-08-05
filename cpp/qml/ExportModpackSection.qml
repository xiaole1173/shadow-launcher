// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
import Qt.labs.platform

/// 整合包导出（版本设置独立分区，Section 7）
/// 后端 ModpackExporter 全程 worker 线程（哈希/双平台查询/打包），信号回主线程，
/// UI 零阻塞。复用 InputBox/ShadowSwitch/ShadowDropdown/ShadowButton 通用元件。
Item {
    id: root

    // ── 外部注入 ──
    property var backend: null
    property var toastManager: null
    property string versionId: ""
    property string versionName: ""

    // ── 表单状态 ──
    property string _packName: ""
    property string _packVersion: "1.0.0"
    property string _format: "modrinth"       // modrinth | curseforge
    property bool _modrinthOnly: false        // ModrinthUploadMode（同主流启动器）
    property bool _hostedAssetsOnly: false    // 仅打包包内资源（不联网查询，主流启动器 CheckAdvancedInclude）
    property bool _includeJava: false         // 打包便携 Java（主流启动器 IncludeJava）
    property bool _includeConfig: true
    property bool _includeSaves: false
    property bool _includeResourcepacks: true
    property bool _includeShaderpacks: true
    property var _saves: []                    // 版本下全部存档
    property var _selectedSaves: []            // 勾选的存档
    property string _savePath: ""
    property bool _busy: false
    property real _progress: 0
    property string _statusText: ""
    property bool _done: false

    /// 联网查询失败请求确认（顶层 ConfirmDialog 处理，避免 opened 绑定覆盖赋值）
    signal lookupDecisionRequested(string message)

    // backend/toastManager 由 MainWindow 在 Loader onLoaded 时注入（晚于本组件 onCompleted），
    // 注入完成前 _loadSaves 会挂起，这里补执行
    property bool _pendingSavesLoad: false
    onBackendChanged: {
        if (backend && backend.modpackExporter && _pendingSavesLoad) {
            _pendingSavesLoad = false
            _loadSaves()
        }
    }

    onVersionNameChanged: {
        if (!_packName.length) _packName = versionName
        _resetSavePath()
    }

    // 切换版本：重载存档列表、清空勾选（版本变了存档列表随之变化）
    onVersionIdChanged: {
        _loadSaves()
        _selectedSaves = []
    }

    function _resetSavePath() {
        var dlDir = StandardPaths.writableLocation(StandardPaths.DownloadLocation)
        if (!dlDir) dlDir = StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
        var ext = _format === "curseforge" ? ".zip" : ".mrpack"
        _savePath = (dlDir ? dlDir + "/" : "") + (versionName || "modpack") + ext
    }

    function _loadSaves() {
        if (!backend || !backend.modpackExporter) {
            _pendingSavesLoad = true
            return
        }
        _pendingSavesLoad = false
        _saves = backend.modpackExporter.listSaves(versionId) || []
        _selectedSaves = []
    }

    // ── 后端信号（声明式 Connections）──
    // 关键：不能用 onCompleted 里 connect——组件 onCompleted 执行时 MainWindow 还没
    // 注入 backend（Loader onLoaded 才赋值），连接会被整体跳过；Connections 的
    // target 用绑定表达式，backend 注入后自动生效，根治"导出无任何反馈"
    Connections {
        target: backend && backend.modpackExporter ? backend.modpackExporter : null
        function onBusyChanged() {
            root._busy = backend.modpackExporter.busy
        }
        function onProgressChanged() {
            root._progress = backend.modpackExporter.progress
            root._statusText = backend.modpackExporter.statusText
        }
        function onFinished(success, outPath, error) {
            root._busy = false
            if (success) {
                root._progress = 1
                root._statusText = qsTr("导出完成") + " " + error
                root._done = true
                if (root.toastManager) root.toastManager.show(qsTr("整合包已导出: ") + outPath)
            } else {
                root._progress = 0
                root._done = false
                root._statusText = qsTr("导出失败: ") + (error || qsTr("未知错误"))
                if (root.toastManager) root.toastManager.show(qsTr("导出失败: ") + (error || qsTr("未知错误")), 5000)
            }
        }
        // ── 联网查询失败 → 通知顶层弹窗询问是否继续（同主流启动器）──
        function onLookupFailed(platform, detail) {
            root.lookupDecisionRequested(detail)
        }
    }

    Component.onCompleted: {
        _loadSaves()   // backend 未注入时挂起，onBackendChanged 补载
    }

    // 点『导出』：校验后弹保存位置窗（主流启动器 交互），确认后真正开始
    function _startExport() {
        if (!backend || !backend.modpackExporter) {
            if (root.toastManager) root.toastManager.show(qsTr("导出模块未就绪，请稍后重试"), 3000)
            console.info("[export] modpackExporter is null")
            return
        }
        if (_busy) return
        if (!_packName.trim()) {
            if (root.toastManager) root.toastManager.show(qsTr("请填写整合包名称"), 3000)
            return
        }
        // ModrinthUploadMode 强制 Modrinth 格式（同主流启动器）
        if (_modrinthOnly) _format = "modrinth"
        // 默认路径：下载目录 + 名称 + 后缀（打开对话框的初始位置）
        var ext = _format === "curseforge" ? ".zip" : ".mrpack"
        var dlDir = StandardPaths.writableLocation(StandardPaths.DownloadLocation)
        if (!dlDir) dlDir = StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
        var defaultPath = (dlDir ? dlDir + "/" : "") + (_packName.trim() || "modpack") + ext
        exportFileDialog.currentFile = "file:///" + defaultPath.replace(/\\/g, "/")
        exportFileDialog.open()
    }

    // 保存窗确认 → 开始导出（真正入口）
    function _doExport(path) {
        var e = backend ? backend.modpackExporter : null
        if (!e) {
            if (root.toastManager) root.toastManager.show(qsTr("导出模块未就绪，请稍后重试"), 3000)
            console.log("[export] _doExport: modpackExporter is null")
            return
        }
        var ext = _format === "curseforge" ? ".zip" : ".mrpack"
        if (!path.toLowerCase().endsWith(ext)) path += ext
        root._savePath = path
        root._done = false
        root._progress = 0
        root._statusText = ""
        var fmt = _format === "curseforge" ? 1 : 0
        console.info("[export] start: " + path)
        e.exportVersion(versionId, _packName.trim(), _packVersion.trim(),
                        _includeConfig, _selectedSaves,
                        _includeResourcepacks, _includeShaderpacks,
                        _modrinthOnly, _hostedAssetsOnly, _includeJava, fmt, path)
    }

    // 内容可滚动：分区高度有限，存档子项展开/矮窗口时避免裁切
    ScrollView {
        id: exportScroll
        anchors.fill: parent
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        // 显式实例化滚动条（拿 id 供内容区留白）：overlay 模式浮在内容上会遮挡右侧，
        // 内容宽度减滚动条宽度后，滚动条落在独立留白区，不遮内容
        ScrollBar.vertical: ScrollBar {
            id: exportVBar
            policy: ScrollBar.AsNeeded
        }
        ColumnLayout {
            id: exportCol
            width: exportScroll.availableWidth - exportVBar.width
            // 高度自适应：内容矮时不滚动（撑满），内容高时随 Flickable 滚动
            height: Math.max(exportScroll.availableHeight, exportCol.implicitHeight)
            // 显式同步 Flickable contentHeight 到「实际布局高度」——不能绑 implicitHeight：
            // ColumnLayout.implicitHeight 会漏掉显式 height 的子项（实测 218 vs 实际 374），
            // 滚动范围偏小导致滚不到底部；childrenRect.height 是布局后的真实内容高度
            Binding {
                target: exportScroll.contentItem
                property: "contentHeight"
                value: exportCol.childrenRect.height
            }
            spacing: 12

        // ── 标题 ──
        Text {
            text: qsTr("导出整合包")
            font.pixelSize: StyleTokens.fontSizeMd
            font.bold: true
            color: StyleTokens.textSecondary
        }
        Text {
            text: qsTr("将当前版本打包为整合包：Modrinth (.mrpack) 或 CurseForge (.zip)。本地模组将自动联网匹配在线来源（Modrinth SHA1 + CurseForge 指纹），匹配成功以引用形式打包，否则原文件直装。全程后台执行，不阻塞其他操作。")
            font.pixelSize: StyleTokens.fontSizeSm
            color: StyleTokens.textTertiary
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        // ── 基本信息 ──
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6
                Text { text: qsTr("整合包名称"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textSecondary }
                InputBox {
                    Layout.fillWidth: true
                    text: root._packName
                    placeholderText: root.versionName || qsTr("输入整合包名称")
                    enabled: !root._busy
                    onTextChanged: root._packName = text
                }
            }
            ColumnLayout {
                Layout.preferredWidth: 130
                spacing: 6
                Text { text: qsTr("整合包版本"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textSecondary }
                InputBox {
                    Layout.fillWidth: true
                    text: root._packVersion
                    placeholderText: "1.0.0"
                    enabled: !root._busy
                    onTextChanged: root._packVersion = text
                }
            }
            ColumnLayout {
                Layout.preferredWidth: 170
                spacing: 6
                Text { text: qsTr("导出格式"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textSecondary }
                ShadowDropdown {
                    Layout.fillWidth: true
                    model: [
                        { value: "modrinth", label: qsTr("Modrinth (.mrpack)") },
                        { value: "curseforge", label: qsTr("CurseForge (.zip)") }
                    ]
                    currentValue: root._format
                    enabled: !root._busy
                    onValueSelected: function(v) {
                        var ext = v === "curseforge" ? ".zip" : ".mrpack"
                        if (root._savePath) {
                            // 保留目录，只换后缀（用户已选过路径时不重置）
                            root._savePath = root._savePath.replace(/\.(mrpack|zip)$/i, "") + ext
                        }
                        root._format = v
                    }
                }
            }
        }

        Item { height: 6; width: 1 }

        // ── 导出内容 ──
        Text {
            text: qsTr("导出内容")
            font.pixelSize: StyleTokens.fontSizeXs
            color: "#9ca0b4"
            font.letterSpacing: 1.5
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            // 模组：必含（只读）
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 38
                radius: StyleTokens.radiusMd
                color: StyleTokens.bgCard
                border.color: StyleTokens.bgElevated
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14; anchors.rightMargin: 14
                    spacing: 8
                    Text {
                        text: qsTr("模组 (mods/)")
                        color: StyleTokens.textPrimary
                        font.pixelSize: StyleTokens.fontSizeSm
                        Layout.fillWidth: true
                    }
                    Text {
                        text: qsTr("必含")
                        color: StyleTokens.textMuted
                        font.pixelSize: StyleTokens.fontSizeXs
                        font.bold: true
                    }
                    Text {
                        text: qsTr("· 已禁用的模组 (.disabled) 自动排除")
                        color: StyleTokens.textTertiary
                        font.pixelSize: StyleTokens.fontSizeXs
                    }
                }
            }

            // config
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                Text {
                    text: qsTr("配置文件 (config/)")
                    color: StyleTokens.textSecondary
                    font.pixelSize: StyleTokens.fontSizeSm
                    Layout.fillWidth: true
                }
                ShadowSwitch {
                    checked: root._includeConfig
                    enabled: !root._busy
                    onToggled: root._includeConfig = checked
                }
            }
            // 存档（含子项展开）
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                Text {
                    text: qsTr("存档 (saves/)")
                    color: StyleTokens.textSecondary
                    font.pixelSize: StyleTokens.fontSizeSm
                    Layout.fillWidth: true
                }
                ShadowSwitch {
                    checked: root._includeSaves
                    enabled: !root._busy
                    onToggled: {
                        root._includeSaves = checked
                        if (checked && root._saves.length === 0) root._loadSaves()
                        if (!checked) root._selectedSaves = []
                    }
                }
            }
            // 存档子项（勾选展开）
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: savesList.count > 0 ? Math.min(savesList.count * 30 + 12, 132) : 0
                radius: StyleTokens.radiusMd
                color: StyleTokens.bgCard
                border.color: StyleTokens.bgElevated
                visible: root._includeSaves
                clip: true
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 6
                    spacing: 2
                    Text {
                        visible: root._saves.length === 0
                        text: qsTr("该版本暂无存档")
                        color: StyleTokens.textMuted
                        font.pixelSize: StyleTokens.fontSizeXs
                        Layout.fillWidth: true
                        Layout.preferredHeight: 24
                        verticalAlignment: Text.AlignVCenter
                    }
                    Repeater {
                        id: savesList
                        model: root._saves
                        delegate: RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 28
                            spacing: 8
                            Text {
                                text: modelData
                                color: StyleTokens.textSecondary
                                font.pixelSize: StyleTokens.fontSizeSm
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }
                            ShadowSwitch {
                                checked: root._selectedSaves.indexOf(modelData) >= 0
                                enabled: !root._busy
                                onToggled: {
                                    var arr = root._selectedSaves.slice()
                                    var idx = arr.indexOf(modelData)
                                    if (checked && idx < 0) arr.push(modelData)
                                    if (!checked && idx >= 0) arr.splice(idx, 1)
                                    root._selectedSaves = arr
                                }
                            }
                        }
                    }
                }
            }
            // 资源包
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                Text {
                    text: qsTr("资源包 (resourcepacks/)")
                    color: StyleTokens.textSecondary
                    font.pixelSize: StyleTokens.fontSizeSm
                    Layout.fillWidth: true
                }
                ShadowSwitch {
                    checked: root._includeResourcepacks
                    enabled: !root._busy
                    onToggled: root._includeResourcepacks = checked
                }
            }
            // 光影
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                Text {
                    text: qsTr("光影 (shaderpacks/)")
                    color: StyleTokens.textSecondary
                    font.pixelSize: StyleTokens.fontSizeSm
                    Layout.fillWidth: true
                }
                ShadowSwitch {
                    checked: root._includeShaderpacks
                    enabled: !root._busy
                    onToggled: root._includeShaderpacks = checked
                }
            }
        }

        Item { height: 6; width: 1 }

        // ── 高级 ──
        Text {
            text: qsTr("高级")
            font.pixelSize: StyleTokens.fontSizeXs
            color: "#9ca0b4"
            font.letterSpacing: 1.5
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            Text {
                text: qsTr("仅使用 Modrinth 资源（Modrinth 上传模式）")
                color: StyleTokens.textSecondary
                font.pixelSize: StyleTokens.fontSizeSm
                Layout.fillWidth: true
            }
            ShadowSwitch {
                checked: root._modrinthOnly
                enabled: !root._busy && !root._hostedAssetsOnly
                onToggled: {
                    root._modrinthOnly = checked
                    if (checked) {
                        root._format = "modrinth"
                        root._resetSavePath()
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            Text {
                text: qsTr("仅打包包内资源（跳过联网查询）")
                color: StyleTokens.textSecondary
                font.pixelSize: StyleTokens.fontSizeSm
                Layout.fillWidth: true
            }
            ShadowSwitch {
                checked: root._hostedAssetsOnly
                enabled: !root._busy
                onToggled: {
                    root._hostedAssetsOnly = checked
                    // 与 主流启动器 一致：勾选后禁止 Modrinth 上传模式
                    if (checked) root._modrinthOnly = false
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            Text {
                text: qsTr("包含 Java 运行时")
                color: StyleTokens.textSecondary
                font.pixelSize: StyleTokens.fontSizeSm
                Layout.fillWidth: true
            }
            ShadowSwitch {
                checked: root._includeJava
                enabled: !root._busy
                onToggled: root._includeJava = checked
            }
        }

        Item { height: 6; width: 1 }

        Item { height: 6; width: 1 }

        // ── 操作 ──
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Item { Layout.fillWidth: true }
            ShadowButton {
                text: root._busy ? qsTr("取消导出") : qsTr("导出")
                btnWidth: 140
                z: 10
                onClicked: {
                    console.info("[export] btn clicked, busy=" + root._busy)
                    if (root._busy) {
                        if (backend && backend.modpackExporter) backend.modpackExporter.cancel()
                    } else {
                        root._startExport()
                    }
                }
            }
            ShadowButton {
                text: qsTr("打开文件夹")
                btnWidth: 110
                outlined: true
                visible: root._done
                onClicked: {
                    if (backend && backend.openPath) backend.openPath(root._savePath)
                }
            }
        }

        // ── 进度 ──
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6
            visible: root._busy || root._progress > 0
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 8
                radius: StyleTokens.radiusSm
                color: StyleTokens.bgInput
                Rectangle {
                    width: parent.width * Math.min(root._progress, 1.0)
                    height: 8
                    radius: StyleTokens.radiusSm
                    color: root._progress >= 1.0 ? StyleTokens.success : "#6080e8"
                    Behavior on width { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                }
            }
            Text {
                Layout.fillWidth: true
                text: root._statusText
                color: root._progress >= 1.0 ? StyleTokens.success : StyleTokens.textTertiary
                font.pixelSize: StyleTokens.fontSizeXs
                elide: Text.ElideRight
            }
        }
    }
    }

    // ── 保存位置选择 ──
    FileDialog {
        id: exportFileDialog
        fileMode: FileDialog.SaveFile
        title: qsTr("保存整合包")
        nameFilters: root._format === "curseforge"
            ? [qsTr("CurseForge 整合包 (*.zip)"), qsTr("所有文件 (*.*)")]
            : [qsTr("Modrinth 整合包 (*.mrpack)"), qsTr("所有文件 (*.*)")]
        onAccepted: {
            // 显式 id 访问（信号处理器作用域歧义防护）+ 防御式路径转换：
            // selectedFile 在不同 Qt 版本/对话框实现下可能是 QUrl 或带 file:/// 前缀的字符串
            var sel = exportFileDialog.selectedFile
            var p = ""
            if (typeof sel === "string") {
                p = sel
            } else if (sel && typeof sel.toString === "function") {
                p = sel.toString()
            }
            p = String(p).replace(/^(file:\/{2,3})/i, "")
            console.info("[export] onAccepted path=" + p)
            root._doExport(p)
        }
        onRejected: { /* 用户取消选择：不导出 */ }
    }
}
