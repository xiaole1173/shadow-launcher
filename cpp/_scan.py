import os, re

# Check ShadowDropdown
c = open('qml/ShadowDropdown.qml','r',encoding='utf-8').read()
print('=== ShadowDropdown.qml ===')
print(f'Lines: {c.count(chr(10))}')
print(f'Has enabled property: {"enabled" in c}')
blank_fix = '_itemValue(modelData) === ""' in c or "_itemValue(modelData) === ''" in c
print(f'Has blank item fix: {blank_fix}')

# Check SettingsJavaPage ComboBox locations
c2 = open('qml/SettingsJavaPage.qml','r',encoding='utf-8').read()
print('\n=== SettingsJavaPage.qml ===')
# Find first ComboBox
idx = c2.find('ComboBox {')
if idx >= 0:
    start = max(0, idx - 50)
    ctx = c2[start:idx+200]
    print(f'First ComboBox at offset {idx}:')
    print(ctx[:300])

# Check SettingsExperimentalPage
c3 = open('qml/SettingsExperimentalPage.qml','r',encoding='utf-8').read()
print('\n=== SettingsExperimentalPage.qml ===')
idx = c3.find('ComboBox {')
if idx >= 0:
    start = max(0, idx - 50)
    ctx = c3[start:idx+200]
    print(f'First ComboBox at offset {idx}:')
    print(ctx[:300])

# Check SettingsGeneralPage
c4 = open('qml/SettingsGeneralPage.qml','r',encoding='utf-8').read()
print('\n=== SettingsGeneralPage.qml ===')
cnt = c4.count('ShadowDropdown')
print(f'ShadowDropdown count: {cnt}')

# Check Version pages
c5 = open('qml/VersionLaunchSection.qml','r',encoding='utf-8').read()
c6 = open('qml/VersionSettingsPage.qml','r',encoding='utf-8').read()
print(f'\nVersionLaunchSection.qml: ComboBox={c5.count("ComboBox")} Popup={c5.count("Popup {")}')
print(f'VersionSettingsPage.qml: ComboBox={c6.count("ComboBox")} Popup={c6.count("Popup {")}')
