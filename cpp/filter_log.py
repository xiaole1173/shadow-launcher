#!/usr/bin/env python3
"""Filter Shadow Launcher log for AssetDownloader diagnostics + key events."""
import re
import sys
from pathlib import Path

LOG = Path(r"D:\latest-code\cpp\build\Release\logs\shadow_launcher_2026-07-29.log")

INCLUDE_CATEGORIES = {
    "ShadowDownloader.Asset",
    "Download",      # FileDownloader phase/start messages
    "Version",       # step status changes (not progress spam)
}

# Lines we want even if from other categories
FORCE_LINES = re.compile(
    r"\[(Asset|state|speed|dns|pre-check|floor|accel|source|limit|all done|cancelled|all hosts|"
    r"startDownload|v2\+|Phase:|引擎|下载完成|开始安装|下载失败|已完成|安装状态)"
)

# Lines to always EXCLUDE (step progress spam)
EXCLUDE_PATTERNS = [
    re.compile(r"步骤.*idx=\d+.*状态=active.*进度=\d+%"),
    re.compile(r"下载步骤 cat=\d+ 文件="),
    re.compile(r"添加下载任务 名称="),
    re.compile(r"任务已排队.*队列总数="),
    re.compile(r"步骤.*进度=0%"),
    re.compile(r"开始下载 URL="),
]

def should_include(line):
    # Extract category
    m = re.search(r'\[(\S+)\]', line)
    if not m:
        return False
    cat = m.group(1)
    
    # Always exclude spam lines
    for p in EXCLUDE_PATTERNS:
        if p.search(line):
            return False
    
    # Include our category
    if cat == "ShadowDownloader.Asset":
        return True
    
    # Include forced lines
    if FORCE_LINES.search(line):
        return True
    
    # Include Download phase messages (not per-file ones)
    if cat == "Download":
        if any(kw in line for kw in ["Phase:", "引擎", "添加下载任务", "线程完成", "合并", 
                                       "下载完成", "SHA1"]):
            return True
        return False
    
    # Include Version for key events only
    if cat == "Version":
        if any(kw in line for kw in ["安装状态变更", "开始安装", "完成", "失败", "双源竞速",
                                       "版本JSON获取成功", "下载引擎 v9"]):
            return True
        return False
    
    return False

def main():
    lines = LOG.read_text(encoding="utf-8").splitlines()
    
    # Collect per-second speed samples for summary
    speed_samples = []
    
    for line in lines:
        if not should_include(line):
            continue
        
        # Strip the HTML-like prefix from fmtSize
        cleaned = line.replace('"', '')
        print(cleaned)
        
        # Collect speed from state lines
        if "speed=" in line:
            m = re.search(r'speed=\s*"?(\d+\.?\d*)\s*MB/s', line)
            if m:
                speed_samples.append(float(m.group(1)))
    
    # Summary
    if speed_samples:
        peak = max(speed_samples)
        avg = sum(speed_samples) / len(speed_samples)
        print(f"\n{'='*60}")
        print(f"Speed summary: peak={peak:.1f} MB/s, avg={avg:.1f} MB/s, samples={len(speed_samples)}")

if __name__ == "__main__":
    main()
