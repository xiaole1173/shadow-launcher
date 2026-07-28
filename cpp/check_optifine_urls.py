#!/usr/bin/env python3
"""Check OptiFine download URL construction and test connectivity."""

import urllib.request, urllib.error
import re, sys, io

# Force UTF-8 output for emoji
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

mc_version = '1.16.5'
optifine_type = 'HD_U'
optifine_patch = 'G8'

# BMCLAPI URLs
bmcl_url = f'https://bmclapi2.bangbang93.com/optifine/{mc_version}/{optifine_type}/{optifine_patch}'
bmcl_url2 = f'https://bmclapi2.bangbang93.com/optifine/{mc_version}/{optifine_type}_{optifine_patch}'
print(f'[BMCLAPI our]     {bmcl_url}')
print(f'[BMCLAPI 主流启动器]    {bmcl_url2}')

# Official
filename = f'OptiFine_{mc_version}_{optifine_type}_{optifine_patch}.jar'
adload_url = f'https://optifine.net/adloadx?f={filename}'
print(f'[Official]        {adload_url}')
print(f'[Filename]        {filename}')

# Test BMCLAPI primary
print('\n=== BMCLAPI primary ===')
try:
    req = urllib.request.Request(bmcl_url, headers={'User-Agent': 'Mozilla/5.0'})
    resp = urllib.request.urlopen(req, timeout=15)
    data = resp.read()
    ct = resp.headers.get_content_type()
    print(f'  HTTP {resp.status} | Type: {ct} | Size: {len(data)} bytes')
    print(f'  Final URL: {resp.url}')
    is_zip = data[:2] == b'PK'
    print(f'  Valid ZIP: {is_zip}')
    if not is_zip:
        print(f'  Preview: {data[:100]}')
except urllib.error.HTTPError as e:
    print(f'  HTTP {e.code}: {e.reason}')
except Exception as e:
    print(f'  Error: {e}')

# Test BMCLAPI 主流启动器 alt
print('\n=== BMCLAPI 主流启动器 alt ===')
try:
    req = urllib.request.Request(bmcl_url2, headers={'User-Agent': 'Mozilla/5.0'})
    resp = urllib.request.urlopen(req, timeout=15)
    data = resp.read()
    print(f'  HTTP {resp.status} | Size: {len(data)} bytes')
    print(f'  Valid ZIP: {data[:2] == b"PK"}')
    if data[:2] != b'PK':
        print(f'  Preview: {data[:100]}')
except urllib.error.HTTPError as e:
    print(f'  HTTP {e.code}: {e.reason}')
except Exception as e:
    print(f'  Error: {e}')

# Test official adloadx
print('\n=== Official adloadx ===')
try:
    ua = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36'
    req = urllib.request.Request(adload_url, headers={'User-Agent': ua})
    resp = urllib.request.urlopen(req, timeout=15)
    html = resp.read().decode('utf-8', errors='replace')
    print(f'  HTTP {resp.status} | Page size: {len(html)} bytes')
    
    matches = re.findall(r'downloadx\?f=[^"\';\s]+', html)
    if matches:
        for i, m in enumerate(matches):
            dl_full = 'https://optifine.net/' + m
            print(f'  Download #{i+1}: {dl_full}')
            try:
                req2 = urllib.request.Request(dl_full, headers={'User-Agent': ua})
                resp2 = urllib.request.urlopen(req2, timeout=30)
                data2 = resp2.read()
                print(f'    HTTP {resp2.status} | Size: {len(data2)} bytes')
                print(f'    Valid ZIP: {data2[:2] == b"PK"}')
                if data2[:2] != b'PK':
                    print(f'    Preview: {data2[:100]}')
            except Exception as e2:
                print(f'    Download error: {e2}')
    else:
        print(f'  No downloadx link found')
        # Check if there's a captcha or rate limit
        if 'captcha' in html.lower():
            print('  (Rate-limited / captcha triggered)')
        print(f'  First 500 chars:\n{html[:500]}')
except urllib.error.HTTPError as e:
    print(f'  HTTP {e.code}: {e.reason}')
    try:
        body = e.read().decode('utf-8', errors='replace')[:500]
        print(f'  Body: {body}')
    except: pass
except Exception as e:
    print(f'  Error: {e}')

print('\nDone.')
