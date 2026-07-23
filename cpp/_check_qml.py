import os
with open('CMakeLists.txt', 'r', encoding='utf-8') as f:
    lines = f.readlines()
in_qml = False
for i, line in enumerate(lines, 1):
    if 'qml_resources' in line and 'PREFIX' in line:
        in_qml = True
        print('--- QML resources ---')
    if in_qml and ')' in line and i < 250:
        in_qml = False
    if in_qml and 'qml/' in line and '#' not in line:
        fn = line.strip().rstrip(',').strip()
        exists = os.path.exists(fn)
        status = "OK" if exists else "MISSING!"
        if not exists:
            print(f'MISSING: {fn}')
