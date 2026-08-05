// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

/// 整合包导出（版本设置独立分区，Section 7）
/// 后端 ModpackExporter 全程 worker 线程打包，进度/结果信号回主线程，
/// 本组件不阻塞 UI。复用 InputBox/ShadowSwitch/ShadowButton 通用元件，
/// 布局与配色对齐「工具与维护」分区。
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
    property bool _includeConfig: true
    property bool _includeSaves: false
    property bool _includeResourcepacks: true
    property bool _includeShaderpacks: true
    property string _savePath: ""
    property bool _busy: false
    property real _progress: 0
    property string _statusText: ""
    property bool _done: false

    onVersionNameChanged: {
        if (!_packName.length) _packName = versionName
        _resetSavePath()
    }

    function _resetSavePath() {
        var dlDir = Qt.StandardPaths.writableLocation(Qt.StandardPaths.DownloadLocation)
        if (!dlDir) dlDir = Qt.StandardPaths.writableLocation(Qt.StandardPaths.DocumentsLocation)
        _savePath = (dlDir ? dlDir + "/" : "") + (versionName || "modpack") + ".mrpack"
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
                    root._statusText = qsTr("导出完成")
                    root._done = true
                    if (root.toastManager) root.toastManager.show(qsTr("整合包已导出: ") + out)
                } else {
                    root._progress = 0
                    root._done = false
                    root._statusText = qsTr("导出失败: ") + (err || qsTr("未知错误"))
                    if (root.toastManager) root.toastManager.show(qsTr("导出失败: ") + (err || qsTr("未知错误")), 5000)
                }
            })
        }
    }

    function _startExport() {
        var e = backend ? backend.modpackExporter : null
        if (!e || _busy) return
        if (!_packName.trim() || !_savePath.trim()) {
            if (root.toastManager) root.toastManager.show(qsTr("请填写整合包名称并选择保存位置"), 3000)
            return
        }
        var path = _savePath
        if (!/\.mrpack$/i.test(path)) path += ".mrpack"
        root._savePath = path
        root._done = false
        root._progress = 0
        root._statusText = ""
        e.exportVersion(versionId, _packName.trim(),
                        _includeSaves, _includeResourcepacks, _includeShaderpacks, path)
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
            text: qsTr("将当前版本打包为 .mrpack（Modrinth 格式），可在任意支持该格式的启动器导入。导出在后台线程执行，不影响其他操作。")
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
                Layout.preferredWidth: 140
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
            // 存档
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
                    onToggled: root._includeSaves = checked
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
                enabled: !root._busy || root._busy
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

    // ── 保存位置选择 ──
    FileDialog {
        id: exportFileDialog
        fileMode: FileDialog.SaveFile
        title: qsTr("保存整合包")
        nameFilters: [qsTr("Modrinth 整合包 (*.mrpack)"), qsTr("所有文件 (*.*)")]
        currentFile: root._savePath
        onAccepted: {
            var p = String(selectedFile).replace(/^(file:\/{2,3})/i, "")
            if (!/\.mrpack$/i.test(p)) p += ".mrpack"
            root._savePath = p
        }
    }
}
