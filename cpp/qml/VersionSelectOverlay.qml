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

    // ── 视图模式：false=版本选择页，true=游戏文件夹页（2026-08-18）──
    property bool foldersMode: false
    property int importedFolderCount: 0   // 已导入文件夹条目数（文件夹页空态判断；放根组件作用域）
    property var mcFolder: backend ? backend.mcFolder : null

    // backend is set by Loader.onLoaded AFTER Component.onCompleted
    // Must watch for backend change to trigger version scan
    onBackendChanged: {
        if (backend) {
            backend.refreshVersionDetails()
            deferRefreshTimer.start()
            // Direct populate — Connections.enabled not re-evaluated when signal fires
            versionRightPanel.populateVersionDetails()
            if (mcFolder) mcFolder.refreshFolders()   // 预载文件夹列表
        }
    }

    // 进入文件夹页：确保列表最新（用户可能外部改动过）
    onFoldersModeChanged: {
        if (foldersMode && mcFolder) mcFolder.refreshFolders()
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
            color: hasBg ? "#9911141C" : StyleTokens.bgSecondary; radius: StyleTokens.radiusLg; border.color: StyleTokens.bgInput
            ColumnLayout {
                id: versionRightPanel
                anchors.fill: parent; anchors.margins: 12; spacing: 6

                // ── Header row: title + refresh + search + 切换游戏文件夹 + 导入整合包 ──
                RowLayout {
                    Layout.fillWidth: true; spacing: 8
                    Text {
                        text: foldersMode ? qsTr("已导入文件夹") : qsTr("已安装版本")
                        font.pixelSize: StyleTokens.fontSizeXs; color: "#9ca0b4"; font.letterSpacing: 1.5
                    }
                    RefreshButton {
                        onClicked: {
                            if (foldersMode) {
                                // 刷新文件夹条目列表（评估结论：保留——覆盖外部删除/改名/移动等场景，成本低）
                                if (mcFolder) mcFolder.refreshFolders()
                                toastManager.show(qsTr("正在刷新文件夹列表..."))
                            } else if (backend) {
                                backend.refreshVersionDetails()
                                toastManager.show("正在扫描版本...")
                            }
                        }
                    }
                    SearchBox {
                        id: searchField
                        Layout.fillWidth: true
                        placeholderText: foldersMode ? qsTr("搜索文件夹...") : qsTr("搜索版本...")
                    }
                    // 切换游戏文件夹 / 返回版本选择页面（2026-08-18，搜索框与导入整合包中间）
                    ShadowButton {
                        id: toggleFolderBtn
                        text: foldersMode ? qsTr("返回版本选择页面") : qsTr("切换游戏文件夹")
                        iconSource: foldersMode ? "icons/lucide/arrow-left.svg" : "icons/lucide/folder-sync.svg"
                        iconSize: 14
                        accentColor: foldersMode ? StyleTokens.bgHover : StyleTokens.accent
                        btnWidth: foldersMode ? 132 : 116
                        onClicked: {
                            foldersMode = !foldersMode
                        }
                        Behavior on accentColor { ColorAnimation { duration: 180; easing.type: Easing.OutCubic } }
                    }
                    // Import modpack button — 通用组件样式，与搜索栏平齐（2026-08-07 自左侧卡片迁入）
                    ShadowButton {
                        id: importPackBtn
                        text: qsTr("导入整合包")
                        iconSource: "icons/lucide/package.svg"
                        iconSize: 14
                        accentColor: StyleTokens.accent
                        btnWidth: 96
                        visible: !foldersMode
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
                        visible: !foldersMode
                        onClicked: { showVersionSelect = false; switchPage(1); toastManager.show("正在前往下载页面") }
                        ToolTip { visible: installBtn._hovered; text: qsTr("安装新版本"); delay: 500 }
                    }
                    // Sort button
                    Rectangle {
                        id: sortBtn
                        visible: !foldersMode
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
                        visible: !foldersMode
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

                // ══════════════════════════════════════════════════
                // 版本列表（版本选择模式）
                // ══════════════════════════════════════════════════
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

                // ══════════════════════════════════════════════════
                // 游戏文件夹列表（游戏文件夹模式，2026-08-18）
                // ══════════════════════════════════════════════════
                ListModel { id: folderListModel }

                function basenameOf(p) {
                    p = (p || "").replace(/\\/g, "/")
                    var parts = p.split("/")
                    return parts.length > 0 ? parts[parts.length - 1] : p
                }

                function rebuildFolderList() {
                    folderListModel.clear()
                    importedFolderCount = 0
                    var items = mcFolder ? mcFolder.folders : []
                    var srch = searchField.text.trim().toLowerCase()
                    // 1) 固有文件夹恒为第一项
                    for (var i = 0; i < items.length; i++) {
                        if (items[i].isDefault) { folderListModel.append(items[i]); break }
                    }
                    // 2) 常驻「添加文件夹」卡片
                    folderListModel.append({ isAddCard: true })
                    // 3) 已导入文件夹（搜索过滤）
                    for (var j = 0; j < items.length; j++) {
                        var it = items[j]
                        if (it.isDefault) continue
                        if (srch !== "") {
                            var nm = (it.name || "").toLowerCase()
                            var p = (it.path || "").toLowerCase()
                            if (nm.indexOf(srch) < 0 && p.indexOf(srch) < 0) continue
                        }
                        folderListModel.append(it)
                        importedFolderCount++
                    }
                }

                Connections {
                    target: mcFolder; enabled: mcFolder !== null
                    function onFoldersChanged() { versionRightPanel.rebuildFolderList() }
                }

                Connections {
                    target: searchField
                    function onTextChanged() {
                        if (foldersMode) versionRightPanel.rebuildFolderList()
                        else versionRightPanel.applyFilterSort()
                    }
                }

                // ── 添加文件夹流程 ──
                function onAddFolderClicked() {
                    if (!mcFolder) return
                    var path = mcFolder.pickFolderDialog()
                    if (path && path.length > 0) {
                        namePopup.openForAdd(path)
                    }
                }

                // ── 卡片点击：切换活动文件夹 ──
                function onFolderCardClicked(path, isDefault) {
                    if (!mcFolder) return
                    if (isDefault) {
                        mcFolder.revertToDefault()
                        toastManager.show(qsTr("已切换到默认游戏目录"))
                    } else {
                        mcFolder.setActiveFolder(path)
                        toastManager.show(qsTr("已切换游戏文件夹"))
                    }
                    foldersMode = false
                }

                // ── 设置菜单（删除条目/打开/重命名）──
                function openFolderMenu(btn, path, name, isDefault) {
                    folderMenuPopup.menuPath = path
                    folderMenuPopup.menuName = name
                    folderMenuPopup.menuIsDefault = isDefault
                    var pos = btn.mapToItem(versionSelectOverlay, 0, 0)
                    folderMenuPopup.x = pos.x + btn.width - folderMenuPopup.width
                    folderMenuPopup.y = pos.y + btn.height + 4
                    folderMenuPopup.open()
                }

                function openRemoveConfirm(path, name) {
                    removeFolderConfirm.onAccept = function() {
                        if (mcFolder) {
                            mcFolder.removeGameFolder(path)
                            toastManager.show(qsTr("已移除文件夹条目（未删除文件夹）"))
                        }
                    }
                    removeFolderConfirm.title = qsTr("移除文件夹条目")
                    removeFolderConfirm.message = qsTr("仅从启动器移除「%1」的条目，不会删除文件夹本身。").arg(name)
                    removeFolderConfirm.opened = true
                }

                // 异步扫描加载指示器（覆盖在版本列表上方）
                Rectangle {
                    anchors.fill: parent; visible: !foldersMode && backend && backend.isScanningVersions
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

                // ── 版本列表 ──
                ScrollView {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    visible: !foldersMode
                    clip: true
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded
                    opacity: foldersMode ? 0 : 1
                    Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
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
                                        showVersionSelect = false  // 进版本设置同时收起版本选择，防止透明背景（自定义背景）下两浮层叠加
                                        mouse.accepted = true
                                    }
                                }
                            }
                        }
                    }
                }

                // ── 游戏文件夹列表 ──
                ScrollView {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    visible: foldersMode
                    clip: true
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded
                    opacity: foldersMode ? 1 : 0
                    Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                    ListView {
                        id: folderList
                        anchors.fill: parent
                        model: folderListModel
                        spacing: 4
                        clip: true
                        delegate: Rectangle {
                            id: folderItem
                            // 注意：不能声明 required property var model —— 那会关闭 ListView
                            // 隐式注入的 index（导致 "index is not defined"）。用隐式 model 访问
                            // model.isAddCard 等角色（与版本列表 delegate 同模式）。
                            width: folderList.width - 4
                            height: model.isAddCard ? 46 : 62
                            radius: StyleTokens.radiusMd
                            color: model.isAddCard ? "transparent"
                                 : ((fCardBody.containsMouse || model.active) ? "#191e2a" : "transparent")
                            border.color: model.isAddCard ? "#33557f"
                                : (model.active ? "#4a5ec8" : (fCardBody.containsMouse ? StyleTokens.bgHover : "transparent"))
                            border.width: model.isAddCard ? 1 : (model.active ? 1.5 : 0)
                            Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }

                            // ── Staggered entrance animation ──
                            property int _entranceDelay: index * 40
                            opacity: 0
                            Component.onCompleted: fEntranceTimer.start()
                            Timer {
                                id: fEntranceTimer
                                interval: folderItem._entranceDelay
                                onTriggered: folderItem.opacity = 1
                            }
                            Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }

                            // ═══ 添加文件夹卡片 ═══
                            MouseArea {
                                id: addCardBody
                                visible: model.isAddCard
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: versionRightPanel.onAddFolderClicked()
                                RowLayout {
                                    anchors.centerIn: parent; spacing: 8
                                    Image {
                                        source: "icons/lucide/folder-plus.svg"
                                        Layout.preferredWidth: 16; Layout.preferredHeight: 16
                                        opacity: 0.75
                                    }
                                    Text {
                                        text: qsTr("添加文件夹")
                                        font.pixelSize: StyleTokens.fontSizeSm
                                        color: addCardBody.containsMouse ? StyleTokens.textSecondary : StyleTokens.textTertiary
                                        Behavior on color { ColorAnimation { duration: 150 } }
                                    }
                                }
                            }

                            // ═══ 文件夹卡片 ═══
                            MouseArea {
                                id: fCardBody
                                visible: !model.isAddCard
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: versionRightPanel.onFolderCardClicked(model.path, model.isDefault)
                            }
                            RowLayout {
                                visible: !model.isAddCard
                                anchors.fill: parent
                                anchors.leftMargin: 12; anchors.rightMargin: 6
                                spacing: 10
                                Image {
                                    source: model.active ? "icons/lucide/folder-open.svg" : "icons/lucide/folder.svg"
                                    Layout.preferredWidth: 20; Layout.preferredHeight: 20
                                    Layout.alignment: Qt.AlignVCenter
                                    opacity: model.exists ? 1 : 0.45
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    RowLayout {
                                        Layout.fillWidth: true; spacing: 6
                                        Text {
                                            text: model.name || ""
                                            font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.DemiBold
                                            color: model.exists ? StyleTokens.textSecondary : StyleTokens.textTertiary
                                            Layout.fillWidth: true; elide: Text.ElideRight
                                        }
                                        // 固有文件夹特殊标签
                                        Rectangle {
                                            visible: model.isDefault
                                            Layout.preferredHeight: 20
                                            Layout.preferredWidth: 40
                                            Layout.alignment: Qt.AlignVCenter
                                            radius: StyleTokens.radiusSm
                                            color: StyleTokens.accentSubtle; border.color: StyleTokens.accent; border.width: 1
                                            Text {
                                                anchors.centerIn: parent
                                                text: qsTr("固有")
                                                font.pixelSize: 11; font.weight: Font.Medium; color: StyleTokens.accentLink
                                                horizontalAlignment: Text.AlignHCenter
                                            }
                                        }
                                        // 使用中标签
                                        Rectangle {
                                            visible: model.active && !model.isDefault
                                            Layout.preferredHeight: 20
                                            Layout.preferredWidth: 46
                                            Layout.alignment: Qt.AlignVCenter
                                            radius: StyleTokens.radiusSm
                                            color: "#123a22"; border.color: StyleTokens.success; border.width: 1
                                            Text {
                                                anchors.centerIn: parent
                                                text: qsTr("使用中")
                                                font.pixelSize: 11; font.weight: Font.Medium; color: StyleTokens.success
                                                horizontalAlignment: Text.AlignHCenter
                                            }
                                        }
                                        // 不存在标签
                                        Rectangle {
                                            visible: !model.exists
                                            Layout.preferredHeight: 20
                                            Layout.preferredWidth: 50
                                            Layout.alignment: Qt.AlignVCenter
                                            radius: StyleTokens.radiusSm
                                            color: StyleTokens.errorBg; border.color: StyleTokens.error; border.width: 1
                                            Text {
                                                anchors.centerIn: parent
                                                text: qsTr("不存在")
                                                font.pixelSize: 11; font.weight: Font.Medium; color: StyleTokens.errorLight
                                                horizontalAlignment: Text.AlignHCenter
                                            }
                                        }
                                        // 设置按钮（删除条目/打开/重命名）——与标题/标签同排，天然居中对齐
                                        ShadowIconButton {
                                            source: "icons/lucide/settings.svg"
                                            sourceWidth: 15; sourceHeight: 15
                                            type: "normal"
                                            Layout.alignment: Qt.AlignVCenter
                                            onClicked: versionRightPanel.openFolderMenu(this, model.path, model.name, model.isDefault)
                                            // 不挂 ToolTip：默认框风格不符（2026-08-18），点击即弹样式化菜单
                                        }
                                    }
                                    // 副标题：路径（小字）
                                    Text {
                                        text: model.path || ""
                                        font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.textTertiary
                                        Layout.fillWidth: true; elide: Text.ElideMiddle
                                    }
                                    // 信息行：布局 · 来源 · 版本数
                                    Text {
                                        visible: model.exists
                                        text: model.layoutName + (model.launcherName && model.launcherName !== "未知"
                                              ? " · " + model.launcherName : "") + " · " + model.versionCount + " 个版本"
                                        font.pixelSize: 10; color: StyleTokens.textSubtle
                                    }
                                }
                            }
                        }
                    }
                }

                // 文件夹空态提示
                Text {
                    Layout.alignment: Qt.AlignHCenter; Layout.topMargin: 40
                    visible: foldersMode && importedFolderCount === 0 && folderListModel.count <= 1
                    text: qsTr("还没有导入文件夹\n点击「添加文件夹」导入其他启动器的 .minecraft")
                    font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary
                    horizontalAlignment: Text.AlignHCenter
                }
                // 文件夹搜索无匹配
                Text {
                    Layout.alignment: Qt.AlignHCenter; Layout.topMargin: 40
                    visible: foldersMode && searchField.text.trim() !== "" && folderListModel.count <= 1
                    text: qsTr("没有匹配的文件夹")
                    font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
    }

    // ════════════════════════════════════════════════════════════
    // 设置菜单（删除条目 / 打开 / 重命名）
    // ════════════════════════════════════════════════════════════
    Popup {
        id: folderMenuPopup
        parent: versionSelectOverlay
        width: 148
        padding: 4
        modal: false  // 非模态：不压暗背景（2026-08-18 修复——modal 会蒙上一层灰色遮罩）
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        property string menuPath: ""
        property string menuName: ""
        property bool menuIsDefault: false

        background: Rectangle {
            radius: StyleTokens.radiusMd
            color: StyleTokens.surfaceOverlay
            border.color: StyleTokens.borderLight; border.width: 1
        }

        contentItem: Column {
            spacing: 2
            // 打开
            Rectangle {
                width: 140; height: 32; radius: StyleTokens.radiusSm
                color: optOpen.containsMouse ? StyleTokens.bgHover : "transparent"
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 8; spacing: 8
                    Image { source: "icons/lucide/folder-open.svg"; Layout.preferredWidth: 14; Layout.preferredHeight: 14 }
                    Text { text: qsTr("打开"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textSecondary }
                }
                MouseArea {
                    id: optOpen; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (backend) backend.openPath(folderMenuPopup.menuPath)
                        folderMenuPopup.close()
                    }
                }
            }
            // 重命名
            Rectangle {
                width: 140; height: 32; radius: StyleTokens.radiusSm
                color: optRename.containsMouse ? StyleTokens.bgHover : "transparent"
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 8; spacing: 8
                    Image { source: "icons/lucide/folder-sync.svg"; Layout.preferredWidth: 14; Layout.preferredHeight: 14 }
                    Text { text: qsTr("重命名"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textSecondary }
                }
                MouseArea {
                    id: optRename; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        folderMenuPopup.close()
                        namePopup.openForRename(folderMenuPopup.menuPath, folderMenuPopup.menuName)
                    }
                }
            }
            // 删除条目（固有文件夹不可删除）
            Rectangle {
                width: 140; height: 32; radius: StyleTokens.radiusSm
                color: optRemove.containsMouse ? "#2a1a1a" : "transparent"
                visible: !folderMenuPopup.menuIsDefault
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 8; spacing: 8
                    Image { source: "icons/lucide/trash-2.svg"; Layout.preferredWidth: 14; Layout.preferredHeight: 14; opacity: 0.85 }
                    Text { text: qsTr("删除条目"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.errorLight }
                }
                MouseArea {
                    id: optRemove; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        folderMenuPopup.close()
                        versionRightPanel.openRemoveConfirm(folderMenuPopup.menuPath, folderMenuPopup.menuName)
                    }
                }
            }
        }
    }

    // ════════════════════════════════════════════════════════════
    // 命名弹窗（添加 / 重命名共用）
    // ════════════════════════════════════════════════════════════
    GenericPopup {
        id: namePopup
        title: qsTr("命名游戏文件夹")
        cardWidth: 400
        opened: false
        property string mode: "add"       // add | rename
        property string targetPath: ""
        onClosed: opened = false
        onRejected: opened = false

        ColumnLayout {
            // 锚定内容容器 + 16px 对称边距：左右呼吸感一致，输入框撑满，按钮贴右缘（2026-08-18）
            anchors.left: parent.left; anchors.right: parent.right
            anchors.leftMargin: 16; anchors.rightMargin: 16
            anchors.top: parent.top; anchors.topMargin: 8
            spacing: 12

            Text {
                text: namePopup.mode === "add"
                      ? qsTr("为选中的文件夹命名（仅本启动器内可见）")
                      : qsTr("修改文件夹名称（仅本启动器内可见）")
                font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary
                wrapMode: Text.WordWrap; Layout.fillWidth: true
            }
            InputBox {
                id: nameInput
                Layout.fillWidth: true
                placeholderText: qsTr("文件夹名称")
                onAccepted: namePopup.confirm()
            }
            Text {
                text: namePopup.targetPath
                font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.textSubtle
                elide: Text.ElideMiddle; Layout.fillWidth: true
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 10
                ShadowButton {
                    text: qsTr("取消"); btnWidth: 80
                    accentColor: StyleTokens.bgHover
                    onClicked: namePopup.opened = false
                }
                ShadowButton {
                    text: qsTr("确定"); btnWidth: 80
                    accentColor: StyleTokens.accent
                    onClicked: namePopup.confirm()
                }
            }
        }

        function confirm() {
            var n = nameInput.text.trim()
            if (n === "") { toastManager.show(qsTr("名称不能为空"), "warning"); return }
            if (!mcFolder) return
            if (mode === "add") {
                mcFolder.addGameFolder(targetPath, n)
                toastManager.show(qsTr("已添加文件夹: %1").arg(n))
            } else {
                mcFolder.renameGameFolder(targetPath, n)
                toastManager.show(qsTr("已重命名: %1").arg(n))
            }
            opened = false
        }

        function openForAdd(path) {
            mode = "add"; targetPath = path
            nameInput.text = versionRightPanel.basenameOf(path)
            opened = true
            focusTimer.start()
        }
        function openForRename(path, current) {
            mode = "rename"; targetPath = path
            nameInput.text = current
            opened = true
            focusTimer.start()
        }
    }

    // 命名弹窗输入框焦点（Timer 非 Item，不能放进 GenericPopup 的 default content）
    Timer { id: focusTimer; interval: 60; onTriggered: if (namePopup.opened) nameInput.forceActiveFocus() }

    // ════════════════════════════════════════════════════════════
    // 删除条目确认
    // ════════════════════════════════════════════════════════════
    ConfirmDialog {
        id: removeFolderConfirm
    }
}
