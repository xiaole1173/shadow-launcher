d = open(r'D:\latest-code\cpp\build\Release\ShadowLauncher.exe', 'rb').read()
tests = [
    'install_profile.json',
    'MOJMAPS',
    'MAPPINGS',
    'client_mappings',
    '下载缺失的 client_mappings',
]
for s in tests:
    idx = d.find(s.encode('utf-16-le'))
    print(f'  {s}: {"YES" if idx>=0 else "NO"}')
