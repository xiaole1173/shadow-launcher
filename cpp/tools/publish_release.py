#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2025-2026 Shadow
"""
Shadow Launcher 发布工具（2026-08-15，自建更新服务器版）

定位：pack.ps1 的配套工具 —— 打包由 pack.ps1 完成（windeployqt + 7-Zip +
compat.json 生成），本工具只负责把产物组织成自建服务器所需的 latest.json
（字段兼容 Gitee release，UpdateManager 直接消费），并输出上传清单。

子命令：
  all      — 先跑 pack.ps1（打 dist + zip + compat.json），再生成 latest.json
  manifest — 从已有产物（dist 目录）生成 latest.json（不重新打包）
  config   — 显示当前配置

用法：
  python publish_release.py all --notes "更新说明"
  python publish_release.py manifest --notes "更新说明"          # 已跑过 pack.ps1 时
  python publish_release.py manifest --notes-file notes.txt
  python publish_release.py config

配置：本目录 publish_config.json（git 忽略，由 publish_config.example.json 复制改值）：
  {
    "base_url":  "https://shadowlauncher.cn/downloads",   # 服务器上放置资产的目录
    "dist_dir":  "D:/latest-code/cpp/dist",                # pack.ps1 产物目录
    "output_dir": "D:/latest-code/publish"                 # latest.json 输出目录
  }

发布流程（三步）：
  1. MSBuild 构建 Release（SHADOW_DEV=1 时 pack.ps1 自动重构建）
  2. python publish_release.py all --notes "更新说明"
     → 产物: dist/ShadowLauncher_<版本>.zip（pack.ps1）
            dist/ShadowLauncher/compat.json（pack.ps1，full_sha256=zip 哈希）
            <output_dir>/latest.json（本工具）
  3. 上传 latest.json + compat.json + zip 到服务器 base_url 目录
     → 客户端（secrets_local.h 已填 SHADOW_UPDATE_API_URL）检查/下载全走自建服务器

服务器 nginx 参考：tools/update_server_nginx.example.conf
"""

import argparse
import glob
import hashlib
import json
import os
import re
import subprocess
import sys

TOOL_DIR = os.path.dirname(os.path.abspath(__file__))
CONFIG_PATH = os.path.join(TOOL_DIR, "publish_config.json")
PACK_SCRIPT = os.path.join(os.path.dirname(TOOL_DIR), "pack.ps1")   # cpp/pack.ps1


def load_config():
    cfg = {
        "base_url": "https://shadowlauncher.cn/downloads",
        "dist_dir": "D:/latest-code/cpp/dist",
        "output_dir": "D:/latest-code/publish",
    }
    if os.path.exists(CONFIG_PATH):
        with open(CONFIG_PATH, "r", encoding="utf-8") as f:
            cfg.update(json.load(f))
    return cfg


def sha256_of(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def find_latest_zip(dist_dir):
    """dist 目录下最新的 ShadowLauncher_*.zip。"""
    zips = [p for p in glob.glob(os.path.join(dist_dir, "ShadowLauncher_*.zip"))
            if os.path.isfile(p)]
    if not zips:
        return None
    return max(zips, key=os.path.getmtime)


def version_from_zip(zip_path):
    m = re.search(r"ShadowLauncher_(.+)\.zip$", os.path.basename(zip_path))
    return m.group(1) if m else None


def run_pack():
    if not os.path.exists(PACK_SCRIPT):
        print(f"[pack] 错误: 找不到 pack.ps1: {PACK_SCRIPT}")
        return 1
    print(f"[pack] 调用 pack.ps1 ...")
    r = subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass",
                        "-File", PACK_SCRIPT])
    if r.returncode != 0:
        print(f"[pack] pack.ps1 失败 exit={r.returncode}")
        return r.returncode
    print("[pack] pack.ps1 完成")
    return 0


