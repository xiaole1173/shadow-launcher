# -*- coding: utf-8 -*-
c = open('D:\\latest-code\\cpp\\qml\\DownloadPage.qml', 'r', encoding='utf-8').read()

opens = c.count('{')
closes = c.count('}')
print(f'Overall braces: {opens} open, {closes} close, diff={opens-closes}')
print(f'SearchBox count: {c.count("SearchBox")}')

# Check context around each search box
for name, id_str in [('Mod', 'modInput'), ('Shader', 'shaderInput'), ('RP', 'rpSearchInput')]:
    idx = c.find('id: ' + id_str)
    if idx < 0:
        print(f'ERROR: {id_str} not found!')
        continue
    start = max(0, idx - 200)
    end = min(len(c), idx + 250)
    seg = c[start:end]
    lines = seg.split('\n')
    print(f'=== {name} ({id_str}) ===')
    for i, l in enumerate(lines):
        line_no = c[:start].count('\n') + i + 1
        print(f'  L{line_no}: {l}')
    print()
