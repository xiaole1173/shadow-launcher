// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: page
    color: "transparent"

    // Set by MainWindow loader onLoaded — carries the loaded autoLangMode combo index
    property int _initLangModeIdx: 0

    property int currentSection: 0

    opacity: 0; y: 10
    Behavior on opacity { NumberAnimation { duration: AnimationTokens.itemFadeInDuration; easing.type: AnimationTokens.itemFadeInEasing } }
    Behavior on y { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
    Component.onCompleted: { opacity = 1; y = 0; appWindow.pageLoading = false }

    // Called when switching sections — show loading bar
    function switchSection(idx) {
        if (idx === currentSection) return
        currentSection = idx
        if (appWindow) appWindow.pageLoading = true
        sectionLoadTimer.restart()
        console.info("[UI] 设置 切段 section=" + idx)
    }
    Timer {
        id: sectionLoadTimer
        interval: 60
        onTriggered: { appWindow.pageLoading = false }
    }

    RowLayout {
        anchors.fill: parent; spacing: 0

        // Left nav
        Rectangle {
            Layout.preferredWidth: 180; Layout.fillHeight: true; color: "transparent"

            ListView {
                id: nav; anchors.fill: parent; anchors.margins: 8

                function navLabel(key) {
                    switch (key) {
                        case "general": return qsTr("通用设置")
                        case "java": return qsTr("Java 设置")
                        case "memory": return qsTr("内存设置")
                        case "experimental": return qsTr("实验性功能")
                        case "about": return qsTr("关于")
                        default: return key
                    }
                }

                model: [
                    { label: "通用设置", icon: "settings", key: "general" },
                    { label: "Java 设置", icon: "terminal", key: "java" },
                    { label: "内存设置", icon: "cpu", key: "memory" },
                    { label: "实验性功能", icon: "flask-conical", key: "experimental" },
                    { label: "关于", icon: "info", key: "about" }
                ]
                currentIndex: 0; spacing: 2

                delegate: Rectangle {
                    width: nav.width - 16; height: 38; radius: StyleTokens.radiusMd
                    color: nav.currentIndex === index ? "#181c28" : (navMouse.containsMouse ? "#11141c" : "transparent")
                    Behavior on color { ColorAnimation { duration: 200; easing.type: Easing.OutCubic } }
                    scale: navMouse.containsMouse ? 1.02 : 1.0
                    Behavior on scale { NumberAnimation { duration: AnimationTokens.buttonDuration; easing.type: AnimationTokens.buttonEasing } }
                    Row {
                        anchors.left: parent.left; anchors.leftMargin: 14; anchors.verticalCenter: parent.verticalCenter
                        spacing: 8
                        Image {
                            anchors.verticalCenter: parent.verticalCenter
                            source: "icons/lucide/" + modelData.icon + ".svg"
                            width: 16; height: 16
                        }
                        Text {
                            text: nav.navLabel(modelData.key); color: nav.currentIndex === index ? "#e8ecf8" : "#8890a0"; font.pixelSize: StyleTokens.fontSizeMd
                            Behavior on color { ColorAnimation { duration: 200; easing.type: Easing.OutCubic } }
                            font.weight: nav.currentIndex === index ? Font.DemiBold : Font.Normal
                        }
                    }
                    MouseArea { id: navMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { nav.currentIndex = index; page.switchSection(index) } }
                }
            }

            Rectangle {
                id: settingsIndicator
                z: 10
                x: 8; y: 8 + nav.currentIndex * 40
                width: 2; height: 38; color: "#5d6fe0"; radius: StyleTokens.radiusXs
                Behavior on y { SmoothedAnimation { velocity: 200; duration: 300 } }
            }
        }

        // Divider
        Rectangle { Layout.preferredWidth: 1; Layout.fillHeight: true; color: StyleTokens.bgInput }

        // Right content — lazy-loaded sections
        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true; color: "transparent"

            // Section 0: General (loaded immediately, stays cached)
            Loader {
                id: generalLoader
                anchors.fill: parent; anchors.margins: 24
                active: true
                opacity: page.currentSection === 0 ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                sourceComponent: generalComponent
                asynchronous: true
            }

            // Section 1: Java (external file)
            Loader {
                id: javaLoader
                anchors.fill: parent; anchors.margins: 24
                active: true
                source: "SettingsJavaPage.qml"
                opacity: page.currentSection === 1 ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                asynchronous: false
                onItemChanged: {
                    if (item) {
                        item.backend = Qt.binding(function() { return backend })
                        item.toastManager = toastManager
                        item.refreshAll()
                    }
                }
            }

            // Section 2: Memory (lazy loaded, cached after first load)
            Loader {
                id: memoryLoader
                anchors.fill: parent; anchors.margins: 24
                active: true
                opacity: page.currentSection === 2 ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                sourceComponent: memoryComponent
                asynchronous: true
            }

            // Section 3: Experimental (lazy loaded, cached after first load)
            Loader {
                id: experimentalLoader
                anchors.fill: parent; anchors.margins: 24
                active: true
                opacity: page.currentSection === 3 ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                source: "SettingsExperimentalPage.qml"
                asynchronous: true
            }

            // Section 4: About (lazy loaded, cached after first load)
            Loader {
                id: aboutLoader
                anchors.fill: parent; anchors.margins: 24
                active: true
                opacity: page.currentSection === 4 ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                sourceComponent: aboutComponent
                asynchronous: true
            }
        }
    }

    // ────── COMPONENTS ──────

    Component {
        id: generalComponent
        Item {
            ColumnLayout {
                anchors.fill: parent; spacing: 12
                Text { text: qsTr("通用设置"); font.pixelSize: StyleTokens.fontSizeXl; font.bold: true; color: StyleTokens.textPrimary }

                Rectangle { Layout.fillWidth: true; height: 52; radius: StyleTokens.radiusMd; color: "#11141c"; border.color: StyleTokens.bgInput
                    RowLayout {
                        anchors.left: parent.left; anchors.leftMargin: 14
                        anchors.right: parent.right; anchors.rightMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 8
                        Text { text: qsTr("版本隔离"); font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.Medium; color: "#e8ecf8"; Layout.fillWidth: true }
                        Text { text: "已开启"; font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary }
                        ShadowSwitch { id: isolationSwitch; Layout.alignment: Qt.AlignVCenter; checked: true; enabled: false }
                    }
                }

                // ═══ 下载设置 ═══
                Text { text: qsTr("下载"); font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.DemiBold; color: "#b8c0d0"; Layout.topMargin: 8 }

                // ── 文件下载源 ──
                Text { text: qsTr("文件下载源"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary }
                Rectangle {
                    id: fileSrcCard
                    Layout.fillWidth: true
                    implicitHeight: fileSrcCard._open ? 148 : 40
                    radius: StyleTokens.radiusMd; color: "#11141c"; clip: true
                    border.color: fileSrcCard._open ? "#5068d8" : "#1a1f2a"; border.width: 1

                    Behavior on implicitHeight { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
                    Behavior on border.color { ColorAnimation { duration: 200 } }

                    property bool _open: false
                    property var items: [{t:qsTr("尽量使用镜像源"),v:0},{t:qsTr("尽量使用官方源"),v:1},{t:qsTr("尽量使用官方源，速度过慢切换镜像源"),v:2}]
                    property int _sel: (backend) ? backend.fileDownloadSource : 0
                    property string _label: _sel===0 ? items[0].t : (_sel===1 ? items[1].t : items[2].t)

                    Rectangle { width: parent.width; height: 40; color: "#0d1016"; radius: StyleTokens.radiusMd
                        RowLayout { anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 6
                            Text { Layout.fillWidth: true; text: fileSrcCard._label; color: "#b8c0d0"; font.pixelSize: StyleTokens.fontSizeMd; elide: Text.ElideRight }
                            Text { text: fileSrcCard._open ? "▲" : "▼"; color: "#8088a0"; font.pixelSize: StyleTokens.fontSizeXs }
                        }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: fileSrcCard._open = !fileSrcCard._open }
                    }

                    Column {
                        id: listCol; width: parent.width; y: 40
                        Repeater { model: fileSrcCard.items
                            delegate: Rectangle { width: listCol.width; height: 36; radius: StyleTokens.radiusSm
                                color: modelData.v===fileSrcCard._sel?"#5068d8":(m1.containsMouse?"#1e2540":"transparent")
                                Text { anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left; anchors.leftMargin: 16; text: modelData.t; color: modelData.v===fileSrcCard._sel?"#fff":"#b8c0d0"; font.pixelSize: StyleTokens.fontSizeSm }
                                MouseArea { id: m1; anchors.fill: parent; cursorShape: Qt.PointingHandCursor; hoverEnabled: true
                                    onClicked: { fileSrcCard._sel=modelData.v; if(backend) backend.fileDownloadSource=modelData.v; fileSrcCard._open=false }
                                }
                            }
                        }
                    }
                }

                // ── 版本列表源 ──
                Text { text: qsTr("版本列表源"); font.pixelSize: StyleTokens.fontSizeSm; color: "#8088a0"; Layout.topMargin: 8 }
                Rectangle {
                    id: listSrcCard
                    Layout.fillWidth: true
                    implicitHeight: listSrcCard._open2 ? 148 : 40
                    radius: StyleTokens.radiusMd; color: "#11141c"; clip: true
                    border.color: listSrcCard._open2 ? "#5068d8" : "#1a1f2a"; border.width: 1

                    Behavior on implicitHeight { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
                    Behavior on border.color { ColorAnimation { duration: 200 } }

                    property bool _open2: false
                    property var items: [{t:qsTr("尽量使用镜像源"),v:0},{t:qsTr("尽量使用官方源"),v:1},{t:qsTr("尽量使用官方源，速度过慢切换镜像源"),v:2}]
                    property int _sel: (backend) ? backend.listDownloadSource : 0
                    property string _label: _sel===0 ? items[0].t : (_sel===1 ? items[1].t : items[2].t)

                    Rectangle { width: parent.width; height: 40; color: "#0d1016"; radius: StyleTokens.radiusMd
                        RowLayout { anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 6
                            Text { Layout.fillWidth: true; text: listSrcCard._label; color: "#b8c0d0"; font.pixelSize: StyleTokens.fontSizeMd; elide: Text.ElideRight }
                            Text { text: listSrcCard._open2 ? "▲" : "▼"; color: "#8088a0"; font.pixelSize: StyleTokens.fontSizeXs }
                        }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: listSrcCard._open2 = !listSrcCard._open2 }
                    }

                    Column {
                        id: listCol2; width: parent.width; y: 40
                        Repeater { model: listSrcCard.items
                            delegate: Rectangle { width: listCol2.width; height: 36; radius: StyleTokens.radiusSm
                                color: modelData.v===listSrcCard._sel?"#5068d8":(m2.containsMouse?"#1e2540":"transparent")
                                Text { anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left; anchors.leftMargin: 16; text: modelData.t; color: modelData.v===listSrcCard._sel?"#fff":"#b8c0d0"; font.pixelSize: StyleTokens.fontSizeSm }
                                MouseArea { id: m2; anchors.fill: parent; cursorShape: Qt.PointingHandCursor; hoverEnabled: true
                                    onClicked: { listSrcCard._sel=modelData.v; if(backend) backend.listDownloadSource=modelData.v; listSrcCard._open2=false }
                                }
                            }
                        }
                    }
                }

                // ── 最大线程数 ──
                Text { id:thrLab; text:qsTr("最大线程数")+": "+thrSld._v; font.pixelSize:StyleTokens.fontSizeSm; color:"#8088a0"; Layout.topMargin:8 }
                Item {
                    id:thrSld
                    Layout.fillWidth:true; Layout.preferredHeight:28
                    property int _v: (backend) ? backend.maxDownloadThreads : 64
                    Rectangle { id:thrTrk; height:5; radius: StyleTokens.radiusXs; color:StyleTokens.bgElevated
                        anchors.verticalCenter:parent.verticalCenter; anchors.left:parent.left; anchors.leftMargin:8; anchors.right:parent.right; anchors.rightMargin:8
                        Rectangle { anchors.left:parent.left; anchors.top:parent.top; anchors.bottom:parent.bottom
                            width:thrTrk.width*((thrSld._v-1)/127); radius: StyleTokens.radiusXs; color:StyleTokens.accentHover }
                    }
                    Rectangle { id:thrHnd; width:14; height:14; radius: StyleTokens.radiusMd; color:"#fff"; border.width:2; border.color:StyleTokens.accentHover
                        anchors.verticalCenter:thrTrk.verticalCenter
                        x:thrTrk.x+thrTrk.width*((thrSld._v-1)/127)-7 }
                    MouseArea { anchors.fill:parent
                        function snp(v){return Math.max(1,Math.min(128,Math.round(v)))}
                        onPositionChanged:function(m){if(pressed){thrSld._v=snp(1+(m.x-thrTrk.x)/thrTrk.width*127);if(backend)backend.maxDownloadThreads=thrSld._v}}
                        onClicked:function(m){thrSld._v=snp(1+(m.x-thrTrk.x)/thrTrk.width*127);if(backend)backend.maxDownloadThreads=thrSld._v}
                    }
                }

                // ── 速度限制 ──
                Text { id:spdLab; text:qsTr("速度限制")+": "+(spdSld._s>=0?spdSld._s.toFixed(1)+" MB/s":qsTr("无限制")); font.pixelSize:StyleTokens.fontSizeSm; color:"#8088a0"; Layout.topMargin:8 }
                Item {
                    id:spdSld
                    Layout.fillWidth:true; Layout.preferredHeight:28
                    property double _s: (backend) ? backend.downloadSpeedLimitMB : -1
                    function s2i(v){if(v<0)return 21; if(v<=5)return Math.round(v*2); return 10+Math.round((v-5)/1.5)}
                    function i2s(i){if(i>=21)return -1; if(i<=10)return Math.max(0.1,i*0.5); return 5.0+(i-10)*1.5}
                    property int _i:s2i(_s)
                    Rectangle { id:spdTrk; height:5; radius: StyleTokens.radiusXs; color:StyleTokens.bgElevated
                        anchors.verticalCenter:parent.verticalCenter; anchors.left:parent.left; anchors.leftMargin:8; anchors.right:parent.right; anchors.rightMargin:8
                        Rectangle { anchors.left:parent.left; anchors.top:parent.top; anchors.bottom:parent.bottom
                            width:spdTrk.width*(spdSld._i/21); radius: StyleTokens.radiusXs; color:StyleTokens.accentHover }
                    }
                    Rectangle { id:spdHnd; width:14; height:14; radius: StyleTokens.radiusMd; color:"#fff"; border.width:2; border.color:StyleTokens.accentHover
                        anchors.verticalCenter:spdTrk.verticalCenter
                        x:spdTrk.x+spdTrk.width*(spdSld._i/21)-7 }
                    MouseArea { anchors.fill:parent
                        function snp(v){return Math.max(0,Math.min(21,Math.round(v)))}
                        onPositionChanged:function(m){if(pressed){spdSld._i=snp((m.x-spdTrk.x)/spdTrk.width*21);spdSld._s=spdSld.i2s(spdSld._i);if(backend)backend.downloadSpeedLimitMB=spdSld._s}}
                        onClicked:function(m){spdSld._i=snp((m.x-spdTrk.x)/spdTrk.width*21);spdSld._s=spdSld.i2s(spdSld._i);if(backend)backend.downloadSpeedLimitMB=spdSld._s}
                    }
                }

                Item { Layout.fillHeight: true }
            }
    }
}

    Component {
        id: memoryComponent
        Loader {
            anchors.fill: parent
            source: "SettingsMemorySection.qml"
            onItemChanged: {
                if (item) {
                    item.backend = backend
                    item.refreshAll()
                }
            }
        }
    }


    Component {
        id: aboutComponent
        Flickable {
            id: aboutFlick
            anchors.fill: parent
            contentHeight: aboutColumn.implicitHeight
            flickableDirection: Flickable.VerticalFlick
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ColumnLayout {
                id: aboutColumn
                anchors.left: parent.left; anchors.right: parent.right
                spacing: 14
                Text { text: qsTr("关于"); font.pixelSize: StyleTokens.fontSizeXl; font.bold: true; color: StyleTokens.textPrimary }

                // About card
                Rectangle {
                    Layout.fillWidth: true; radius: StyleTokens.radiusLg; color: "#11141c"; border.color: StyleTokens.bgInput
                    Layout.preferredHeight: aboutCardContent.height + 34
                    ColumnLayout {
                        id: aboutCardContent
                        anchors.left: parent.left; anchors.right: parent.right
                        anchors.top: parent.top; anchors.margins: 17; spacing: 8
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "Shadow Launcher"; font.pixelSize: StyleTokens.fontSizeLg; font.bold: true; color: StyleTokens.textPrimary }
                            Item { Layout.fillWidth: true }
                            Text { text: backend ? backend.appVersion : ""; font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textMuted }
                        }
                        RowLayout {
                            Layout.fillWidth: true; spacing: 8
                            ShadowButton {
                                accentColor: "#2d3748"
                                text: qsTr("GitHub")
                                Layout.preferredWidth: 74; Layout.preferredHeight: 26
                                font.pixelSize: StyleTokens.fontSizeSm
                                onClicked: Qt.openUrlExternally("https://github.com/xiaole1173/shadow-launcher")
                            }
                            ShadowButton {
                                accentColor: "#1a2848"
                                text: backend && backend.updateChecking ? qsTr("检查中...") : qsTr("检查更新")
                                Layout.preferredWidth: 74; Layout.preferredHeight: 26
                                font.pixelSize: StyleTokens.fontSizeSm
                                enabled: backend && !backend.updateChecking
                                onClicked: {
                                    if (backend) backend.checkForUpdate()
                                }
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("(C) 2025-2026 影 / Shadow / xiaole1173")
                            font.pixelSize: StyleTokens.fontSizeSm; color: "#707888"
                        }
                        ShadowButton {
                            accentColor: "transparent"
                            text: qsTr("GNU AGPL v3 许可证")
                            Layout.preferredWidth: 130; Layout.preferredHeight: 24
                            font.pixelSize: StyleTokens.fontSizeSm
                            onClicked: Qt.openUrlExternally("https://github.com/xiaole1173/shadow-launcher/blob/master/LICENSE")
                        }
                        Text {
                            Layout.fillWidth: true; wrapMode: Text.WordWrap
                            text: qsTr("非 Minecraft 官方产品。未经 Mojang 或 Microsoft 批准，也不与 Mojang 或 Microsoft 关联。")
                            font.pixelSize: StyleTokens.fontSizeSm; color: "#707888"; lineHeight: 1.4
                        }
                    }
                }



                // Acknowledgments card
                Rectangle {
                    Layout.fillWidth: true; radius: StyleTokens.radiusLg; color: "#11141c"; border.color: StyleTokens.bgInput
                    Layout.preferredHeight: ackContent.height + 34
                    ColumnLayout {
                        id: ackContent
                        anchors.left: parent.left; anchors.right: parent.right
                        anchors.top: parent.top; anchors.margins: 17; spacing: 12
                        Text { text: qsTr("鸣谢"); font.pixelSize: StyleTokens.fontSizeXl; font.bold: true; color: StyleTokens.textPrimary }
                        Column { Layout.fillWidth: true; spacing: 14
                            Item {
                                width: parent.width; height: ack1Desc.y + ack1Desc.height
                                Text { id: ack1Name; text: "bangbang93"; font.pixelSize: StyleTokens.fontSizeMd; font.bold: true; color: "#e8ecf8"; width: parent.width }
                                Text { id: ack1Desc; text: qsTr("提供的 BMCLAPI 极大程度上解决了各类官方源的逆天下载速度问题！包括 Minecraft、Forge、Neoforge、Optifine 等等。"); font.pixelSize: StyleTokens.fontSizeSm; color: "#8890a0"; width: parent.width; wrapMode: Text.WordWrap; lineHeight: 1.35; anchors.top: ack1Name.bottom; anchors.topMargin: 3 }
                            }
                            Item {
                                width: parent.width; height: ack2Desc.y + ack2Desc.height
                                Text { id: ack2Name; text: "z0z0r4"; font.pixelSize: StyleTokens.fontSizeMd; font.bold: true; color: "#e8ecf8"; width: parent.width }
                                Text { id: ack2Desc; text: qsTr("提供的 MCIM 解决了直连 Modrinth 的逆天下载速度问题。"); font.pixelSize: StyleTokens.fontSizeSm; color: "#8890a0"; width: parent.width; wrapMode: Text.WordWrap; lineHeight: 1.35; anchors.top: ack2Name.bottom; anchors.topMargin: 3 }
                            }
                            Item {
                                width: parent.width; height: ack3Desc.y + ack3Desc.height
                                Text { id: ack3Name; text: "Lucide"; font.pixelSize: StyleTokens.fontSizeMd; font.bold: true; color: "#e8ecf8"; width: parent.width }
                                Text { id: ack3Desc; text: qsTr("提供了启动器目前可见的所有图标！"); font.pixelSize: StyleTokens.fontSizeSm; color: "#8890a0"; width: parent.width; wrapMode: Text.WordWrap; lineHeight: 1.35; anchors.top: ack3Name.bottom; anchors.topMargin: 3 }
                            }
                            Item {
                                width: parent.width; height: ack4Desc.y + ack4Desc.height
                                Text { id: ack4Name; text: "ChunMoMo"; font.pixelSize: StyleTokens.fontSizeMd; font.bold: true; color: "#e8ecf8"; width: parent.width }
                                Text { id: ack4Desc; text: qsTr("提供了测试联机功能的电脑，将我从想测试联机功能但找不到两台电脑的水深火热中救了出来......"); font.pixelSize: StyleTokens.fontSizeSm; color: "#8890a0"; width: parent.width; wrapMode: Text.WordWrap; lineHeight: 1.35; anchors.top: ack4Name.bottom; anchors.topMargin: 3 }
                            }
                        }
                    }
                }

                // Launcher Logs card
                Rectangle {
                    Layout.fillWidth: true; radius: StyleTokens.radiusLg; color: "#11141c"; border.color: StyleTokens.bgInput
                    Layout.preferredHeight: logsRow.height + 24
                    RowLayout {
                        id: logsRow
                        anchors.left: parent.left; anchors.right: parent.right
                        anchors.top: parent.top; anchors.margins: 12
                        spacing: 10
                        Image {
                            source: "icons/lucide/folder-open.svg"; width: 18; height: 18
                        }
                        Text {
                            text: qsTr("启动器日志"); font.pixelSize: StyleTokens.fontSizeMd; color: StyleTokens.textPrimary
                            Layout.fillWidth: true
                        }
                        ShadowButton {
                            accentColor: "#2d3748"; text: qsTr("打开文件夹")
                            Layout.preferredHeight: 26; font.pixelSize: StyleTokens.fontSizeSm
                            onClicked: backend.openLauncherLogsFolder()
                        }
                    }
                }
            }
        }
    }

    // ── Background crop overlay ──
    Loader {
        id: bgCropOverlay
        active: false
        sourceComponent: bgCropComp
        anchors.fill: parent; z: 10
    }
    Component {
        id: bgCropComp
        BackgroundCropOverlay {
            anchors.fill: parent
            cropAR: appWindow ? (appWindow.width / appWindow.height) : 1.778
            onClosed: bgCropOverlay.active = false
        }
    }

    // ── Update check connections ──
    Connections {
        target: typeof backend !== "undefined" ? backend : null
        function onToastMessage(message) {
            toastManager.show(message)
        }
    }
}
