import urllib.request, json

# Try different BMCLAPI version endpoint URLs
urls = [
    'https://bmclapi2.bangbang93.com/version/1.18.2/json',
    'https://bmclapi2.bangbang93.com/version/1.18.2',
    'https://bmclapi2.bangbang93.com/versions/1.18.2.json',
    'https://bmclapi2.bangbang93.com/versions/1.18.2/1.18.2.json',
]

for url in urls:
    try:
        req = urllib.request.Request(url)
        resp = urllib.request.urlopen(req, timeout=5)
        data = json.loads(resp.read())
        print(f"=== {url} ===")
        print(f"  id: {data.get('id')}")
        print(f"  time: {data.get('time')}")
        print(f"  releaseTime: {data.get('releaseTime')}")
        if 'downloads' in data:
            cm = data['downloads'].get('client_mappings', {})
            if cm:
                print(f"  client_mappings: sha1={cm.get('sha1','?')}")
        print()
    except Exception as e:
        print(f"  {url}: {e}\n")
