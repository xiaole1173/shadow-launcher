import os

for root, dirs, files in os.walk('src'):
    for f in files:
        if f.endswith('.cpp') or f.endswith('.h'):
            fp = os.path.join(root, f)
            with open(fp, 'r', encoding='utf-8', errors='replace') as ff:
                for i, line in enumerate(ff):
                    low = line.lower()
                    if 'getforge' in low:
                        print(f'{fp}:{i+1}: {line.rstrip()[:200]}')
