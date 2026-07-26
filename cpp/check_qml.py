import os
path = r'D:\latest-code\cpp\qml\InstallPage.qml'
with open(path, 'r', encoding='utf-8', errors='ignore') as f:
    content = f.read()

# Find installModLoader for forge
idx = content.find('installModLoader(mcVersion, "forge"')
if idx >= 0:
    start = max(0, idx - 200)
    print(content[start:idx+120])
else:
    print("NOT FOUND")
    # Try alternative
    for line in content.split('\n'):
        if 'installModLoader' in line:
            print(line.strip()[:150])
