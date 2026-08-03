// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs

// ═══════════════════════════════════════════════════════════════════
// ModpackImportOverlay — 整合包导入弹窗（最终版：纯文件选择入口）
//
// 生命周期：show() 弹出文件选择 → 选文件 → 「开始导入」→
//   startImport() 启动后端任务 → 立即 hide() 永久关闭本弹窗 →
//   跳转全局下载进度页（InstallProgressPage，导航第 5 项）。
// 导入执行 / 进度 / 模组列表 / 日志 / 结果全部常驻下载进度页，
// 本弹窗不再有任何执行或结果回显逻辑。
//
// 对外接口强制保留：show()、hide()、toastManager、appWindow
// （MainWindow / VersionSelectPage / VersionSelectOverlay 三处调用点零改动）。
// ═══════════════════════════════════════════════════════════════════
Rectangle {
    id: root
    anchors.fill: parent
    color: "transparent"
    visible: false
    z: 300

    property var toastManager: null
    property var appWindow: null

    // ── 内部状态 ──
    property string _filePath: ""
    property string _fileName: ""
    // 来源标识（Modrinth / CurseForge）：声明为真实属性供下载进度页经
    // mainWindow.modpackImportOverlay 读取。切勿用动态属性赋值——
    // Qt 6 qmlcachegen 编译模式下对 QML 对象运行时赋新属性会抛
    // "Cannot assign to non-existent property"，直接中断 _startImport。
    property string _modpackImportFormat: ""

    // ═══════════ 公共 API（强制保留）═══════════
    function show() {
        if (!backend || !backend.modpackImporter) {
            if (toastManager) toastManager.show(qsTr("后端未就绪"))
            return
        }
        // 单任务限制：同一时间只允许一个整合包任务（下载或导入）
        if (backend.modpackBusy()) {
            if (toastManager) toastManager.show(qsTr("已有整合包任务（下载或导入）进行中，请等待完成"), "", 5000)
            return
        }
        root.visible = true
        forceActiveFocus()
    }

    function hide() {
        root.visible = false
    }

    // ═══════════ 内部逻辑 ═══════════
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

    // 开始导入：启动后端任务 → 永久关闭弹窗 → 路由到全局下载进度页
    function _startImport() {
        console.log("[modpack-import] _startImport: file=", root._filePath)
        if (!root._filePath) return
        // 来源标识写入弹窗根属性（下载进度页经 mainWindow.modpackImportOverlay 读取）
        root._modpackImportFormat = /\.mrpack$/i.test(root._filePath) ? "Modrinth" : "CurseForge"
        // 1. 调用后端（同步触发 busyChanged → 下载进度页条目出现）
        backend.modpackImporter.startImport(root._filePath)
        // 2. 永久关闭弹窗（本弹窗生命周期到此结束，不再二次弹出）
        root.hide()
        // 3. 路由到下载进度页（导航第 5 项）
        if (root.appWindow && typeof root.appWindow.switchPage === "function")
            root.appWindow.switchPage(5)
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

    // ═══════════ 遮罩层（点击关闭）═══════════
    // 弹窗位于窗口圆角容器之外：遮罩需自带圆角，避免把窗口透明圆角区域涂黑成方角
    Rectangle {
        anchors.fill: parent
        color: "#80000000"
        radius: StyleTokens.radiusWindow   // 统一窗口边角圆角渲染
        clip: true
        opacity: root.visible ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

        MouseArea {
            anchors.fill: parent
            onClicked: root.hide()
        }
    }

    // ═══════════ 弹窗窗体 ═══════════
    // 无外边框：圆角容器 + 无 border（边框与圆角组合在本渲染环境下角部会呈尖角/异常，
    // 且用户明确不要外围框；圆角轮廓本身已足够与遮罩区分）
    Rectangle {
        id: card
        anchors.centerIn: parent
        width: 520
        height: 400
        radius: StyleTokens.radiusWindow
        color: StyleTokens.surfaceOverlay
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
                color: StyleTokens.surfaceOverlay   // 显式背景色，杜绝透明继承异常

                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 1
                    color: StyleTokens.borderLight
                }

                Text {
                    anchors.left: parent.left; anchors.leftMargin: 20
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("导入整合包")
                    color: StyleTokens.textPrimary
                    font.pixelSize: StyleTokens.fontSizeLg
                    font.bold: true
                }

                ShadowIconButton {
                    anchors.right: parent.right; anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    source: "icons/lucide/x.svg"
                    sourceWidth: 14; sourceHeight: 14
                    type: "close"
                    onClicked: root.hide()
                }
            }

            // ── 主体：文件选择 ──
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 24
                spacing: 16

                Item { Layout.fillHeight: true }

                // 拖拽 / 点击选择区
                // 无静态边框（用户要求去掉细线方框）；仅在拖拽悬停时显示强调色边框作为反馈
                Rectangle {
                    id: dropBox
                    Layout.fillWidth: true
                    Layout.preferredHeight: 180
                    radius: StyleTokens.radiusXl
                    color: root._filePath ? StyleTokens.bgCard : StyleTokens.bgInput
                    border.color: dropArea.containsDrag ? StyleTokens.accent : "transparent"
                    border.width: dropArea.containsDrag ? 2 : 0
                    Behavior on border.color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
                    Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 10

                        Rectangle {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.preferredWidth: 48
                            Layout.preferredHeight: 48
                            radius: 24
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
                        onEntered: function(drag) { if (drag.hasUrls) drag.accept(Qt.CopyAction) }
                        onDropped: function(drop) {
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
                    text: qsTr("导入将在全局下载进度页中执行，完成后自动刷新版本列表")
                    font.pixelSize: StyleTokens.fontSizeXs
                    color: StyleTokens.textMuted
                }

                Item { Layout.fillHeight: true }
            }

            // ── 底部按钮栏 ──
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                color: StyleTokens.surfaceOverlay   // 显式背景色，杜绝透明继承异常

                Rectangle {
                    anchors.top: parent.top
                    width: parent.width
                    height: 1
                    color: StyleTokens.borderLight
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 10

                    Item { Layout.fillWidth: true }

                    ShadowButton {
                        text: qsTr("取消")
                        accentColor: StyleTokens.bgElevated
                        textColor: StyleTokens.textSecondary
                        Layout.preferredWidth: 96
                        Layout.preferredHeight: 32
                        onClicked: root.hide()
                    }
                    ShadowButton {
                        id: startImportBtn
                        text: qsTr("开始导入")
                        enabled: root._filePath.length > 0
                        accentColor: StyleTokens.accent
                        Layout.preferredWidth: 128
                        Layout.preferredHeight: 32
                        onClicked: {
                            console.log("[modpack-import] 开始导入 clicked, file=", root._filePath)
                            root._startImport()
                        }
                    }
                }
            }
        }
    }

    // ═══════════ Esc 键关闭 ═══════════
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape && root.visible) {
            root.hide()
            event.accepted = true
        }
    }

    // 供调试/自动化读取的按钮引用（/eval 测试用，不参与业务逻辑）
    property alias startImportButton: startImportBtn

    focus: true
}
