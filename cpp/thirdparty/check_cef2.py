import os

d = r'D:\latest-code\cpp\thirdparty\cef\Release'
print('=== All files in CEF Release/ ===')
all_files = sorted(os.listdir(d))
for f in all_files:
    fp = os.path.join(d, f)
    sz = os.path.getsize(fp)
    mark = "  << MISSING from build" if not os.path.exists(os.path.join(r'D:\latest-code\cpp\build\Release', f)) else ""
    print(f'  {f:45s} {sz/1024/1024:8.1f} MB{mark}' if sz > 100000 else f'  {f:45s} {sz/1024:10.0f} KB{mark}')

print()
print('=== CEF Resources/ ===')
res = r'D:\latest-code\cpp\thirdparty\cef\Resources'
if os.path.exists(res):
    for f in sorted(os.listdir(res)):
        fp = os.path.join(res, f)
        if os.path.isdir(fp):
            print(f'  {f}/ ({len(os.listdir(fp))} items)')
        else:
            print(f'  {f:45s} {os.path.getsize(fp)/1024:10.0f} KB')
