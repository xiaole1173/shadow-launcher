#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_mcmod_map.py — 自研脚本：从 MC百科公开 mod 列表生成「class-id 索引映射」。

只提取 ID 映射所需的极简字段（classId / 英文名 / CurseForge slug / Modrinth slug），
不搬运百科的介绍正文、物品、教程等任何内容，符合「不搬运百科数据」的硬规则。

数据来源（MC 百科自身公开入口）：
    https://www.mcmod.cn/sitemap1.xml       —— 全站模组 class 页清单
    https://www.mcmod.cn/class/{id}.html    —— 页面内嵌 CurseForge / Modrinth 外链

用法：
    python tools/gen_mcmod_map.py                 # 全量抓取，输出 qml/mcmod_map.json
    python tools/gen_mcmod_map.py --limit 200     # 只抓前 200 个（冒烟测试）
    python tools/gen_mcmod_map.py --resume        # 从断点缓存继续（自动）
    python tools/gen_mcmod_map.py --workers 16    # 并发数

输出（qml/mcmod_map.json）：
    [ {"id": 564, "en": "Automagy 2", "cf": "automagy"}, ... ]
    字段均可缺省（无英文名则省略 en，无外链则省略 cf/mr）。

断点：tools/_mcmod_map_resume.jsonl（每行一个已抓取的 id 结果），
     失败（网络/解析异常）不落盘，下次 --resume 会重试。
