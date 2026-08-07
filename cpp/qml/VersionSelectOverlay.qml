// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: versionSelectOverlay
    readonly property bool hasBg: backend && typeof backend.customBgPath === "string" && backend.customBgPath.length > 0
    anchors.fill: parent; color: hasBg ? "transparent" : StyleTokens.bgPrimary; z: 5
    property var backend: null
    property var toastManager: null
    property var appWindow: null
    property var mainWindow: null

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
            if (backend) { backend.refreshInstalledList() }
        }
    }

    BackButton { x: 16; y: 16; onClicked: appWindow.showVersionSelect = false }
    RowLayout {
        id: vsContent
        anchors.fill: parent; anchors.margins: 16; anchors.topMargin: 52; spacing: 16

        // ── Content entrance ──
        opacity: 0
        Component.onCompleted: vsContent.opacity = 1
        Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
        // ── 版本列表卡片：占满整页（2026-08-07 左侧“版本文件夹”卡片已移除）──
        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true
            color: StyleTokens.bgSecondary; radius: StyleTokens.radiusLg; border.color: StyleTokens.bgInput
            ColumnLayout {
                id: versionRightPanel
                anchors.fill: parent; anchors.margins: 12; spacing: 6

                // Header row: title + refresh + search + sort
                RowLayout {
                    Layout.fillWidth: true; spacing: 8
                    Text { text: qsTr("已安装版本"); font.pixelSize: StyleTokens.fontSizeXs; color: "#9ca0b4"; font.letterSpacing: 1.5 }
                    RefreshButton {
                        onClicked: {
                            if (backend) {
                                backend.refreshVersionDetails()
                                toastManager.show("正在扫描版本...")
                            }
                        }
                    }
                    SearchBox {
                        id: searchField
                        Layout.fillWidth: true
                        placeholderText: qsTr("搜索版本...")
                    }
                    // Import modpack button — 通用组件样式，与搜索栏平齐（2026-08-07 自左侧卡片迁入）
                    ShadowButton {
                        id: importPackBtn
                        text: qsTr("导入整合包")
                        iconSource: "icons/lucide/package.svg"
                        iconSize: 14
                        accentColor: StyleTokens.accent
                        btnWidth: 96
                        onClicked: {
                            var importOverlay = appWindow ? appWindow.modpackImportOverlay : null
                            if (importOverlay) {
                                importOverlay.show()
                            } else if (toastManager) {
                                toastManager.show("无法打开导入面板", "error")
                            }
                        }
                    }
                    // Install button — shortcut to download new versions
                    ShadowIconButton {
                        id: installBtn
                        icon: "+"; iconPixelSize: 18
                        defaultColor: StyleTokens.accent; hoverColor: "#2553a8"
                        iconColor: "#ffffff"; iconHoverColor: "#ffffff"
                        onClicked: { showVersionSelect = false; switchPage(1); toastManager.show("正在前往下载页面") }
                        ToolTip { visible: installBtn._hovered; text: qsTr("安装新版本"); delay: 500 }
                    }
                    // Sort button
                    Rectangle {
                        id: sortBtn
                        width: 70; height: 28; radius: StyleTokens.radiusSm; color: sortHover.hovered ? StyleTokens.accentSubtle : "#0d1018"
                        border.color: sortHover.hovered ? StyleTokens.accentHover : StyleTokens.bgCard
                        scale: sortPressed ? 0.92 : (sortHover.hovered ? 1.03 : 1.0)
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        property int versionSortIndex: 0
                        RowLayout {
                            anchors.centerIn: parent; spacing: 4
                            Text { text: sortBtn.versionSortIndex === 0 ? "↓ 版本" : (sortBtn.versionSortIndex === 1 ? "↓ 版本" : (sortBtn.versionSortIndex === 2 ? "↓ 大小" : "↓ 模组")); font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.textTertiary }
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
                        width: 80; height: 28; radius: StyleTokens.radiusSm; color: loaderFiltHover.hovered ? StyleTokens.accentSubtle : "#0d1018"
                        border.color: loaderFiltHover.hovered ? StyleTokens.accentHover : StyleTokens.bgCard
                        scale: loadFiltPressed ? 0.92 : (loaderFiltHover.hovered ? 1.03 : 1.0)
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        property int loaderFilterIndex: 0  // 0=全部 1=原版 2=Forge 3=Fabric 4=NeoForge 5=Quilt
                        property var loaderLabels: ["全部类型", "原版", "Forge", "Fabric", "NeoForge", "Quilt"]
                        property bool loadFiltPressed: false
                        RowLayout {
                            anchors.centerIn: parent; spacing: 4
                            Text { text: loaderFilter.loaderLabels[loaderFilter.loaderFilterIndex]; font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.textTertiary }
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
                    if (!details || details.length === 0) {
                        // 扫描结果为空（如 versions 文件夹被清空）时必须同步清空展示列表，
                        // 否则 ListView 绑定的 versionFilteredModel 残留旧数据，点刷新看不到变化
                        versionFilteredModel.clear()
                        return
                    }
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

                // 异步扫描加载指示器（覆盖在版本列表上方）
                Rectangle {
                    anchors.fill: parent; visible: backend && backend.isScanningVersions
                    color: StyleTokens.bgSecondary
                    z: 10
                    ColumnLayout {
                        anchors.centerIn: parent; spacing: 12
                        Text {
                            text: "⏳"; font.pixelSize: 28
                            Layout.alignment: Qt.AlignHCenter
                            NumberAnimation on rotation { from: 0; to: 360; duration: 2000; loops: Animation.Infinite }
                        }
                        Text { text: qsTr("正在扫描版本..."); font.pixelSize: StyleTokens.fontSizeSm; color: "#7e8596"; Layout.alignment: Qt.AlignHCenter }
                    }
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
                                // 整合包：显示整合包图标而非加载器图标（.shadow_modpack 标记由导入流程写入）
                                if (model.isModpack) return "icons/lucide/package.svg"
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
                                        // 整合包行：本地图标（导入时解码落盘）→ 标记 URL → 通用图标，逐级回退
                                        source: model.isModpack
                                            ? (model.modpackIconPath || model.modpackIcon || "icons/lucide/package.svg")
                                            : getBlockIcon()
                                        Layout.preferredWidth: 18; Layout.preferredHeight: 18
                                        fillMode: Image.PreserveAspectFit
                                        smooth: true
                                        onStatusChanged: {
                                            if (status === Image.Error) source = "icons/lucide/package.svg"
                                        }
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
                    font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary
                    horizontalAlignment: Text.AlignHCenter
                    visible: versionFilteredModel.count === 0
                }
            }
        }
    }
}
