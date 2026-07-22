import os, re

c = open('qml/DownloadPage.qml','r',encoding='utf-8').read()
# Find all search bar areas
for m in re.finditer(r'(Rectangle\s*\{[^}]*?(?:search|filter|搜索).*?(?:TextInput|TextField).*?filter)', c, re.DOTALL):
    line_no = c[:m.start()].count('\n') + 1
    ctx = m.group()[:300].replace('\n', ' ')
    print(f'=== Search bar L{line_no} ===')
    print(ctx[:400])
    print()

# Also find any Image with search.svg
for m in re.finditer(r'search\.svg', c):
    line_no = c[:m.start()].count('\n') + 1
    ctx_start = max(0, m.start()-80)
    ctx_end = min(len(c), m.start()+150)
    ctx = c[ctx_start:ctx_end].replace('\n', ' ')
    print(f'=== search.svg at L{line_no} ===')
    print(ctx[:300])
    print()
