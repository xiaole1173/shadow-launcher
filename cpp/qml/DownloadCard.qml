// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

// DownloadCard — 统一下载页卡片组件
// 样式基准：资源包卡片（44×44 图标 + 标签行 + 底部信息）
// 兼容 Mod / 光影 / 资源包 三种数据模型
//
// 用法：
//   DownloadCard {
//       title: model.title
//       iconUrl: model.icon
//       slug: model.slug
//       downloads: model.downloads
//       source: "Modrinth"
//       gameVersions: model.versions
//       loaders: model.loadersList
//       dateModified: model.dateModified
//       categoriesJson: model.categories || "[]"
//       featuresJson: model.features || "[]"
//       resolutionsJson: model.resolutions || "[]"
//       onClicked: { /* 打开详情 */ }
//   }

Rectangle {
    id: root

    implicitWidth: parent ? parent.width : 300
    implicitHeight: contentCol.implicitHeight + 20
    radius: StyleTokens.radiusLg
    color: hovered ? "#121620" : "#0e1018"
    border.color: hovered ? StyleTokens.accentHover : StyleTokens.bgInput
    border.width: hovered ? 1.5 : 1
    scale: hovered ? 1.01 : 1.0

    opacity: 0
    Component.onCompleted: opacity = 1

    Behavior on color { ColorAnimation { duration: 150 } }
    Behavior on border.color { ColorAnimation { duration: 150 } }
    Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutBack } }
    Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }

    // ── 公共 API ──
    property string title: ""
    property string description: ""
    property string iconUrl: ""
    property string slug: ""
    property int downloads: 0
    property string source: ""            // "Modrinth" 等来源标签
    property string gameVersions: ""      // 逗号分隔的 MC 版本
    property string loaders: ""           // 逗号分隔的加载器名 (Mod)
    property string dateModified: ""      // ISO 日期

    // RP 风格标签 (ListModel 中的 JSON string 数组)
    property string categoriesJson: "[]"
    property string featuresJson: "[]"
    property string resolutionsJson: "[]"

    signal clicked()

    // ── 内部状态 ──
    property bool hovered: false

    // ── 悬停辉光 ──
    Rectangle {
        anchors.fill: parent; radius: parent.radius
        opacity: root.hovered ? 0.06 : 0
        gradient: Gradient {
            GradientStop { position: 0; color: StyleTokens.accentLight }
            GradientStop { position: 1; color: StyleTokens.accentLight }
        }
        Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
    }

    // ── 主布局 ──
    RowLayout {
        anchors.fill: parent; anchors.margins: 10; spacing: 10

        // ── 图标 44×44 ──
        Rectangle {
            width: 44; height: 44; radius: StyleTokens.radiusLg
            color: StyleTokens.bgCard
            Layout.preferredWidth: 44; Layout.preferredHeight: 44
            Layout.alignment: Qt.AlignTop
            clip: true

            Image {
                id: iconImg
                anchors.fill: parent
                fillMode: Image.PreserveAspectCrop
                asynchronous: true; cache: true
                sourceSize.width: 88; sourceSize.height: 88
                source: root.iconUrl || ""
                onStatusChanged: {
                    if (status === Image.Ready) iconFb.visible = false
                    else if (status === Image.Error) iconFb.visible = true
                }
            }
            Text {
                id: iconFb
                anchors.centerIn: parent
                text: root.title ? root.title[0] : "?"
                color: StyleTokens.accentHover
                font.pixelSize: StyleTokens.fontSizeXl; font.bold: true
                visible: !root.iconUrl
            }
        }

        // ── 内容区 ──
        ColumnLayout {
            id: contentCol
            Layout.fillWidth: true
            spacing: 3

            // Row 1: 标题 + 下载量 + 来源标签
            RowLayout {
                Layout.fillWidth: true; spacing: 6

                Text {
                    text: root.title || ""
                    color: "#d0d4e0"
                    font.pixelSize: StyleTokens.fontSizeMd; font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                RowLayout {
                    spacing: 3
                    visible: root.downloads > 0
                    Image {
                        source: "icons/lucide/download.svg"
                        width: 8; height: 8; sourceSize.width: 8; sourceSize.height: 8
                        Layout.alignment: Qt.AlignVCenter
                    }
                    Text {
                        text: formatDownloads(root.downloads)
                        color: "#788090"; font.pixelSize: StyleTokens.fontSizeXs
                    }
                }

                Rectangle {
                    visible: root.source !== ""
                    Layout.preferredWidth: 56; height: 15; radius: StyleTokens.radiusXs
                    color: StyleTokens.bgElevated
                    Text {
                        anchors.centerIn: parent
                        text: root.source; color: "#9088e0"; font.pixelSize: StyleTokens.fontSizeXs
                    }
                }
            }

            // ── 标签区 (RP 风格: 分类/特性/分辨率) ──
            ColumnLayout {
                id: tagSection
                Layout.fillWidth: true
                spacing: 3
                visible: categories.length > 0 || features.length > 0 || resolutions.length > 0

                readonly property var categories: parseJSON(root.categoriesJson)
                readonly property var features: parseJSON(root.featuresJson)
                readonly property var resolutions: parseJSON(root.resolutionsJson)

                DownloadCardTagRow {
                    visible: tagSection.categories.length > 0
                    tags: tagSection.categories.map(function(t){ return root.translateCategory(t) })
                    tagBg: "#151922"; tagBorder: StyleTokens.bgHover; tagColor: "#788090"
                }
                DownloadCardTagRow {
                    visible: tagSection.features.length > 0
                    tags: tagSection.features.map(function(t){ return root.translateFeature(t) })
                    tagBg: "#1a1428"; tagBorder: "#382848"; tagColor: "#a878c8"
                }
                DownloadCardTagRow {
                    visible: tagSection.resolutions.length > 0
                    tags: tagSection.resolutions
                    tagBg: "#282218"; tagBorder: "#504828"; tagColor: "#c8a860"
                }
            }

            // ── 描述 (1行) ──
            Text {
                Layout.fillWidth: true
                text: root.description || ""
                color: StyleTokens.textMuted
                font.pixelSize: StyleTokens.fontSizeXs
                elide: Text.ElideRight; maximumLineCount: 1
                visible: root.description !== ""
            }

            // ── 底部信息行: 版本范围 + 日期 + 加载器 ──
            RowLayout {
                Layout.fillWidth: true; spacing: 8

                Text {
                    text: formatVersionRange(root.gameVersions)
                    color: "#687080"; font.pixelSize: StyleTokens.fontSizeXs
                    visible: text !== ""
                }
                Text {
                    text: formatDate(root.dateModified)
                    color: "#687080"; font.pixelSize: StyleTokens.fontSizeXs
                    visible: root.dateModified !== ""
                }
                Text {
                    text: formatLoaders(root.loaders)
                    color: "#687080"; font.pixelSize: StyleTokens.fontSizeXs
                    elide: Text.ElideRight; Layout.fillWidth: true
                    visible: root.loaders !== ""
                }
                Item { Layout.fillWidth: true }
            }
        }
    }

    // ── 鼠标区域 ──
    MouseArea {
        anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
        onEntered: root.hovered = true
        onExited: root.hovered = false
        onClicked: root.clicked()
    }

    // ── 工具函数 ──
    function parseJSON(str) {
        if (!str) return []
        try { var a = JSON.parse(str); return Array.isArray(a) ? a : [] }
        catch(e) { return [] }
    }

    readonly property var _catMap: ({
        "combat":"战斗", "cursed":"猎奇", "decoration":"装饰",
        "modded":"模组适配", "realistic":"写实", "simplistic":"简约",
        "themed":"主题", "tweaks":"微调", "utility":"实用",
        "vanilla-like":"原版", "fantasy":"幻想", "modern":"现代",
        "medieval":"中世纪", "futuristic":"未来", "cartoon":"卡通",
        "pvp":"PVP", "minigame":"小游戏", "gui":"界面", "font":"字体",
        "hd":"高清", "photorealism":"照片", "cute":"可爱",
        "dark":"暗色", "light":"亮色", "clean":"简洁"
    })
    readonly property var _featMap: ({
        "audio": "音频", "blocks": "方块", "core-shaders": "核心着色器",
        "entities": "实体", "environment": "环境", "equipment": "装备",
        "fonts": "字体", "gui": "图形界面", "items": "物品",
        "locale": "本地化", "models": "模型", "minecraft": "Minecraft"
    })

    function translateCategory(en) {
        if (!en) return en
        var lc = String(en).toLowerCase()
        return _catMap[lc] || en
    }

    function translateFeature(en) {
        if (!en) return en
        var lc = String(en).toLowerCase()
        return _featMap[lc] || en
    }

    function formatDownloads(n) {
        if (n >= 100000000) return (n / 100000000).toFixed(1) + "亿"
        if (n >= 10000) return (n / 10000).toFixed(0) + "万"
        return String(n || 0)
    }

    function formatVersionRange(vs) {
        if (!vs || typeof vs !== "string" || vs.length === 0) return ""
        var parts = vs.split(",")
        var minV = null, maxV = null
        for (var i = 0; i < parts.length; i++) {
            var v = parts[i].trim(); if (!v) continue
            var segs = v.split("."); if (segs.length < 2) continue
            var key = segs[0] + "." + segs[1]
            if (!minV || key < minV) minV = key
            if (!maxV || key > maxV) maxV = key
        }
        if (!minV) return ""
        return minV === maxV ? "适用: " + minV : "适用: " + minV + "-" + maxV
    }

    function formatLoaders(s) {
        if (!s) return ""
        var parts = s.split(",")
        for (var i = 0; i < parts.length; i++) {
            var t = parts[i].trim()
            if (t.length > 0) parts[i] = t.charAt(0).toUpperCase() + t.slice(1)
        }
        return "加载器：" + parts.filter(function(x){return x}).join(", ")
    }

    function formatDate(isoStr) {
        if (!isoStr) return ""
        return String(isoStr).slice(0, 10)
    }
}
