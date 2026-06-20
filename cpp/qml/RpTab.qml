import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

Item {
    id: rpTab
    anchors.fill: parent
    property var backend
    property var mainWindow: null
    signal triggerDownloadBall(real sourceX, real sourceY)
    property string rpGameVersion: ""
    property string rpDownloadDir: ""
    property bool rpResultsReady: false
    property real rpLoadingProgress: 0
    property int rpDebugSeq: 0
    property string rpCategoryFilter: ""
    property string rpFeatureFilter: ""
    property string rpResolutionFilter: ""
    property var rpFeatureMap: ({})
    property int rpPage: 0
    property bool rpLoadingMore: false
    property bool rpShowPreReleases: false
    property int rpVersionCacheVersion: 0
    property bool rpHasMore: true
    property int rpTotalHits: 0
    property string complianceNotice: ""


        ColumnLayout {
            spacing: 8

            // ── Filter Card ──
            Rectangle {
                Layout.fillWidth: true; height: 150; radius: 10
                color: "#11141c"; border.color: "#1e2230"

                ColumnLayout {

                    // Row 1: 名称 + 来源
                    RowLayout {
                        Layout.fillWidth: true; spacing: 10

                        Text { text: "名称"; color: "#9094a8"; font.pixelSize: 11; Layout.preferredWidth: 32 }

                        Rectangle {
                            Layout.fillWidth: true; height: 28; radius: 5
                            color: "#0c0e14"
                            border.color: rpSearchInput.activeFocus ? "#5068c8" : "#2a3040"
                            border.width: 1

                            TextInput {
                                id: rpSearchInput
                                color: "#d0d4e0"; verticalAlignment: TextInput.AlignVCenter; font.pixelSize: 12
                                // REMOVED onAccepted trigger — search only on button click (Fix 1)

                                Text {
                                    text: "输入资源包名称..."; color: "#505468"; font.pixelSize: 11
                                    visible: !rpSearchInput.text
                                }
                            }
                        }

                        Text { text: "来源"; color: "#9094a8"; font.pixelSize: 11; Layout.preferredWidth: 32 }

                        Rectangle {
                            id: rpSourcePill
                            Layout.preferredWidth: 140; height: 28; radius: 5
                            color: rpSrcHov.hovered ? "#1a2848" : "#0c0e14"
                            border.color: "#2a3040"; border.width: 1

                            RowLayout {
                                Text {
                                    Layout.fillWidth: true
                                    text: "Modrinth (MCIM)"; color: "#788db0"; font.pixelSize: 11
                                    elide: Text.ElideRight
                                }
                                Text { text: "▾"; color: "#505468"; font.pixelSize: 8 }
                            }
                            MouseArea {
                                id: rpSrcHov; anchors.fill: parent
                                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { if (rpSourceMenu.visible) rpSourceMenu.close(); else rpSourceMenu.open() }
                            }

                            Popup {
                                id: rpSourceMenu; closePolicy: Popup.NoAutoClose
                                y: parent.height + 4; width: 160
                                padding: 4
                                background: Rectangle { color: "#151922"; radius: 8; border.color: "#1e2230" }
                                ColumnLayout {
                                    width: parent.width - 8; spacing: 2
                                    Repeater {
                                        model: [
                                            { value: "modrinth", label: "Modrinth (MCIM镜像)" },
                                            { value: "modrinth-direct", label: "Modrinth (直连)" }
                                        ]
                                        Rectangle {
                                            Layout.fillWidth: true; height: 26; radius: 4
                                            color: mouse2.hovered ? "#1a2848" : "transparent"
                                            Text {
                                                color: "#9094a8"; font.pixelSize: 11
                                            }
                                            MouseArea {
                                                id: mouse2; anchors.fill: parent
                                                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    rpSourceMenu.close()
                                                    // FIX 4: Only MCIM source is currently operational.
                                                    // modrinth-direct is non-functional — no action on click.
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Row 2: 版本 + 类型
                    RowLayout {
                        Layout.fillWidth: true; spacing: 10

                        Text { text: "版本"; color: "#9094a8"; font.pixelSize: 11; Layout.preferredWidth: 32 }

                        Rectangle {
                            id: rpVerPill
                            Layout.preferredWidth: 120; height: 28; radius: 5
                            color: rpVerHov.hovered ? "#1a2848" : "#0c0e14"
                            border.color: root.rpGameVersion ? "#5068c8" : "#2a3040"; border.width: 1

                            RowLayout {
                                Text {
                                    Layout.fillWidth: true
                                    text: root.rpGameVersion ? ("MC " + root.rpGameVersion) : "全部"
                                    color: root.rpGameVersion ? "#8aaeff" : "#788090"; font.pixelSize: 11
                                    elide: Text.ElideRight
                                }
                                Text { text: "▾"; color: "#505468"; font.pixelSize: 8 }
                            }
                            MouseArea {
                                id: rpVerHov; anchors.fill: parent
                                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { if (rpVersionMenu.visible) { rpVersionMenu.close() } else { rpVersionMenu.open() } }
                            }

                            Popup {
                                id: rpVersionMenu; closePolicy: Popup.NoAutoClose
                                y: parent.height + 4; width: 140
                                height: Math.min(rpVerFlick.contentHeight + 8, 220)
                                padding: 0
                                background: Rectangle { color: "#151922"; radius: 8; border.color: "#1e2230" }

                                Flickable {
                                    id: rpVerFlick
                                    contentHeight: rpVerInner.implicitHeight; clip: true
                                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                    ColumnLayout {
                                        id: rpVerInner
                                        width: parent.width; spacing: 2

                                        Rectangle {
                                            Layout.fillWidth: true; height: 26; radius: 4
                                            color: !root.rpGameVersion ? "#1a2848" : "transparent"
                                            Text {
                                                color: !root.rpGameVersion ? "#5068c8" : "#9094a8"; font.pixelSize: 11
                                                font.weight: !root.rpGameVersion ? Font.Bold : Font.Normal
                                            }
                                            MouseArea {
                                                // FIX 1: Removed loadRpFirstPage() — selection only, search on button click
                                                onClicked: { root.rpGameVersion = ""; rpVersionMenu.close() }
                                            }
                                        }

                                        Repeater {
                                            model: {
                                                if (!backend || !backend.versionIds) return ["1.21.10","1.20.6"]
                                                var seen = new Set()
                                                var groups = []
                                                for (var i = 0; i < backend.versionIds.length; i++) {
                                                    var v = backend.versionIds[i]
                                                    if (!root.rpShowPreReleases && !/^[0-9.]+$/.test(v)) continue
                                                    var major = v.split(/[.\-]/).slice(0, 2).join(".")
                                                    if (!seen.has(major)) {
                                                        seen.add(major)
                                                        groups.push(major)
                                                    }
                                                    if (groups.length >= 30) break
                                                }
                                                return groups
                                            }
                                            Rectangle {
                                                Layout.fillWidth: true; height: 26; radius: 4
                                                color: modelData === root.rpGameVersion ? "#1a2848" : "transparent"
                                                Text {
                                                    color: modelData === root.rpGameVersion ? "#5068c8" : "#9094a8"; font.pixelSize: 11
                                                    font.weight: modelData === root.rpGameVersion ? Font.Bold : Font.Normal
                                                }
                                                MouseArea {
                                                    // FIX 1: Removed loadRpFirstPage() — selection only, search on button click
                                                    onClicked: { root.rpGameVersion = modelData; rpVersionMenu.close() }
                                                }
                                            }
                                        }

                                    }
                                }
                            }
                        }

                        Text { text: "筛选"; color: "#9094a8"; font.pixelSize: 11; Layout.preferredWidth: 32 }

                        // Three filter dropdowns: Category | Feature | Resolution
                        RowLayout {
                            Layout.fillWidth: true; spacing: 6

                            // ── Category dropdown ──
                            Rectangle {
                                id: rpCatPill
                                Layout.preferredWidth: 95; height: 28; radius: 5
                                color: rpCatHov.hovered ? "#1a2848" : "#0c0e14"
                                border.color: root.rpCategoryFilter ? "#5068c8" : "#2a3040"; border.width: 1
                                RowLayout {
                                    Text {
                                        Layout.fillWidth: true
                                        text: {
                                            var k = root.rpCategoryFilter
                                            var m = { "": "类别", "combat": "战斗", "cursed": "猎奇", "decoration": "装饰",
                                                "modded": "模组适配", "realistic": "写实", "simplistic": "简约",
                                                "themed": "主题", "tweaks": "微调", "utility": "实用",
                                                "vanilla-like": "原版", "fantasy": "幻想", "modern": "现代",
                                                "medieval": "中世纪", "futuristic": "未来", "cartoon": "卡通",
                                                "pvp": "PVP", "minigame": "小游戏", "gui": "界面", "font": "字体",
                                                "hd": "高清", "photorealism": "照片", "cute": "可爱",
                                                "dark": "暗色", "light": "亮色", "clean": "简洁" }
                                            return m[k] || (k || "类别")
                                        }
                                        color: root.rpCategoryFilter ? "#8aaeff" : "#788090"; font.pixelSize: 11
                                        elide: Text.ElideRight
                                    }
                                    Text { text: "▾"; color: "#505468"; font.pixelSize: 8 }
                                }
                                MouseArea {
                                    id: rpCatHov; anchors.fill: parent
                                    hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                    onClicked: { rpFeatMenu.close(); rpResMenu.close(); if (rpCatMenu.visible) rpCatMenu.close(); else rpCatMenu.open() }
                                }
                                Popup {
                                    id: rpCatMenu; closePolicy: Popup.NoAutoClose
                                    y: parent.height + 4; width: 160
                                    height: Math.min(rpCatFlick.contentHeight + 8, 240)
                                    padding: 0
                                    background: Rectangle { color: "#151922"; radius: 8; border.color: "#1e2230" }
                                    Flickable {
                                        id: rpCatFlick
                                        contentHeight: rpCatInner.implicitHeight; clip: true
                                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                                        ColumnLayout {
                                            id: rpCatInner; width: parent.width; spacing: 2
                                            Repeater {
                                                model: [
                                                    {key:"", label:"全部"},
                                                    {key:"combat", label:"战斗"},
                                                    {key:"cursed", label:"猎奇"},
                                                    {key:"decoration", label:"装饰"},
                                                    {key:"modded", label:"模组适配"},
                                                    {key:"realistic", label:"写实"},
                                                    {key:"simplistic", label:"简约"},
                                                    {key:"themed", label:"主题"},
                                                    {key:"tweaks", label:"微调"},
                                                    {key:"utility", label:"实用"},
                                                    {key:"vanilla-like", label:"原版"},
                                                    {key:"fantasy", label:"幻想"},
                                                    {key:"modern", label:"现代"},
                                                    {key:"medieval", label:"中世纪"},
                                                    {key:"futuristic", label:"未来"},
                                                    {key:"cartoon", label:"卡通"},
                                                    {key:"pvp", label:"PVP"},
                                                    {key:"minigame", label:"小游戏"},
                                                    {key:"gui", label:"界面"},
                                                    {key:"font", label:"字体"},
                                                    {key:"hd", label:"高清"},
                                                    {key:"photorealism", label:"照片"},
                                                    {key:"cute", label:"可爱"},
                                                    {key:"dark", label:"暗色"},
                                                    {key:"light", label:"亮色"},
                                                    {key:"clean", label:"简洁"}
                                                ]
                                                Rectangle {
                                                    Layout.fillWidth: true; height: 26; radius: 4
                                                    color: root.rpCategoryFilter === modelData.key ? "#1a2848" : "transparent"
                                                    Text {
                                                        color: root.rpCategoryFilter === modelData.key ? "#5068c8" : "#9094a8"; font.pixelSize: 11
                                                        font.weight: root.rpCategoryFilter === modelData.key ? Font.Bold : Font.Normal
                                                    }
                                                    MouseArea {
                                                        onClicked: { root.rpCategoryFilter = modelData.key; rpCatMenu.close() }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            // ── Feature dropdown ──
                            Rectangle {
                                id: rpFeatPill
                                Layout.preferredWidth: 95; height: 28; radius: 5
                                color: rpFeatHov.hovered ? "#1a2848" : "#0c0e14"
                                border.color: root.rpFeatureFilter ? "#5068c8" : "#2a3040"; border.width: 1
                                RowLayout {
                                    Text {
                                        Layout.fillWidth: true
                                        text: {
                                            var k = root.rpFeatureFilter
                                            var m = { "audio": "音频", "blocks": "方块", "core-shaders": "核心着色器",
                                                "entities": "实体", "environment": "环境", "equipment": "装备",
                                                "fonts": "字体", "gui": "图形界面", "items": "物品",
                                                "locale": "本地化", "models": "模型", "minecraft": "Minecraft" }
                                            return k ? (m[k] || k) : "功能"
                                        }
                                        color: root.rpFeatureFilter ? "#8aaeff" : "#788090"; font.pixelSize: 11
                                        elide: Text.ElideRight
                                    }
                                    Text { text: "▾"; color: "#505468"; font.pixelSize: 8 }
                                }
                                MouseArea {
                                    id: rpFeatHov; anchors.fill: parent
                                    hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                    onClicked: { rpCatMenu.close(); rpResMenu.close(); if (rpFeatMenu.visible) rpFeatMenu.close(); else rpFeatMenu.open() }
                                }
                                Popup {
                                    id: rpFeatMenu; closePolicy: Popup.NoAutoClose
                                    y: parent.height + 4; width: 160
                                    height: Math.min(rpFeatFlick.contentHeight + 8, 240)
                                    padding: 0
                                    background: Rectangle { color: "#151922"; radius: 8; border.color: "#1e2230" }
                                    Flickable {
                                        id: rpFeatFlick
                                        contentHeight: rpFeatInner.implicitHeight; clip: true
                                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                                        ColumnLayout {
                                            id: rpFeatInner; width: parent.width; spacing: 2
                                            Repeater {
                                                model: [
                                                    {key:"", label:"全部"},
                                                    {key:"audio", label:"音频"},
                                                    {key:"blocks", label:"方块"},
                                                    {key:"core-shaders", label:"核心着色器"},
                                                    {key:"entities", label:"实体"},
                                                    {key:"environment", label:"环境"},
                                                    {key:"equipment", label:"装备"},
                                                    {key:"fonts", label:"字体"},
                                                    {key:"gui", label:"图形界面"},
                                                    {key:"items", label:"物品"},
                                                    {key:"locale", label:"本地化"},
                                                    {key:"models", label:"模型"},
                                                    {key:"minecraft", label:"Minecraft"}
                                                ]
                                                Rectangle {
                                                    Layout.fillWidth: true; height: 26; radius: 4
                                                    color: root.rpFeatureFilter === modelData.key ? "#1a2848" : "transparent"
                                                    Text {
                                                        color: root.rpFeatureFilter === modelData.key ? "#5068c8" : "#9094a8"; font.pixelSize: 11
                                                        font.weight: root.rpFeatureFilter === modelData.key ? Font.Bold : Font.Normal
                                                    }
                                                    MouseArea {
                                                        onClicked: { root.rpFeatureFilter = modelData.key; rpFeatMenu.close() }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            // ── Resolution dropdown ──
                            Rectangle {
                                id: rpResPill
                                Layout.preferredWidth: 95; height: 28; radius: 5
                                color: rpResHov.hovered ? "#1a2848" : "#0c0e14"
                                border.color: root.rpResolutionFilter ? "#5068c8" : "#2a3040"; border.width: 1
                                RowLayout {
                                    Text {
                                        Layout.fillWidth: true
                                        text: root.rpResolutionFilter || "分辨率"
                                        color: root.rpResolutionFilter ? "#8aaeff" : "#788090"; font.pixelSize: 11
                                        elide: Text.ElideRight
                                    }
                                    Text { text: "▾"; color: "#505468"; font.pixelSize: 8 }
                                }
                                MouseArea {
                                    id: rpResHov; anchors.fill: parent
                                    hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                    onClicked: { rpCatMenu.close(); rpFeatMenu.close(); if (rpResMenu.visible) rpResMenu.close(); else rpResMenu.open() }
                                }
                                Popup {
                                    id: rpResMenu; closePolicy: Popup.NoAutoClose
                                    y: parent.height + 4; width: 130
                                    height: Math.min(rpResFlick.contentHeight + 8, 240)
                                    padding: 0
                                    background: Rectangle { color: "#151922"; radius: 8; border.color: "#1e2230" }
                                    Flickable {
                                        id: rpResFlick
                                        contentHeight: rpResInner.implicitHeight; clip: true
                                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                                        ColumnLayout {
                                            id: rpResInner; width: parent.width; spacing: 2
                                            Repeater {
                                                model: [
                                                    {key:"", label:"全部"},
                                                    {key:"8x", label:"8x"},
                                                    {key:"16x", label:"16x"},
                                                    {key:"32x", label:"32x"},
                                                    {key:"64x", label:"64x"},
                                                    {key:"128x", label:"128x"},
                                                    {key:"256x", label:"256x"},
                                                    {key:"512x", label:"512x"}
                                                ]
                                                Rectangle {
                                                    Layout.fillWidth: true; height: 26; radius: 4
                                                    color: root.rpResolutionFilter === modelData.key ? "#1a2848" : "transparent"
                                                    Text {
                                                        color: root.rpResolutionFilter === modelData.key ? "#5068c8" : "#9094a8"; font.pixelSize: 11
                                                        font.weight: root.rpResolutionFilter === modelData.key ? Font.Bold : Font.Normal
                                                    }
                                                    MouseArea {
                                                        onClicked: { root.rpResolutionFilter = modelData.key; rpResMenu.close() }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Row 2.5: Pre-release toggle — FIX 3: moved below version/type row, left-aligned
                    RowLayout {
                        Layout.fillWidth: true; spacing: 8
                        Text {
                            text: root.rpShowPreReleases ? "\u2b07 \u9690\u85cf\u6d4b\u8bd5\u7248" : "\u25b8 \u663e\u793a\u6d4b\u8bd5\u7248"
                            color: root.rpShowPreReleases ? "#5068c8" : "#505468"; font.pixelSize: 10
                            MouseArea {
                                // FIX 3: Pre-release toggle ONLY filters the version dropdown, not search results
                                onClicked: { root.rpShowPreReleases = !root.rpShowPreReleases }
                            }
                        }
                        Item { Layout.fillWidth: true }
                    }

                    // Row 3: Buttons
                    RowLayout {
                        Layout.fillWidth: true; spacing: 8
                        Item { Layout.fillWidth: true }

                        Rectangle {
                            width: 72; height: 28; radius: 5
                            color: "#5068c8"
                            Text { anchors.centerIn: parent; text: "搜索"; color: "#e8ecf8"; font.pixelSize: 12 }
                            MouseArea {
                                // FIX 1: Search button — the ONLY trigger for loadRpFirstPage() from filter changes
                                onClicked: loadRpFirstPage()
                            }
                        }

                        Rectangle {
                            width: 72; height: 28; radius: 5
                            color: rpResetHov.hovered ? "#252a38" : "#151922"
                            border.color: "#2a3040"; border.width: 1
                            visible: root.rpCategoryFilter || root.rpFeatureFilter || root.rpResolutionFilter || root.rpGameVersion || rpSearchInput.text
                            Text { anchors.centerIn: parent; text: "重置"; color: "#9094a8"; font.pixelSize: 12 }
                            MouseArea {
                                id: rpResetHov; anchors.fill: parent
                                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    root.rpCategoryFilter = ""
                                    root.rpFeatureFilter = ""
                                    root.rpResolutionFilter = ""
                                    root.rpGameVersion = ""
                                    rpSearchInput.text = ""
                                    // FIX 1: Clear all filters, then trigger search
                                    loadRpFirstPage()
                                }
                            }
                        }
                    }
                }
            }

            Text {
                text: "资源包 | 来源: MCIM (mcimirror.top) | " + (rpTotalHits || rpResultsModel.count || 0) + " 个结果"
                color: "#505468"; font.pixelSize: 11
            }
            // ── Results: vertical full-width cards ──
            ScrollView {
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                ScrollBar.vertical.policy: ScrollBar.AsNeeded

                ListView {
                    id: rpListView
                    model: rpResultsModel
                    cacheBuffer: 200

                    // Footer: load more indicator
                    footer: Rectangle {
                        width: rpListView.width; height: rpHasMore ? 40 : 0
                        color: "transparent"; visible: rpHasMore
                        Text {
                            text: rpLoadingMore ? "加载中..." : "加载更多"
                            color: "#5068c8"; font.pixelSize: 11
                        }
                        MouseArea {
                            onClicked: { if (!rpLoadingMore && rpHasMore) loadNextRpPage() }
                        }
                    }

                    // Auto-load when scrolled near bottom
                    onContentYChanged: {
                        if (!rpLoadingMore && rpHasMore && rpListView.count > 0) {
                            var bottomEdge = contentHeight - height
                            if (contentY >= bottomEdge - 100) {
                                loadNextRpPage()
                            }
                        }
                    }

                    delegate: Rectangle {
                        width: rpListView.width - 8; height: 130; clip: true
                        color: rpCardHov.hovered ? "#121620" : "#0e1018"
                        radius: 10; border.color: rpCardHov.hovered ? "#5068c8" : "#1a1f2a"; border.width: 1

                        RowLayout {

                            // Icon (network image with MCIM fallback)
                            Rectangle {
                                width: 44; height: 44; radius: 10; color: "#1a1f2e"
                                Layout.preferredWidth: 44; Layout.preferredHeight: 44
                                clip: true

                                Image {
                                    id: rpCardIcon
                                    property string rpIconCacheKey: ""

                                    source: ""
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true; cache: true
                                    autoTransform: true
                                    sourceSize.width: 88; sourceSize.height: 88

                                    function triggerIconLoad() {
                                        if (!model || !model.icon) return
                                        var url = model.icon
                                        url = url.replace("cdn.modrinth.com", "mod.mcimirror.top")
                                        url = url.replace("cdn-alt.modrinth.com", "mod.mcimirror.top")
                                        rpIconCacheKey = url
                                        rpIconFallback.visible = false
                                        // Check if already cached
                                        if (backend && backend.cachedIconPath) {
                                            var cached = backend.cachedIconPath(url)
                                            if (cached) {
                                                source = cached
                                                return
                                            }
                                            // Trigger async download+convert
                                            backend.cacheIconAsync(url)
                                        }
                                    }
                                    Component.onCompleted: triggerIconLoad()

                                    Connections {
                                        target: backend
                                        function onIconCached(webpUrl, pngPath) {
                                            if (webpUrl !== rpCardIcon.rpIconCacheKey) return
                                            if (pngPath) {
                                                rpCardIcon.source = pngPath
                                            } else {
                                                rpIconFallback.visible = true
                                            }
                                        }
                                    }
                                    onStatusChanged: {
                                        if (status === Image.Loading) return
                                        if (status === Image.Ready) { rpIconFallback.visible = false }
                                        else if (source && source.toString() !== "") { rpIconFallback.visible = true }
                                    }
                                }

                                Text {
                                    id: rpIconFallback
                                    text: model ? (model.title ? model.title[0] : "R") : "R"
                                    color: "#5068c8"; font.pixelSize: 18; font.bold: true
                                }
                            }

                            // Content
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 3

                                // Row 1: Title + downloads
                                RowLayout {
                                    Layout.fillWidth: true; spacing: 6
                                    Text {
                                        text: model.title || ""; color: "#d0d4e0"
                                        font.pixelSize: 13; font.bold: true; elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                    Text {
                                        text: {
                                            var d = model ? (model.downloads || 0) : 0
                                            if (d >= 1000000) return "↓" + (d/1000).toFixed(1) + "K"
                                            if (d >= 1000) return "↓" + (d/1000).toFixed(1) + "K"
                                            return "↓" + d
                                        }
                                        color: "#788090"; font.pixelSize: 10
                                    }
                                }

                                // Row 2: Chinese tags (imperative, no Repeater binding)
                                Item {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: rpTagRow.hasTags ? 18 : 0
                                    visible: rpTagRow.hasTags
                                    clip: true
                                    Row {
                                        id: rpTagRow
                                        spacing: 4
                                        property string tagsJson: ""
                                        property string _tagPending: ""
                                        property bool hasTags: false
                                        property var tagMap: ({
                                            "combat":"战斗", "cursed":"猎奇", "decoration":"装饰",
                                            "modded":"模组适配", "realistic":"写实", "simplistic":"简约",
                                            "themed":"主题", "tweaks":"微调", "utility":"实用",
                                            "vanilla-like":"原版", "fantasy":"幻想", "modern":"现代",
                                            "medieval":"中世纪", "futuristic":"未来", "cartoon":"卡通",
                                            "pvp":"PVP", "minigame":"小游戏", "gui":"界面", "font":"字体",
                                            "hd":"高清", "photorealism":"照片", "cute":"可爱",
                                            "dark":"暗色", "light":"亮色", "clean":"简洁"
                                        })
                                        Timer {
                                            id: rpTagTimer; interval: 1
                                            onTriggered: {
                                                var json = rpTagRow._tagPending; rpTagRow._tagPending = ""
                                                for (var i = rpTagRow.children.length - 1; i >= 0; i--) {
                                                    if (rpTagRow.children[i] !== rpTagComp) rpTagRow.children[i].destroy()
                                                }
                                                if (!json || json === "" || json === "[]") { rpTagRow.hasTags = false; return }
                                                var tags = []; try { tags = JSON.parse(json) } catch(e) { rpTagRow.hasTags = false; return }
                                                rpTagRow.hasTags = (tags.length > 0)
                                                for (var t = 0; t < Math.min(tags.length, 4); t++) {
                                                    var en = String(tags[t]).toLowerCase()
                                                    rpTagComp.createObject(rpTagRow, {
                                                        "tagLabel": rpTagRow.tagMap[en] || en
                                                    })
                                                }
                                            }
                                        }
                                        onTagsJsonChanged: { rpTagRow._tagPending = tagsJson; rpTagTimer.restart() }
                                        Component {
                                            id: rpTagComp
                                            Rectangle {
                                                height: 16; width: tText.implicitWidth + 10; radius: 4
                                                color: "#151922"; border.color: "#2a3040"; border.width: 1
                                                property string tagLabel: ""
                                                Text {
                                                    id: tText; anchors.centerIn: parent
                                                    text: tagLabel; color: "#788090"; font.pixelSize: 9
                                                }
                                            }
                                        }
                                        Binding {
                                            target: rpTagRow; property: "tagsJson"
                                            value: model ? (model.categories || "[]") : "[]"
                                            when: model !== null
                                        }
                                    }
                                }

                                // Row 2.5: Feature tags (PBR, animations, etc.)
                                Item {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: rpFeatRow.hasFeats ? 18 : 0
                                    visible: rpFeatRow.hasFeats
                                    clip: true
                                    Row {
                                        id: rpFeatRow
                                        spacing: 4
                                        property string featJson: ""
                                        property string _featPending: ""
                                        property bool hasFeats: false
                                        Timer {
                                            id: rpFeatTimer; interval: 1
                                            onTriggered: {
                                                var json = rpFeatRow._featPending; rpFeatRow._featPending = ""
                                                for (var i = rpFeatRow.children.length - 1; i >= 0; i--) {
                                                    if (rpFeatRow.children[i] !== rpFeatComp) rpFeatRow.children[i].destroy()
                                                }
                                                if (!json || json === "" || json === "[]") { rpFeatRow.hasFeats = false; return }
                                                var tags = []; try { tags = JSON.parse(json) } catch(e) { rpFeatRow.hasFeats = false; return }
                                                rpFeatRow.hasFeats = (tags.length > 0)
                                                for (var t = 0; t < Math.min(tags.length, 4); t++) {
                                                    var en = String(tags[t]).toLowerCase()
                                                    rpFeatComp.createObject(rpFeatRow, {
                                                        "tagLabel": root.rpFeatureMap[en] || en
                                                    })
                                                }
                                            }
                                        }
                                        onFeatJsonChanged: { rpFeatRow._featPending = featJson; rpFeatTimer.restart() }
                                        Component {
                                            id: rpFeatComp
                                            Rectangle {
                                                height: 16; width: tText2.implicitWidth + 10; radius: 4
                                                color: "#1a1428"; border.color: "#382848"; border.width: 1
                                                property string tagLabel: ""
                                                Text {
                                                    id: tText2; anchors.centerIn: parent
                                                    text: tagLabel; color: "#a878c8"; font.pixelSize: 9
                                                }
                                            }
                                        }
                                        Binding {
                                            target: rpFeatRow; property: "featJson"
                                            value: model ? (model.features || "[]") : "[]"
                                            when: model !== null
                                        }
                                    }
                                }

                                // Row 2.6: Resolution tags
                                Item {
                                    Layout.fillWidth: true; Layout.preferredHeight: rpResTagRow2.hasRes ? 18 : 0
                                    visible: rpResTagRow2.hasRes
                                    clip: true
                                    Row {
                                        id: rpResTagRow2
                                        spacing: 4
                                        property bool hasRes: false
                                        property string resJson: ""
                                        property string _resPending: ""
                                        Timer {
                                            id: rpResTimer; interval: 1
                                            onTriggered: {
                                                var json = rpResTagRow2._resPending; rpResTagRow2._resPending = ""
                                                for (var i = rpResTagRow2.children.length - 1; i >= 0; i--) {
                                                    if (rpResTagRow2.children[i] !== rpResComp) rpResTagRow2.children[i].destroy()
                                                }
                                                if (!json || json === "" || json === "[]") { rpResTagRow2.hasRes = false; return }
                                                var tags = []; try { tags = JSON.parse(json) } catch(e) { rpResTagRow2.hasRes = false; return }
                                                rpResTagRow2.hasRes = (tags.length > 0)
                                                for (var t = 0; t < Math.min(tags.length, 4); t++) {
                                                    rpResComp.createObject(rpResTagRow2, {
                                                        "tagLabel": String(tags[t])
                                                    })
                                                }
                                            }
                                        }
                                        onResJsonChanged: { rpResTagRow2._resPending = resJson; rpResTimer.restart() }
                                        Component {
                                            id: rpResComp
                                            Rectangle {
                                                height: 16; width: tText3.implicitWidth + 10; radius: 4
                                                color: "#282218"; border.color: "#504828"; border.width: 1
                                                property string tagLabel: ""
                                                Text {
                                                    id: tText3; anchors.centerIn: parent
                                                    text: tagLabel; color: "#c8a860"; font.pixelSize: 9
                                                }
                                            }
                                        }
                                        Binding {
                                            target: rpResTagRow2; property: "resJson"
                                            value: model ? (model.resolutions || "[]") : "[]"
                                            when: model !== null
                                        }
                                    }
                                }

                                // Row 3: Description (1 line)
                                Text {
                                    Layout.fillWidth: true
                                    text: model.desc || ""; color: "#505468"; font.pixelSize: 10
                                    elide: Text.ElideRight; maximumLineCount: 1
                                }

                                // Row 4: Version chips + date
                                RowLayout {
                                    Layout.fillWidth: true; spacing: 6
                                    Item {
                                        Layout.fillWidth: true; Layout.preferredHeight: 18
                                        clip: true
                                        Row {
                                            id: rpChipRow
                                            spacing: 3
                                            property string chipsJson: ""
                                            property string _pending: ""
                                            Timer {
                                                id: rpChipTimer; interval: 1
                                                onTriggered: {
                                                    var json = rpChipRow._pending; rpChipRow._pending = ""
                                                    for (var i = rpChipRow.children.length - 1; i >= 0; i--) {
                                                        if (rpChipRow.children[i] !== rpChipComp && rpChipRow.children[i] !== rpChipPlaceholder)
                                                            rpChipRow.children[i].destroy()
                                                    }
                                                    if (!json || json === "") return
                                                    var items = []; try { items = JSON.parse(json) } catch(e) { return }
                                                    rpChipPlaceholder.visible = (items.length === 0)
                                                    for (var j = 0; j < items.length; j++) {
                                                        rpChipComp.createObject(rpChipRow, {
                                                            "chipText": items[j].text, "chipColor": items[j].color
                                                        })
                                                    }
                                                }
                                            }
                                            onChipsJsonChanged: { rpChipRow._pending = chipsJson; rpChipTimer.restart() }
                                            Component {
                                                id: rpChipComp
                                                Rectangle {
                                                    height: 16; width: cText.implicitWidth + 8; radius: 4
                                                    color: "#151922"; border.color: chipColor; border.width: 1
                                                    property string chipColor: "#90a0c8"; property string chipText: ""
                                                    Text {
                                                        id: cText; anchors.centerIn: parent
                                                        text: chipText; color: chipColor; font.pixelSize: 8
                                                        font.family: "Consolas, monospace"
                                                    }
                                                }
                                            }
                                            Rectangle {
                                                id: rpChipPlaceholder; height: 16; width: 48; radius: 4
                                                color: "#151922"; border.color: "#404860"; border.width: 1
                                                Text { anchors.centerIn: parent; text: "加载中"; color: "#404860"; font.pixelSize: 8 }
                                            }
                                            Binding {
                                                target: rpChipRow; property: "chipsJson"
                                                value: model ? (model.chips || "") : ""
                                                when: model !== null
                                            }
                                        }
                                    }
                                    Text {
                                        text: model.updated ? String(model.updated).slice(0, 10) : ""
                                        color: "#404860"; font.pixelSize: 9; visible: model.updated !== undefined
                                    }
                                }
                            }
                        }

                        MouseArea {
                            id: rpCardHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                console.log("[RP-DEBUG] card clicked:", model.slug)
                                rpDetailSlug = model.slug; rpDetailTitle = model.title
                                var iconUrl = model.icon || ""
                                iconUrl = iconUrl.replace("cdn.modrinth.com", "mod.mcimirror.top")
                                iconUrl = iconUrl.replace("cdn-alt.modrinth.com", "mod.mcimirror.top")
                                // Use same icon cache as main page cards (Qt lacks webp support)
                                if (backend && backend.cachedIconPath) {
                                    var cached = backend.cachedIconPath(iconUrl)
                                    if (cached) {
                                        page.rpDetailIconUrl = cached
                                    } else {
                                        page.rpDetailIconUrl = iconUrl  // temporary, will update after cache
                                        backend.cacheIconAsync(iconUrl)
                                    }
                                } else {
                                    page.rpDetailIconUrl = iconUrl
                                }
                                page.rpDetailAuthor = model.author || ""
                                page.rpDetailDesc = model.desc || ""
                                page.rpDetailDownloads = model.downloads || 0
                                page.rpDetailUpdated = model.updated || ""
                            }
                        }
                    }
                }
            }
        }
}
