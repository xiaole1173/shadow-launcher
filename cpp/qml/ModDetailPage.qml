// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

// ModDetailPage
// Full-screen detail page for a Mod project's version list
// Architecture: InstallPage-style — fixed top bar + Flickable + cards

Rectangle {
    id: root
    readonly property bool hasBg: backend && typeof backend.customBgPath === "string" && backend.customBgPath.length > 0
    anchors.fill: parent
    color: hasBg ? "transparent" : StyleTokens.bgPrimary

    // ── 入场动画：从右侧滑入 ──
    x: 60
    opacity: 0
    Component.onCompleted: { x = 0; opacity = 1 }
    Behavior on x { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } }
    Behavior on opacity { NumberAnimation { duration: 350; easing.type: Easing.OutCubic } }

    // ── Injected properties (set by Loader onLoaded) ──
    property var backend: null
    property var toastManager: null
    property var mainWindow: null

    property string modDetailSlug: ""
    // CF 详情（slug 为纯数字 modId）：下载量数据不可靠 → 卡片不显示（2026-08-15）
    property bool _isCfDetail: /^\d+$/.test(modDetailSlug)
    property string modDetailTitle: ""
    property string modDetailZh: ""    // 中文名（列表卡片传入；空则回退英文 title）
    // 详情页显示标题：中文（英文）组合，与列表卡片一致
    readonly property string displayTitle: {
        if (modDetailZh !== "" && modDetailTitle !== "" && modDetailZh !== modDetailTitle)
            return modDetailZh + "（" + modDetailTitle + "）"
        return modDetailZh !== "" ? modDetailZh : (modDetailTitle || modDetailSlug || "")
    }
    property string modDetailDesc: ""
    property string modDetailIcon: ""
    property string modDetailIconRaw: ""

    // ── 详情页跳转/复制按钮（统一胶囊，详情页与列表卡一致映射）──
    property var _linkItems: []
    property var _copyItems: []
    function refreshLinks() {
        if (!backend || !modDetailSlug) { _linkItems = []; _copyItems = []; return }
        var r = backend.resolveProjectLinks(modDetailTitle, modDetailSlug, "mod")
        var links = []
        if (r.mrUrl) links.push({ label: "Modrinth", url: r.mrUrl })
        if (r.cfUrl) links.push({ label: "CurseForge", url: r.cfUrl })
        if (r.mcmodUrl) links.push({ label: "MC 百科", url: r.mcmodUrl })
        var name = modDetailTitle || modDetailSlug
        var zh = modDetailZh !== "" ? modDetailZh : backend.resolveModZh(modDetailTitle)
        if (zh && zh !== name) name = zh + " " + name
        _linkItems = links
        _copyItems = [
            { label: "复制名称", text: name }
        ]
    }
    onModDetailTitleChanged: if (backend) refreshLinks()

    property bool modDetailLoading: false
    property var modDetailRawVersions: []
    property var modDetailVersionMap: ({})
    property var pendingModDownload: ({})

    // ── 前置模组 ──
    property var modDetailDependencies: []
    property bool modDetailDepsLoading: false
    property bool showDeps: false
    property var modNavStack: []
    // ── 前置模组缓存 (slug → deps)：返回上一级时避免重新网络请求（2026-08-07 修复）──
    property var _depsCache: ({})

    // ── 版本列表入场动画 ──
    property bool _versionListEnter: false
    // ── 版本数据缓存 (slug → {rawVersions, versionMap, grouped}) ──
    property var _versionCache: ({})
    // ── 2026-08-15：版本级前置依赖（悬停 tooltip）──
    property var _versionDepsCache: ({})      // versionId → deps
    property string _hoverDepsVersion: ""     // 当前悬停版本的 Modrinth versionId
    property bool _hoverDepsLoading: false
    property var _hoverDepsList: []
    // NOTE: 初始 true（无前置证据 → 不显示）
    property bool _hoverDepsEmpty: true
    // NOTE: Popup 宽度估算（名字长度自适应）
    property int _depsEstWidth: 260
    // ── 2026-08-15 单例 tooltip 状态（每卡片一个 Popup 会拖慢详情页：Popup 重量级）──
    property bool _tipHovered: false      // 有卡片悬停
    property real _tipX: -10000
    property real _tipY: -10000
    // 实验性开关（设置-实验性功能）：关闭时悬停不显示任何前置信息
    property bool _tipEnabled: backend ? !!backend.modDepsTooltipEnabled : false

    // ── tooltip 位置计算（绑定当前悬停卡片的鼠标位置，持续跟随 + 翻转防溢出）──
    function _computeTipX(anchor, px, py) {
        var p = anchor.mapToItem(root, px, py)
        var gap = 12
        var tipW = Math.min(500, root._depsEstWidth)
        return (p.x + gap + tipW > root.width) ? p.x - gap - tipW : p.x + gap
    }
    function _computeTipY(anchor, px, py) {
        var p = anchor.mapToItem(root, px, py)
        var gap = 12
        var tipH = 120
        var midY = p.y - tipH / 2
        if (midY < 4) return p.y + gap
        if (midY + tipH > root.height) return p.y - tipH - gap
        return midY
    }
    function _hideDepsTip() {
        _tipHovered = false
    }

    // ── tooltip 文本组装（richText 多行：Text 单组件排版，杜绝布局重叠）──
    function _depsTipRichText() {
        if (_hoverDepsLoading)
            return "<b>前置模组</b><br>&nbsp;&nbsp;正在获取前置模组..."
        var lines = ["<b>前置模组</b>"]
        for (var i = 0; i < _hoverDepsList.length; i++) {
            var d = _hoverDepsList[i]
            var t = d.dependency_type === "required" ? "必需" : "可选"
            var v = d.version_number || "任意版本"
            var tc = d.dependency_type === "required" ? StyleTokens.warning : StyleTokens.textSubtle
            lines.push("&nbsp;&nbsp;" + (d.title || d.project_id || "") +
                       "&nbsp;&nbsp;<font color='#787c90'>" + v + "</font>" +
                       "&nbsp;&nbsp;<font color='" + tc + "'>" + t + "</font>")
        }
        return lines.join("<br>")
    }

    signal goBack()

    // ── Listen for dependency resolution ──
    Connections {
        target: backend && backend.modManager ? backend.modManager : null
        function onDependenciesResolved(slug, deps) {
            if (slug !== modDetailSlug) return
            modDetailDepsLoading = false
            var enriched = enrichDeps(deps)
            modDetailDependencies = enriched
            showDeps = enriched.length > 0
            // 写入缓存：返回上一级再进入时秒开（2026-08-07 修复）
            _depsCache[slug] = enriched
        }
    }

    // ── 司南引擎图标就绪：更新详情页大图（未缓存时先空，下载完成即显）──
    Connections {
        target: backend
        function onIconReady(url, localPath) {
            if (modDetailIconRaw && url === modDetailIconRaw)
                modDetailIcon = localPath
            // 依赖卡图标：司南引擎下载完成后更新对应项，重新赋值数组触发刷新
            var arr = modDetailDependencies
            var changed = false
            for (var i = 0; i < arr.length; i++) {
                if (arr[i].icon_url === url) {
                    arr[i].icon = localPath
                    changed = true
                }
            }
            if (changed)
                modDetailDependencies = arr.slice()
        }
    }
    function resolveDetailIcon() {
        if (!modDetailIconRaw || !backend) return ""
        return backend.resolveIconUrl(modDetailIconRaw)
    }

    // ── Trigger version fetch ──
    onModDetailSlugChanged: {
        refreshLinks()
        if (modDetailSlug && backend) {
            // 切换详情：滚动复位 + 版本计数瞬时归零（避免旧内容残留、计数动画拖尾）
            contentFlick.contentY = 0
            countBehavior.enabled = false
            verCountText._displayCount = 0
            countBehavior.enabled = true
            var isCf = /^\d+$/.test(modDetailSlug)
            // ── CurseForge 详情（slug 为纯数字 modId）──
            if (isCf) {
                var cachedCf = _versionCache[modDetailSlug]
                if (cachedCf) {
                    modDetailLoading = false
                    modDetailRawVersions = cachedCf.raw
                    modDetailVersionMap = cachedCf.map
                    expandedGroups = []
                    showTestVersions = false
                    _rebuildGrouped()
                } else {
                    modDetailLoading = true
                    modDetailRawVersions = []
                    modDetailVersionMap = {}
                    expandedGroups = []
                    showTestVersions = false
                    _versionListEnter = true
                    backend.fetchModVersionsCf(modDetailSlug)
                }
            } else {
                // ── 检查版本缓存 ──
                var cached = _versionCache[modDetailSlug]
                if (cached) {
                    modDetailLoading = false
                    modDetailRawVersions = cached.raw
                    modDetailVersionMap = cached.map
                    expandedGroups = []
                    showTestVersions = false
                    _rebuildGrouped()
                } else {
                    modDetailLoading = true
                    modDetailRawVersions = []
                    modDetailVersionMap = {}
                    expandedGroups = []
                    showTestVersions = false
                    _versionListEnter = false
                    backend.fetchModVersions([modDetailSlug])
                }
            }

            // Fetch dependencies（CF 与 Modrinth 统一在此处理，勿提前 return——2026-08-07 修复）
            if (backend.modManager) {
                // 缓存命中：直接显示，不重新请求（返回上一级时秒开）
                var cachedDeps = _depsCache[modDetailSlug]
                if (cachedDeps) {
                    // 重新 resolve 图标本地缓存（首次未命中时已触发下载，二次进入秒开）
                    modDetailDependencies = enrichDeps(cachedDeps)
                    showDeps = modDetailDependencies.length > 0
                    modDetailDepsLoading = false
                } else {
                    modDetailDepsLoading = true
                    modDetailDependencies = []
                    showDeps = false
                    if (isCf) {
                        // CF 详情：走 mod 详情端点拉依赖（与版本列表解耦，双源均带 latestFiles.dependencies）
                        if (backend.fetchCfDependencies)
                            backend.fetchCfDependencies(modDetailSlug)
                    } else {
                        backend.modManager.getModDependencies(modDetailSlug)
                    }
                }
            }
        }
    }

    // ── Helpers ──
    // 依赖数据统一加工：中文名反查 + 图标换镜像域名 + 走司南引擎本地缓存（icon 字段，命中秒开/未命中空）
    function enrichDeps(deps) {
        var enriched = []
        var arr = deps || []
        for (var i = 0; i < arr.length; i++) {
            var d = arr[i]
            var e = {}
            for (var k in d) e[k] = d[k]
            // 中文名反查
            var zh = (backend && d.title) ? backend.resolveModZh(d.title) : ""
            if (zh) e.zh = zh
            // 图标：换镜像域名（与列表卡一致），再走司南引擎本地缓存
            if (e.icon_url) {
                e.icon_url = e.icon_url.replace("cdn.modrinth.com", "mod.mcimirror.top")
                                       .replace("cdn-alt.modrinth.com", "mod.mcimirror.top")
                e.icon = backend ? backend.resolveIconUrl(e.icon_url) : ""
            } else {
                e.icon = ""
            }
            enriched.push(e)
        }
        return enriched
    }
    function stripSuffix(v) {
        var re = /-(?:snapshot|pre|rc|alpha|beta)[\d.\-]*$/i
        var m = v.match(re)
        return m ? v.slice(0, m.index) : v
    }
    // Strip composite-key loader suffix: "1.21.8|fabric" → "1.21.8"
    function stripLoader(v) {
        var idx = v.lastIndexOf("|")
        return idx >= 0 ? v.substring(0, idx) : v
    }
    function preReleaseTag(v) {
        if (/-snapshot/i.test(v)) return "快照版"
        if (/-pre/i.test(v)) return "预览版"
        if (/-rc/i.test(v)) return "发布候选版"
        return ""
    }
    function isTestVersion(v) {
        // ── 2026-08-18：测试版规则全覆盖 ──
        // 标准快照：25w14a、21w19a、20w14i（^\d{1,2}w\d{2}[a-z]$）
        // 愚人节特殊快照：
        //   20w14∞        — ∞ 符号后缀
        //   20w14infinite  — 单词后缀
        //   22w13oneblockatatime — 单词后缀
        //   23w13a_or_b    — a_or_b 后缀
        //   24w14potato    — potato 后缀
        //   25w14craftmine — craftmine 后缀
        //   1.RV-Pre1      — 愚人节 RV 版本
        // 预发布/候选（1.17-pre1 / 1.17-rc1）由 stripSuffix 剥离，此处不重复
        if (/^\d{1,2}w\d{2}[a-z]+(?:_or_b)?$/i.test(v)) return true   // 标准 + 单词/或后缀
        if (/^\d{1,2}w\d{2}[a-z]+$/i.test(v)) return true             // 冗余安全
        if (/^\d{1,2}w\d{2}∞$/.test(v)) return true                   // ∞ 符号
        if (/^1\.RV(-pre\d+)?$/i.test(v)) return true                 // 愚人节 RV
        return false
    }
    function testMajor(v) {
        var m = v.match(/^(\d{1,2}w)/i)
        return m ? m[1] : v
    }

    // ── Loader tag colors (unified with VersionSettingsOverlay top bar) ──
    function _tagColor(loader) {
        // All known loaders use white text (StyleTokens.textPrimary)
        if (loader === "fabric" || loader === "forge" || loader === "neoforge"
            || loader === "quilt" || loader === "liteloader" || loader === "optifine")
            return StyleTokens.textPrimary
        return StyleTokens.accentLink
    }
    function _tagBg(loader) {
        if (loader === "fabric") return StyleTokens.loaderFabric
        if (loader === "forge") return StyleTokens.textDanger
        if (loader === "neoforge") return StyleTokens.loaderNeoforge
        if (loader === "quilt") return StyleTokens.success
        if (loader === "liteloader") return StyleTokens.accentHover
        if (loader === "optifine") return StyleTokens.loaderOptifine
        return StyleTokens.accentSubtle
    }
    function _capLoader(loader) {
        if (!loader) return ""
        return loader.charAt(0).toUpperCase() + loader.slice(1)
    }

    property bool showTestVersions: false

    // ── 2026-08-18：grouped 计算降频 + 懒渲染 ──
    // 旧实现是 property 绑定：modDetailRawVersions（最多 4300 项）每次变化都同步
    // 全量重算 grouped（O(n) JS 循环 + 每项 map 查表 + 分组排序），在数据到达的
    // 同一帧阻塞主线程 → 详情页卡死。改为显式 _rebuildGrouped()：数据就绪后
    // Qt.callLater 延迟一帧批量计算一次（期间 UI 保持 spinner），结果存缓存，
    // 不再由绑定驱动反复求值。
    property var grouped: _groupedCache
    property var _groupedCache: []
    property bool _groupedComputing: false

    function _rebuildGrouped() {
        if (_groupedComputing) return   // 已排队，防重入
        _groupedComputing = true
        Qt.callLater(function() {
            _groupedComputing = false
            var groups = {}
            var raw = modDetailRawVersions || []
            var map = modDetailVersionMap || {}
            for (var i = 0; i < raw.length; i++) {
                var v = raw[i]
                var d = map[v]
                var gv = d ? (d.gameVersion || stripLoader(v)) : stripLoader(v)
                var gvs = d ? (d.gameVersions || []) : []
                if (gvs.length === 0) gvs = [gv]
                var first = gvs[0] || gv
                var base = stripSuffix(first)
                if (!showTestVersions && isTestVersion(base)) continue
                var major
                if (isTestVersion(base)) {
                    major = testMajor(base)
                } else {
                    var parts = base.split(".")
                    major = parts.length >= 2 ? parts[0] + "." + parts[1] : base
                }
                if (!groups[major]) groups[major] = []
                groups[major].push(v)
            }
            for (var k in groups) {
                groups[k].sort(function(a,b){
                    var da = getVersionDetail(a); var db = getVersionDetail(b)
                    var dateA = da ? da.date : ""; var dateB = db ? db.date : ""
                    if (dateA > dateB) return -1; if (dateA < dateB) return 1; return 0
                })
            }
            var result = []
            for (var kk in groups) { result.push({major: kk, versions: groups[kk]}) }
            result.sort(function(a,b){
                var aTest = /w$/i.test(a.major), bTest = /w$/i.test(b.major)
                if (aTest && !bTest) return -1
                if (!aTest && bTest) return 1
                if (aTest && bTest) { return parseInt(a.major) - parseInt(b.major) }
                var as = a.major.split("."), bs = b.major.split(".")
                var am = parseInt(as[0])||0, bm = parseInt(bs[0])||0
                if (am !== bm) return bm - am
                return (parseInt(bs[1])||0) - (parseInt(as[1])||0)
            })
            _groupedCache = result
            // 版本计数回填（缓存命中路径不走 onModVersionsPartial，需在此更新）
            verCountText._displayCount = raw.length
            // 数据就绪 → 触发入场
            if (raw.length > 0) _versionListEnter = true
        })
    }
    property var expandedGroups: []

    function isExpanded(major) { return expandedGroups.indexOf(major) >= 0 }
    function toggleGroup(major) {
        var arr = expandedGroups.slice(); var idx = arr.indexOf(major)
        if (idx >= 0) arr.splice(idx, 1); else arr.push(major)
        expandedGroups = arr
    }
    function getVersionDetail(verStr) {
        var map = modDetailVersionMap || {}; return map[verStr] || null
    }
    // ── 2026-08-15：版本卡片悬停 → 显示/请求该版本的前置依赖 ──
    // 有缓存直接显示；无缓存先置 loading 再调后端（返回后 onVersionDependenciesResolved 回填）
    // 项目级无任何前置（modDetailDependencies 空）→ 该 mod 版本不可能有前置 → 不显示
    //（_hoverDepsEmpty 保持 true）
    function _showVersionDeps(verStr) {
        if (modDetailDependencies.length === 0) return
        var d = getVersionDetail(verStr)
        var vid = d ? (d.id || "") : ""
        if (!vid) return
        _hoverDepsVersion = vid
        if (_versionDepsCache[vid] !== undefined) {
            _hoverDepsLoading = false
            _hoverDepsList = _versionDepsCache[vid]
            _hoverDepsEmpty = _hoverDepsList.length === 0
            return
        }
        _hoverDepsLoading = true
        _hoverDepsList = []
        _hoverDepsEmpty = false   // 确认请求发出 → 显示 loading
        if (backend) backend.fetchVersionDependencies(modDetailSlug, vid)
    }
    function formatDate(isoStr) {
        if (!isoStr) return "-"; return isoStr.slice(0, 10)
    }
    function formatDL(n) {
        if (n >= 100000000) return (n / 100000000).toFixed(1) + "亿"
        if (n >= 10000) return (n / 10000).toFixed(0) + "万"
        return String(n || 0)
    }

    // ━━━━━━━━━━━━━━━━━━━━ TOP BAR ━━━━━━━━━━━━━━━━━━━━
    Rectangle {
        id: topBar
        anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
        height: 44; color: hasBg ? "transparent" : StyleTokens.bgPrimary; z: 10

        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12; spacing: 10

            // Back button — InstallPage style
            BackButton {
                id: backBtn
                onClicked: {
                    var stack = modNavStack || []
                    if (stack.length > 0) {
                        var prev = stack.pop()
                        modNavStack = stack
                        modDetailSlug = prev.slug
                        modDetailTitle = prev.title
                        modDetailZh = prev.zh || ""
                        modDetailDesc = prev.desc || ""
                        modDetailIcon = prev.icon || ""
                    } else {
                        root.goBack()
                    }
                }
            }

            Item { Layout.fillWidth: true }

            // Title
            Text {
                text: displayTitle
                font.pixelSize: StyleTokens.fontSizeLg; font.weight: Font.Bold; color: StyleTokens.textSecondary
                Layout.fillWidth: true
                elide: Text.ElideRight; horizontalAlignment: Text.AlignHCenter
            }

            Item { Layout.fillWidth: true }
            Item { width: backBtn.width } // spacer for symmetry
        }
    }

    // ━━━━━━━━━━━━━━━━━━━━ CONTENT ━━━━━━━━━━━━━━━━━━━━
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
                cardIcon: root.modDetailIcon !== "" ? root.modDetailIcon : (root.modDetailIconRaw ? root.resolveDetailIcon() : "")
                cardTitle: root.displayTitle
                cardDesc: root.modDetailDesc

                // Stats
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 24
                    Text {
                        text: "Slug: " + (modDetailSlug || "")
                        color: StyleTokens.textSubtle; font.pixelSize: StyleTokens.fontSizeSm
                        elide: Text.ElideRight; Layout.fillWidth: true
                    }
                    Text {
                        id: verCountText
                        property int _displayCount: 0
                        text: qsTr("版本数量: ") + _displayCount
                        color: StyleTokens.textSubtle; font.pixelSize: StyleTokens.fontSizeSm
                        Behavior on _displayCount {
                            id: countBehavior
                            NumberAnimation { duration: 2000; easing.type: Easing.OutCubic }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Rectangle {
                        id: testToggleBtn
                        Layout.preferredWidth: Math.max(64, testBtn.implicitWidth + 14)
                        Layout.preferredHeight: 22
                        Layout.alignment: Qt.AlignVCenter
                        radius: StyleTokens.radiusSm
                        color: showTestVersions ? StyleTokens.infoBg : StyleTokens.bgSecondary
                        border.color: (testHov.containsMouse || showTestVersions) ? StyleTokens.accentHover : StyleTokens.borderLight
                        border.width: (testHov.containsMouse || showTestVersions) ? 1.5 : 1

                        property real _eScale: 1.0
                        scale: _eScale
                        Timer { id: testRestoreTimer; interval: 100
                            onTriggered: { testToggleBtn._eScale = 1.0 }
                        }
                        Behavior on color { ColorAnimation { duration: 150 } }
                        Behavior on border.color { ColorAnimation { duration: 150 } }
                        Behavior on border.width { NumberAnimation { duration: 150 } }
                        Behavior on _eScale {
                            SpringAnimation { spring: 1.8; damping: 0.3; epsilon: 0.01 }
                        }
                        Text {
                            id: testBtn; anchors.centerIn: parent; font.pixelSize: StyleTokens.fontSizeXs
                            text: showTestVersions ? "隐藏测试版" : "显示测试版"
                            color: showTestVersions ? StyleTokens.accentLink : StyleTokens.textMuted
                        }
                        MouseArea {
                            id: testHov; anchors.fill: parent; hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                testToggleBtn._eScale = 0.9
                                testRestoreTimer.restart()
                                showTestVersions = !showTestVersions
                                _rebuildGrouped()   // 2026-08-18：切换后异步重建分组
                            }
                        }
                    }

                    // ── 跳转 / 复制按钮（DetailLinkBar 纯文本胶囊，与测试版开关同行）──
                    DetailLinkBar {
                        Layout.fillWidth: true
                        backend: root.backend
                        toastManager: root.toastManager
                        links: root._linkItems
                        copyItems: root._copyItems
                    }
                }
            }

            // ── 前置模组提醒 ──
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: (showDeps && !modDetailLoading) ? depSection.implicitHeight : 0
                visible: showDeps && !modDetailLoading && modDetailDependencies.length > 0
                clip: true

                ColumnLayout {
                    id: depSection
                    width: parent.width
                    spacing: 8

                    Text {
                        text: "\u524D\u7F6E\u6A21\u7EC4"
                        color: StyleTokens.warning
                        font.pixelSize: StyleTokens.fontSizeSm
                        font.weight: Font.Medium
                    }

                    Repeater {
                        model: modDetailDependencies
                        delegate: Rectangle {
                            id: depCard
                            Layout.fillWidth: true
                            height: 48
                            radius: StyleTokens.radiusMd
                            color: depHover.containsMouse ? "#252a1c10" : "#1a2b1c00"
                            border.color: depHover.containsMouse ? "#804b3a00" : "#664b3a00"

                            opacity: 0
                            Timer {
                                interval: index * 80 + 200
                                running: showDeps && !modDetailLoading
                                repeat: false
                                onTriggered: depCard.opacity = 1
                            }
                            Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on border.color { ColorAnimation { duration: 150 } }

                            Rectangle {
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: 3; radius: StyleTokens.radiusXs
                                color: StyleTokens.warning
                            }

                            Row {
                                anchors.left: parent.left
                                anchors.leftMargin: 14
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 10

                                Text {
                                    text: "\u26A0"
                                    anchors.verticalCenter: parent.verticalCenter
                                    font.pixelSize: StyleTokens.fontSizeLg
                                }

                                Rectangle {
                                    width: 30; height: 30; radius: StyleTokens.radiusSm; color: StyleTokens.bgInput
                                    anchors.verticalCenter: parent.verticalCenter
                                    clip: true
                                    Image {
                                        anchors.fill: parent
                                        fillMode: Image.PreserveAspectCrop
                                        asynchronous: true; cache: true
                                        source: modelData.icon || modelData.icon_url || ""
                                        sourceSize.width: 60; sourceSize.height: 60
                                    }
                                }

                                Column {
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 2
                                    Text {
                                        text: {
                                            var t = modelData.title || modelData.project_id || ""
                                            if (modelData.zh && modelData.zh !== modelData.title)
                                                return modelData.zh + "（" + t + "）"
                                            return t
                                        }
                                        color: depHover.containsMouse ? "#f5d080" : "#f0c060"
                                        font.pixelSize: StyleTokens.fontSizeMd
                                        font.weight: Font.Medium
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        text: modelData.dependency_type === "required" ? "\u5FC5\u9700\u524D\u7F6E" : "\u53EF\u9009\u524D\u7F6E"
                                        color: StyleTokens.warning
                                        font.pixelSize: StyleTokens.fontSizeSm
                                    }
                                }
                            }

                            MouseArea {
                                id: depHover
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: modelData.slug ? Qt.PointingHandCursor : Qt.ArrowCursor
                                onClicked: {
                                    if (modelData.slug) {
                                        var stack = modNavStack || []
                                        stack.push({
                                            slug: modDetailSlug,
                                            title: modDetailTitle,
                                            zh: modDetailZh,
                                            desc: modDetailDesc,
                                            icon: modDetailIcon
                                        })
                                        modNavStack = stack
                                        modDetailTitle = modelData.title
                                        modDetailZh = modelData.zh || ""
                                        modDetailDesc = modelData.description || ""
                                        modDetailIcon = modelData.icon || modelData.icon_url || ""
                                        modDetailSlug = modelData.slug
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ── Loading indicator ──
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: (modDetailLoading || grouped.length === 0) ? 60 : 0
                visible: modDetailLoading || grouped.length === 0

                Row {
                    anchors.centerIn: parent; spacing: 8

                    Rectangle {
                        id: spinnerBox
                        width: 24; height: 24; radius: StyleTokens.radiusXl; color: "transparent"
                        visible: modDetailLoading

                        property real _angle: 0
                        NumberAnimation on _angle {
                            running: modDetailLoading
                            from: 0; to: 360; duration: 1000; loops: Animation.Infinite
                        }
                        on_AngleChanged: spinCanvas.requestPaint()

                        Canvas {
                            id: spinCanvas
                            anchors.fill: parent; visible: modDetailLoading
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.clearRect(0, 0, width, height)
                                var cx = width / 2, cy = height / 2
                                var r = Math.min(cx, cy) - 3
                                if (r <= 0) return
                                var startRad = (spinnerBox._angle - 90) * Math.PI / 180
                                var endRad = (spinnerBox._angle + 180) * Math.PI / 180
                                ctx.strokeStyle = StyleTokens.accentVivid
                                ctx.lineWidth = 2; ctx.lineCap = "round"
                                ctx.beginPath()
                                ctx.arc(cx, cy, r, startRad, endRad)
                                ctx.stroke()
                            }
                        }
                    }
                    Text {
                        text: modDetailLoading ? "加载版本中..."
                            : (grouped.length === 0 ? "无可用版本" : "")
                        color: StyleTokens.textMuted; font.pixelSize: StyleTokens.fontSizeSm
                    }
                }
            }

            // ── Section: Version List ──
            Text {
                visible: _versionListEnter
                opacity: _versionListEnter ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
                text: qsTr("版本列表")
                font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.DemiBold; color: StyleTokens.textTertiary
                Layout.topMargin: 8; Layout.leftMargin: 4
            }

            // ── Version groups ──
            ColumnLayout {
                id: versionGroupsLayout
                Layout.fillWidth: true
                spacing: 8
                visible: _versionListEnter

                Repeater {
                    model: !modDetailLoading ? grouped : []
                    delegate: ExpandableGroupCard {
                        id: groupCard
                        Layout.fillWidth: true
                        title: "MC " + modelData.major
                        subtitle: modelData.versions.length + " 个版本"
                        expanded: isExpanded(modelData.major)
                        onToggled: toggleGroup(modelData.major)

                        // ── 2026-08-18：懒渲染（大 Mod 分组展开不卡主线程）──
                        // 每组默认只渲染前 30 个版本，点“还有 X 个版本”追加 50 个。
                        property int _visibleCount: 30
                        readonly property int _totalCount: modelData.versions.length
                        readonly property bool _hasMore: _visibleCount < _totalCount

                        // ── 错峰入场 ──
                        opacity: 0
                        Timer {
                            interval: index * 80 + 100
                            running: _versionListEnter
                            repeat: false
                            onTriggered: groupCard.opacity = 1
                        }
                        Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutBack; easing.overshoot: 0.2 } }

                    Repeater {
                        // ── 2026-08-18：懒渲染 ──
                        // 大 Mod（如 simple-voice-chat 4300 版本）展开分组时一次性创建
                        // 全部 delegate 会卡死主线程。每分组只渲染前 _visibleCount 个
                        //（未展开时为空数组 → 零 delegate），超出部分由分组底部的
                        // “还有 X 个版本”按钮点击加载更多。
                        model: groupCard.expanded
                            ? modelData.versions.slice(0, groupCard._visibleCount)
                            : []
                        // ── 2026-08-15：版本卡片外包悬停层（前置依赖 tooltip）──
                        // 外包 Item 保持原布局（width/x），Popup 挂 root 防 clip 裁剪，
                        // 翻转逻辑仿 StatsPage（右侧溢出时翻到左侧）
                        delegate: Item {
                            id: verRow
                            width: parent.width - 24
                            x: 24
                            // NOTE: 2026-08-15 必须显式行高！Item 默认 implicitHeight=0，
                            // 外包后版本卡片行塌陷 → 分组展开后内容不可见（"下拉框展不开"）。
                            implicitHeight: verCard.implicitHeight

                            HoverHandler {
                                id: verHover
                                onHoveredChanged: {
                                    if (verHover.hovered && root._tipEnabled) {
                                        root._showVersionDeps(modelData)
                                        root._tipHovered = true
                                    } else {
                                        root._hideDepsTip()
                                    }
                                }
                            }
                            // ── 鼠标跟随：hovered 时由本卡片的鼠标位置持续驱动单例
                            // Popup 位置（只有 hovered 的这对 Binding 活跃，开销极小）──
                            Binding {
                                target: root; property: "_tipX"
                                value: root._computeTipX(verRow, verHover.point.position.x, verHover.point.position.y)
                                when: verHover.hovered && root._tipEnabled
                            }
                            Binding {
                                target: root; property: "_tipY"
                                value: root._computeTipY(verRow, verHover.point.position.x, verHover.point.position.y)
                                when: verHover.hovered && root._tipEnabled
                            }

                            DetailVersionCard {
                                id: verCard
                                width: parent.width
                                versionLabel: {
                                    var d = getVersionDetail(modelData)
                                    return d ? d.versionNumber : modelData
                                }

                                tags: {
                                    var result = []
                                    var d = getVersionDetail(modelData)
                                    if (d && d.loaders) {
                                        for (var li = 0; li < d.loaders.length; li++) {
                                            var rawLoader = d.loaders[li].toLowerCase()
                                            result.push({
                                                text: root._capLoader(rawLoader),
                                                color: root._tagColor(rawLoader),
                                                bg: root._tagBg(rawLoader)
                                            })
                                        }
                                    }
                                    var gvClean = d ? (d.gameVersion || "") : ""
                                    var pr = preReleaseTag(gvClean || root.stripLoader(modelData))
                                    if (pr) result.push({text: pr, color: StyleTokens.warning, bg: StyleTokens.warningBg})
                                    return result
                                }

                                infoLines: {
                                    var d = getVersionDetail(modelData)
                                    var gvClean = d ? (d.gameVersion || "") : ""
                                    return [
                                        { label: "MC:", value: d ? (d.gameVersions || [gvClean || modelData]).join(", ") : (gvClean || modelData) },
                                        // CF 源下载量数据不可靠（镜像 downloadCount 恒 0），
                                        // CF 详情卡片不显示下载量（2026-08-15）
                                        { label: "", value: (root._isCfDetail
                                            ? formatDate(d ? d.date : "")
                                            : formatDate(d ? d.date : "") + "  |  下载量 " + formatDL(d ? d.downloads : 0)) }
                                    ]
                                }

                                hasDownload: true
                                onDownloadClicked: {
                                    var d = getVersionDetail(modelData)
                                    if (!d || !d.url) {
                                        if (toastManager) toastManager.show("无法获取下载地址")
                                        return
                                    }
                                    var loaders = d.loaders || []
                                    var loader = loaders.length > 0 ? loaders[0] : ""
                                    var vn = d.versionNumber || modelData
                                    var safeTitle = (modDetailTitle || modDetailSlug || "mod").replace(/[\\\/:*?"<>|]/g, "_").replace(/\s+/g, "_")
                                    var fn = safeTitle + "-" + vn + (loader ? "-" + loader : "") + ".jar"
                                    var mineDir = String(backend ? (backend.minecraftDir || "") : "")
                                    var defaultPath = mineDir ? (mineDir.replace(/\\+$/, "") + "/" + fn) : fn
                                    pendingModDownload = {
                                        slug: modDetailSlug, title: modDetailTitle || modDetailSlug,
                                        versionNumber: vn, loader: loader, gameVersion: d.gameVersion || root.stripLoader(modelData),
                                        url: d.url, filename: fn, size: d.size || 0,
                                        sha1: d.sha1 || "", defaultPath: defaultPath,
                                        displayName: (modDetailTitle || modDetailSlug) + " " + vn
                                    }
                                    // 默认定位到 versions 文件夹，方便用户选择安装到的游戏版本
                                    var versionsFolder = backend ? backend.gameDir + "/versions" : "."
                                    modFileDialog.currentFolder = "file:///" + versionsFolder.replace(/\\/g, "/")
                                    modFileDialog.currentFile = "file:///" + defaultPath.replace(/\\/g, "/")
                                    modFileDialog.open()
                                }
                        }
                }
            }

                // ── 2026-08-18：懒渲染“还有 X 个版本”（大 Mod 分组点击追加）──
                Rectangle {
                    visible: groupCard.expanded && groupCard._hasMore
                    width: parent ? parent.width - 56 : 0
                    anchors.left: parent ? parent.left : undefined
                    anchors.leftMargin: 24
                    height: 30
                    radius: StyleTokens.radiusMd
                    color: moreHov.containsMouse ? StyleTokens.accentSubtle : "transparent"
                    border.color: moreHov.containsMouse ? StyleTokens.accentHover : "transparent"
                    border.width: 1
                    Behavior on color { ColorAnimation { duration: 150 } }
                    Text {
                        anchors.centerIn: parent
                        text: "还有 " + (groupCard._totalCount - groupCard._visibleCount) + " 个版本"
                        color: moreHov.containsMouse ? StyleTokens.accentLink : StyleTokens.textMuted
                        font.pixelSize: StyleTokens.fontSizeSm
                    }
                    MouseArea {
                        id: moreHov; anchors.fill: parent; hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            groupCard._visibleCount += 50
                        }
                    }
                }
            }
            }
            }  // ColumnLayout

            Item { Layout.fillWidth: true; height: 40 }
        }
    }

    // ━━━━━━━━━━━━━━━━━━━━ CONNECTIONS ━━━━━━━━━━━━━━━━━━━━
    Connections {
        target: backend
        enabled: backend !== null
        function onModVersionsPartial(slug, versions, details) {
            if (slug !== root.modDetailSlug) return
            root.modDetailLoading = false
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
                    arr.push(v)
                    // Use C++ game_version for plain MC version; fall back to
                    // stripping loader suffix from composite key (e.g. "1.21.8|fabric")
                    var gameVer = d ? (d.game_version || "") : ""
                    if (!gameVer) {
                        var pipeIdx = v.lastIndexOf("|")
                        gameVer = pipeIdx >= 0 ? v.substring(0, pipeIdx) : v
                    }
                    map[v] = {
                        versionNumber: d ? (d.version_number || v) : v,
                        id: d ? (d.id || "") : "",   // 2026-08-15：Modrinth versionId（悬停前置依赖用）
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
                }
            }
            root.modDetailRawVersions = arr
            root.modDetailVersionMap = map
            verCountText._displayCount = arr.length
            // ── 存入缓存 ──
            root._versionCache[root.modDetailSlug] = { raw: arr, map: map }
            // ── 2026-08-18：异步重建分组（Qt.callLater 延迟一帧，主线程不卡）──
            root._rebuildGrouped()

            // CF 详情前置模组已在 onModDetailSlugChanged 走 fetchCfDependencies
            // （/mods/{id} latestFiles 提取；镜像与官方双源均带依赖，2026-08-07 实测）
        }
        function onCfDependenciesResolved(modId, deps) {
            if (modId !== root.modDetailSlug) return
            root.modDetailDepsLoading = false
            var enriched = root.enrichDeps(deps)
            root.modDetailDependencies = enriched
            root.showDeps = enriched.length > 0
            // 写入缓存：返回上一级再进入时秒开（2026-08-07 修复）
            root._depsCache[modId] = enriched
        }
        // ── 2026-08-15：版本级前置依赖（悬停 tooltip）──
        // 结果总是缓存；仅当仍悬停该版本时更新 UI（快速悬停多个版本时旧结果不覆盖）
        function onVersionDependenciesResolved(versionId, deps) {
            root._versionDepsCache[versionId] = (deps || [])
            if (versionId === root._hoverDepsVersion) {
                root._hoverDepsLoading = false
                root._hoverDepsList = root._versionDepsCache[versionId]
                root._hoverDepsEmpty = root._hoverDepsList.length === 0
                // 按最长前置名估算 Popup 宽度（名字 + 版本号~190 + 标签30 + 间距/内边距~40）
                var maxLen = 0
                for (var di = 0; di < root._hoverDepsList.length; di++) {
                    var t = root._hoverDepsList[di].title || root._hoverDepsList[di].project_id || ""
                    maxLen = Math.max(maxLen, t.length)
                }
                root._depsEstWidth = Math.max(260, Math.min(480, maxLen * 8 + 260))
            }
        }
        function onModVersionsProgress(done, total) {
            if (root.modDetailSlug === "") return
            root.modDetailLoading = done < total
        }
        function onModFileDownloadFailed(dlId, errorDetail, displayName) {
            if (mainWindow) {
                mainWindow.modDlErrorInfo = {dlId: dlId, displayName: displayName, errorDetail: errorDetail}
                mainWindow.showModDlError = true
            }
        }
    }

    // ── Mod download file dialog ──
    FileDialog {
        id: modFileDialog
        fileMode: FileDialog.SaveFile
        title: "保存 Mod 文件"
        nameFilters: ["JAR 文件 (*.jar)", "所有文件 (*.*)"]
        onAccepted: {
            var p = pendingModDownload
            if (!p || !p.url) return
            var savePath = String(selectedFile).replace(/^(file:\/{2,3})/i, "")
            if (!/\.jar$/i.test(savePath)) savePath += ".jar"
            var dlId = backend.downloadModFile(p.url, savePath, p.displayName, p.size || 0, p.sha1 || "")
            if (toastManager) toastManager.show("开始下载 " + p.displayName)
            if (mainWindow) mainWindow.showModDownloadProgress()
            pendingModDownload = {}
        }
        onRejected: { pendingModDownload = {} }
    }

    // ── 2026-08-15：单例前置依赖 tooltip（整个页面共享一个，杜绝每卡片一个 Popup 拖慢）──
    // 位置由 _positionDepsTip 在 hover 时设置；内容单 Text richText 排版；
    // 无前置（_hoverDepsEmpty）→ 不显示；鼠标移入 Popup 保持显示（tipArea）。
    Popup {
        id: depsTip
        parent: root
        visible: (root._tipHovered || tipArea.containsMouse) && !root._hoverDepsEmpty
        x: root._tipX
        y: root._tipY
        padding: 10
        closePolicy: Popup.NoAutoClose
        z: 100

        MouseArea {
            id: tipArea
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.NoButton
        }

        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 120; easing.type: Easing.OutCubic }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 100; easing.type: Easing.InCubic }
        }
        background: Rectangle {
            radius: StyleTokens.radiusMd
            color: StyleTokens.accentSubtle
            border.color: StyleTokens.bgInput; border.width: 1
        }
        contentItem: Text {
            id: depsTipContent
            text: root._depsTipRichText()
            textFormat: Text.RichText
            font.pixelSize: StyleTokens.fontSizeSm
            color: StyleTokens.textSecondary
            width: parent.width
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        }
        width: Math.min(500, root._depsEstWidth)
        height: depsTipContent.implicitHeight + 20
    }
}
