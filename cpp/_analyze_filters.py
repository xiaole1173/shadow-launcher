# -*- coding: utf-8 -*-
c = open('D:\\latest-code\\cpp\\qml\\DownloadPage.qml', 'r', encoding='utf-8').read()
lines = c.split('\n')

# Find each filter card and print its structure
for name, marker in [('Mod', 'Row 1'), ('Shader', 'Row 1'), ('RP', 'Row 1')]:
    # Skip the DP comment marker, find the actual RowLayout markers
    pass

# Better approach: find where each filter Rectangle starts
for name, search_id in [('Mod', 'modInput'), ('Shader', 'shaderInput'), ('RP', 'rpSearchInput')]:
    idx = c.find('id: ' + search_id)
    # Go back to find the enclosing Rectangle
    # Look for a Rectangle with Layout.fillWidth above the search box
    before = c[:idx]
    lines_before = before.split('\n')
    # Find the Rectangle line
    for i in range(len(lines_before)-1, max(0, len(lines_before)-30), -1):
        l = lines_before[i]
        if l.strip().startswith('Rectangle') and '{' in l:
            print(f'{name} filter card starts at line {i+1}: {l.strip()}')
            break
    
    # Go forward to find the next major section boundary
    after = c[idx:]
    # Find // Results or // -- Card Grid or some section marker
    for marker in ['// -- Card Grid --', '// Results', '// Row 1', '// Tab']:
        # Only look for markers NOT before the search box
        pos = c.find(marker, idx)
        if pos > 0:
            line_no = c[:pos].count('\n') + 1
            print(f'  -> \"{marker.strip()}\" at line {line_no}')
    
    print()
