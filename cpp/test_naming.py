import urllib.request, json, sys

sys.stdout.reconfigure(encoding='utf-8')
hdr = {"User-Agent": "Mozilla/5.0"}

def test_url(url, label=None):
    try:
        req = urllib.request.Request(url, headers=hdr)
        resp = urllib.request.urlopen(req, timeout=8)
        cl = resp.headers.get("Content-Length", "?")
        size_kb = int(cl)//1024 if cl.isdigit() else "?"
        status = "HTTP200" if resp.status==200 else f"HTTP{resp.status}"
        print(f"  OK {status:8s} {size_kb:6s}KB  {label or ''}")
        resp.close()
        return int(cl) if cl.isdigit() else 0
    except Exception as e:
        code = str(e).split("Error ")[1][:3] if "Error" in str(e) else str(e)[:20]
        print(f"  NG {code:8s}       {label or ''}")
        return 0

# Fetch version lists to understand branch naming
for mc in ["1.7.2", "1.8.9", "1.9", "1.10", "1.7.10", "1.5.2"]:
    url = f"https://bmclapi2.bangbang93.com/forge/minecraft/{mc}"
    try:
        req = urllib.request.Request(url, headers=hdr)
        data = json.load(urllib.request.urlopen(req, timeout=10))
    except:
        continue
    last = data[-1]
    branch = last.get("branch", None)
    ver = last["version"]
    mcv = last.get("mcversion", mc)
    
    print(f"\n--- MC={mc} ver={ver} branch={branch} ---")
    
    # Format 1: mc-forge
    f1 = f"{mcv}-{ver}"
    test_url(f"https://bmclapi2.bangbang93.com/maven/net/minecraftforge/forge/{f1}/forge-{f1}-installer.jar", f"new: {f1}")
    
    # Format 2: mc-forge-mc (old)
    f2 = f"{mcv}-{ver}-{mcv}"
    test_url(f"https://bmclapi2.bangbang93.com/maven/net/minecraftforge/forge/{f2}/forge-{f2}-installer.jar", f"old: {f2}")
    
    # Format 3: mc-forge-BRANCH
    if branch:
        f3 = f"{mcv}-{ver}-{branch}"
        test_url(f"https://bmclapi2.bangbang93.com/maven/net/minecraftforge/forge/{f3}/forge-{f3}-installer.jar", f"br:  {f3}")

# User's claimed versions
print("\n--- User's versions ---")
for mc, ver in [("1.7.2", "1.7.2-10.12.2.1147"), ("1.9", "1.9-12.16.1.1938-1.9.0"), ("1.10", "1.10-12.18.0.2000-1.10.0")]:
    test_url(f"https://bmclapi2.bangbang93.com/maven/net/minecraftforge/forge/{ver}/forge-{ver}-installer.jar", f"{mc}: {ver}")
