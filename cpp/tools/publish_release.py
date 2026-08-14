#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2025-2026 Shadow
"""
Shadow Launcher 发布工具（2026-08-15，自建更新服务器版）

功能：
  package  — 把 Release 目录打包成全量更新 zip（自动排除 .minecraft/logs 等）
  manifest — 生成 latest.json + compat.json（自动计算 exe/full 的 SHA256）
  all      — package + manifest 一键完成

用法：
  python publish_release.py all --version v0.2.3 --notes "更新说明"
  python publish_release.py all --version v0.2.3 --notes-file notes.txt --force-full "Qt 升级"
  python publish_release.py package --release-dir build/Release --out out
  python publish_release.py manifest --version v0.2.3 --exe build/Release/ShadowLauncher.exe --zip out/ShadowLauncher.zip
  python publish_release.py config

配置：本目录 publish_config.json（git 忽略，由 publish_config.example.json 复制改值）：
  {
    "base_url": "https://update.example.com/update",
    "qt_version": "6.8.3",
    "resource_epoch": 1,
    "release_dir": "D:/latest-code/cpp/build/Release",
    "output_dir": "D:/latest-code/publish"
  }

发布流程：
  1. MSBuild 构建 Release
  2. python publish_release.py all --version vX.Y.Z --notes "..."
  3. 把 output_dir 下所有文件上传到服务器 /update/ 目录（与 base_url 对应）
  4. 客户端 next 启动/手动检查即可发现新版本（检查接口 + exe/zip 下载均走
     base_url，nginx 默认支持 Range 断点续传）

服务器 nginx 参考：tools/update_server_nginx.example.conf
"""

import argparse
import hashlib
import json
import os
import shutil
import sys
import zipfile

TOOL_DIR = os.path.dirname(os.path.abspath(__file__))
CONFIG_PATH = os.path.join(TOOL_DIR, "publish_config.json")

# 全量 zip 打包排除清单（与 SLUpdater 全量安装保留项一致）
EXCLUDE_NAMES = {
    ".minecraft", "logs", "java_cache", "_update",
    "agreement_consent.txt", "ShadowLauncher.exe.old",
}
EXCLUDE_SUFFIXES = (".pdb", ".old")


def load_config():
    cfg = {
        "base_url": "https://update.example.com/update",
        "qt_version": "6.8.3",
        "resource_epoch": 1,
        "release_dir": "build/Release",
        "output_dir": "publish",
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


def collect_files(root):
    """返回 (相对路径列表, 总字节)，排除清单与全量更新保留项一致。"""
    files, total = [], 0
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in EXCLUDE_NAMES]
        for fn in filenames:
            if fn in EXCLUDE_NAMES or fn.endswith(EXCLUDE_SUFFIXES):
                continue
            # 构建产物里的测试 exe 无需打进全量包
            if fn.endswith("Test.exe") or fn.endswith("Test.pdb"):
                continue
            rel = os.path.relpath(os.path.join(dirpath, fn), root)
            files.append(rel)
            total += os.path.getsize(os.path.join(dirpath, fn))
    files.sort()
    return files, total


def cmd_package(args, cfg):
    release_dir = os.path.abspath(args.release_dir or cfg["release_dir"])
    out_dir = os.path.abspath(args.out or cfg["output_dir"])
    os.makedirs(out_dir, exist_ok=True)

    exe = os.path.join(release_dir, "ShadowLauncher.exe")
    if not os.path.exists(exe):
        print(f"[package] 错误: Release 目录缺少 ShadowLauncher.exe: {exe}")
        return 1

    files, total = collect_files(release_dir)
    if not files:
        print("[package] 错误: 没有可打包的文件")
        return 1

    zip_path = os.path.join(out_dir, "ShadowLauncher.zip")
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
        for rel in files:
            zf.write(os.path.join(release_dir, rel), rel)

    print(f"[package] 全量包已生成: {zip_path}")
    print(f"[package] 文件数={len(files)} 原始大小={total/1048576:.1f}MB "
          f"压缩后={os.path.getsize(zip_path)/1048576:.1f}MB")
    return 0


