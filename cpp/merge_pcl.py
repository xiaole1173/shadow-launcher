# -*- coding: utf-8 -*-
"""合并 PCL WikiEntries.txt 汉化库到 mod_zh.json（影确认的现有条目优先）。"""
import json, re, sys, io

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

PCL = r"C:\Users\蔡朝彬\Downloads\PCL-main\PCL-main\PCLCS\Resource\WikiEntries.txt"
CUR = r"D:\latest-code\cpp\qml\mod_zh.json"
OUT = r"D:\latest-code\cpp\qml\mod_zh_merged.json"
REPORT = r"D:\latest-code\cpp\merge_pcl_report.txt"

def norm(s):
    return re.sub(r'[^a-z0-9]', '', s.lower())

def title_case(s):
    return s.replace('-', ' ').title()

def has_cjk(s):
    return any('\u4e00' <= c <= '\u9fff' for c in s)

def clean_en(s):
    s = s.strip().strip("'\"‘’“”")
    s = re.split(r'\s*/\s*', s)[0]  # 去 "/" 分隔的多个英文名
    s = re.split(r'\s*[||]\s*', s)[0]
    return s.strip()

with open(PCL, encoding='utf-8') as f:
    lines = f.read().splitlines()

data_lines = lines[:-1]

entries = {}  # norm(en) -> {"en", "zh"}
skipped_num = 0
skipped_nozh = 0

for line in data_lines:
    if not line.strip():
        continue
    for entry in line.split('\u00a8'):
        entry = entry.strip()
        if not entry:
            continue
        parts = entry.split('|')
        if len(parts) < 2:
            continue
        slugs_str = parts[0].strip()
        zh_full = parts[-1].strip()

        mr = cf = None
        if slugs_str.startswith('@'):
            mr = slugs_str[1:].strip()
        elif slugs_str.endswith('@'):
            cf = slugs_str[:-1].strip(); mr = cf
        elif '@' in slugs_str:
            cf, mr = slugs_str.split('@', 1); cf = cf.strip(); mr = mr.strip()
        else:
            cf = slugs_str.strip()

        # '*' 占位符 → 用 slug 推导英文名
        if '*' in zh_full:
            base = mr if (mr and not mr.isdigit()) else cf if (cf and not cf.isdigit()) else ''
            zh_full = zh_full.replace('*', ' (' + title_case(base) + ')' if base else '')

        # 剥离第一个「不含中文」的括号（英文/外文名），取其前为 zh、其内为 en
        zh = zh_full
        en = None
        for m in re.finditer(r'[（(]([^（）()]*)[）)]', zh_full):
            inner = m.group(1).strip()
            if not has_cjk(inner):
                en = clean_en(inner)
                zh = (zh_full[:m.start()] + zh_full[m.end():]).strip()
                break
        if not en or not re.search(r'[A-Za-z]', en):
            base = mr if (mr and not mr.isdigit()) else cf if (cf and not cf.isdigit()) else ''
            if base and re.search(r'[A-Za-z]', base):
                en = title_case(base)
        # 清理 zh：去尾部变体标记 + 多余空白
        zh = re.sub(r'\s*[-–—]\s*[^-–—]*$', '', zh).strip()
        zh = re.sub(r'\s+', ' ', zh).strip()
        if not en or not re.search(r'[A-Za-z]', en):
            skipped_num += 1
            continue
        if not zh or not has_cjk(zh):
            skipped_nozh += 1
            continue
        # 过滤：zh 残留外文括号 / 箭头·emoji 等特殊符号
        if re.search(r'[（(][^（）()]*[）)]', zh):
            skipped_nozh += 1
            continue
        if re.search(r'[\u2190-\u27BF\u2B00-\u2BFF\uE000-\uF8FF\U0001F000-\U0001FAFF]', zh):
            skipped_nozh += 1
            continue
        k = norm(en)
        if k and k not in entries:
            entries[k] = {"en": en, "zh": zh}

# 载入现有（影确认优先）
with open(CUR, encoding='utf-8') as f:
    cur = json.load(f)
existing = {}
for it in cur:
    en = it.get('en', ''); zh = it.get('zh', ''); aliases = it.get('aliases', [])
    k = norm(en)
    if k:
        existing[k] = {"en": en, "zh": zh, "aliases": aliases}

merged = dict(existing)
added = 0
for k, v in entries.items():
    if k not in merged:
        merged[k] = {"en": v["en"], "zh": v["zh"], "aliases": []}
        added += 1

out_list = sorted(merged.values(), key=lambda x: x['en'].lower())
with open(OUT, 'w', encoding='utf-8') as f:
    json.dump(out_list, f, ensure_ascii=False, indent=2)

# 报告
rep = []
rep.append(f"PCL parsed entries: {len(entries)}")
rep.append(f"  skipped (no english / numeric id): {skipped_num}")
rep.append(f"  skipped (no cjk in zh): {skipped_nozh}")
rep.append(f"existing (confirmed): {len(existing)}")
rep.append(f"merged total: {len(out_list)} (added {added} from PCL)")
rep.append(f"output: {OUT}")
rep.append("")
rep.append("=== sanity: entries whose en is pure-ish numeric or suspicious ===")
sus = [it for it in out_list if not re.search(r'[A-Za-z]{2}', it['en'])]
for it in sus[:40]:
    rep.append(f"  [{it['en']}] -> {it['zh']}")
rep.append("")
rep.append("=== first 60 samples (alphabetical) ===")
for it in out_list[:60]:
    rep.append(f"  {it['en']}  ->  {it['zh']}")
rep.append("")
rep.append("=== spot check known mods ===")
for probe in ['Sodium', 'Iris Shaders', 'Simple Voice Chat', 'Applied Energistics 2', 'Create', 'JEI', 'Sodium Reforged', 'Occultism', 'Distant Horizons']:
    k = norm(probe)
    hit = [it for it in out_list if norm(it['en']) == k]
    rep.append(f"  {probe}: " + ("; ".join(f"{it['en']} -> {it['zh']}" for it in hit) if hit else "NOT FOUND"))

with open(REPORT, 'w', encoding='utf-8') as f:
    f.write("\n".join(rep))

print(f"PCL parsed: {len(entries)} | existing: {len(existing)} | merged: {len(out_list)} (added {added})")
print(f"report: {REPORT}")
print(f"output: {OUT}")