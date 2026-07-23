# -*- coding: utf-8 -*-
c = open('D:\\latest-code\\cpp\\qml\\DownloadPage.qml', 'r', encoding='utf-8').read()

# Replace Shader search box
old_shader = c[54955:56272]  # from previous extraction
new_shader = (
    "                        SearchBox {\n"
    "                            id: shaderInput\n"
    "                            placeholderText: qsTr(\"输入光影名称...\")\n"
    "                            onAccepted: shaderTab.doSearch()\n"
    "                        }"
)

assert c.count(old_shader) == 1, f"Expected 1 Shader match, got {c.count(old_shader)}"
c = c.replace(old_shader, new_shader, 1)
print(f'Shader replaced. Size: {len(c)}')

open('D:\\latest-code\\cpp\\qml\\DownloadPage.qml', 'w', encoding='utf-8').write(c)
print('Done')
