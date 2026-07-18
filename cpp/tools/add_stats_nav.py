import pathlib

p = pathlib.Path(r'D:/latest-code/cpp/qml/MainWindow.qml')
c = p.read_text('utf-8')

# Insert stats page after multiplayer page (index 2), before settings page
old_block = '''                    Rectangle { anchors.fill: parent; visible: navListIndex === 2; color: hasCustomBg ? "transparent" : "#0c0f16"
                        Loader { asynchronous: true; anchors.fill: parent; active: true; source: "MultiplayerPage.qml"; onLoaded: { item.toastManager = toastManager } }
                    }


                    Rectangle { anchors.fill: parent; visible: navListIndex === 3 || _settingsFadeOut;'''

new_block = '''                    Rectangle { anchors.fill: parent; visible: navListIndex === 2; color: hasCustomBg ? "transparent" : "#0c0f16"
                        Loader { asynchronous: true; anchors.fill: parent; active: true; source: "MultiplayerPage.qml"; onLoaded: { item.toastManager = toastManager } }
                    }

                    // Stats page
                    Rectangle { anchors.fill: parent; visible: navListIndex === 3; color: hasCustomBg ? "transparent" : "#0c0f16"
                        opacity: navListIndex === 3 ? 1 : 0
                        Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                        Loader { id: statsPageLoader; asynchronous: true; anchors.fill: parent; active: navListIndex === 3; source: "StatsPage.qml"
                            onLoaded: { if (item) item.onVisible() }
                        }
                    }
                    onNavListIndexChanged: { if (navListIndex === 3 && statsPageLoader.item) statsPageLoader.item.onVisible() }


                    Rectangle { anchors.fill: parent; visible: navListIndex === 4 || _settingsFadeOut;'''

c = c.replace(old_block, new_block)

# Update settings opacity check from 3 to 4
c = c.replace('opacity: navListIndex === 3 ? 1 : 0', 'opacity: navListIndex === 4 ? 1 : 0')

# Update settings Loader active check from 3 to 4
c = c.replace('active: navListIndex === 3 || _settingsFadeOut', 'active: navListIndex === 4 || _settingsFadeOut')

# Update download progress guard from >= 3 to >= 4
c = c.replace('navListIndex >= 3 && navModel.get(navListIndex).pageKey', 'navListIndex >= 4 && navModel.get(navListIndex).pageKey')

p.write_text(c, 'utf-8')
print('MainWindow.qml updated successfully')
