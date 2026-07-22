import os, re

qml_files = []
for root, dirs, files in os.walk('qml'):
    for f in files:
        if f.endswith('.qml'):
            qml_files.append(os.path.join(root, f))

# Find files with RefreshButton already
print('=== Files using RefreshButton component ===')
for f in sorted(qml_files):
    c = open(f,'r',encoding='utf-8').read()
    if 'RefreshButton' in c:
        cnt = c.count('RefreshButton')
        print(f'  {f}: {cnt} usages')
        
print('\n=== Files with inline refresh buttons (NOT using RefreshButton) ===')
for f in sorted(qml_files):
    c = open(f,'r',encoding='utf-8').read()
    if 'RefreshButton' in c:
        continue
    
    # Detect inline refresh buttons by checking for refresh-cw icon OR refresh-related text in Rectangle
    finds = []
    has_refresh_icon = 'refresh-cw' in c.lower() or 'refresh.svg' in c.lower()
    
    # Find comments with "刷新" or "Refresh" near Rectangle blocks
    for m in re.finditer(r'//.*?(?:刷新|Refresh|refresh).*?\n', c):
        line_no = c[:m.start()].count('\n') + 1
        ctx_start = max(0, m.start()-50)
        ctx_end = min(len(c), m.end()+100)
        ctx = c[ctx_start:ctx_end].replace('\n', ' ')
        finds.append(f'  L{line_no}: {ctx}')
    
    # Also check for refresh-related onClicked handlers
    for m in re.finditer(r'(?:refresh|刷新).*?(?:clicked|onClicked)', c, re.IGNORECASE):
        line_no = c[:m.start()].count('\n') + 1
        ctx_start = max(0, m.start()-100)
        ctx_end = min(len(c), m.end()+100)
        ctx = c[ctx_start:ctx_end].replace('\n', ' ')
        finds.append(f'  L{line_no}: {ctx}')
    
    if has_refresh_icon or finds:
        print(f'\n{f}:')
        if has_refresh_icon:
            print(f'  Has refresh-cw icon')
        for fi in finds:
            print(fi)
