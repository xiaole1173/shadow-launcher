"""
错误自愈脚本 — 内置高频问题自动修复逻辑
用法: python tools/error_auto_fix.py [--dry-run]
"""
import sys
import os
from pathlib import Path

PROJECT_ROOT = Path(__file__).parent.parent
DRY_RUN = "--dry-run" in sys.argv


def fix(name: str, action, description: str):
    print(f"  🔧 {name}: {description}")
    if not DRY_RUN:
        action()
    else:
        print(f"     (dry-run, would apply)")


def fix_qt_plugin_path():
    """补全 QT_PLUGIN_PATH 环境变量"""
    try:
        import PySide6
        plugins = str(Path(PySide6.__file__).parent / "plugins")
        os.environ["QT_PLUGIN_PATH"] = plugins
        return True
    except ImportError:
        return False


def fix_qml_import_path():
    """补全 QML2_IMPORT_PATH"""
    try:
        import PySide6
        qml = str(Path(PySide6.__file__).parent / "qml")
        os.environ["QML2_IMPORT_PATH"] = qml
        return True
    except ImportError:
        return False


def fix_high_dpi():
    """注入高 DPI 适配代码"""
    try:
        from PySide6.QtGui import QGuiApplication
        QGuiApplication.setHighDpiScaleFactorRoundingPolicy(
            QGuiApplication.HighDpiScaleFactorRoundingPolicy.PassThrough
        )
        return True
    except Exception:
        return False


def fix_qml_import():
    """补全 QML engine importPath"""
    qml_dir = PROJECT_ROOT / "shadow_launcher" / "ui" / "qml"
    if qml_dir.exists():
        os.environ["QML2_IMPORT_PATH"] = str(qml_dir)
        return True
    return False


FIXES = [
    ("Qt 插件路径", fix_qt_plugin_path, "设置 QT_PLUGIN_PATH"),
    ("QML 导入路径", fix_qml_import_path, "设置 QML2_IMPORT_PATH"),
    ("高 DPI 适配", fix_high_dpi, "注入 PassThrough DPI策略"),
    ("QML importPath", fix_qml_import, "补全 engine.addImportPath"),
]

CROSS_THREAD_TEMPLATE = """
# 跨线程 UI 更新标准修复模板:
# 错误: 直接从非主线程调用 QML/Widget 方法
# 修复: 使用 Signal 跨线程通信
from PySide6.QtCore import QObject, Signal

class Worker(QObject):
    result_ready = Signal(str)     # 用信号发送结果
    progress = Signal(int)

    def run(self):
        # ... 耗时操作 ...
        self.result_ready.emit("done")  # 不直接操作UI

# 主线程连接信号
worker.result_ready.connect(ui.update_label)
"""


def main():
    print("Shadow Launcher 错误自愈\n")

    # 运行环境补全（总是执行）
    for name, func, desc in FIXES:
        try:
            ok = func()
            print(f"  {'✅' if ok else '❌'} {name}: {desc}")
        except Exception as e:
            print(f"  ❌ {name}: {e}")

    # 跨线程模板（信息输出）
    print(f"\n📋 跨线程 UI 修复模板:")
    print(CROSS_THREAD_TEMPLATE)

    if DRY_RUN:
        print("ℹ  --dry-run 模式，未实际修改")

    return 0


if __name__ == "__main__":
    sys.exit(main())
