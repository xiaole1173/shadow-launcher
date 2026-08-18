// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "websites_data.js" as SitesData

// ═══ 实用网站页 ═══
// 两级界面：
//   0) 大类卡片（含「本启动器」大类），数据源 qml/websites_data.js（由 websites_data.json 生成）
//   1) 点击大类 → 该大类的网站卡片（主标题=站点名，副标题=网址+简介），点击直达浏览器
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

    function loadData() {
        try {
            categories = SitesData.data.categories || []
            dataReady = true
            console.info("[sites] websites_data.js imported, categories=" + categories.length)
        } catch (e) {
            console.warn("[sites] import websites_data.js failed:", e)
        }
    }

    Component.onCompleted: {
        loadData()
        pageEnterAnim.start()
    }

    function openUrl(url) {
        Qt.openUrlExternally(url)
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

            // ═══ 视图 0：大类卡片 ═══
            GridView {
                id: catGrid
                anchors.fill: parent
                visible: root.currentCategory < 0
                clip: true
                model: root.categories
                // 两列自适应（含 8px 卡片间距）
                cellWidth: Math.max(240, (catGrid.width - 8) / 2)
                cellHeight: 96
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: Rectangle {
                    id: cCard
                    width: catGrid.cellWidth - 8
                    height: catGrid.cellHeight - 8
                    radius: StyleTokens.radiusLg
                    color: cHover.hovered ? StyleTokens.bgHover : StyleTokens.bgCard
                    border.color: cHover.hovered ? StyleTokens.accent : StyleTokens.bgInput
                    border.width: 1
                    scale: cHover.hovered ? 1.02 : 1.0
                    Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }
                    Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                    Behavior on border.color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 14; anchors.rightMargin: 12
                        anchors.topMargin: 12; anchors.bottomMargin: 12
                        spacing: 12

                        Rectangle {
                            width: 42; height: 42; radius: StyleTokens.radiusMd
                            color: StyleTokens.surfaceOverlay
                            Layout.alignment: Qt.AlignVCenter
                            Image {
                                anchors.fill: parent; anchors.margins: 9
                                source: "icons/lucide/" + (modelData.icon || "globe") + ".svg"
                                fillMode: Image.PreserveAspectFit
                                sourceSize.width: 24; sourceSize.height: 24
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            Text {
                                text: modelData.title
                                font.pixelSize: StyleTokens.fontSizeMd
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

                        Image {
                            source: "icons/lucide/chevron-right.svg"
                            width: 16; height: 16
                            opacity: cHover.hovered ? 1.0 : 0.55
                            Behavior on opacity { NumberAnimation { duration: 150 } }
                            Layout.alignment: Qt.AlignVCenter
                        }
                    }

                    MouseArea {
                        id: cHover
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.currentCategory = index
                    }
                }
            }

            // ═══ 视图 1：网站卡片 ═══
            GridView {
                id: siteGrid
                anchors.fill: parent
                visible: root.currentCategory >= 0
                clip: true
                model: root.currentCategory >= 0 && root.currentCategory < root.categories.length
                       ? root.categories[root.currentCategory].sites : []
                cellWidth: Math.max(280, (siteGrid.width - 8) / 2)
                cellHeight: 116
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: Rectangle {
                    id: sCard
                    width: siteGrid.cellWidth - 8
                    height: siteGrid.cellHeight - 8
                    radius: StyleTokens.radiusLg
                    color: sHover.hovered ? StyleTokens.bgHover : StyleTokens.bgCard
                    border.color: sHover.hovered ? StyleTokens.accent : StyleTokens.bgInput
                    border.width: 1
                    scale: sHover.hovered ? 1.02 : 1.0
                    Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }
                    Behavior on color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }
                    Behavior on border.color { ColorAnimation { duration: AnimationTokens.colorDuration; easing.type: AnimationTokens.buttonEasing } }

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 12

                        // 网站 favicon
                        Rectangle {
                            width: 46; height: 46; radius: StyleTokens.radiusMd
                            color: StyleTokens.surfaceOverlay
                            Layout.alignment: Qt.AlignTop
                            Image {
                                id: siteIcon
                                anchors.fill: parent; anchors.margins: 3
                                source: "icons/sites/" + modelData.icon + ".png"
                                fillMode: Image.PreserveAspectFit
                                sourceSize.width: 40; sourceSize.height: 40
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
                    }

                    MouseArea {
                        id: sHover
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (modelData.url) root.openUrl(modelData.url)
                        }
                    }
                }
            }
        }
    }
}
