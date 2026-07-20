import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

// ════════════════════════════════════════════
// Java 下载页面 — 级联下拉选择 → 下载卡片
// ════════════════════════════════════════════

Item {
    id: page
    // Size/position set by external anchors (DownloadPage)

    property var javaBackend: null
    property var toastManager: null
    property real progressPct: 0
    property string progressSpeedText: ""
    property string pendingDownloadFile: ""
    property real pendingDownloadSize: 0
    property int toastStartCount: 0
    property int toastFinishCount: 0

    // ── Step labels + hints ──
    readonly property var stepLabels: [
        "① 选择 Java 版本",
        "② 选择 JRE / JDK",
        "③ 选择 CPU 架构",
        "④ 选择操作系统",
        "⑤ 选择安装形式"
    ]
    readonly property var stepHints: [
        "",
        "JRE = 运行环境（仅运行 Java 程序）\nJDK = 开发工具包（含 JRE + 编译工具）。\n一般玩家选 JRE 即可。",
        "如果您是 Windows 系统，一般选择 x64。\nx86 为 32 位系统，arm64 为 ARM 芯片。",
        "选择您的操作系统。",
        "MSI = Windows 安装程序（自动配置环境变量）。\nZIP = 绿色免安装版本（解压即用）。"
    ]

    Flickable {
        id: flick
        anchors.fill: parent
        anchors.margins: 20
        contentHeight: contentColumn.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
        }

        ColumnLayout {
            id: contentColumn
            width: flick.width
        spacing: 12

        // ── Title ──
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Text {
                text: "Java 运行环境下载"
                color: "#d0d4e0"
                font.pixelSize: StyleTokens.fontSizeXl
                font.weight: Font.Bold
            }
            Item { Layout.fillWidth: true }
            Text {
                text: "来源：清华大学开源软件镜像站"
                color: "#687080"
                font.pixelSize: StyleTokens.fontSizeSm
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        // ── Dropdown 1: Version ──
        JavaDropdown {
            id: ddVersion
            label: stepLabels[0]
            hint: stepHints[0]
            model: javaBackend ? javaBackend.javaVersions : []
            currentValue: javaBackend ? javaBackend.selectedVersion : ""
            enabled: javaBackend && javaBackend.javaVersions.length > 0
            showSpinner: javaBackend && javaBackend.javaVersions.length === 0 && javaBackend.fetching
            loadingText: "正在拉取版本列表......"
            onSelected: javaBackend.selectedVersion = val
        }

        // ── Dropdown 2: JRE/JDK ──
        JavaDropdown {
            id: ddType
            label: stepLabels[1]
            hint: stepHints[1]
            model: javaBackend ? javaBackend.javaTypes : []
            currentValue: javaBackend ? javaBackend.selectedType : ""
            enabled: javaBackend && javaBackend.selectedVersion !== "" && javaBackend.javaTypes.length > 0
            showSpinner: javaBackend && javaBackend.selectedVersion !== "" && javaBackend.javaTypes.length === 0 && javaBackend.fetching
            onSelected: javaBackend.selectedType = val
        }

        // ── Dropdown 3: Arch ──
        JavaDropdown {
            id: ddArch
            label: stepLabels[2]
            hint: stepHints[2]
            model: javaBackend ? javaBackend.javaArchs : []
            currentValue: javaBackend ? javaBackend.selectedArch : ""
            enabled: javaBackend && javaBackend.selectedType !== "" && javaBackend.javaArchs.length > 0
            showSpinner: javaBackend && javaBackend.selectedType !== "" && javaBackend.javaArchs.length === 0 && javaBackend.fetching
            onSelected: javaBackend.selectedArch = val
        }

        // ── Dropdown 4: OS ──
        JavaDropdown {
            id: ddOS
            label: stepLabels[3]
            hint: stepHints[3]
            model: javaBackend ? javaBackend.javaOSes : []
            currentValue: javaBackend ? javaBackend.selectedOS : ""
            enabled: javaBackend && javaBackend.selectedArch !== "" && javaBackend.javaOSes.length > 0
            showSpinner: javaBackend && javaBackend.selectedArch !== "" && javaBackend.javaOSes.length === 0 && javaBackend.fetching
            onSelected: javaBackend.selectedOS = val
        }

        // ── Step 5: Download cards ──
        Text {
            text: stepLabels[4]
            color: "#d0d4e0"
            font.pixelSize: StyleTokens.fontSizeMd
            font.weight: Font.Medium
            visible: javaBackend && javaBackend.selectedOS !== ""
        }
        Text {
            text: stepHints[4]
            color: "#687080"
            font.pixelSize: StyleTokens.fontSizeSm
            visible: javaBackend && javaBackend.selectedOS !== ""
            wrapMode: Text.WordWrap
        }

        // Download cards
        Flow {
            Layout.fillWidth: true
            spacing: 12
            visible: javaBackend && javaBackend.selectedOS !== ""

            Repeater {
                model: javaBackend ? javaBackend.javaFiles : []
                delegate: JavaDownloadCard {
                    fileInfo: modelData
                    onDownloadClicked: {
                        if (progressBar.visible) {
                            if (toastManager) toastManager.show("请等待当前Java下载完成")
                            return
                        }
                        pendingDownloadFile = modelData.name
                        pendingDownloadSize = modelData.size ? modelData.size : 0
                        saveDirDialog.open()
                    }
                    progressPct: 0  // updated via Connections
                    progressSpeed: 0
                }
            }
        }

        // ── Download progress ──
        Rectangle {
            id: progressBar
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            radius: StyleTokens.radiusSm
            color: StyleTokens.bgSecondary
            border.color: StyleTokens.bgElevated
            visible: false
            clip: true

            Rectangle {
                id: progressFill
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: parent.width * 0
                color: "#3a4eb8"
                radius: StyleTokens.radiusSm
                Behavior on width { SmoothedAnimation { duration: 300 } }
            }
            Text {
                anchors.centerIn: parent
                text: progressPct + "% · " + progressSpeedText
                color: "#b0b8c8"
                font.pixelSize: StyleTokens.fontSizeSm
            }
        }

        Connections {
            target: javaBackend
            enabled: javaBackend !== null
            function onDownloadProgress(pct, dlBytes, totalBytes, speedMBps) {
                progressBar.visible = true
                progressPct = pct
                if (progressBar.width > 0) progressFill.width = progressBar.width * (pct / 100.0)
                let dlMB = (dlBytes / 1048576).toFixed(1)
                if (totalBytes > 0) {
                    let totalMB = (totalBytes / 1048576).toFixed(1)
                    progressSpeedText = dlMB + "/" + totalMB + " MB · " + speedMBps.toFixed(1) + " MB/s"
                } else {
                    progressSpeedText = "下载中 " + dlMB + " MB · " + speedMBps.toFixed(1) + " MB/s"
                }
            }
            function onDownloadFinished(ok, path) {
                if (ok) {
                    progressPct = 100
                    progressFill.width = progressBar.width
                    progressSpeedText = "下载完成!"
                    progressFill.color = "#3ab868"
                    // Extract filename from path
                    var fileName = path.substring(path.lastIndexOf("/") + 1)
                    if (toastManager) {
                        toastManager.show(fileName + " 下载完成")
                        toastFinishCount++
                    }
                } else {
                    progressSpeedText = "下载失败"
                    progressFill.color = "#d04848"
                }
                hideTimer.start()
            }
        }

        Timer {
            id: hideTimer
            interval: 3000
            onTriggered: { progressBar.visible = false; progressFill.color = "#3a4eb8" }
        }

        Item { Layout.fillHeight: true } // spacer
    }
    } // Flickable

    // ── Save dialog (shows filename, user picks directory) ──
    FileDialog {
        id: saveDirDialog
        title: "保存 Java 安装文件"
        fileMode: FileDialog.SaveFile
        currentFile: pendingDownloadFile
        onAccepted: {
            if (pendingDownloadFile && javaBackend) {
                progressBar.visible = true
                progressPct = 0
                progressSpeedText = "准备下载..."
                progressFill.width = 0
                progressFill.color = "#3a4eb8"

                // Toast
                if (toastManager) {
                    toastManager.show(pendingDownloadFile + " 开始下载")
                    toastStartCount++
                }

                var fullPath = String(selectedFile).replace("file:///", "")
                var lastSlash = fullPath.lastIndexOf("/")
                var dir = fullPath.substring(0, lastSlash)
                var name = fullPath.substring(lastSlash + 1)
                javaBackend.downloadJavaFileTo(name, dir, pendingDownloadSize)
                pendingDownloadFile = ""
                pendingDownloadSize = 0
            }
        }
        onRejected: pendingDownloadFile = ""
    }

    // ════════════════════════════════════════════
    // [调试] DEBUG OVERLAY — 实时状态日志
    // ════════════════════════════════════════════
    Rectangle {
        anchors.right: parent.right
        anchors.rightMargin: 20
        anchors.verticalCenter: parent.verticalCenter
        width: 420; height: debugCol.implicitHeight + 10
        radius: StyleTokens.radiusMd
        color: "#1a0000"
        border.color: "#661111"
        z: 999
        visible: false  // 隐藏调试面板，保留监测功能

        Column {
            id: debugCol
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 6 }
            spacing: 2

            function log(label, value) {
                return "<b>" + label + "</b>: " + value
            }

            Text {
                text: debugCol.log("fetching", String(page.javaBackend ? page.javaBackend.fetching : "null"))
                color: page.javaBackend && page.javaBackend.fetching ? "#ff8844" : "#885555"
                font.pixelSize: StyleTokens.fontSizeXs
                font.family: "Consolas, monospace"
            }
            Text {
                text: debugCol.log("versions", String(page.javaBackend ? page.javaBackend.javaVersions.length : "null"))
                color: page.javaBackend && page.javaBackend.javaVersions.length > 0 ? "#44ff44" : "#885555"
                font.pixelSize: StyleTokens.fontSizeXs
                font.family: "Consolas, monospace"
            }
            Text {
                text: debugCol.log("types", String(page.javaBackend ? page.javaBackend.javaTypes.length : "null"))
                color: page.javaBackend && page.javaBackend.javaTypes.length > 0 ? "#44ff44" : "#885555"
                font.pixelSize: StyleTokens.fontSizeXs
                font.family: "Consolas, monospace"
            }
            Text {
                text: debugCol.log("archs", String(page.javaBackend ? page.javaBackend.javaArchs.length : "null"))
                color: page.javaBackend && page.javaBackend.javaArchs.length > 0 ? "#44ff44" : "#885555"
                font.pixelSize: StyleTokens.fontSizeXs
                font.family: "Consolas, monospace"
            }
            Text {
                text: debugCol.log("oses", String(page.javaBackend ? page.javaBackend.javaOSes.length : "null"))
                color: page.javaBackend && page.javaBackend.javaOSes.length > 0 ? "#44ff44" : "#885555"
                font.pixelSize: StyleTokens.fontSizeXs
                font.family: "Consolas, monospace"
            }

            Text { text: "── 选中值 ──"; color: "#aa6666"; font.pixelSize: StyleTokens.fontSizeXs; font.family: "Consolas, monospace" }
            Text {
                text: debugCol.log("selVer", String(page.javaBackend ? page.javaBackend.selectedVersion || "(empty)" : "null"))
                color: "#88aadd"
                font.pixelSize: StyleTokens.fontSizeXs
                font.family: "Consolas, monospace"
            }
            Text {
                text: debugCol.log("selType", String(page.javaBackend ? page.javaBackend.selectedType || "(empty)" : "null"))
                color: "#88aadd"
                font.pixelSize: StyleTokens.fontSizeXs
                font.family: "Consolas, monospace"
            }
            Text {
                text: debugCol.log("selArch", String(page.javaBackend ? page.javaBackend.selectedArch || "(empty)" : "null"))
                color: "#88aadd"
                font.pixelSize: StyleTokens.fontSizeXs
                font.family: "Consolas, monospace"
            }
            Text {
                text: debugCol.log("selOS", String(page.javaBackend ? page.javaBackend.selectedOS || "(empty)" : "null"))
                color: "#88aadd"
                font.pixelSize: StyleTokens.fontSizeXs
                font.family: "Consolas, monospace"
            }

            Text { text: "── 动画触发 ──"; color: "#aa6666"; font.pixelSize: StyleTokens.fontSizeXs; font.family: "Consolas, monospace" }
            Text {
                text: debugCol.log("toast", page.toastManager ? "set" : "NULL")
                color: page.toastManager ? "#44ff44" : "#ff4444"
                font.pixelSize: StyleTokens.fontSizeXs
                font.family: "Consolas, monospace"
            }
            Text {
                text: debugCol.log("tStart", page.toastStartCount)
                color: page.toastStartCount > 0 ? "#44ff44" : "#884444"
                font.pixelSize: StyleTokens.fontSizeXs
                font.family: "Consolas, monospace"
            }
            Text {
                text: debugCol.log("tFinish", page.toastFinishCount)
                color: page.toastFinishCount > 0 ? "#44ff44" : "#884444"
                font.pixelSize: StyleTokens.fontSizeXs
                font.family: "Consolas, monospace"
            }
            Text {
                text: debugCol.log("vSpinner", String(ddVersion.showSpinner))
                color: ddVersion.showSpinner ? "#ffaa00" : "#664400"
                font.pixelSize: StyleTokens.fontSizeXs
                font.family: "Consolas, monospace"
            }
            Text {
                text: debugCol.log("tSpinner", String(ddType.showSpinner))
                color: ddType.showSpinner ? "#ffaa00" : "#664400"
                font.pixelSize: StyleTokens.fontSizeXs
                font.family: "Consolas, monospace"
            }
            Text {
                text: debugCol.log("aSpinner", String(ddArch.showSpinner))
                color: ddArch.showSpinner ? "#ffaa00" : "#664400"
                font.pixelSize: StyleTokens.fontSizeXs
                font.family: "Consolas, monospace"
            }
            Text {
                text: debugCol.log("oSpinner", String(ddOS.showSpinner))
                color: ddOS.showSpinner ? "#ffaa00" : "#664400"
                font.pixelSize: StyleTokens.fontSizeXs
                font.family: "Consolas, monospace"
            }

            Text { text: "── 启用状态 ──"; color: "#aa6666"; font.pixelSize: StyleTokens.fontSizeXs; font.family: "Consolas, monospace" }
            Text {
                text: debugCol.log("vEnabled", String(ddVersion.enabled))
                color: ddVersion.enabled ? "#44ff44" : "#884444"
                font.pixelSize: StyleTokens.fontSizeXs
                font.family: "Consolas, monospace"
            }
            Text {
                text: debugCol.log("tEnabled", String(ddType.enabled))
                color: ddType.enabled ? "#44ff44" : "#884444"
                font.pixelSize: StyleTokens.fontSizeXs
                font.family: "Consolas, monospace"
            }
            Text {
                text: debugCol.log("aEnabled", String(ddArch.enabled))
                color: ddArch.enabled ? "#44ff44" : "#884444"
                font.pixelSize: StyleTokens.fontSizeXs
                font.family: "Consolas, monospace"
            }
            Text {
                text: debugCol.log("oEnabled", String(ddOS.enabled))
                color: ddOS.enabled ? "#44ff44" : "#884444"
                font.pixelSize: StyleTokens.fontSizeXs
                font.family: "Consolas, monospace"
            }

            Text { text: "── 下载进度 ──"; color: "#aa6666"; font.pixelSize: StyleTokens.fontSizeXs; font.family: "Consolas, monospace" }
            Text {
                text: "pct: " + progressPct + "%"
                color: "#88ddff"
                font.pixelSize: StyleTokens.fontSizeXs
                font.family: "Consolas, monospace"
            }
            Text {
                text: "speed: " + progressSpeedText
                color: "#88ddff"
                font.pixelSize: StyleTokens.fontSizeXs
                font.family: "Consolas, monospace"
            }
            Text {
                text: "barVis: " + progressBar.visible
                color: progressBar.visible ? "#44ff44" : "#884444"
                font.pixelSize: StyleTokens.fontSizeXs
                font.family: "Consolas, monospace"
            }
        }
    }

    // ── Component: JavaDropdown ──
    component JavaDropdown: ColumnLayout {
        id: root
        property string label
        property string hint
        property var model: []
        property string currentValue
        property bool enabled: true
        property bool showSpinner: false
        property string loadingText: ""
        signal selected(string val)

        spacing: 4

        Text {
            text: root.label
            color: "#d0d4e0"
            font.pixelSize: StyleTokens.fontSizeMd
            font.weight: Font.Medium
        }

        RowLayout {
            spacing: 8

            // Wrapper so overlay MouseArea can sit on top of disabled ComboBox
            Item {
                Layout.preferredWidth: 280
                Layout.preferredHeight: 34

                ComboBox {
                    id: combo
                    anchors.fill: parent
                    enabled: root.enabled

                model: root.model
                currentIndex: root.currentValue ? root.model.indexOf(root.currentValue) : -1

                onCurrentValueChanged: {
                    // Force clear when parent selection resets
                    if (!currentValue || currentValue === "") combo.currentIndex = -1
                }

                // ── Same dark style as SettingsPage ──

                // ── Dark style, grayed when disabled ──
                background: Rectangle {
                    radius: StyleTokens.radiusMd
                    color: root.enabled ? (combo.hovered ? "#252a38" : "#161a24") : "#10131c"
                    border.color: root.enabled ? (combo.hovered ? "#3a4eb8" : "#1e2230") : "#181c24"
                    border.width: 1
                    Behavior on color { ColorAnimation { duration: 200 } }
                    Behavior on border.color { ColorAnimation { duration: 200 } }
                }
                contentItem: Text {
                    leftPadding: 12
                    verticalAlignment: Text.AlignVCenter
                    text: root.currentValue ? root.currentValue : "请选择"
                    color: root.enabled ? "#d0d4e0" : "#525868"
                    font.pixelSize: StyleTokens.fontSizeSm
                    elide: Text.ElideRight
                }
                indicator: Canvas {
                    width: 12; height: 12
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.strokeStyle = combo.hovered ? "#8088f0" : "#606478"
                        ctx.lineWidth = 1.5
                        ctx.beginPath(); ctx.moveTo(0, 3); ctx.lineTo(6, 9); ctx.lineTo(12, 3); ctx.stroke()
                    }
                }

                delegate: ItemDelegate {
                    width: combo.popup.width
                    contentItem: Text {
                        text: modelData
                        color: "#d0d4e0"
                        font.pixelSize: StyleTokens.fontSizeSm
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 12
                    }
                    background: Rectangle {
                        color: highlighted ? "#252a38" : "transparent"
                        Behavior on color { ColorAnimation { duration: 150 } }
                    }
                    highlighted: combo.highlightedIndex === index
                }

                popup: Popup {
                    y: combo.height + 4
                    width: combo.width
                    implicitHeight: contentItem.implicitHeight + 8
                    padding: 4

                    enter: Transition {
                        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 150; easing.type: Easing.OutCubic }
                        NumberAnimation { property: "y"; from: combo.height - 2; to: combo.height + 4; duration: 200; easing.type: Easing.OutCubic }
                    }
                    exit: Transition {
                        NumberAnimation { property: "opacity"; to: 0; duration: 100 }
                    }

                    contentItem: ListView {
                        clip: true
                        implicitHeight: contentHeight
                        model: combo.popup.visible ? combo.delegateModel : null
                        currentIndex: combo.highlightedIndex
                    }
                    background: Rectangle {
                        radius: StyleTokens.radiusMd
                        color: StyleTokens.surfaceOverlay
                        border.color: StyleTokens.bgElevated
                    }
                }

                onActivated: root.selected(root.model[currentIndex])
            }
            }

            // Blue spinner + loading text (wrap for animated entry/exit)
            Row {
                id: spinnerRow
                spacing: 6
                Layout.alignment: Qt.AlignVCenter
                visible: root.showSpinner

                Rectangle {
                    width: 18; height: 18
                    radius: StyleTokens.radiusLg
                    color: "transparent"
                    opacity: root.showSpinner ? 1 : 0
                    scale: root.showSpinner ? 1 : 0.3

                    Behavior on opacity {
                        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                    }
                    Behavior on scale {
                        NumberAnimation { duration: 350; easing.type: Easing.OutBack }
                    }

                    // rotating arc
                    Canvas {
                        id: spinnerCanvas
                        anchors.fill: parent
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            ctx.strokeStyle = "#4a70e0"
                            ctx.lineWidth = 2.5
                            ctx.lineCap = "round"
                            ctx.beginPath()
                            ctx.arc(width/2, height/2, width/2 - 2, -Math.PI/2, Math.PI, false)
                            ctx.stroke()
                        }
                    }

                    RotationAnimator on rotation {
                        from: 0; to: 360; duration: 1000; loops: Animation.Infinite
                        running: root.showSpinner
                    }

                    // Force repaint when spinner becomes visible
                    Component.onCompleted: if (root.showSpinner) spinnerCanvas.requestPaint()
                    onOpacityChanged: if (opacity > 0) spinnerCanvas.requestPaint()
                }

                // Loading text (e.g. "正在拉取版本列表......")
                Text {
                    text: root.loadingText
                    color: "#687080"
                    font.pixelSize: StyleTokens.fontSizeSm
                    visible: root.loadingText !== ""
                }
            }
        }  // Row

        Text {
            text: root.hint
            color: "#687080"
            font.pixelSize: StyleTokens.fontSizeSm
            visible: root.hint !== ""
            wrapMode: Text.WordWrap
            Layout.maximumWidth: 400
        }
    }

    // ── Component: JavaDownloadCard ──
    component JavaDownloadCard: Rectangle {
        id: card
        width: 220; height: 80; radius: StyleTokens.radiusLg
        color: StyleTokens.surfaceOverlay
        border.color: cardMouse.containsMouse ? "#3a4eb8" : "#1e2230"
        border.width: 1

        property var fileInfo: ({})
        property real progressPct: 0
        property double progressSpeed: 0
        signal downloadClicked()

        scale: cardMouse.containsMouse ? 1.03 : 1.0
        Behavior on scale { NumberAnimation { duration: 150 } }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 4

            Text {
                text: card.fileInfo.name || "???"
                color: "#d0d4e0"
                font.pixelSize: StyleTokens.fontSizeMd
                font.weight: Font.Medium
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            RowLayout {
                spacing: 8
                // File size
                Text {
                    text: {
                        var s = card.fileInfo.size || 0
                        if (s > 1048576) return (s / 1048576).toFixed(1) + " MB"
                        if (s > 1024) return (s / 1024).toFixed(1) + " KB"
                        return s + " B"
                    }
                    color: "#687080"
                    font.pixelSize: StyleTokens.fontSizeSm
                }
                // Extension badge
                Rectangle {
                    radius: StyleTokens.radiusXs
                    color: StyleTokens.accentSubtle
                    implicitWidth: extText.implicitWidth + 10
                    implicitHeight: 18
                    Text {
                        id: extText
                        anchors.centerIn: parent
                        text: card.fileInfo.ext ? card.fileInfo.ext.toUpperCase() : ""
                        color: StyleTokens.accentLink
                        font.pixelSize: StyleTokens.fontSizeXs
                        font.weight: Font.Bold
                    }
                }
                Item { Layout.fillWidth: true }
                // Download icon
                Text {
                    text: "↓"
                    color: StyleTokens.accentLink
                    font.pixelSize: StyleTokens.fontSizeLg
                    font.weight: Font.Bold
                }
            }
        }

        MouseArea {
            id: cardMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: card.downloadClicked()
        }
    }
}
