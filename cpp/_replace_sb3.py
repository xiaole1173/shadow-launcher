# -*- coding: utf-8 -*-
c = open('D:\\latest-code\\cpp\\qml\\DownloadPage.qml', 'r', encoding='utf-8').read()
orig = c
print(f'File size: {len(c)}')

def extract_rect_block(text, start_pos):
    """Find the Rectangle { ... } that contains start_pos"""
    # Go back to the Rectangle { line
    rect_brace = text.rfind('{', start_pos - 300, start_pos)
    # Find the line start of this brace
    line_start = text.rfind('\n', 0, rect_brace) + 1
    # Verify the line contains 'Rectangle'
    line = text[line_start:rect_brace]
    if 'Rectangle' not in line:
        # Try harder - go back more
        for offset in range(400, 1000, 100):
            rect_brace = text.rfind('{', start_pos - offset, start_pos - offset + 100)
            line_start = text.rfind('\n', 0, rect_brace) + 1
            line = text[line_start:rect_brace]
            if 'Rectangle' in line:
                break
    
    # Parse braces from rect_brace
    bal = 1
    i = rect_brace + 1
    while i < len(text) and bal > 0:
        if text[i] == '{': bal += 1
        if text[i] == '}': bal -= 1
        i += 1
    
    block = text[line_start:i]
    return block, line_start, i

# ─── MOD ───
mod_mid = c.find('Keys.onReturnPressed: modTab.doModSearch()')
mod_block, mod_start, mod_end = extract_rect_block(c, mod_mid)
print(f'Mod block: {len(mod_block)}b, from {mod_start} to {mod_end}')
print('  First 80:', repr(mod_block[:80]))
print('  Last 80:', repr(mod_block[-80:]))

new_mod = (
    "                        SearchBox {\n"
    "                            id: modInput\n"
    "                            placeholderText: qsTr(\"输入 Mod 名称...\")\n"
    "                            onAccepted: modTab.doModSearch()\n"
    "                        }"
)

# ─── SHADER ───
shader_mid = c.find('Keys.onReturnPressed: shaderTab.doSearch()')
shader_block, shader_start, shader_end = extract_rect_block(c, shader_mid)
print(f'Shader block: {len(shader_block)}b, from {shader_start} to {shader_end}')
print('  First 80:', repr(shader_block[:80]))
print('  Last 80:', repr(shader_block[-80:]))

new_shader = (
    "                        SearchBox {\n"
    "                            id: shaderInput\n"
    "                            placeholderText: qsTr(\"输入光影名称...\")\n"
    "                            onAccepted: shaderTab.doSearch()\n"
    "                        }"
)

# ─── RP ───
rp_mid = c.find('REMOVED onAccepted')
if rp_mid < 0:
    rp_mid = c.find('rpSearchInput')
    # Find the TextInput with id: rpSearchInput
    # Actually let's search for the specific comment
    rp_mid = c.find('search only on button click')
print(f'RP mid marker at {rp_mid}')

rp_block, rp_start, rp_end = extract_rect_block(c, rp_mid)
print(f'RP block: {len(rp_block)}b, from {rp_start} to {rp_end}')
print('  First 80:', repr(rp_block[:80]))
print('  Last 80:', repr(rp_block[-80:]))

new_rp = (
    "                        SearchBox {\n"
    "                            id: rpSearchInput\n"
    "                            placeholderText: qsTr(\"输入资源包名称...\")\n"
    "                        }"
)

# ─── Apply replacements (reverse order to preserve indices) ───
c = orig  # start fresh

# Replace RP first (largest index)
c = c[:rp_start] + new_rp + c[rp_end:]
print(f'After RP: {len(c)}')

# Replace Shader next (middle index) - recalculate from original positions
# Actually the indices shifted. Let me recalculate:
if shader_start < rp_start:
    pass  # shader is before rp, so its position is unchanged
elif shader_start > rp_start:
    shader_start = shader_start - (rp_end - rp_start) + (len(new_rp) - len(rp_block))
    shader_end = shader_end - (rp_end - rp_start) + (len(new_rp) - len(rp_block))

# Actually, let me just do str.replace for safety
# Find the exact text in the modified `c`
c = orig  # restart clean

# Verify each block exists exactly once
for block, name in [(mod_block, 'Mod'), (shader_block, 'Shader'), (rp_block, 'RP')]:
    cnt = c.count(block)
    print(f'{name} exact match: {cnt}')
    if cnt != 1:
        print(f'  ERROR: expected 1 match')

# Do replacements
c = c.replace(mod_block, new_mod, 1)
c = c.replace(shader_block, new_shader, 1)
c = c.replace(rp_block, new_rp, 1)

print(f'\nFinal size: {len(c)} (was {len(orig)})')

for id_str in ['modInput', 'shaderInput', 'rpSearchInput', 'SearchBox']:
    n = c.count(id_str)
    print(f'{id_str}: {n}')

open('D:\\latest-code\\cpp\\qml\\DownloadPage.qml', 'w', encoding='utf-8').write(c)
print('Written OK')
