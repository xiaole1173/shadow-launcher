import json

path = r'D:\latest-code\cpp\build\Release\.minecraft\versions\1.19.4\1.19.4.json'
with open(path, 'r', encoding='utf-8') as f:
    v = json.load(f)
    text = json.dumps(v)

# Search for specific patterns
for name in ['client', 'mappings', '20230314', '1.19.4']:
    idx = text.find(name)
    if idx >= 0:
        print(f'Found "{name}" at pos {idx}: ...{text[max(0,idx-20):idx+len(name)+80]}...')

# Also search for the exact path
if 'client/1.19.4-20230314' in text:
    print('\nFOUND: client/1.19.4-20230314 path reference')
else:
    print('\nNOT FOUND: client/1.19.4-20230314 path reference')
    print('This means the vanilla JSON does NOT reference this mapping file.')
    print('Let us try the mojang mappings in the version JSON...')
