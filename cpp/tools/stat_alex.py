# -*- coding: utf-8 -*-
import io, sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
data = open(r'D:\latest-code\cpp\build\Release\logs\shadow_launcher_2026-08-10.log', encoding='utf-8', errors='replace').read()
lines = data.splitlines()
for i, l in enumerate(lines):
    if 'alexscaves' in l or ('精卫' in l and 'minecraftdungeons' in l):
        print(i, l[:210])
