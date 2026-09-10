#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""从 PCL 的 WikiEntries.txt 萃取 Modrinth slug(mr) 补进 mcmod_map.json。

只取事实字段：class id(行号) + Modrinth slug；不改 en/cf、不搬中文名。
平台语义(与 PCL WikiEntry.cs 逐行一致)：
  @slug        -> mr = slug
  slug@        -> cf = slug 且 mr = slug
  cf@mr        -> cf = 左, mr = 右
  slug(无@)    -> cf = slug
末尾一行为浏览量，舍弃。
"""
import io, json, os, shutil, datetime

PCL_TXT = r"C:\Users\蔡朝彬\Downloads\PCL-main\PCL-main\PCLCS\Resource\WikiEntries.txt"
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "qml", "mcmod_map.json")

def parse_pcl_mr(path):
    """return {class_id: [mr_slug, ...]}（有序、去重、小写）"""
    with io.open(path, encoding="utf-8") as f:
        lines = f.read().splitlines()
    lines = lines[:-1]  # 末行浏览量
    out = {}
    line_no = 0
    for line in lines:
        line_no += 1
        if line == "":
            continue
        for entry in line.split('¨'):
            if not entry.strip():
                continue
            slugpart = entry.split('|')[0].strip()
            if not slugpart:
                continue
            mr = None
            if slugpart.startswith('@'):
                mr = slugpart[1:]
            elif slugpart.endswith('@'):
                mr = slugpart[:-1]
            elif '@' in slugpart:
                mr = slugpart.split('@')[1]
            # else: 纯 cf，无 mr
            if mr:
                mr = mr.strip().lower()
                if mr:
                    lst = out.setdefault(line_no, [])
                    if mr not in lst:
                        lst.append(mr)
    return out

def main():
    pcl = parse_pcl_mr(PCL_TXT)

    with io.open(OUT, encoding="utf-8") as f:
        cur = json.load(f)

    bak = OUT + ".bak-mr-" + datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    shutil.copyfile(OUT, bak)

    primary = {}  # id -> 主条目(不含 mr，含 en/cf)
    mr_list = {}  # id -> 有序 mr slug 列表
    for e in cur:
        iid = e.get("id")
        if not isinstance(iid, int) or iid <= 0:
            continue
        if iid not in primary:
            primary[iid] = {"id": iid}
            mr_list[iid] = []
        for k in ("en", "cf"):
            if not primary[iid].get(k) and e.get(k):
                primary[iid][k] = e[k]
        if e.get("mr") and e["mr"] not in mr_list[iid]:
            mr_list[iid].append(e["mr"])

    pcl_ids_new = 0
    for iid, slugs in pcl.items():
        if iid not in primary:
            primary[iid] = {"id": iid}
            mr_list[iid] = []
            pcl_ids_new += 1
        for s in slugs:
            if s not in mr_list[iid]:
                mr_list[iid].append(s)

    out = []
    for iid in sorted(primary):
        mrs = mr_list[iid]
        p = dict(primary[iid])
        if mrs:
            p["mr"] = mrs[0]
        out.append(p)
        for s in mrs[1:]:
            out.append({"id": iid, "mr": s})

    # 去掉只有 id、没有任何键的条目
    out = [o for o in out if any(k in o for k in ("en", "cf", "mr"))]

    with io.open(OUT, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, separators=(",", ":"))

    mr_total = sum(1 for o in out if o.get("mr"))
    print("PCL mr id 数 :", len(pcl))
    print("其中新增条目 :", pcl_ids_new)
    print("现有条目数   :", len(cur))
    print("输出条目数   :", len(out))
    print("含 mr 条目数 :", mr_total)
    print("backup:", bak)

if __name__ == "__main__":
    main()
