"""
Shadow Launcher — 命令行版本安装工具 (v2: stdout pipeline)
用法:
  python install_cli.py <version>              # 安装指定版本（多源模式）
  python install_cli.py <version> --source 0   # BMCLAPI
  python install_cli.py <version> --source 1   # Mojang 官方
  python install_cli.py <version> --source 2   # MCBBS 镜像
  python install_cli.py --list                 # 列出可安装的版本
  python install_cli.py --status               # 查看安装进度
  python install_cli.py --cancel               # 取消当前安装

进度输出格式 (stdout):
  LOG:message              — 日志消息
  PROG:completed:total:downloaded_bytes:total_bytes:filename  — 进度更新
  DONE:success:elapsed     — 安装完成
  FAIL:message             — 安装失败
  CANCEL                   — 已取消
"""

import sys
import os
import time
import json
import threading
import signal

_project_root = os.path.dirname(os.path.abspath(__file__))
if _project_root not in sys.path:
    sys.path.insert(0, _project_root)

from shadow_launcher.core.versions import (
    fetch_version_manifest, fetch_version_json, get_installed_versions, MINECRAFT_DIR,
)
from shadow_launcher.core.downloader import MIRROR_SOURCES, VersionDownloader

STATE_FILE = os.path.join(_project_root, "shadow_launcher", "config", "install_state.json")


def _emit(line: str):
    """Write to stdout — this is the pipeline to the backend."""
    print(line, flush=True)


def _write_state(state: dict):
    """Write final state to JSON for process-external queries (--status)."""
    os.makedirs(os.path.dirname(STATE_FILE), exist_ok=True)
    with open(STATE_FILE, "w", encoding="utf-8") as f:
        json.dump(state, f, indent=2, ensure_ascii=False)


# ═══ 安装核心 ═══

_downloader = None
_cancelled = threading.Event()


def install_version(version_id: str, source_index: int = 0):
    global _downloader, _cancelled
    _cancelled.clear()

    _emit("LOG:正在加载版本清单...")
    manifest = fetch_version_manifest(use_mirror=True)
    _emit(f"LOG:版本清单已加载: {len(manifest.versions)} 个版本")

    version_info = None
    for v in manifest.versions:
        if v.id == version_id:
            version_info = v
            break
    if not version_info:
        _emit(f"FAIL:未找到版本: {version_id}")
        return False

    _emit(f"LOG:正在获取 {version_id} 元数据...")
    version_json = fetch_version_json(version_info)
    _emit(f"LOG:libraries: {len(version_json.get('libraries', []))} 个")

    mirror = MIRROR_SOURCES[source_index] if 0 <= source_index < len(MIRROR_SOURCES) else MIRROR_SOURCES[0]
    _emit(f"LOG:下载源: {mirror.name}")

    tick = 0

    def on_progress(cf: int, tf: int, db: int, tb: int, name: str):
        nonlocal tick
        tick += 1
        # Emit every file for real-time updates
        safe = name.encode('ascii', 'replace').decode()
        _emit(f"PROG:{cf}:{tf}:{db}:{tb}:{safe}")

    def on_log(msg: str):
        safe = msg.encode('ascii', 'replace').decode()
        _emit(f"LOG:{safe}")

    _downloader = VersionDownloader(
        mirror_source=mirror,
        minecraft_dir=MINECRAFT_DIR,
        max_workers=32,
    )
    _downloader.on_progress = on_progress
    _downloader.on_log = on_log

    _emit(f"LOG:开始下载 {version_id}（{mirror.name}）...")
    t0 = time.time()

    try:
        success = _downloader.download_version(version_json, version_id)
    except Exception as e:
        _emit(f"FAIL:{e}")
        return False

    elapsed = time.time() - t0

    if success:
        _emit(f"DONE:{success}:{elapsed:.1f}")
    elif _cancelled.is_set():
        _emit("CANCEL")
    else:
        _emit(f"FAIL:下载失败")

    # Write state file for --status queries
    _write_state({
        "version": version_id,
        "source": mirror.name,
        "status": "done" if success else ("cancelled" if _cancelled.is_set() else "failed"),
        "started": t0,
        "progress": tf_count if success else 0,
        "total": tf_count if success else 0,
        "elapsed": elapsed,
    })

    return success


# ═══ CLI 入口 ═══

def main():
    if "--list" in sys.argv:
        manifest = fetch_version_manifest(use_mirror=True)
        _emit(f"LOG:可用版本: {len(manifest.versions)}")
        for v in manifest.versions[:20]:
            _emit(f"LOG:  {v.id} ({v.type})")
        return

    if "--status" in sys.argv:
        if os.path.exists(STATE_FILE):
            with open(STATE_FILE, "r", encoding="utf-8") as f:
                state = json.load(f)
            _emit(json.dumps(state, indent=2, ensure_ascii=False))
        else:
            _emit("LOG:没有进行中的安装")
        return

    if "--cancel" in sys.argv:
        _cancelled.set()
        _emit("CANCEL")
        return

    if len(sys.argv) < 2:
        _emit("LOG:用法: python install_cli.py <version> [--source 0|1|2]")
        _emit("LOG:选项: --list --status --cancel")
        return

    version_id = sys.argv[1]
    source_index = 0
    if "--source" in sys.argv:
        idx = sys.argv.index("--source")
        if idx + 1 < len(sys.argv):
            source_index = int(sys.argv[idx + 1])

    install_version(version_id, source_index)


if __name__ == "__main__":
    main()
