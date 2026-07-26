import json

path = r'D:\latest-code\cpp\build\Release\.minecraft\versions\1.19.4\1.19.4.json'
with open(path, 'r', encoding='utf-8') as f:
    v = json.load(f)

for lib in v.get('libraries', []):
    name = lib.get('name', '')
    if 'client' in name or 'mappings' in name:
        print(f'name: {name}')
        if 'downloads' in lib:
            art = lib['downloads'].get('artifact', {})
            print(f'  path: {art.get("path", "N/A")}')
            print(f'  sha1: {art.get("sha1", "N/A")}')
            print(f'  size: {art.get("size", "N/A")}')

# Check if the file exists
import os
p = r'D:\latest-code\cpp\build\Release\.minecraft\libraries\net\minecraft\client'
if os.path.exists(p):
    print('\nContents of client dir:')
    for root, dirs, files in os.walk(p):
        for f in files[:20]:
            print(f'  {os.path.join(root, f)}')
else:
    print('\nClient directory does not exist')
