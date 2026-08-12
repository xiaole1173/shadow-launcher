# -*- coding: utf-8 -*-
import io, sys, re
sys.stdout.reconfigure(encoding='utf-8', errors='replace')
data = open(r'D:\latest-code\cpp\build\Release\logs\shadow_launcher_2026-08-10.log', encoding='utf-8', errors='replace').read()
lines = data.splitlines()

# 只看 18:29 之后的精卫日志
starts = {}; done = {}; failed = {}; switches = {}
first_ts = None; last_ts = None
for l in lines:
    if '[精卫]' not in l: continue
    ts = l[:19]
    if ts < '18:29:00': continue
    if first_ts is None: first_ts = ts
    last_ts = ts
    if '[完成]' in l:
        f = l.split('[完成]', 1)[1].strip()
        done[f] = ts
        continue
    if '[失败]' in l:
        rest = l.split('[失败]', 1)[1].strip()
        f = rest.split(':', 1)[0].strip()
        failed.setdefault(f, []).append(ts)
        continue
    m = re.search(r'文件=([^ ]+)', l)
    if not m: continue
    f = m.group(1)
    if '启动' in l:
        starts[f] = starts.get(f, 0) + 1
    elif '慢速换源' in l:
        switches[f] = switches.get(f, 0) + 1

print('period:', first_ts, '→', last_ts)
print('distinct files started:', len(starts))
print('done:', len(done), '| failed:', len(failed), '| never-done:', len([f for f in starts if f not in done]))
print()
print('--- failed files (first 25) ---')
for f, ts in sorted(failed.items()):
    if len(f) < 100:
        print('%-50s %s x%d' % (f[:50], ts[11:], len(ts)))
print()
print('--- switch-heavy files ---')
for f, c in sorted(switches.items(), key=lambda x: -x[1])[:10]:
    print('%-50s sw=%d st=%d done=%s' % (f[:50], c, starts.get(f,0), done.get(f,'--')))
