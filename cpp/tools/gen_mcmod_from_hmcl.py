#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""从 HMCL 的 mod_data.txt 合并生成 mcmod_map.json。

数据说明：HMCL 的 mod_data.txt 文件头自述版权归 mcmod.cn（MC百科）。
本脚本只萃取事实字段（mcmod class id / CurseForge slug / 英文名），
并与既有抓取结果按 class id 合并（既有优先，HMCL 补缺；mr 仅来自既有）。
"""
import io, json, os, sys, shutil, datetime

HMCL_TXT = r"C:\Users\蔡朝彬\Downloads\HMCL-main\HMCL-main\HMCL\src\main\resources\assets\mod_data.txt"
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "qml", "mcmod_map.json")

def parse_hmcl(path):
    entries = []
    with io.open(path, encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            parts = line.split(";")
            if len(parts) < 6:
                continue
            cf = parts[0].strip().lower()
            mcmod = parts[1].strip()
            en = parts[4].strip()
            if not en:
                zh = parts[3].strip()
                if zh and all(0x20 <= ord(c) < 0x7f for c in zh):
                    en = zh
            if not mcmod.isdigit():
                continue
            id_ = int(mcmod)
            if id_ <= 0:
                continue
            entries.append((id_, cf, en))
    return entries

def main():
    hmcl = parse_hmcl(HMCL_TXT)

    with io.open(OUT, encoding="utf-8") as f:
        existing = json.load(f)

    # 备份
    bak = OUT + ".bak-" + datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    shutil.copyfile(OUT, bak)

    merged = {}  # id -> {"id":, "en":, "cf":, "mr":}
    def slot(id_):
        d = merged.get(id_)
        if d is None:
            d = {"id": id_, "en": "", "cf": "", "mr": ""}
            merged[id_] = d
        return d

    # 既有优先（保留我们抓到的 en/cf/mr）
    for e in existing:
        id_ = e.get("id")
        if not isinstance(id_, int) or id_ <= 0:
            continue
        d = slot(id_)
        for k in ("en", "cf", "mr"):
            v = (e.get(k) or "").strip()
            if v and not d[k]:
                d[k] = v.lower() if k in ("cf", "mr") else v
    # HMCL 补缺
    for id_, cf, en in hmcl:
        d = slot(id_)
        if en and not d["en"]:
            d["en"] = en
        if cf and not d["cf"]:
            d["cf"] = cf

    out = []
    for id_ in sorted(merged):
        d = merged[id_]
        o = {"id": id_}
        for k in ("en", "cf", "mr"):
            if d[k]:
                o[k] = d[k]
        if any(k in o for k in ("en", "cf", "mr")):
            out.append(o)

    with io.open(OUT, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, separators=(",", ":"))

    en = sum(1 for o in out if "en" in o)
    cf = sum(1 for o in out if "cf" in o)
    mr = sum(1 for o in out if "mr" in o)
    print("HMCL parsed :", len(hmcl))
    print("existing     :", len(existing))
    print("merged total :", len(out), " en:", en, " cf:", cf, " mr:", mr)
    print("backup       :", bak)
    print("written      :", OUT)

if __name__ == "__main__":
    main()
