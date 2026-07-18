"""
Mojang CDN speed test — 64 threads, official source, HTTP Range.
Downloads Minecraft 1.21.1 client.jar from Mojang CDN.
Uses only Python stdlib (no pip needed).
"""
import urllib.request
import concurrent.futures
import time
import hashlib
import os
import threading

URL = "https://piston-data.mojang.com/v1/objects/30c73b1c5da787909b2f73340419fdf13b9def88/client.jar"
OUTPUT = "test_client_1.21.1.jar"
WORKERS = 64
lock = threading.Lock()
total_received = [0]

def head_size():
    req = urllib.request.Request(URL, method='HEAD')
    req.add_header('User-Agent', 'ShadowLauncher/1.0')
    with urllib.request.urlopen(req, timeout=30) as resp:
        return int(resp.headers.get('Content-Length', 0))

def download_chunk(start, end, part_file, chunk_id):
    headers = {
        'User-Agent': 'ShadowLauncher/1.0',
        'Range': f'bytes={start}-{end}'
    }
    req = urllib.request.Request(URL, headers=headers)
    try:
        with urllib.request.urlopen(req, timeout=60) as resp:
            data = resp.read()
            with open(part_file, 'wb') as f:
                f.write(data)
            with lock:
                total_received[0] += len(data)
            return len(data)
    except Exception as e:
        print(f"  chunk[{chunk_id}] error: {e}")
        return 0

def main():
    size = head_size()
    if size == 0:
        print("ERROR: Could not determine file size")
        return

    print(f"Target: Minecraft 1.21.1 client.jar")
    print(f"Source: Mojang CDN (piston-data.mojang.com)")
    print(f"File size: {size / (1024*1024):.1f} MB")
    print(f"Workers: {WORKERS}")
    print(f"Policy: PreferOfficial (Mojang only, no mirrors)")
    print()

    chunk_size = size // WORKERS
    futures = []
    start_time = time.time()

    with concurrent.futures.ThreadPoolExecutor(max_workers=WORKERS) as executor:
        for i in range(WORKERS):
            s = i * chunk_size
            e = (s + chunk_size - 1) if i < WORKERS - 1 else size - 1
            pf = f"{OUTPUT}.part{i:04d}"
            futures.append(executor.submit(download_chunk, s, e, pf, i))

        concurrent.futures.wait(futures)

    elapsed = time.time() - start_time
    mbps = (total_received[0] / (1024*1024)) / elapsed

    # Merge parts
    with open(OUTPUT, 'wb') as out:
        for i in range(WORKERS):
            pf = f"{OUTPUT}.part{i:04d}"
            try:
                with open(pf, 'rb') as f:
                    out.write(f.read())
                os.remove(pf)
            except FileNotFoundError:
                pass

    sha = hashlib.sha1()
    with open(OUTPUT, 'rb') as f:
        sha.update(f.read())
    expected = "30c73b1c5da787909b2f73340419fdf13b9def88"

    print()
    print("=" * 55)
    print(f"  RESULT: {'OK' if sha.hexdigest() == expected else 'SHA1 MISMATCH'}")
    print(f"  Downloaded: {total_received[0]/(1024*1024):.2f} MB")
    print(f"  Time:       {elapsed:.2f}s")
    print(f"  Speed:      {mbps:.1f} MB/s ({mbps*8:.1f} Mbps)")
    print(f"  SHA1:       {'MATCH' if sha.hexdigest() == expected else 'MISMATCH'}")
    target = 3.0
    print(f"  Target:     >= {target} MB/s → {'PASS' if mbps >= target else 'BELOW'}")
    print("=" * 55)

    os.remove(OUTPUT)
    print(f"\nCleaned up {OUTPUT}")

if __name__ == '__main__':
    main()
