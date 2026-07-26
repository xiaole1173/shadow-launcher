import urllib.request, json, sys

sys.stdout.reconfigure(encoding='utf-8')
hdr = {"User-Agent": "Mozilla/5.0"}

# Check: does branch field determine the Maven version?
mc_list = ["1.5.2", "1.6.4", "1.7.2", "1.7.10", "1.8", "1.8.9", "1.9", "1.9.4", "1.10", "1.10.2", "1.11", "1.12.2"]

for mc in mc_list:
    url = f"https://bmclapi2.bangbang93.com/forge/minecraft/{mc}"
    try:
        req = urllib.request.Request(url, headers=hdr)
        data = json.load(urllib.request.urlopen(req, timeout=10))
    except Exception as e:
        print(f"{mc}: API failed {e}")
        continue
    
    # Check latest version
    last = data[-1]
    branch = last.get("branch", None)
    ver = last["version"]
    mcv = last.get("mcversion", mc)
    
    # Also sample a few versions randomly to check branch consistency
    branches = set()
    for v in data[-5:]:
        b = v.get("branch", None)
        branches.add(b if b else "NULL")
    
    print(f"{mc:10s} 最新={ver:25s}  branch={str(branch):10s}  样本branches={branches}")
