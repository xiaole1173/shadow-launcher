import os, re

qml_files = []
for root, dirs, files in os.walk('qml'):
    for f in files:
        if f.endswith('.qml'):
            qml_files.append(os.path.join(root, f))

for f in sorted(qml_files):
    c = open(f,'r',encoding='utf-8').read()
    finds = []
    # Find inline search bars: TextInput + some icon/Image nearby
    # Look for patterns like: TextInput with search field, or "search.svg", or explicit search bar Rectangle blocks
    for m in re.finditer(r'(Rectangle\s*\{[^}]*?(?:search.*?field|searchField|search.*?input|搜索|search.*?bar)[^}]*?\})', c, re.DOTALL):
        line_no = c[:m.start()].count('\n') + 1
        ctx = m.group()[:200].replace('\n', ' ')
        finds.append(f'  L{line_no}: {ctx}')
    
    # Also look for specific pattern: search.svg in Image source
    for m in re.finditer(r'search\.svg', c):
        line_no = c[:m.start()].count('\n') + 1
        ctx_start = max(0, m.start()-60)
        ctx_end = min(len(c), m.start()+100)
        ctx = c[ctx_start:ctx_end].replace('\n', ' ')
        finds.append(f'  L{line_no}: {ctx}')
    
    if finds:
        print(f'{f}:')
        for fi in finds[:5]:
            print(fi)
        if len(finds) > 5:
            print(f'  ...({len(finds)-5} more)')
        print()
