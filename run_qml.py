"""
Shadow Launcher QML 版本入口
"""

import sys
import os

# 确保项目根在 path
_project_root = os.path.dirname(os.path.abspath(__file__))
if _project_root not in sys.path:
    sys.path.insert(0, _project_root)

from PySide6.QtGui import QGuiApplication
from PySide6.QtWidgets import QApplication
from PySide6.QtQml import QQmlApplicationEngine
from PySide6.QtCore import QUrl, QtMsgType, qInstallMessageHandler

from shadow_launcher.ui.qml.backend import ShadowBackend


def _qt_message_handler(mode, context, message):
    """Filter harmless Qt warnings"""
    if "Could not parse application stylesheet" in message:
        return
    if "QFont::setPointSize" in message:
        return
    if "libpng warning" in message:
        return
    # Suppress QML warnings we can't fix (Qt internals)
    if "ShaderEffect" in message:
        return
    if mode in (QtMsgType.QtWarningMsg, QtMsgType.QtCriticalMsg, QtMsgType.QtFatalMsg):
        ctx = f"{context.file or '?'}:{context.line}" if context.file else "?"
        print(f"[Qt {mode.name}] {ctx} | {message}", file=sys.stderr, flush=True)


def main():
    qInstallMessageHandler(_qt_message_handler)

    from PySide6.QtWidgets import QApplication
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

    # 加载主窗口
    main_qml = os.path.join(qml_dir, "MainWindow.qml")
    if not os.path.exists(main_qml):
        print(f"Error: QML file not found: {main_qml}", file=sys.stderr)
        sys.exit(1)

    engine.load(QUrl.fromLocalFile(main_qml))

    if not engine.rootObjects():
        print("Error: QML failed to load", file=sys.stderr)
        sys.exit(1)

    sys.exit(app.exec())


if __name__ == "__main__":
    main()
