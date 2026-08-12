# -*- coding: utf-8 -*-
import io, sys, re
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
data = open(r'D:\latest-code\cpp\build\Release\logs\shadow_launcher_2026-08-10.log', encoding='utf-8', errors='replace').read()
lines = data.splitlines()

done = {}     # file -> time
failed = {}   # file -> [time]
starts = {}   # file -> count
switches = {}
for l in lines:
    if '[精卫]' not in l: continue
    if '[完成]' in l:
        f = l.split('[完成]', 1)[1].strip()
        done[f] = l[:19]
        continue
    if '[失败]' in l:
        rest = l.split('[失败]', 1)[1].strip()
        f = rest.split(':', 1)[0].strip()
        failed.setdefault(f, []).append(l[:19])
        continue
    m = re.search(r'文件=([^ ]+)', l)
    if not m: continue
    f = m.group(1)
    if '启动' in l:
        starts[f] = starts.get(f, 0) + 1
    if '慢速换源' in l:
        switches[f] = switches.get(f, 0) + 1

# 只显示 启动>1 或 未完成 的文件
print('%-52s %5s %5s %6s %s' % ('file', 'st', 'sw', 'done@', 'failed'))
allf = sorted(set(list(starts) + list(done) + list(failed)))
nodone = 0
for f in allf:
    st = starts.get(f, 0)
    d = done.get(f, '')
    fl = ','.join(t[11:] for t in failed.get(f, []))[:30]
    if st > 1 or not d or fl:
        print('%-52s %5d %5d %6s %s' % (f[:52], st, switches.get(f, 0), d[11:] if d else '--', fl))
        if not d: nodone += 1
print()
print('total files:', len(allf), '| never done:', len([f for f in allf if f not in done]))
print('never-done files:')
for f in sorted([f for f in allf if f not in done]):
    print('  ', f[:80], '| starts=', starts.get(f,0))
