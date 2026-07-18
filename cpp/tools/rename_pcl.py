import re

files = [
    'D:/latest-code/cpp/src/core/parallel_downloader.h',
    'D:/latest-code/cpp/src/core/parallel_downloader.cpp',
    'D:/latest-code/cpp/src/core/version_downloader.h',
]

replacements = [
    # Remove PCL branding from comments and strings
    ('PCL2-style global chunk pool', 'global chunk pool'),
    ('PCL2-style chunk pool download', 'chunk pool download'),
    ('PCL2-style global chunk pool', 'global chunk pool'),
    ('PCL2-style', 'Enhanced'),
    ('PCL2 speed floor', 'adaptive speed floor'),
    ('PCL2 principle', 'The principle'),
    ('PCL2 全局池', '全局池'),
    ('PCL2 parity: NetTaskThreadLimit ', ''),
]

for f in files:
    with open(f, 'r', encoding='utf-8') as fh:
        content = fh.read()
    original = content
    for old, new in replacements:
        content = content.replace(old, new)
    if content != original:
        with open(f, 'w', encoding='utf-8', newline='\n') as fh:
            fh.write(content)
        print(f'Updated: {f}')
    else:
        print(f'No changes: {f}')

print('Done')
