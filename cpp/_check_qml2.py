import os
with open('CMakeLists.txt', 'r', encoding='utf-8') as f:
    lines = f.readlines()
for i in range(220, 240):
    line = lines[i-1].rstrip()
    if 'qml/' in line:
        fn = line.strip().rstrip(',').strip()
        exists = os.path.exists(fn)
        status = "EXISTS" if exists else "MISSING"
        print(f'Line {i}: {line}  -> {status}')
