import pathlib, re

# === MainWindow.qml ===
f = pathlib.Path(r'D:/latest-code/cpp/build/Release/qml/MainWindow.qml')
c = f.read_text('utf-8')

# Fix navLabel: shorten multiplayer label
c = c.replace('\u8054\u673a\uff08\u5185\u6d4b\uff09', '\u8054\u673a')  # 联机（内测）→ 联机

# Fix switchPage: multiline + add logUI call
old_switch = 'function switchPage(index) { if (navListIndex === 3 && index !== 3) _settingsFadeOut = true; if (navListIndex === 1 && index !== 1) _dlFadeOut = true; navListIndex = index; showVersionSelect = false; showVersionSettings = false; pageLoading = true; loadTimer.restart() }'
new_switch = 'function switchPage(index) {\n        if (navListIndex === 3 && index !== 3) _settingsFadeOut = true\n        if (navListIndex === 1 && index !== 1) _dlFadeOut = true\n        navListIndex = index\n        showVersionSelect = false\n        showVersionSettings = false\n        pageLoading = true\n        loadTimer.restart()\n        if (backend) backend.logUI(qsTr("\u8fdb\u5165\u9875\u9762 \u2014 ") + navLabel(navModel[index].pageKey))\n    }'
c = c.replace(old_switch, new_switch)
f.write_text(c, 'utf-8')
pathlib.Path(r'D:/latest-code/cpp/qml/MainWindow.qml').write_text(c, 'utf-8')
print('MainWindow.qml done')

# === DownloadPage.qml ===
dp = pathlib.Path(r'D:/latest-code/cpp/build/Release/qml/DownloadPage.qml')
dc = dp.read_text('utf-8')

# Add tab switch logging in onClicked of tab buttons
# Tab buttons set currentTab; search for "currentTab =" assignments
dc = dc.replace(
    'page.currentTab = 0',
    'page.currentTab = 0; if (backend) backend.logUI(qsTr("\u4e0b\u8f7d\u9875 \u2014 \u5207\u6362\u81f3 MC\u7248\u672c"))')
dc = dc.replace(
    'page.currentTab = 1',
    'page.currentTab = 1; if (backend) backend.logUI(qsTr("\u4e0b\u8f7d\u9875 \u2014 \u5207\u6362\u81f3 \u6a21\u7ec4"))')
dc = dc.replace(
    'page.currentTab = 2',
    'page.currentTab = 2; if (backend) backend.logUI(qsTr("\u4e0b\u8f7d\u9875 \u2014 \u5207\u6362\u81f3 \u5149\u5f71"))')
dc = dc.replace(
    'page.currentTab = 3',
    'page.currentTab = 3; if (backend) backend.logUI(qsTr("\u4e0b\u8f7d\u9875 \u2014 \u5207\u6362\u81f3 \u8d44\u6e90\u5305"))')
dp.write_text(dc, 'utf-8')
pathlib.Path(r'D:/latest-code/cpp/qml/DownloadPage.qml').write_text(dc, 'utf-8')
print('DownloadPage.qml done')

# === VersionSelectOverlay.qml: version selection ===
vs = pathlib.Path(r'D:/latest-code/cpp/build/Release/qml/VersionSelectOverlay.qml')
vsc = vs.read_text('utf-8')
# Find where a version is clicked/selected
# Search for "currentSelectedVersion =" assignment in onClicked
vsc = vsc.replace(
    'currentSelectedVersion = model.id',
    'currentSelectedVersion = model.id; if (backend) backend.logUI(qsTr("\u9009\u62e9\u7248\u672c\uff1a") + model.id)')
vs.write_text(vsc, 'utf-8')
pathlib.Path(r'D:/latest-code/cpp/qml/VersionSelectOverlay.qml').write_text(vsc, 'utf-8')
print('VersionSelectOverlay.qml done')

print('\nAll QML files updated.')