def cmd_manifest(args, cfg):
    dist_dir = os.path.abspath(args.dist_dir or cfg["dist_dir"])
    out_dir = os.path.abspath(args.out or cfg["output_dir"])
    os.makedirs(out_dir, exist_ok=True)

    # 1. 定位 zip
    zip_path = os.path.abspath(args.zip) if args.zip else find_latest_zip(dist_dir)
    if not zip_path or not os.path.exists(zip_path):
        print(f"[manifest] 错误: 找不到 ShadowLauncher_*.zip（先跑 pack.ps1 或指定 --zip）: {dist_dir}")
        return 1
    version = args.version or version_from_zip(zip_path)
    if not version:
        print("[manifest] 错误: 无法从 zip 文件名推断版本，请指定 --version")
        return 1

    # 2. compat.json（pack.ps1 生成于 dist/ShadowLauncher/，full_sha256=zip 哈希）
    # ⚠ pack.ps1 用 PowerShell Out-File -Encoding utf8 写文件，带 UTF-8 BOM →
    #    必须 utf-8-sig 读取，否则 json.load 报 "Unexpected UTF-8 BOM"
    compat_path = args.compat or os.path.join(dist_dir, "ShadowLauncher", "compat.json")
    if not os.path.exists(compat_path):
        print(f"[manifest] 错误: 找不到 compat.json: {compat_path}")
        return 1
    with open(compat_path, "r", encoding="utf-8-sig") as f:
        compat = json.load(f)

    # 3. 防呆校验：compat.full_sha256 必须等于 zip 实际哈希
    actual_sha = sha256_of(zip_path)
    declared_sha = (compat.get("full_sha256") or "").lower()
    if declared_sha and declared_sha != actual_sha:
        print("[manifest] 错误: compat.json full_sha256 与 zip 实际哈希不一致！")
        print(f"  declared: {declared_sha}")
        print(f"  actual  : {actual_sha}")
        print("  说明 zip 与 compat.json 不是同一次 pack 的产物，拒绝生成")
        return 1
    if not declared_sha:
        print("[manifest] 警告: compat.json 缺 full_sha256（可能未压缩后更新），照常生成")

    # 4. notes（latest.json body，更新公告用）
    notes = args.notes
    if args.notes_file:
        with open(args.notes_file, "r", encoding="utf-8") as f:
            notes = f.read().strip()

    base_url = (args.base_url or cfg["base_url"]).rstrip("/")

    # 5. latest.json（字段兼容 Gitee release；force_full 模式资产 = compat + zip）
    # ── 2026-08-15：URL 加 ?v=<sha前12位> 版本戳 ──
    # CF 橙色代理默认缓存二进制（zip）4h，覆盖上传后客户端仍拿到旧文件（实测
    # SHA256 校验失败：期望新哈希、实际旧哈希）。哈希戳使每次发版 URL 变化 →
    # CF 缓存键变化 → 强制回源新文件，彻底绕开缓存。
    zip_name = os.path.basename(zip_path)
    zip_size = os.path.getsize(zip_path)
    ver_stamp = actual_sha[:12]
    latest = {
        "tag_name": version,
        "body": notes or "",
        "assets": [
            {"name": "compat.json",
             "browser_download_url": f"{base_url}/compat.json?v={ver_stamp}"},
            {"name": zip_name,
             "browser_download_url": f"{base_url}/{zip_name}?v={ver_stamp}",
             "size": zip_size},
        ],
    }
    latest_path = os.path.join(out_dir, "latest.json")
    with open(latest_path, "w", encoding="utf-8") as f:
        json.dump(latest, f, ensure_ascii=False, indent=2)

    # 6. 把 compat.json 拷到输出目录（去 BOM、utf-8 输出——Qt QJsonDocument
    #    fromJson 对前导 BOM 会解析失败，上传给客户端的 compat.json 必须无 BOM）
    out_compat = os.path.join(out_dir, "compat.json")
    with open(out_compat, "w", encoding="utf-8") as f:
        json.dump(compat, f, ensure_ascii=False, indent=2)

    print(f"[manifest] 版本        : {version}")
    print(f"[manifest] zip         : {zip_path} ({zip_size/1048576:.1f}MB)")
    print(f"[manifest] mode        : {compat.get('update_mode')}  (自建服务器建议保持 force_full)")
    print(f"[manifest] full_sha256 : {actual_sha}")
    print(f"[manifest] 已生成      : {latest_path}")
    print(f"[manifest] 已生成      : {out_compat}")
    print(f"[manifest] 上传清单（到服务器 {base_url}/）:")
    print(f"            latest.json")
    print(f"            compat.json")
    print(f"            {zip_name}")
    print(f"[manifest] 客户端 API  : {base_url}/latest.json")
    return 0


def cmd_all(args, cfg):
    r = run_pack()
    if r != 0:
        return r
    return cmd_manifest(args, cfg)


def cmd_config(_args, cfg):
    print(json.dumps(cfg, ensure_ascii=False, indent=2))
    print(f"\n配置文件: {CONFIG_PATH}")
    return 0


def main():
    p = argparse.ArgumentParser(description="Shadow Launcher 发布工具（pack.ps1 配套）")
    sub = p.add_subparsers(dest="cmd", required=True)

    for name in ("all", "manifest"):
        sp = sub.add_parser(name)
        sp.add_argument("--version", help="版本号（默认从 zip 文件名解析）")
        sp.add_argument("--notes", default="", help="更新说明（latest.json body）")
        sp.add_argument("--notes-file", help="更新说明（从文件读）")
        sp.add_argument("--zip", help="zip 路径（默认取 dist 目录最新）")
        sp.add_argument("--compat", help="compat.json 路径（默认 dist/ShadowLauncher/compat.json）")
        sp.add_argument("--dist-dir", help="dist 目录（默认取配置）")
        sp.add_argument("--out", help="输出目录（默认取配置 output_dir）")
        sp.add_argument("--base-url", help="服务器 URL 前缀（默认取配置）")

    sub.add_parser("config")

    args = p.parse_args()
    cfg = load_config()
    if args.cmd == "manifest":
        return cmd_manifest(args, cfg)
    if args.cmd == "all":
        return cmd_all(args, cfg)
    if args.cmd == "config":
        return cmd_config(args, cfg)
    return 1


if __name__ == "__main__":
    sys.exit(main())
