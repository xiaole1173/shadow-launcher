# -*- coding: utf-8 -*-
# Final fix: replace RP search box
c = open('D:\\latest-code\\cpp\\qml\\DownloadPage.qml', 'r', encoding='utf-8').read()

idx = c.find('rpSearchInput')
print(f'rpSearchInput at char {idx}')

# Look for the RP search Rectangle
before = c[idx-600:idx]
print('Context before rpSearchInput (last 400 chars):')
print(repr(before[-400:]))

# Look at the actual page structure
# rpSearchInput should be inside a Rectangle. Find the last Rectangle before it
rect_pos = c.rfind('Rectangle', idx - 600, idx)
print(f'\nLast Rectangle before rpSearchInput: {rect_pos}')
if rect_pos > 0:
    print(f'Rectangle line content: {c[rect_pos:c.find(chr(10), rect_pos)]}')
