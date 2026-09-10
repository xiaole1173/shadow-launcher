// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Effects

Window {
    id: appWindow
    visible: true
    flags: Qt.FramelessWindowHint | Qt.Window
    minimumWidth: 800; minimumHeight: 550
    width: 960; height: 620
    title: "Shadow Launcher"
    color: "transparent"

    property int navListIndex: 0
    property int pendingSubTab: -1  // for --navigate auto-test
    property string currentSelectedVersion: backend ? backend.selectedVersion : ""
    property int loginMode: backend ? backend.lastLoginMode : 0
    property bool showVersionSelect: false
    property bool showVersionSettings: false
    // 版本选择与版本设置互斥：开设置必收选择（自定义背景透明时两浮层会视觉叠加，点击只到上层）
    onShowVersionSettingsChanged: if (showVersionSettings) showVersionSelect = false
    // 全局拖放提示文案（由 packDropArea 按拖入类型写入）
    property string packDropHintTitle: qsTr("松开以导入整合包")
    property string packDropHintSub: qsTr("支持 .zip（CurseForge）与 .mrpack（Modrinth）")
    property bool showInstallPage: false
    property var debugWindow: null
    property string installMcVersion: ""
    property var offlineHistory: []
    property bool pageLoading: false
    property bool _settingsFadeOut: false
    property bool _installProgressFadeOut: false
    property bool _dlFadeOut: false
    property bool _usFadeOut: false
    property var runningListModel: []
    property bool _pendingCrashAnalyze: false   // Loader 异步时挂起崩溃分析请求
    property var _pendingCrashResult: null      // Loader 异步时挂起崩溃分析结果

    // ── 官网页面直链（2026-09-06：v1.0.2 更新内容 / 启动器公告）──
    // 官网 VitePress 渲染，仅直链可访问（站点无导航入口、不进站内搜索）。
    // 路径含中文，此处直接写 percent-encoded，避免 QUrl 编码歧义。
    //   完整中文路径：/docs/notes/v1.0.2更新内容.html 、 /docs/notes/启动器公告.html
    readonly property string changelogUrl: "https://shadowlauncher.cn/docs/notes/v1.0.2%E6%9B%B4%E6%96%B0%E5%86%85%E5%AE%B9.html"
    readonly property string announcementUrl: "https://shadowlauncher.cn/docs/notes/%E5%90%AF%E5%8A%A8%E5%99%A8%E5%85%AC%E5%91%8A.html"

    // 打开官网 v1.0.2 更新内容页（左下角版本号 / 蓝色启动 toast 共用）
    function openChangelogPage() {
        if (backend) backend.logUiMsg("打开官网 v1.0.2 更新内容页")
        Qt.openUrlExternally(appWindow.changelogUrl)
    }
    // 打开官网启动器公告页（橙黄色启动 toast 用）
    function openAnnouncementPage() {
        if (backend) backend.logUiMsg("打开官网启动器公告页")
        Qt.openUrlExternally(appWindow.announcementUrl)
    }

    // ── v1.0.2 启动 toast（2026-09-06：沿用 v1.0.1 结构）──
    // 一次性：用户点击对应 toast 后写标记，此后不再弹出；一直不点击则每次启动都弹。
    // 蓝色(info)=更新内容；橙黄色(warning)=启动器公告。
    function maybeShowStartupToasts() {
        if (!backend || !toastManager) return
        // 仅 v1.0.2 展示本次 toast；版本升级后自然停用（后续版本另行配置）
        if (backend.appVersion !== "v1.0.2") return
        // 本次是否属于"更新后首次启动"（新装/同版本重启为 false）
        var isUpdate = backend.consumeJustUpdatedFlag()
        var changelogDone = backend.readUiFlag("startup/changelog_v102_clicked")
        // 公告（黄 toast）标志沿用 v101：公告内容未更新，v1.0.1 已点过的用户 v1.0.2 不重复弹
        var announcementDone = backend.readUiFlag("startup/announcement_v101_clicked")
        // 蓝色 toast（更新内容）：仅更新用户弹出
        if (isUpdate && !changelogDone) {
            toastManager.showAction("您已成功更新至v1.0.2，点击查看v1.0.2更新内容。", function() {
                appWindow.openChangelogPage()
                if (backend) backend.writeUiFlag("startup/changelog_v102_clicked", true)
            }, "info")
        }
        // 黄色 toast（公告）：新装 + 更新都必须弹出
        if (!announcementDone) {
            toastManager.showAction("重要：《启动器进入“懒更新”策略通知和开发者的一些心里话》，请点击查看。", function() {
                appWindow.openAnnouncementPage()
                if (backend) backend.writeUiFlag("startup/announcement_v101_clicked", true)
            }, "warning")
        }
    }

    function navLabel(key) {
        switch (key) {
            case "home": return qsTr("启动")
            case "download": return qsTr("下载")
            case "multiplayer": return qsTr("联机")
            case "stats": return qsTr("统计")
            case "settings": return qsTr("设置")
            case "download_progress": return qsTr("下载进度")
            case "help_docs": return qsTr("帮助文档")
            case "useful_sites": return qsTr("实用网站")
            default: return key
        }
    }

    // 截图测试模式 / 外部调用：打开版本设置浮层并跳到指定分区（-1=保持概览）
    function openVersionSettingsSection(section) {
        showVersionSelect = false  // 与版本选择互斥，防透明背景叠加
        showVersionSettings = true
        if (versionSettingsLoader.item) {
            if (section >= 0) {
                versionSettingsLoader.item.currentNavIndex = section
            }
        } else if (section >= 0) {
            // Loader 尚未就绪：等 onLoaded 后重试（截图模式时序需要）
            var t = Qt.createQmlObject('import QtQuick; Timer { interval: 300; running: true; repeat: false; onTriggered: { if (versionSettingsLoader.item) versionSettingsLoader.item.currentNavIndex = ' + section + '; destroy() } }', appWindow)
        }
    }

    // Mod download error dialog state
    property bool showModDlError: false
    property var modDlErrorInfo: ({})

    Component.onCompleted: {
        // Dev mode: debug window available via F12, not auto-shown
        var comp = Qt.createComponent("DebugWindow.qml")
        if (comp.status === Component.Ready) {
            debugWindow = comp.createObject(appWindow)
            // NOTE: logMessage → DebugWindow disconnected to avoid QML main-thread
            // CPU churn from millions of per-chunk log emissions. DebugWindow still
            // exists for manual toggle (F12) but receives no auto-feed.
            // To re-enable: uncomment the line below.
            if (debugWindow) {
                // backend.logMessage.connect(function(msg) { debugWindow.info(msg) })
            }
        }
        console.log("[main] window completed, t=" + Date.now())
        if (backend) {
            backend.refreshInstalled()
            runningListModel = backend.runningGames()
            console.log("[main] init done, t=" + Date.now())
        }
        // v1.0.2 启动 toast（延迟触发，避免与窗口初始化竞争）
        startupToastTimer.start()
    }

    function addOfflineHistory(name) {
        var h = offlineHistory.slice()
        h = h.filter(function(x) { return x !== name })
        h.unshift(name)
        if (h.length > 10) h = h.slice(0, 10)
        offlineHistory = h
    }
    function switchPage(index) {
        if (navListIndex === 4 && index !== 4) _settingsFadeOut = true
        if (navListIndex === 1 && index !== 1) _dlFadeOut = true
        if (navListIndex === 5 && index !== 5) _installProgressFadeOut = true
        if (navListIndex === 7 && index !== 7) _usFadeOut = true
        navListIndex = index
        showVersionSelect = false
        showVersionSettings = false
        pageLoading = true
        loadTimer.restart()
        var pageKey = (navModel && index < navModel.count) ? navModel.get(index).pageKey : "unknown"
        console.info("[UI] 切换到 " + pageKey)
        if (backend) backend.logUiMsg(qsTr("进入页面 — ") + navLabel(pageKey))
    }

    Timer {
        id: loadTimer
        interval: 100
        onTriggered: pageLoading = false
    }

    // v1.0.2 启动 toast（2026-09-06：延迟触发，待窗口与后端就绪）
    Timer {
        id: startupToastTimer
        interval: 800
        running: false
        repeat: false
        onTriggered: appWindow.maybeShowStartupToasts()
    }

    // 自动检测游戏文件变化（每30秒）
    Timer {
        id: fileChangeTimer
        interval: 30000
        running: true
        repeat: true
        onTriggered: {
            if (backend) backend.checkFileChanges()
        }
    }

    // ═══ Download panel: replaced old nav-item management ═══
    // Floating FAB + side panel replaced the old full-page DownloadProgressPage
    // showModDownloadProgress kept as stub for external callers (ModDetailPage etc.)
    function showModDownloadProgress() {
        // Panel auto-shows — no explicit navigation needed
        console.log("[main] showModDownloadProgress: download panel handles visibility")
    }

    Connections {
        target: backend; enabled: backend !== null
        function onInstallingChanged() {
            console.log("[main] onInstallingChanged: installing=", backend.installing)
            // No explicit nav manipulation needed — navigateToProgress handles it
        }
        function onSelectedVersionClearedAfterDelete() {
            // Binding auto-updates — no explicit assignment needed
        }
        function onInstallFinished(success) {
            if (!success && toastManager) {
                // 整合包导入进行中：MC/加载器路失败≠整包失败（模组路可能仍在跑），
                // 失败提示统一由 importFinished 在整包全闭环后弹出。
                if (backend && backend.modpackImporter && backend.modpackImporter.busy) return
                toastManager.show("安装失败: 有文件下载失败或校验不通过", "", 5000)
            }
        }
        function onInstallComplete(installName) {
            console.log("[main] installComplete:", installName, "installing=", backend.installing)
            // 整合包导入：MC+加载器安装完成只是其中一路（模组仍在并行下载），
            // 成功/失败 Toast 由 importFinished 在「MC+加载器+全部模组」闭环后统一弹出。
            if (backend && backend.modpackImporter && backend.modpackImporter.busy) return
            if (toastManager) {
                toastManager.show(installName + " 下载完成")
            }
            // onInstallingChanged will handle nav hiding with a delay
        }
        // ── 全局 Toast（Forge/NeoForge 安装自动下载 Java 等场景，任意页面可见）──
        function onToastMessage(message) {
            if (toastManager) toastManager.show(message)
        }
        // ── 2026-08-15：更新包下载完成 → 常驻 toast → 确认框 → 退出重启安装 ──
        // toast 已简化为"整条可点"（无内部按钮组件），点击任意位置即触发
        function onUpdateReadyForRestart(version) {
            var ver = version || ""
            if (!toastManager) return
            toastManager.showAction("更新已就绪：" + ver + "，点击立即重启安装", function() {
                confirmDialogLoader.open(
                    qsTr("重启安装更新"),
                    qsTr("更新 %1 已下载完成，重启后将自动完成安装。\n若正在进行下载或安装任务，请等待完成后再重启。").arg(ver),
                    function() {
                        var busy = (backend && (backend.installing
                                                || backend.isResourceDownloading
                                                || (backend.modpackBusy && backend.modpackBusy())))
                        if (busy) {
                            if (toastManager) toastManager.show(qsTr("有下载任务正在进行，请等待完成后再重启"), "", 5000)
                            return
                        }
                        if (backend) backend.quitForUpdate()
                    })
            })
        }
        // ── 模组/光影/资源包文件下载完成/失败：成功/失败 Toast（卡片保留绿色/红色终态）──
        function onModFileDownloadFinished(dlId, success, filePath, displayName) {
            if (toastManager) {
                if (success) {
                    toastManager.show((displayName || "文件") + " 下载完成")
                } else {
                    toastManager.show((displayName || "文件") + " 下载失败", "", 5000)
                }
            }
        }
        function onModFileDownloadFailed(dlId, errorDetail, displayName) {
            if (toastManager) {
                toastManager.show((displayName || "文件") + " 下载失败: " + (errorDetail || "未知错误"), "", 5000)
            }
        }
        // ── 2026-08-15：主动取消下载 → "取消成功" toast（Mod/资源包/光影/数据包/整合包文件）──
        function onModFileDownloadCancelled(dlId, displayName) {
            if (toastManager) {
                toastManager.show((displayName || "文件") + " 取消成功")
            }
        }
        // ── 2026-08-15：MC/加载器安装取消 → toast（cancelNotification 此前无人连接）──
        function onCancelNotification(installId, message) {
            if (toastManager) toastManager.show(message)
        }
        function onResourceDownloadStateChanged() {
            console.log("[main] resourceDownloadStateChanged downloading=", backend ? backend.isResourceDownloading : false)
            // Download panel auto-shows — no nav manipulation needed
        }
        function onLaunchBlocked(reason) {
            if (toastManager) toastManager.show(reason)
        }
        // ── Auto-test: navigate to page + sub-tab ──
        function onNavigateToRequested(pageIndex, subTab) {
            console.log("[auto-test] navigateToRequested: page", pageIndex, "tab", subTab)
            if (pageIndex === 1) {
                pendingSubTab = subTab
                // Loader 可能已加载完成（onLoaded 已消费过 pendingSubTab=-1），
                // 此时直接设置 currentTab 触发 onCurrentTabChanged → 搜索
                var dlItem = downloadPageLoader.item
                if (dlItem && subTab >= 0) {
                    dlItem.currentTab = subTab
                    pendingSubTab = -1
                }
            } else if (pageIndex === 4) {
                // Settings page section (0=general 1=java 2=memory 3=experimental 4=about)
                var stItem = settingsPageLoader.item
                if (stItem && subTab >= 0) {
                    stItem.selectSection(subTab)
                } else if (subTab >= 0) {
                    // Loader item not ready yet — retry shortly
                    var timer = Qt.createQmlObject('import QtQuick; Timer { interval: 800; running: true; repeat: false; onTriggered: { if (settingsPageLoader.item) settingsPageLoader.item.selectSection(' + subTab + '); destroy() } }', appWindow)
                }
            } else if (pageIndex === 99) {
                // 截图测试：打开版本设置浮层到指定分区（-1=概览），不切换主页面
                openVersionSettingsSection(subTab)
                return
            }
            switchPage(pageIndex)
        }
        // ── Auto-test: open RP detail page ──
        function onOpenRpDetailRequested(slug) {
            console.log("[auto-test] openRpDetailRequested:", slug)
            switchPage(1)
            pendingSubTab = 3
            let timer = Qt.createQmlObject('import QtQuick; Timer { interval: 1500; running: true; repeat: false; onTriggered: { if (downloadPageLoader.item) { downloadPageLoader.item.rpDetailSlug = "' + slug + '"; downloadPageLoader.item.rpDetailTitle = "' + slug + '" } destroy() } }', appWindow)
        }
        function onOpenModDetailRequested(slug) {
            console.log("[auto-test] openModDetailRequested:", slug)
            switchPage(1)
            pendingSubTab = 1
            let timer = Qt.createQmlObject('import QtQuick; Timer { interval: 1500; running: true; repeat: false; onTriggered: { var item = downloadPageLoader.item; if (item) { item.modDetailSlug = "' + slug + '"; item.modDetailTitle = "' + slug + '"; item.modDetailPage.modDetailExpanded = []; item.modDetailPage.modDetailSelectedVer = ""; if (item.backend) item.backend.fetchModVersions(["' + slug + '"]) } destroy() } }', appWindow)
        }
        function onOpenShaderDetailRequested(slug) {
            console.log("[auto-test] openShaderDetailRequested:", slug)
            switchPage(1)
            pendingSubTab = 2
            let timer = Qt.createQmlObject('import QtQuick; Timer { interval: 1500; running: true; repeat: false; onTriggered: { var item = downloadPageLoader.item; if (item) { item.shaderDetailSlug = "' + slug + '"; item.shaderDetailTitle = "' + slug + '"; item.shaderDetailPage.shaderDetailExpanded = []; item.shaderDetailPage.shaderDetailSelectedVer = ""; if (item.backend) item.backend.fetchShaderVersions(["' + slug + '"]) } destroy() } }', appWindow)
        }
        // ── Auto-test: open modpack detail page ──
        function onOpenPackDetailRequested(slug) {
            console.log("[auto-test] openPackDetailRequested:", slug)
            switchPage(1)
            pendingSubTab = 4
            let timer = Qt.createQmlObject('import QtQuick; Timer { interval: 1500; running: true; repeat: false; onTriggered: { var item = downloadPageLoader.item; if (item) { item._packDetailSlug = "' + slug + '"; item._packDetailTitle = "' + slug + '"; item._packDetailDesc = ""; item._packDetailIcon = ""; item._packDetailSource = "Modrinth"; item._showPackDetail = true } destroy() } }', appWindow)
        }
        // ── Auto-test: toggle pre-release switch ──
        function onSetRpShowPreReleases(show) {
            console.log("[auto-test] setRpShowPreReleases:", show)
            switchPage(1); pendingSubTab = 3
            let timer = Qt.createQmlObject('import QtQuick; Timer { interval: 1500; running: true; repeat: false; onTriggered: { if (downloadPageLoader.item) { downloadPageLoader.item.rpShowPreReleases = ' + show + ' } destroy() } }', appWindow)
        }
        // ── Auto-test: open version dropdown ──
        function onOpenRpVersionMenu() {
            console.log("[auto-test] openRpVersionMenu")
            switchPage(1); pendingSubTab = 3
            let timer = Qt.createQmlObject('import QtQuick; Timer { interval: 2000; running: true; repeat: false; onTriggered: { if (downloadPageLoader.item) { downloadPageLoader.item.toggleVersionMenu() } destroy() } }', appWindow)
        }
        // ── Auto-test: expand detail group ──
        function onExpandRpDetailGroup(major) {
            console.log("[auto-test] expandRpDetailGroup:", major)
            let t = Qt.createQmlObject('import QtQuick; Timer { interval: 500; running: true; repeat: false; onTriggered: { if (downloadPageLoader.item) { downloadPageLoader.item.rpDetailExpanded = "' + major + '" } destroy() } }', appWindow)
        }
    }

    // ═══ 整合包导入完成：整包全闭环（MC+加载器+全部模组）后才弹成功/失败 Toast，
    //      并关闭常驻下载进度页（对齐普通资源下载完成后的页面表现）═══
    Connections {
        target: backend ? backend.modpackImporter : null
        enabled: target !== null
        function onImportFinished(success, versionName, error) {
            console.log("[main] modpack importFinished: success=", success, "name=", versionName, "error=", error)
            if (toastManager) {
                if (success) {
                    toastManager.show("整合包导入完成: " + (versionName || ""), 4000)
                } else if (error && error.indexOf("取消") >= 0) {
                    toastManager.show("整合包导入已取消", 3000)
                } else {
                    toastManager.show("整合包导入失败: " + (error || "未知错误"), 5000)
                }
            }
            // 任务结束后关闭常驻下载进度页：仅当用户仍停留在进度页时才自动返回启动页
            modpackDoneNavTimer.restart()
        }
    }

    Timer {
        id: modpackDoneNavTimer
        interval: 1800
        repeat: false
        onTriggered: {
            if (navListIndex === 5) switchPage(0)
        }
    }

    property bool hasCustomBg: {
        if (!backend) return false
        var p = backend.customBgPath
        return typeof p === "string" && p.length > 0
    }

    // ── Rounded window container ──
    // With a custom background the Qt clip cannot round the (transformed)
    // image corners on this platform, so the window becomes square while a
    // background is active; rounding returns when no background is set.
    Rectangle {
        anchors.fill: parent
        radius: hasCustomBg ? 0 : StyleTokens.radiusWindow
        color: hasCustomBg ? "transparent" : StyleTokens.bgPrimary
        clip: true

        // ── Custom background: sharp layer (crop transform) + blur layer ──
        // Corner rounding comes from the outer container Rectangle (radius +
        // clip), exactly as before the blur feature. The blur layer uses
        // PreserveAspectCrop (no transforms) so the container clip rounds it
        // correctly; the sharp layer keeps its original crop transform.
        Item {
            id: bgFrame
            anchors.fill: parent; z: -2
            visible: hasCustomBg
            readonly property real cropX: hasCustomBg && backend && typeof backend.cropX === "number" ? backend.cropX : 0.5
            readonly property real cropY: hasCustomBg && backend && typeof backend.cropY === "number" ? backend.cropY : 0.5
            readonly property real blurAmount: hasCustomBg && backend && typeof backend.backgroundBlur === "number" ? backend.backgroundBlur : 0.0

            // Sharp layer (original crop positioning)
            Image {
                id: bgImage
                source: hasCustomBg ? backend.customBgPath : ""
                fillMode: Image.PreserveAspectFit
                transformOrigin: Item.TopLeft
                cache: false; asynchronous: true

                readonly property real _s: Math.max(bgFrame.width / Math.max(implicitWidth, 1),
                                                     bgFrame.height / Math.max(implicitHeight, 1))
                readonly property real _dispW: implicitWidth * _s
                readonly property real _dispH: implicitHeight * _s
                readonly property real _overX: Math.max(0, _dispW - bgFrame.width)
                readonly property real _overY: Math.max(0, _dispH - bgFrame.height)

                scale: _s
                x: _overX > 0 ? -_overX * bgFrame.cropX : -_dispW * (bgFrame.cropX - 0.5)
                y: _overY > 0 ? -_overY * bgFrame.cropY : -_dispH * (bgFrame.cropY - 0.5)
            }

            // ── Blur layer: pure QML multi-resolution blend (no shader) ──
            Item {
                id: bgBlurLayer
                anchors.fill: parent
                visible: bgFrame.blurAmount > 0.001
                opacity: bgFrame.blurAmount
                Behavior on opacity { NumberAnimation { duration: 150 } }
                Image {
                    width: bgImage._dispW
                    height: bgImage._dispH
                    transformOrigin: Item.TopLeft
                    x: bgImage._overX > 0 ? -bgImage._overX * bgFrame.cropX : -bgImage._dispW * (bgFrame.cropX - 0.5)
                    y: bgImage._overY > 0 ? -bgImage._overY * bgFrame.cropY : -bgImage._dispH * (bgFrame.cropY - 0.5)
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    source: hasCustomBg ? backend.customBgPath : ""
                    sourceSize.width: Math.max(96, Math.round(bgFrame.width * 0.5))
                    opacity: 0.25
                }
                Image {
                    width: bgImage._dispW
                    height: bgImage._dispH
                    transformOrigin: Item.TopLeft
                    x: bgImage._overX > 0 ? -bgImage._overX * bgFrame.cropX : -bgImage._dispW * (bgFrame.cropX - 0.5)
                    y: bgImage._overY > 0 ? -bgImage._overY * bgFrame.cropY : -bgImage._dispH * (bgFrame.cropY - 0.5)
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    source: hasCustomBg ? backend.customBgPath : ""
                    sourceSize.width: Math.max(64, Math.round(bgFrame.width * 0.28))
                    opacity: 0.3
                }
                Image {
                    width: bgImage._dispW
                    height: bgImage._dispH
                    transformOrigin: Item.TopLeft
                    x: bgImage._overX > 0 ? -bgImage._overX * bgFrame.cropX : -bgImage._dispW * (bgFrame.cropX - 0.5)
                    y: bgImage._overY > 0 ? -bgImage._overY * bgFrame.cropY : -bgImage._dispH * (bgFrame.cropY - 0.5)
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    source: hasCustomBg ? backend.customBgPath : ""
                    sourceSize.width: Math.max(32, Math.round(bgFrame.width * 0.06))
                    opacity: 0.6
                }
            }
        }

        // ── Dark mask overlay ──
        Rectangle {
            anchors.fill: parent; z: -1
            color: StyleTokens.scrim
            opacity: hasCustomBg ? (1.0 - backend.contentOpacity) : 0
            visible: hasCustomBg
            Behavior on opacity { NumberAnimation { duration: 300 } }
        }

    // Window-wide top drag area — covers margins & gaps
    MouseArea {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 52
        property point clickPos: Qt.point(0, 0)
        onPressed: (mouse) => { clickPos = Qt.point(mouse.x, mouse.y) }
        onPositionChanged: (mouse) => {
            if (mouse.buttons & Qt.LeftButton) {
                appWindow.x += mouse.x - clickPos.x
                appWindow.y += mouse.y - clickPos.y
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent; spacing: 0

        // Spacer — buttons moved to floating right edge (same height as sidebar SHADOW)
        Item { Layout.fillWidth: true; height: 2 }

        // ── Loading bar (Android-style indeterminate) ──
        // FIX: fixed height 2px + opacity control → zero layout jitter
        // FIX: inset from rounded window corners (radius: StyleTokens.radiusWindow)
        Rectangle {
            id: loadingBar
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.preferredHeight: 2
            opacity: pageLoading ? 1 : 0
            color: "transparent"
            clip: true
            Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

            Rectangle {
                id: loadingSlider
                width: 100; height: 2; radius: StyleTokens.radiusSm
                color: StyleTokens.accentLight
                x: -100
                y: 0

                SequentialAnimation on x {
                    running: loadingBar.opacity > 0
                    loops: Animation.Infinite
                    NumberAnimation { from: -100; to: 100; duration: 600; easing.type: Easing.InOutCubic }
                    NumberAnimation { from: 100; to: appWindow.width + 100; duration: 400; easing.type: Easing.InCubic }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true
            Layout.margins: 8; spacing: 8

            Rectangle {
                id: sidebarContainer
                Layout.preferredWidth: 200; Layout.fillHeight: true
                layer.enabled: true
                visible: !appWindow.showInstallPage
                color: hasCustomBg ? "transparent" : StyleTokens.shadowColor; radius: StyleTokens.radiusMd
                opacity: hasCustomBg ? backend.sidebarOpacity : 1.0
                Behavior on opacity { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } }

                // Sidebar top drag area
                MouseArea {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: 44
                    property point clickPos: Qt.point(0, 0)
                    onPressed: (mouse) => { clickPos = Qt.point(mouse.x, mouse.y) }
                    onPositionChanged: (mouse) => {
                        if (mouse.buttons & Qt.LeftButton) {
                            appWindow.x += mouse.x - clickPos.x
                            appWindow.y += mouse.y - clickPos.y
                        }
                    }
                }

                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 8; spacing: 2
                    Text { id: shadowLogo; Layout.topMargin: 8; Layout.bottomMargin: 20; Layout.leftMargin: 16; text: "SHADOW"; font.pixelSize: StyleTokens.fontSizeLg; font.bold: true; color: StyleTokens.textSecondary }

                    // Navigation model
                    ListModel {
                        id: navModel
                        ListElement { label: "启动"; pageKey: "home"; icon: "home" }
                        ListElement { label: "下载"; pageKey: "download"; icon: "download" }
                        ListElement { label: "联机"; pageKey: "multiplayer"; icon: "globe" }
                        ListElement { label: "统计"; pageKey: "stats"; icon: "bar-chart-3" }
                        ListElement { label: "设置"; pageKey: "settings"; icon: "settings" }
                        ListElement { label: qsTr("下载进度"); pageKey: "download_progress"; icon: "download" }
                        ListElement { label: qsTr("帮助文档"); pageKey: "help_docs"; icon: "external-link" }
                        ListElement { label: qsTr("实用网站"); pageKey: "useful_sites"; icon: "compass" }
                    }

                    Repeater {
                        id: navRepeater
                        model: navModel
                        Item {
                            id: navItemDelegate
                            width: parent ? parent.width : 180; Layout.fillWidth: true; height: 44
                            // Expose window-position for fly ball animation
                            property var windowRoot: appWindow
                            scale: navMouse.containsMouse ? 1.03 : 1.0
                            Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                            Rectangle { anchors.fill: parent; color: navMouse.containsMouse ? StyleTokens.bgSecondary : "transparent"; Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } } }
                            Row {
                                anchors.left: parent.left; anchors.leftMargin: 20; anchors.verticalCenter: parent.verticalCenter
                                spacing: 8
                                Image {
                                    anchors.verticalCenter: parent.verticalCenter; width: 18; height: 18
                                    source: model.icon ? ("icons/lucide/" + model.icon + ".svg") : ""
                                    visible: model.icon !== undefined && model.icon !== ""
                                }
                                Text { text: appWindow.navLabel(model.pageKey); font.pixelSize: StyleTokens.fontSizeMd; color: navListIndex === index ? StyleTokens.textSecondary : StyleTokens.textSubtle }
                            }
                            MouseArea { id: navMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: {
                                if (model.pageKey === "help_docs") {
                                    // 帮助文档是链接项：直接开浏览器，不切换页面
                                    Qt.openUrlExternally("https://shadowlauncher.cn/docs/index.html")
                                    return
                                }
                                switchPage(index)
                            } }
                        }
                    }
                    Item { Layout.fillHeight: true }

                    // ═══ Running Games ═══
                    Text {
                        visible: backend ? backend.runningCount > 0 : false
                        Layout.leftMargin: 16; Layout.topMargin: 4
                        text: "运行中 (" + (backend ? backend.runningCount : 0) + ")"
                        font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.accentLight
                    }
                    Repeater {
                        id: runningList
                        model: appWindow.runningListModel
                        Item {
                            width: parent ? parent.width - 16 : 180; Layout.fillWidth: true; height: 32
                            Rectangle {
                                anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8
                                color: runningItemHover.containsMouse ? StyleTokens.accentSubtle : StyleTokens.bgPrimary
                                radius: StyleTokens.radiusSm
                                RowLayout {
                                    anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 6
                                    Text {
                                        text: modelData.displayVersion || modelData.version || "?"
                                        font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textSecondary
                                        elide: Text.ElideRight; Layout.fillWidth: true
                                    }
                                    Rectangle {
                                        width: 20; height: 20; radius: StyleTokens.radiusLg
                                        color: runningKillHover.containsMouse ? StyleTokens.errorLight : StyleTokens.textDanger
                                        scale: runningKillHover.containsMouse ? 1.15 : 1.0
                                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                                        Text { anchors.centerIn: parent; text: "\u2715"; font.pixelSize: StyleTokens.fontSizeXs; color: StyleTokens.textInverse }
                                        MouseArea {
                                            id: runningKillHover
                                            anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                            onClicked: { if (backend) backend.killGameByPid(modelData.pid) }
                                        }
                                    }
                                }
                            }
                            HoverHandler { id: runningItemHover }
                        }
                    }

                    // 版本号（2026-09-06：可点击 → 打开官网 v1.0.2 更新内容页）
                    Item {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: versionLabel.implicitWidth + 20
                        Layout.preferredHeight: 20
                        Text {
                            id: versionLabel
                            anchors.centerIn: parent
                            text: backend ? backend.appVersion : ""
                            font.pixelSize: StyleTokens.fontSizeXs
                            color: versionHover.containsMouse ? StyleTokens.accentLink : StyleTokens.bgHover
                        }
                        MouseArea {
                            id: versionHover
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (backend) appWindow.openChangelogPage()
                            }
                        }
                    }
                }

                // Animated selection indicator overlay
                Rectangle {
                    id: navIndicator
                    z: 10
                    x: 8
                    // 光条直接跟随当前菜单项的实际 y（含 SHADOW 标题真实高度与间距），
                    // 避免旧硬编码偏移（52/44）在菜单增多或间距变化后越偏越上
                    y: 8 + (navRepeater.count > navListIndex && navRepeater.itemAt(navListIndex)
                            ? navRepeater.itemAt(navListIndex).y
                            : 8 + shadowLogo.implicitHeight + 20 + 2 + navListIndex * (44 + 2))
                    width: 2; height: 44; color: StyleTokens.accentLight
                    Behavior on y { SmoothedAnimation { velocity: 200; duration: 300 } }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0

                // ── Right-side header (matched to SHADOW height, sidebar color) ──
                Rectangle {
                    id: headerBar
                    Layout.fillWidth: true; height: 44
                    visible: !appWindow.showInstallPage
                    color: "transparent"
                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 6
                        spacing: 0
                        Item { Layout.fillWidth: true }
                        ShadowIconButton { icon: "\u2014"; type: "normal"; onClicked: appWindow.showMinimized() }
                        Item { width: 6 }
                        ShadowIconButton { icon: "\u2715"; type: "close"; onClicked: appWindow.close() }
                    }
                }

                Rectangle {
                    id: pageContainer
                    Layout.fillWidth: true; Layout.fillHeight: true; color: "transparent"
                    layer.enabled: true; layer.smooth: true
                    clip: true
                    // Sequential: overlay fades first, then page fades in
                    // Note: Behavior only animates explicit assignments, not bindings
                    opacity: 1.0

                    PropertyAnimation { id: pageFadeInAnim; target: pageContainer; property: "opacity"; to: 1.0; duration: 500; easing.type: Easing.InOutCubic }

                    // ========== HOMEPAGE ==========
                    Loader {
                        id: homePageLoader
                        asynchronous: true
                        anchors.fill: parent
                        source: "HomePage.qml"
                        opacity: navListIndex === 0 && !showVersionSelect && !showVersionSettings ? 1 : 0
                        visible: opacity > 0
                        enabled: opacity >= 1
                        Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        // 每次进入「启动」页校验选中版本真实存在：被删除/不存在的版本自动切换，
                        // 避免用户对不存在版本误操作（快捷按钮/启动/版本设置入口）
                        onVisibleChanged: {
                            if (visible && backend && backend.ensureSelectedVersionValid) {
                                if (backend.ensureSelectedVersionValid()) {
                                    if (toastManager) toastManager.show(qsTr("所选版本已不存在，已自动切换到可用版本"))
                                }
                            }
                        }
                        onItemChanged: {
                            if (item) {
                                item.backend = backend
                                item.toastManager = toastManager
                                item.appWindow = appWindow
                                item.loginMode = loginMode
                                item.currentSelectedVersion = Qt.binding(function() { return currentSelectedVersion })
                                item.versionSelectRequested.connect(function() { showVersionSelect = true })
                                item.versionSettingsRequested.connect(function() { showVersionSettings = true })
                                item.loginModeChanged.connect(function(mode) { loginMode = mode })
                                // Microsoft login signal → HomePage UI (prevents stale "正在打开浏览器...")
                                var msLoginForm = item.msLoginForm
                                if (msLoginForm) {
                                    backend.microsoftLoginProgress.connect(function(step, detail) {
                                        msLoginForm.msStatusText = step + ": " + detail
                                    })
                                    backend.microsoftLoginSuccess.connect(function(username, uuid) {
                                        msLoginForm.msInProgress = false
                                        msLoginForm.msStatusText = ""
                                    })
                                    backend.microsoftLoginFailed.connect(function(error) {
                                        msLoginForm.msInProgress = false
                                        msLoginForm.msStatusText = ""
                                        if (toastManager) toastManager.show("登录失败: " + error, "", 5000)
                                    })
                                }
                            }
                        }
                    }

                    // ========== DOWNLOAD & MULTIPLAYER & SETTINGS ==========
                    // ========== MULTIPLAYER ==========
                    Rectangle { anchors.fill: parent; visible: navListIndex === 2; color: hasCustomBg ? "transparent" : StyleTokens.bgPrimary
                        Loader { asynchronous: true; anchors.fill: parent; active: true; source: "MultiplayerPage.qml"; onLoaded: { item.toastManager = toastManager } }
                    }

                    Rectangle { anchors.fill: parent; visible: navListIndex === 3; color: hasCustomBg ? "transparent" : StyleTokens.bgPrimary
                        opacity: navListIndex === 3 ? 1 : 0
                        Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
                        Loader { id: statsPageLoader; asynchronous: true; anchors.fill: parent; active: navListIndex === 3; source: "StatsPage.qml"
                            onLoaded: { if (item) { item.backend = backend; item.onVisible() } }
                        }
                    }


                    Rectangle { anchors.fill: parent; visible: navListIndex === 1 || _dlFadeOut; color: hasCustomBg ? "transparent" : StyleTokens.bgPrimary
                        opacity: navListIndex === 1 ? 1 : 0
                        Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
                        onOpacityChanged: { if (opacity === 0) _dlFadeOut = false }
                        Loader { id: downloadPageLoader; asynchronous: true; anchors.fill: parent; active: navListIndex === 1 || _dlFadeOut; source: "DownloadPage.qml"
                            onLoaded: {
                                item.mainWindow = appWindow
                                item.toastManager = toastManager
                                if (item.triggerDownloadBall) {
                                    item.triggerDownloadBall.connect(function(sx, sy) {
                                        appWindow.animateDownloadBall(sx, sy)
                                    })
                                }
                                // Apply pending sub-tab navigation from --navigate
                                if (pendingSubTab >= 0) {
                                    item.currentTab = pendingSubTab
                                    console.log("[auto-test] navigateTo: download tab", pendingSubTab)
                                    pendingSubTab = -1
                                }
                            }
                        } }
                    Rectangle { anchors.fill: parent; visible: navListIndex === 4 || _settingsFadeOut; color: hasCustomBg ? "transparent" : StyleTokens.bgPrimary
                        opacity: navListIndex === 4 ? 1 : 0
                        Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
                        onOpacityChanged: { if (opacity === 0) _settingsFadeOut = false }
                        Loader { id: settingsPageLoader; anchors.fill: parent; active: navListIndex === 4 || _settingsFadeOut; source: "SettingsPage.qml"
                            onLoaded: {
                                if (item)
                                    item._initLangModeIdx = backend.diagAutoLangComboIdx()
                            }
                        } }

                    // ── Install progress page ──
                    Rectangle {
                        anchors.fill: parent
                        visible: navListIndex === 5 || _installProgressFadeOut
                        color: hasCustomBg ? "transparent" : StyleTokens.bgPrimary

                        Rectangle {
                            anchors.fill: parent
                            color: hasCustomBg ? "transparent" : StyleTokens.bgPrimary
                            opacity: navListIndex === 5 ? 1 : 0
                            Behavior on opacity { NumberAnimation { duration: 150 } }
                            onOpacityChanged: { if (opacity === 0) _installProgressFadeOut = false }

                            Loader {
                                id: installProgressPageLoader
                                asynchronous: true
                                anchors.fill: parent
                                active: true  // 预加载避免切换卡顿
                                source: "InstallProgressPage.qml"
                                onLoaded: {
                                    item.mainWindow = appWindow
                                    item.backend = backend
                                }
                            }
                        }
                    }

                    // ── Useful websites page ──
                    Rectangle {
                        anchors.fill: parent
                        visible: navListIndex === 7 || _usFadeOut
                        color: hasCustomBg ? "transparent" : StyleTokens.bgPrimary

                        Rectangle {
                            anchors.fill: parent
                            color: hasCustomBg ? "transparent" : StyleTokens.bgPrimary
                            opacity: navListIndex === 7 ? 1 : 0
                            Behavior on opacity { NumberAnimation { duration: 150 } }
                            onOpacityChanged: { if (opacity === 0) _usFadeOut = false }

                            Loader {
                                id: usefulSitesPageLoader
                                asynchronous: true
                                anchors.fill: parent
                                active: navListIndex === 7 || _usFadeOut
                                source: "UsefulWebsitesPage.qml"
                                onLoaded: {
                                    item.backend = backend
                                    item.toastManager = toastManager
                                    item.appWindow = appWindow
                                }
                            }
                        }
                    }

                    // ========== VERSION SELECT OVERLAY ==========
                    Loader {
                        id: versionSelectLoader
                        asynchronous: true
                        anchors.fill: parent; z: 5
                        property bool _keepActive: false
                        active: showVersionSelect || _keepActive
                        source: active ? "VersionSelectOverlay.qml" : ""
                        opacity: showVersionSelect ? 1 : 0
                        visible: opacity > 0
                        Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
                        onLoaded: {
                            _keepActive = true
                            item.backend = backend
                            item.toastManager = toastManager
                            item.appWindow = appWindow
                            item.mainWindow = appWindow
                        }
                        onVisibleChanged: {
                            if (!visible && !showVersionSelect) {
                                _keepActive = false
                            }
                            if (item) {
                                item.visible = visible
                            }
                        }
                    }
                    // ========== VERSION SETTINGS OVERLAY ==========
                    Loader {
                        id: versionSettingsLoader
                        anchors.fill: parent; z: 5
                        property bool _keepActive: false
                        active: showVersionSettings || _keepActive
                        source: active ? "VersionSettingsOverlay.qml" : ""
                        opacity: showVersionSettings ? 1 : 0
                        visible: opacity > 0
                        Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
                        onLoaded: {
                            _keepActive = true
                            item.backend = backend
                            item.toastManager = toastManager
                        }
                        onVisibleChanged: {
                            if (!visible && !showVersionSettings) {
                                _keepActive = false
                            }
                            if (item) {
                                item.visible = visible
                            }
                        }
                    }

                    // ═══ Post-update changelog overlay (inside content area, doesn't block title bar) ═══
                    Item {
                        id: changelogOverlay
                        anchors.fill: parent
                        z: 10
                        visible: false

                        property bool _running: false

                        function show(version, notes) {
                            if (typeof backend !== "undefined" && backend) {
                                backend.logUiMsg("更新公告弹出 version=" + version + " 内容长度=" + notes.length)
                            }
                            versionText.text = qsTr("%1 更新").arg(version)
                            notesText.text = notes
                            visible = true
                            _running = true
                            showAnim.start()
                        }
                        function hide() {
                            if (typeof backend !== "undefined" && backend) {
                                backend.logUiMsg("更新公告关闭")
                            }
                            _running = false
                            hideAnim.start()
                        }

                        // Dimming background — blocks clicks from reaching UI behind
                        Rectangle {
                            id: dimBg
                            anchors.fill: parent
                            color: "#00000000"
                            opacity: 0

                            MouseArea {
                                anchors.fill: parent
                                // 仅拦截事件防止穿透，不关闭公告
                            }
                        }

                        // Card
                        Rectangle {
                            id: card
                            width: Math.min(580, parent.width - 48)
                            height: Math.min(500, parent.height - 80)
                            anchors.centerIn: parent
                            radius: StyleTokens.radiusXl
                            color: StyleTokens.bgSecondary
                            border { color: StyleTokens.bgInput; width: 1 }
                            scale: 0.85
                            opacity: 0

                            // ── Entry animation ──
                            ParallelAnimation {
                                id: showAnim
                                NumberAnimation { target: dimBg; property: "opacity"; to: 1; duration: 350; easing.type: Easing.OutCubic }
                                NumberAnimation { target: card; property: "scale"; from: 0.85; to: 1.0; duration: 500; easing.type: Easing.OutBack; easing.overshoot: 0.25 }
                                NumberAnimation { target: card; property: "opacity"; from: 0; to: 1; duration: 350; easing.type: Easing.OutCubic }
                            }

                            // ── Exit animation ──
                            SequentialAnimation {
                                id: hideAnim
                                ParallelAnimation {
                                    NumberAnimation { target: card; property: "scale"; to: 0.9; duration: 200; easing.type: Easing.InBack; easing.overshoot: 0.1 }
                                    NumberAnimation { target: card; property: "opacity"; to: 0; duration: 200; easing.type: Easing.InCubic }
                                    NumberAnimation { target: dimBg; property: "opacity"; to: 0; duration: 200; easing.type: Easing.InCubic }
                                }
                                ScriptAction { script: { changelogOverlay.visible = false } }
                            }

                            Column {
                                anchors.fill: parent
                                anchors.margins: 24
                                spacing: 16

                                // Title
                                Text {
                                    id: versionText
                                    font { pixelSize: 20; bold: true }
                                    color: StyleTokens.textPrimary
                                }

                                // Separator
                                Rectangle {
                                    width: parent.width
                                    height: 1
                                    color: StyleTokens.bgInput
                                }

                                // Scrollable Markdown content
                                Flickable {
                                    id: flick
                                    width: parent.width
                                    height: parent.height - 135
                                    clip: true
                                    contentHeight: notesText.implicitHeight
                                    boundsBehavior: Flickable.StopAtBounds

                                    ScrollBar.vertical: ScrollBar {
                                        policy: ScrollBar.AsNeeded
                                        contentItem: Rectangle {
                                            implicitWidth: 4
                                            radius: StyleTokens.radiusXs
                                            color: StyleTokens.textMuted
                                        }
                                    }

                                    Text {
                                        id: notesText
                                        width: parent.width - 8
                                        textFormat: Text.MarkdownText
                                        font.pixelSize: StyleTokens.fontSizeMd
                                        color: StyleTokens.textTertiary
                                        wrapMode: Text.Wrap
                                        onLinkActivated: function(link) { Qt.openUrlExternally(link) }
                                    }
                                }

                                // Bottom separator
                                Rectangle {
                                    width: parent.width
                                    height: 1
                                    color: StyleTokens.bgInput
                                }

                                // Close button — scale + color on hover/press
                                Rectangle {
                                    id: closeBtn
                                    width: 110
                                    height: 36
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    radius: StyleTokens.radiusMd
                                    scale: 1.0
                                    color: closeBtnMa.containsMouse ? (closeBtnMa.pressed ? StyleTokens.infoBg : StyleTokens.accentSubtle) : StyleTokens.infoBg

                                    Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
                                    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutBack; easing.overshoot: 0.2 } }

                                    Text {
                                        anchors.centerIn: parent
                                        text: qsTr("知道了")
                                        color: StyleTokens.textSecondary
                                        font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.Medium
                                    }

                                    MouseArea {
                                        id: closeBtnMa
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onPressed: closeBtn.scale = 0.92
                                        onReleased: closeBtn.scale = 1.0
                                        onClicked: changelogOverlay.hide()
                                    }
                                }
                            }
                        }
                    }


            }
        }
    }

    // ═══ Changelog signal listener (Window-level, always active) ═══
    Connections {
        target: typeof backend !== "undefined" ? backend : null
        function onUpdateChangelogAvailable(version, notes) {
            changelogOverlay.show(version, notes)
        }
    }

    // ════════════════════════════════════════════
    //  Install Page Overlay (top-level, covers entire window)
    // ════════════════════════════════════════════
    Rectangle {
        id: installPageOverlay
        anchors.fill: parent; color: hasCustomBg ? "transparent" : StyleTokens.bgPrimary; z: 21
        opacity: 0
        visible: true
        Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }

        Loader {
            id: installPageLoader
            anchors.fill: parent
            property bool _keepActive: false
            active: showInstallPage || _keepActive
            source: active ? "InstallPage.qml" : ""
            onLoaded: {
                _keepActive = true
                if (backend) backend.logMessage("[install] Loader onLoaded")
                if (item) {
                    item.backend = backend
                    item.mcVersion = installMcVersion
                    item.toastManager = toastManager
                    item.goBack.connect(function() { showInstallPage = false })
                    item.navigateToProgress.connect(function() {
                        showInstallPage = false
                        // 下一帧再跳转，避免与 overlay 关闭动画竞争导致卡顿
                        Qt.callLater(function() { navListIndex = 5 })
                    })
                    item.requestMinimize.connect(function() { appWindow.showMinimized() })
                    item.requestClose.connect(function() { appWindow.close() })
                }
            }
            Connections {
                target: appWindow
                function onShowInstallPageChanged() {
                    if (appWindow.showInstallPage) {
                        console.info("[UI] 打开 安装页 mcVersion=" + appWindow.installMcVersion)
                        overlayFadeInTimer.start()
                        pageContainer.opacity = 0
                        if (installPageLoader.item) {
                            installPageLoader.item.mcVersion = appWindow.installMcVersion
                        }
                    } else {
                        console.info("[UI] 关闭 安装页")
                        installPageOverlay.opacity = 0
                        pageFadeInTimer.start()
                        installUnloadTimer.start()
                    }
                }
            }
        }
    }

    Timer {
        id: overlayFadeInTimer
        interval: 120  // page dims first, then overlay fades in
        onTriggered: { installPageOverlay.opacity = 1.0 }
    }

    Timer {
        id: pageFadeInTimer
        interval: 150  // start brightening while overlay still fading
        onTriggered: { pageFadeInAnim.start() }
    }

    Timer {
        id: installUnloadTimer
        interval: 500
        repeat: false
        onTriggered: { if (!showInstallPage) installPageLoader._keepActive = false }
    }

    Loader {
        id: launchOverlayLoader; anchors.fill: parent; z: 20
        source: "LaunchOverlay.qml"
        active: true  // Always loaded for smooth hide animation
        onLoaded: {
            if (item) {
                item.backend = backend
                item.toastManager = toastManager
                }
        }
    }

    // ════════════════════════════════════════════
    //  Download animation — flying ball ═══
    // ════════════════════════════════════════════
    Rectangle {
        id: flyBall
        z: 500
        width: 12; height: 12; radius: StyleTokens.radiusMd
        color: StyleTokens.accentLight
        visible: false
        opacity: 0
    }

    ParallelAnimation {
        id: flyBallAnim
        property real startX: 0
        property real startY: 0
        property real endX: 0
        property real endY: 0
        NumberAnimation { target: flyBall; property: "x"; from: flyBallAnim.startX; to: flyBallAnim.endX; duration: 400; easing.type: Easing.InCubic }
        NumberAnimation { target: flyBall; property: "y"; from: flyBallAnim.startY; to: flyBallAnim.endY; duration: 400; easing.type: Easing.InCubic }
        onStopped: {
            // When fly reaches target, bounce then fade
            flyBallBounce.restart()
            flyBallFade.start()
        }
    }

    SequentialAnimation {
        id: flyBallBounce
        NumberAnimation { target: flyBall; property: "scale"; from: 1.0; to: 1.7; duration: 140; easing.type: Easing.OutBack }
        NumberAnimation { target: flyBall; property: "scale"; from: 1.7; to: 0.8; duration: 100; easing.type: Easing.InCubic }
        NumberAnimation { target: flyBall; property: "scale"; from: 0.8; to: 1.0; duration: 120; easing.type: Easing.OutCubic }
    }

    NumberAnimation {
        id: flyBallFade
        target: flyBall; property: "opacity"; to: 0; duration: 350; easing.type: Easing.OutCubic
        onStopped: { flyBall.visible = false }
    }

    // Nav item bounce overlay (inside sidebar)
    Rectangle {
        id: navBounceOverlay
        z: 200
        visible: false
        width: 184; height: 40; radius: StyleTokens.radiusMd
        color: StyleTokens.accentLight
        x: 8; y: 0
        opacity: 0
        
        NumberAnimation on opacity {
            id: navOverlayFade
            from: 0.3; to: 0; duration: 420; easing.type: Easing.OutCubic
            onStopped: { navBounceOverlay.visible = false }
        }
        NumberAnimation on scale {
            id: navOverlayScale
            from: 0.9; to: 1.06; duration: 260; easing.type: Easing.OutBack
        }
    }

    function animateDownloadBall(sourceX, sourceY) {
        // Target the FAB button at bottom-right
        var targetX = parent.width - 16 - 22  // rightMargin 16 + half width 22
        var targetY = parent.height - 16 - 22  // bottomMargin 16 + half height 22

        // Diagnostic: write trace to file
        if (backend) backend.logMessage("[flyBall] (" + sourceX.toFixed(0) + "," + sourceY.toFixed(0) + ") → (" + targetX.toFixed(0) + "," + targetY.toFixed(0) + ")")

        // Position and show ball
        flyBall.x = sourceX
        flyBall.y = sourceY
        flyBall.opacity = 1.0
        flyBall.scale = 1.0
        flyBall.visible = true

        // Set anim targets and fly
        flyBallAnim.startX = sourceX
        flyBallAnim.startY = sourceY
        flyBallAnim.endX = targetX
        flyBallAnim.endY = targetY
        flyBallAnim.restart()

        // FAB overlay bounce indicator
        if (downloadFab) {
            downloadFab.scale = 1.2
            fabBounceBack.restart()
        }
    }

    Connections {
        target: backend; enabled: backend !== null
        function onLogMessage(msg) { console.log("[backend]", msg) }
        function onRunningCountChanged() {
            appWindow.runningListModel = backend ? backend.runningGames() : []
            console.log("[main] runningCountChanged → list refreshed: " + appWindow.runningListModel.length + " games")
        }
        function onCrashDetected(report) {
            console.log("[crash] crashDetected signal received:", JSON.stringify(report))
            crashDialogLoader.active = true
            if (crashDialogLoader.item) {
                crashDialogLoader.item.crashData = report
            } else {
                // Loader 异步未就绪：缓存结果，onItemChanged 里补设
                _pendingCrashResult = report
            }
        }
        // ── 崩溃分析 v2：启动失败 → Toast 提示 → 弹窗进入分析态 → 结果态 ──
        function onCrashAnalysisStarted() {
            console.log("[crash] analysis started")
            if (toastManager) toastManager.show(qsTr("启动失败，正在分析日志信息…"), 3500)
            // Loader 异步加载：item 可能还没创建好，先设挂起标志，
            // onItemChanged 里补调用 beginAnalyzing（否则第一次只有 Toast 没窗口）
            _pendingCrashAnalyze = true
            crashDialogLoader.active = true
            if (crashDialogLoader.item) {
                _pendingCrashAnalyze = false
                crashDialogLoader.item.beginAnalyzing()
            }
        }
        function onCrashAnalysisReady(report) {
            console.log("[crash] analysis ready:", JSON.stringify(report))
            crashDialogLoader.active = true
            if (crashDialogLoader.item) {
                crashDialogLoader.item.crashData = report
            } else {
                // Loader 未就绪：缓存结果，onItemChanged 里补设
                _pendingCrashResult = report
            }
        }
    }

    // Confirm Dialog (常驻加载 — 2026-08-15: 原 active:false+asynchronous 异步
    // 懒加载需要 _pendingTitle 挂起机制，但该属性位于嵌套块不在根 Window 上，
    // open() 访问报 "Cannot assign to non-existent property"。改为常驻加载后
    // item 立即可用，open() 只操作 Loader 自身属性，不再依赖外部属性)
    Loader {
        id: confirmDialogLoader
        active: true
        anchors.fill: parent; z: 399
        source: "ConfirmDialog.qml"

        // Proxy for backward compatibility — external files use confirmDialog.xxx
        function open(title, message, onAccept) {
            if (!item) return
            item.title = title
            item.message = message
            item.onAccept = onAccept
            item.opened = true
        }
        function close() {
            if (item) item.opened = false
        }
    }

    // Compatibility object — other files still access confirmDialog.xxx
    property QtObject confirmDialog: QtObject {
        property string title: ""
        property string message: ""
        property var onAccept: null
        property bool visible: false
        onVisibleChanged: {
            if (visible) {
                confirmDialogLoader.open(title, message, onAccept)
            } else {
                confirmDialogLoader.close()
            }
        }
    }
    }

    // Mod download error dialog
    Rectangle {
        id: modDlErrorDialog; z: 400
        anchors.centerIn: parent; width: 400; height: 200; radius: StyleTokens.radiusLg
        color: StyleTokens.surfaceOverlay; border.color: StyleTokens.errorBg; border.width: 1
        visible: showModDlError
        Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
        ColumnLayout {
            anchors.fill: parent; anchors.margins: 20; spacing: 12
            Text { text: "[警告] 下载失败"; font.pixelSize: StyleTokens.fontSizeLg; font.bold: true; color: StyleTokens.errorLight }
            Text { text: modDlErrorInfo.displayName || ""; font.pixelSize: StyleTokens.fontSizeMd; color: StyleTokens.textSecondary }
            Text {
                Layout.fillWidth: true
                text: modDlErrorInfo.errorDetail || "未知错误"
                color: StyleTokens.errorLight; font.pixelSize: StyleTokens.fontSizeSm; wrapMode: Text.WordWrap
            }
            RowLayout {
                spacing: 10; Layout.alignment: Qt.AlignRight
                Rectangle {
                    width: 80; height: 30; radius: StyleTokens.radiusMd
                    color: skipHov.hovered ? StyleTokens.errorBg : StyleTokens.bgSecondary
                    border.color: skipHov.hovered ? StyleTokens.statusOff : StyleTokens.errorBg
                    Text { anchors.centerIn: parent; text: "跳过"; color: StyleTokens.textDanger; font.pixelSize: StyleTokens.fontSizeSm }
                    MouseArea { id: skipHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: { backend.cancelModFileDownload(modDlErrorInfo.dlId || 0); showModDlError = false }
                    }
                }
                Rectangle {
                    width: 80; height: 30; radius: StyleTokens.radiusMd
                    color: retryHov.hovered ? StyleTokens.warningBg : StyleTokens.bgSecondary
                    border.color: retryHov.hovered ? StyleTokens.warning : StyleTokens.warningBg
                    Text { anchors.centerIn: parent; text: "重试"; color: StyleTokens.warning; font.pixelSize: StyleTokens.fontSizeSm }
                    MouseArea { id: retryHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: { backend.retryModFileDownload(modDlErrorInfo.dlId || 0); showModDlError = false }
                    }
                }
            }
        }
    }
    Rectangle {
        anchors.fill: parent; z: 399; color: StyleTokens.scrim
        opacity: showModDlError ? 0.5 : 0
        visible: showModDlError
        Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
        MouseArea { anchors.fill: parent; onClicked: { showModDlError = false } }
    }

    // Crash detection dialog (lazy-loaded)
    Loader {
        id: crashDialogLoader; asynchronous: true; active: false
        anchors.fill: parent; z: 500
        source: "CrashDialog.qml"
        onItemChanged: {
            if (item) {
                item.backend = backend
                item.toastManager = toastManager
                item.exportDialogRef = crashLogExportDialog
                // 补消费挂起的分析请求（第一次启动失败时 Loader 未就绪）
                if (_pendingCrashAnalyze) {
                    _pendingCrashAnalyze = false
                    item.beginAnalyzing()
                }
                if (_pendingCrashResult) {
                    var report = _pendingCrashResult
                    _pendingCrashResult = null
                    item.crashData = report
                }
            }
        }
    }

    // 崩溃日志导出对话框（放 Window 顶层——FileDialog 声明在 Popup 内部
    // 会导致 Qt6Core.dll 崩溃 0xc0000005，必须挂在普通窗口上下文）
    FileDialog {
        id: crashLogExportDialog
        title: "导出崩溃日志为 ZIP"
        fileMode: FileDialog.SaveFile
        nameFilters: ["ZIP 文件 (*.zip)"]
        defaultSuffix: "zip"
        // currentFile 由 CrashDialog 点击导出时动态设置（带时间戳）
        onAccepted: {
            var dl = crashDialogLoader.item
            if (!dl || !dl.backend) return
            var sel = crashLogExportDialog.selectedFile
            var path = ""
            if (typeof sel === "string") {
                path = sel
            } else if (sel && typeof sel.toString === "function") {
                path = sel.toString()
            }
            if (path.indexOf("file:///") === 0) path = path.substring(8)
            if (!path) return
            if (!/\.zip$/i.test(path)) path += ".zip"
            var result = dl.backend.exportCrashLogs(path)
            if (result && dl.toastManager) dl.toastManager.show("日志已导出到: " + result, 5000)
        }
    }


    // ═══ Toast notification system ═══
    ToastManager {
        id: toastManager
        anchors.fill: parent
        z: 9999
    }

    // ═══ Agreement overlay (shown on first launch / when agreements updated) ═══
    Loader {
        id: agreementLoader
        anchors.fill: parent
        z: 10000
        active: backend ? !backend.agreementAccepted : false
        source: "AgreementOverlay.qml"
        asynchronous: false
    }

    // ═══ FAB 下载按钮 (右下角浮动圆形) ═══
    Rectangle {
        id: downloadFab
        z: 50
        width: 44; height: 44; radius: 22
        color: fabMouse.containsMouse ? StyleTokens.bgHover : StyleTokens.accentSubtle
        border.color: fabMouse.containsMouse ? StyleTokens.borderLight : StyleTokens.infoBg
        anchors.right: parent.right; anchors.bottom: parent.bottom
        anchors.rightMargin: 16; anchors.bottomMargin: 16
        // ── Fade in/out: opacity drives animation, visible hides render tree ──
        readonly property bool _hasDownloads: backend && backend.installCardsModel && backend.installCardsModel.count > 0
        opacity: _hasDownloads ? 1 : 0
        scale: 0.8 + opacity * 0.2  // scale 0.8→1.0 as opacity goes 0→1
        visible: opacity > 0.01  // keep in render tree during fade-out
        enabled: opacity > 0.5

        Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
        Behavior on scale { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
        Behavior on color { ColorAnimation { duration: 150 } }
        Behavior on border.color { ColorAnimation { duration: 150 } }

        // 下载图标 (Lucide)
        Image {
            anchors.centerIn: parent
            source: "icons/lucide/download.svg"
            width: 20; height: 20
            sourceSize.width: 20; sourceSize.height: 20
        }

        // 数量徽章 (仅 >1 时显示)
        Rectangle {
            anchors.top: parent.top; anchors.right: parent.right
            anchors.topMargin: -4; anchors.rightMargin: -4
            width: 20; height: 20; radius: 10
            color: StyleTokens.accent
            visible: backend && backend.installCardsModel && backend.installCardsModel.count > 0
            Text {
                anchors.centerIn: parent
                text: backend ? (backend.installCardsModel ? backend.installCardsModel.count : "") : ""
                font.pixelSize: 11; color: StyleTokens.textInverse
            }
        }

        MouseArea {
            id: fabMouse
            anchors.fill: parent; hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                downloadPanel.expanded = !downloadPanel.expanded
                // Panel visibility is auto-managed by _cardCount > 0 — never set .visible directly
            }
        }

        // FAB 弹跳回弹动画 (由 animateDownloadBall 触发)
        NumberAnimation {
            id: fabBounceBack
            target: downloadFab; property: "scale"; to: 1.0
            duration: 300; easing.type: Easing.OutBack; easing.overshoot: 2.0
        }

        // FAB 脉冲循环动画 (面板折叠时持续吸引注意力)
        SequentialAnimation {
            id: fabPulse
            loops: Animation.Infinite
            running: downloadFab._hasDownloads && !downloadPanel.expanded
            NumberAnimation { target: downloadFab; property: "scale"; to: 1.08; duration: 800; easing.type: Easing.InOutSine }
            NumberAnimation { target: downloadFab; property: "scale"; to: 1.0; duration: 800; easing.type: Easing.InOutSine }
            PauseAnimation { duration: 2000 }
        }
    }

    // ═══ 下载队列面板 ═══
    DownloadQueuePanel {
        id: downloadPanel
        backendRef: backend
        // visible 由面板内部的 _cardCount > 0 控制
    }

    } // ── end rounded Rectangle

    // ═══ Modpack import overlay ═══
    ModpackImportOverlay {
        id: modpackImportOverlayItem
        anchors.fill: parent
        z: 300
        toastManager: toastManager
        appWindow: appWindow
        visible: false
    }
    property QtObject modpackImportOverlay: modpackImportOverlayItem

    // ═══ 全局拖放导入路由（仿整合包导入思路，扩展支持 Mod / 资源包）═══
    // 版本设置浮层打开且停在 Mod管理/资源包管理 分区时：
    //   .jar → importMod；.zip → importResourcePack
    // 其余情况：.zip/.mrpack → 整合包导入（原有行为不变）
    DropArea {
        id: packDropArea
        anchors.fill: parent
        z: 301   // 弹窗层之上；DropArea 不拦截鼠标点击，仅响应拖放
        // 导入窗口打开时禁用：窗口内部有自己的 dropArea（显示文件名/选择），
        // 全局层若仍拦截会把拖拽"吃掉"，用户无法拖入窗口。
        enabled: !modpackImportOverlayItem.visible

        // 当前版本设置浮层所在分区（-1=未打开/未加载）
        function settingsSection() {
            if (!versionSettingsLoader.visible || !versionSettingsLoader.item) return -1
            return versionSettingsLoader.item.currentNavIndex
        }
        function localPath(url) {
            var p = url.toString()
            if (p.startsWith("file:///")) p = p.substring(8)
            return p
        }

        onEntered: function(drag) {
            if (!drag.hasUrls || drag.urls.length === 0) return
            var p = localPath(drag.urls[0])
            var sec = settingsSection()
            if (/\.jar$/i.test(p) && sec === 3) {
                drag.accept(Qt.CopyAction)
                packDropHintTitle = qsTr("松开以导入 Mod")
                packDropHintSub = qsTr("将复制到当前版本的 mods 文件夹")
                packDropHint.visible = true
            } else if (/\.zip$/i.test(p) && sec === 4) {
                drag.accept(Qt.CopyAction)
                packDropHintTitle = qsTr("松开以导入资源包")
                packDropHintSub = qsTr("将复制到当前版本的 resourcepacks 文件夹")
                packDropHint.visible = true
            } else if (/\.(zip|mrpack)$/i.test(p)) {
                drag.accept(Qt.CopyAction)
                packDropHintTitle = qsTr("松开以导入整合包")
                packDropHintSub = qsTr("支持 .zip（CurseForge）与 .mrpack（Modrinth）")
                packDropHint.visible = true
            }
        }
        onExited: packDropHint.visible = false
        onDropped: function(drop) {
            packDropHint.visible = false
            if (!drop.hasUrls || drop.urls.length === 0) return
            var path = localPath(drop.urls[0])
            var sec = settingsSection()

            // Mod 拖入导入（版本设置-Mod管理）
            if (/\.jar$/i.test(path) && sec === 3) {
                if (!backend) { if (toastManager) toastManager.show(qsTr("后端未就绪")); return }
                if (!currentSelectedVersion) { if (toastManager) toastManager.show(qsTr("请先选择一个版本")); return }
                if (backend.importMod(path, currentSelectedVersion)) {
                    if (toastManager) toastManager.show(qsTr("已导入 Mod: %1").arg(path.split("/").pop()))
                } else {
                    if (toastManager) toastManager.show(qsTr("Mod 导入失败: %1").arg(path.split("/").pop()))
                }
                if (versionSettingsLoader.item) versionSettingsLoader.item.refreshModsUi()
                return
            }

            // 资源包拖入导入（版本设置-资源包管理）
            if (/\.zip$/i.test(path) && sec === 4) {
                if (!backend) { if (toastManager) toastManager.show(qsTr("后端未就绪")); return }
                if (!currentSelectedVersion) { if (toastManager) toastManager.show(qsTr("请先选择一个版本")); return }
                if (backend.importResourcePack(path, currentSelectedVersion)) {
                    if (toastManager) toastManager.show(qsTr("已导入资源包: %1").arg(path.split("/").pop()))
                } else {
                    if (toastManager) toastManager.show(qsTr("资源包导入失败: %1").arg(path.split("/").pop()))
                }
                if (versionSettingsLoader.item) versionSettingsLoader.item.refreshRpsUi()
                return
            }

            // 整合包导入（原有全局行为）
            if (!/\.(zip|mrpack)$/i.test(path)) {
                if (toastManager) toastManager.show(qsTr("不支持的文件格式，请拖入 .jar（Mod）、.zip（资源包/整合包）或 .mrpack（整合包）"))
                return
            }
            if (!backend || !backend.modpackImporter) {
                if (toastManager) toastManager.show(qsTr("后端未就绪"))
                return
            }
            // 单任务限制：同一时间只允许一个整合包任务（下载或导入）
            if (backend.modpackBusy()) {
                if (toastManager) toastManager.show(qsTr("已有整合包任务（下载或导入）进行中，请等待完成"), 5000)
                return
            }
            // 标准导入流程：格式识别与导入执行全在后端（ModpackImporter）
            backend.modpackImporter.startImport(path)
            switchPage(5)   // 下载进度页（与弹窗导入同一路由）
            if (toastManager) toastManager.show(qsTr("开始导入整合包: %1").arg(path.split("/").pop()))
        }
    }

    // 拖放悬停提示层
    Rectangle {
        id: packDropHint
        anchors.fill: parent
        z: 302
        visible: false
        color: "#99000000"

        Rectangle {
            anchors.centerIn: parent
            width: 360
            height: 170
            radius: StyleTokens.radiusXl
            color: StyleTokens.surfaceOverlay
            border.color: StyleTokens.accent
            border.width: 2

            Column {
                anchors.centerIn: parent
                spacing: 12

                Image {
                    anchors.horizontalCenter: parent.horizontalCenter
                    source: "icons/lucide/package.svg"
                    width: 40; height: 40
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: packDropHintTitle
                    font.pixelSize: StyleTokens.fontSizeLg
                    font.bold: true
                    color: StyleTokens.textPrimary
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: packDropHintSub
                    font.pixelSize: StyleTokens.fontSizeXs
                    color: StyleTokens.textMuted
                }
            }
        }
    }

}
