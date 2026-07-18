// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: page
    color: "transparent"

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
                sourceComponent: experimentalComponent
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
                        Switch { id: isolationSwitch; Layout.alignment: Qt.AlignVCenter; checked: true; enabled: false
                            palette { mid: "#3a4eb8"; dark: "#252835"; light: "#5d6fe0"; button: checked ? "#5d6fe0" : "#353845" }
                            opacity: 0.5
                        }
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
        id: experimentalComponent
        Flickable {
            anchors.fill: parent
            contentWidth: width
            contentHeight: expCol.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            property bool hasBg: backend ? (typeof backend.customBgPath === "string" && backend.customBgPath.length > 0) : false
            ColumnLayout {
                id: expCol
                width: parent.width
                spacing: 12
                Text { text: qsTr("实验性功能"); font.pixelSize: StyleTokens.fontSizeXl; font.bold: true; color: StyleTokens.textPrimary }
                Text { text: qsTr("以下功能处于实验阶段，可能存在不稳定的情况。"); font.pixelSize: StyleTokens.fontSizeSm; color: "#707888"; wrapMode: Text.WordWrap; Layout.fillWidth: true }

                Rectangle {
                    Layout.fillWidth: true; Layout.preferredHeight: 110; radius: StyleTokens.radiusLg; color: "#11141c"; border.color: StyleTokens.bgInput
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 14; spacing: 8
                        RowLayout {
                            Layout.fillWidth: true; spacing: 6
                            Text { text: qsTr("嵌入式窗口登录"); font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.DemiBold; color: StyleTokens.textPrimary }
                            Rectangle { radius: StyleTokens.radiusSm; height: 18; width: tagText.implicitWidth + 10; color: StyleTokens.bgHover
                                Text { id: tagText; anchors.centerIn: parent; text: qsTr("实验性"); font.pixelSize: StyleTokens.fontSizeXs; color: "#a098e0" }
                            }
                            Item { Layout.fillWidth: true }
                            Text { text: embeddedSwitch.checked ? qsTr("已开启") : qsTr("已关闭"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary }
                            Switch {
                                id: embeddedSwitch; Layout.alignment: Qt.AlignVCenter
                                checked: backend ? backend.embeddedLoginEnabled : false
                                onToggled: { if (backend) backend.setEmbeddedLoginEnabled(checked) }
                                palette { mid: "#3a4eb8"; dark: "#252835"; light: "#5d6fe0"; button: checked ? "#5d6fe0" : "#353845" }
                            }
                        }
                        Text {
                            text: qsTr("在启动器内嵌窗口中完成 Microsoft 登录，无需跳转外部浏览器。\n输入 Windows 已登录的 Microsoft 账户后，会自动唤起系统 PIN/指纹验证窗口，可快捷登录。")
                            font.pixelSize: StyleTokens.fontSizeSm; color: "#707888"; wrapMode: Text.WordWrap; Layout.fillWidth: true
                        }
                    }
                }

                // ── Language selector ──
                Rectangle {
                    Layout.fillWidth: true; Layout.preferredHeight: 110; radius: StyleTokens.radiusLg; color: "#11141c"; border.color: StyleTokens.bgInput
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 14; spacing: 8
                        RowLayout {
                            Layout.fillWidth: true; spacing: 6
                            Text { text: qsTr("界面语言"); font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.DemiBold; color: StyleTokens.textPrimary }
                            Rectangle { radius: StyleTokens.radiusSm; height: 18; width: langTag.implicitWidth + 10; color: StyleTokens.bgHover
                                Text { id: langTag; anchors.centerIn: parent; text: qsTr("实验性"); font.pixelSize: StyleTokens.fontSizeXs; color: "#a098e0" }
                            }
                            Item { Layout.fillWidth: true }
                            ComboBox {
                                id: langCombo
                                model: ["简体中文（中国大陆）", "繁體中文（香港特別行政區 / 澳門特別行政區）", "繁體中文（中國台灣）"]
                                property bool _syncBack: false
                                Component.onCompleted: {
                                    _syncBack = true
                                    currentIndex = (backend && backend.readLanguageFile) ? backend.readLanguageFile() : 0
                                    _syncBack = false
                                }
                                onActivated: {
                                    if (_syncBack) return
                                    if (backend) backend.switchLanguage(currentIndex)
                                }
                                Layout.preferredWidth: 280

                                // ── Custom dark style ──
                                background: Rectangle {
                                    radius: StyleTokens.radiusMd; color: langCombo.hovered ? "#252a38" : "#161a24"
                                    border.color: langCombo.hovered ? "#3a4eb8" : "#1e2230"
                                    border.width: 1
                                    Behavior on color { ColorAnimation { duration: 200 } }
                                    Behavior on border.color { ColorAnimation { duration: 200 } }
                                }
                                contentItem: Text {
                                    leftPadding: 12; verticalAlignment: Text.AlignVCenter
                                    text: langCombo.displayText; color: "#d0d4e0"; font.pixelSize: StyleTokens.fontSizeSm
                                    elide: Text.ElideRight
                                }
                                indicator: Canvas {
                                    width: 12; height: 12; anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter
                                    onPaint: {
                                        var ctx = getContext("2d")
                                        ctx.strokeStyle = langCombo.hovered ? "#8088f0" : "#606478"
                                        ctx.lineWidth = 1.5
                                        ctx.beginPath(); ctx.moveTo(0, 3); ctx.lineTo(6, 9); ctx.lineTo(12, 3); ctx.stroke()
                                    }
                                }
                                delegate: ItemDelegate {
                                    width: langCombo.popup.width  // use popup width
                                    contentItem: Text {
                                        text: modelData; color: "#d0d4e0"; font.pixelSize: StyleTokens.fontSizeSm
                                        verticalAlignment: Text.AlignVCenter; leftPadding: 12
                                    }
                                    background: Rectangle {
                                        color: highlighted ? "#252a38" : "transparent"
                                        Behavior on color { ColorAnimation { duration: 150 } }
                                    }
                                    highlighted: langCombo.highlightedIndex === index
                                }
                                popup: Popup {
                                    y: langCombo.height + 4
                                    width: 380
                                    implicitHeight: contentItem.implicitHeight + 8
                                    padding: 4
                                    contentItem: ListView {
                                        clip: true; implicitHeight: contentHeight
                                        model: langCombo.popup.visible ? langCombo.delegateModel : null
                                        currentIndex: langCombo.highlightedIndex
                                    }
                                    background: Rectangle {
                                        radius: StyleTokens.radiusMd; color: "#161a24"; border.color: StyleTokens.bgElevated
                                    }
                                }
                            }
                        }
                        Text {
                            text: qsTr("选择语言后即时生效，无需重启启动器。")
                            font.pixelSize: StyleTokens.fontSizeSm; color: "#707888"; wrapMode: Text.WordWrap; Layout.fillWidth: true
                        }
                    }
                }

                // ── MC auto-language ──
                Rectangle {
                    Layout.fillWidth: true; Layout.preferredHeight: langModeCard.implicitHeight + 28
                    radius: StyleTokens.radiusLg; color: "#11141c"; border.color: StyleTokens.bgInput
                    ColumnLayout {
                        id: langModeCard
                        anchors.fill: parent; anchors.margins: 14; spacing: 6
                        RowLayout {
                            Layout.fillWidth: true; spacing: 6
                            Text { text: qsTr("游戏语言自动调整"); font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.DemiBold; color: StyleTokens.textPrimary }
                            Rectangle { radius: StyleTokens.radiusSm; height: 18; width: langNewTag.implicitWidth + 10; color: "#2a3a5c"
                                Text { id: langNewTag; anchors.centerIn: parent; text: qsTr("新增"); font.pixelSize: StyleTokens.fontSizeXs; color: "#70b8ff" }
                            }
                            Item { Layout.fillWidth: true }
                            ComboBox {
                                id: langModeCombo
                                model: [qsTr("系统区域（启动时自动检测）"), qsTr("IP 属地（安装时自动调整）"), qsTr("关闭（手动设置）")]
                                property bool _syncLangMode: false
                                Component.onCompleted: {
                                    _syncLangMode = true
                                    currentIndex = backend && backend.settings ? backend.settings.autoLangMode : 1
                                    _syncLangMode = false
                                }
                                onActivated: {
                                    if (_syncLangMode) return
                                    if (backend && backend.settings)
                                        backend.settings.autoLangMode = currentIndex
                                }
                                Layout.preferredWidth: 280

                                background: Rectangle {
                                    radius: StyleTokens.radiusMd; color: langModeCombo.hovered ? "#252a38" : "#161a24"
                                    border.color: langModeCombo.hovered ? "#3a4eb8" : "#1e2230"
                                    border.width: 1
                                    Behavior on color { ColorAnimation { duration: 200 } }
                                    Behavior on border.color { ColorAnimation { duration: 200 } }
                                }
                                contentItem: Text {
                                    leftPadding: 12; verticalAlignment: Text.AlignVCenter
                                    text: langModeCombo.displayText; color: "#d0d4e0"; font.pixelSize: StyleTokens.fontSizeSm
                                    elide: Text.ElideRight
                                }
                                indicator: Canvas {
                                    width: 12; height: 12; anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter
                                    onPaint: {
                                        var ctx = getContext("2d")
                                        ctx.strokeStyle = langModeCombo.hovered ? "#8088f0" : "#606478"
                                        ctx.lineWidth = 1.5
                                        ctx.beginPath(); ctx.moveTo(0, 3); ctx.lineTo(6, 9); ctx.lineTo(12, 3); ctx.stroke()
                                    }
                                }
                                delegate: ItemDelegate {
                                    width: langModeCombo.popup.width
                                    contentItem: Text {
                                        text: modelData; color: "#d0d4e0"; font.pixelSize: StyleTokens.fontSizeSm
                                        verticalAlignment: Text.AlignVCenter; leftPadding: 12
                                    }
                                    background: Rectangle {
                                        color: highlighted ? "#252a38" : "transparent"
                                        Behavior on color { ColorAnimation { duration: 150 } }
                                    }
                                    highlighted: langModeCombo.highlightedIndex === index
                                }
                                popup: Popup {
                                    y: langModeCombo.height + 4
                                    width: 380
                                    implicitHeight: contentItem.implicitHeight + 8
                                    padding: 4
                                    contentItem: ListView {
                                        clip: true; implicitHeight: contentHeight
                                        model: langModeCombo.popup.visible ? langModeCombo.delegateModel : null
                                        currentIndex: langModeCombo.highlightedIndex
                                    }
                                    background: Rectangle {
                                        radius: StyleTokens.radiusMd; color: "#161a24"; border.color: StyleTokens.bgElevated
                                    }
                                }
                            }
                        }
                        Text {
                            text: {
                                switch (langModeCombo.currentIndex) {
                                    case 0: return qsTr("每次启动游戏时根据电脑系统区域自动写入 options.txt") + "（默认识别 zh_CN / zh_HK / zh_TW / ja_JP / ko_KR）"
                                    case 1: return qsTr("安装版本后根据 IP 属地自动写入 options.txt，启动时不覆盖")
                                    case 2: return qsTr("不自动调整游戏语言，可在 Minecraft 游戏设置中手动选择")
                                    default: return ""
                                }
                            }
                            font.pixelSize: StyleTokens.fontSizeSm; color: "#707888"; wrapMode: Text.WordWrap; Layout.fillWidth: true
                        }
                    }
                }

                // ── Custom background ──
                Rectangle {
                    Layout.fillWidth: true; Layout.preferredHeight: bgCardContent.implicitHeight + 28
                    radius: StyleTokens.radiusLg; color: "#11141c"; border.color: StyleTokens.bgInput
                    ColumnLayout {
                        id: bgCardContent
                        anchors.fill: parent; anchors.margins: 14; spacing: 8
                        RowLayout {
                            Layout.fillWidth: true; spacing: 6
                            Text { text: qsTr("自定义背景"); font.pixelSize: StyleTokens.fontSizeMd; font.weight: Font.DemiBold; color: StyleTokens.textPrimary }
                            Rectangle { radius: StyleTokens.radiusSm; height: 18; width: bgTag.implicitWidth + 10; color: StyleTokens.bgHover
                                Text { id: bgTag; anchors.centerIn: parent; text: qsTr("实验性"); font.pixelSize: StyleTokens.fontSizeXs; color: "#a098e0" }
                            }
                            Item { Layout.fillWidth: true }
                            Text { text: hasBg ? qsTr("已设置") : qsTr("未设置"); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textTertiary }
                            Rectangle { width: 60; height: 28; radius: StyleTokens.radiusSm; color: bgBrowseHov.hovered ? "#252a38" : "#161a24"; border.color: StyleTokens.bgHover
                                scale: bgBrowseHov.containsMouse ? 1.05 : 1.0
                                Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                                Text { anchors.centerIn: parent; text: qsTr("浏览"); font.pixelSize: StyleTokens.fontSizeSm; color: "#d0d4e0" }
                                MouseArea { id: bgBrowseHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                    onClicked: { if (backend) backend.pickBackgroundImage() }
                                }
                            }
                            Rectangle { visible: hasBg; width: 60; height: 28; radius: StyleTokens.radiusSm; color: bgClearHov.hovered ? "#302020" : "#161a24"; border.color: StyleTokens.bgHover
                                scale: bgClearHov.containsMouse ? 1.05 : 1.0
                                Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                                Text { anchors.centerIn: parent; text: qsTr("清除"); font.pixelSize: StyleTokens.fontSizeSm; color: "#d08080" }
                                MouseArea { id: bgClearHov; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                    onClicked: { if (backend) backend.setCustomBgPath("") }
                                }
                            }
                        }

                        // Preview thumbnail (click to adjust crop)
                        Image {
                            id: bgPreview
                            Layout.fillWidth: true; Layout.preferredHeight: 100
                            visible: hasBg
                            source: hasBg ? backend.customBgPath : ""
                            fillMode: Image.PreserveAspectFit
                            cache: false; asynchronous: true
                        }
                        MouseArea {
                            anchors.fill: bgPreview; visible: hasBg
                            hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            onClicked: { bgCropOverlay.active = true }
                        }
                        Rectangle {
                            opacity: hasBg ? 0 : 0
                            anchors.fill: bgPreview
                            color: "transparent"; border.color: "#3a5ed0"; border.width: 1.5; radius: StyleTokens.radiusSm
                        }
                        Rectangle { visible: hasBg; Layout.fillWidth: true; height: 1; color: StyleTokens.bgInput }

                        // Hint text
                        Text {
                            visible: hasBg; Layout.fillWidth: true; wrapMode: Text.WordWrap
                            font.pixelSize: StyleTokens.fontSizeXs; color: "#606480"
                            text: qsTr("当前预览的是所选中的整个图片，而非裁剪后的图片。点击图片，可手动更改选区范围。")
                        }

                        // Sliders
                        RowLayout {
                            visible: hasBg
                            Layout.fillWidth: true; spacing: 16
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 4
                                Text { text: qsTr("菜单栏透明度"); font.pixelSize: StyleTokens.fontSizeSm; color: "#b0b8c8" }
                                RowLayout { Layout.fillWidth: true; spacing: 8
                                    Slider {
                                        id: sidebarSPSlider
                                        Layout.fillWidth: true; from: 0.40; to: 1.0; stepSize: 0.05
                                        value: backend ? backend.sidebarOpacity : 0.90
                                        onMoved: { if (backend) backend.setSidebarOpacity(value) }
                                        background: Rectangle {
                                            implicitHeight: 4
                                            x: sidebarSPSlider.leftPadding
                                            y: sidebarSPSlider.topPadding + sidebarSPSlider.availableHeight / 2 - height / 2
                                            width: sidebarSPSlider.availableWidth; height: 4; radius: StyleTokens.radiusXs; color: StyleTokens.bgHover
                                            Rectangle {
                                                width: sidebarSPSlider.visualPosition * parent.width; height: 4; radius: StyleTokens.radiusXs; color: "#3a4eb8"
                                            }
                                        }
                                        handle: Rectangle {
                                            implicitWidth: 12; implicitHeight: 12
                                            x: sidebarSPSlider.leftPadding + sidebarSPSlider.visualPosition * (sidebarSPSlider.availableWidth - width)
                                            y: sidebarSPSlider.topPadding + sidebarSPSlider.availableHeight / 2 - height / 2
                                            radius: StyleTokens.radiusMd; color: StyleTokens.accentLight
                                        }
                                    }
                                    Text { text: (backend ? backend.sidebarOpacity : 0.90).toFixed(2); font.pixelSize: StyleTokens.fontSizeSm; color: "#a0a8c0"; Layout.preferredWidth: 32 }
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 4
                                Text { text: qsTr("背景可见度"); font.pixelSize: StyleTokens.fontSizeSm; color: "#b0b8c8" }
                                RowLayout { Layout.fillWidth: true; spacing: 8
                                    Slider {
                                        id: contentSPSlider
                                        Layout.fillWidth: true; from: 0.40; to: 1.0; stepSize: 0.05
                                        value: backend ? backend.contentOpacity : 0.70
                                        onMoved: { if (backend) backend.setContentOpacity(value) }
                                        background: Rectangle {
                                            implicitHeight: 4
                                            x: contentSPSlider.leftPadding
                                            y: contentSPSlider.topPadding + contentSPSlider.availableHeight / 2 - height / 2
                                            width: contentSPSlider.availableWidth; height: 4; radius: StyleTokens.radiusXs; color: StyleTokens.bgHover
                                            Rectangle {
                                                width: contentSPSlider.visualPosition * parent.width; height: 4; radius: StyleTokens.radiusXs; color: "#3a4eb8"
                                            }
                                        }
                                        handle: Rectangle {
                                            implicitWidth: 12; implicitHeight: 12
                                            x: contentSPSlider.leftPadding + contentSPSlider.visualPosition * (contentSPSlider.availableWidth - width)
                                            y: contentSPSlider.topPadding + contentSPSlider.availableHeight / 2 - height / 2
                                            radius: StyleTokens.radiusMd; color: StyleTokens.accentLight
                                        }
                                    }
                                    Text { text: (backend ? backend.contentOpacity : 0.70).toFixed(2); font.pixelSize: StyleTokens.fontSizeSm; color: "#a0a8c0"; Layout.preferredWidth: 32 }
                                }
                            }
                        }

                        Text {
                            text: qsTr("选择一张图片作为启动器背景。菜单栏和背景可见度可分别调节。")
                            font.pixelSize: StyleTokens.fontSizeSm; color: "#707888"; wrapMode: Text.WordWrap; Layout.fillWidth: true
                        }
                    }
                }

                Item { Layout.fillHeight: true }
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
                            Text { text: "v" + (backend ? backend.appVersion : ""); font.pixelSize: StyleTokens.fontSizeSm; color: StyleTokens.textMuted }
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
