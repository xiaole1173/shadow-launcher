// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: root

    Layout.fillWidth: true
    implicitHeight: col.implicitHeight + 24
    radius: StyleTokens.radiusLg
    color: StyleTokens.bgSecondary
    border.color: StyleTokens.bgElevated

    // ── 公共 API ──
    property string cardType: "mod"
    property string searchPlaceholder: "搜索..."
    property alias searchText: searchInput.text
    signal searchClicked()
    signal resetClicked()

    property var rawVersionIds: []
    property string mcVersion: ""
    property bool showPreReleases: false
    signal preReleaseToggled()

    // Mod
    property var modLoaderModel: []
    property var modLoaderLabels: ({})
    property var modCatModel: []
    property var modCatLabels: ({})
    property var modEnvModel: []
    property var modEnvLabels: ({})
    property string modLoader: ""
    property string modCategory: ""
    property string modEnvironment: ""

    // Shader
    property string shaderCategory: ""
    property string shaderFeature: ""
    property string shaderPerformance: ""
    property string shaderLoader: ""

    // RP
    property string rpCategory: ""
    property string rpFeature: ""
    property string rpResolution: ""

    // ── 内联数据模型 ──
    readonly property var _shaderCats: [
        {label:"全部", slug:""}, {label:"原版风格", slug:"vanilla-like"},
        {label:"幻想", slug:"fantasy"}, {label:"半写实", slug:"semi-realistic"},
        {label:"写实", slug:"realistic"}, {label:"卡通", slug:"cartoon"}, {label:"搞怪", slug:"cursed"}
    ]
    readonly property var _shaderFeatures: [
        {label:"全部", slug:""}, {label:"阴影", slug:"shadows"},
        {label:"泛光", slug:"bloom"}, {label:"反射", slug:"reflections"},
        {label:"植被", slug:"foliage"}, {label:"PBR 材质", slug:"pbr"},
        {label:"彩色光照", slug:"colored-lighting"}, {label:"路径追踪", slug:"path-tracing"}
    ]
    readonly property var _shaderPerfs: [
        {label:"全部", slug:""}, {label:"极低", slug:"potato"},
        {label:"低", slug:"low"}, {label:"中", slug:"medium"}, {label:"高", slug:"high"}
    ]
    readonly property var _shaderLoaders: [
        {label:"全部", slug:""}, {label:"Iris", slug:"iris"}, {label:"OptiFine", slug:"optifine"}
    ]

    readonly property var _rpCategories: [
        {key:"", label:"全部"}, {key:"combat", label:"战斗"},
        {key:"cursed", label:"猎奇"}, {key:"decoration", label:"装饰"},
        {key:"modded", label:"模组适配"}, {key:"realistic", label:"写实"},
        {key:"simplistic", label:"简约"}, {key:"themed", label:"主题"},
        {key:"tweaks", label:"微调"}, {key:"utility", label:"实用"},
        {key:"vanilla-like", label:"原版"}, {key:"fantasy", label:"幻想"},
        {key:"modern", label:"现代"}, {key:"medieval", label:"中世纪"},
        {key:"futuristic", label:"未来"}, {key:"cartoon", label:"卡通"},
        {key:"pvp", label:"PVP"}, {key:"minigame", label:"小游戏"},
        {key:"gui", label:"界面"}, {key:"font", label:"字体"},
        {key:"hd", label:"高清"}, {key:"photorealism", label:"照片"},
        {key:"cute", label:"可爱"}, {key:"dark", label:"暗色"},
        {key:"light", label:"亮色"}, {key:"clean", label:"简洁"}
    ]
    readonly property var _rpFeatures: [
        {key:"", label:"全部"}, {key:"audio", label:"音频"},
        {key:"blocks", label:"方块"}, {key:"core-shaders", label:"核心着色器"},
        {key:"entities", label:"实体"}, {key:"environment", label:"环境"},
        {key:"equipment", label:"装备"}, {key:"fonts", label:"字体"},
        {key:"gui", label:"图形界面"}, {key:"items", label:"物品"},
        {key:"locale", label:"本地化"}, {key:"models", label:"模型"},
        {key:"minecraft", label:"Minecraft"}
    ]
    readonly property var _rpResolutions: [
        {key:"", label:"全部"}, {key:"8x", label:"8x"}, {key:"16x", label:"16x"},
        {key:"32x", label:"32x"}, {key:"64x", label:"64x"}, {key:"128x", label:"128x"},
        {key:"256x", label:"256x"}, {key:"512x", label:"512x"}
    ]

    function _mcVersionLabel(v) { return v ? "MC " + v : "全部" }

    ColumnLayout {
        id: col
        anchors.fill: parent; anchors.margins: 12; spacing: 8

        // ═════════════════════════════════════════════
        // Row 1: 名称 + 搜索 + MC版本 + 测试版
        // ═════════════════════════════════════════════
        RowLayout {
            Layout.fillWidth: true; spacing: 10

            Text { text: "名称"; color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: 32 }
            SearchBox {
                id: searchInput
                placeholderText: root.searchPlaceholder
                Layout.fillWidth: true
                Layout.minimumWidth: 100
            }

            Text { text: "版本"; color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: 28 }
            ShadowDropdown {
                id: verDropdown
                Layout.preferredWidth: 100
                model: {
                    if (!root.rawVersionIds || root.rawVersionIds.length === 0)
                        return ["1.21.10","1.20.6"]
                    var seen = new Set(); var groups = []
                    for (var i = 0; i < root.rawVersionIds.length; i++) {
                        var v = root.rawVersionIds[i]
                        if (!root.showPreReleases && !/^[0-9.]+$/.test(v)) continue
                        var major = v.split(/[.\-]/).slice(0,2).join(".")
                        if (!seen.has(major)) { seen.add(major); groups.push(major) }
                        if (groups.length >= 30) break
                    }
                    return [""].concat(groups)
                }
                labelFn: function(v) { return root._mcVersionLabel(v) }
                currentValue: root.mcVersion
                onValueSelected: function(v) { root.mcVersion = v }
            }

            // 测试版开关
            Rectangle {
                id: preTog
                width: preTogText.implicitWidth + 16; height: 24; radius: StyleTokens.radiusSm
                color: preTogMouse.containsMouse ? (root.showPreReleases ? "#282040" : "#1a1e28") : (root.showPreReleases ? "#1e1838" : "#12151c")
                border.color: root.showPreReleases ? "#504080" : StyleTokens.borderLight; border.width: 1
                Behavior on color { ColorAnimation { duration: 150 } }
                Text {
                    id: preTogText
                    anchors.centerIn: parent
                    text: root.showPreReleases ? "隐藏测试版" : "显示测试版"
                    color: root.showPreReleases ? "#9088e0" : "#687080"; font.pixelSize: StyleTokens.fontSizeXs
                }
                MouseArea {
                    id: preTogMouse
                    anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: { root.showPreReleases = !root.showPreReleases; root.preReleaseToggled() }
                }
            }
        }

        // ═════════════════════════════════════════════
        // Row 2: Mod 筛选条件
        // ═════════════════════════════════════════════
        RowLayout {
            visible: root.cardType === "mod"
            Layout.fillWidth: true; spacing: 8

            Text { text: "加载器"; color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: Math.max(36, implicitWidth) }
            ShadowDropdown {
                id: modLdrDropdown; Layout.fillWidth: true; Layout.minimumWidth: 80
                model: root.modLoaderModel
                labelFn: function(v) { return root.modLoaderLabels[v] || "全部" }
                currentValue: root.modLoader
                onValueSelected: function(v) { root.modLoader = v }
            }
            Text { text: "类别"; color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: Math.max(28, implicitWidth) }
            ShadowDropdown {
                id: modCatDropdown; Layout.fillWidth: true; Layout.minimumWidth: 80
                model: root.modCatModel
                labelFn: function(v) { return root.modCatLabels[v] || "全部" }
                currentValue: root.modCategory
                onValueSelected: function(v) { root.modCategory = v }
            }
            Text { text: "环境"; color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: Math.max(28, implicitWidth) }
            ShadowDropdown {
                id: modEnvDropdown; Layout.fillWidth: true; Layout.minimumWidth: 80
                model: root.modEnvModel
                labelFn: function(v) { return root.modEnvLabels[v] || v || "全部" }
                currentValue: root.modEnvironment
                onValueSelected: function(v) { root.modEnvironment = v }
            }
        }

        // ═════════════════════════════════════════════
        // Row 2: Shader 筛选条件
        // ═════════════════════════════════════════════
        RowLayout {
            visible: root.cardType === "shader"
            Layout.fillWidth: true; spacing: 8

            Text { text: "风格"; color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: Math.max(28, implicitWidth) }
            ShadowDropdown {
                id: shaderCatDropdown; Layout.fillWidth: true; Layout.minimumWidth: 80
                model: root._shaderCats; valueKey: "slug"
                currentValue: root.shaderCategory
                onValueSelected: function(v) { root.shaderCategory = v }
            }
            Text { text: "特性"; color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: Math.max(28, implicitWidth) }
            ShadowDropdown {
                id: shaderFeatDropdown; Layout.fillWidth: true; Layout.minimumWidth: 90
                model: root._shaderFeatures; valueKey: "slug"
                currentValue: root.shaderFeature
                onValueSelected: function(v) { root.shaderFeature = v }
            }
            Text { text: "性能"; color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: Math.max(28, implicitWidth) }
            ShadowDropdown {
                id: shaderPerfDropdown; Layout.fillWidth: true; Layout.minimumWidth: 70
                model: root._shaderPerfs; valueKey: "slug"
                currentValue: root.shaderPerformance
                onValueSelected: function(v) { root.shaderPerformance = v }
            }
            Text { text: "加载器"; color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: Math.max(32, implicitWidth) }
            ShadowDropdown {
                id: shaderLdrDropdown; Layout.fillWidth: true; Layout.minimumWidth: 80
                model: root._shaderLoaders; valueKey: "slug"
                currentValue: root.shaderLoader
                onValueSelected: function(v) { root.shaderLoader = v }
            }
        }

        // ═════════════════════════════════════════════
        // Row 2: RP 筛选条件
        // ═════════════════════════════════════════════
        RowLayout {
            visible: root.cardType === "resourcepack"
            Layout.fillWidth: true; spacing: 8

            Text { text: "类别"; color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: Math.max(28, implicitWidth) }
            ShadowDropdown {
                id: rpCatDropdown; Layout.fillWidth: true; Layout.minimumWidth: 80
                model: root._rpCategories; valueKey: "key"
                labelFn: function(v) {
                    for (var i = 0; i < root._rpCategories.length; i++)
                        if (root._rpCategories[i].key === v) return root._rpCategories[i].label
                    return v || "全部"
                }
                currentValue: root.rpCategory
                onValueSelected: function(v) { root.rpCategory = v }
            }
            Text { text: "功能"; color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: Math.max(28, implicitWidth) }
            ShadowDropdown {
                id: rpFeatDropdown; Layout.fillWidth: true; Layout.minimumWidth: 90
                model: root._rpFeatures; valueKey: "key"
                labelFn: function(v) {
                    for (var i = 0; i < root._rpFeatures.length; i++)
                        if (root._rpFeatures[i].key === v) return root._rpFeatures[i].label
                    return v || "全部"
                }
                currentValue: root.rpFeature
                onValueSelected: function(v) { root.rpFeature = v }
            }
            Text { text: "分辨率"; color: "#9094a8"; font.pixelSize: StyleTokens.fontSizeSm; Layout.preferredWidth: Math.max(36, implicitWidth) }
            ShadowDropdown {
                id: rpResDropdown; Layout.fillWidth: true; Layout.minimumWidth: 70
                model: root._rpResolutions; valueKey: "key"
                currentValue: root.rpResolution
                onValueSelected: function(v) { root.rpResolution = v }
            }
        }

        // ═════════════════════════════════════════════
        // Row 3: 搜索 + 重置条件 按钮
        // ═════════════════════════════════════════════
        RowLayout {
            Layout.fillWidth: true; spacing: 12

            Rectangle {
                id: searchBtn
                width: 80; height: 34; radius: StyleTokens.radiusMd
                color: searchBtnMouse.containsMouse ? "#5a78e0" : StyleTokens.accentHover
                scale: searchBtnMouse.pressed ? 0.94 : (searchBtnMouse.containsMouse ? 1.04 : 1.0)
                Behavior on color { ColorAnimation { duration: 150 } }
                Behavior on scale { SpringAnimation { spring: 1.6; damping: 0.28; epsilon: 0.01 } }
                Text {
                    anchors.centerIn: parent
                    text: "搜索"; color: StyleTokens.textInverse
                    font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.DemiBold
                }
                MouseArea {
                    id: searchBtnMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: root.searchClicked()
                }
            }

            Rectangle {
                id: resetBtn
                width: 100; height: 34; radius: StyleTokens.radiusMd
                color: resetBtnMouse.containsMouse ? "#252a38" : "#151922"
                border.color: resetBtnMouse.containsMouse ? "#5068c8" : StyleTokens.bgHover; border.width: 1
                scale: resetBtnMouse.pressed ? 0.94 : (resetBtnMouse.containsMouse ? 1.04 : 1.0)
                Behavior on color { ColorAnimation { duration: 150 } }
                Behavior on border.color { ColorAnimation { duration: 150 } }
                Behavior on scale { SpringAnimation { spring: 1.6; damping: 0.28; epsilon: 0.01 } }
                Text {
                    anchors.centerIn: parent
                    text: "重置条件"; color: "#b0b8c8"
                    font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.Medium
                }
                MouseArea {
                    id: resetBtnMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: root.resetClicked()
                }
            }

            Item { Layout.fillWidth: true }
        }
    }
}