def cmd_manifest(args, cfg):
    if not args.version:
        print("[manifest] 错误: 必须指定 --version（如 v0.2.3）")
        return 1

    out_dir = os.path.abspath(args.out or cfg["output_dir"])
    os.makedirs(out_dir, exist_ok=True)

    # 定位 exe / zip
    exe_path = os.path.abspath(args.exe) if args.exe else os.path.join(
        os.path.abspath(args.release_dir or cfg["release_dir"]), "ShadowLauncher.exe")
    zip_path = os.path.abspath(args.zip) if args.zip else os.path.join(out_dir, "ShadowLauncher.zip")
    if not os.path.exists(exe_path):
        print(f"[manifest] 错误: 找不到 exe: {exe_path}")
        return 1
    if not os.path.exists(zip_path):
        print(f"[manifest] 错误: 找不到全量 zip: {zip_path}（先跑 package 或指定 --zip）")
        return 1
    # zip 必须包含主程序（否则全量安装后无 exe 可运行）
    with zipfile.ZipFile(zip_path) as zf:
        if "ShadowLauncher.exe" not in zf.namelist():
            print(f"[manifest] 错误: zip 内缺少 ShadowLauncher.exe: {zip_path}")
            return 1

    # SHA256（小写，与启动器 toLower 比较一致）
    exe_sha = sha256_of(exe_path)
    full_sha = sha256_of(zip_path)

    notes = args.notes
    if args.notes_file:
        with open(args.notes_file, "r", encoding="utf-8") as f:
            notes = f.read().strip()

    base_url = (args.base_url or cfg["base_url"]).rstrip("/")
    qt_version = args.qt or cfg["qt_version"]
    epoch = args.epoch if args.epoch is not None else cfg["resource_epoch"]

    # ── compat.json ──
    compat = {
        "qt_version": qt_version,
        "resource_epoch": int(epoch),
        "update_mode": "force_full" if args.force_full else "auto",
        "exe_sha256": exe_sha,
        "full_sha256": full_sha,
    }
    if args.force_full:
        compat["force_reason"] = args.force_full
    compat_path = os.path.join(out_dir, "compat.json")
    with open(compat_path, "w", encoding="utf-8") as f:
        json.dump(compat, f, ensure_ascii=False, indent=2)

    # ── latest.json（字段兼容 Gitee release，UpdateManager 直接消费）──
    exe_size = os.path.getsize(exe_path)
    zip_size = os.path.getsize(zip_path)
    latest = {
        "tag_name": args.version,
        "body": notes or "",
        "assets": [
            {"name": "compat.json",
             "browser_download_url": f"{base_url}/compat.json"},
            {"name": "ShadowLauncher.exe",
             "browser_download_url": f"{base_url}/ShadowLauncher.exe",
             "size": exe_size},
            {"name": "ShadowLauncher.zip",
             "browser_download_url": f"{base_url}/ShadowLauncher.zip",
             "size": zip_size},
        ],
    }
    latest_path = os.path.join(out_dir, "latest.json")
    with open(latest_path, "w", encoding="utf-8") as f:
        json.dump(latest, f, ensure_ascii=False, indent=2)

    print(f"[manifest] 版本        : {args.version}")
    print(f"[manifest] qt_version  : {qt_version}  resource_epoch={epoch}  mode={compat['update_mode']}")
    print(f"[manifest] exe  SHA256 : {exe_sha}")
    print(f"[manifest] zip  SHA256 : {full_sha}")
    print(f"[manifest] 已生成: {compat_path}")
    print(f"[manifest] 已生成: {latest_path}")
    print("[manifest] 上传清单（到服务器 /update/，对应 base_url）:")
    print(f"            {os.path.basename(latest_path)}")
    print(f"            {os.path.basename(compat_path)}")
    print(f"            ShadowLauncher.exe  ({exe_size/1048576:.1f}MB)")
    print(f"            ShadowLauncher.zip  ({zip_size/1048576:.1f}MB)")
    return 0


def cmd_all(args, cfg):
    r1 = cmd_package(args, cfg)
    if r1 != 0:
        return r1
    return cmd_manifest(args, cfg)


def cmd_config(_args, cfg):
    print(json.dumps(cfg, ensure_ascii=False, indent=2))
    print(f"\n配置文件: {CONFIG_PATH}")
    return 0


def main():
    p = argparse.ArgumentParser(description="Shadow Launcher 发布工具")
    sub = p.add_subparsers(dest="cmd", required=True)

    for name in ("package", "manifest", "all"):
        sp = sub.add_parser(name)
        sp.add_argument("--version", help="新版本号，如 v0.2.3（manifest/all 必填）")
        sp.add_argument("--notes", default="", help="更新说明（直接传入）")
        sp.add_argument("--notes-file", help="更新说明（从文件读）")
        sp.add_argument("--exe", help="增量 exe 路径（默认 release_dir/ShadowLauncher.exe）")
        sp.add_argument("--zip", help="全量 zip 路径（默认 output_dir/ShadowLauncher.zip）")
        sp.add_argument("--release-dir", help="Release 构建目录（默认取配置）")
        sp.add_argument("--out", help="输出目录（默认取配置 output_dir）")
        sp.add_argument("--base-url", help="服务器 URL 前缀（默认取配置）")
        sp.add_argument("--qt", help="Qt 版本（默认取配置；与构建的 SHADOW_QT_VERSION 一致）")
        sp.add_argument("--epoch", type=int, help="资源 epoch（默认取配置；与 SHADOW_RESOURCE_EPOCH 一致）")
        sp.add_argument("--force-full", nargs="?", const="手动指定",
                        help="强制全量更新模式，可选原因文本")

    sub.add_parser("config")

    args = p.parse_args()
    cfg = load_config()
    if args.cmd == "package":
        return cmd_package(args, cfg)
    if args.cmd == "manifest":
        return cmd_manifest(args, cfg)
    if args.cmd == "all":
        return cmd_all(args, cfg)
    if args.cmd == "config":
        return cmd_config(args, cfg)
    return 1


if __name__ == "__main__":
    sys.exit(main())
