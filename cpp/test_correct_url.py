import urllib.request

hdr = {"User-Agent": "Mozilla/5.0"}
urls = [
    ("1.7.2",  "1.7.2-10.12.2.1147"),
    ("1.9",    "1.9-12.16.1.1938-1.9.0"),
    ("1.10",   "1.10-12.18.0.2000-1.10.0"),
]

for mc, ver in urls:
    for label, base in [("官方", "https://maven.minecraftforge.net"), ("BMCLAPI", "https://bmclapi2.bangbang93.com")]:
        url = f"{base}/maven/net/minecraftforge/forge/{ver}/forge-{ver}-installer.jar"
        try:
            req = urllib.request.Request(url, headers=hdr)
            resp = urllib.request.urlopen(req, timeout=10)
            cl = resp.headers.get("Content-Length", "?")
            print(f"[{mc}] {label} HTTP {resp.status}  Size={cl}")
            resp.close()
        except Exception as e:
            err = str(e)[:80]
            print(f"[{mc}] {label} ERR: {err}")
