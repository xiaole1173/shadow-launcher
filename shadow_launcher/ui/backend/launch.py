"""
LaunchMixin — 游戏启动/停止/进度管理
供 ShadowBackend 多重继承使用

所有 Property 在 ShadowBackend 主类中声明，
Mixin 只包含 Slot 方法和内存工具。
"""

import threading
import time

from PySide6.QtCore import Slot

from shadow_launcher.core.launcher import launch_minecraft, auto_memory, get_system_memory, get_memory_status
from shadow_launcher.core.versions import fetch_version_json


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

        self._launching = True
        self._launch_version = version_id
        self._launch_username = self._account.username
        memory = auto_memory()
        self._launch_memory = memory
        self._launch_progress = 0
        self._launch_status = "正在准备..."

        self.launchProgressChanged.emit(0, "正在准备启动参数...")
        self.logMessage.emit(
            f"🚀 启动 {version_id} | {self._account.username} | 自动分配 {memory}MB"
        )

        def _launch():
            try:
                # Phase 1: 准备阶段（逐步推进进度）
                for i in range(1, 6):
                    time.sleep(0.2)
                    self._update_launch_progress(i * 10, f"准备中 ({i * 20}%)")

                # Phase 2: 启动游戏进程
                self._update_launch_progress(55, "正在启动游戏进程...")
                self._launch_process = launch_minecraft(
                    version_id=version_id,
                    version_json=version_json,
                    username=self._account.username,
                    uuid=self._account.uuid,
                    access_token=self._account.access_token,
                    max_memory=memory,
                    logged_in=self._account.is_online,
                )

                # Phase 3: 进程已启动
                self._update_launch_progress(70, "进程已启动")
                self.logMessage.emit("游戏进程已启动，等待窗口...")
                self.minecraftStarted.emit()

                # Phase 4: 等待窗口初始化
                time.sleep(1.0)
                self._update_launch_progress(90, "窗口已打开")

                time.sleep(0.5)
                self._update_launch_progress(100, "启动完成")

                def _watch():
                    for line in self._launch_process.stdout:
                        self.logMessage.emit(line.rstrip())
                    rc = self._launch_process.wait()
                    self.logMessage.emit(f"--- 退出 (code={rc}) ---")
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
        if self._launch_process and self._launch_process.poll() is None:
            self._launch_process.terminate()
            self.logMessage.emit("用户取消了启动")
        self._launching = False
        self.launchProgressChanged.emit(0, "")

    @Slot()
    def killGameProcess(self):
        if self._launch_process and self._launch_process.poll() is None:
            self._launch_process.kill()
            self.logMessage.emit("⚠ 已强制结束游戏进程")
            self._launching = False
            self.launchProgressChanged.emit(0, "")
