import json, os

p = os.path.join(r'D:\latest-code\cpp', 'build', 'Release', '.minecraft', 'versions', '1.19.4', '1.19.4.json')
print(f'Path: {p}')
print(f'Exists: {os.path.exists(p)}')

with open(p, 'r', encoding='utf-8') as f:
    v = json.load(f)

dls = v.get('downloads', {})
print(f'Download keys: {list(dls.keys())}')
cm = dls.get('client_mappings', {})
print(f'client_mappings: sha1={cm.get("sha1")} url={cm.get("url","")[:100]}')
print(f'id: {v.get("id")}')
print(f'type: {v.get("type")}')

# The key question: how to derive the timestamp
# Let's look at the client jar URL
client_jar = dls.get('client', {})
client_url = client_jar.get('url', '')
print(f'client jar url: {client_url[:200]}')

# Version manifest approach: we know MC 1.19.4 was released on 2023-03-14
# The timestamp pattern is YYYYMMDD.HHMMSS from the release time
# From mojang version manifest: 1.19.4 releaseTime = "2023-03-14T12:29:34+00:00"
# → timestamp = "1.19.4-20230314.122934"

# But to do this programmatically we need the releaseTime from the version manifest
# Let me check if the local version JSON has a 'time' field
for k in v.keys():
    if 'time' in k.lower():
        print(f'{k}: {v[k]}')
