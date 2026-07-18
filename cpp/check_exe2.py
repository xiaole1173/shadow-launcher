import struct
with open(r'D:\latest-code\cpp\build\Release\ShadowLauncher.exe','rb') as f:
    data = f.read()

idx = data.find(b'Shadow Launcher')
if idx >= 0:
    after = data[idx:idx+100]
    parts = after.split(b'\x00')[:5]
    print('Found:', [p.decode(errors='ignore') for p in parts if p])

for s in [b'0.1.0-beta.1', b'0.1.0', b'1.0.0']:
    print(f'  "{s.decode()}": {data.count(s)}')
