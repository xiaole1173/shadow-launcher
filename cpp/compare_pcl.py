import json

pcl = json.load(open(r'D:\Minecraft\.minecraft\versions\26.2-NeoForge_26.2.0.35-beta\26.2-NeoForge_26.2.0.35-beta.json', 'r', encoding='utf-8'))
our = json.load(open(r'D:\latest-code\cpp\build\Release\.minecraft\versions\26.2-neoforge-26.2.0.34-beta\26.2-neoforge-26.2.0.34-beta.json', 'r', encoding='utf-8'))

pcl_names = {l.get('name') for l in pcl['libraries'] if isinstance(l, dict)}
our_names = {l.get('name') for l in our['libraries'] if isinstance(l, dict)}

only_in_pcl = pcl_names - our_names
only_in_our = our_names - pcl_names

if only_in_pcl:
    print('=== 仅在 主流启动器 中的库 ===')
    for n in sorted(only_in_pcl): print(f'  {n}')
if only_in_our:
    print('=== 仅在我们中的库 ===')
    for n in sorted(only_in_our): print(f'  {n}')
if not only_in_pcl and not only_in_our:
    print('libraries 完全一致')

# Check mainClass
print(f'\nmainClass: 主流启动器={pcl.get("mainClass")}  我们={our.get("mainClass")}')

# Check downloads.client
pcl_dl = pcl.get('downloads', {})
our_dl = our.get('downloads', {})
print(f'downloads.client 存在: 主流启动器={"client" in pcl_dl}  我们={"client" in our_dl}')

# Check inheritsFrom
print(f'inheritsFrom 存在: 主流启动器={"inheritsFrom" in pcl}  我们={"inheritsFrom" in our}')

# Check total counts
print(f'\n主流启动器 libraries: {len(pcl_names)}  我们: {len(our_names)}')
print(f'主流启动器 总条目: {len(pcl["libraries"])}  我们: {len(our["libraries"])}')
