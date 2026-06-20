import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: page
    color: "transparent"

    opacity: 0; y: 10
    Behavior on opacity { NumberAnimation { duration: 250 } }
    Behavior on y { NumberAnimation { duration: 250 } }
    Component.onCompleted: { opacity = 1; y = 0 }

    function fmtSpeed(bps) {
        if (bps <= 0) return "0 B/s"
        if (bps < 1024) return bps.toFixed(0) + " B/s"
        if (bps < 1048576) return (bps / 1024).toFixed(1) + " KB/s"
        return (bps / 1048576).toFixed(2) + " MB/s"
    }
    function fmtSize(b) {
        if (b <= 0) return "0 B"
        if (b < 1024) return b + " B"
        if (b < 1048576) return (b / 1024).toFixed(2) + " KB"
        return (b / 1048576).toFixed(2) + " MB"
    }

    // The key: read backend properties INTO local JS vars ONCE per binding evaluation
    // This ensures QML actually re-evaluates the property chain.

    ColumnLayout {
        anchors.fill: parent; anchors.margins: 20; spacing: 12

        // ── Header ──
        RowLayout {
            Layout.fillWidth: true
            Component.onCompleted: console.log("[progress-ui] page loaded")
            Text { text: "下载进度"; font.pixelSize: 18; font.bold: true; color: "#e8ecf8"; Layout.fillWidth: true }
            Rectangle {
                radius: 4; height: 24; width: statusText.implicitWidth + 16
                color: (backend && backend.installing) ? "#1a3028" : "#201a1a"
                Text {
                    id: statusText; anchors.centerIn: parent
                    text: (backend && backend.installing) ? "下载中" : "空闲"
                    color: (backend && backend.installing) ? "#4bc870" : "#886060"
                    font.pixelSize: 11
                    onTextChanged: console.log("[progress-ui] status badge changed to:", text)
                }
            }
        }

        // ── 排队面板 ──
        Rectangle {
            Layout.fillWidth: true
            height: queueLabel.implicitHeight + 20
            visible: backend && backend.downloadQueue && backend.downloadQueue.length > 0
            color: "#11141c"
            radius: 8
            border.color: "#2a2000"
            Text {
                id: queueLabel
                anchors.left: parent.left; anchors.leftMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                text: "排队中: " + (backend && backend.downloadQueue ? backend.downloadQueue.length : 0) + " 个版本"
                color: "#e0a040"
                font.pixelSize: 12
                font.bold: true
            }
        }

        // ── 排队列表 ──
        Repeater {
            model: backend ? (backend.downloadQueue || []) : []
            delegate: Rectangle {
                width: parent ? parent.width : 200
                height: 36
                radius: 6
                color: "#0e100a"
                border.color: "#282010"
                RowLayout {
                    anchors.fill: parent; anchors.margins: 10
                    spacing: 8
                    Text { text: "⏳"; font.pixelSize: 12; color: "#e0a040" }
                    Text { text: modelData.versionId || ""; font.pixelSize: 12; color: "#c0a050"; Layout.fillWidth: true }
                    Text { text: "等待中"; font.pixelSize: 10; color: "#706030" }
                    Rectangle {
                        width: 60; height: 20; radius: 4; color: "#2a1a10"
                        Text { anchors.centerIn: parent; text: "取消"; font.pixelSize: 10; color: "#806060" }
                        MouseArea {
                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: { if (backend) { backend.cancelQueuedDownload(modelData.versionId); toastManager.show("已取消下载") } }
                        }
                    }
                }
            }
        }

        // ── Version info ──
        Rectangle {
            Layout.fillWidth: true; height: 52; radius: 8; color: "#11141c"; border.color: "#1a1f2a"
            RowLayout { anchors.fill: parent; anchors.margins: 14
                Text { text: "当前: " + (backend ? (backend.installVersionId || "--") : "--"); color: "#e8ecf8"; font.pixelSize: 13; font.bold: true; Layout.fillWidth: true }
                Text {
                    text: (backend && backend.installing) ? (backend.installPhase === "verifying" ? "校验中" : (backend.installPhase === "done" ? "已完成" : "下载中")) : "--"
                    color: (backend && backend.installPhase === "done") ? "#4bc870" : "#5d6fe0"
                    font.pixelSize: 11
                }
            }
        }

        // ── Total progress (file count) — hidden during verification ──
        Rectangle {
            Layout.fillWidth: true; height: 64; radius: 8; color: "#0e111a"; border.color: "#1a1f2a"
            visible: (!backend || !backend.verifyRunning)
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 12; spacing: 6
                RowLayout {
                    Text { text: "文件"; font.pixelSize: 12; color: "#8890a0" }
                    Text { text: (backend ? backend.installProgress : 0) + " / " + (backend ? backend.installTotal : 0); font.pixelSize: 13; font.weight: Font.DemiBold; color: "#e8ecf8"; Layout.fillWidth: true }
                    Text { text: (backend && backend.installTotal > 0) ? (backend.installProgress / backend.installTotal * 100).toFixed(1) + "%" : "0%"; font.pixelSize: 14; font.weight: Font.Bold; color: "#5d6fe0" }
                }
                // Progress bar
                Rectangle { Layout.fillWidth: true; height: 8; radius: 4; color: "#1a1f2a"
                    Rectangle {
                        height: 8; radius: 4; color: "#5068d8"
                        width: (backend && backend.installTotal > 0) ? parent.width * (backend.installProgress / backend.installTotal) : 0
                        Behavior on width { NumberAnimation { duration: 200 } }
                    }
                }
            }
        }

        // ── Single file progress ──
        Rectangle {
            Layout.fillWidth: true; height: 56; radius: 8; color: "#0e111a"; border.color: "#1a1f2a"
            visible: (backend && backend.installing && backend.installFile !== "" && !backend.verifyRunning)
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 12; spacing: 4
                RowLayout {
                    Text { text: "文件"; font.pixelSize: 12; color: "#707888" }
                    Text { text: backend ? (backend.installFile || "--") : "--"; font.pixelSize: 12; color: "#aab0c0"; elide: Text.ElideMiddle; Layout.fillWidth: true }
                    Text { text: backend ? fmtSize(backend.installBytesDownloaded) + " / " + fmtSize(backend.installBytesTotal) : ""; font.pixelSize: 10; color: "#707888"; font.family: "Consolas, monospace" }
                }
                Rectangle { Layout.fillWidth: true; height: 6; radius: 3; color: "#1a1f2a"
                    Rectangle {
                        height: 6; radius: 3; color: "#4a6eb8"
                        width: (backend && backend.installBytesTotal > 0) ? parent.width * (backend.installBytesDownloaded / backend.installBytesTotal) : 0
                        Behavior on width { NumberAnimation { duration: 200 } }
                    }
                }
            }
        }

        // ── Speed ──
        Text {
            Layout.fillWidth: true
            text: {
                var spd = backend ? backend.installSpeed : 0
                return "速度: " + fmtSpeed(spd)
            }
            color: "#707888"; font.pixelSize: 11
            visible: backend && backend.installing && !backend.verifyRunning
        }

        // ═══════════════════════════════════════════════════════
        // ── Resource pack download ──
        // ═══════════════════════════════════════════════════════
        Rectangle {
            Layout.fillWidth: true; height: 80; radius: 8; color: "#0e111a"; border.color: "#2a1f3a"
            visible: backend && backend.isResourceDownloading && backend.resourceDownloadTotal > 0
            Component.onCompleted: { if (visible) console.log("[progress] RP download section created") }
            onVisibleChanged: { if (visible) console.log("[progress] RP download section visible") }
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 12; spacing: 6
                RowLayout {
                    Text { text: "资源包"; font.pixelSize: 12; color: "#a090c8"; font.bold: true }
                    Text { 
                        text: backend ? (backend.resourceDownloadFile || "--") : "--"
                        font.pixelSize: 12; color: "#c8b8e8"; elide: Text.ElideRight
                        Layout.maximumWidth: 200; Layout.fillWidth: true
                    }
                    Text {
                        text: (backend && backend.resourceDownloadTotal > 0) 
                            ? (backend.resourceDownloadProgress / backend.resourceDownloadTotal * 100).toFixed(1) + "%" 
                            : "--"
                        color: "#a090d0"; font.pixelSize: 12; font.bold: true
                    }
                }
                RowLayout {
                    Text { 
                        text: backend ? fmtSize(backend.resourceDownloadProgress || 0) + " / " + fmtSize(backend.resourceDownloadTotal || 0) : ""
                        color: "#8878a8"; font.pixelSize: 10; font.family: "Consolas, monospace"
                        Layout.fillWidth: true
                    }
                }
                Rectangle { Layout.fillWidth: true; height: 6; radius: 3; color: "#1a1f2a"
                    Rectangle {
                        height: 6; radius: 3; color: "#8868c8"
                        width: (backend && backend.resourceDownloadTotal > 0) 
                            ? parent.width * (backend.resourceDownloadProgress / backend.resourceDownloadTotal) : 0
                        Behavior on width { NumberAnimation { duration: 200 } }
                    }
                }
            }
        }

        // ═══════════════════════════════════════════════════════
        // ── Mod file download ──
        // ═══════════════════════════════════════════════════════
        ListModel { id: modDownloadModel }
        Connections {
            target: backend
            function onModFileDownloadStarted(dlId, fileName, fileSize, displayName) {
                console.log("[progress] mod dl started:", dlId, displayName)
                modDownloadModel.append({dlId: dlId, name: displayName, fileName: fileName,
                    received: 0, total: fileSize, done: false, error: "", speed: 0, paused: false,
                    lastBytes: 0, lastTime: Date.now()})
            }
            function onModFileDownloadProgress(dlId, received, total) {
                for (var i = 0; i < modDownloadModel.count; i++) {
                    if (modDownloadModel.get(i).dlId === dlId) {
                        var dl = modDownloadModel.get(i)
                        var now = Date.now()
                        var elapsed = (now - dl.lastTime) / 1000
                        if (elapsed > 0.3) {
                            var spd = Math.round((received - dl.lastBytes) / elapsed)
                            modDownloadModel.setProperty(i, "speed", spd)
                            modDownloadModel.setProperty(i, "lastBytes", received)
                            modDownloadModel.setProperty(i, "lastTime", now)
                        }
                        modDownloadModel.setProperty(i, "received", received)
                        modDownloadModel.setProperty(i, "total", total)
                        return
                    }
                }
            }
            function onModFileDownloadFinished(dlId, success, filePath, displayName) {
                console.log("[progress] mod dl finished:", dlId, success)
                for (var i = 0; i < modDownloadModel.count; i++) {
                    if (modDownloadModel.get(i).dlId === dlId) {
                        modDownloadModel.setProperty(i, "done", true)
                        if (success) modDownloadModel.setProperty(i, "received", modDownloadModel.get(i).total)
                        delete modDlSpeeds[dlId]
                        return
                    }
                }
            }
            function onModFileDownloadFailed(dlId, errorDetail, displayName) {
                console.log("[progress] mod dl failed:", dlId, errorDetail)
                for (var i = 0; i < modDownloadModel.count; i++) {
                    if (modDownloadModel.get(i).dlId === dlId) {
                        modDownloadModel.setProperty(i, "done", true)
                        modDownloadModel.setProperty(i, "error", errorDetail)
                        delete modDlSpeeds[dlId]
                        return
                    }
                }
            }
        }

        Repeater {
            model: modDownloadModel
            delegate: Rectangle {
                width: parent ? parent.width : 200
                height: dlCol.implicitHeight + 16; radius: 8; color: "#0e111a"; border.color: "#1a202a"
                ColumnLayout {
                    id: dlCol
                    anchors.fill: parent; anchors.margins: 10; spacing: 5
                    RowLayout {
                        Text { text: model.done ? (model.error ? "❌" : "✅") : "📦"; font.pixelSize: 14; color: model.error ? "#e06060" : (model.done ? "#4bc870" : "#5068c8") }
                        Text { text: model.name; font.pixelSize: 12; color: "#c0c8e0"; Layout.fillWidth: true; elide: Text.ElideRight }
                        // Pause button (active only)
                        Rectangle {
                            visible: !model.done && !model.error && !model.paused
                            width: 50; height: 20; radius: 4; color: pauseBtnHov.hovered ? "#3a3010" : "#2a2010"
                            Text { anchors.centerIn: parent; text: "暂停"; font.pixelSize: 9; color: "#c0a030" }
                            MouseArea { id: pauseBtnHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { backend.pauseModFileDownload(model.dlId); modDownloadModel.setProperty(index, "paused", true) }
                            }
                        }
                        // Cancel button (active or paused)
                        Rectangle {
                            visible: !model.done && !model.error
                            width: 50; height: 20; radius: 4; color: cancelHov.hovered ? "#3a1818" : "#2a1010"
                            Text { anchors.centerIn: parent; text: "取消"; font.pixelSize: 9; color: "#c06060" }
                            MouseArea { id: cancelHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { backend.cancelModFileDownload(model.dlId); modDownloadModel.remove(index) }
                            }
                        }
                        // Resume button (paused only)
                        Rectangle {
                            visible: model.paused && !model.done && !model.error
                            width: 50; height: 20; radius: 4; color: resumeBtnHov.hovered ? "#283818" : "#182810"
                            Text { anchors.centerIn: parent; text: "继续"; font.pixelSize: 9; color: "#70c040" }
                            MouseArea { id: resumeBtnHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                                    modDownloadModel.setProperty(index, "paused", false)
                                                    backend.resumeModFileDownload(model.dlId)
                                                }
                            }
                        }
                        // Skip button (error only)
                        Rectangle {
                            visible: model.done && model.error
                            width: 50; height: 20; radius: 4; color: skipHov.hovered ? "#3a1818" : "#2a1010"
                            Text { anchors.centerIn: parent; text: "跳过"; font.pixelSize: 9; color: "#c06060" }
                            MouseArea { id: skipHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { modDownloadModel.remove(index) }
                            }
                        }
                        // Retry button (error only)
                        Rectangle {
                            visible: model.done && model.error
                            width: 50; height: 20; radius: 4; color: retryHov.hovered ? "#3a3020" : "#2a2010"
                            Text { anchors.centerIn: parent; text: "重试"; font.pixelSize: 9; color: "#c0a030" }
                            MouseArea { id: retryHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { backend.retryModFileDownload(model.dlId) }
                            }
                        }
                    }
                    // Progress bar (only when not done with error)
                    Rectangle {
                        Layout.fillWidth: true; height: 6; radius: 3; color: "#1a1f2a"
                        visible: !(model.done && model.error)
                        Rectangle {
                            height: 6; radius: 3
                            color: model.paused ? "#c0a030" : (model.done ? "#4bc870" : "#5068c8")
                            width: model.total > 0 ? parent.width * (model.received / model.total) : 0
                            Behavior on width { NumberAnimation { duration: 200 } }
                        }
                    }
                    Text {
                        visible: model.paused; color: "#c0a030"; font.pixelSize: 10
                        text: "⏸ 已暂停 — " + fmtSize(model.received) + " / " + fmtSize(model.total)
                    }
                    Text {
                        visible: model.total > 0
                        text: fmtSize(model.received) + " / " + fmtSize(model.total)
                            + (model.done ? "" : ("  " + fmtSpeed(model.speed || 0)))
                        color: "#707888"; font.pixelSize: 10; font.family: "Consolas, monospace"
                    }
                    Text {
                        visible: model.error !== ""
                        text: model.error; color: "#d08080"; font.pixelSize: 10
                        wrapMode: Text.WordWrap; Layout.fillWidth: true
                    }
                }
            }
        }

        // ── Verifying — independent progress bar ──
        Rectangle {
            Layout.fillWidth: true; height: 80; radius: 8; color: "#0e111a"; border.color: "#2a2a50"
            visible: backend && backend.verifyRunning
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 12; spacing: 8
                RowLayout {
                    Rectangle { width: 12; height: 12; radius: 6; color: "#5068d8"
                        RotationAnimator on rotation { from: 0; to: 360; duration: 1200; loops: Animation.Infinite; running: parent.visible } }
                    Text { text: "正在校验游戏文件完整性..."; font.pixelSize: 13; color: "#a0b0e8"; Layout.fillWidth: true }
                    Text { text: (backend && backend.verifyTotal > 0) ? (backend.verifyChecked + " / " + backend.verifyTotal) : "--"; font.pixelSize: 11; color: "#606888" }
                }
                Rectangle { Layout.fillWidth: true; height: 8; radius: 4; color: "#1a1f2a"
                    Rectangle {
                        height: 8; radius: 4; color: "#5068d8"
                        width: (backend && backend.verifyTotal > 0) ? parent.width * (backend.verifyChecked / backend.verifyTotal) : 0
                        Behavior on width { NumberAnimation { duration: 200 } }
                    }
                }
            }
        }

        // ── Pause / Resume + Cancel ──
        RowLayout {
            spacing: 10
            visible: backend && backend.installing && !backend.verifyRunning

            // Pause / Resume
            Rectangle {
                width: pauseBtnText.implicitWidth + 24; height: 30; radius: 5
                color: pauseMouse.containsMouse ? "#202040" : "#151528"
                border.color: pauseMouse.containsMouse ? "#5060a0" : "#303060"
                scale: pauseMouse.pressed ? 0.9 : 1.0
                Behavior on scale { NumberAnimation { duration: 100; easing.type: Easing.OutCubic } }
                Text {
                    id: pauseBtnText
                    anchors.centerIn: parent
                    text: (backend && backend.installPaused) ? "继续下载" : "暂停下载"
                    color: pauseMouse.containsMouse ? "#8090e0" : "#6068a0"
                    font.pixelSize: 11
                }
                MouseArea {
                    id: pauseMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (backend) {
                            if (backend.installPaused) backend.resumeInstall()
                            else backend.pauseInstall()
                        }
                    }
                }
            }

            // Cancel
            Rectangle {
                width: 100; height: 30; radius: 5
                color: cancelMouse.containsMouse ? "#502020" : "#2a1a1a"
                border.color: cancelMouse.containsMouse ? "#a04040" : "#603030"
                scale: cancelMouse.pressed ? 0.9 : 1.0
                Behavior on scale { NumberAnimation { duration: 100; easing.type: Easing.OutCubic } }
                Text { anchors.centerIn: parent; text: "取消下载"; color: cancelMouse.containsMouse ? "#ff6060" : "#a06060"; font.pixelSize: 11 }
                MouseArea { id: cancelMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { if (backend) { backend.cancelInstall(); toastManager.show("已取消安装") } } }
            }
        }

        // ── Empty state ──
        Rectangle {
            Layout.fillWidth: true; height: 100; radius: 8; color: "#11141c"; border.color: "#1a1f2a"
            visible: !backend || (!Boolean(backend.installing) && !Boolean(backend.downloading) && String(backend.installPhase) !== "done" && (!backend.downloadQueue || !Array.isArray(backend.downloadQueue) || backend.downloadQueue.length === 0))
            ColumnLayout { anchors.centerIn: parent; spacing: 6
                Text { Layout.alignment: Qt.AlignHCenter; text: "当前没有进行中的下载"; font.pixelSize: 13; color: "#707888" }
                Text { Layout.alignment: Qt.AlignHCenter; text: "前往下载页面选择版本开始下载"; font.pixelSize: 11; color: "#505468" }
            }
        }

        // ── Done state ──
        Rectangle {
            Layout.fillWidth: true; height: 80; radius: 8; color: "#0e1a14"; border.color: "#1a3028"
            visible: backend && backend.installPhase === "done"
            ColumnLayout { anchors.centerIn: parent; spacing: 6
                Text { Layout.alignment: Qt.AlignHCenter; text: "安装完成"; font.pixelSize: 15; font.bold: true; color: "#4bc870" }
                Text { Layout.alignment: Qt.AlignHCenter; text: "版本 " + (backend ? backend.installVersionId : "") + " 已就绪"; font.pixelSize: 11; color: "#608868" }
            }
        }

        Item { Layout.fillHeight: true }
    }

    // ── Toast on install complete ──
    Connections {
        target: backend
        function onInstallPhaseChanged() {
            if (backend && backend.installPhase === "done") {
                toastManager.show("🎉 安装完成")
            }
        }
    }
}
