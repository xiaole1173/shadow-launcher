import os

d = r'D:\latest-code\cpp\build\Release'
required = [
    'libcef.dll', 'libcef.lib',
    'chrome_100_percent.pak', 'chrome_200_percent.pak', 'resources.pak',
    'icudtl.dat', 'v8_context_snapshot.bin', 'snapshot_blob.bin',
    'locales',
]

print('=== CEF runtime files in build output ===')
for f in required:
    fp = os.path.join(d, f)
    exists = os.path.exists(fp)
    if not exists:
        print(f'  {f:40s} ** MISSING **')
    elif f.endswith(('.bin', '.dat', '.pak')):
        size = os.path.getsize(fp)
        print(f'  {f:40s} OK  ({size/1024/1024:.1f} MB)')
    elif f == 'locales':
        sub = os.listdir(fp)
        print(f'  {f:40s} OK  ({len(sub)} files)')
        if sub:
            print(f'    Sample: {sub[0]}')
    else:
        size = os.path.getsize(fp)
        print(f'  {f:40s} OK  ({size/1024:.0f} KB)')

# Check for subprocess
print()
for sfx in ['exe']:
    sp = os.path.join(d, f'libcef_subprocess.{sfx}')
    if os.path.exists(sp):
        print(f'  {sp}  EXISTS')
        break
else:
    print(f'  libcef_subprocess.exe  ** NOT FOUND (need to copy manually)')

# Check CEF original Release dir
print()
cef_rel = r'D:\latest-code\cpp\thirdparty\cef\Release'
print('=== CEF source Release/ (original) ===')
all_files = sorted(os.listdir(cef_rel))
for f in all_files:
    fp = os.path.join(cef_rel, f)
    sz = os.path.getsize(fp)
    print(f'  {f:45s} {sz/1024/1024:8.1f} MB' if sz > 100000 else f'  {f:45s} {sz/1024:10.0f} KB')

# Check for subprocess in original CEF
print()
sp_orig = r'D:\latest-code\cpp\thirdparty\cef\Release\libcef_subprocess.exe'
if os.path.exists(sp_orig):
    print(f'libcef_subprocess.exe in original CEF: YES ({os.path.getsize(sp_orig)/1024:.0f} KB)')
else:
    print('libcef_subprocess.exe in original CEF: ** NOT FOUND')
    # Look for *subprocess* anywhere
    for root, dirs, files in os.walk(r'D:\latest-code\cpp\thirdparty\cef'):
        for f in files:
            if 'subprocess' in f.lower():
                print(f'  Found subprocess: {os.path.join(root, f)}')
