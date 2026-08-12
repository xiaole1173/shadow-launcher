import struct

path = r'build\Release\ShadowLauncher.exe'
data = open(path, 'rb').read()
e_lfanew = struct.unpack_from('<I', data, 0x3C)[0]
nsec = struct.unpack_from('<H', data, e_lfanew+6)[0]
opt = e_lfanew + 24
sec = opt + struct.unpack_from('<H', data, e_lfanew+20)[0]
sections = []
for i in range(nsec):
    off = sec + i*40
    name = data[off:off+8].rstrip(b'\x00').decode('ascii', 'replace')
    vsize, vaddr, rsize, roff = struct.unpack_from('<IIII', data, off+8)
    sections.append((name, vaddr, vsize, roff, rsize))

def rva_to_off(rva):
    for name, vaddr, vsize, roff, rsize in sections:
        if vaddr <= rva < vaddr + max(vsize, rsize):
            return roff + (rva - vaddr)
    return None

opt = e_lfanew + 24
imp_rva = struct.unpack_from('<I', data, opt+120)[0]
imp_size = struct.unpack_from('<I', data, opt+124)[0]
nimp = imp_size // 20
off = rva_to_off(imp_rva)
print(f'imports: {nimp} entries at RVA {hex(imp_rva)}')

# Build slot -> name map by walking each descriptor's IAT (OriginalFirstThunk may be 0 -> use FirstThunk)
for i in range(nimp):
    ent = off + i*20
    oft_rva, ts, fwd, name_rva, iat_rva = struct.unpack_from('<IIIII', data, ent)
    if name_rva == 0:
        continue
    dllname = data[rva_to_off(name_rva):].split(b'\x00')[0].decode()
    thunk_rva = oft_rva if oft_rva else iat_rva
    thunk_off = rva_to_off(thunk_rva)
    iat_off = rva_to_off(iat_rva)
    names = []
    for slot in range(512):
        val = struct.unpack_from('<Q', data, thunk_off + slot*8)[0]
        if val == 0:
            break
        if val & 0x8000000000000000:
            fn = f'ord_{val & 0xFFFF}'
        else:
            hint_off = rva_to_off(val & 0x7FFFFFFF)
            if hint_off is None:
                fn = '??'
            else:
                fn = data[hint_off+2:].split(b'\x00')[0].decode()
        names.append((iat_rva + slot*8, fn))
    if names:
        lo = min(a for a, _ in names)
        hi = max(a for a, _ in names)
        print(f'{dllname}: IAT {hex(lo)}-{hex(hi)} ({len(names)} entries)')
        # show entries overlapping our range of interest (RVA)
        for a, fn in names:
            if 0x315740 <= a <= 0x315C00:
                print(f'   {hex(a)} = {fn}')
