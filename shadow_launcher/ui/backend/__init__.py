"""
ShadowBackend — Python → QML 桥接层
通过 Property / Signal / Slot 暴露给 QML 引擎

拆分为独立功能模块：
- account.py   — 账号登录/登出
- version.py   — 版本列表/安装/刷新
- launch.py    — 游戏启动/停止/进度
- settings.py  — 版本隔离/工具方法
"""

import json
import os
import psutil
import subprocess
import sys
import threading
import time

from PySide6.QtCore import QObject, Signal, Slot, Property
import os

from shadow_launcher.core.versions import (
    MINECRAFT_DIR, get_installed_versions,
    is_isolation_enabled, get_isolated_versions,
    get_version_game_dir,
)
from shadow_launcher.core.downloader import MIRROR_SOURCES

from .account import AccountMixin
from .version import VersionMixin
from .launch import LaunchMixin
from .settings import SettingsMixin
from .resource import ResourceMixin


class ShadowBackend(QObject, AccountMixin, VersionMixin, LaunchMixin, SettingsMixin, ResourceMixin):
    """主后端，组合所有功能 Mixin

    Mixin 只提供 Slot 方法和初始化逻辑。
    所有 Property 和 Signal 在此统一声明。
    """

    # ═══ 信号 ═══
    logMessage = Signal(str)
    versionListReady = Signal()
    installedVersionsChanged = Signal()
    accountChanged = Signal()
    launchProgressChanged = Signal(int, str)
    launchCancelled = Signal()
    minecraftStarted = Signal()
    minecraftStopped = Signal()
    installProgressChanged = Signal(int)
    installVersionChanged = Signal(str)
    installTotalChanged = Signal(int)
    installFileProgress = Signal(str)
    installFinished = Signal(bool)
    installBytesProgress = Signal(int, int)
    installPhaseChanged = Signal(str)
    isolationChanged = Signal()
    installingChanged = Signal()
    versionListRefreshing = Signal()
    installQueueChanged = Signal()
    installActiveChanged = Signal()

    # 资源下载信号
    searchResultsReady = Signal(list)
    resourceDownloadProgress = Signal(int, int, str)
    resourceDownloadDone = Signal(bool)

    # 皮肤
    skinReady = Signal()

    # 设置信号
    memorySettingsChanged = Signal()
    generalSettingsChanged = Signal()

    def __init__(self, parent=None):
        QObject.__init__(self, parent)

        # ═══ 持久化日志 ═══
        from shadow_launcher.core.logger import log_info, log_user_action, get_log_path, get_recent_logs
        self.logMessage.connect(log_info)
        log_info("Backend 初始化完成")

        # ═══ 共享状态 ═══
        self._version_manifest = None
        self._version_ids = []
        self._installed_ids = []
        self._selected_version = ""
        self._source_index = 0
        self._isolation_enabled = is_isolation_enabled()
        self._isolated_versions = get_isolated_versions()
        self._java_path = ""
        self._java_version = ""
        self._java_major = 0

        # 调用各 Mixin 的初始化
        self._init_account()
        self._init_versions()

        # Load session state (last used dir, version)
        self._load_session_state()

        # Scan version details (async, don't block GUI)
        threading.Thread(target=self._async_initial_scan, daemon=True).start()

        # ═══ 下载队列系统 ═══
        self._download_queue: list = []  # 排队中的版本
        self._active_downloads: list = []  # 正在下载（最多2个）
        self._download_lock = threading.Lock()
        self._max_concurrent_downloads = 2

        # 文件变化监控
        self._last_installed_count = len(self._installed_ids)

    gameDirInfoChanged = Signal()

    @Property("QVariantMap", notify=gameDirInfoChanged)
    def gameDirInfo(self):
        base = self._game_dir if hasattr(self, "_game_dir") and self._game_dir else MINECRAFT_DIR
        info = {"versionCount": 0, "modCount": 0, "sizeDisplay": ""}
        try:
            from shadow_launcher.core.versions import get_installed_versions
            vers = get_installed_versions(base)
            info["versionCount"] = len(vers)

            # Mod count
            mods = os.path.join(base, "mods")
            if os.path.isdir(mods):
                info["modCount"] = sum(1 for f in os.listdir(mods) if f.endswith(".jar"))

            # Size — scan all relevant dirs
            total = 0
            for scan in ["libraries", "versions", "mods", "config", "saves", "resourcepacks", "shaderpacks", os.path.join("assets", "objects")]:
                sd = os.path.join(base, scan)
                if os.path.isdir(sd):
                    for dirpath, _, filenames in os.walk(sd):
                        for fn in filenames:
                            try:
                                total += os.path.getsize(os.path.join(dirpath, fn))
                            except OSError:
                                pass
            info["sizeDisplay"] = self._format_size(total)
        except Exception:
            pass
        return info

    @Slot()
    def refreshGameDirInfo(self):
        self.gameDirInfoChanged.emit()

    @Property(str, notify=gameDirInfoChanged)
    def diskFree(self):
        try:
            import shutil
            base = self._game_dir if hasattr(self, '_game_dir') and self._game_dir else MINECRAFT_DIR
            usage = shutil.disk_usage(base)
            free_gb = usage.free / (1024**3)
            if free_gb >= 1:
                return f"{free_gb:.1f} GB"
            return f"{int(usage.free / (1024**2))} MB"
        except Exception:
            return "未知"

    @Property(float, notify=gameDirInfoChanged)
    def diskPercent(self):
        try:
            import shutil
            base = self._game_dir if hasattr(self, '_game_dir') and self._game_dir else MINECRAFT_DIR
            usage = shutil.disk_usage(base)
            return round(usage.used / usage.total * 100, 1)
        except Exception:
            return 0

    def _async_initial_scan(self):
        self._scan_version_details()
        self.versionDetailsChanged.emit()
        self.installedVersionsChanged.emit()

    def _session_file(self):
        return os.path.join(os.path.dirname(__file__), "..", "..", "config", "session.json")

    def _load_session_state(self):
        try:
            sf = self._session_file()
            if os.path.exists(sf):
                with open(sf, "r") as f:
                    state = json.load(f)
                self._selected_version = state.get("version", "")
                self._source_index = state.get("sourceIndex", 0)
                self._last_login_mode = state.get("lastLoginMode", 0)
        except Exception:
            pass

    def _save_session_state(self):
        try:
            sf = self._session_file()
            os.makedirs(os.path.dirname(sf), exist_ok=True)
            with open(sf, "w") as f:
                f.write(json.dumps({
                    "version": self._selected_version,
                    "sourceIndex": self._source_index,
                    "lastLoginMode": self._last_login_mode
                }))
        except Exception:
            pass

    @Property(str, notify=generalSettingsChanged)
    def selectedVersion(self):
        return self._selected_version

    @Slot(str)
    def setSelectedVersion(self, v: str):
        self._selected_version = v
        self._save_session_state()
        self.generalSettingsChanged.emit()

    @Property(int, notify=generalSettingsChanged)
    def lastLoginMode(self):
        return getattr(self, '_last_login_mode', 0)

    @Slot(int)
    def setLastLoginMode(self, mode: int):
        self._last_login_mode = mode
        self._save_session_state()
        self.generalSettingsChanged.emit()

    # ═══ 账号属性 ═══

    @Property(str, notify=accountChanged)
    def username(self):
        return self._username

    @Property(bool, notify=accountChanged)
    def isLoggedIn(self):
        return self._is_logged_in

    @Property(bool, notify=accountChanged)
    def isOnline(self):
        return self._is_online

    @Property(str, notify=accountChanged)
    def accountUuid(self):
        return self._account_uuid

    @Property(str, notify=skinReady)
    def skinPath(self):
        # Return as file:// URL for QML Image source
        p = os.path.abspath(self._skin_path) if self._skin_path else ""
        return "file:///" + p.replace("\\", "/") if p else ""

    @Property(str, notify=accountChanged)
    def playerName(self):
        return self._username

    # ═══ 版本属性 ═══

    @Property("QVariantList", notify=versionListReady)
    def versionIds(self):
        return self._version_ids

    @Property("QVariantList", notify=versionListReady)
    def versionList(self):
        """Return [{id, type}] for all versions in manifest, for QML filtering."""
        if not self._version_manifest:
            return []
        return [{"id": v.id, "type": v.type} for v in self._version_manifest.versions]

    @Property("QVariantList", notify=versionListReady)
    def releaseVersions(self):
        """Pre-filtered: release versions only [id, ...]"""
        return [v.id for v in self._version_manifest.versions if v.type == "release"] if self._version_manifest else []

    @Property("QVariantList", notify=versionListReady)
    def snapshotVersions(self):
        """Pre-filtered: snapshot versions only [id, ...]"""
        return [v.id for v in self._version_manifest.versions if v.type == "snapshot"] if self._version_manifest else []

    @Property("QVariantList", notify=versionListReady)
    def oldVersions(self):
        """Pre-filtered: old_alpha/old_beta versions only [id, ...]"""
        return [v.id for v in self._version_manifest.versions if v.type in ("old_alpha", "old_beta")] if self._version_manifest else []

    @Property("QVariantList", notify=installedVersionsChanged)
    def installedVersions(self):
        """返回已安装版本列表，合并队列/下载中的版本确保UI一致性"""
        result = list(self._installed_ids) if self._installed_ids else []
        with self._download_lock:
            for item in self._download_queue:
                vid = item.get("versionId", "")
                if vid and vid not in result:
                    result.append(vid)
            for item in self._active_downloads:
                vid = item.get("versionId", "")
                if vid and vid not in result:
                    result.append(vid)
        return result

    @Property(str, notify=versionListReady)
    def selectedVersion(self):
        return self._selected_version

    # ═══ 下载源列表 ═══

    @Property("QVariantList", constant=True)
    def mirrorSources(self):
        return [{"name": m.name, "desc": m.desc} for m in MIRROR_SOURCES]

    # ═══ 启动属性 ═══

    @Property(str, notify=launchProgressChanged)
    def launchVersion(self):
        return self._launch_version

    @Property(str, notify=launchProgressChanged)
    def launchUsername(self):
        return self._launch_username

    @Property(int, notify=launchProgressChanged)
    def launchMemory(self):
        return self._launch_memory

    @Property(int, notify=launchProgressChanged)
    def launchProgress(self):
        return self._launch_progress

    @Property(str, notify=launchProgressChanged)
    def launchStatus(self):
        return self._launch_status

    @Property(bool, notify=launchProgressChanged)
    def launching(self):
        return self._launching

    isRunningChanged = Signal()

    @Property(bool, notify=isRunningChanged)
    def isRunning(self):
        return self._launch_process is not None and self._launch_process.poll() is None

    # ═══ 安装属性（实时，由 stdout 管道更新） ═══

    _install_state_file = os.path.join(os.path.dirname(__file__), "..", "..", "config", "install_state.json")
    _install_process = None
    _install_reader_thread = None
    
    # Internal storage for property values
    _install_progress = 0
    _install_total = 0
    _install_bytes_downloaded = 0
    _install_bytes_total = 0
    _install_file = ""
    _install_speed = 0.0
    _install_running = False
    _install_phase = ""
    _install_version_id = ""
    _install_elapsed = 0.0

    @Property(int, notify=installProgressChanged)
    def installProgress(self):
        return self._install_progress

    @Property(int, notify=installTotalChanged)
    def installTotal(self):
        return self._install_total

    @Property(str, notify=installFileProgress)
    def installFile(self):
        return self._install_file

    @Property(bool, notify=installingChanged)
    def installing(self):
        return self._install_running

    @Property(int, notify=installBytesProgress)
    def installBytesDownloaded(self):
        return self._install_bytes_downloaded

    @Property(int, notify=installBytesProgress)
    def installBytesTotal(self):
        return self._install_bytes_total

    @Property(str, notify=installPhaseChanged)
    def installPhase(self):
        return self._install_phase

    @Property(str, notify=installVersionChanged)
    def installVersionId(self):
        return self._install_version_id

    @Property(float, notify=installBytesProgress)
    def installSpeed(self):
        return self._install_speed


    @Slot(str, int)
    def installVersion(self, version_id: str, source_index: int = 0):
        """安装版本 - 支持多版本排队下载"""
        cli_path = os.path.join(os.path.dirname(__file__), "..", "..", "..", "install_cli.py")
        cli_path = os.path.abspath(cli_path)

        if not os.path.exists(cli_path):
            self.logMessage.emit(f"未找到安装脚本: {cli_path}")
            return

        # 检查是否已经安装
        installed = get_installed_versions()
        if version_id in installed:
            self.logMessage.emit(f"{version_id} 已安装，无需重复下载")
            return

        # ═══ 下载队列逻辑 ═══
        with self._download_lock:
            for existing in self._active_downloads + self._download_queue:
                if existing.get("versionId") == version_id:
                    self.logMessage.emit(f"{version_id} 已在下载队列中")
                    return

            download_item = {
                "versionId": version_id, "sourceIndex": source_index,
                "state": "downloading", "progress": 0, "total": 0,
                "bytesDownloaded": 0, "bytesTotal": 0, "file": "",
                "speed": 0.0, "phase": "starting", "t0": time.time(),
            }

            if len(self._active_downloads) < self._max_concurrent_downloads:
                self._active_downloads.append(download_item)
            else:
                download_item["state"] = "queued"
                self._download_queue.append(download_item)
                self.logMessage.emit(f"{version_id} 已加入下载队列 (位置 {len(self._download_queue)})")
                self.installQueueChanged.emit()
                self.installActiveChanged.emit()
                return

            self.installActiveChanged.emit()

        self.logMessage.emit(f"开始下载: {version_id} (源: {source_index})")

        # Reset all install state
        self._install_progress = 0
        self._install_total = 0
        self._install_bytes_downloaded = 0
        self._install_bytes_total = 0
        self._install_file = ""
        self._install_speed = 0.0

        cmd = [sys.executable, cli_path, version_id, "--source", str(source_index)]
        try:
            self._install_process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                encoding='utf-8',
                errors='replace',
                cwd=os.path.dirname(cli_path),
            )
        except Exception as e:
            self.logMessage.emit(f"启动安装进程失败: {e}")
            return

        self.installPhaseChanged.emit("starting")
        self.installingChanged.emit()
        self.installVersionChanged.emit(version_id)
        self._install_t0 = time.time()

        def _read_pipe():
            """Read stdout lines from subprocess and emit signals."""
            try:
                for line in self._install_process.stdout:
                    line = line.strip()
                    if not line:
                        continue

                    if line.startswith("PROG:"):
                        parts = line[5:].split(":", 4)
                        if len(parts) >= 5:
                            try:
                                cf = int(parts[0])
                                tf = int(parts[1])
                                db = int(parts[2])
                                tb = int(parts[3])
                                name = parts[4]
                            except ValueError:
                                continue
                            self._install_progress = cf
                            self._install_total = tf
                            self._install_bytes_downloaded = db
                            self._install_bytes_total = tb
                            self._install_file = name.replace('???', '')
                            if self._install_phase != "downloading":
                                self._install_phase = "downloading"
                                self.installPhaseChanged.emit("downloading")
                            self.installProgressChanged.emit(cf)
                            self.installTotalChanged.emit(tf)
                            self.installFileProgress.emit(name)
                            elapsed = time.time() - self._install_t0 if time.time() > self._install_t0 else 0.001
                            self._install_speed = db / elapsed if elapsed > 0 else 0
                            self.installBytesProgress.emit(db, tb)
                            if cf % 50 == 0 and tf > 0:
                                pct = cf / tf * 100
                                self.logMessage.emit(f"[安装] {version_id} | {pct:.1f}% ({cf}/{tf}) | {self._install_speed/1048576:.1f} MB/s | {name[:40]}")

                    elif line.startswith("LOG:"):
                        self.logMessage.emit(line[4:])

                    elif line.startswith("DONE:"):
                        self._install_phase = "done"
                        self.installPhaseChanged.emit("done")
                        self.logMessage.emit(f"✅ {version_id} 安装完成！")
                        self.refreshInstalled()
                        self._on_download_complete(download_item)
                        break

                    elif line.startswith("FAIL:"):
                        self._install_phase = "failed"
                        self.installPhaseChanged.emit("failed")
                        self.logMessage.emit(f"FAIL {line[5:]}")
                        self.refreshInstalled()
                        self._on_download_complete(download_item)
                        break

                    elif line.startswith("CANCEL"):
                        self._install_phase = "cancelled"
                        self.installPhaseChanged.emit("cancelled")
                        self._on_download_complete(download_item)
                        break
            except Exception as e:
                self.logMessage.emit(f"读取安装进度失败: {e}")
                self._on_download_complete(download_item)
            finally:
                self._install_process = None
                self._install_reader_thread = None

        self._install_reader_thread = threading.Thread(target=_read_pipe, daemon=True)
        self._install_reader_thread.start()

    def _on_download_complete(self, item: dict):
        """下载完成后的清理和队列推进"""
        next_version = None
        next_source = 0

        with self._download_lock:
            if item in self._active_downloads:
                self._active_downloads.remove(item)

            if self._download_queue and len(self._active_downloads) < self._max_concurrent_downloads:
                next_item = self._download_queue.pop(0)
                next_version = next_item["versionId"]
                next_source = next_item.get("sourceIndex", 0)

            self.installQueueChanged.emit()
            self.installActiveChanged.emit()

        # 同步到旧的单版本属性
        if next_version:
            self.installVersion(next_version, next_source)

        if self._active_downloads:
            self._sync_install_state_from_active()
        else:
            def _delayed_reset():
                time.sleep(3)
                self._install_running = False
                self._install_phase = ""
                self.installingChanged.emit()
                self.installPhaseChanged.emit("")
            threading.Thread(target=_delayed_reset, daemon=True).start()

    def _sync_install_state_from_active(self):
        """将当前活跃下载同步到旧的单版本属性（兼容 DownloadPage QML 绑定）"""
        if not self._active_downloads:
            return
        first = self._active_downloads[0]
        self._install_running = True
        self._install_phase = first.get("phase", "downloading")
        self._install_version_id = first["versionId"]
        self.installingChanged.emit()
        self.installPhaseChanged.emit(self._install_phase)
        self.installVersionChanged.emit(self._install_version_id)

    # ═══ 下载队列属性 ═══

    @Property("QVariantList", notify=installQueueChanged)
    def downloadQueue(self):
        """排队中的下载项 [{versionId, state, ...}]"""
        return [dict(item) for item in self._download_queue]

    @Property("QVariantList", notify=installActiveChanged)
    def activeDownloads(self):
        """活跃的下载项 [{versionId, state, progress, total, ...}]"""
        return [dict(item) for item in self._active_downloads]

    @Slot(str)
    def cancelQueuedDownload(self, version_id: str):
        """取消队列中的下载"""
        with self._download_lock:
            for item in self._download_queue:
                if item.get("versionId") == version_id:
                    self._download_queue.remove(item)
                    self.logMessage.emit(f"已取消 {version_id} 的排队下载")
                    self.installQueueChanged.emit()
                    return
            self.logMessage.emit(f"{version_id} 不在排队中")

    # ═══ 版本列表刷新 ═══

    @Slot()
    def refreshVersionList(self):
        """手动刷新版本列表"""
        self.versionListRefreshing.emit()
        self.logMessage.emit("正在刷新版本列表...")

        def _refresh():
            try:
                from shadow_launcher.core.versions import fetch_version_manifest
                manifest = fetch_version_manifest(use_mirror=True)
                self._version_manifest = manifest
                self._version_ids = [v.id for v in manifest.versions]
                self._installed_ids = get_installed_versions()
                self.versionListReady.emit()
                self.installedVersionsChanged.emit()
                self._last_installed_count = len(self._installed_ids)
                self.logMessage.emit(
                    f"版本列表已刷新 (正式版: {len([v for v in manifest.versions if v.type == 'release'])}, "
                    f"快照: {len([v for v in manifest.versions if v.type == 'snapshot'])})"
                )
            except Exception as e:
                self.logMessage.emit(f"刷新版本列表失败: {e}")

        t = threading.Thread(target=_refresh, daemon=True)
        t.start()

    @Slot()
    def checkFileChanges(self):
        """检查游戏文件是否变化，自动刷新"""
        try:
            current_count = len(get_installed_versions())
            if current_count != self._last_installed_count:
                self._last_installed_count = current_count
                self.logMessage.emit("检测到游戏文件变化，自动刷新...")
                self.refreshInstalled()
                self.refreshVersionDetails()
        except Exception:
            pass

    @Slot()
    def cancelInstall(self):
        if self._install_process:
            self._install_process.terminate()
            self._install_process = None
        if self._install_poll_timer:
            self._install_poll_timer.cancel()
            self._install_poll_timer = None
        self.logMessage.emit("⚠ 安装已取消")

    @Slot()
    def refreshInstallState(self):
        """手动刷新安装状态（供 QML Timer 调用）"""
        self._poll_install_state()

    # ═══ 工具属性 ═══

    @Property(str, constant=True)
    def gameDir(self):
        return MINECRAFT_DIR

    # ═══ 版本隔离属性 ═══

    @Property(bool, notify=isolationChanged)
    def isolationEnabled(self):
        return self._isolation_enabled

    @Property("QVariantList", notify=isolationChanged)
    def isolatedVersions(self):
        return self._isolated_versions


    # ═══ Java 属性 ═══

    javaPathChanged = Signal()

    @Property(str, notify=javaPathChanged)
    def javaPath(self):
        return self._java_path

    @javaPath.setter
    def javaPath(self, value):
        if self._java_path != value:
            self._java_path = value
            self.javaPathChanged.emit()

    @Property("QVariantList", notify=javaPathChanged)
    def javaList(self):
        """Return list of all detected Java installations [{path, version, major}]"""
        from .settings import find_java_installations
        results = find_java_installations()
        # Deduplicate by major version (keep first occurrence = highest path priority)
        seen_majors = set()
        deduped = []
        for item in [{"path": p, "version": v, "major": m} for p, v, m in results]:
            if item["major"] not in seen_majors:
                seen_majors.add(item["major"])
                deduped.append(item)
        return deduped

    @Property(str, notify=javaPathChanged)
    def javaVersion(self):
        return self._java_version

    @Property(int, notify=javaPathChanged)
    def javaMajor(self):
        return self._java_major

    @Property(str, notify=javaPathChanged)
    def javaCompatibility(self) -> str:
        """Return 'compatible', 'recommended', 'incompatible', or 'unknown' based on selected version"""
        if not self._java_major or not self._selected_version:
            return "unknown"
        # MC version → recommended Java
        # 1.6-1.11 → Java 8
        # 1.12-1.16 → Java 8 or 11
        # 1.17 → Java 16
        # 1.18 → Java 17
        # 1.19 → Java 17
        # 1.20+ → Java 17+
        vid = self._selected_version
        parts = vid.replace("forge-", "").replace("fabric-", "").replace("neoforge-", "").split(".")
        try:
            mc_major = int(parts[1]) if len(parts) > 1 and parts[0] == "1" else int(parts[0])
        except (ValueError, IndexError):
            return "unknown"
        java_major = self._java_major
        # 1.17 needs Java 16+
        if mc_major == 17:
            return "compatible" if java_major >= 16 else "incompatible"
        # 1.18+ needs Java 17+
        if mc_major >= 18:
            return "compatible" if java_major >= 17 else "incompatible"
        # 1.12-1.16: Java 8+ okay
        if 12 <= mc_major <= 16:
            return "recommended" if java_major == 8 else ("compatible" if java_major <= 17 else "incompatible")
        # 1.6-1.11: Java 8 best
        if mc_major <= 11:
            return "recommended" if java_major == 8 else ("compatible" if java_major <= 11 else "incompatible")
        return "compatible"

    @Property(bool, notify=javaPathChanged)
    def javaInstalled(self):
        return bool(self._java_path and os.path.isfile(self._java_path))

    # ═══ JVM 参数 ═══
    _jvm_args: str = "-Xmx2G -XX:+UseG1GC -XX:+UnlockExperimentalVMOptions"
    _game_args: str = ""

    @Property(str, notify=memorySettingsChanged)
    def jvmArgs(self):
        return self._jvm_args

    @Slot(str)
    def setJvmArgs(self, args: str):
        self._jvm_args = args
        self.memorySettingsChanged.emit()

    @Property(str, notify=memorySettingsChanged)
    def gameArgs(self):
        return self._game_args

    @Slot(str)
    def setGameArgs(self, args: str):
        self._game_args = args
        self.memorySettingsChanged.emit()

    @Slot(result=str)
    def pickJava(self) -> str:
        """Open file dialog to pick Java executable."""
        from PySide6.QtWidgets import QFileDialog
        path, _ = QFileDialog.getOpenFileName(None, "选择 Java 可执行文件", "",
            "Java (javaw.exe java.exe);;All Files (*.*)")
        if path:
            self.javaPath = path
            self._detect_java_version(path)
        return path

    @Slot()
    def detectJava(self):
        """Auto-detect Java installation — 使用全面的 find_java_installations."""
        from .settings import find_java_installations, pick_best_java
        results = find_java_installations()
        if results:
            # Prefer Java 17+ for modern MC versions, fall back to highest available
            best = pick_best_java(results, min_version=17)
            if best:
                path, ver_str, major = best
                self._java_path = path
                self._java_version = ver_str
                self._java_major = major
                self.javaPathChanged.emit()
                self.logMessage.emit(f"自动检测: Java {major} ({path})")
                return
        self.logMessage.emit("未找到 Java，请手动指定路径")

    def _detect_java_version(self, java_path: str):
        """Detect Java version from a java executable."""
        import subprocess, re
        try:
            result = subprocess.run([java_path, "-version"], capture_output=True, text=True, timeout=10)
            output = result.stdout + result.stderr
            m = re.search(r'version "([\d.]+)', output)
            if m:
                ver = m.group(1)
                self._java_version = ver
                self._java_major = int(ver.split('.')[0])
                self.javaPathChanged.emit()
                return
        except Exception:
            pass
        self._java_version = "未知"
        self._java_major = 0
        self.javaPathChanged.emit()

    # ═══ Memory properties ═══

    @Property(int, notify=memorySettingsChanged)
    def minMemoryMb(self):
        return self._min_memory_mb

    @Property(int, notify=memorySettingsChanged)
    def maxMemoryMb(self):
        return self._max_memory_mb

    @Property(str, notify=memorySettingsChanged)
    def minMemory(self):
        return f"{self._min_memory_mb}M"

    @Property(str, notify=memorySettingsChanged)
    def maxMemory(self):
        return f"{self._max_memory_mb}M"

    # ═══ General settings properties ═══

    @Property(bool, notify=generalSettingsChanged)
    def closeAfterLaunch(self):
        return self._close_after_launch

    @Property(bool, notify=generalSettingsChanged)
    def closeOnLaunch(self):
        """QML alias"""
        return self._close_after_launch

    @Slot(bool)
    def setCloseOnLaunch(self, value: bool):
        self.setCloseAfterLaunch(value)

    @Property(str, notify=generalSettingsChanged)
    def logLevel(self):
        return self._log_level

    # ═══ Game directories ═══

    gameDirsChanged = Signal()

    @Property("QVariantList", notify=gameDirsChanged)
    def gameDirectories(self):
        dirs = [MINECRAFT_DIR]

        # Auto-detect official Minecraft paths
        official_paths = [
            os.path.join(os.environ.get("APPDATA", ""), ".minecraft"),
            os.path.join(os.environ.get("LOCALAPPDATA", ""), "Packages",
                         "Microsoft.4297127D64EC6_8wekyb3d8bbwe", "LocalCache", "Local", "minecraft"),
        ]
        for p in official_paths:
            if p and os.path.isdir(p) and p != MINECRAFT_DIR and p not in dirs:
                dirs.append(p)

        # Scan for extra game dirs
        extra_file = os.path.join(os.path.dirname(__file__), "..", "..", "config", "game_dirs.json")
        if os.path.exists(extra_file):
            try:
                with open(extra_file, "r") as f:
                    extra = json.loads(f.read())
                    if isinstance(extra, list):
                        for p in extra:
                            if p not in dirs:
                                dirs.append(p)
            except Exception:
                pass
        return dirs

    @Slot(int)
    def setGameDir(self, index: int):
        """Set active game directory by index"""
        dirs = self.gameDirectories
        if 0 <= index < len(dirs):
            self._game_dir = dirs[index]
            self.logMessage.emit(f"已切换游戏目录: {dirs[index]}")
            self.refreshInstalled()
            self.installedVersionsChanged.emit()

    @Slot()
    def addGameDir(self):
        """Open file dialog to add game directory"""
        from PySide6.QtWidgets import QFileDialog
        path = QFileDialog.getExistingDirectory(None, "选择 Minecraft 文件夹", MINECRAFT_DIR)
        if path and os.path.isdir(path):
            extra_file = os.path.join(os.path.dirname(__file__), "..", "..", "config", "game_dirs.json")
            existing = []
            if os.path.exists(extra_file):
                try:
                    with open(extra_file, "r") as f:
                        existing = json.loads(f.read())
                except Exception:
                    pass
            if path not in existing and path != MINECRAFT_DIR:
                existing.append(path)
                os.makedirs(os.path.dirname(extra_file), exist_ok=True)
                with open(extra_file, "w") as f:
                    f.write(json.dumps(existing, indent=2))
                self.gameDirsChanged.emit()
                self.logMessage.emit(f"已添加游戏文件夹: {path}")

    @Slot(int)
    def openGameDir(self, index: int = -1):
        """Open game directory by index or active dir"""
        dirs = self.gameDirectories
        if 0 <= index < len(dirs):
            p = dirs[index]
        else:
            p = self._game_dir if hasattr(self, "_game_dir") and self._game_dir else MINECRAFT_DIR
        if os.path.isdir(p):
            import subprocess as _sp
            _sp.run(["explorer", p], check=False)
            self.logMessage.emit("已打开: " + p)

    @Slot()
    def importModpack(self):
        from PySide6.QtWidgets import QFileDialog
        path, _ = QFileDialog.getOpenFileName(None, "导入整合包", "",
            "整合包 (*.zip *.mrpack);;Zip 文件 (*.zip);;所有文件 (*)")
        if not path:
            return
        self.logMessage.emit(f"[整合包] 正在导入: {os.path.basename(path)}")
        # Extract to versions folder
        base = self._game_dir if hasattr(self, '_game_dir') and self._game_dir else MINECRAFT_DIR
        ver_dir = os.path.join(base, "versions")
        os.makedirs(ver_dir, exist_ok=True)
        try:
            import zipfile
            import tempfile
            with tempfile.TemporaryDirectory() as tmp:
                with zipfile.ZipFile(path, 'r') as zf:
                    zf.extractall(tmp)
                # Look for version directories
                imported = 0
                for item in os.listdir(tmp):
                    item_path = os.path.join(tmp, item)
                    if os.path.isdir(item_path):
                        # Check if it's a version folder (has .json)
                        has_json = any(f.endswith('.json') for f in os.listdir(item_path))
                        if has_json:
                            import shutil
                            dst = os.path.join(ver_dir, item)
                            if not os.path.exists(dst):
                                shutil.copytree(item_path, dst)
                                imported += 1
                            else:
                                self.logMessage.emit(f"[整合包] 跳过已存在: {item}")
                if imported > 0:
                    self.refreshInstalled()
                    self.installedVersionsChanged.emit()
                    self._scan_version_details()
                    self.versionDetailsChanged.emit()
                    self.logMessage.emit(f"[整合包] 导入完成，{imported} 个版本")
                else:
                    self.logMessage.emit("[整合包] 未找到有效版本目录")
        except Exception as e:
            self.logMessage.emit(f"[整合包] 导入失败: {e}")

    @Slot(int)
    def removeGameDir(self, index: int):
        """Remove a custom game directory from the list (does not delete files)"""
        extra_file = os.path.join(os.path.dirname(__file__), "..", "..", "config", "game_dirs.json")
        dirs = self.gameDirectories
        if 0 < index < len(dirs):  # Index 0 is always MINECRAFT_DIR, can't remove
            path = dirs[index]
            existing = []
            if os.path.exists(extra_file):
                try:
                    with open(extra_file, "r") as f:
                        raw = f.read()
                        existing = json.loads(raw) if raw else []
                except Exception:
                    existing = []
            if path in existing:
                existing.remove(path)
            os.makedirs(os.path.dirname(extra_file), exist_ok=True)
            with open(extra_file, "w") as f:
                json.dump(existing, f)
            self.gameDirsChanged.emit()
            self.logMessage.emit(f"已移除目录: {path}")

    @Slot(int)
    def setDefaultGameDir(self, index: int):
        """Set a directory as the default (first in list)"""
        if index == 0:
            return
        dirs = list(self.gameDirectories)
        if 0 <= index < len(dirs):
            dirs.insert(0, dirs.pop(index))
            extra_file = os.path.join(os.path.dirname(__file__), "..", "..", "config", "game_dirs.json")
            custom = [d for d in dirs if d != MINECRAFT_DIR and d not in [
                os.path.join(os.environ.get("APPDATA", ""), ".minecraft"),
                os.path.join(os.environ.get("LOCALAPPDATA", ""), "Packages",
                             "Microsoft.4297127D64EC6_8wekyb3d8bbwe", "LocalCache", "Local", "minecraft"),
            ]]
            os.makedirs(os.path.dirname(extra_file), exist_ok=True)
            with open(extra_file, "w") as f:
                json.dump(custom, f)
            self.setGameDir(0)

    # ═══ Folder shortcuts ═══

    @Slot(str)
    def openVersionDir(self, version_id: str):
        dirs = self.gameDirectories
        base = self._game_dir if hasattr(self, '_game_dir') else dirs[0] if dirs else MINECRAFT_DIR
        vdir = os.path.join(base, "versions", version_id or "")
        if os.path.isdir(vdir):
            os.startfile(vdir)
        elif version_id:
            vdir = os.path.join(base, "versions")
            if os.path.isdir(vdir):
                os.startfile(vdir)

    @Slot(str)
    def copyVersionPath(self, version_id: str):
        """Copy version folder path to clipboard"""
        dirs = self.gameDirectories
        base = self._game_dir if hasattr(self, '_game_dir') else dirs[0] if dirs else MINECRAFT_DIR
        vdir = os.path.join(base, "versions", version_id or "")
        from PySide6.QtWidgets import QApplication
        QApplication.clipboard().setText(vdir)

    @Slot()
    def openSavesFolder(self):
        dirs = self.gameDirectories
        base = self._game_dir if hasattr(self, '_game_dir') else dirs[0] if dirs else MINECRAFT_DIR
        spath = os.path.join(base, "saves")
        if os.path.isdir(spath):
            os.startfile(spath)
        else:
            os.startfile(base)

    @Slot()
    def openScreenshotsFolder(self):
        spath = self._get_data_dir("screenshots")
        os.makedirs(spath, exist_ok=True)
        os.startfile(spath)

    @Slot()
    def openLogFolder(self):
        """打开日志文件夹"""
        from shadow_launcher.core.logger import LOG_DIR
        os.makedirs(LOG_DIR, exist_ok=True)
        os.startfile(LOG_DIR)

    @Slot(str)
    def logUserAction(self, action: str):
        """记录用户操作到持久化日志"""
        from shadow_launcher.core.logger import log_user_action
        log_user_action(action)

    @Slot(result=str)
    def getRecentLogs(self):
        """获取最近日志（供UI诊断面板显示）"""
        from shadow_launcher.core.logger import get_recent_logs
        return get_recent_logs(100)

    # ═══ 完整性校验信号和状态 ═══
    verifyProgressChanged = Signal(int, int)
    verifyResultReady = Signal(str)
    verifyFinished = Signal(bool)

    _verify_running: bool = False
    _verify_progress_done: int = 0
    _verify_progress_total: int = 0
    _verify_result_text: str = ""
    _verify_ok: bool = False

    @Property(bool, notify=verifyFinished)
    def verifyRunning(self):
        return self._verify_running

    @Property(int, notify=verifyProgressChanged)
    def verifyProgressDone(self):
        return self._verify_progress_done

    @Property(int, notify=verifyProgressChanged)
    def verifyProgressTotal(self):
        return self._verify_progress_total

    @Property(str, notify=verifyResultReady)
    def verifyResultText(self):
        return self._verify_result_text

    @Property(bool, notify=verifyFinished)
    def verifyResultOk(self):
        return self._verify_ok

    @Slot(str)
    def verifyVersion(self, version_id: str):
        if not version_id:
            self.verifyResultReady.emit("[完整性校验] 未选择版本")
            return
        if self._verify_running:
            self.logMessage.emit("[完整性校验] 校验正在进行中，请稍后")
            return

        self._verify_running = True
        self._verify_progress_done = 0
        self._verify_progress_total = 0
        self._verify_result_text = "正在准备校验..."
        self._verify_ok = False
        self.verifyProgressChanged.emit(0, 0)
        self.verifyResultReady.emit(self._verify_result_text)
        self.verifyFinished.emit(False)

        base = self._game_dir if hasattr(self, '_game_dir') else MINECRAFT_DIR

        def _do_verify():
            try:
                import json
                import hashlib

                vjson_path = os.path.join(base, "versions", version_id, f"{version_id}.json")
                if not os.path.exists(vjson_path):
                    self._verify_result_text = f"版本 {version_id} 不存在（缺少版本 JSON）"
                    self._verify_running = False
                    self.verifyResultReady.emit(self._verify_result_text)
                    self.verifyFinished.emit(False)
                    return

                with open(vjson_path, "r", encoding="utf-8") as f:
                    version_json = json.load(f)

                # Collect all expected items
                items = []  # (display_name, file_path, expected_sha1)

                # Jar
                jar_path = os.path.join(base, "versions", version_id, f"{version_id}.jar")
                jar_sha = version_json.get("downloads", {}).get("client", {}).get("sha1")
                items.append((f"{version_id}.jar", jar_path, jar_sha))

                # Libraries
                for lib in version_json.get("libraries", []):
                    rules = lib.get("rules", [])
                    if rules:
                        ok = False
                        for r in rules:
                            os_name = r.get("os", {}).get("name", "")
                            action = r.get("action", "allow")
                            if os_name and "windows" in os_name.lower():
                                ok = (action == "allow")
                                break
                        if not ok:
                            continue

                    for art_key in ["artifact"]:
                        a = lib.get("downloads", {}).get(art_key)
                        if a:
                            p = a.get("path", "")
                            items.append((f"lib/{p}", os.path.join(base, "libraries", p), a.get("sha1")))

                    for cls_name, art in lib.get("downloads", {}).get("classifiers", {}).items():
                        if "natives-windows" in cls_name:
                            p = art.get("path", "")
                            items.append((f"lib/{p}", os.path.join(base, "libraries", p), art.get("sha1")))

                # Asset index
                asset_idx = version_json.get("assetIndex", {})
                idx_path = os.path.join(base, "assets", "indexes", f"{asset_idx.get('id', 'legacy')}.json")
                if os.path.exists(idx_path):
                    with open(idx_path, "r", encoding="utf-8") as f:
                        idx_data = json.load(f)
                    for name, obj in idx_data.get("objects", {}).items():
                        sha = obj["hash"]
                        pf = sha[:2]
                        items.append((f"asset/{name}", os.path.join(base, "assets", "objects", pf, sha), sha))

                total = len(items)
                self._verify_progress_total = total
                done = 0
                missing = []

                for name, fpath, sha1 in items:
                    if not os.path.exists(fpath):
                        missing.append(f"缺失: {name}")
                    elif sha1:
                        try:
                            h = hashlib.sha1()
                            with open(fpath, "rb") as fh:
                                while True:
                                    chunk = fh.read(65536)
                                    if not chunk:
                                        break
                                    h.update(chunk)
                            if h.hexdigest() != sha1:
                                missing.append(f"损坏: {name}")
                        except Exception:
                            missing.append(f"读取失败: {name}")

                    done += 1
                    if done % 100 == 0 or done == total:
                        self._verify_progress_done = done
                        self.verifyProgressChanged.emit(done, total)
                        self._verify_result_text = f"校验中... {done}/{total}"
                        self.verifyResultReady.emit(self._verify_result_text)

                self._verify_running = False
                if missing:
                    result = f"校验完成：{len(missing)} 个文件有问题\n"
                    result += "\n".join(missing[:20])
                    if len(missing) > 20:
                        result += f"\n... 共 {len(missing)} 个问题"
                    self._verify_result_text = result
                    self._verify_ok = False
                    self.verifyResultReady.emit(result)
                    self.verifyFinished.emit(False)
                    self.logMessage.emit(f"[完整性校验] {version_id} 发现 {len(missing)} 个问题")
                else:
                    self._verify_result_text = f"✅ {version_id} 全部文件通过校验！"
                    self._verify_ok = True
                    self.verifyResultReady.emit(self._verify_result_text)
                    self.verifyFinished.emit(True)
                    self.logMessage.emit(f"[完整性校验] {version_id} 全部通过")

            except Exception as e:
                self._verify_running = False
                self._verify_ok = False
                self._verify_result_text = f"校验失败: {str(e)}"
                self.verifyResultReady.emit(self._verify_result_text)
                self.verifyFinished.emit(False)
                self.logMessage.emit(f"[完整性校验] 出错: {e}")

        threading.Thread(target=_do_verify, daemon=True).start()

    @Slot(str)
    def repairVersion(self, version_id: str):
        """Re-install version to fix missing/corrupt files"""
        self.logMessage.emit(f"[修复] 正在修复 {version_id}...")
        install_exists = hasattr(self, 'installVersion')
        if not install_exists:
            self.logMessage.emit("[修复] 安装模块不可用")
            return
        # Reinstall — same as fresh install but keeps config/mods/saves
        thread = threading.Thread(target=self.installVersion, args=(version_id,), daemon=True)
        thread.start()
        self.logMessage.emit(f"[修复] {version_id} 修复任务已启动")

    @Slot(str)
    def cloneVersion(self, version_id: str):
        """Clone a version to a new name (version_id_copy)"""
        import shutil
        base = self._game_dir if hasattr(self, '_game_dir') and self._game_dir else MINECRAFT_DIR
        src = os.path.join(base, "versions", version_id)
        dst = os.path.join(base, "versions", version_id + "_copy")
        if os.path.isdir(src) and not os.path.exists(dst):
            shutil.copytree(src, dst)
            # Rename json
            json_src = os.path.join(dst, f"{version_id}.json")
            json_dst = os.path.join(dst, f"{version_id}_copy.json")
            if os.path.isfile(json_src):
                os.rename(json_src, json_dst)
            self.refreshInstalled()
            self.installedVersionsChanged.emit()
            self._scan_version_details()
            self.versionDetailsChanged.emit()
            self.logMessage.emit(f"已克隆版本: {version_id} → {version_id}_copy")

    @Slot(str)
    def renameVersion(self, version_id: str):
        """Prompt rename for a version"""
        from PySide6.QtWidgets import QInputDialog
        new_name, ok = QInputDialog.getText(None, "重命名版本", f"{version_id} 的新名称:")
        if ok and new_name and new_name != version_id:
            base = self._game_dir if hasattr(self, '_game_dir') and self._game_dir else MINECRAFT_DIR
            src = os.path.join(base, "versions", version_id)
            dst = os.path.join(base, "versions", new_name)
            if not os.path.exists(dst):
                os.rename(src, dst)
                # Rename json
                json_src = os.path.join(dst, f"{version_id}.json")
                json_dst = os.path.join(dst, f"{new_name}.json")
                if os.path.isfile(json_src):
                    os.rename(json_src, json_dst)
                if self._selected_version == version_id:
                    self._selected_version = new_name
                self.refreshInstalled()
                self.installedVersionsChanged.emit()
                self._scan_version_details()
                self.versionDetailsChanged.emit()
                self.logMessage.emit(f"已重命名: {version_id} → {new_name}")

    @Slot(str)
    def migrateVersion(self, version_id: str):
        """Move version to a different game directory"""
        from PySide6.QtWidgets import QFileDialog, QMessageBox
        target_dir = QFileDialog.getExistingDirectory(None, f"选择 {version_id} 的目标目录",
            os.path.join(MINECRAFT_DIR, "versions"))
        if not target_dir:
            return
        base = self._game_dir if hasattr(self, '_game_dir') and self._game_dir else MINECRAFT_DIR
        src = os.path.join(base, "versions", version_id)
        dst = os.path.join(target_dir, "versions", version_id)
        if not os.path.isdir(src):
            self.logMessage.emit(f"[迁移] 未找到源版本: {src}")
            return
        if os.path.exists(dst):
            self.logMessage.emit(f"[迁移] 目标已存在: {dst}")
            return
        import shutil
        try:
            shutil.move(src, dst)
            self.refreshInstalled()
            self.installedVersionsChanged.emit()
            self._scan_version_details()
            self.versionDetailsChanged.emit()
            self.logMessage.emit(f"[迁移] {version_id} 已移至 {target_dir}")
        except Exception as e:
            self.logMessage.emit(f"[迁移] 失败: {e}")

    @Slot()
    def configureAuthlib(self):
        self.logMessage.emit("[第三方登录] AuthLib-Injector 配置功能将在后续版本中完善")

    # ═══ System memory info ═══

    @Property(str, notify=memorySettingsChanged)
    def systemMemoryInfo(self):
        try:
            mem = psutil.virtual_memory()
            total_gb = mem.total / (1024**3)
            avail_gb = mem.available / (1024**3)
            return f"总计 {total_gb:.1f} GB  |  可用 {avail_gb:.1f} GB"
        except Exception:
            return "无法获取"

    @Property(bool, notify=memorySettingsChanged)
    def autoMemoryEnabled(self):
        return getattr(self, '_auto_memory', True)

    # ═══ 版本详情 ═══
    _version_details: list = []
    _version_detail_dir: str = ""
    _cached_lib_asset_size: int = None
    _cached_lib_asset_base: str = ""

    versionDetailsChanged = Signal()

    @Property("QVariantList", notify=versionDetailsChanged)
    def versionDetails(self):
        return self._version_details

    def _scan_version_details(self):
        """Scan all installed versions for metadata (loader, mods, size, etc)"""
        base = self._game_dir if hasattr(self, '_game_dir') and self._game_dir else MINECRAFT_DIR
        versions_dir = os.path.join(base, "versions")
        mods_base = os.path.join(base, "mods")

        if not os.path.exists(versions_dir):
            self._version_details = []
            self._version_detail_dir = ""
            return

        global_mods = {}
        if os.path.isdir(mods_base):
            for f in os.listdir(mods_base):
                if f.endswith('.jar'):
                    global_mods[f] = True

        details = []
        for name in sorted(os.listdir(versions_dir), reverse=True):
            ver_dir = os.path.join(versions_dir, name)
            json_file = os.path.join(ver_dir, f"{name}.json")
            if not (os.path.isdir(ver_dir) and os.path.exists(json_file)):
                continue

            # Loader detection
            loader_type = "原版"
            loader_version = ""
            for candidate in [
                ("forge", "Forge"), ("fabric", "Fabric"),
                ("neoforge", "NeoForge"), ("quilt", "Quilt")
            ]:
                for fname in os.listdir(ver_dir):
                    if fname.endswith('.jar') and candidate[0] in fname.lower():
                        loader_type = candidate[1]
                        # Try extract version from filename
                        import re
                        m = re.search(r'(\d+\.\d+\.\d+)', fname)
                        if m:
                            loader_version = m.group(1)
                        break
                if loader_type != "原版":
                    break

            # Also check mods folder for loader mods
            if loader_type == "原版":
                ver_mods_dir = os.path.join(ver_dir, "mods")
                mods_check = [
                    (mods_base, True), (ver_mods_dir, True)
                ]
                for md, _ in mods_check:
                    if not os.path.isdir(md):
                        continue
                    for f in os.listdir(md):
                        fl = f.lower()
                        if "fabric" in fl:
                            loader_type = "Fabric"
                            break
                        elif "neoforge" in fl:
                            loader_type = "NeoForge"
                            break
                        elif "forge" in fl:
                            loader_type = "Forge"
                            break
                    if loader_type != "原版":
                        break

            # Version type tag
            vtype = "release"
            vl = name.lower()
            if any(k in vl for k in ["pre", "rc", "w"]):
                vtype = "snapshot"
            elif any(k in vl for k in ["alpha", "beta", "inf", "rd", "classic"]):
                vtype = "old"
            elif "forge" in vl or "fabric" in vl or "mod" in vl:
                vtype = "modded"

            # Size — version dir only (shared libs/assets shown separately)
            total_size = 0
            for dirpath, dirnames, filenames in os.walk(ver_dir):
                for fn in filenames:
                    fp = os.path.join(dirpath, fn)
                    try:
                        total_size += os.path.getsize(fp)
                    except OSError:
                        pass

            # Shared libraries + assets size (cache once per scan)
            if self._cached_lib_asset_size is None or self._cached_lib_asset_base != base:
                self._cached_lib_asset_size = 0
                self._cached_lib_asset_base = base
                for scan_dir in ["libraries", os.path.join("assets", "objects")]:
                    sd = os.path.join(base, scan_dir)
                    if os.path.isdir(sd):
                        for dirpath, _, filenames in os.walk(sd):
                            for fn in filenames:
                                try:
                                    self._cached_lib_asset_size += os.path.getsize(os.path.join(dirpath, fn))
                                except OSError:
                                    pass
            # Note: lib_asset_size stored separately for total disk usage display
            lib_asset_size = (self._cached_lib_asset_size or 0)

            # Mod count (check shared mods + version-specific mods + isolated game dir mods)
            mod_count = 0
            for md in [mods_base, os.path.join(ver_dir, "mods"),
                       os.path.join(get_version_game_dir(name), "mods")]:
                if os.path.isdir(md):
                    mod_count += sum(1 for f in os.listdir(md) if f.endswith('.jar'))

            details.append({
                "id": name,
                "loaderType": loader_type,
                "loaderVersion": loader_version,
                "versionType": vtype,
                "sizeBytes": total_size,
                "sizeDisplay": self._format_size(total_size),
                "modCount": mod_count,
            })

        self._version_details = details
        self._version_detail_dir = base

    @staticmethod
    def _format_size(size_bytes: int) -> str:
        if size_bytes < 1024:
            return f"{size_bytes} B"
        elif size_bytes < 1048576:
            return f"{size_bytes / 1024:.1f} KB"
        elif size_bytes < 1073741824:
            return f"{size_bytes / 1048576:.1f} MB"
        else:
            return f"{size_bytes / 1073741824:.2f} GB"

    @Slot()
    def refreshVersionDetails(self):
        self._scan_version_details()
        self.versionDetailsChanged.emit()

    @Property("QVariantMap", notify=versionDetailsChanged)
    def currentVersionSummary(self):
        """Return {sizeDisplay, modCount, loaderType, versionType} for current version"""
        vid = self._selected_version or ""
        for d in self._version_details:
            if d.get("id") == vid:
                return {
                    "sizeDisplay": d.get("sizeDisplay", "-"),
                    "modCount": d.get("modCount", 0),
                    "loaderType": d.get("loaderType", "原版"),
                    "versionType": d.get("versionType", "release"),
                    "isolated": self.isVersionIsolated(vid),
                }
        return {"sizeDisplay": "-", "modCount": 0, "loaderType": "原版", "versionType": "release", "isolated": False}

    def _get_data_dir(self, subdir: str = "") -> str:
        """获取当前版本的数据目录（存档/资源包等），跟随版本隔离"""
        base = self._game_dir if hasattr(self, '_game_dir') and self._game_dir else MINECRAFT_DIR
        vid = self._selected_version if hasattr(self, '_selected_version') and self._selected_version else ""
        if vid and is_isolation_enabled():
            base = get_version_game_dir(vid)
        return os.path.join(base, subdir) if subdir else base

    @Slot(result="QVariantList")
    def listSaves(self):
        """Scan saves folder and return [{name, size, sizeDisplay, mtime}]"""
        saves_dir = self._get_data_dir("saves")
        saves = []
        if not os.path.isdir(saves_dir):
            return saves
        for sname in sorted(os.listdir(saves_dir)):
            spath = os.path.join(saves_dir, sname)
            if not os.path.isdir(spath):
                continue
            # Calculate size
            total = 0
            try:
                for dirpath, _, filenames in os.walk(spath):
                    for fn in filenames:
                        try:
                            total += os.path.getsize(os.path.join(dirpath, fn))
                        except OSError:
                            pass
            except OSError:
                pass
            try:
                st = os.stat(spath)
                saves.append({
                    "name": sname,
                    "size": total,
                    "sizeDisplay": self._format_size(total),
                    "mtime": st.st_mtime,
                })
            except OSError:
                pass
        return saves

    @Slot(str)
    def deleteSave(self, save_name: str):
        """Delete a world save"""
        spath = os.path.join(self._get_data_dir("saves"), save_name)
        if os.path.isdir(spath):
            import shutil
            shutil.rmtree(spath, ignore_errors=True)
            self.logMessage.emit(f"已删除存档: {save_name}")
        else:
            self.logMessage.emit(f"未找到存档: {save_name}")

    @Slot(result="QVariantList")
    def listResourcePacks(self):
        """Scan resourcepacks folder, return [{name, size, sizeDisplay, mtime}]"""
        rp_dir = self._get_data_dir("resourcepacks")
        packs = []
        if not os.path.isdir(rp_dir):
            return packs
        for fname in sorted(os.listdir(rp_dir)):
            fpath = os.path.join(rp_dir, fname)
            if not fname.endswith('.zip') and not os.path.isdir(fpath):
                continue
            try:
                st = os.stat(fpath)
                size = st.st_size if os.path.isfile(fpath) else 0
                if os.path.isdir(fpath):
                    for dirpath, _, filenames in os.walk(fpath):
                        for fn in filenames:
                            try:
                                size += os.path.getsize(os.path.join(dirpath, fn))
                            except OSError:
                                pass
                packs.append({
                    "name": fname,
                    "size": size,
                    "sizeDisplay": self._format_size(size),
                    "mtime": st.st_mtime,
                    "isDir": os.path.isdir(fpath),
                })
            except OSError:
                pass
        return packs

    @Slot(str)
    def deleteResourcePack(self, pack_name: str):
        """Delete a resource pack"""
        base = self._game_dir if hasattr(self, '_game_dir') and self._game_dir else MINECRAFT_DIR
        fpath = os.path.join(base, "resourcepacks", pack_name)
        if os.path.isdir(fpath):
            import shutil
            shutil.rmtree(fpath, ignore_errors=True)
            self.logMessage.emit(f"已删除资源包: {pack_name}")
        elif os.path.isfile(fpath):
            os.remove(fpath)
            self.logMessage.emit(f"已删除资源包: {pack_name}")
        else:
            self.logMessage.emit(f"未找到资源包: {pack_name}")

    @Slot()
    def openResourcePacksFolder(self):
        base = self._game_dir if hasattr(self, '_game_dir') and self._game_dir else MINECRAFT_DIR
        rp = os.path.join(base, "resourcepacks")
        if os.path.isdir(rp):
            os.startfile(rp)
        else:
            os.startfile(base)

    @Slot(str)
    def openModsFolder(self, version_id: str = ""):
        base = self._game_dir if hasattr(self, '_game_dir') and self._game_dir else MINECRAFT_DIR
        if version_id:
            vmods = os.path.join(base, "versions", version_id, "mods")
            if os.path.isdir(vmods):
                os.startfile(vmods)
                return
        gmods = os.path.join(base, "mods")
        if os.path.isdir(gmods):
            os.startfile(gmods)
        else:
            os.startfile(base)

    @Slot(str)
    def openConfigFolder(self, version_id: str = ""):
        base = self._game_dir if hasattr(self, '_game_dir') and self._game_dir else MINECRAFT_DIR
        if version_id:
            vcfg = os.path.join(base, "versions", version_id, "config")
            if os.path.isdir(vcfg):
                os.startfile(vcfg)
                return
        gcfg = os.path.join(base, "config")
        if os.path.isdir(gcfg):
            os.startfile(gcfg)
        else:
            os.startfile(base)

    @Slot(str)
    @Slot(result=bool)
    def openLogsFolder(self) -> bool:
        base = self._game_dir if hasattr(self, '_game_dir') and self._game_dir else MINECRAFT_DIR
        glogs = os.path.join(base, "logs")
        if os.path.isdir(glogs):
            os.startfile(glogs)
            return True
        return False

    @Slot(str)
    def openShaderPacksFolder(self):
        base = self._game_dir if hasattr(self, '_game_dir') and self._game_dir else MINECRAFT_DIR
        sp = os.path.join(base, "shaderpacks")
        if os.path.isdir(sp):
            os.startfile(sp)
        else:
            os.startfile(base)

    @Slot(result=bool)
    def openLatestLog(self) -> bool:
        """Open latest.log, return True if found"""
        base = self._game_dir if hasattr(self, '_game_dir') and self._game_dir else MINECRAFT_DIR
        log = os.path.join(base, "logs", "latest.log")
        if os.path.isfile(log):
            os.startfile(log)
            return True
        return False

    @Slot(result=bool)
    def openCrashLog(self) -> bool:
        """Open most recent crash report, return True if found"""
        base = self._game_dir if hasattr(self, '_game_dir') and self._game_dir else MINECRAFT_DIR
        crash_dir = os.path.join(base, "crash-reports")
        if os.path.isdir(crash_dir):
            files = sorted([f for f in os.listdir(crash_dir) if f.endswith('.txt')],
                           key=lambda x: os.path.getmtime(os.path.join(crash_dir, x)), reverse=True)
            if files:
                os.startfile(os.path.join(crash_dir, files[0]))
                return True
        return False

    @Slot(result="QVariantList")
    def listMods(self):
        """Scan mods folder and return [{name, size, sizeDisplay, mtime, path}]"""
        base = self._get_data_dir()
        mods = []
        for search_dir in [os.path.join(base, "mods"),
                           os.path.join(getattr(self, '_game_dir', MINECRAFT_DIR), "versions", self._selected_version or "", "mods")]:
            if not os.path.isdir(search_dir):
                continue
            for fname in sorted(os.listdir(search_dir)):
                if not fname.endswith('.jar'):
                    continue
                fpath = os.path.join(search_dir, fname)
                try:
                    st = os.stat(fpath)
                    mods.append({
                        "name": fname,
                        "size": st.st_size,
                        "sizeDisplay": self._format_size(st.st_size),
                        "mtime": st.st_mtime,
                        "path": fpath,
                    })
                except OSError:
                    pass
        return mods

    @Slot(str, str)
    def deleteMod(self, mod_name: str, version_id: str = ""):
        """Delete a mod file by name"""
        base = self._game_dir if hasattr(self, '_game_dir') and self._game_dir else MINECRAFT_DIR
        for search_dir in [os.path.join(base, "mods"),
                           os.path.join(base, "versions", version_id or (self._selected_version or ""), "mods")]:
            fpath = os.path.join(search_dir, mod_name)
            if os.path.isfile(fpath):
                try:
                    os.remove(fpath)
                    self.logMessage.emit(f"已删除 Mod: {mod_name}")
                    return
                except OSError:
                    pass
        self.logMessage.emit(f"未找到 Mod: {mod_name}")

    @Slot(str)
    def copyVersionPath(self, path_type: str):
        """Copy path to clipboard. path_type: 'version' | 'mods' | 'config' | 'logs' | 'root'"""
        import subprocess as _subprocess
        base = self._game_dir if hasattr(self, '_game_dir') and self._game_dir else MINECRAFT_DIR
        v = self._selected_version

        mapping = {
            "version": os.path.join(base, "versions", v) if v else base,
            "mods": os.path.join(base, "mods"),
            "config": os.path.join(base, "config"),
            "logs": os.path.join(base, "logs"),
            "root": base,
        }
        path = mapping.get(path_type, base)
        try:
            _subprocess.run(["cmd", "/c", "echo", path, "|", "clip"], shell=True, check=False)
            self.logMessage.emit(f"已复制路径: {path}")
        except Exception:
            pass

    @Slot(bool)
    def setAutoMemoryEnabled(self, enabled: bool):
        self._auto_memory = enabled
        if enabled:
            try:
                mem = psutil.virtual_memory()
                # Auto: allocate 25% of available RAM as max, half of that as min
                avail_mb = int(mem.available / (1024 * 1024))
                self.setMaxMemory(min(avail_mb // 4, 8192))
                self.setMinMemory(min(avail_mb // 8, 2048))
            except Exception:
                pass
        self.memorySettingsChanged.emit()

    @Slot(str, result=str)
    def versionType(self, version_id: str = "") -> str:
        """Return version loader type: 'vanilla', 'forge', 'fabric', 'neoforge', 'quilt', or 'unknown'"""
        vid = version_id or self._selected_version
        if not vid:
            return "vanilla"
        # Check version details
        for detail in self._version_details:
            if detail.get("id") == vid:
                lt = (detail.get("loaderType") or "").lower()
                if lt in ("forge", "fabric", "neoforge", "quilt"):
                    return lt
                return "vanilla"
        # Fallback: check version.json for mainClass
        ver_dir = os.path.join(MINECRAFT_DIR, "versions", vid)
        ver_json = os.path.join(ver_dir, f"{vid}.json")
        if os.path.isfile(ver_json):
            try:
                import json
                with open(ver_json, "r", encoding="utf-8") as f:
                    data = json.load(f)
                mc = data.get("mainClass", "")
                if "forge" in mc.lower():
                    return "forge"
                if "fabric" in mc.lower():
                    return "fabric"
                if "neoforge" in mc.lower():
                    return "neoforge"
                if "quilt" in mc.lower():
                    return "quilt"
            except Exception:
                pass
        return "vanilla"

    @Slot(str, result=bool)
    def isModdedVersion(self, version_id: str = "") -> bool:
        """Check if version supports mods"""
        t = self.versionType(version_id)
        return t in ("forge", "fabric", "neoforge", "quilt")
