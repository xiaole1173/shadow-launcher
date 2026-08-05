// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

/// 整合包导出浮层（.mrpack，Modrinth 格式）
/// 挂载于版本设置浮层（工具分区「导出整合包」入口），复用 GenericPopup/
/// ShadowButton/ShadowSwitch/InputBox 通用元件，风格与全启动器一致。
Item {
    id: root
    anchors.fill: parent

    // ── 外部注入 ──
    property var backend: null
    property var toastManager: null
    property string versionId: ""
    property string versionName: ""
    property bool opened: false

    // ── 内部状态 ──
    property string _packName: ""
    property string _savePath: ""
    property bool _includeSaves: false
    property bool _includeResourcepacks: true
    property bool _includeShaderpacks: true
    property bool _busy: false
    property real _progress: 0
    property string _statusText: ""

    signal closed()

    onOpenedChanged: {
        if (root.opened) {
            // 每次打开重置表单（默认包名 = 版本名，默认路径 = 下载目录）
            _packName = versionName
            _includeSaves = false
            _includeResourcepacks = true
            _includeShaderpacks = true
            _busy = false
            _progress = 0
            _statusText = ""
            var dlDir = Qt.StandardPaths.writableLocation(Qt.StandardPaths.DownloadLocation)
            if (!dlDir) dlDir = Qt.StandardPaths.writableLocation(Qt.StandardPaths.DocumentsLocation)
            _savePath = (dlDir ? dlDir + "/" : "") + (versionName || "modpack") + ".mrpack"
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
                if (ok) {
                    root._progress = 1
                    root._statusText = qsTr("导出完成")
                    if (root.toastManager) root.toastManager.show(qsTr("整合包已导出: ") + out)
                } else {
                    root._progress = 0
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
        e.exportVersion(versionId, _packName.trim(),
                        _includeSaves, _includeResourcepacks, _includeShaderpacks, path)
    }

    GenericPopup {
        id: popup
        anchors.fill: parent
        title: qsTr("导出整合包")
        subtitle: qsTr("打包为 .mrpack（Modrinth 格式），可在任意启动器导入")
        cardWidth: 460
        opened: root.opened
        onClosed: {
            // 导出中关闭 = 取消导出
            if (root._busy && backend && backend.modpackExporter)
                backend.modpackExporter.cancel()
            root.closed()
        }
        onRejected: {
            if (root._busy && backend && backend.modpackExporter)
                backend.modpackExporter.cancel()
        }

        ColumnLayout {
            width: popup.availableWidth
            anchors.margins: 0
            spacing: 14

            // ── 导出版本（只读提示）──
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 20; Layout.rightMargin: 20
                Layout.topMargin: 8
                spacing: 8
                Text {
                    text: qsTr("导出版本")
                    color: StyleTokens.textSecondary
                    font.pixelSize: StyleTokens.fontSizeSm
                }
                Text {
                    text: root.versionName || root.versionId
                    color: StyleTokens.textPrimary
                    font.pixelSize: StyleTokens.fontSizeSm
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }

            // ── 整合包名称 ──
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 20; Layout.rightMargin: 20
                spacing: 6
                Text {
                    text: qsTr("整合包名称")
                    color: StyleTokens.textSecondary
                    font.pixelSize: StyleTokens.fontSizeSm
                }
                InputBox {
                    Layout.fillWidth: true
                    text: root._packName
                    placeholderText: root.versionName || qsTr("输入整合包名称")
                    enabled: !root._busy
                    onTextChanged: root._packName = text
                }
            }

            // ── 包含内容开关 ──
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 20; Layout.rightMargin: 20
                spacing: 10
                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: qsTr("包含存档")
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
                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: qsTr("包含资源包")
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
                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: qsTr("包含光影")
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

            // ── 保存位置 ──
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 20; Layout.rightMargin: 20
                spacing: 6
                Text {
                    text: qsTr("保存位置")
                    color: StyleTokens.textSecondary
                    font.pixelSize: StyleTokens.fontSizeSm
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
            }

            // ── 操作按钮 ──
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 20; Layout.rightMargin: 20
                Layout.topMargin: 4
                spacing: 10
                Item { Layout.fillWidth: true }
                ShadowButton {
                    text: root._busy ? qsTr("取消导出") : qsTr("取消")
                    btnWidth: 96
                    outlined: true
                    onClicked: {
                        if (root._busy && backend && backend.modpackExporter)
                            backend.modpackExporter.cancel()
                        root.closed()
                    }
                }
                ShadowButton {
                    text: qsTr("导出")
                    btnWidth: 120
                    enabled: !root._busy && root._packName.trim().length > 0 && root._savePath.trim().length > 0
                    onClicked: root._startExport()
                }
            }

            // ── 进度区 ──
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 20; Layout.rightMargin: 20
                Layout.bottomMargin: 16
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
        nameFilters: [qsTr("Modrinth 整合包 (*.mrpack)"), qsTr("所有文件 (*.*)")]
        currentFile: root._savePath
        onAccepted: {
            var p = String(selectedFile).replace(/^(file:\/{2,3})/i, "")
            if (!/\.mrpack$/i.test(p)) p += ".mrpack"
            root._savePath = p
        }
    }
}
