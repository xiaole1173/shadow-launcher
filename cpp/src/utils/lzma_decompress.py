"""
Shadow Launcher LZMA decompressor helper.
Reads LZMA/ALONE-format data from stdin, writes decompressed output to stdout.
Usage: python decompress_lzma.py < input.lzma > output.bin
"""
import sys, struct, lzma

data = sys.stdin.buffer.read()

if len(data) < 13:
    sys.stderr.write(f'ERROR: Input too small ({len(data)} bytes, need >=13)\n')
    sys.exit(1)

# Parse LZMA ALONE header (13 bytes)
props_byte = data[0]
dict_size = struct.unpack_from('<I', data, 1)[0]
uncomp_size = struct.unpack_from('<Q', data, 5)[0]

lc = props_byte % 9
lp = (props_byte // 9) % 5
pb = props_byte // 45

raw_data = data[13:]

try:
    # First try FORMAT_ALONE (standard .lzma file format)
    decomp = lzma.decompress(data, format=lzma.FORMAT_ALONE)
    sys.stdout.buffer.write(decomp)
except Exception as e1:
    try:
        # Fallback: FORMAT_RAW with explicit filters
        filters = [{'id': lzma.FILTER_LZMA1, 'dict_size': dict_size,
                     'lc': lc, 'lp': lp, 'pb': pb}]
        decomp = lzma.decompress(raw_data, format=lzma.FORMAT_RAW, filters=filters)
        sys.stdout.buffer.write(decomp)
    except Exception as e2:
        try:
            # Last resort: try FORMAT_AUTO (XZ or ALONE)
            decomp = lzma.decompress(data)
            sys.stdout.buffer.write(decomp)
        except Exception as e3:
            sys.stderr.write(f'ERROR: LZMA decompression failed\n  FORMAT_ALONE: {e1}\n  FORMAT_RAW: {e2}\n  FORMAT_AUTO: {e3}\n')
            sys.exit(1)
