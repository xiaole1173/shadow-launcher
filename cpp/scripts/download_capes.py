#!/usr/bin/env python3
"""
Download official Java Edition cape PNG textures from textures.minecraft.net
using confirmed hashes from Minecraft Wiki.
"""
import os, re
import requests
from pathlib import Path

OUTPUT_DIR = Path(r"D:\latest-code\cpp\resources\capes")
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

SESSION = requests.Session()
SESSION.headers.update({
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"
})

# All confirmed texture hashes from Minecraft Wiki
CAPE_HASHES = {
    # ===== CONFIRMED =====
    "migrator":              "2340c0e03dd24a11b15a8b33c2a7e9e32abb2051b2481d0ba7defd635ca7a933",
    "vanilla":               "f9a76537647989f9a0b6d001e320dac591c359e9e61a31f4ce11c88f207f0ad4",
    "menace":                "dbc21e222528e30dc88445314f7be6ff12d3aeebc3c192054fba7e3b3f8c77b1",
    "15th_anniversary":      "cd9d82ab17fd92022dbd4a86cde4c382a7540e117fae7b9a2853658505a80625",
    "cherry_blossom":        "afd553b39358a24edfe3b8a9a939fa5fa4faa4d9a9c3d6af8eafb377fa05c2bb",
    "mcc_15th_year":         "56c35628fe1c4d59dd52561a3d03bfa4e1a76d397c8b9c476c2f77cb6aebb1df",
    "followers":             "569b7f2a1d00d26f30efe3f9ab9ac817b1e6d35f4f3cfb0324ef2d328223d350",
    "purple_heart":          "cb40a92e32b57fd732a00fc325e7afb00a7ca74936ad50d8e860152e482cfbde",
    "founders":              "99aba02ef05ec6aa4d42db8ee43796d6cd50e4b2954ab29f0caeb85f96bf52a1",
    # ===== TO BE VERIFIED VIA WEB SEARCH =====
    # Placeholder - will be replaced after web searches
}

def download(url, filepath, timeout=15):
    """Download and save. Returns True on success."""
    try:
        r = SESSION.get(url, timeout=timeout, stream=True)
        r.raise_for_status()
        ct = r.headers.get("Content-Type", "")
        if "text/html" in ct and len(r.content) < 500:
            return False
        with open(filepath, "wb") as f:
            for chunk in r.iter_content(8192):
                f.write(chunk)
        sz = os.path.getsize(filepath)
        if sz < 200:
            os.remove(filepath)
            return False
        # Validate it's a real PNG
        with open(filepath, "rb") as f:
            header = f.read(4)
        if header != b"\x89PNG":
            os.remove(filepath)
            print(f"    NOT A PNG (deleted)")
            return False
        print(f"    OK ({sz} bytes)")
        return True
    except Exception as e:
        print(f"    FAIL: {e}")
        return False

def main():
    print("=" * 60)
    print("  Downloading Java Edition Cape Textures")
    print("  Source: textures.minecraft.net (official)")
    print("=" * 60)
    
    ok_list = []
    fail_list = []
    
    for name, hash_val in CAPE_HASHES.items():
        filename = f"{name}.png"
        filepath = OUTPUT_DIR / filename
        if filepath.exists() and filepath.stat().st_size > 200:
            # Verify it's actually a PNG (not leftover HTML)
            with open(filepath, "rb") as f:
                if f.read(4) == b"\x89PNG":
                    print(f"  [{name}] SKIP (exists, valid PNG)")
                    ok_list.append(name)
                    continue
                else:
                    print(f"  [{name}] Removing invalid file...")
                    filepath.unlink()
        
        url = f"http://textures.minecraft.net/texture/{hash_val}"
        print(f"  [{name}] Downloading...")
        if download(url, str(filepath)):
            ok_list.append(name)
        else:
            fail_list.append(name)
    
    print("\n" + "=" * 60)
    print(f"  Downloaded: {len(ok_list)}/{len(CAPE_HASHES)}")
    for n in ok_list:
        p = OUTPUT_DIR / f"{n}.png"
        print(f"    [OK] {n}.png ({p.stat().st_size} bytes)")
    if fail_list:
        print(f"  Failed: {len(fail_list)}")
        for n in fail_list:
            print(f"    [--] {n}.png")
    
    # List all valid PNGs
    print("\n--- All capes in directory ---")
    for p in sorted(OUTPUT_DIR.glob("*.png")):
        if p.stat().st_size > 200:
            with open(p, "rb") as f:
                if f.read(4) == b"\x89PNG":
                    print(f"  {p.name}: {p.stat().st_size} bytes")

if __name__ == "__main__":
    main()
