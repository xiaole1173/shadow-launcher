# -*- coding: utf-8 -*-
# Replace Shader search box
c = open('D:\\latest-code\\cpp\\qml\\DownloadPage.qml', 'r', encoding='utf-8').read()

# Find the shader search Rectangle
idx = c.find('shaderInput')
shader_rect_start = idx - 300
# Go back from shaderInput (L1206) to find the Rectangle
rect_brace = c.rfind('{', shader_rect_start, idx)
line_start = c.rfind('\n', 0, rect_brace) + 1
line = c[line_start:rect_brace]

# Back up to find Rectangle
while 'Rectangle' not in line and rect_brace > shader_rect_start - 500:
    rect_brace = c.rfind('{', shader_rect_start - 100, rect_brace - 1)
    if rect_brace < 0:
        break
    line_start = c.rfind('\n', 0, rect_brace) + 1
    line = c[line_start:rect_brace]

print(f'Line: {line.strip()}')
print(f'Brace at char: {rect_brace}')

# Close braces
bal = 1
i = rect_brace + 1
while i < len(c) and bal > 0:
    if c[i] == '{': bal += 1
    if c[i] == '}': bal -= 1
    i += 1

old_shader = c[line_start:i]
print(f'Old shader block: {len(old_shader)} bytes')
print(f'First 100: {repr(old_shader[:100])}')
print(f'Last 100: {repr(old_shader[-100:])}')

new_shader = (
    "                        SearchBox {\n"
    "                            id: shaderInput\n"
    "                            placeholderText: qsTr(\"输入光影名称...\")\n"
    "                            onAccepted: shaderTab.doSearch()\n"
    "                        }"
)

assert c.count(old_shader) == 1, f"Expected 1 match, got {c.count(old_shader)}"
c = c.replace(old_shader, new_shader, 1)

# Print refs for verification
import re
for m in re.finditer(r'\bshaderInput\b', c):
    ln = c[:m.start()].count('\n') + 1
    print(f'  L{ln}: {c[m.start():m.end()+40]}')

open('D:\\latest-code\\cpp\\qml\\DownloadPage.qml', 'w', encoding='utf-8').write(c)
print(f'Done. Size: {len(c)}')
