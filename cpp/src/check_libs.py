import json
d = json.load(open(r'D:\latest-code\cpp\build\Release\.minecraft\versions\26.2-neoforge-26.2.0.7-beta\26.2-neoforge-26.2.0.7-beta.json', 'r', encoding='utf-8'))
print('Total libraries:', len(d['libraries']))
for i, l in enumerate(d['libraries']):
    print(f'{i}: {l["name"]}')
