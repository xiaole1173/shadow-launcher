// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
import Qt.labs.platform as Platform

/// 整合包导出（版本设置独立分区，Section 7）——完全对齐主流启动器实现 PageInstanceExport：
/// 规则驱动选项（C++ exportContext 动态渲染，按版本实际情况显隐）、子项勾选、
/// 配置保存/读取、隐私项默认不勾。后端 ModpackExporter 全程 worker 线程，UI 零阻塞。
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
    property bool _hostedAssetsOnly: false    // 仅打包包内资源（主流启动器 CheckAdvancedInclude）
    property bool _includeJava: false         // 打包便携 Java（主流启动器 IncludeJava）
    property var _saves: []                   // 版本下全部存档 [{name, modified}]
    property var _selectedSaves: []           // 勾选的存档名
    property var _selectedRp: ({})            // 资源包子项勾选 {name: bool}
    property var _selectedShaders: ({})      // 光影子项勾选 {name: bool}
    property var _extraFiles: []              // 追加内容绝对路径（配置读取）
    property var _rulesOverride: []           // 配置读取的自定义规则（主流启动器 RulesOverrides 覆盖模式）
    property string _configPackPath: ""      // 配置指定的输出路径（非空时导出不弹保存窗，主流启动器 PackPath）
    property string _savePath: ""
    property bool _busy: false
    property real _progress: 0
    property string _statusText: ""
    property bool _done: false

    // 导出上下文 + 选项勾选（主流启动器 ExportOption 集合，C++ optionDefs 驱动）
    property var _ctx: ({})
    property var _checked: ({})               // {optionId: bool}
    property bool _pendingSavesLoad: false

    /// 联网查询失败请求确认（顶层 ConfirmDialog 处理）
    signal lookupDecisionRequested(string message)

    // backend/toastManager 由 MainWindow 在 Loader onLoaded 时注入（晚于本组件 onCompleted）
    onBackendChanged: {
        if (backend && backend.modpackExporter) {
            if (_pendingSavesLoad) {
                _pendingSavesLoad = false
                _loadSaves()
            }
            _loadCtx()
        }
    }
    // 每次进入导出分区刷新上下文（外部可能增删了 mods/config 等目录）
    onVisibleChanged: {
        if (visible) _loadCtx()
    }

    onVersionNameChanged: {
        if (!_packName.length) _packName = versionName
        _resetSavePath()
    }

    // 切换版本：重载上下文与存档、重置勾选（含子项）
    onVersionIdChanged: {
        _loadSaves()
        _selectedSaves = []
        _rulesOverride = []
        _loadCtx()
        _initChecked(true)
    }

    function _resetSavePath() {
        // writableLocation 返回 QUrl（file:/// 前缀）；_savePath 需要纯本地路径
        var dl = String(Platform.StandardPaths.writableLocation(Platform.StandardPaths.DownloadLocation) || "")
        if (dl.length === 0) dl = String(Platform.StandardPaths.writableLocation(Platform.StandardPaths.DocumentsLocation) || "")
        dl = dl.replace(/^file:\/{2,3}/i, "")
        var ext = _format === "curseforge" ? ".zip" : ".mrpack"
        _savePath = (dl ? dl + "/" : "") + (versionName || "modpack") + ext
    }

    function _loadSaves() {
        if (!backend || !backend.modpackExporter) {
            _pendingSavesLoad = true
            return
        }
        _pendingSavesLoad = false
        _saves = backend.modpackExporter.listSaves(versionId) || []
        // 同主流启动器：存档子项默认全勾（勾选主选项即导出全部存档，可单独取消）
        var all = []
        for (var i = 0; i < _saves.length; i++) all.push(_saves[i].name)
        _selectedSaves = all
    }

    // 拉取当前版本导出上下文（同步快操作：版本 JSON + 目录存在性 + 选项可见性）
    function _loadCtx() {
        if (backend && backend.modpackExporter && versionId) {
            _ctx = backend.modpackExporter.exportContext(versionId) || {}
        } else {
            _ctx = {}
        }
        _initChecked()
        console.info("[export] ctx loaded version=" + versionId
                     + " opts=" + (_ctx.options || []).length
                     + " hasSaves=" + _ctx.hasSaves
                     + " saves=" + _saves.length)
    }

    // 初始化选项勾选：可见选项按 defaultChecked；保留用户已改的勾选（切版本时重置）
    function _initChecked(forceReset) {
        var opts = _ctx.options || []
        var next = {}
        for (var i = 0; i < opts.length; i++) {
            var id = opts[i].id
            if (forceReset || _checked[id] === undefined)
                next[id] = opts[i].defaultChecked
            else
                next[id] = _checked[id]
        }
        _checked = next
        if (forceReset) {
            _selectedRp = {}
            _selectedShaders = {}
        }
    }

    // 组装勾选选项（含子项 id:name / id:dir:name；子项仅在父选项勾选时生效）
    function _buildCheckedOptions() {
        var arr = []
        for (var id in _checked) if (_checked[id]) arr.push(id)
        if (_checked["resourcepacks"] === true) {
            var rps = _ctx.rpItems || []
            for (var i = 0; i < rps.length; i++) {
                if (_selectedRp[rps[i].name] === false) continue
                arr.push("resourcepacks:" + (rps[i].type === "dir" ? "dir:" : "") + rps[i].name)
            }
        }
        if (_checked["shaderpacks"] === true) {
            var shs = _ctx.shaderItems || []
            for (var j = 0; j < shs.length; j++) {
                if (_selectedShaders[shs[j].name] === false) continue
                arr.push("shaderpacks:" + (shs[j].type === "dir" ? "dir:" : "") + shs[j].name)
            }
        }
        return arr
    }

    // ── 后端信号（声明式 Connections；不能 onCompleted connect——backend 注入晚于组件创建）──
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
            } else if (error === "已取消") {
                // 用户取消：静默恢复（不弹“失败”误导）
                root._progress = 0
                root._statusText = qsTr("已取消导出")
            } else {
                root._progress = 0
                root._done = false
                root._statusText = qsTr("导出失败: ") + (error || qsTr("未知错误"))
                if (root.toastManager) root.toastManager.show(qsTr("导出失败: ") + (error || qsTr("未知错误")), 5000)
            }
        }
        function onLookupFailed(platform, detail) {
            // 导出分区不可见（用户已关闭浮层）时无法弹确认框 → 自动继续（未托管直装），防止 worker 死等
            if (root.visible) {
                root.lookupDecisionRequested(detail)
            } else if (backend && backend.modpackExporter) {
                backend.modpackExporter.continueAfterLookupFailure(true)
            }
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
        // 包名兜底：为空用版本名（同主流启动器 StartExport：空名用 HintText=版本名）
        var name = _packName.trim() || versionName || "modpack"
        if (_packName !== name) _packName = name
        // 配置指定输出路径（主流启动器 PackPath）：直接导出，不弹保存窗
        if (_configPackPath.length > 0) {
            root._doExport(_configPackPath)
            return
        }
        // ModrinthUploadMode 强制 Modrinth 格式（同主流启动器）
        if (_modrinthOnly) _format = "modrinth"
        // 预填默认文件名（native 对话框支持不存在的文件；同主流启动器体验）
        var ext = _format === "curseforge" ? ".zip" : ".mrpack"
        var dlUrl = Platform.StandardPaths.writableLocation(Platform.StandardPaths.DownloadLocation)
        if (!dlUrl || String(dlUrl).length === 0)
            dlUrl = Platform.StandardPaths.writableLocation(Platform.StandardPaths.DocumentsLocation)
        var defaultPath = (String(dlUrl || "").replace(/^file:\/{2,3}/i, "") || "")
            + "/" + (_packName.trim() || "modpack") + ext
        exportFileDialog.currentFile = "file:///" + defaultPath.replace(/\\/g, "/")
        exportFileDialog.open()
    }

    // 保存窗确认 → 开始导出（真正入口）
    function _doExport(path) {
        var e = backend ? backend.modpackExporter : null
        if (!e) {
            if (root.toastManager) root.toastManager.show(qsTr("导出模块未就绪，请稍后重试"), 3000)
            console.info("[export] _doExport: modpackExporter is null")
            return
        }
        if (!path || path.length === 0) {
            if (root.toastManager) root.toastManager.show(qsTr("保存路径无效，请重新选择"), 3000)
            console.info("[export] _doExport: empty path")
            return
        }
        // 相对路径防御：用户选择的路径必须是绝对路径（防落到启动器根目录）
        if (!path.includes(":/") && !path.startsWith("//")) {
            if (root.toastManager) root.toastManager.show(qsTr("保存路径无效，请选择完整路径"), 3000)
            console.info("[export] _doExport: relative path rejected: " + path)
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
                        _buildCheckedOptions(), _selectedSaves,
                        _modrinthOnly, _hostedAssetsOnly, _includeJava, fmt, path, _extraFiles, _rulesOverride)
    }

    // ── 配置保存/读取（主流启动器 export_config.txt 语义）──
    function _saveConfig() {
        var e = backend ? backend.modpackExporter : null
        if (!e) return
        var dlUrl = Platform.StandardPaths.writableLocation(Platform.StandardPaths.DownloadLocation)
        if (!dlUrl || String(dlUrl).length === 0)
            dlUrl = Platform.StandardPaths.writableLocation(Platform.StandardPaths.DocumentsLocation)
        if (dlUrl) configSaveDialog.currentFolder = dlUrl
        configSaveDialog.open()
    }
    // 清除配置影响（主流启动器 ResetConfigOverrides）：恢复界面勾选模式
    function _clearConfigOverride() {
        _rulesOverride = []
        _configPackPath = ""
        _extraFiles = []
        if (root.toastManager) root.toastManager.show(qsTr("已清除配置覆盖，恢复界面勾选"))
    }
    function _writeConfig(path) {
        var e = backend ? backend.modpackExporter : null
        if (!e) return
        var cfg = {
            name: _packName,
            version: _packVersion,
            includeJava: _includeJava,
            hostedAssetsOnly: _hostedAssetsOnly,
            modrinthUploadMode: _modrinthOnly,
            format: _format === "curseforge" ? 1 : 0,
            packPath: _savePath,
            options: _buildCheckedOptions(),
            unchecked: _uncheckedOptions(),
            extraFiles: _extraFiles
        }
        if (e.saveExportConfig(path, cfg)) {
            if (root.toastManager) root.toastManager.show(qsTr("导出配置已保存: ") + path)
        } else {
            if (root.toastManager) root.toastManager.show(qsTr("保存配置失败"), 3000)
        }
    }
    // 当前可见但未勾选的选项 id（配置还原用，主流启动器 读取后勾选状态与保存时一致）
    function _uncheckedOptions() {
        var arr = []
        var opts = _ctx.options || []
        for (var i = 0; i < opts.length; i++) {
            var id = opts[i].id
            if (!_checked[id]) arr.push(id)
        }
        return arr
    }
    function _loadConfig(path) {
        var e = backend ? backend.modpackExporter : null
        if (!e) return
        var cfg = e.loadExportConfig(path) || {}
        if (!cfg.options) {   // 空配置（文件不存在/无内容）
            if (root.toastManager) root.toastManager.show(qsTr("读取配置失败"), 3000)
            return
        }
        if (cfg.name) _packName = cfg.name
        if (cfg.version) _packVersion = cfg.version
        if (cfg.includeJava !== undefined) _includeJava = cfg.includeJava
        if (cfg.hostedAssetsOnly !== undefined) _hostedAssetsOnly = cfg.hostedAssetsOnly
        if (cfg.modrinthUploadMode !== undefined) _modrinthOnly = cfg.modrinthUploadMode
        if (cfg.format !== undefined) _format = cfg.format === 1 ? "curseforge" : "modrinth"
        var arr = cfg.options
        var next = {}
        for (var i = 0; i < arr.length; i++) next[arr[i]] = true
        // 未在配置里的可见选项保持默认（避免全部变未勾选）
        var opts = _ctx.options || []
        for (var j = 0; j < opts.length; j++)
            if (next[opts[j].id] === undefined) next[opts[j].id] = opts[j].defaultChecked
        _checked = next
        _extraFiles = cfg.extraFiles || []
        _configPackPath = cfg.packPath || ""
        // 未勾选选项还原（保存时记录的 UncheckedOptions）
        if (cfg.unchecked) {
            for (var u = 0; u < cfg.unchecked.length; u++)
                next[cfg.unchecked[u]] = false
            _checked = next
        }
        // 自定义规则覆盖模式（主流启动器 RulesOverrides：手工编辑的规则整体生效）
        if (cfg.rawRules && cfg.rawRules.length > 0) {
            _rulesOverride = cfg.rawRules
            if (root.toastManager) root.toastManager.show(qsTr("已读取导出配置（自定义规则生效）"))
        } else {
            _rulesOverride = []
            if (root.toastManager) root.toastManager.show(qsTr("已读取导出配置"))
        }
    }

    // 内容可滚动：分区高度有限，存档子项展开/矮窗口时避免裁切
    ScrollView {
        id: exportScroll
        anchors.fill: parent
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        // 显式实例化滚动条（拿 id 供内容区留白）：overlay 模式浮在内容上会遮挡右侧
        ScrollBar.vertical: ScrollBar {
            id: exportVBar
            policy: ScrollBar.AsNeeded
            // 内容不溢出时彻底隐藏（防残留灰色小点/残留 thumb）
            visible: exportScroll.contentItem
                     && exportScroll.contentItem.contentHeight > exportScroll.contentItem.height
        }
        ColumnLayout {
            id: exportCol
            // 滚动条可见才让出宽度；隐藏时全宽（避免内容无谓变窄）
            width: exportScroll.availableWidth
                   - (exportVBar.visible ? exportVBar.width : 0)
            height: Math.max(exportScroll.availableHeight, exportCol.implicitHeight)
            // contentHeight 绑「实际布局高度」（childrenRect）——implicitHeight 会漏显式 height 子项
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
            text: qsTr("将当前版本打包为整合包：Modrinth (.mrpack) 或 CurseForge (.zip)。模组将自动联网匹配在线来源（Modrinth SHA1 + CurseForge 指纹），匹配成功以引用形式打包，否则原文件直装。选项按版本实际情况显示；标记“默认不导出”的为个人数据，分享前请留意。全程后台执行，不阻塞其他操作。")
            font.pixelSize: StyleTokens.fontSizeSm
            color: StyleTokens.textTertiary
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        // ── 基本信息：整合包名称单独一行（长名称显示不下），版本+格式一行 ──
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
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
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
                            root._savePath = root._savePath.replace(/\.(mrpack|zip)$/i, "") + ext
                        }
                        root._format = v
                    }
                }
            }
        }

        Item { height: 6; width: 1 }

        // ── 导出内容（主流启动器 动态选项）──
        Text {
            text: qsTr("导出内容")
            font.pixelSize: StyleTokens.fontSizeXs
            color: "#9ca0b4"
            font.letterSpacing: 1.5
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            // 空状态：该版本没有任何可导出内容时给出提示（避免一片空白）
            Text {
                visible: (_ctx.options || []).length === 0
                text: qsTr("该版本暂无内容可导出（mods/config/存档等目录均为空）")
                color: StyleTokens.textMuted
                font.pixelSize: StyleTokens.fontSizeSm
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }

            // 动态选项（C++ exportContext 按版本实际可见性过滤；子项随父选项勾选显隐，同主流启动器）
            Repeater {
                model: _ctx.options || []
                delegate: RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 30
                    spacing: 8
                    visible: {
                        if (modelData.parent)
                            return root._checked[modelData.parent] === true
                        return true
                    }
                    Text {
                        text: (modelData.parent ? "     " : "") + modelData.title
                        color: StyleTokens.textSecondary
                        font.pixelSize: StyleTokens.fontSizeSm
                        Layout.fillWidth: true
                    }
                    Text {
                        text: modelData.description
                        color: StyleTokens.textTertiary
                        font.pixelSize: StyleTokens.fontSizeXs
                        visible: modelData.description && modelData.description.length > 0
                    }
                    Text {
                        text: qsTr("默认不导出")
                        color: "#b8860b"
                        font.pixelSize: StyleTokens.fontSizeXs
                        visible: modelData.privacy === true
                    }
                    ShadowSwitch {
                        checked: root._checked[modelData.id] !== undefined ? root._checked[modelData.id] : modelData.defaultChecked
                        enabled: !root._busy
                        onToggled: {
                            // 整体赋值：JS 对象属性突变不触发 QML 绑定更新（子项面板/子项随父显隐依赖它）
                            var next = {}
                            for (var k in root._checked) next[k] = root._checked[k]
                            next[modelData.id] = checked
                            root._checked = next
                            // 取消勾选存档/资源包/光影时清空对应子项选择
                            if (!checked && modelData.id === "saves") root._selectedSaves = []
                            if (!checked && modelData.id === "resourcepacks") root._selectedRp = {}
                            if (!checked && modelData.id === "shaderpacks") root._selectedShaders = {}
                        }
                    }
                }
            }

            // ── 存档子项（勾选 saves 时展开；同主流启动器 ReloadSubOptions，显示修改时间）──
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: savesSub.count > 0 ? Math.min(savesSub.count * 28 + 12, 140) : 0
                radius: StyleTokens.radiusMd
                color: StyleTokens.bgCard
                border.color: StyleTokens.bgElevated
                visible: root._checked["saves"] === true && !!_ctx.hasSaves
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
                        id: savesSub
                        model: root._saves
                        delegate: RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 26
                            spacing: 8
                            Text {
                                text: modelData.name
                                color: StyleTokens.textSecondary
                                font.pixelSize: StyleTokens.fontSizeSm
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }
                            Text {
                                text: modelData.modified
                                color: StyleTokens.textTertiary
                                font.pixelSize: StyleTokens.fontSizeXs
                            }
                            ShadowSwitch {
                                checked: root._selectedSaves.indexOf(modelData.name) >= 0
                                enabled: !root._busy
                                onToggled: {
                                    var arr = root._selectedSaves.slice()
                                    var idx = arr.indexOf(modelData.name)
                                    if (checked && idx < 0) arr.push(modelData.name)
                                    if (!checked && idx >= 0) arr.splice(idx, 1)
                                    root._selectedSaves = arr
                                }
                            }
                        }
                    }
                }
            }

            // ── 资源包子项 ──
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: rpSub.count > 0 ? Math.min(rpSub.count * 26 + 12, 140) : 0
                radius: StyleTokens.radiusMd
                color: StyleTokens.bgCard
                border.color: StyleTokens.bgElevated
                visible: root._checked["resourcepacks"] === true && (_ctx.rpItems || []).length > 0
                clip: true
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 6
                    spacing: 2
                    Repeater {
                        id: rpSub
                        model: _ctx.rpItems || []
                        delegate: RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 24
                            spacing: 8
                            Text {
                                text: (modelData.type === "dir" ? "📁 " : "🗜 ") + modelData.name
                                color: StyleTokens.textSecondary
                                font.pixelSize: StyleTokens.fontSizeSm
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }
                            ShadowSwitch {
                                checked: root._selectedRp[modelData.name] !== false
                                enabled: !root._busy
                                onToggled: {
                                    var next = {}
                                    for (var k in root._selectedRp) next[k] = root._selectedRp[k]
                                    next[modelData.name] = checked
                                    root._selectedRp = next
                                }
                            }
                        }
                    }
                }
            }

            // ── 光影子项 ──
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: shaderSub.count > 0 ? Math.min(shaderSub.count * 26 + 12, 140) : 0
                radius: StyleTokens.radiusMd
                color: StyleTokens.bgCard
                border.color: StyleTokens.bgElevated
                visible: root._checked["shaderpacks"] === true && (_ctx.shaderItems || []).length > 0
                clip: true
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 6
                    spacing: 2
                    Repeater {
                        id: shaderSub
                        model: _ctx.shaderItems || []
                        delegate: RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 24
                            spacing: 8
                            Text {
                                text: (modelData.type === "dir" ? "📁 " : "🗜 ") + modelData.name
                                color: StyleTokens.textSecondary
                                font.pixelSize: StyleTokens.fontSizeSm
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }
                            ShadowSwitch {
                                checked: root._selectedShaders[modelData.name] !== false
                                enabled: !root._busy
                                onToggled: {
                                    var next = {}
                                    for (var k in root._selectedShaders) next[k] = root._selectedShaders[k]
                                    next[modelData.name] = checked
                                    root._selectedShaders = next
                                }
                            }
                        }
                    }
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
        // 二次分发警告（勾选“仅打包包内资源”时提示，同主流启动器 CheckAdvancedInclude）
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            radius: StyleTokens.radiusMd
            color: "#3a2e12"
            border.color: "#6b5418"
            visible: root._hostedAssetsOnly
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12; anchors.rightMargin: 12
                spacing: 8
                Text {
                    text: qsTr("⚠ 打包资源文件可能违反部分 Mod 的使用协议，请尽量不要公开分发包含资源文件的整合包！")
                    color: "#d9b45a"
                    font.pixelSize: StyleTokens.fontSizeXs
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
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
            Text {
                text: qsTr("未找到可打包的 Java 运行时")
                color: "#b8860b"
                font.pixelSize: StyleTokens.fontSizeXs
                visible: _ctx && _ctx.javaAvailable === false
            }
            ShadowSwitch {
                checked: root._includeJava
                enabled: !root._busy && _ctx && _ctx.javaAvailable !== false
                onToggled: root._includeJava = checked
            }
        }

        Item { height: 6; width: 1 }

        // ── 导出配置（主流启动器 高级：保存/读取）──
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            ShadowButton {
                text: qsTr("保存配置")
                btnWidth: 120
                outlined: true
                enabled: !root._busy
                onClicked: root._saveConfig()
            }
            ShadowButton {
                text: qsTr("读取配置")
                btnWidth: 120
                outlined: true
                enabled: !root._busy
                onClicked: configOpenDialog.open()
            }
            ShadowButton {
                text: qsTr("清除覆盖")
                btnWidth: 120
                outlined: true
                enabled: !root._busy
                visible: root._rulesOverride.length > 0 || root._configPackPath.length > 0
                onClicked: root._clearConfigOverride()
            }
            Text {
                text: qsTr("配置文件可手工编辑规则段（! 反转、* ? [] 通配、\\ 结尾=目录）；含 PackPath 时导出不弹保存窗")
                color: StyleTokens.textTertiary
                font.pixelSize: StyleTokens.fontSizeXs
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }

        Item { height: 6; width: 1 }

        // ── 操作 ──
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Item { Layout.fillWidth: true }
            ShadowButton {
                text: root._busy ? qsTr("取消导出") : qsTr("导出")
                btnWidth: 140
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

    // ── 保存位置选择（native 对话框：支持预填不存在的默认文件名，同主流启动器）──
    Platform.FileDialog {
        id: exportFileDialog
        fileMode: Platform.FileDialog.SaveFile
        title: qsTr("保存整合包")
        defaultSuffix: root._format === "curseforge" ? "zip" : "mrpack"
        nameFilters: root._format === "curseforge"
            ? [qsTr("CurseForge 整合包 (*.zip)"), qsTr("所有文件 (*.*)")]
            : [qsTr("Modrinth 整合包 (*.mrpack)"), qsTr("所有文件 (*.*)")]
        onAccepted: {
            // 防御式路径转换：selectedFile 可能是 QUrl/字符串/空——空则依次回退
            // selectedFiles[0] → currentFile；全空则拒绝导出（防落到相对路径 .mrpack）
            var sel = exportFileDialog.selectedFile
            if (!sel || String(sel).length === 0) {
                var sfs = exportFileDialog.selectedFiles
                if (sfs && sfs.length > 0) sel = sfs[0]
            }
            if (!sel || String(sel).length === 0) {
                sel = exportFileDialog.currentFile
            }
            var p = ""
            if (typeof sel === "string") {
                p = sel
            } else if (sel && typeof sel.toString === "function") {
                p = sel.toString()
            }
            p = String(p).replace(/^(file:\/{2,3})/i, "")
            console.info("[export] onAccepted path=" + p + " (selType=" + typeof sel + ")")
            if (!p || p.length === 0) {
                if (root.toastManager) root.toastManager.show(qsTr("未能获取保存路径，请重试"), 3000)
                return
            }
            root._doExport(p)
        }
        onRejected: { /* 用户取消选择：不导出 */ }
    }

    // ── 配置保存 / 读取 ──
    Platform.FileDialog {
        id: configSaveDialog
        fileMode: Platform.FileDialog.SaveFile
        title: qsTr("保存导出配置")
        nameFilters: [qsTr("导出配置 (*.txt)"), qsTr("所有文件 (*.*)")]
        defaultSuffix: "txt"
        onAccepted: {
            var sel = configSaveDialog.selectedFile
            var p = ""
            if (typeof sel === "string") {
                p = sel
            } else if (sel && typeof sel.toString === "function") {
                p = sel.toString()
            }
            p = String(p).replace(/^(file:\/{2,3})/i, "")
            root._writeConfig(p)
        }
    }
    Platform.FileDialog {
        id: configOpenDialog
        title: qsTr("读取导出配置")
        nameFilters: [qsTr("导出配置 (*.txt)"), qsTr("所有文件 (*.*)")]
        onAccepted: {
            var sel = configOpenDialog.selectedFile
            var p = ""
            if (typeof sel === "string") {
                p = sel
            } else if (sel && typeof sel.toString === "function") {
                p = sel.toString()
            }
            p = String(p).replace(/^(file:\/{2,3})/i, "")
            root._loadConfig(p)
        }
    }
}
