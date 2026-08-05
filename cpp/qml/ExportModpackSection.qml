// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

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
    property string _lookupMessage: ""
    property bool _lookupOpen: false
    property bool _lookupAccepted: false

    onVersionNameChanged: {
        if (!_packName.length) _packName = versionName
        _resetSavePath()
    }

    function _resetSavePath() {
        var dlDir = Qt.StandardPaths.writableLocation(Qt.StandardPaths.DownloadLocation)
        if (!dlDir) dlDir = Qt.StandardPaths.writableLocation(Qt.StandardPaths.DocumentsLocation)
        var ext = _format === "curseforge" ? ".zip" : ".mrpack"
        _savePath = (dlDir ? dlDir + "/" : "") + (versionName || "modpack") + ext
    }

    function _loadSaves() {
        if (backend && backend.modpackExporter) {
            _saves = backend.modpackExporter.listSaves(versionId) || []
            _selectedSaves = []
        }
    }

    Component.onCompleted: {
        if (backend && backend.modpackExporter) {
            var e = backend.modpackExporter
            e.busyChanged.connect(function() { root._busy = e.busy })
            e.progressChanged.connect(function() {
                root._progress = e.progress
                root._statusText = e.statusText
            })
            e.finished.connect(function(ok, out, err) {
                root._busy = false
                if (ok) {
                    root._progress = 1
                    root._statusText = qsTr("导出完成") + " " + err
                    root._done = true
                    if (root.toastManager) root.toastManager.show(qsTr("整合包已导出: ") + out)
                } else {
                    root._progress = 0
                    root._done = false
                    root._statusText = qsTr("导出失败: ") + (err || qsTr("未知错误"))
                    if (root.toastManager) root.toastManager.show(qsTr("导出失败: ") + (err || qsTr("未知错误")), 5000)
                }
            })
            // ── 联网查询失败 → 弹窗询问是否继续（同主流启动器）──
            e.lookupFailed.connect(function(platform, detail) {
                root._lookupMessage = detail
                root._lookupOpen = true
            })
        }
        _loadSaves()
    }

    function _startExport() {
        var e = backend ? backend.modpackExporter : null
        if (!e || _busy) return
        if (!_packName.trim() || !_savePath.trim()) {
            if (root.toastManager) root.toastManager.show(qsTr("请填写整合包名称并选择保存位置"), 3000)
            return
        }
        // ModrinthUploadMode 强制 Modrinth 格式（同主流启动器）
        if (_modrinthOnly) _format = "modrinth"
        var ext = _format === "curseforge" ? ".zip" : ".mrpack"
        var path = _savePath
        if (!path.toLowerCase().endsWith(ext)) path += ext
        root._savePath = path
        root._done = false
        root._progress = 0
        root._statusText = ""
        var fmt = _format === "curseforge" ? 1 : 0
        e.exportVersion(versionId, _packName.trim(), _packVersion.trim(),
                        _includeConfig, _selectedSaves,
                        _includeResourcepacks, _includeShaderpacks,
                        _modrinthOnly, _hostedAssetsOnly, _includeJava, fmt, path)
    }

    ColumnLayout {
        anchors.fill: parent
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
                        root._format = v
                        root._resetSavePath()
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

        // ── 保存位置 ──
        Text {
            text: qsTr("保存位置")
            font.pixelSize: StyleTokens.fontSizeXs
            color: "#9ca0b4"
            font.letterSpacing: 1.5
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                radius: StyleTokens.radiusMd
                color: StyleTokens.bgInput
                border.color: StyleTokens.border
                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 10; anchors.rightMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    text: root._savePath
                    color: StyleTokens.textSecondary
                    font.pixelSize: StyleTokens.fontSizeXs
                    elide: Text.ElideMiddle
                    verticalAlignment: Text.AlignVCenter
                }
            }
            ShadowButton {
                text: qsTr("选择...")
                btnWidth: 88
                outlined: true
                enabled: !root._busy
                onClicked: exportFileDialog.open()
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

    // ── 联网查询失败确认（主流启动器 弹窗询问是否继续）──
    ConfirmDialog {
        title: qsTr("联网获取文件信息失败")
        message: root._lookupMessage
        opened: root._lookupOpen
        onAccept: {
            // ConfirmDialog 确认按钮会先置 opened=false（触发 closed）再调 onAccept——
            // 必须用标志区分，否则 onClosed 的 false 会覆盖这里的 true
            root._lookupAccepted = true
            if (backend && backend.modpackExporter) backend.modpackExporter.continueAfterLookupFailure(true)
        }
        onClosed: {
            root._lookupOpen = false
            if (!root._lookupAccepted && backend && backend.modpackExporter)
                backend.modpackExporter.continueAfterLookupFailure(false)
            root._lookupAccepted = false
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
        currentFile: root._savePath
        onAccepted: {
            var p = String(selectedFile).replace(/^(file:\/{2,3})/i, "")
            var ext = root._format === "curseforge" ? ".zip" : ".mrpack"
            if (!p.toLowerCase().endsWith(ext)) p += ext
            root._savePath = p
        }
    }
}
