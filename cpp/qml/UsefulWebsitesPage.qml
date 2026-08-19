// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "websites_data.js" as SitesData

// ═══ 实用网站页 ═══
// 两级界面：
//   0) 大类卡片（「本启动器」置顶占满整行，其余两列），数据源 qml/websites_data.js
//   1) 点击大类 → 该大类的网站卡片（主标题=站点名，副标题=网址+简介），点击直达浏览器
//
// 动画体系（2026-08-19 增强）：
//   - 载入：进入页面/切换视图时，卡片从下方滑入 + 淡入，逐张 stagger 延迟
//   - 悬停：上浮 + 图标放大 + 箭头右移 + 高亮边框/底色
//   - 点击：按压缩小 + 底色闪光反馈
Item {
    id: root
    anchors.fill: parent

    property var backend: null
    property var toastManager: null
    property var appWindow: null

    // ── 数据（从 websites_data.js 直接导入，无 XHR——同步 XHR 对 qrc 在 Loader 页内抛 Invalid state）──
    property var categories: []
    property bool dataReady: false
    // -1 = 大类视图；>=0 = 该大类的网站视图
    property int currentCategory: -1

    // ── 载入动画触发器：数值 +1 时对应视图的卡片重播滑入动画 ──
    property int catEpoch: 0
    property int siteEpoch: 0

    function loadData() {
        try {
            var all = SitesData.data.categories || []
            // 「本启动器」置顶
            var launcher = null
            var rest = []
            for (var i = 0; i < all.length; i++) {
                if (all[i].key === "launcher") launcher = all[i]
                else rest.push(all[i])
            }
            categories = launcher ? [launcher].concat(rest) : all
            dataReady = true
            console.info("[sites] websites_data.js imported, categories=" + categories.length
                         + ", first=" + (categories[0] ? categories[0].key : "?"))
        } catch (e) {
            console.warn("[sites] import websites_data.js failed:", e)
        }
    }

    Component.onCompleted: {
        loadData()
        catEpoch++          // 数据就绪后立即触发大类卡片载入动画
        pageEnterAnim.start()
    }

    function openUrl(url) {
        Qt.openUrlExternally(url)
    }

    // 切换视图 → 触发对应视图的卡片载入动画
    onCurrentCategoryChanged: {
        if (currentCategory < 0) catEpoch++
        else siteEpoch++
    }

    // ── 入场动画 ──
    opacity: 0
    NumberAnimation {
        id: pageEnterAnim
        target: root
        property: "opacity"
        to: 1
        duration: AnimationTokens.pageDuration
        easing.type: AnimationTokens.pageEasing
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 20; anchors.rightMargin: 20
        anchors.topMargin: 16; anchors.bottomMargin: 16
        spacing: 12

        // ── 页头：返回 + 标题 ──
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            BackButton {
                id: backBtn
                visible: root.currentCategory >= 0
                onClicked: root.currentCategory = -1
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    id: headerTitle
                    text: root.currentCategory >= 0
                          ? (root.categories.length > 0 && root.currentCategory < root.categories.length ? root.categories[root.currentCategory].title : "")
                          : qsTr("实用网站")
                    font.pixelSize: StyleTokens.fontSizeXl
                    font.bold: true
                    color: StyleTokens.textPrimary
                    elide: Text.ElideRight
                    Layout.fillWidth: true; Layout.minimumWidth: 0
                }
                Text {
                    id: headerSub
                    text: root.currentCategory >= 0
                          ? qsTr("共 %1 个网站，点击卡片直达").arg(root.currentCategory < root.categories.length ? root.categories[root.currentCategory].sites.length : 0)
                          : qsTr("精选 MC 生态实用网站，按大类浏览")
                    font.pixelSize: StyleTokens.fontSizeSm
                    color: StyleTokens.textTertiary
                    elide: Text.ElideRight
                    Layout.fillWidth: true; Layout.minimumWidth: 0
                }
            }
            Item { Layout.fillWidth: true }
        }

        // ── 分隔线 ──
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: StyleTokens.bgInput
        }

        // ── 内容区 ──
        Item {
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true

            // ═══ 视图 0：大类卡片（本启动器置顶占满整行，其余两列）═══
            Flickable {
                id: catFlick
                anchors.fill: parent
                clip: true
                contentWidth: catFlick.width
                contentHeight: catFlow.implicitHeight + 6 + 8
                visible: root.currentCategory < 0
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                Flow {
                    id: catFlow
                    // 四周留边距：hover 放大 1.015 不溢出容器边缘（全宽卡 ±5.8px < 8px）
                    x: 8
                    y: 6
                    width: catFlick.width - 16
                    spacing: 12

                    Repeater {
                        model: root.categories

                        delegate: Rectangle {
                            id: cCard
                            // 首卡（本启动器）占满整行；其余两列
                            readonly property bool isWide: index === 0
                            // ── 悬停/点击状态（卡片自身属性，由 MouseArea 显式赋值——
                            //    直接绑定后声明的 MouseArea.hovered 在本编译环境绑定不生效）──
                            property bool cHovered: false
                            property bool cPressed: false
                            width: isWide ? catFlow.width : (catFlow.width - catFlow.spacing) / 2
                            height: isWide ? 88 : 92
                            radius: StyleTokens.radiusLg
                            color: cHovered ? StyleTokens.bgHover : StyleTokens.bgCard
                            border.color: cHovered ? StyleTokens.accent : StyleTokens.bgInput
                            border.width: 1
                            clip: true

                            // ── 悬停 / 点击状态 ──
                            scale: (cHovered ? 1.015 : 1.0) * (cPressed ? 0.96 : 1.0)
                            Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }
                            Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                            Behavior on border.color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                            // hover 时提升层级，放大不被邻近卡片遮挡
                            z: cHovered ? 2 : 0

                            // 悬停上浮（独立 transform，与滑入动画不冲突）
                            transform: [
                                Translate { id: cSlide; y: 26 },
                                Translate {
                                    id: cLift
                                    y: cHovered ? -3 : 0
                                    Behavior on y { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }
                                }
                            ]

                            // 悬停高亮底
                            Rectangle {
                                anchors.fill: parent; radius: StyleTokens.radiusLg
                                color: StyleTokens.accentLight
                                opacity: cHovered ? 0.08 : 0.0
                                Behavior on opacity { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
                            }

                            // ── 载入动画：下滑入 + 淡入，逐张 stagger ──
                            opacity: 0
                            function playIn() {
                                cCard.opacity = 0
                                cSlide.y = 26
                                cAnim.restart()
                            }
                            // SequentialAnimation + PauseAnimation 实现逐张延迟（NumberAnimation 无 delay 属性，此编译器不认）
                            SequentialAnimation {
                                id: cAnim
                                PauseAnimation { duration: index * 55 }
                                ParallelAnimation {
                                    NumberAnimation {
                                        target: cCard; property: "opacity"
                                        to: 1; duration: 420; easing.type: Easing.OutCubic
                                    }
                                    NumberAnimation {
                                        target: cSlide; property: "y"
                                        to: 0; duration: 420; easing.type: Easing.OutCubic
                                    }
                                }
                            }
                            Component.onCompleted: playIn()
                            Connections {
                                target: root
                                function onCatEpochChanged() { cCard.playIn() }
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: isWide ? 16 : 14
                                anchors.rightMargin: 12
                                anchors.topMargin: 10; anchors.bottomMargin: 10
                                spacing: 12

                                // 大类图标
                                Rectangle {
                                    width: 44; height: 44; radius: StyleTokens.radiusMd
                                    color: StyleTokens.surfaceOverlay
                                    Layout.alignment: Qt.AlignVCenter
                                    Image {
                                        anchors.fill: parent; anchors.margins: 10
                                        source: "icons/lucide/" + (modelData.icon || "globe") + ".svg"
                                        fillMode: Image.PreserveAspectFit
                                        sourceSize.width: 24; sourceSize.height: 24
                                        scale: cHovered ? 1.12 : 1.0
                                        Behavior on scale { NumberAnimation { duration: 180; easing.type: Easing.OutBack } }
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 3
                                    Text {
                                        text: modelData.title
                                        font.pixelSize: isWide ? StyleTokens.fontSizeLg : StyleTokens.fontSizeMd
                                        font.weight: Font.DemiBold
                                        color: StyleTokens.textSecondary
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true; Layout.minimumWidth: 0
                                    }
                                    Text {
                                        text: qsTr("%1 个网站").arg(modelData.sites ? modelData.sites.length : 0)
                                        font.pixelSize: StyleTokens.fontSizeXs
                                        color: StyleTokens.textTertiary
                                        Layout.fillWidth: true; Layout.minimumWidth: 0
                                    }
                                }

                                // 箭头：悬停右移 + 点亮
                                Image {
                                    source: "icons/lucide/chevron-right.svg"
                                    width: 16; height: 16
                                    Layout.alignment: Qt.AlignVCenter
                                    opacity: cHovered ? 1.0 : 0.5
                                    Behavior on opacity { NumberAnimation { duration: 160 } }
                                    transform: Translate {
                                        id: cArrow
                                        x: cHovered ? 4 : 0
                                        Behavior on x { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }
                                    }
                                }
                            }

                            // 点击闪光反馈
                            Rectangle {
                                id: cFlash
                                anchors.fill: parent; radius: StyleTokens.radiusLg
                                color: StyleTokens.accentLight
                                opacity: 0
                            }
                            NumberAnimation {
                                id: cFlashScale
                                target: cFlash; property: "opacity"
                                to: 0; duration: 220; easing.type: Easing.OutCubic
                            }

                            MouseArea {
                                id: cHover
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onEntered: cCard.cHovered = true
                                onExited: cCard.cHovered = false
                                onPressedChanged: {
                                    cCard.cPressed = pressed
                                    if (pressed) {
                                        cFlash.opacity = 0.18
                                        cFlashScale.restart()
                                    }
                                }
                                onClicked: root.currentCategory = index
                            }
                        }
                    }
                }
            }

            // ═══ 视图 1：网站卡片 ═══
            Flickable {
                id: siteFlick
                anchors.fill: parent
                clip: true
                contentWidth: siteFlick.width
                contentHeight: siteFlow.implicitHeight + 6 + 8
                visible: root.currentCategory >= 0
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                Flow {
                    id: siteFlow
                    // 四周留边距：hover 放大 1.015 不溢出容器边缘
                    x: 8
                    y: 6
                    width: siteFlick.width - 16
                    spacing: 12

                    Repeater {
                        model: root.currentCategory >= 0 && root.currentCategory < root.categories.length
                               ? root.categories[root.currentCategory].sites : []

                        delegate: Rectangle {
                            id: sCard
                            // ── 悬停/点击状态（卡片自身属性，由 MouseArea 显式赋值——
                            //    直接绑定后声明的 MouseArea.hovered 在本编译环境绑定不生效）──
                            property bool sHovered: false
                            property bool sPressed: false
                            width: (siteFlow.width - siteFlow.spacing) / 2
                            height: 120
                            radius: StyleTokens.radiusLg
                            color: sHovered ? StyleTokens.bgHover : StyleTokens.bgCard
                            border.color: sHovered ? StyleTokens.accent : StyleTokens.bgInput
                            border.width: 1
                            clip: true

                            // ── 悬停 / 点击状态 ──
                            scale: (sHovered ? 1.015 : 1.0) * (sPressed ? 0.96 : 1.0)
                            Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }
                            Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                            Behavior on border.color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                            // hover 时提升层级，放大不被邻近卡片遮挡
                            z: sHovered ? 2 : 0

                            // 悬停上浮
                            transform: [
                                Translate { id: sSlide; y: 30 },
                                Translate {
                                    id: sLift
                                    y: sHovered ? -3 : 0
                                    Behavior on y { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }
                                }
                            ]

                            Rectangle {
                                anchors.fill: parent; radius: StyleTokens.radiusLg
                                color: StyleTokens.accentLight
                                opacity: sHovered ? 0.08 : 0.0
                                Behavior on opacity { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
                            }

                            // ── 载入动画：下滑入 + 淡入，逐张 stagger ──
                            opacity: 0
                            function playIn() {
                                sCard.opacity = 0
                                sSlide.y = 30
                                sAnim.restart()
                            }
                            // SequentialAnimation + PauseAnimation 实现逐张延迟
                            SequentialAnimation {
                                id: sAnim
                                PauseAnimation { duration: index * 50 }
                                ParallelAnimation {
                                    NumberAnimation {
                                        target: sCard; property: "opacity"
                                        to: 1; duration: 420; easing.type: Easing.OutCubic
                                    }
                                    NumberAnimation {
                                        target: sSlide; property: "y"
                                        to: 0; duration: 420; easing.type: Easing.OutCubic
                                    }
                                }
                            }
                            Component.onCompleted: playIn()
                            Connections {
                                target: root
                                function onSiteEpochChanged() { sCard.playIn() }
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 12

                                // 网站 favicon
                                Rectangle {
                                    width: 48; height: 48; radius: StyleTokens.radiusMd
                                    color: StyleTokens.surfaceOverlay
                                    Layout.alignment: Qt.AlignTop
                                    Image {
                                        id: siteIcon
                                        anchors.fill: parent; anchors.margins: 3
                                        source: "icons/sites/" + modelData.icon + ".png"
                                        fillMode: Image.PreserveAspectFit
                                        sourceSize.width: 42; sourceSize.height: 42
                                        scale: sHovered ? 1.1 : 1.0
                                        Behavior on scale { NumberAnimation { duration: 180; easing.type: Easing.OutBack } }
                                        onStatusChanged: {
                                            if (status === Image.Error) source = "icons/lucide/globe.svg"
                                        }
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    spacing: 3

                                    // 主标题：站点名称
                                    Text {
                                        text: modelData.name
                                        font.pixelSize: StyleTokens.fontSizeMd
                                        font.weight: Font.DemiBold
                                        color: StyleTokens.textPrimary
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true; Layout.minimumWidth: 0
                                    }

                                    // 副标题：网址
                                    Text {
                                        text: modelData.url
                                        font.pixelSize: StyleTokens.fontSizeXs
                                        color: StyleTokens.accentLink
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true; Layout.minimumWidth: 0
                                    }

                                    // 副标题：简介
                                    Text {
                                        text: modelData.desc || ""
                                        font.pixelSize: StyleTokens.fontSizeXs
                                        color: StyleTokens.textTertiary
                                        elide: Text.ElideRight
                                        maximumLineCount: 2
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true; Layout.minimumWidth: 0
                                        Layout.fillHeight: true
                                        verticalAlignment: Text.AlignTop
                                    }
                                }

                                // 悬停出现的「打开」箭头
                                Image {
                                    source: "icons/lucide/external-link.svg"
                                    width: 14; height: 14
                                    Layout.alignment: Qt.AlignVCenter
                                    opacity: sHovered ? 1.0 : 0.0
                                    Behavior on opacity { NumberAnimation { duration: 160 } }
                                    transform: Translate {
                                        id: sArrow
                                        x: sHovered ? 0 : -4
                                        Behavior on x { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }
                                    }
                                }
                            }

                            // 点击闪光反馈
                            Rectangle {
                                id: sFlash
                                anchors.fill: parent; radius: StyleTokens.radiusLg
                                color: StyleTokens.accentLight
                                opacity: 0
                            }
                            NumberAnimation {
                                id: sFlashScale
                                target: sFlash; property: "opacity"
                                to: 0; duration: 220; easing.type: Easing.OutCubic
                            }

                            MouseArea {
                                id: sHover
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onEntered: sCard.sHovered = true
                                onExited: sCard.sHovered = false
                                onPressedChanged: {
                                    sCard.sPressed = pressed
                                    if (pressed) {
                                        sFlash.opacity = 0.18
                                        sFlashScale.restart()
                                    }
                                }
                                onClicked: {
                                    if (modelData.url) root.openUrl(modelData.url)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
