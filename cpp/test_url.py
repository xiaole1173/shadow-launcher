import urllib.request, socket

socket.setdefaulttimeout(8)
UA = {"User-Agent": "Mozilla/5.0"}

# Try exotic URL patterns for 1.7.2 forge 10.12.2.1161
mc, fv = "1.7.2", "10.12.2.1161"

patterns = [
    # Standard new/old (already tested, keep for reference)
    ("maven std new", f"https://bmclapi2.bangbang93.com/maven/net/minecraftforge/forge/{mc}-{fv}/forge-{mc}-{fv}-installer.jar"),
    ("maven std old", f"https://bmclapi2.bangbang93.com/maven/net/minecraftforge/forge/{mc}-{fv}-{mc}/forge-{mc}-{fv}-{mc}-installer.jar"),
    # Different groupId (minecraftforge artifact instead of forge)
    ("mcforge artifact", f"https://bmclapi2.bangbang93.com/maven/net/minecraftforge/minecraftforge/{mc}-{fv}/minecraftforge-{mc}-{fv}-installer.jar"),
    # Different repo layout (no version in path)
    ("maven flat", f"https://bmclapi2.bangbang93.com/maven/net/minecraftforge/forge/forge-{mc}-{fv}-installer.jar"),
    # Old forge legacy path
    ("maven legacy", f"https://bmclapi2.bangbang93.com/maven/net/minecraftforge/forge/{mc}/{fv}/forge-{mc}-{fv}-installer.jar"),
    # Official forge maven
    ("official new", f"https://maven.minecraftforge.net/net/minecraftforge/forge/{mc}-{fv}/forge-{mc}-{fv}-installer.jar"),
    ("official old", f"https://maven.minecraftforge.net/net/minecraftforge/forge/{mc}-{fv}-{mc}/forge-{mc}-{fv}-{mc}-installer.jar"),
    # Official forge files server (old domain)
    ("files.mf.net", f"https://files.minecraftforge.net/maven/net/minecraftforge/forge/{mc}-{fv}-{mc}/forge-{mc}-{fv}-{mc}-installer.jar"),
]

def test(url):
    try:
        req = urllib.request.Request(url, headers=UA)
        resp = urllib.request.urlopen(req, timeout=8)
        cl = resp.headers.get("Content-Length", "?")
        return f"OK {cl}b"
    except Exception as e:
        code = getattr(e, 'code', 'ERR')
        return f"ERR {code}"

print(f"Testing MC={mc} Forge={fv}")
print("-" * 80)
for name, url in patterns:
    print(f"  {name:<20}: {test(url)}")

# Also test 1.9 and 1.10 with official maven and old format
print("\n\n1.9 (12.16.1.1938) and 1.10 (12.18.0.2000) with OLD format from official:")
for mc2, fv2 in [("1.9", "12.16.1.1938"), ("1.10", "12.18.0.2000")]:
    url = f"https://maven.minecraftforge.net/net/minecraftforge/forge/{mc2}-{fv2}-{mc2}/forge-{mc2}-{fv2}-{mc2}-installer.jar"
    print(f"  {mc2} {fv2} old format official: {test(url)}")
