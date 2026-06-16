"""
启动前校验模块 — P0 五项强校验
供 LaunchMixin 调用
"""

import os
import sys
import struct
import subprocess
import json
import time
import threading

from shadow_launcher.core.launcher import MINECRAFT_DIR, get_java_path, get_system_memory


class PreLaunchChecker:
    """启动前校验结果容器"""

    def __init__(self):
        self.errors = []    # 强校验失败，阻止启动
        self.warnings = []  # 弱校验失败，可强制启动
        self.all_pass = True

    def error(self, msg: str):
        self.errors.append(msg)
        self.all_pass = False

    def warn(self, msg: str):
        self.warnings.append(msg)

    def to_dict(self) -> dict:
        return {
            "passed": len(self.errors) == 0,
            "errors": self.errors,
            "warnings": self.warnings,
        }


def check_java_executable() -> str | None:
    """
    P0-1: 校验 java.exe 是否存在并可执行
    返回: java_path 或 None（失败）
    """
    java_path = get_java_path()
    if not os.path.isfile(java_path):
        return None
    # 检查是否真的能执行
    try:
        result = subprocess.run(
            [java_path, "-version"],
            capture_output=True, text=True,
            timeout=10,
            creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == "win32" else 0,
        )
        if result.returncode != 0:
            return None
    except Exception:
        return None
    return java_path


def check_java_arch(java_path: str) -> tuple[bool, str]:
    """
    P0：检测 Java 架构（32/64位）
    返回: (is_64bit, arch_str)
    """
    try:
        result = subprocess.run(
            [java_path, "-XshowSettings:all", "-version"],
            capture_output=True, text=True,
            timeout=15,
            creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == "win32" else 0,
        )
        output = result.stderr + result.stdout
        if "64-Bit" in output:
            return True, "64-bit"
        elif "32-Bit" in output:
            return False, "32-bit"
        else:
            # 回退：检查 PE 头
            with open(java_path, "rb") as f:
                f.seek(60)
                pe_offset = struct.unpack("<I", f.read(4))[0]
                f.seek(pe_offset + 4)
                machine = struct.unpack("<H", f.read(2))[0]
                if machine == 0x8664:
                    return True, "64-bit"
                elif machine == 0x014C:
                    return False, "32-bit"
    except Exception:
        pass
    return True, "unknown"  # 默认假设64位


def check_version_jar(version_id: str) -> str | None:
    """
    P0-2: 校验核心 Jar 文件存在
    返回: jar_path 或 None
    """
    jar_path = os.path.join(MINECRAFT_DIR, "versions", version_id, f"{version_id}.jar")
    if os.path.isfile(jar_path) and os.path.getsize(jar_path) > 0:
        return jar_path
    return None


def check_version_json_valid(version_id: str) -> str | None:
    """
    P0-2: 校验 version.json 存在且格式正确
    返回: json_path 或 None
    """
    json_path = os.path.join(MINECRAFT_DIR, "versions", version_id, f"{version_id}.json")
    if not os.path.isfile(json_path):
        return None
    try:
        with open(json_path, "r", encoding="utf-8") as f:
            data = json.load(f)
        if not isinstance(data, dict) or "id" not in data:
            return None
    except Exception:
        return None
    return json_path


def check_natives_dir(version_id: str) -> bool:
    """
    P0-3: 校验 natives 库目录是否有文件（至少.dll）
    注意：natives 在启动时才解压，这里只检查是否曾经解压过
    实际完整性在 extract_natives 后检查
    """
    natives_dir = os.path.join(MINECRAFT_DIR, "versions", version_id, f"{version_id}-natives")
    if not os.path.isdir(natives_dir):
        return False
    # 检查是否有任何 dll/so/dylib 文件
    for root, dirs, files in os.walk(natives_dir):
        for fname in files:
            if fname.endswith((".dll", ".so", ".dylib", ".jnilib")):
                return True
    return False


def check_memory_limits(max_memory: int) -> tuple[bool, str]:
    """
    P0-4: 校验内存分配是否合理
    - max_memory ≤ 系统可用物理内存
    - max_memory 不低于 512MB
    """
    try:
        import psutil
        avail = psutil.virtual_memory().available // (1024 * 1024)
    except ImportError:
        avail = get_system_memory() // 2  # 粗略估计

    if max_memory < 512:
        return False, f"分配内存 {max_memory}MB 过低，最低需要 512MB"
    if max_memory > avail:
        return False, f"分配内存 {max_memory}MB 超过系统可用内存 {avail}MB"
    if max_memory > avail * 0.9:
        return True, f"分配内存 {max_memory}MB 接近系统可用上限 {avail}MB，可能导致系统卡顿"
    return True, ""


def check_process_alive(process, timeout: float = 5.0) -> dict:
    """
    P0-5: 进程启动后存活检测
    在 timeout 秒内持续检测进程是否存活
    返回: {"alive": bool, "exit_code": int|None, "early_error": str|None}
    """
    result = {"alive": False, "exit_code": None, "early_error": None}

    for _ in range(int(timeout * 10)):  # 每 100ms 检查一次
        time.sleep(0.1)
        rc = process.poll()
        if rc is not None:
            # 进程已退出
            result["exit_code"] = rc
            # 尝试读取早期日志
            try:
                lines = []
                for line in process.stdout:
                    lines.append(line.strip())
                    if len(lines) > 50:
                        break
                result["early_error"] = "\n".join(lines[-20:]) if lines else "无日志输出"
            except Exception:
                result["early_error"] = "无法读取日志"
            return result

    # timeout 内未退出 → 存活
    result["alive"] = True
    return result
