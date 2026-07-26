import json

path = r'D:\latest-code\cpp\build\Release\.minecraft\versions\1.19.4\1.19.4.json'
with open(path, 'r', encoding='utf-8') as f:
    v = json.load(f)

for lib in v.get('libraries', []):
    name = lib.get('name', '')
    if '1.19.4-20230314' in name:
        print(f'name: {name}')
        if 'downloads' in lib:
            art = lib['downloads'].get('artifact', {})
            print(f'  path: {art.get("path", "N/A")}')
            print(f'  url:  {art.get("url", "N/A")[:150]}')
            print(f'  sha1: {art.get("sha1", "N/A")}')
            print(f'  size: {art.get("size", "N/A")}')

# Also look in the merged forge JSON
fp = r'D:\latest-code\cpp\build\Release\.minecraft\versions\1.19.4-forge-45.4.3\1.19.4-forge-45.4.3.json'
with open(fp, 'r', encoding='utf-8') as f:
    fv = json.load(f)

print('\n--- Forge version JSON libraries with mapping ---')
for lib in fv.get('libraries', []):
    name = lib.get('name', '')
    if 'client' in name and '20230' in name:
        print(f'  {name}')
