import os, json

versions_dir = r'D:\latest-code\cpp\build\Release\.minecraft\versions'
for d in os.listdir(versions_dir):
    jar = os.path.join(versions_dir, d, d + '.jar')
    json_file = os.path.join(versions_dir, d, d + '.json')
    print(f'Dir: {d}')
    print(f'  JAR exists: {os.path.exists(jar)}, size: {os.path.getsize(jar) if os.path.exists(jar) else 0}')
    if os.path.exists(json_file):
        with open(json_file) as f:
            data = json.load(f)
        print(f'  JSON id: {data.get("id","?")}')
        print(f'  JSON inheritsFrom: {data.get("inheritsFrom","(none)")}')
        # Check downloads.client for mc version identification
        client = data.get('downloads', {}).get('client', {})
        if client:
            print(f'  downloads.client.url: {client.get("url","?")}')
    print()

neo_dir = r'D:\latest-code\cpp\build\Release\.minecraft\libraries\net\neoforged\minecraft-client-patched'
if os.path.exists(neo_dir):
    print('minecraft-client-patched directory exists!')
    for root, dirs, files in os.walk(neo_dir):
        for f in files:
            fp = os.path.join(root, f)
            print(f'  {fp} ({os.path.getsize(fp)})')
else:
    print('minecraft-client-patched: NOT FOUND')

# Also list all versions
print('\nAll version dirs:')
for d in sorted(os.listdir(versions_dir)):
    print(f'  {d}')
