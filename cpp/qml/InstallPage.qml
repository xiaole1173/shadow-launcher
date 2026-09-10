// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

// InstallPage
// MC version + mod loader installation page
// Cards: MC version, Forge, NeoForge, Fabric, Optifine, Fabric API

Rectangle {
    id: root
    readonly property bool hasBg: backend && typeof backend.customBgPath === "string" && backend.customBgPath.length > 0
    anchors.fill: parent
    color: hasBg ? "transparent" : StyleTokens.bgPrimary

    property var backend: null
    property string mcVersion: ""
    property var toastManager: null
    readonly property bool _devMode: Qt.application.arguments.indexOf("--dev") >= 0
    signal goBack()
    signal navigateToProgress()
    signal requestMinimize()
    signal requestClose()

    Component.onCompleted: {
        if (backend) {
            backend.logMessage("[install] InstallPage loaded, mcVersion=" + mcVersion)
            // 刷新已安装版本列表（节流：避免同步扫盘卡 UI）
            deferRefreshInstalled()
        }
    }

    onMcVersionChanged: {
        if (mcVersion && backend) {
            // 刷新已安装版本列表（节流）
            deferRefreshInstalled()
            triggerQueries()
        }
    }

    function triggerQueries() {
        console.log("[install] triggerQueries for mcVersion=" + mcVersion)
        if (!backend || !mcVersion) return
        backend.logMessage("[install] querying mod loaders for " + mcVersion)
        forgeVersions = []; fabricVersions = []; neoforgeVersions = []; optifineVersions = []
        forgeLoading = true; fabricLoading = true; neoforgeLoading = true; optifineLoading = true
        selectedForge = ""; selectedNeoForge = ""; selectedFabric = ""; selectedOptifine = ""
        selectedOptifineType = ""; selectedOptifinePatch = ""
        activeLoader = ""; customName = ""
        console.log("[install] selections reset: selectedForge='" + selectedForge + "' selectedFabric='" + selectedFabric + "'")
        // Fire all loader queries immediately
        backend.queryForgeVersions(mcVersion)
        backend.queryFabricVersions(mcVersion)
        backend.queryNeoForgeVersions(mcVersion)
        backend.queryOptifineVersions(mcVersion)
        backend.queryFabricApiVersions(mcVersion)
    }

    property bool forgeLoading: false
    property bool fabricLoading: false
    property bool neoforgeLoading: false
    property var fabricApiVersions: []
    property bool fabricApiLoading: false

    property bool optifineLoading: false

    Connections {
        target: backend
        function onForgeVersionsReady(list) {
            forgeLoading = false; forgeVersions = annotateTypes(list)
        }
        function onFabricVersionsReady(list) {
            fabricLoading = false; fabricVersions = annotateTypes(list)
        }
        function onNeoforgeVersionsReady(list) {
            neoforgeLoading = false; neoforgeVersions = annotateTypes(list)
        }
        function onOptifineVersionsReady(list) {
            optifineLoading = false; optifineVersions = annotateTypes(list)
        }
        function onFabricApiVersionsReady(list) {
            fabricApiLoading = false; fabricApiVersions = list
        }
        function onInstalledVersionsChanged() {
            var n = customName !== "" ? customName : fullVersionName
            checkVersionConflict(n)
        }
    }

    // ── 通用版本比较：支持纯数字（47.0.14）与字母+数字混合（HD_U_G8 / HD_U_G10）──
    function versionTokens(v) {
        var s = String(v)
        var toks = s.match(/[A-Za-z]+|\d+/g)
        return toks || []
    }
    // 升序语义：a < b → 负数。数字段按数值，字母段按字典序。
    function compareVersionTokens(a, b) {
        var ta = versionTokens(a), tb = versionTokens(b)
        var n = Math.max(ta.length, tb.length)
        for (var i = 0; i < n; i++) {
            var x = i < ta.length ? ta[i] : null
            var y = i < tb.length ? tb[i] : null
            if (x === null && y === null) return 0
            if (x === null) return -1   // a 更短 → 更小（如 HD_U_G8 vs HD_U_G8_pre1）
            if (y === null) return 1
            var xn = /^\d+$/.test(x), yn = /^\d+$/.test(y)
            if (xn && yn) {
                var diff = parseInt(x, 10) - parseInt(y, 10)
                if (diff !== 0) return diff
                if (x !== y) return x < y ? -1 : 1  // 前导零兜底
            } else if (xn) {
                return 1   // 数字段 > 字母段
            } else if (yn) {
                return -1
            } else {
                if (x !== y) return x < y ? -1 : 1
            }
        }
        return 0
    }

    function annotateTypes(inputList) {
        var result = []
        for (var i = 0; i < inputList.length; i++) {
            var item = inputList[i]
            if (!item.version) continue
            var m = { version: item.version, date: item.date || "" }
            var v = item.version.toLowerCase()
            if (v.indexOf("snapshot") >= 0) m.type = "snapshot"
            else if (v.indexOf("preview") >= 0 || v.indexOf("_pre") >= 0) m.type = "preview"
            else if (v.indexOf("alpha") >= 0) m.type = "alpha"
            else if (v.indexOf("beta") >= 0 || v.indexOf("-pre") >= 0 || v.indexOf("rc") >= 0) m.type = "beta"
            else m.type = (item.type === "recommended") ? "release" : (item.type || "release")  // recommended 并入 release
            result.push(m)
        }
        var typeOrder = { release: 0, beta: 1, snapshot: 2, preview: 3, alpha: 4 }
        result.sort(function(a, b) {
            if (typeOrder[a.type] !== typeOrder[b.type]) return typeOrder[a.type] - typeOrder[b.type]
            // 降序：最新在前
            return compareVersionTokens(b.version, a.version)
        })
        var foundLatest = false
        for (var j = 0; j < result.length; j++) {
            if (!foundLatest && result[j].type === "release") {
                result[j].isLatestRelease = true
                foundLatest = true
            }
        }
        return result
    }

    property string selectedForge: ""
    property string selectedNeoForge: ""
    property string selectedFabric: ""
    property string selectedOptifine: ""
    property string selectedOptifineType: ""
    property string selectedOptifinePatch: ""
    property string selectedFabricApi: ""
    property string selectedFabricApiUrl: ""
    property string selectedFabricApiFile: ""

    // User data import
    property string importArchivePath: ""
    property string importArchiveName: ""
    property var importItems: []

    function cancelImport() {
        importArchivePath = ""
        importArchiveName = ""
        importItems = []
        if (backend && backend.userDataBackend) backend.userDataBackend.cancelPendingImport()
        if (backend) backend.cancelPendingUserDataImport(fullVersionName)
    }

    function formatSize(bytes) {
        if (!bytes || bytes < 0) return "0 B"
        var units = ["B", "KB", "MB", "GB"]
        var u = 0, v = bytes
        while (v >= 1024 && u < units.length - 1) { v /= 1024; u++ }
        return v.toFixed(u === 0 ? 0 : 1) + " " + units[u]
    }

    property string activeLoader: ""

    // ── OptiFine-Forge compatibility ──
    function getOptifineForge(optiVer) {
        var found = optifineVersions.find(function(v) { return v.version === optiVer })
        if (!found) return ""
        var f = found.forge || ""
        // "Forge 61.0.8" → "61.0.8"; "Forge N/A" → "N/A"
        if (f === "Forge N/A" || f === "N/A" || f === "") return "N/A"
        var m = f.match(/Forge\s+(.+)/)
        return m ? m[1] : f
    }

    // The Forge version required by the selected OptiFine
    readonly property string optifineForgeCompat: {
        if (!selectedOptifine) return ""
        var fv = getOptifineForge(selectedOptifine)
        return (fv === "N/A" || fv === "") ? "N/A" : fv
    }

    // ── Counts ──
    // How many OptiFine versions are compatible with the selected Forge
    readonly property int forgeCompatibleOptifineCount: {
        if (!selectedForge) return -1  // -1 = no Forge selected
        var cnt = 0
        for (var i = 0; i < optifineVersions.length; i++) {
            var fv = getOptifineForge(optifineVersions[i].version)
            if (fv === selectedForge) cnt++
        }
        return cnt
    }

    // How many Forge versions are compatible with the selected OptiFine
    readonly property int optifineCompatibleForgeCount: {
        if (!selectedOptifine) return -1
        var target = optifineForgeCompat
        if (target === "N/A" || target === "") return 0
        var cnt = 0
        for (var i = 0; i < forgeVersions.length; i++) {
            if (forgeVersions[i].version === target) cnt++
        }
        return cnt
    }

    // ── Sorted version lists (compatible first) ──
    readonly property var optifineVersionsSorted: {
        if (optifineVersions.length === 0) return []
        if (!selectedForge) return optifineVersions
        var compat = [], rest = []
        for (var i = 0; i < optifineVersions.length; i++) {
            var v = optifineVersions[i]
            var fv = getOptifineForge(v.version)
            if (fv === selectedForge) compat.push(v)
            else rest.push(v)
        }
        return compat.concat(rest)
    }

    readonly property var forgeVersionsSorted: {
        if (forgeVersions.length === 0) return []
        if (!selectedOptifine) return forgeVersions
        var target = optifineForgeCompat
        if (target === "N/A" || target === "") return forgeVersions
        var compat = [], rest = []
        for (var i = 0; i < forgeVersions.length; i++) {
            var v = forgeVersions[i]
            if (v.version === target) compat.push(v)
            else rest.push(v)
        }
        return compat.concat(rest)
    }

    // ── Per-item enable/disable ──
    function forgeItemEnabled(forgeVer) {
        if (!selectedOptifine) return true
        var target = optifineForgeCompat
        if (target === "N/A" || target === "") return false
        return forgeVer === target
    }

    function optifineItemEnabled(optiVer) {
        if (!selectedForge) return true
        var fv = getOptifineForge(optiVer)
        if (fv === "N/A" || fv === "") return false
        return fv === selectedForge
    }

    // ── Auto-clear incompatible selections ──
    onSelectedOptifineChanged: {
        if (optifineForgeCompat === "N/A") {
            if (activeLoader === "forge") activeLoader = ""
            selectedForge = ""
        } else if (optifineForgeCompat && selectedForge !== optifineForgeCompat) {
            selectedForge = ""
        }
    }

    onSelectedForgeChanged: {
        if (!selectedForge) return
        if (forgeCompatibleOptifineCount <= 0) {
            if (activeLoader === "optifine") activeLoader = ""
            selectedOptifine = ""
        } else if (selectedOptifine) {
            var fv = getOptifineForge(selectedOptifine)
            if (fv !== selectedForge) {
                if (activeLoader === "optifine") activeLoader = ""
                selectedOptifine = ""
            }
        }
    }

    property var forgeVersions: []
    property var neoforgeVersions: []
    property var fabricVersions: []
    property var optifineVersions: []

    property bool hasModLoader: activeLoader !== ""

    property string versionSuffix: {
        var parts = []
        if (activeLoader === "forge" && selectedForge) parts.push("forge-" + selectedForge)
        if (activeLoader === "neoforge" && selectedNeoForge) parts.push("neoforge-" + selectedNeoForge)
        if (activeLoader === "fabric" && selectedFabric) parts.push("fabric-" + selectedFabric)
        if (activeLoader === "fabric" && selectedFabricApi) parts.push("fabric-api-" + selectedFabricApi)
        if (activeLoader === "optifine" && selectedOptifine) parts.push("OptiFine_" + selectedOptifine)
        return parts.join("-")
    }

    property string fullVersionName: mcVersion + (versionSuffix ? "-" + versionSuffix : "")
    property string customName: ""
    property bool versionConflict: false
    property string conflictMessage: ""

    function checkVersionConflict(name) {
        var prev = versionConflict
        if (!backend || !name) {
            versionConflict = false; conflictMessage = ""; return
        }
        var names = backend.activeVersionNames || backend.installedVersions
        if (!names) {
            versionConflict = false; conflictMessage = ""; return
        }
        for (var i = 0; i < names.length; i++) {
            if (names[i] === name) {
                versionConflict = true
                conflictMessage = "\u7248\u672c \"" + name + "\" \u5df2\u5b58\u5728\uff0c\u8bf7\u4fee\u6539\u540d\u79f0"
                console.log("[install] versionConflict=true, name=" + name + " matches activeVersionNames[" + i + "]")
                return
            }
        }
        versionConflict = false; conflictMessage = ""
        if (prev !== versionConflict) console.log("[install] versionConflict cleared, name=" + name)
    }

    // ── 节流的 refreshInstalled：同步扫盘（versions 目录），高频调用（改名/切版本/点击）
    //    会卡主线程 UI——300ms 合并为一次，最后触发
    Timer {
        id: installRefreshTimer
        interval: 300
        repeat: false
        onTriggered: { if (backend) backend.refreshInstalled() }
    }
    function deferRefreshInstalled() {
        installRefreshTimer.restart()
    }

    onFullVersionNameChanged: {
        if (backend) {
            // 加载器/版本改变时刷新 versions 文件夹（节流）
            deferRefreshInstalled()
        }
    }

    onCustomNameChanged: {
        if (backend) {
            deferRefreshInstalled()
        }
    }

    // TOP BAR
    Rectangle {
        id: topBar
        anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
        height: 44; color: hasBg ? "transparent" : StyleTokens.bgPrimary; z: 10

        // Drag handler (behind RowLayout, only fires on empty areas)
        MouseArea {
            anchors.fill: parent; z: -1
            property point lastPos: Qt.point(0, 0)
            onPressed: function(mouse) { lastPos = Qt.point(mouse.x, mouse.y) }
            onPositionChanged: function(mouse) {
                var win = root.Window.window
                if (win) {
                    win.x += mouse.x - lastPos.x
                    win.y += mouse.y - lastPos.y
                }
            }
        }

        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12; spacing: 10
            BackButton {
                onClicked: root.goBack()
            }
            Item { Layout.fillWidth: true }
            Text { text: mcVersion || "???"; font.pixelSize: StyleTokens.fontSizeLg; font.weight: Font.Bold; color: StyleTokens.textSecondary }
            Item { Layout.fillWidth: true }
            ShadowIconButton { icon: "\u2014"; type: "normal"; onClicked: { root.requestMinimize() } }
            Item { width: 6 }
            ShadowIconButton { icon: "\u2715"; type: "close"; onClicked: { root.requestClose() } }
        }
    }

    // CONTENT AREA
    Flickable {
        id: contentFlick
        anchors.top: topBar.bottom; anchors.left: parent.left; anchors.right: parent.right
        anchors.bottom: parent.bottom
        contentWidth: width; contentHeight: contentCol.implicitHeight + 32
        clip: true; flickableDirection: Flickable.VerticalFlick; boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 6 }

        ColumnLayout {
            id: contentCol
            width: parent.width - 32; x: 16; spacing: 12

            // CARD 1 - MC Version
            Rectangle {
                id: mcCard
                Layout.fillWidth: true
                implicitHeight: root.versionConflict ? 116 : 96
                radius: StyleTokens.radiusLg; color: mcCardHovered ? StyleTokens.accentSubtle : StyleTokens.bgSecondary
                border.color: root.versionConflict ? StyleTokens.statusOff : (mcCardHovered ? StyleTokens.accentHover : StyleTokens.border)
                border.width: mcCardHovered ? 1.5 : 1
                clip: true
                scale: mcCardHovered ? 1.005 : 1.0

                property bool mcCardHovered: false

                Behavior on color { ColorAnimation { duration: 200 } }
                Behavior on border.color { ColorAnimation { duration: 250 } }
                Behavior on implicitHeight { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                Behavior on scale { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }

                HoverHandler { id: mcHover; onHoveredChanged: mcCard.mcCardHovered = hovered }

                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 12; spacing: 8
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: "Minecraft " + mcVersion; font.pixelSize: StyleTokens.fontSizeXl; font.weight: Font.Bold; color: StyleTokens.textSecondary; verticalAlignment: Text.AlignVCenter }
                    }
                    Rectangle {
                        Layout.fillWidth: true; height: 32; radius: StyleTokens.radiusSm
                        color: StyleTokens.surfaceOverlay
                        border.color: root.versionConflict ? StyleTokens.textDanger : (nameInput.activeFocus ? StyleTokens.accentHover : StyleTokens.borderLight)
                        border.width: 1
                        Behavior on border.color { ColorAnimation { duration: 200 } }
                        TextInput {
                            id: nameInput; anchors.fill: parent; anchors.margins: 8
                            text: root.customName !== "" ? root.customName : root.fullVersionName
                            font.pixelSize: StyleTokens.fontSizeMd; color: StyleTokens.textSecondary
                            selectByMouse: true; clip: true; verticalAlignment: TextInput.AlignVCenter
                            onTextChanged: {
                                // 每次输入立即更新 customName 并重新检测冲突
                                var t = text.trim()
                                if (t !== root.fullVersionName) {
                                    root.customName = t
                                } else {
                                    root.customName = ""
                                }
                                root.checkVersionConflict(t !== "" ? t : root.fullVersionName)
                            }
                        }
                    }
                    // Conflict warning
                    Text {
                        visible: root.versionConflict
                        text: root.conflictMessage
                        font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.errorLight
                        Layout.fillWidth: true; elide: Text.ElideRight
                    }
                }
            }

            // SECTION LABEL: Mod Loader
            Text {
                text: "Mod \u52a0\u8f7d\u5668"
                font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.DemiBold; color: StyleTokens.textTertiary
                Layout.topMargin: 8; Layout.leftMargin: 4
            }

            // Forge
            ModLoaderCard {
                Layout.fillWidth: true; title: "Forge"; loaderKey: "forge"
                versions: root.forgeLoading ? [{version: "加载中...", type: "", date: ""}] :
                          root.forgeVersionsSorted
                versionEnabled: root.forgeItemEnabled
                badgeText: (root.selectedOptifine && root.optifineCompatibleForgeCount > 0) ? "存在兼容的Forge" : ""
                disabled: root.hasModLoader && root.activeLoader !== "forge"
                    || (root.selectedOptifine && root.optifineCompatibleForgeCount <= 0)
                disabledReason: root.activeLoader === "neoforge" ? "NeoForge 已选中"
                    : root.activeLoader === "fabric" ? "Fabric 已选中"
                    : (root.selectedOptifine && root.optifineCompatibleForgeCount <= 0) ? "无兼容的Forge可用"
                    : ""
                selectedVersion: root.selectedForge
                onVersionSelected: function(ver, sha1) { root.selectedForge = ver; root.activeLoader = "forge"; root.customName = ""; if (backend) { backend.logMessage("[install] Forge: " + ver); if (sha1) backend.cacheForgeInstallerSha1(mcVersion, ver, sha1) } }
                onVersionCleared: { root.selectedForge = ""; root.activeLoader = ""; root.customName = "" }
            }

            // NeoForge
            ModLoaderCard {
                Layout.fillWidth: true; title: "NeoForge"; loaderKey: "neoforge"
                versions: root.neoforgeLoading ? [{version: "加载中...", type: "", date: ""}] : root.neoforgeVersions
                disabled: root.hasModLoader && root.activeLoader !== "neoforge"
                disabledReason: root.activeLoader === "forge" ? "Forge \u5df2\u9009\u4e2d" : root.activeLoader === "fabric" ? "Fabric \u5df2\u9009\u4e2d" : root.selectedOptifine ? "\u4e0d\u517c\u5bb9 NeoForge" : ""
                selectedVersion: root.selectedNeoForge
                onVersionSelected: function(ver) { root.selectedNeoForge = ver; root.activeLoader = "neoforge"; root.customName = "" }
                onVersionCleared: { root.selectedNeoForge = ""; root.activeLoader = ""; root.customName = "" }
            }

            // Fabric
            ModLoaderCard {
                Layout.fillWidth: true; title: "Fabric"; loaderKey: "fabric"
                versions: root.fabricLoading ? [{version: "加载中...", type: "", date: ""}] : root.fabricVersions
                disabled: root.hasModLoader && root.activeLoader !== "fabric"
                disabledReason: root.activeLoader === "forge" ? "Forge \u5df2\u9009\u4e2d" : root.activeLoader === "neoforge" ? "NeoForge \u5df2\u9009\u4e2d" : root.selectedOptifine ? "\u4e0d\u517c\u5bb9 Fabric" : ""
                selectedVersion: root.selectedFabric
                onVersionSelected: function(ver) { root.selectedFabric = ver; root.activeLoader = "fabric"; root.customName = "" }
                onVersionCleared: { root.selectedFabric = ""; root.activeLoader = ""; root.customName = "" }
            }

            // Fabric API (unified card, visible only when Fabric is selected)
            ModLoaderCard {
                Layout.fillWidth: true; title: "Fabric API"; loaderKey: "fabric_api"
                visible: root.activeLoader === "fabric"
                badgeText: root.activeLoader === "fabric" && !root.selectedFabricApi ? "\u5efa\u8bae\u5b89\u88c5" : ""
                disabled: false
                versions: root.fabricApiLoading ? [{version: "加载中...", type: "", date: ""}] : root.fabricApiVersions
                selectedVersion: root.selectedFabricApi
                onVersionSelected: function(ver) {
                    root.selectedFabricApi = ver
                    for (var i = 0; i < root.fabricApiVersions.length; i++) {
                        if (root.fabricApiVersions[i].version === ver) {
                            root.selectedFabricApiUrl = root.fabricApiVersions[i].url
                            root.selectedFabricApiFile = root.fabricApiVersions[i].filename
                            break
                        }
                    }
                }
                onVersionCleared: {
                    root.selectedFabricApi = ""
                    root.selectedFabricApiUrl = ""
                    root.selectedFabricApiFile = ""
                }
            }

            // SECTION LABEL: Shader
            Text { text: "\u5149\u5f71\u52a0\u8f7d\uff08\u9ad8\u6e05\u4fee\u590d\uff09"; font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.DemiBold; color: StyleTokens.textTertiary; Layout.topMargin: 8; Layout.leftMargin: 4 }

            // Optifine
            ModLoaderCard {
                Layout.fillWidth: true; title: "Optifine"; loaderKey: "optifine"
                versions: root.optifineLoading ? [{version: "加载中...", type: "", date: ""}] : root.optifineVersionsSorted
                badgeText: (root.selectedForge && root.forgeCompatibleOptifineCount > 0) ? "存在兼容的Optifine" : ""
                versionEnabled: root.optifineItemEnabled
                disabled: root.activeLoader === "fabric" || root.activeLoader === "neoforge"
                    || (root.selectedForge && root.forgeCompatibleOptifineCount <= 0)
                disabledReason: root.activeLoader === "fabric" ? "Fabric \u4e0d\u517c\u5bb9 Optifine"
                    : root.activeLoader === "neoforge" ? "Optifine \u4e0d\u652f\u6301 NeoForge"
                    : (root.selectedForge && root.forgeCompatibleOptifineCount <= 0) ? "\u65e0\u517c\u5bb9\u7684Optifine\u53ef\u7528"
                    : ""
                selectedVersion: root.selectedOptifine
                onVersionSelected: function(ver) { root.selectedOptifine = ver; root.customName = ""; if (!root.hasModLoader) root.activeLoader = "optifine";
                    var found = root.optifineVersions.find(function(v) { return v.version === ver })
                    if (found) { root.selectedOptifineType = found.bmclType || ""; root.selectedOptifinePatch = found.bmclPatch || "" } }
                onVersionCleared: { root.selectedOptifine = ""; root.selectedOptifineType = ""; root.selectedOptifinePatch = ""; if (root.activeLoader === "optifine") root.activeLoader = "" }
            }

            // ── User data import section (dev mode only) ──
            ColumnLayout {
                Layout.fillWidth: true; spacing: 6
                Layout.topMargin: 8
                visible: root._devMode

                // Import button row
                RowLayout { spacing: 8; Layout.fillWidth: true
                    Rectangle {
                        id: importBtn
                        width: 180; height: 36; radius: StyleTokens.radiusMd
                        color: importBtnHover.containsMouse ? StyleTokens.accentSubtle : StyleTokens.bgSecondary
                        border.color: importBtnHover.containsMouse ? StyleTokens.accentViolet : StyleTokens.accentSubtle
                        border.width: 1
                        Behavior on color { ColorAnimation { duration: 200 } }
                        Behavior on border.color { ColorAnimation { duration: 200 } }
                        scale: importBtnHover.containsMouse ? 1.03 : 1.0
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

                        // Press bounce
                        property real _pressScale: 1.0
                        SequentialAnimation {
                            id: importPressAnim
                            PropertyAction { target: importBtn; property: "_pressScale"; value: 0.93 }
                            NumberAnimation { target: importBtn; property: "_pressScale"; to: 1.0; duration: 350; easing.type: Easing.OutBack; easing.overshoot: 2.0 }
                        }

                        transform: Scale { origin.x: importBtn.width / 2; origin.y: importBtn.height / 2; xScale: importBtn._pressScale; yScale: importBtn._pressScale }

                        Row {
                            anchors.centerIn: parent; spacing: 6
                            Image {
                                source: "icons/lucide/package.svg"
                                width: 18; height: 18
                                anchors.verticalCenter: parent.verticalCenter
                                fillMode: Image.PreserveAspectFit
                            }
                            Text {
                                id: importBtnText
                                text: qsTr("导入用户数据")
                                font.pixelSize: StyleTokens.fontSizeMd; color: StyleTokens.accentLink
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                        MouseArea {
                            id: importBtnHover
                            anchors.fill: parent; hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            enabled: !root.importArchivePath
                            onClicked: {
                                importPressAnim.start()
                                importFileDialog.open()
                            }
                        }
                    }

                    // Archive name + X
                    Rectangle {
                        visible: root.importArchivePath !== ""
                        width: Math.min(archiveNameText.implicitWidth, 200) + 36; height: 24; radius: StyleTokens.radiusSm
                        color: StyleTokens.accentSubtle; border.color: StyleTokens.bgHover
                        Text {
                            id: archiveNameText
                            anchors.centerIn: parent
                            text: root.importArchiveName || ""
                            font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary
                            elide: Text.ElideMiddle
                        }
                        HoverHandler { id: archiveHover }
                        Rectangle {
                            anchors.right: parent.right; anchors.rightMargin: 2
                            anchors.verticalCenter: parent.verticalCenter
                            width: 20; height: 20; radius: StyleTokens.radiusLg
                            color: removeImportHover.hovered ? StyleTokens.errorBg : "transparent"
                            opacity: archiveHover.hovered ? 1 : 0
                            Behavior on opacity { NumberAnimation { duration: 150 } }
                            Image {
                                anchors.centerIn: parent
                                source: "icons/lucide/x.svg"
                                width: 12; height: 12
                                fillMode: Image.PreserveAspectFit
                            }
                            HoverHandler { id: removeImportHover }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                onClicked: root.cancelImport()
                            }
                        }
                    }
                }

                // Import preview items
                Repeater {
                    model: root.importItems
                    RowLayout {
                        spacing: 6; Layout.fillWidth: true
                        Image {
                            source: modelData.isDir ? "icons/lucide/folder.svg" : "icons/lucide/file-text.svg"
                            width: 14; height: 14
                            fillMode: Image.PreserveAspectFit
                        }
                        Text {
                            text: modelData.name || ""
                            font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textSecondary
                        }
                        Text {
                            text: modelData.sizeBytes > 0 ? root.formatSize(modelData.sizeBytes) : ""
                            font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.textMuted
                        }
                        RowLayout {
                            visible: modelData.riskLevel >= 2
                            spacing: 3; Layout.fillWidth: true
                            Image {
                                source: "icons/lucide/alert-triangle-red.svg"
                                width: 12; height: 12
                                fillMode: Image.PreserveAspectFit
                            }
                            Text {
                                text: modelData.warning || ""
                                font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.errorLight
                                wrapMode: Text.WordWrap; elide: Text.ElideRight
                                maximumLineCount: 1; Layout.fillWidth: true
                            }
                        }
                        RowLayout {
                            visible: modelData.riskLevel === 1
                            spacing: 3; Layout.fillWidth: true
                            Image {
                                source: "icons/lucide/alert-triangle.svg"
                                width: 12; height: 12
                                fillMode: Image.PreserveAspectFit
                            }
                            Text {
                                text: modelData.warning || ""
                                font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.warning
                                wrapMode: Text.WordWrap; elide: Text.ElideRight
                                maximumLineCount: 1; Layout.fillWidth: true
                            }
                        }
                    }
                }
            }

            Item { Layout.fillWidth: true; height: 80 }  // extra space for floating button
        }
    }

    // ── Floating install button (elastic bounce from bottom) ──
    Rectangle {
        id: installBtn
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        width: 280; height: 48; radius: StyleTokens.radiusXl

        // Physical shadow (offset layer)
        Rectangle {
            anchors.fill: parent; anchors.topMargin: 4; anchors.leftMargin: 2; radius: StyleTokens.radiusXl
            color: StyleTokens.scrim; opacity: 0.3; z: -1
        }

        color: root.versionConflict ? StyleTokens.accentSubtle
                : (installHover.containsMouse ? StyleTokens.accent : StyleTokens.accentHover)
        opacity: root.hasModLoader || mcVersion !== "" ? 1.0 : 0.0

        // Visibility scale (appear/disappear) — separate from press animation
        property real _baseScale: (root.hasModLoader || mcVersion !== "") ? 1.0 : 0.8
        // Press bounce (animated, reset to 1.0 after bounce)
        property real _pressScale: 1.0
        scale: _baseScale * _pressScale

        // Bounce from bottom when visible
        anchors.bottomMargin: (root.hasModLoader || mcVersion !== "") ? 24 : -60

        Behavior on anchors.bottomMargin {
            NumberAnimation { duration: 500; easing.type: Easing.OutBack; easing.overshoot: 2.0 }
        }
        Behavior on opacity {
            NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
        }
        Behavior on _baseScale {
            NumberAnimation { duration: 400; easing.type: Easing.OutBack; easing.overshoot: 1.5 }
        }
        Behavior on color { ColorAnimation { duration: 200 } }

        // Elastic press feedback on click
        SequentialAnimation {
            id: installPressBounce
            PropertyAction { target: installBtn; property: "_pressScale"; value: 0.92 }
            NumberAnimation { target: installBtn; property: "_pressScale"; to: 1.0; duration: 450; easing.type: Easing.OutBack; easing.overshoot: 2.5 }
        }

        Row {
            anchors.centerIn: parent; spacing: 8
            Image {
                source: "icons/lucide/download.svg"
                width: 20; height: 20
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                id: installLabel
                anchors.verticalCenter: parent.verticalCenter
                text: root.versionConflict ? qsTr("名称冲突") : qsTr("开始下载")
                font.pixelSize: StyleTokens.fontSizeLg; font.weight: Font.Bold
                color: root.versionConflict ? StyleTokens.textTertiary : StyleTokens.textInverse
            }
        }

        MouseArea {
            id: installHover
            anchors.fill: parent; hoverEnabled: true
            cursorShape: root.versionConflict ? Qt.ArrowCursor : Qt.PointingHandCursor
            enabled: !root.versionConflict
            onClicked: {
                // 点击前刷新版本列表并检查命名冲突（节流，避免同步扫盘卡 UI）
                deferRefreshInstalled()
                var n = root.customName !== "" ? root.customName : root.fullVersionName
                root.checkVersionConflict(n)
                if (root.versionConflict) {
                    console.log("[install] download blocked: versionConflict=true, name=" + n)
                    if (root.toastManager) {
                        root.toastManager.show("版本名 \"" + n + "\" 已存在，请修改后重试", "warning")
                    }
                    return
                }

                installPressBounce.start()
                console.log("[install] floating button clicked: name=" + n)

                // ═══ Navigate to progress page FIRST, defer download ═══
                // This avoids UI freeze caused by synchronous I/O in installVersion/installModLoader.
                root.navigateToProgress()

                if (backend) {
                    // Defer the actual download start so the progress page renders immediately
                    Qt.callLater(function() {
                        backend.logMessage("[install] download: " + n)
                        backend.cancelModLoaderQueries()
                        var hasLoaderInstalled = false
                        var dlName = n  // snapshot the vars for the closure
                        var dlMcVersion = mcVersion
                        var dlForge = selectedForge
                        var dlNeoForge = selectedNeoForge
                        var dlFabric = selectedFabric
                        var dlFabricApi = selectedFabricApi
                        var dlFabricApiUrl = selectedFabricApiUrl
                        var dlFabricApiFile = selectedFabricApiFile
                        var dlOptifine = selectedOptifine
                        var dlOptifineType = selectedOptifineType
                        var dlOptifinePatch = selectedOptifinePatch
                        if (dlForge !== "") {
                            backend.installModLoader(dlMcVersion, "forge", dlForge, dlName)
                            hasLoaderInstalled = true
                        }
                        if (dlNeoForge !== "") {
                            backend.installModLoader(dlMcVersion, "neoforge", dlNeoForge, dlName)
                            hasLoaderInstalled = true
                        }
                        if (dlFabric !== "") {
                            var gameDir = backend.getVersionGameDir(dlName) || ""
                            if (dlFabricApi !== "" && dlFabricApiUrl !== "") {
                                var apiPath = gameDir + "/mods/" + dlFabricApiFile
                                backend.installModLoader(dlMcVersion, "fabric", dlFabric, dlName,
                                    dlFabricApi, dlFabricApiUrl, apiPath)
                            } else {
                                backend.installModLoader(dlMcVersion, "fabric", dlFabric, dlName)
                            }
                            hasLoaderInstalled = true
                        }
                        if (dlOptifine !== "") {
                            if (hasLoaderInstalled) {
                                backend.installOptifineJar(dlMcVersion, dlOptifine, dlOptifineType, dlOptifinePatch)
                            } else {
                                backend.installOptifine(dlMcVersion, dlOptifine, "", dlName, dlOptifineType, dlOptifinePatch)
                            }
                        }
                        if (!hasLoaderInstalled && dlOptifine === "") {
                            backend.installVersion(dlName)
                        }
                        // Set pending user data import
                        if (root.importArchivePath !== "") {
                            backend.setPendingUserDataImport(dlName, root.importArchivePath)
                        }
                    })
                }
            }
        }
    }

    // Import FileDialog
    FileDialog {
        id: importFileDialog
        title: qsTr("导入用户数据")
        fileMode: FileDialog.OpenFile
        nameFilters: ["ZIP 文件 (*.zip)"]
        onAccepted: {
            var sel = importFileDialog.selectedFile
            // selectedFile 可能是 QUrl 或带 file:/// 前缀的字符串——统一防御式转本地路径
            var path = ""
            if (typeof sel === "string") {
                path = sel
            } else if (sel && typeof sel.toString === "function") {
                path = sel.toString()
            }
            if (path.indexOf("file:///") === 0) path = path.substring(8)
            if (backend && backend.userDataBackend) {
                backend.userDataBackend.validateArchive(backend.gameDir, path)
            }
        }
    }

    Connections {
        target: backend ? backend.userDataBackend : null
        enabled: backend && backend.userDataBackend
        function onValidateFinished(success, archivePath, items, error) {
            if (success) {
                root.importArchivePath = archivePath
                root.importArchiveName = archivePath.split("/").pop().split("\\").pop()
                root.importItems = items
                if (backend && backend.userDataBackend) {
                    backend.userDataBackend.setPendingImport(archivePath, items)
                }
            } else {
                if (root.toastManager) {
                    root.toastManager.show(error || "导入失败", "error")
                }
            }
        }
    }
}
