import os
root = r'D:\latest-code\cpp\qml'
for rootdir, dirs, files in os.walk(root):
    for f in files:
        if not f.endswith('.qml'):
            continue
        path = os.path.join(rootdir, f)
        with open(path, encoding='utf-8') as fh:
            for i, line in enumerate(fh, 1):
                lower = line.lower()
                if '1.0.0' in line or 'VERSION' in line or 'appVersion' in lower or 'versionString' in lower:
                    print(f'{os.path.relpath(path, root)}:{i}:{line.rstrip()}')
