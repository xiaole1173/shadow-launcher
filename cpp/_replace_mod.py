# -*- coding: utf-8 -*-
# Replace only Mod search box with minimal SearchBox
c = open('D:\\latest-code\\cpp\\qml\\DownloadPage.qml', 'r', encoding='utf-8').read()

old_mod = c[37476:38816]  # from previous extraction: mod input Rectangle
new_mod = (
    "                        SearchBox {\n"
    "                            id: modInput\n"
    "                            placeholderText: qsTr(\"输入 Mod 名称...\")\n"
    "                            onAccepted: modTab.doModSearch()\n"
    "                        }"
)

assert c.count(old_mod) == 1, f"Expected 1 match for Mod, got {c.count(old_mod)}"
c = c.replace(old_mod, new_mod, 1)
print(f'Replaced Mod. Size: {len(c)}')

# Verify refs preserved
for ref in ['modInput.text', 'modInput.activeFocus', 'modInput']:
    n = c.count(ref)
    print(f'  {ref}: {n}')

open('D:\\latest-code\\cpp\\qml\\DownloadPage.qml', 'w', encoding='utf-8').write(c)
print('Done')
