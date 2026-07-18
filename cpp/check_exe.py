import sys
with open(r'D:\latest-code\cpp\build\Release\ShadowLauncher.exe', 'rb') as f:
    data = f.read()

# Search for version strings
import re
# Find "1.0.0" and "0.1.0" and "0.1.0-beta.1"
for pat in [b'1.0.0', b'0.1.0', b'0.1.0-beta.1', b'SHADOW_DISPLAY']:
    count = data.count(pat)
    if count > 0:
        idx = data.find(pat)
        ctx = data[max(0,idx-20):idx+len(pat)+40]
        print(f'Found "{pat.decode(errors="replace")}" : {count}x, context: {ctx}')
    else:
        print(f'NOT found: "{pat.decode()}"')

# Check PE timestamp
import struct
pe_offset = struct.unpack_from('<I', data, 0x3C)[0]
timestamp = struct.unpack_from('<I', data, pe_offset+8)[0]
import datetime
print(f'PE timestamp: {datetime.datetime.utcfromtimestamp(timestamp)} UTC')
