"""
LaunchMixin — 游戏启动/停止/进度管理
供 ShadowBackend 多重继承使用

启动流程：前置身份校验 → 运行环境校验 → 文件完整性校验 → 启动参数终检 → 进程早期监控
"""

import threading
import time
import sys
import subprocess

from PySide6.QtCore import Slot

from shadow_launcher.core.launcher import launch_minecraft, auto_memory, get_system_memory, get_memory_status
from shadow_launcher.core.versions import fetch_version_json
from shadow_launcher.ui.backend.prelaunch_check import (
    PreLaunchChecker,
    check_java_executable,
    check_java_arch,
    check_version_jar,
    check_version_json_valid,
    check_memory_limits,
    check_process_alive,
)


class LaunchMixin:
    """游戏启动功能"""

    # 启动状态（类型提示）
    _launch_version: str = ""
    _launch_username: str = ""
    _launch_memory: int = 4096
    _launch_progress: int = 0
    _launch_status: str = ""
    _launching: bool = False
    _launch_process = None
    _launch_cancelled: bool = False  # 用户取消标志
    _java_arch_info: str = ""  # 缓存 Java 架构信息

    # ═══ 内存工具 ═══

    @Slot(result=int)
    def getAutoMemory(self) -> int:
        return auto_memory()

    @Slot(result=int)
    def getSystemMemory(self) -> int:
        return get_system_memory()

    @Slot(result="QVariantMap")
    def getMemoryStatus(self):
        """获取系统内存状态，返回 dict 供 QML 使用"""
        return get_memory_status()

    # ═══ 启动 ═══

    @Slot(str)
    def launch(self, version_id: str):
        if self._launching:
            self.logMessage.emit("已在启动中")
            return

        if not self._account:
            self.logMessage.emit("请先登录")
            return

        if version_id not in self._installed_ids:
            self.logMessage.emit(f"{version_id} 尚未安装")
            return

        version_json = None
        if self._version_manifest:
            for v in self._version_manifest.versions:
                if v.id == version_id:
                    version_json = fetch_version_json(v)
                    break

        if version_json is None:
            self.logMessage.emit(f"无法获取 {version_id} 的数据")
            return

        # ═══════════════════════════════════════
        # 启动前校验（P0 五项 + 额外检查）
        # ═══════════════════════════════════════
        checker = PreLaunchChecker()
        self.launchProgressChanged.emit(2, "正在进行启动前检查...")

        # P0-1: Java 可执行文件校验
        java_path = check_java_executable()
        if java_path is None:
            checker.error("未找到有效的 Java 可执行文件，请在设置中配置 Java 路径")
        else:
            # 检测 Java 架构
            is_64bit, arch_str = check_java_arch(java_path)
            self._java_arch_info = arch_str
            self.logMessage.emit(f"Java: {java_path} ({arch_str})")

            if not is_64bit:
                checker.warn("检测到 32 位 Java，最大内存限制为 2GB，建议更换 64 位 Java")

        # P0-2: 核心 Jar 文件
        jar_path = check_version_jar(version_id)
        if jar_path is None:
            checker.error(f"版本 {version_id} 的核心 Jar 文件缺失，请重新安装该版本")

        # version.json 校验
        json_path = check_version_json_valid(version_id)
        if json_path is None:
            checker.error(f"版本 {version_id} 的配置文件已损坏")
        else:
            # 额外：用本地 json 覆盖远程拉取的 version_json（本地包含完整 libraries 信息）
            try:
                import json as _json
                with open(json_path, "r", encoding="utf-8") as f:
                    local_json = _json.load(f)
                if local_json and "libraries" in local_json:
                    version_json = local_json
            except Exception:
                pass

        # P0-3: Natives 库校验（启动后解压时再细查，这里只检查目录）
        # 实际在 extract_natives 后检查

        # P0-4: 内存分配校验
        memory = auto_memory()
        mem_ok, mem_msg = check_memory_limits(memory)
        if not mem_ok:
            checker.error(mem_msg)
        elif mem_msg:
            checker.warn(mem_msg)

        # -- 检查结果汇总 --
        if not checker.all_pass:
            for err in checker.errors:
                self.logMessage.emit(f"校验失败: {err}")
            self.launchProgressChanged.emit(0, "")
            # 返回错误列表给 QML（通过 logMessage）
            self.logMessage.emit(f">>> 启动已阻止：{len(checker.errors)} 项检查未通过")
            return

        for warn in checker.warnings:
            self.logMessage.emit(f"预警: {warn}")

        # ═══════════════════════════════════════
        # 校验通过，正式开始启动
        # ═══════════════════════════════════════
        self._launching = True
        self._launch_version = version_id
        self._launch_username = self._account.username
        self._launch_memory = memory
        self._launch_progress = 0
        self._launch_status = "正在准备..."

        # 32位 Java 强制限制内存上限
        if self._java_arch_info == "32-bit" and memory > 2048:
            memory = 2048
            self._launch_memory = memory
            self.logMessage.emit("32位 Java 限制内存为 2048MB")

        self.launchProgressChanged.emit(5, "正在准备启动参数...")
        self.logMessage.emit(
            f"启动 {version_id} | {self._account.username} | 内存 {memory}MB"
        )

        def _launch():
            try:
                # Phase 1: 准备阶段（逐步推进进度）
                for i in range(1, 6):
                    if self._launch_cancelled:
                        self._launching = False
                        self.launchProgressChanged.emit(0, "")
                        return
                    time.sleep(0.2)
                    self._update_launch_progress(5 + i * 8, f"准备中 ({i * 20}%)")

                if self._launch_cancelled:
                    self._launching = False
                    self.launchProgressChanged.emit(0, "")
                    return

                # Phase 2: 启动游戏进程
                self._update_launch_progress(45, "正在解压原生库...")
                self._launch_process = launch_minecraft(
                    version_id=version_id,
                    version_json=version_json,
                    username=self._account.username,
                    uuid=self._account.uuid,
                    access_token=self._account.access_token,
                    max_memory=memory,
                    logged_in=self._account.is_online,
                )

                # Phase 3: 进程已启动 → 存活检测
                self._update_launch_progress(70, "进程已启动，检测中...")
                self.logMessage.emit("游戏进程已启动，等待窗口...")
                self.minecraftStarted.emit()

                # P0-5: 进程早期存活检测（3秒）
                alive_result = check_process_alive(self._launch_process, timeout=3.0)
                if not alive_result["alive"]:
                    self._update_launch_progress(0, "启动失败")
                    self.logMessage.emit(
                        f"游戏进程异常退出 (exit code: {alive_result['exit_code']})"
                    )
                    if alive_result["early_error"]:
                        self.logMessage.emit(f"早期日志:\n{alive_result['early_error']}")
                    self._launching = False
                    self.minecraftStopped.emit()
                    self.launchProgressChanged.emit(0, "")
                    return

                # Phase 4: 进程存活，等待窗口初始化
                self._update_launch_progress(80, "进程运行正常")
                time.sleep(1.0)
                self._update_launch_progress(90, "窗口已打开")

                time.sleep(0.5)
                self._update_launch_progress(100, "启动完成")
                time.sleep(1.5)
                self._launching = False
                self.launchProgressChanged.emit(0, "")  # notify QML to close overlay

                # 日志监控线程
                def _watch():
                    try:
                        for line in self._launch_process.stdout:
                            self.logMessage.emit(line.rstrip())
                    except Exception:
                        pass
                    rc = self._launch_process.wait()
                    self.logMessage.emit(f"--- 退出 (exit code: {rc}) ---")
                    self._launch_process = None
                    self._launching = False
                    self.minecraftStopped.emit()
                    self.launchProgressChanged.emit(0, "")

                threading.Thread(target=_watch, daemon=True).start()

            except Exception as e:
                self.logMessage.emit(f"启动失败: {e}")
                self._launching = False
                self.launchProgressChanged.emit(0, "")

        threading.Thread(target=_launch, daemon=True).start()

    def _update_launch_progress(self, value: int, status: str):
        self._launch_progress = value
        self._launch_status = status
        self.launchProgressChanged.emit(value, status)

    @Slot()
    def cancelLaunch(self):
        self._launch_cancelled = True  # signal the launch thread to abort
        if self._launch_process:
            pid = self._launch_process.pid
            try:
                # Windows: kill entire process tree
                if sys.platform == "win32":
                    subprocess.run(["taskkill", "/F", "/T", "/PID", str(pid)],
                                   capture_output=True, timeout=10)
                else:
                    self._launch_process.kill()
                self.logMessage.emit("已强制结束游戏进程")
            except Exception as e:
                self.logMessage.emit(f"结束进程失败: {e}")
            self._launch_process = None
        else:
            self.logMessage.emit("用户取消了启动")
        self._launching = False
        self.launchProgressChanged.emit(0, "")

    @Slot()
    def killGameProcess(self):
        if self._launch_process:
            pid = self._launch_process.pid
            try:
                if sys.platform == "win32":
                    subprocess.run(["taskkill", "/F", "/T", "/PID", str(pid)],
                                   capture_output=True, timeout=10)
                else:
                    self._launch_process.kill()
                self.logMessage.emit("已强制结束游戏进程")
            except Exception as e:
                self.logMessage.emit(f"结束进程失败: {e}")
            self._launch_process = None
            self._launching = False
            self.launchProgressChanged.emit(0, "")

    @Slot()
    def killMinecraft(self):
        """QML 调用的强制结束入口"""
        self.killGameProcess()
