// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

// ═══════════════════════════════════════════════════════════════════
// ModpackDetailPage — 整合包详情页（Modrinth + CurseForge 双源）
//
// 结构对齐 ModDetailPage：topBar + DetailInfoCard + 版本分组列表。
// 版本数据复用 modVersionsPartial 信号（fetchModpackVersions 自动路由
// Modrinth slug / CF 数字 id），版本行复用 DetailVersionCard。
// 下载流程与其余 Tab 不同：不选保存路径，弹窗输入「版本名称」→
// 添加下载任务（卡片标题「整合包：输入名（实际名）」）→ 下载完成后
// 自动转入导入整合包流程（backend 侧联动）。
// ═══════════════════════════════════════════════════════════════════
Rectangle {
    id: root
    readonly property bool hasBg: backend && typeof backend.customBgPath === "string" && backend.customBgPath.length > 0
    anchors.fill: parent
    color: hasBg ? "transparent" : StyleTokens.bgPrimary

    property var backend: null
    property var mainWindow: null
    property var toastManager: null

    // ── 详情数据（DownloadPage 传入）──
    property string modpackDetailSlug: ""
    property string modpackDetailTitle: ""
    property string modpackDetailDesc: ""
    property string modpackDetailIcon: ""
    property string modpackDetailSource: ""
    property int modpackDetailDownloads: 0
    property string modpackDetailUpdated: ""

    // ── 版本数据（modVersionsPartial 回传）──
    property var modpackRawVersions: []
    property var modpackVersionMap: ({})
    property bool modpackLoading: false
    property var modpackGrouped: []
    property var modpackExpandedGroups: []

    // ── 下载弹窗状态 ──
    property var _pendingVersion: null
    property bool _showNameDialog: false

    signal goBack()

    // ── 加载版本 ──
    onModpackDetailSlugChanged: {
        if (modpackDetailSlug && backend) {
            modpackLoading = true
            modpackRawVersions = []
            modpackVersionMap = {}
            modpackGrouped = []
            modpackExpandedGroups = []
            backend.fetchModpackVersions(modpackDetailSlug)
        }
    }

    // ── 版本数据解析（与 ModDetailPage 同构）──
    Connections {
        target: backend
        enabled: backend !== null
        function onModVersionsPartial(slug, versions, details) {
            if (slug !== root.modpackDetailSlug) return
            root.modpackLoading = false
            var arr = []
            var map = {}
            if (versions && versions.length > 0) {
                for (var vi = 0; vi < versions.length; vi++) {
                    var v = versions[vi]
                    var d = details ? details[v] : null
                    var gvs = []
                    if (d) {
                        if (d.game_versions) gvs = d.game_versions
                        else if (d.gameVersions) gvs = d.gameVersions
                    }
                    var gameVer = d ? (d.game_version || "") : ""
                    if (!gameVer) {
                        var pipeIdx = v.lastIndexOf("|")
                        gameVer = pipeIdx >= 0 ? v.substring(0, pipeIdx) : v
                    }
                    map[v] = {
                        versionNumber: d ? (d.version_number || v) : v,
                        gameVersion: gameVer,
                        gameVersions: gvs.length > 0 ? gvs : [gameVer],
                        loaders: d ? (d.loaders || []) : [],
                        date: d ? (d.date_published || "") : "",
                        downloads: d ? (d.downloads || 0) : 0,
                        url: d ? (d.url || d.download_url || "") : "",
                        filename: d ? (d.filename || "") : "",
                        size: d ? (d.size || 0) : 0,
                        sha1: d ? (d.sha1 || "") : ""
                    }
                    arr.push(v)
                }
            }
            root.modpackRawVersions = arr
            root.modpackVersionMap = map
            root.modpackGrouped = buildGroups(arr, map)
        }
    }

    function buildGroups(raw, map) {
        var groups = {}
        for (var i = 0; i < raw.length; i++) {
            var v = raw[i]
            var d = map[v]
            var gv = d ? (d.gameVersion || "") : ""
            if (!gv) continue
            if (!groups[gv]) groups[gv] = []
            groups[gv].push(v)
        }
        var result = []
        for (var k in groups) {
            groups[k].sort(function(a, b) {
                var da = map[a] ? map[a].date : ""
                var db = map[b] ? map[b].date : ""
                if (da > db) return -1; if (da < db) return 1; return 0
            })
            result.push({major: k, versions: groups[k]})
        }
        result.sort(function(a, b) {
            var as = a.major.split("."), bs = b.major.split(".")
            var am = parseInt(as[0]) || 0, bm = parseInt(bs[0]) || 0
            if (am !== bm) return bm - am
            return (parseInt(bs[1]) || 0) - (parseInt(as[1]) || 0)
        })
        return result
    }

    function isGroupExpanded(major) { return root.modpackExpandedGroups.indexOf(major) >= 0 }
    function toggleGroup(major) {
        var arr = root.modpackExpandedGroups.slice()
        var idx = arr.indexOf(major)
        if (idx >= 0) arr.splice(idx, 1); else arr.push(major)
        root.modpackExpandedGroups = arr
    }
    function fmtSize(bytes) {
        if (!bytes || bytes < 0) return ""
        if (bytes >= 1073741824) return (bytes / 1073741824).toFixed(2) + " GB"
        if (bytes >= 1048576) return (bytes / 1048576).toFixed(1) + " MB"
        if (bytes >= 1024) return (bytes / 1024).toFixed(0) + " KB"
        return bytes + " B"
    }
    function fmtDate(iso) { return iso ? iso.slice(0, 10) : "-" }
    function fmtDL(n) {
        if (n >= 100000000) return (n / 100000000).toFixed(1) + "亿"
        if (n >= 10000) return (n / 10000).toFixed(0) + "万"
        return String(n || 0)
    }

    // ── 下载：弹版本名输入框 → 添加下载任务（自动导入由后端联动）──
    function requestDownload(verStr) {
        var d = root.modpackVersionMap[verStr] || {}
        if (!d.url) {
            if (toastManager) toastManager.show("该版本无可用下载地址")
            return
        }
        root._pendingVersion = d
        root._showNameDialog = true
    }
    function confirmDownload(name) {
        var d = root._pendingVersion || {}
        if (!name || !name.trim()) {
            if (toastManager) toastManager.show("版本名称不能为空")
            return
        }
        var displayName = root.modpackDetailTitle || root.modpackDetailSlug
        var dlId = backend.downloadModpack(d.url, d.filename, d.size || 0, d.sha1 || "",
                                           name.trim(), displayName)
        root._showNameDialog = false
        root._pendingVersion = null
        if (toastManager) {
            if (dlId >= 0) toastManager.show("已添加下载任务：整合包 " + name.trim())
            else toastManager.show("添加下载任务失败", "", 5000)
        }
        if (mainWindow && typeof mainWindow.showModDownloadProgress === "function")
            mainWindow.showModDownloadProgress()
    }

    // ═══════════════ TOP BAR ═══════════════
    Rectangle {
        id: topBar
        anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
        height: 44; color: StyleTokens.bgPrimary; z: 10

        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12; spacing: 10
            BackButton { onClicked: root.goBack() }
            Item { Layout.fillWidth: true }
            Text {
                text: root.modpackDetailTitle || root.modpackDetailSlug || ""
                font.pixelSize: StyleTokens.fontSizeLg; font.weight: Font.Bold; color: StyleTokens.textSecondary
                Layout.fillWidth: true
                elide: Text.ElideRight; horizontalAlignment: Text.AlignHCenter
            }
            Item { Layout.fillWidth: true }
            Item { width: 40 } // spacer for symmetry
        }
    }

    // ═══════════════ CONTENT ═══════════════
    Flickable {
        id: contentFlick
        anchors.top: topBar.bottom; anchors.left: parent.left; anchors.right: parent.right
        anchors.bottom: parent.bottom
        contentWidth: width; contentHeight: contentCol.implicitHeight + 32
        clip: true; flickableDirection: Flickable.VerticalFlick
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 6 }

        ColumnLayout {
            id: contentCol
            width: parent.width - 32; x: 16; spacing: 12

            // ── INFO CARD ──
            DetailInfoCard {
                id: infoCard
                cardIcon: root.modpackDetailIcon
                cardTitle: root.modpackDetailTitle
                cardDesc: root.modpackDetailDesc

                // 统计行：来源 / 下载量 / 更新日期 / 版本数
                RowLayout {
                    Layout.fillWidth: true; spacing: 14

                    Text {
                        text: "来源：" + (root.modpackDetailSource || "Modrinth")
                        color: root.modpackDetailSource === "CurseForge" ? "#F08A5D" : "#9088e0"
                        font.pixelSize: StyleTokens.fontSizeSm; font.bold: true
                    }
                    Text {
                        text: "下载 " + fmtDL(root.modpackDetailDownloads)
                        color: StyleTokens.textMuted; font.pixelSize: StyleTokens.fontSizeSm
                        visible: root.modpackDetailDownloads > 0
                    }
                    Text {
                        text: "更新 " + fmtDate(root.modpackDetailUpdated)
                        color: StyleTokens.textMuted; font.pixelSize: StyleTokens.fontSizeSm
                        visible: root.modpackDetailUpdated !== ""
                    }
                    Text {
                        text: "版本 " + root.modpackRawVersions.length
                        color: StyleTokens.textMuted; font.pixelSize: StyleTokens.fontSizeSm
                        visible: root.modpackRawVersions.length > 0
                    }
                    Item { Layout.fillWidth: true }
                }
            }

            // ── 版本列表 ──
            Text {
                text: "版本列表"
                color: StyleTokens.textSecondary
                font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.DemiBold
                Layout.topMargin: 4
            }

            // Loading
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: (root.modpackLoading || root.modpackGrouped.length === 0) ? 60 : 0
                visible: root.modpackLoading || root.modpackGrouped.length === 0
                Text {
                    anchors.centerIn: parent
                    text: root.modpackLoading ? "正在获取版本列表..." : "暂无版本信息"
                    color: StyleTokens.textMuted
                    font.pixelSize: StyleTokens.fontSizeSm
                }
            }

            Repeater {
                model: root.modpackGrouped
                delegate: ExpandableGroupCard {
                    Layout.fillWidth: true
                    title: "MC " + modelData.major
                    subtitle: modelData.versions.length + " 个版本"
                    expanded: root.isGroupExpanded(modelData.major) || root.modpackGrouped.length === 1
                    onToggled: root.toggleGroup(modelData.major)

                    Repeater {
                        model: modelData.versions
                        delegate: DetailVersionCard {
                            required property string modelData
                            width: parent.width
                            versionLabel: (root.modpackVersionMap[modelData] && root.modpackVersionMap[modelData].versionNumber) || modelData
                            tags: {
                                var d = root.modpackVersionMap[modelData] || {}
                                var t = []
                                var lds = d.loaders || []
                                for (var li = 0; li < lds.length; li++) {
                                    var l = String(lds[li])
                                    t.push({text: l.charAt(0).toUpperCase() + l.slice(1),
                                            color: "#b0b8c8", bg: "#1e2230"})
                                }
                                return t
                            }
                            infoLines: [
                                {label: "日期", value: root.fmtDate(root.modpackVersionMap[modelData] ? root.modpackVersionMap[modelData].date : "")},
                                {label: "大小", value: root.fmtSize(root.modpackVersionMap[modelData] ? root.modpackVersionMap[modelData].size : 0)}
                            ]
                            hasDownload: true
                            onDownloadClicked: root.requestDownload(modelData)
                        }
                    }
                }
            }

            // 底部提示
            Text {
                Layout.fillWidth: true
                Layout.topMargin: 8
                text: "下载完成后将自动开始导入整合包（解析、下载模组、安装 MC 与加载器），可随时在下载进度页查看"
                color: StyleTokens.textMuted
                font.pixelSize: StyleTokens.fontSizeXs
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
            Item { Layout.fillWidth: true; height: 40 }
        }
    }

    // ═══════════════ 版本名称输入弹窗 ═══════════════
    Rectangle {
        anchors.fill: parent
        color: "#80000000"
        radius: StyleTokens.radiusWindow
        clip: true
        visible: root._showNameDialog
        z: 50
        opacity: root._showNameDialog ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

        MouseArea { anchors.fill: parent; onClicked: root._showNameDialog = false }

        Rectangle {
            id: nameCard
            anchors.centerIn: parent
            width: 480
            height: 240
            radius: StyleTokens.radiusWindow
            color: StyleTokens.surfaceOverlay
            clip: true
            scale: root._showNameDialog ? 1.0 : 0.92
            opacity: root._showNameDialog ? 1 : 0
            Behavior on scale { NumberAnimation { duration: 300; easing.type: Easing.OutBack } }
            Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    color: StyleTokens.surfaceOverlay
                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left; anchors.right: parent.right
                        height: 1; color: StyleTokens.borderLight
                    }
                    Text {
                        anchors.left: parent.left; anchors.leftMargin: 20
                        anchors.verticalCenter: parent.verticalCenter
                        text: "下载整合包"
                        color: StyleTokens.textPrimary
                        font.pixelSize: StyleTokens.fontSizeLg
                        font.bold: true
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.margins: 24
                    spacing: 12

                    Text {
                        Layout.fillWidth: true
                        text: "输入版本名称（将以此名称注册版本）："
                        color: StyleTokens.textSecondary
                        font.pixelSize: StyleTokens.fontSizeMd
                    }

                    InputBox {
                        id: versionNameInput
                        Layout.fillWidth: true
                        placeholderText: root.modpackDetailTitle || "版本名称"
                        Component.onCompleted: forceActiveFocus()
                        Keys.onReturnPressed: root.confirmDownload(versionNameInput.text)
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "版本注册名为你输入的名称，整合包实际名称仅作标注展示"
                        color: StyleTokens.textMuted
                        font.pixelSize: StyleTokens.fontSizeXs
                        wrapMode: Text.WordWrap
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 52
                    color: StyleTokens.surfaceOverlay
                    Rectangle {
                        anchors.top: parent.top
                        width: parent.width; height: 1
                        color: StyleTokens.borderLight
                    }
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16; anchors.rightMargin: 16
                        spacing: 10
                        Item { Layout.fillWidth: true }
                        ShadowButton {
                            text: "取消"
                            accentColor: StyleTokens.bgElevated
                            textColor: StyleTokens.textSecondary
                            Layout.preferredWidth: 96; Layout.preferredHeight: 32
                            onClicked: root._showNameDialog = false
                        }
                        ShadowButton {
                            text: "开始下载"
                            accentColor: StyleTokens.accent
                            Layout.preferredWidth: 128; Layout.preferredHeight: 32
                            onClicked: root.confirmDownload(versionNameInput.text)
                        }
                    }
                }
            }
        }
    }
}
