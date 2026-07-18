"""
持久化日志系统 — 记录所有后端操作到文件

特性：
- 自动轮转（单文件最大 5MB，保留最近 5 个备份）
- 时间戳精确到毫秒
- 分级：DEBUG / INFO / WARN / ERROR / FATAL
- 崩溃时自动写入崩溃报告
- 线程安全
"""

import os
import sys
import time
import threading
import traceback
from datetime import datetime

LOG_DIR = os.path.join(os.path.expanduser("~"), ".shadow", "logs")
LOG_FILE = os.path.join(LOG_DIR, "launcher.log")
MAX_SIZE = 5 * 1024 * 1024  # 5MB
MAX_BACKUPS = 5

_lock = threading.Lock()
_start_time = datetime.now()


def _ensure_dir():
    os.makedirs(LOG_DIR, exist_ok=True)


def _rotate_if_needed():
    """如果日志文件超过上限，轮转备份"""
    try:
        if os.path.exists(LOG_FILE) and os.path.getsize(LOG_FILE) >= MAX_SIZE:
            # 轮转旧备份
            for i in range(MAX_BACKUPS - 1, 0, -1):
                src = f"{LOG_FILE}.{i}"
                dst = f"{LOG_FILE}.{i + 1}"
                if os.path.exists(src):
                    if os.path.exists(dst):
                        os.remove(dst)
                    os.rename(src, dst)
            # 轮转当前日志
            bak = f"{LOG_FILE}.1"
            if os.path.exists(bak):
                os.remove(bak)
            os.rename(LOG_FILE, bak)
    except Exception:
        pass  # 轮转失败不阻塞日志写入


def _write_log(level: str, message: str):
    """线程安全写入日志"""
    _ensure_dir()
    _rotate_if_needed()
    ts = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
    line = f"[{ts}] [{level:5s}] {message}\n"
    try:
        with _lock:
            with open(LOG_FILE, "a", encoding="utf-8") as f:
                f.write(line)
                f.flush()
    except Exception:
        pass  # 写日志失败不能影响主流程


def log_info(msg: str):
    """普通信息"""
    _write_log("INFO", msg)


def log_warn(msg: str):
    """警告"""
    _write_log("WARN", msg)


def log_error(msg: str):
    """错误"""
    _write_log("ERROR", msg)


def log_debug(msg: str):
    """调试信息（仅开发模式）"""
    if os.environ.get("SHADOW_DEV", "").lower() in ("1", "true", "yes"):
        _write_log("DEBUG", msg)


def log_startup():
    """记录启动事件"""
    _ensure_dir()
    ts = _start_time.strftime("%Y-%m-%d %H:%M:%S")
    header = f"{'='*60}\n"
    header += f"  Shadow Launcher 启动 — {ts}\n"
    header += f"  Python: {sys.version}\n"
    header += f"  Platform: {sys.platform} | {os.name}\n"
    header += f"  Executable: {sys.executable}\n"
    header += f"  PID: {os.getpid()}\n"
    header += f"  Log File: {LOG_FILE}\n"
    header += f"{'='*60}"
    _write_log("INFO", header)


def log_shutdown(exit_code: int = 0, reason: str = ""):
    """记录关闭事件"""
    duration = datetime.now() - _start_time
    msg = f"启动器关闭 (exit_code={exit_code}, 运行时间={duration})"
    if reason:
        msg += f" — {reason}"
    _write_log("INFO", msg)


def log_crash(exc_info=None):
    """记录未捕获的崩溃"""
    if exc_info is None:
        exc_info = sys.exc_info()
    if exc_info and exc_info[0]:
        tb = "".join(traceback.format_exception(*exc_info))
        _write_log("FATAL", f"未捕获异常:\n{tb}")
    else:
        _write_log("FATAL", "未知原因崩溃")


def log_user_action(action: str, detail: str = ""):
    """记录用户关键操作"""
    msg = f"[用户操作] {action}"
    if detail:
        msg += f" | {detail}"
    _write_log("INFO", msg)


def get_log_path() -> str:
    """返回当前日志文件路径"""
    _ensure_dir()
    return LOG_FILE


def get_recent_logs(lines: int = 50) -> str:
    """读取最近 N 行日志（供 UI 显示）"""
    _ensure_dir()
    if not os.path.exists(LOG_FILE):
        return ""
    try:
        with open(LOG_FILE, "r", encoding="utf-8", errors="replace") as f:
            all_lines = f.readlines()
        return "".join(all_lines[-lines:])
    except Exception:
        return ""


def install_crash_handler():
    """安装全局崩溃捕获器"""
    _original_excepthook = sys.excepthook

    def _handler(exc_type, exc_value, exc_tb):
        log_crash((exc_type, exc_value, exc_tb))
        if _original_excepthook:
            _original_excepthook(exc_type, exc_value, exc_tb)

    sys.excepthook = _handler

    # 也捕获 threading 异常
    _original_thread_excepthook = threading.excepthook

    def _thread_handler(args):
        log_crash((args.exc_type, args.exc_value, args.exc_traceback))
        if _original_thread_excepthook:
            _original_thread_excepthook(args)

    threading.excepthook = _thread_handler


def install_qt_handler():
    """安装 Qt 消息拦截器 — 捕获所有 QML 警告/布局错误/渲染问题"""
    try:
        from PySide6.QtCore import qInstallMessageHandler, QtMsgType

        def _qt_handler(msg_type, context, message):
            type_map = {
                QtMsgType.QtDebugMsg: "QT_DBG",
                QtMsgType.QtInfoMsg: "QT_INFO",
                QtMsgType.QtWarningMsg: "QT_WARN",
                QtMsgType.QtCriticalMsg: "QT_CRIT",
                QtMsgType.QtFatalMsg: "QT_FATAL",
            }
            level = type_map.get(msg_type, "QT_MSG")
            # 提取文件/行号信息
            loc = ""
            if context.file:
                fname = context.file.split("/")[-1].split("\\")[-1]
                loc = f"({fname}:{context.line})"
            _write_log(level, f"{loc} {message}")

        qInstallMessageHandler(_qt_handler)
        _write_log("INFO", "Qt 消息拦截器已安装 — UI层警告/错误将被记录")
    except ImportError:
        pass  # 无 PySide6 环境（如纯CLI模式）
