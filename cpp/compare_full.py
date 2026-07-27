import json, os

pcl_json = r'D:\Minecraft\.minecraft\versions\26.2-NeoForge_26.2.0.34-beta\26.2-NeoForge_26.2.0.34-beta.json'
our_json = r'D:\latest-code\cpp\build\Release\.minecraft\versions\26.2-neoforge-26.2.0.34-beta\26.2-neoforge-26.2.0.34-beta.json'

pcl = json.load(open(pcl_json, 'r', encoding='utf-8'))
our = json.load(open(our_json, 'r', encoding='utf-8'))

# 1. Top-level fields comparison
print('=== 顶层字段差异 ===')
all_fields = set(list(pcl.keys()) + list(our.keys()))
for f in sorted(all_fields):
    if f == 'libraries': continue
    pv = pcl.get(f)
    ov = our.get(f)
    if pv != ov:
        print(f'  {f}: 主流启动器={repr(pv)[:100]}  我们={repr(ov)[:100]}')

# 2. Libraries comparison
print(f'\n=== 库对比 ===')
pcl_libs = {l['name'] for l in pcl['libraries'] if isinstance(l, dict) and 'name' in l}
our_libs = {l['name'] for l in our['libraries'] if isinstance(l, dict) and 'name' in l}

only_pcl = pcl_libs - our_libs
only_our = our_libs - pcl_libs
common = pcl_libs & our_libs

print(f'主流启动器 库数: {len(pcl_libs)}')
print(f'我们 库数: {len(our_libs)}')
print(f'共同: {len(common)}')
print(f'仅在 主流启动器: {len(only_pcl)}')
print(f'仅在我们: {len(only_our)}')

if only_pcl:
    print('\n仅在 主流启动器 中的库（前20）:')
    for n in sorted(only_pcl)[:20]:
        print(f'  {n}')
if only_our:
    print('\n仅在我们中的库（前20）:')
    for n in sorted(only_our)[:20]:
        print(f'  {n}')

# 3. Version folder contents
print(f'\n=== 版本文件夹内容 ===')
for base, label in [(r'D:\Minecraft\.minecraft\versions\26.2-NeoForge_26.2.0.34-beta', '主流启动器'),
                     (r'D:\latest-code\cpp\build\Release\.minecraft\versions\26.2-neoforge-26.2.0.34-beta', '我们')]:
    items = os.listdir(base)
    txt = [f'{i} ({os.path.getsize(os.path.join(base,i))}B)' if os.path.isfile(os.path.join(base,i)) else i for i in items]
    print(f'{label}: {txt}')
