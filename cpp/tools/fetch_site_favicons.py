# -*- coding: utf-8 -*-
"""
Fetch favicons for all sites in qml/websites_data.json.
Output: icons/sites/<icon_key>.png (converted to PNG via PIL).

Strategy per site:
  1. https://<host>/favicon.ico
  2. https://<host>/favicon.png
  3. https://<host>/apple-touch-icon.png
  4. http://<host>/favicon.ico
  5. https://icon.horse/icon/<host>   (third-party PNG fallback)

The script skips a site if any strategy yields parseable image bytes.
Result summary printed at end; a mark file with failures is written for review.
"""
import json
import os
import sys
import urllib.parse
import urllib.request
import io

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # cpp/
DATA = os.path.join(ROOT, "qml", "websites_data.json")
OUT_DIR = os.path.join(ROOT, "icons", "sites")

UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0 Safari/537.36"
TIMEOUT = 8


def host_of(url):
    return urllib.parse.urlparse(url).hostname or ""


def strategies(host):
    schemes = []
    for proto in ("https", "http"):
        schemes.append(f"{proto}://{host}/favicon.ico")
        schemes.append(f"{proto}://{host}/favicon.png")
        schemes.append(f"{proto}://{host}/apple-touch-icon.png")
    schemes.append(f"https://icon.horse/icon/{host}")
    return schemes


def fetch(url):
    req = urllib.request.Request(url, headers={"User-Agent": UA, "Accept": "image/*,*/*;q=0.8"})
    with urllib.request.urlopen(req, timeout=TIMEOUT) as resp:
        return resp.read()


def to_png_bytes(data, key):
    """Convert arbitrary image bytes (ico/png/jpg) to PNG bytes via PIL."""
    im = Image.open(io.BytesIO(data))
    # Normalise to RGBA, square-crop from centre at 64x64
    im = im.convert("RGBA")
    w, h = im.size
    side = min(w, h)
    left = (w - side) // 2
    top = (h - side) // 2
    im = im.crop((left, top, left + side, top + side)).resize((64, 64), Image.LANCZOS)
    buf = io.BytesIO()
    im.save(buf, "PNG")
    return buf.getvalue()


def main():
    with open(DATA, "r", encoding="utf-8") as f:
        data = json.load(f)
    os.makedirs(OUT_DIR, exist_ok=True)

    seen = {}
    ok, fail = [], []
    for cat in data["categories"]:
        for site in cat["sites"]:
            key = site["icon"]
            host = host_of(site["url"])
            if key in seen:
                continue  # duplicate icon key (e.g. 本启动器 two cards) — fetch once
            seen[key] = True
            if not host:
                fail.append((key, site["url"], "no-host"))
                continue
            saved = False
            for s in strategies(host):
                try:
                    raw = fetch(s)
                    png = to_png_bytes(raw, key)
                    with open(os.path.join(OUT_DIR, key + ".png"), "wb") as f:
                        f.write(png)
                    ok.append(key)
                    saved = True
                    print(f"OK   {key:28s} <- {s}")
                    break
                except Exception as e:
                    last_err = f"{type(e).__name__}: {e}"
            if not saved:
                fail.append((key, site["url"], last_err))
                print(f"FAIL {key:28s} {last_err}")

    print("\n==== SUMMARY ====")
    print(f"OK   : {len(ok)}")
    print(f"FAIL : {len(fail)}")
    for k, u, e in fail:
        print(f"  - {k} ({u}) {e}")

    with open(os.path.join(ROOT, "tools", "favicon_failures.txt"), "w", encoding="utf-8") as f:
        for k, u, e in fail:
            f.write(f"{k}\t{u}\t{e}\n")


if __name__ == "__main__":
    main()
