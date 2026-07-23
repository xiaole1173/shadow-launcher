# -*- coding: utf-8 -*-
c = open('D:\\latest-code\\cpp\\qml\\DownloadPage.qml', 'r', encoding='utf-8').read()
orig = c

print(f'Original size: {len(c)}')

# Verify we're working with the clean version
assert 'SearchBox' not in c, "File already contains SearchBox!"

# ─── MOD search replacement ───
# Find the exact mod input block by locating the TextInput
mod_start = c.find('Keys.onReturnPressed: modTab.doModSearch()')
# Go back to find the opening Rectangle
rect_start = c.rfind('Rectangle {', mod_start - 400, mod_start)
# Go to the closing brace
brace_open = c.find('{', rect_start)
bal = 1
i = brace_open + 1
while i < len(c) and bal > 0:
    if c[i] == '{': bal += 1
    if c[i] == '}': bal -= 1
    i += 1

old_mod = c[rect_start:i]
new_mod = (
    "                        SearchBox {\n"
    "                            id: modInput\n"
    "                            placeholderText: qsTr(\"输入 Mod 名称...\")\n"
    "                            onAccepted: modTab.doModSearch()\n"
    "                        }"
)
print(f'Mod: old={len(old_mod)}b, new={len(new_mod)}b')
c = c[:rect_start] + new_mod + c[i:]

# ─── SHADER search replacement ───
shader_start = c.find('Keys.onReturnPressed: shaderTab.doSearch()')
rect_start = c.rfind('Rectangle {', shader_start - 400, shader_start)
brace_open = c.find('{', rect_start)
bal = 1
i = brace_open + 1
while i < len(c) and bal > 0:
    if c[i] == '{': bal += 1
    if c[i] == '}': bal -= 1
    i += 1

old_shader = c[rect_start:i]
new_shader = (
    "                        SearchBox {\n"
    "                            id: shaderInput\n"
    "                            placeholderText: qsTr(\"输入光影名称...\")\n"
    "                            onAccepted: shaderTab.doSearch()\n"
    "                        }"
)
print(f'Shader: old={len(old_shader)}b, new={len(new_shader)}b')
c = c[:rect_start] + new_shader + c[i:]

# ─── RP search replacement ───
# Find "REMOVED onAccepted" (the RP search has this comment instead of Keys.onReturnPressed)
rp_start = c.find('REMOVED onAccepted')
if rp_start < 0:
    # Fallback: find rpSearchInput TextInput
    rp_start = c.find('rpSearchInput')

rect_start = c.rfind('Rectangle {', rp_start - 400, rp_start)
brace_open = c.find('{', rect_start)
bal = 1
i = brace_open + 1
while i < len(c) and bal > 0:
    if c[i] == '{': bal += 1
    if c[i] == '}': bal -= 1
    i += 1

old_rp = c[rect_start:i]
new_rp = (
    "                        SearchBox {\n"
    "                            id: rpSearchInput\n"
    "                            placeholderText: qsTr(\"输入资源包名称...\")\n"
    "                        }"
)
print(f'RP: old={len(old_rp)}b, new={len(new_rp)}b')
print(f'RP old text preview: {repr(old_rp[:100])}')
c = c[:rect_start] + new_rp + c[i:]

# ─── Write back ───
print(f'Final size: {len(c)} (was {len(orig)})')

# Verify all references still work
for id_str in ['modInput', 'shaderInput', 'rpSearchInput']:
    n = c.count(id_str)
    print(f'{id_str}: {n} refs')
    if n != c.count(id_str):
        print(f'  ERROR: count mismatch!')

# Check there are no more `import QtQuick` duplicates
print(f'Has SearchBox import: {"SearchBox" in c}')
print(f'Has Rectangle TextInput: {c.count("TextInput {")}')

open('D:\\latest-code\\cpp\\qml\\DownloadPage.qml', 'w', encoding='utf-8').write(c)
print('Written OK')
