"""
Shadow Launcher QML 版本入口
Windows 中文环境 + QML热重载 + 环境变量加载
"""

import sys
import os

# ── 0. 前置：中文环境兼容 ──
if sys.platform == "win32":
    import subprocess
    try:
        subprocess.run(["chcp", "65001"], capture_output=True, shell=True)
    except Exception:
        pass

# ── 1. 环境变量加载（从 .env 文件） ──
def _load_env():
    env_file = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".env")
    if not os.path.exists(env_file):
        return
    with open(env_file, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            if "=" in line:
                key, _, value = line.partition("=")
                key, value = key.strip(), value.strip()
                if key and value and key not in os.environ:
                    os.environ[key] = value

_load_env()

# 确保项目根在 path
_project_root = os.path.dirname(os.path.abspath(__file__))
if _project_root not in sys.path:
    sys.path.insert(0, _project_root)

from PySide6.QtGui import QGuiApplication
from PySide6.QtWidgets import QApplication
from PySide6.QtQml import QQmlApplicationEngine
from PySide6.QtCore import QUrl, Qt, QtMsgType, qInstallMessageHandler

from shadow_launcher.ui.qml.backend import ShadowBackend


def _qt_message_handler(mode, context, message):
    """Filter harmless Qt warnings (dev-mode shows all)"""
    if os.environ.get("SHADOW_DEV_MODE") != "1":
        if any(kw in message for kw in [
            "Could not parse application stylesheet",
            "QFont::setPointSize", "libpng warning", "ShaderEffect",
        ]):
            return
    if mode in (QtMsgType.QtWarningMsg, QtMsgType.QtCriticalMsg, QtMsgType.QtFatalMsg):
        ctx = f"{context.file or '?'}:{context.line}" if context.file else "?"
        print(f"[Qt {mode.name}] {ctx} | {message}", file=sys.stderr, flush=True)


def main():
    qInstallMessageHandler(_qt_message_handler)

    # 高 DPI 适配
    QGuiApplication.setHighDpiScaleFactorRoundingPolicy(
        Qt.HighDpiScaleFactorRoundingPolicy.PassThrough
    )

    app = QApplication(sys.argv)
    app.setApplicationName("Shadow Launcher")
    app.setOrganizationName("Shadow")

    # 创建后端
    backend = ShadowBackend()

    # 启动时自动检测 Java
    backend.detectJava()

    # 设置 QML 引擎
    engine = QQmlApplicationEngine()
    qml_dir = os.path.join(os.path.dirname(__file__), "shadow_launcher", "ui", "qml")

    # 添加导入路径
    engine.addImportPath(qml_dir)

    # 注入后端到 QML 上下文
    engine.rootContext().setContextProperty("backend", backend)
    engine.rootContext().setContextProperty("_qmlDir", qml_dir)  # for hot-reload path

    # 加载主窗口
    main_qml = os.path.join(qml_dir, "MainWindow.qml")
    if not os.path.exists(main_qml):
        print(f"Error: QML file not found: {main_qml}", file=sys.stderr)
        sys.exit(1)

    engine.load(QUrl.fromLocalFile(main_qml))

    if not engine.rootObjects():
        print("Error: QML failed to load", file=sys.stderr)
        sys.exit(1)

    # ── 2. QML 热重载（开发模式） ──
    _dev_mode = os.environ.get("SHADOW_DEV_MODE") == "1"
    if _dev_mode:
        _setup_hot_reload(engine, qml_dir)

    sys.exit(app.exec())


def _setup_hot_reload(engine, qml_dir):
    """文件监听 QML 变化 → 清除缓存 + 重新加载"""
    import time
    from watchdog.observers import Observer
    from watchdog.events import FileSystemEventHandler

    class QmlReloadHandler(FileSystemEventHandler):
        def on_modified(self, event):
            if event.src_path.endswith(".qml"):
                print(f"[hot-reload] {os.path.basename(event.src_path)} changed, reloading...", flush=True)
                engine.clearComponentCache()
                main_qml = os.path.join(qml_dir, "MainWindow.qml")
                engine.load(QUrl.fromLocalFile(main_qml))

    try:
        observer = Observer()
        observer.schedule(QmlReloadHandler(), qml_dir, recursive=True)
        observer.daemon = True
        observer.start()
        print("[dev] QML hot-reload enabled (watchdog)", flush=True)
    except Exception as e:
        print(f"[dev] hot-reload unavailable: {e}", flush=True)


if __name__ == "__main__":
    main()
