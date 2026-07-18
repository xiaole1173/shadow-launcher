"""
前置校验脚本 — 运行/编译/打包前自动执行
检查 Python版本、PySide6版本、Qt插件路径、QML文件、中文路径
用法: python tools/env_check.py [--strict]
"""
import sys
import os
import re
from pathlib import Path

PROJECT_ROOT = Path(__file__).parent.parent


def _check(name: str, ok: bool, detail: str) -> bool:
    mark = "[OK]" if ok else "[FAIL]"
    print(f"  {mark} {name}: {detail}")
    return ok


def main():
    print("Shadow Launcher 环境检查\n")
    all_ok = True
    strict = "--strict" in sys.argv

    # 1. Python 版本
    py_ver = f"{sys.version_info.major}.{sys.version_info.minor}"
    py_ok = sys.version_info >= (3, 10)
    all_ok &= _check("Python 版本", py_ok, f"{py_ver} (需要 ≥3.10)")

    # 2. PySide6 版本
    try:
        import PySide6
        from PySide6.QtCore import __version__ as qt_ver
        pyside_ok = True
        all_ok &= _check("PySide6", pyside_ok, f"{PySide6.__version__} / Qt {qt_ver}")
    except ImportError:
        all_ok &= _check("PySide6", False, "未安装")

    # 3. Qt 插件路径
    try:
        import PySide6
        plugins = Path(PySide6.__file__).parent / "plugins"
        platforms = plugins / "platforms"
        has_qwindows = (platforms / "qwindows.dll").exists()
        all_ok &= _check("Qt 插件路径", has_qwindows, str(platforms) if has_qwindows else "缺 qwindows.dll")
    except Exception:
        all_ok &= _check("Qt 插件路径", False, "无法检测")

    # 4. QML 导入路径
    try:
        qml_dir = PROJECT_ROOT / "shadow_launcher" / "ui" / "qml"
        main_qml = qml_dir / "MainWindow.qml"
        qml_ok = main_qml.exists()
        all_ok &= _check("主 QML 文件", qml_ok, str(main_qml) if qml_ok else "不存在")
    except Exception as e:
        all_ok &= _check("QML 文件", False, str(e))

    # 5. 中文路径检测
    path_str = str(PROJECT_ROOT)
    has_cn = bool(re.search(r'[\u4e00-\u9fff]', path_str))
    if has_cn:
        all_ok &= _check("项目路径", not strict, f"含中文字符: {path_str} (Nuitka不兼容)")
    else:
        all_ok &= _check("项目路径", True, "正常")

    # 6. 环境变量
    env_ok = bool(os.environ.get("QT_PLUGIN_PATH") or os.environ.get("QML2_IMPORT_PATH"))
    all_ok &= _check(".env 加载", env_ok, "已加载" if env_ok else "未加载 (运行前请加载 .env)")

    # 7. Java (optional)
    java_home = os.environ.get("JAVA_HOME")
    if java_home:
        all_ok &= _check("JAVA_HOME", True, java_home)

    print(f"\n{'[OK] All checks passed' if all_ok else '[FAIL] Some checks failed'}")
    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())
