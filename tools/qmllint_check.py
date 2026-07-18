"""
QML 语法校验脚本
调用 Qt 自带的 qmllint 检查所有 .qml 文件
用法: python tools/qmllint_check.py
"""
import subprocess
import sys
import os
from pathlib import Path

PROJECT_ROOT = Path(__file__).parent.parent
QML_DIR = PROJECT_ROOT / "shadow_launcher" / "ui" / "qml"


def find_qmllint():
    """查找 qmllint 工具"""
    # PySide6 自带
    import PySide6
    pyside_dir = Path(PySide6.__file__).parent
    qmllint = pyside_dir / "qmllint.exe"
    if qmllint.exists():
        return str(qmllint)

    # PATH 中查找
    result = subprocess.run(["where", "qmllint"], capture_output=True, text=True, shell=True)
    if result.returncode == 0:
        return result.stdout.strip().split("\n")[0]
    return None


def main():
    qmllint = find_qmllint()
    if not qmllint:
        print("⚠ qmllint not found, skipping QML syntax check")
        return 0

    qml_files = list(QML_DIR.glob("*.qml"))
    if not qml_files:
        print("⚠ No QML files found")
        return 0

    print(f"Checking {len(qml_files)} QML files...")
    errors = 0

    for qml_file in sorted(qml_files):
        result = subprocess.run(
            [qmllint, str(qml_file)],
            capture_output=True, text=True,
            timeout=30,
        )
        if result.returncode != 0:
            errors += 1
            # 只输出错误行
            for line in result.stderr.splitlines():
                if "error" in line.lower() or "warning" in line.lower():
                    print(f"  {qml_file.name}: {line.strip()}")

    if errors:
        print(f"\n❌ {errors} file(s) with issues")
        return 1
    else:
        print(f"✅ All {len(qml_files)} QML files passed")
        return 0


if __name__ == "__main__":
    sys.exit(main())
