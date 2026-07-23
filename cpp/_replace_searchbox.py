# -*- coding: utf-8 -*-
import os, re

content = open('D:\\latest-code\\cpp\\qml\\DownloadPage.qml', 'r', encoding='utf-8').read()
print(f'Original size: {len(content)} bytes, lines: {content.count(chr(10))}')

# ─── MOD search ───
old_mod = (
    "                        Rectangle {\n"
    "                            Layout.fillWidth: true; height: 28; radius: StyleTokens.radiusSm\n"
    "                            color: StyleTokens.bgInput\n"
    "                            border.color: modInput.activeFocus ? StyleTokens.accentHover : StyleTokens.borderLight\n"
    "                            border.width: 1\n"
    "                            Behavior on color { ColorAnimation { duration: 200 } }\n"
    "                            Behavior on border.color { ColorAnimation { duration: 200 } }\n"
    "\n"
    "                            TextInput {\n"
    "                                id: modInput\n"
    "                                anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8\n"
    "                                color: StyleTokens.textPrimary; verticalAlignment: TextInput.AlignVCenter; font.pixelSize: StyleTokens.fontSizeSm\n"
    "                                Keys.onReturnPressed: modTab.doModSearch()\n"
    "\n"
    "                                Text {\n"
    "                                    anchors.fill: parent; verticalAlignment: Text.AlignVCenter\n"
    "                                    text: qsTr(\"输入 Mod 名称...\"); color: StyleTokens.textMuted; font.pixelSize: StyleTokens.fontSizeSm\n"
    "                                    visible: !modInput.text\n"
    "                                }\n"
    "                            }\n"
    "                        }"
)

new_mod = (
    "                        SearchBox {\n"
    "                            id: modInput\n"
    "                            placeholderText: qsTr(\"输入 Mod 名称...\")\n"
    "                            onAccepted: modTab.doModSearch()\n"
    "                        }"
)

count = content.count(old_mod)
print(f'Mod old text matches: {count}')
if count != 1:
    # Try to find similar text
    idx = content.find('modInput')
    print(f'  First modInput at char {idx}')
    if idx > 0:
        # Show context around it
        before = content[idx-300:idx+400]
        print(f'  Context: {before}')
else:
    content = content.replace(old_mod, new_mod, 1)
    print(f'  Replaced. Size now: {len(content)}')

# ─── SHADER search ───
old_shader = (
    "                        Rectangle {\n"
    "                            Layout.fillWidth: true; height: 28; radius: StyleTokens.radiusSm\n"
    "                            color: StyleTokens.bgInput\n"
    "                            border.color: shaderInput.activeFocus ? StyleTokens.accentHover : StyleTokens.borderLight; border.width: 1\n"
    "                            Behavior on color { ColorAnimation { duration: 200 } }\n"
    "                            Behavior on border.color { ColorAnimation { duration: 200 } }\n"
    "                            TextInput {\n"
    "                                id: shaderInput\n"
    "                                anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8\n"
    "                                color: StyleTokens.textPrimary; verticalAlignment: TextInput.AlignVCenter; font.pixelSize: StyleTokens.fontSizeSm\n"
    "                                Keys.onReturnPressed: shaderTab.doSearch()\n"
    "                                Text {\n"
    "                                    anchors.fill: parent; verticalAlignment: Text.AlignVCenter\n"
    "                                    text: qsTr(\"输入光影名称...\"); color: StyleTokens.textMuted; font.pixelSize: StyleTokens.fontSizeSm\n"
    "                                    visible: !shaderInput.text\n"
    "                                }\n"
    "                            }\n"
    "                        }"
)

new_shader = (
    "                        SearchBox {\n"
    "                            id: shaderInput\n"
    "                            placeholderText: qsTr(\"输入光影名称...\")\n"
    "                            onAccepted: shaderTab.doSearch()\n"
    "                        }"
)

count = content.count(old_shader)
print(f'Shader old text matches: {count}')
if count == 1:
    content = content.replace(old_shader, new_shader, 1)
    print(f'  Replaced. Size now: {len(content)}')
else:
    print(f'  ERROR: expected 1 match, got {count}')

# ─── RP search ───
old_rp = (
    "                        Rectangle {\n"
    "                            Layout.fillWidth: true; height: 28; radius: StyleTokens.radiusSm\n"
    "                            color: StyleTokens.bgInput\n"
    "                            border.color: rpSearchInput.activeFocus ? StyleTokens.accentHover : StyleTokens.borderLight\n"
    "                            border.width: 1\n"
    "                            Behavior on color { ColorAnimation { duration: 200 } }\n"
    "                            Behavior on border.color { ColorAnimation { duration: 200 } }\n"
    "\n"
    "                            TextInput {\n"
    "                                id: rpSearchInput\n"
    "                                anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8\n"
    "                                color: StyleTokens.textPrimary; verticalAlignment: TextInput.AlignVCenter; font.pixelSize: StyleTokens.fontSizeSm\n"
    "                                // REMOVED onAccepted trigger \u2014 search only on button click (Fix 1)\n"
    "\n"
    "                                Text {\n"
    "                                    anchors.fill: parent; verticalAlignment: Text.AlignVCenter\n"
    "                                    text: qsTr(\"输入资源包名称...\"); color: StyleTokens.textMuted; font.pixelSize: StyleTokens.fontSizeSm\n"
    "                                    visible: !rpSearchInput.text\n"
    "                                }\n"
    "                            }\n"
    "                        }"
)

# NOTE: the old_rp has an em-dash (U+2014) in the comment " — search only..."
# Let me find the actual character used
idx = content.find('REMOVED onAccepted')
if idx > 0:
    actual_char = content[idx+22:idx+23]
    print(f'RP comment separator char: U+{ord(actual_char):04X} ({actual_char})')
    # Use the actual character
    old_rp = old_rp.replace('\u2014', actual_char)

new_rp = (
    "                        SearchBox {\n"
    "                            id: rpSearchInput\n"
    "                            placeholderText: qsTr(\"输入资源包名称...\")\n"
    "                        }"
)

count = content.count(old_rp)
print(f'RP old text matches: {count}')
if count == 1:
    content = content.replace(old_rp, new_rp, 1)
    print(f'  Replaced. Size now: {len(content)}')
else:
    print(f'  ERROR: expected 1 match, got {count}')
    # Try without the second newline
    idx = content.find('rpSearchInput')
    if idx > 0:
        print(f'  rpSearchInput at char {idx}')
        print(f'  Context: {content[idx-300:idx+400]}')

# ─── Write back ───
open('D:\\latest-code\\cpp\\qml\\DownloadPage.qml', 'w', encoding='utf-8').write(content)
print(f'Final size: {len(content)} bytes, lines: {content.count(chr(10))}')

# Verify references
for id_str in ['modInput', 'shaderInput', 'rpSearchInput']:
    count = content.count(id_str)
    print(f'{id_str}: {count} occurrences')
