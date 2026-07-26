import json

with open(r'D:\latest-code\cpp\build\Release\.minecraft\versions\1.19.4\1.19.4.json', 'r', encoding='utf-8') as f:
    v = json.load(f)

# Check client_mappings download
cm = v.get('downloads', {}).get('client_mappings', {})
print('client_mappings download:')
print(f'  sha1: {cm.get("sha1")}')
print(f'  size: {cm.get("size")}')
print(f'  url:  {cm.get("url", "")[:150]}')

# Find net.minecraft:client in libraries
print('\nnet.minecraft:client in libraries:')
for lib in v.get('libraries', []):
    name = lib.get('name', '')
    if name.startswith('net.minecraft:client'):
        print(f'  {name}')
        dls = lib.get('downloads', {})
        for k, dl in dls.items():
            print(f'    {k}: path={dl.get("path","?")}')

# Also check: does the client_mappings URL point to the timestamp version?
url = cm.get('url', '')
if '1.19.4-20230314' in url:
    print('\nclient_mappings URL contains timestamp version')
else:
    print(f'\nclient_mappings URL does NOT contain timestamp: {url[:120]}')