"""
import argparse
import base64
import concurrent.futures as cf
import html
import json
import os
import re
import sys
import threading
import time
from urllib.parse import unquote, urlsplit

import requests

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
OUT_PATH = os.path.join(ROOT, "qml", "mcmod_map.json")
RESUME_PATH = os.path.join(HERE, "_mcmod_map_resume.jsonl")

_TL = threading.local()


def _session():
    s = getattr(_TL, "s", None)
    if s is None:
        s = _TL.s = requests.Session()
    return s


UA = ("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
      "(KHTML, like Gecko) Chrome/126.0 Safari/537.36 ShadowLauncher/1.0")

CLASS_RE = re.compile(r"class/(\d+)\.html")
H4_RE = re.compile(r"<h4[^>]*>(.*?)</h4>", re.S)
TAG_RE = re.compile(r"<[^>]+>")
# <a ...> 标签内提取 data-original-title 与 href（属性顺序不定，分两段提取）
A_RE = re.compile(r"<a\b[^>]*>", re.S)
TITLE_ATTR_RE = re.compile(r'''data-original-title=["']([^"']+)["']''')
HREF_ATTR_RE = re.compile(r'''href=["']([^"']+)["']''')

CF_HOSTS = {"www.curseforge.com", "curseforge.com", "legacy.curseforge.com"}
MR_HOSTS = {"modrinth.com", "www.modrinth.com"}


def decode_target(b64: str):
    """link.mcmod.cn/target/<b64> -> 原始 URL；失败返回空。"""
    try:
        raw = base64.b64decode(b64.strip() + "===", validate=False).decode("utf-8", "ignore")
        return raw.strip()
    except Exception:
        return ""


def slug_from_url(url: str):
    """返回 (platform, slug)；无法识别返回 (None, None)。platform ∈ {cf, mr}。"""
    if not url:
        return None, None
    try:
        p = urlsplit(url)
        host = (p.hostname or "").lower()
        path = unquote(p.path or "").strip("/")
        parts = [x for x in path.split("/") if x]
        if host in CF_HOSTS and len(parts) >= 3 and parts[0] == "minecraft":
            # /minecraft/{mc-mods|modpacks|texture-packs|shaders}/{slug}
            if parts[1] in {"mc-mods", "modpacks", "texture-packs", "shaders"} and parts[2]:
                return "cf", parts[2]
        if host in MR_HOSTS and len(parts) >= 2:
            # /{mod|resourcepack|shader|datapack|modpack}/{slug}
            if parts[0] in {"mod", "resourcepack", "shader", "datapack", "modpack"} and parts[1]:
                return "mr", parts[1]
    except Exception:
        return None, None
    return None, None


def parse_page(html_text: str, class_id: int):
    title_en = ""
    m = H4_RE.search(html_text)
    if m:
        title_en = TAG_RE.sub(" ", m.group(1))
        title_en = html.unescape(title_en).strip()
        title_en = re.sub(r"\s+", " ", title_en)

    cf_slug = ""
    mr_slug = ""
    for a in A_RE.finditer(html_text):
        tag = a.group(0)
        title = TITLE_ATTR_RE.search(tag)
        if not title:
            continue
        kind = title.group(1).strip().lower()
        if kind not in {"curseforge", "modrinth"}:
            continue
        href = HREF_ATTR_RE.search(tag)
        if not href:
            continue
        target = href.group(1).strip()
        if "/target/" not in target:
            # 极少数直接外链
            url = target
        else:
            b64 = target.rsplit("/", 1)[-1]
            url = decode_target(b64)
        plat, slug = slug_from_url(url)
        if plat == "cf" and not cf_slug:
            cf_slug = slug
        elif plat == "mr" and not mr_slug:
            mr_slug = slug
        if cf_slug and mr_slug:
            break

    if not title_en and not cf_slug and not mr_slug:
        return None
    entry = {"id": class_id}
    if title_en:
        entry["en"] = title_en
    if cf_slug:
        entry["cf"] = cf_slug
    if mr_slug:
        entry["mr"] = mr_slug
    return entry


def class_ids():
    ids = []
    for sitemap in ("https://www.mcmod.cn/sitemap1.xml",
                    "https://www.mcmod.cn/sitemap2.xml"):
        try:
            r = requests.get(sitemap, headers={"User-Agent": UA}, timeout=20)
            r.raise_for_status()
            for mm in CLASS_RE.finditer(r.text):
                i = int(mm.group(1))
                if i not in ids:
                    ids.append(i)
        except Exception as e:
            print(f"[warn] sitemap 抓取失败 {sitemap}: {e}", file=sys.stderr)
    ids.sort()
    return ids


def load_resume():
    done = {}
    if os.path.exists(RESUME_PATH):
        with open(RESUME_PATH, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                try:
                    e = json.loads(line)
                    done[e["id"]] = e
                except Exception:
                    continue
    return done


def fetch_one(class_id: int):
    url = f"https://www.mcmod.cn/class/{class_id}.html"
    sess = _session()
    for attempt in range(3):
        try:
            r = sess.get(url, headers={"User-Agent": UA}, timeout=15)
            if r.status_code == 200:
                return parse_page(r.text, class_id)
            if r.status_code in (403, 429):
                time.sleep(0.5 * (attempt + 1))
        except Exception:
            time.sleep(0.3 * (attempt + 1))
    return None  # 失败不落盘，下次 --resume 自动重试


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--limit", type=int, default=0, help="只抓前 N 个（0=全部）")
    ap.add_argument("--workers", type=int, default=16)
    ap.add_argument("--out", default=OUT_PATH)
    ap.add_argument("--resume", action="store_true", default=True)
    ap.add_argument("--no-resume", action="store_true")
    args = ap.parse_args()

    ids = class_ids()
    if args.limit > 0:
        ids = ids[:args.limit]
    print(f"[collect] 模组 class 页共 {len(ids)} 个")

    done = {} if args.no_resume else load_resume()
    todo = [i for i in ids if i not in done]
    print(f"[resume] 已完成 {len(done)}，待抓 {len(todo)}")
    if not todo:
        _write_final(done, args.out)
        return

    t0 = time.time()
    n = 0
    with open(RESUME_PATH, "a", encoding="utf-8") as rf, \
         cf.ThreadPoolExecutor(max_workers=args.workers) as ex:
        futures = {ex.submit(fetch_one, i): i for i in todo}
        for fut in cf.as_completed(futures):
            class_id = futures[fut]
            try:
                entry = fut.result()
            except Exception as e:
                print(f"[fail] {class_id}: {e}", file=sys.stderr)
                continue
            if entry:
                done[class_id] = entry
                rf.write(json.dumps(entry, ensure_ascii=False) + "\n")
                rf.flush()
            n += 1
            if n % 500 == 0:
                el = time.time() - t0
                rate = n / max(el, 1e-6)
                print(f"[progress] {n}/{len(todo)}  {rate:.1f} it/s  "
                      f"剩余≈{(len(todo)-n)/max(rate,1e-6)/60:.1f}min", flush=True)

    # 汇总去重（按 id），保持稳定排序
    _write_final(done, args.out)
    el = time.time() - t0
    print(f"[done] 写入 {args.out}，共 {len(done)} 条，耗时 {el/60:.1f} min")


def _write_final(done, out):
    entries = [done[k] for k in sorted(done.keys())]
    with open(out, "w", encoding="utf-8") as f:
        json.dump(entries, f, ensure_ascii=False, separators=(",", ":"))
    print(f"[written] {out}  ->  {len(entries)} 条")


if __name__ == "__main__":
    main()
