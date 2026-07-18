// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: versionSelectOverlay
    readonly property bool hasBg: backend && typeof backend.customBgPath === "string" && backend.customBgPath.length > 0
    anchors.fill: parent; color: hasBg ? "transparent" : "#0c0f16"; z: 5
    property int activeGameDirIndex: 0
    property var backend: null
    property var toastManager: null
    property var appWindow: null

    // backend is set by Loader.onLoaded AFTER Component.onCompleted
    // Must watch for backend change to trigger version scan
    onBackendChanged: {
        if (backend) {
            backend.refreshVersionDetails()
            deferRefreshTimer.start()
            // Direct populate — Connections.enabled not re-evaluated when signal fires
            versionRightPanel.populateVersionDetails()
        }
    }

    // 延迟刷新定时器 — 先更新UI再扫描文件避免卡顿
    Timer {
        id: deferRefreshTimer
        interval: 80
        onTriggered: {
            if (backend) { backend.refreshInstalledList(); backend.refreshGameDirInfo() }
        }
    }

    Rectangle { x: 16; y: 16; height: 28; width: 80; radius: StyleTokens.radiusSm; color: "transparent"
        scale: vsBackHover.containsMouse ? 1.06 : 1.0
        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
        Text { anchors.centerIn: parent; text: qsTr("\u2190 返回启动"); color: "#a8b0c0"; font.pixelSize: StyleTokens.fontSizeSm }
        MouseArea { id: vsBackHover; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { showVersionSelect = false } }
    }
    RowLayout {
        id: vsContent
        anchors.fill: parent; anchors.margins: 16; anchors.topMargin: 52; spacing: 16

        // ── Content entrance ──
        opacity: 0
        Component.onCompleted: vsContent.opacity = 1
        Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
        Rectangle {
            Layout.preferredWidth: Math.min(220, parent.width * 0.35); Layout.fillHeight: true
            color: "#11141c"; radius: StyleTokens.radiusLg; border.color: StyleTokens.bgInput
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 14; spacing: 6
                Text { text: qsTr("游戏文件夹"); font.pixelSize: StyleTokens.fontSizeXs; color: "#9ca0b4"; font.letterSpacing: 1.5 }
                ListModel { id: gameDirModel }
                Component.onCompleted: {
                    var dirs = backend ? backend.gameDirectories : []
                    for (var d = 0; d < dirs.length; d++) {
                        gameDirModel.append({ path: dirs[d], display: d === 0 ? ".minecraft（默认）" : dirs[d] })
                    }
                }
                Connections {
                    target: backend; enabled: backend !== null
                    function onGameDirChanged() {
                        gameDirModel.clear()
                        var dirs = backend.gameDirectories
                        for (var d = 0; d < dirs.length; d++) {
                            gameDirModel.append({ path: dirs[d], display: d === 0 ? ".minecraft（默认）" : dirs[d] })
                        }
                    }
                }
                ScrollView {
                    Layout.fillWidth: true; Layout.preferredHeight: Math.min(gameDirModel.count * 36 + 4, 160)
                    clip: true
                    ListView {
                        id: gameDirList
                        anchors.fill: parent
                        model: gameDirModel
                        spacing: 2
                        delegate: Rectangle {
                            id: dirItem
                            width: gameDirList.width
                            height: 36; radius: StyleTokens.radiusMd
                            color: dirMouse.containsMouse ? "#1a1f2e" : (versionSelectOverlay.activeGameDirIndex === index ? "#0e131a" : "transparent")
                            border.color: versionSelectOverlay.activeGameDirIndex === index ? "#2a4eb8" : "transparent"
                            scale: dirMouse.containsMouse ? 1.02 : 1.0
                            Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

                            // ── Staggered entrance ──
                            opacity: 0
                            Component.onCompleted: dirItem.opacity = 1
                            Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
                            Text {
                                anchors.left: parent.left; anchors.leftMargin: 12
                                anchors.right: parent.right; anchors.rightMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                text: model.display
                                font.pixelSize: StyleTokens.fontSizeSm
                                color: versionSelectOverlay.activeGameDirIndex === index ? "#8aa8f0" : "#c0c8d8"
                                elide: Text.ElideRight
                                maximumLineCount: 1
                            }
                            ToolTip {
                                visible: dirMouse.containsMouse
                                text: model.path || model.display
                                delay: 600
                            }
                            MouseArea {
                                id: dirMouse; anchors.fill: parent; hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                onClicked: {
                                    versionSelectOverlay.activeGameDirIndex = index
                                    if (backend) { backend.setGameDir(index); toastManager.show("正在扫描版本...") }
                                }
                                onPressed: function(mouse) {
                                    if (mouse.button === Qt.RightButton) {
                                        if (index === 0) {
                                            if (backend) backend.openGameDir()
                                        } else {
                                            confirmDialog.title = "移除文件夹"
                                            confirmDialog.message = "确定要移除 " + model.display + " 吗？\n（不会删除本地文件）"
                                            confirmDialog.onAccept = function() { if (backend) backend.removeGameDir(index) }
                                            confirmDialog.visible = true
                                        }
                                        mouse.accepted = true
                                    }
                                }
                            }
                        }
                    }
                }
                Item { height: 4; width: 1 }

                // Directory info
                Rectangle {
                    Layout.fillWidth: true; height: childrenRect.height + 16; radius: StyleTokens.radiusMd
                    color: StyleTokens.bgPrimary
                    visible: backend ? (backend.gameDirInfo !== undefined && backend.gameDirInfo.versionCount > 0) : false
                    ColumnLayout {
                        anchors.left: parent.left; anchors.leftMargin: 12
                        anchors.right: parent.right; anchors.rightMargin: 12
                        anchors.top: parent.top; anchors.topMargin: 8
                        spacing: 4
                        RowLayout {
                            Text { text: qsTr("版本"); font.pixelSize: StyleTokens.fontSizeXs; color: "#a8b0c0"; Layout.preferredWidth: 40 }
                            Text { text: backend ? (backend.gameDirInfo.versionCount || 0) : 0; font.pixelSize: StyleTokens.fontSizeSm; font.weight: Font.Medium; color: StyleTokens.accentLink }
                        }
                        RowLayout {
                            Text { text: qsTr("模组"); font.pixelSize: StyleTokens.fontSizeXs; color: "#a8b0c0"; Layout.preferredWidth: 40 }
                            Text { text: backend ? (backend.gameDirInfo.modCount || 0) : 0; font.pixelSize: StyleTokens.fontSizeSm; font.weight: Font.Medium; color: StyleTokens.textSecondary }
                        }
                        RowLayout {
                            Text { text: qsTr("占用"); font.pixelSize: StyleTokens.fontSizeXs; color: "#a8b0c0"; Layout.preferredWidth: 40 }
                            Text { text: backend ? (backend.gameDirInfo.sizeDisplay || "") : ""; font.pixelSize: StyleTokens.fontSizeSm; font.weight: Font.Medium; color: StyleTokens.textSecondary }
                        }
                    }
                }

                Item { height: 4; width: 1 }
                Rectangle { Layout.fillWidth: true; height: 30; radius: StyleTokens.radiusMd; color: "transparent"; border.color: "#1e2230"; border.width: 1
                    scale: addDirHover.containsMouse ? 1.03 : 1.0
                    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                    Text { anchors.centerIn: parent; text: qsTr("+ 添加文件夹"); font.pixelSize: StyleTokens.fontSizeSm; color: addDirHover.containsMouse ? "#b0b8e0" : "#9498a8" }
                    MouseArea {
                        id: addDirHover
                        anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: { toastManager.show("功能开发中，请前往文件夹手动添加") }
                    }
                }
                Rectangle { Layout.fillWidth: true; height: 30; radius: StyleTokens.radiusMd; color: "transparent"; border.color: "#1e2230"; border.width: 1
                    scale: importHover.containsMouse ? 1.03 : 1.0
                    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                    Text { anchors.centerIn: parent; text: qsTr("导入整合包"); font.pixelSize: StyleTokens.fontSizeSm; color: importHover.containsMouse ? "#b0b8e0" : "#9498a8" }
                    MouseArea {
                        id: importHover
                        anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            var importOverlay = appWindow ? appWindow.modpackImportOverlay : null
                            if (importOverlay) {
                                importOverlay.show()
                            } else if (toastManager) {
                                toastManager.show("无法打开导入面板", "error")
                            }
                        }
                    }
                }

                // Disk space bar
                Rectangle {
                    Layout.fillWidth: true; height: 36; radius: StyleTokens.radiusMd; color: StyleTokens.bgPrimary
                    Rectangle { anchors.left: parent.left; anchors.leftMargin: 10; anchors.verticalCenter: parent.verticalCenter
                        width: 14; height: 14; radius: StyleTokens.radiusMd
                        color: backend && backend.diskPercent > 90 ? "#c05050" : (backend && backend.diskPercent > 70 ? "#e0a040" : "#4bc870")
                    }
                    Text {
                        anchors.left: parent.left; anchors.leftMargin: 30; anchors.verticalCenter: parent.verticalCenter
                        text: {
                            if (!backend) return "磁盘信息不可用"
                            var pct = backend.diskPercent
                            var freeGb = (backend.diskFree / 1073741824).toFixed(1)
                            var status = pct > 95 ? "危险" : (pct > 80 ? "偏低" : "正常")
                            return "剩余 " + freeGb + " GB  (" + status + ")"
                        }
                        font.pixelSize: StyleTokens.fontSizeSm; color: "#808898"
                    }
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true
            color: "#11141c"; radius: StyleTokens.radiusLg; border.color: StyleTokens.bgInput
            ColumnLayout {
                id: versionRightPanel
                anchors.fill: parent; anchors.margins: 12; spacing: 6

                // Header row: title + refresh + search + sort
                property bool verRefreshPressed: false
                property bool installBtnPressed: false
                RowLayout {
                    Layout.fillWidth: true; spacing: 8
                    Text { text: qsTr("已安装版本"); font.pixelSize: StyleTokens.fontSizeXs; color: "#9ca0b4"; font.letterSpacing: 1.5 }
                    // Refresh installed versions button
                    Rectangle {
                        id: verRefreshBtn
                        width: verRefreshText.implicitWidth + 16; height: 28; radius: StyleTokens.radiusSm
                        color: verRefreshHover.hovered ? "#1a2848" : "#0d1018"
                        border.color: verRefreshHover.hovered ? "#5068c8" : "#1a1f2e"
                        border.width: 1
                        scale: versionRightPanel.verRefreshPressed ? 0.88 : (verRefreshHover.hovered ? 1.06 : 1.0)
                        Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        Behavior on border.color { ColorAnimation { duration: 150 } }
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        Text {
                            id: verRefreshText
                            anchors.centerIn: parent
                            text: qsTr("⟳ 刷新"); font.pixelSize: StyleTokens.fontSizeSm
                            color: verRefreshHover.hovered ? "#8aa8f0" : "#7e8596"
                        }
                        HoverHandler { id: verRefreshHover }
                        ToolTip { visible: verRefreshHover.hovered; text: qsTr("重新扫描已安装版本"); delay: 500 }
                        MouseArea {
                            anchors.fill: parent; hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onPressed: versionRightPanel.verRefreshPressed = true
                            onReleased: versionRightPanel.verRefreshPressed = false
                            onClicked: {
                                if (backend) {
                                    backend.refreshVersionDetails()
                                    toastManager.show("正在扫描版本...")
                                }
                            }
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true; height: 28; radius: StyleTokens.radiusSm; color: StyleTokens.bgPrimary
                        border.color: searchField.activeFocus ? "#5068c8" : "#1a1f2e"
                        TextInput {
                            id: searchField; anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 10
                            color: "#e4e8f2"; font.pixelSize: StyleTokens.fontSizeSm
                            verticalAlignment: TextInput.AlignVCenter
                            Text {
                                anchors.left: parent.left; anchors.leftMargin: 10
                                anchors.verticalCenter: parent.verticalCenter
                                text: qsTr("搜索版本..."); color: "#9ca0b4"; font.pixelSize: StyleTokens.fontSizeSm
                                visible: !searchField.text
                            }
                        }
                    }
                    // Install button — shortcut to download new versions
                    Rectangle {
                        width: 28; height: 28; radius: StyleTokens.radiusSm; color: installBtnHover.hovered ? "#2553a8" : "#3a4eb8"
                        scale: versionRightPanel.installBtnPressed ? 0.9 : 1.0
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        Text { anchors.centerIn: parent; text: "+"; font.pixelSize: StyleTokens.fontSizeXl; font.weight: Font.Bold; color: StyleTokens.textPrimary }
                        HoverHandler { id: installBtnHover }
                        ToolTip { visible: installBtnHover.hovered; text: qsTr("安装新版本"); delay: 500 }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onPressed: versionRightPanel.installBtnPressed = true
                            onReleased: versionRightPanel.installBtnPressed = false
                            onClicked: { showVersionSelect = false; switchPage(1); toastManager.show("正在前往下载页面") }  // 导航到下载页
                        }
                    }
                    // Sort button
                    Rectangle {
                        id: sortBtn
                        width: 70; height: 28; radius: StyleTokens.radiusSm; color: sortHover.hovered ? "#1a2848" : "#0d1018"
                        border.color: sortHover.hovered ? "#5068c8" : "#1a1f2e"
                        scale: sortPressed ? 0.92 : (sortHover.hovered ? 1.03 : 1.0)
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        property int versionSortIndex: 0
                        RowLayout {
                            anchors.centerIn: parent; spacing: 4
                            Text { text: sortBtn.versionSortIndex === 0 ? "↓ 版本" : (sortBtn.versionSortIndex === 1 ? "↓ 版本" : (sortBtn.versionSortIndex === 2 ? "↓ 大小" : "↓ 模组")); font.pixelSize: StyleTokens.fontSizeXs; color: "#b0b8c8" }
                        }
                        HoverHandler { id: sortHover }
                        property bool sortPressed: false
                        MouseArea {
                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onPressed: sortPressed = true
                            onReleased: sortPressed = false
                            onClicked: {
                                sortBtn.versionSortIndex = (sortBtn.versionSortIndex + 1) % 4
                                versionRightPanel.applyFilterSort()
                            }
                        }
                    }

                    // Loader filter
                    Rectangle {
                        id: loaderFilter
                        width: 80; height: 28; radius: StyleTokens.radiusSm; color: loaderFiltHover.hovered ? "#1a2848" : "#0d1018"
                        border.color: loaderFiltHover.hovered ? "#5068c8" : "#1a1f2e"
                        scale: loadFiltPressed ? 0.92 : (loaderFiltHover.hovered ? 1.03 : 1.0)
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        property int loaderFilterIndex: 0  // 0=全部 1=原版 2=Forge 3=Fabric 4=NeoForge 5=Quilt
                        property var loaderLabels: ["全部类型", "原版", "Forge", "Fabric", "NeoForge", "Quilt"]
                        property bool loadFiltPressed: false
                        RowLayout {
                            anchors.centerIn: parent; spacing: 4
                            Text { text: loaderFilter.loaderLabels[loaderFilter.loaderFilterIndex]; font.pixelSize: StyleTokens.fontSizeXs; color: "#b0b8c8" }
                        }
                        HoverHandler { id: loaderFiltHover }
                        MouseArea {
                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onPressed: loadFiltPressed = true
                            onReleased: loadFiltPressed = false
                            onClicked: {
                                loaderFilter.loaderFilterIndex = (loaderFilter.loaderFilterIndex + 1) % loaderFilter.loaderLabels.length
                                versionRightPanel.applyFilterSort()
                            }
                        }
                    }
                }

                // Version list from detail model
                ListModel { id: versionDetailModel }
                ListModel { id: versionFilteredModel }

                function applyFilterSort() {
                    var data = []
                    for (var i = 0; i < versionDetailModel.count; i++) {
                        data.push(versionDetailModel.get(i))
                    }
                    var srch = searchField.text.toLowerCase()
                    data = data.filter(function(d) {
                        var nameMatch = !srch || d.id.toLowerCase().indexOf(srch) >= 0 || d.loaderType.toLowerCase().indexOf(srch) >= 0
                        if (!nameMatch) return false
                        var li = loaderFilter.loaderFilterIndex
                        if (li === 0) return true  // 全部
                        var lt = (d.loaderType || "原版").toLowerCase()
                        if (li === 1) return lt === "原版" || lt === "vanilla"
                        if (li === 2) return lt === "forge"
                        if (li === 3) return lt === "fabric"
                        if (li === 4) return lt === "neoforge"
                        if (li === 5) return lt === "quilt"
                        return true
                    })
                    // Sort: release > snapshot > old, then by releaseTime descending (newest first)
                    function typeOrder(vt) {
                        if (vt === "release") return 0
                        if (vt === "snapshot") return 1
                        return 2  // old / unknown
                    }
                    data.sort(function(a, b) {
                        var si = sortBtn.versionSortIndex
                        if (si === 0) {
                            var toA = typeOrder(a.versionType), toB = typeOrder(b.versionType)
                            if (toA !== toB) return toA - toB
                            var ta = a.releaseTimeMs || 0, tb = b.releaseTimeMs || 0
                            return tb - ta  // newest first
                        }
                        if (si === 1) {
                            var toA = typeOrder(a.versionType), toB = typeOrder(b.versionType)
                            if (toA !== toB) return toA - toB
                            var ta = a.releaseTimeMs || 0, tb = b.releaseTimeMs || 0
                            return ta - tb  // oldest first
                        }
                        if (si === 2) return b.sizeBytes - a.sizeBytes
                        return b.modCount - a.modCount
                    })
                    versionFilteredModel.clear()
                    for (var j = 0; j < data.length; j++) {
                        versionFilteredModel.append(data[j])
                    }
                }

                function populateVersionDetails() {
                    versionDetailModel.clear()
                    var details = backend ? backend.versionDetails : []
                    if (!details || details.length === 0) return
                    for (var i = 0; i < details.length; i++) {
                        versionDetailModel.append(details[i])
                    }
                    applyFilterSort()
                }

                Connections {
                    target: backend; enabled: backend !== null
                    function onVersionDetailsChanged() { versionRightPanel.populateVersionDetails() }
                    function onVersionDetailsReady() {
                        versionRightPanel.populateVersionDetails()
                    }
                }

                Connections {
                    target: searchField
                    function onTextChanged() { versionRightPanel.applyFilterSort() }
                }

                ScrollView {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    clip: true
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded
                    ListView {
                        id: versionSelectList
                        anchors.fill: parent
                        model: versionFilteredModel
                        spacing: 4
                        clip: true
                        delegate: Rectangle {
                            id: versionItem
                            width: versionSelectList.width - 4
                            height: 60; radius: StyleTokens.radiusMd
                            color: verMouse2.containsMouse ? "#191e2a" : "transparent"
                            border.color: currentSelectedVersion === model.id ? "#4a5ec8" : "transparent"
                            border.width: currentSelectedVersion === model.id ? 1.5 : 0

                            // ── Staggered entrance animation ──
                            property int _entranceDelay: index * 40
                            opacity: 0
                            Component.onCompleted: verEntranceTimer.start()
                            Timer {
                                id: verEntranceTimer
                                interval: versionItem._entranceDelay
                                onTriggered: versionItem.opacity = 1
                            }
                            Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }

                            // ── Icon mapping (Minecraft blocks) ──
                            function getBlockIcon() {
                                var lt = model.loaderType || ""
                                if (lt === "Forge") return "icons/blocks/Anvil.png"
                                if (lt === "Fabric") return "icons/blocks/Fabric.png"
                                if (lt === "NeoForge") return "icons/blocks/NeoForge.png"
                                if (lt === "Quilt") return "icons/blocks/RedstoneBlock.png"
                                // Vanilla — by version type
                                var vt = model.versionType || "release"
                                if (vt === "snapshot") return "icons/blocks/CommandBlock.png"
                                if (vt === "old") return "icons/blocks/CobbleStone.png"
                                return "icons/blocks/Grass.png"
                            }
                            function getVtypeLabel() {
                                var vt = model.versionType || "release"
                                if (vt === "snapshot") return "快照版"
                                if (vt === "old") return "旧版"
                                return "正式版"
                            }
                            function parseMcVersion() {
                                var id = model.id || ""
                                var dash = id.indexOf("-")
                                return dash > 0 ? id.substring(0, dash) : id
                            }
                            function formatLoaderInfo() {
                                var lt = model.loaderType || ""
                                var isVanilla = (lt === "原版" || lt === "")
                                if (isVanilla) return ""
                                var lv = model.loaderVersion || ""
                                return lt + (lv ? "-" + lv : "")
                            }

                            ColumnLayout {
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left; anchors.leftMargin: 12
                                anchors.right: parent.right; anchors.rightMargin: 8
                                spacing: 2

                                // ── Row 1: Version name ──
                                RowLayout {
                                    Layout.fillWidth: true; spacing: 6
                                    Text {
                                        text: model.id || ""
                                        font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.DemiBold; color: StyleTokens.textSecondary
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }
                                    // Size + mod count (right-aligned)
                                    Text {
                                        text: model.sizeDisplay || ""
                                        font.pixelSize: StyleTokens.fontSizeSm; font.weight: Font.Medium; color: "#808898"
                                        visible: (model.sizeDisplay || "") !== ""
                                    }
                                    Text {
                                        text: model.modCount > 0 ? model.modCount + " 模组" : ""
                                        font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.textTertiary
                                        visible: model.modCount > 0
                                    }
                                }

                                // ── Row 2: Icon + MC version + loader info ──
                                RowLayout {
                                    Layout.fillWidth: true; spacing: 4
                                    Image {
                                        source: getBlockIcon()
                                        Layout.preferredWidth: 18; Layout.preferredHeight: 18
                                        fillMode: Image.PreserveAspectFit
                                        smooth: true
                                    }
                                    Text {
                                        text: parseMcVersion()
                                        font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary
                                        visible: parseMcVersion() !== ""
                                    }
                                    Text {
                                        text: "，"
                                        font.pixelSize: StyleTokens.fontSizeSm; color: "#606878"
                                        visible: parseMcVersion() !== "" && formatLoaderInfo() !== ""
                                    }
                                    Text {
                                        text: formatLoaderInfo()
                                        font.pixelSize: StyleTokens.fontSizeSm; color: "#c0c8d8"
                                        visible: formatLoaderInfo() !== ""
                                    }
                                }
                            }

                            MouseArea {
                                id: verMouse2
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                onClicked: function(mouse) {
                                    if (backend) backend.setSelectedVersion(model.id)
                                    showVersionSelect = false
                                }
                                onPressed: function(mouse) {
                                    if (mouse.button === Qt.RightButton) {
                                        if (backend) backend.setSelectedVersion(model.id)
                                        showVersionSettings = true
                                        mouse.accepted = true
                                    }
                                }
                            }
                        }
                    }
                }

                // Empty state
                Text {
                    Layout.alignment: Qt.AlignHCenter; Layout.topMargin: 40
                    text: versionFilteredModel.count === 0 && backend && backend.versionDetails && backend.versionDetails.length > 0
                          ? "没有匹配的版本" : (backend && backend.installedVersions && backend.installedVersions.length === 0 ? "还没有安装任何版本\n前往下载页面安装第一个版本吧" : "")
                    font.pixelSize: StyleTokens.fontSizeSm; color: "#a8b0c0"
                    horizontalAlignment: Text.AlignHCenter
                    visible: versionFilteredModel.count === 0
                }
            }
        }
    }
}
