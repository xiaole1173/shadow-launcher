"""
qrc 增量编译脚本
基于文件哈希校验，仅当关联资源变更时才执行 pyside6-rcc
用法: python tools/rcc_build.py
"""
import subprocess
import sys
import hashlib
import json
from pathlib import Path

PROJECT_ROOT = Path(__file__).parent.parent
CACHE_FILE = PROJECT_ROOT / ".rcc_cache.json"
QRC_FILE = PROJECT_ROOT / "resources.qrc"
OUTPUT_FILE = PROJECT_ROOT / "shadow_launcher" / "ui" / "qml" / "resources_rc.py"


def _hash_file(path: Path) -> str:
    if not path.exists():
        return ""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    if not QRC_FILE.exists():
        print("ℹ No resources.qrc found, skipping")
        return 0

    current_hash = _hash_file(QRC_FILE)

    # 读取缓存
    cached = {}
    if CACHE_FILE.exists():
        try:
            cached = json.loads(CACHE_FILE.read_text())
        except Exception:
            pass

    if cached.get("qrc_hash") == current_hash and OUTPUT_FILE.exists():
        print("✅ No changes, skipping rcc build")
        return 0

    print("🔄 Building resources...")
    result = subprocess.run(
        [sys.executable, "-m", "PySide6.pyside6_rcc", str(QRC_FILE), "-o", str(OUTPUT_FILE)],
        capture_output=True, text=True, timeout=60,
    )
    if result.returncode != 0:
        print(f"❌ rcc build failed:\n{result.stderr}")
        return 1

    # 更新缓存
    CACHE_FILE.write_text(json.dumps({"qrc_hash": current_hash}))
    print("✅ rcc build completed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
