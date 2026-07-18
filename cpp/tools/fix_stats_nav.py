import pathlib

p = pathlib.Path(r'D:/latest-code/cpp/qml/MainWindow.qml')
c = p.read_text('utf-8')

# 1. Fix navLabel(): add "stats" case
old_nav = '''    function navLabel(key) {
        switch (key) {
            case "home": return qsTr("\u542f\u52a8")
            case "download": return qsTr("\u4e0b\u8f7d")
            case "multiplayer": return qsTr("\u8054\u673a")
            case "settings": return qsTr("\u8bbe\u7f6e")
            case "download_progress": return qsTr("\u4e0b\u8f7d\u8fdb\u5ea6")
            default: return key
        }
    }'''
new_nav = '''    function navLabel(key) {
        switch (key) {
            case "home": return qsTr("\u542f\u52a8")
            case "download": return qsTr("\u4e0b\u8f7d")
            case "multiplayer": return qsTr("\u8054\u673a")
            case "stats": return qsTr("\u7edf\u8ba1")
            case "settings": return qsTr("\u8bbe\u7f6e")
            case "download_progress": return qsTr("\u4e0b\u8f7d\u8fdb\u5ea6")
            default: return key
        }
    }'''
c = c.replace(old_nav, new_nav)

# 2. Fix icon: "chart" -> "bar-chart-3"
c = c.replace('icon: "chart"', 'icon: "bar-chart-3"')

# 3. Insert StatsPage Rectangle after MultiplayerPage Rectangle
# Find the MultiplayerPage block
old_mp = '''                    Rectangle { anchors.fill: parent; visible: navListIndex === 2; color: hasCustomBg ? "transparent" : "#0c0f16"
                        Loader { asynchronous: true; anchors.fill: parent; active: true; source: "MultiplayerPage.qml"; onLoaded: { item.toastManager = toastManager } }
                    }'''
new_mp = '''                    Rectangle { anchors.fill: parent; visible: navListIndex === 2; color: hasCustomBg ? "transparent" : "#0c0f16"
                        Loader { asynchronous: true; anchors.fill: parent; active: true; source: "MultiplayerPage.qml"; onLoaded: { item.toastManager = toastManager } }
                    }

                    // ========== STATS ==========
                    Rectangle { anchors.fill: parent; visible: navListIndex === 3; color: hasCustomBg ? "transparent" : "#0c0f16"
                        opacity: navListIndex === 3 ? 1 : 0
                        Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
                        Loader { id: statsPageLoader; asynchronous: true; anchors.fill: parent; active: navListIndex === 3; source: "StatsPage.qml"
                            onLoaded: { if (item) item.onVisible() }
                        }
                    }
                    onNavListIndexChanged: { if (navListIndex === 3 && statsPageLoader.item) statsPageLoader.item.onVisible() }'''
c = c.replace(old_mp, new_mp)

# 4. Fix Settings page: visible: navListIndex === 3 -> 4
c = c.replace('visible: navListIndex === 3 || _settingsFadeOut; color: hasCustomBg ? "transparent" : "#0c0f16"',
              'visible: navListIndex === 4 || _settingsFadeOut; color: hasCustomBg ? "transparent" : "#0c0f16"')

# 5. Fix download progress Loader active from >=3 to >=4
c = c.replace('active: navListIndex >= 3; source: active ? "DownloadProgressPage.qml" : ""',
              'active: navListIndex >= 4; source: active ? "DownloadProgressPage.qml" : ""')

p.write_text(c, 'utf-8')
print('All 5 fixes applied to MainWindow.qml')
