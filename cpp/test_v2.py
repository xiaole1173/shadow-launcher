import urllib.request, sys
hdr = {"User-Agent": "Mozilla/5.0"}

# Test a URL we know works
url = "https://bmclapi2.bangbang93.com/maven/net/minecraftforge/forge/1.10.2-12.18.3.2511/forge-1.10.2-12.18.3.2511-installer.jar"
try:
    req = urllib.request.Request(url, headers=hdr)
    r = urllib.request.urlopen(req, timeout=15)
    print(f"1.10.2: HTTP{r.status} Size={r.headers.get('Content-Length','?')}")
    r.close()
except Exception as e:
    print(f"1.10.2 FAIL: {e}")

# Test user's versions
tests = [
    ("1.7.2", "1.7.2-10.12.2.1147"),
    ("1.9", "1.9-12.16.1.1938-1.9.0"),
    ("1.10", "1.10-12.18.0.2000-1.10.0"),
]
for label, ver in tests:
    url = f"https://bmclapi2.bangbang93.com/maven/net/minecraftforge/forge/{ver}/forge-{ver}-installer.jar"
    try:
        req = urllib.request.Request(url, headers=hdr)
        r = urllib.request.urlopen(req, timeout=15)
        cl = r.headers.get("Content-Length", "?")
        kb = int(cl)//1024 if cl.isdigit() else "?"
        print(f"{label}: HTTP{r.status} Size={cl} ({kb}KB)")
        r.close()
    except Exception as e:
        msg = str(e)
        if "404" in msg:
            print(f"{label}: 404 Not Found")
        elif "403" in msg:
            print(f"{label}: 403 Forbidden")
        else:
            print(f"{label}: {msg[:100]}")
