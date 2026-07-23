# -*- coding: utf-8 -*-
import re

c = open('D:\\latest-code\\cpp\\qml\\DownloadPage.qml', 'r', encoding='utf-8').read()

lines = c.split('\n')
for i, l in enumerate(lines):
    if 'shaderInput' in l and 'activeFocus' in l:
        rect_line = -1
        for j in range(i-1, max(0, i-20), -1):
            if 'Rectangle' in lines[j] and '{' in lines[j]:
                rect_line = j
                break
        
        byte_start = sum(len(l) + 1 for l in lines[:rect_line])
        brace_idx = lines[rect_line].find('{')
        rect_brace = byte_start + brace_idx
        
        bal = 1
        k = rect_brace + 1
        while k < len(c) and bal > 0:
            if c[k] == '{': bal += 1
            if c[k] == '}': bal -= 1
            k += 1
        
        old_block = c[byte_start:k]
        
        new_block = (
            '                        SearchBox {\n'
            '                            id: shaderInput\n'
            '                            placeholderText: qsTr("输入光影名称...")\n'
            '                            onAccepted: shaderTab.doSearch()\n'
            '                        }'
        )
        
        n = c.count(old_block)
        print(f'Shader block matches: {n}')
        assert n == 1, f'Expected 1, got {n}'
        
        c = c.replace(old_block, new_block, 1)
        print(f'Size: {len(c)}')
        
        for m in re.finditer(r'\bshaderInput\b', c):
            ln = c[:m.start()].count('\n') + 1
            print(f'  L{ln}: {c[m.start():m.end()+40]}')
        
        open('D:\\latest-code\\cpp\\qml\\DownloadPage.qml', 'w', encoding='utf-8').write(c)
        print('OK')
        break
