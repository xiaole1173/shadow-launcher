"""
VersionMixin — 版本列表/安装/刷新
供 ShadowBackend 多重继承使用

所有 Property 在 ShadowBackend 主类中声明，
Mixin 只包含初始化逻辑和 Slot 方法。
"""

import threading

from PySide6.QtCore import Slot

from shadow_launcher.core.versions import (
    fetch_version_manifest, get_installed_versions,
    fetch_version_json, VersionManifest, VersionInfo,
    MINECRAFT_DIR,
)
from shadow_launcher.core.downloader import MIRROR_SOURCES, MirrorSource, VersionDownloader


class VersionMixin:
    """版本管理功能"""

    # 属性声明（类型提示）
    _version_manifest: VersionManifest = None
    _version_ids: list = []
    _installed_ids: list = []
    _selected_version: str = ""
    _source_index: int = 0

    # 安装状态
    _installer: VersionDownloader = None
    _install_thread = None
    _installing: bool = False
    _paused: bool = False
    _install_progress: int = 0
    _install_total: int = 0
    _install_file: str = ""
    _install_bytes_dl: int = 0
    _install_bytes_total: int = 0
    _install_phase: str = ""
    _install_version_id: str = ""
    _install_speed: float = 0

    # ═══ 初始化 ═══

    def _init_versions(self):
        # Load version manifest synchronously so it's ready before UI interacts
        def _fetch():
            try:
                self.logMessage.emit("正在获取版本列表...")
                manifest = fetch_version_manifest(use_mirror=True)
                self._version_manifest = manifest
                self._version_ids = [
                    v.id for v in manifest.versions
                    if v.type == "release"
                ]
                self._installed_ids = get_installed_versions()
                self.versionListReady.emit()
                self.installedVersionsChanged.emit()
                self.logMessage.emit(
                    f"版本列表已加载 (latest: {manifest.latest_release})"
                )
            except Exception as e:
                self.logMessage.emit(f"获取版本列表失败: {e}")

        # Run fetch in a thread, but initialize _version_manifest to None
        # The DownloadPage will call refreshVersionList() which triggers this
        import threading
        t = threading.Thread(target=_fetch, daemon=True)
        t.start()
        # Wait up to 5s for manifest to load
        t.join(timeout=5.0)

    # ═══ 版本操作 ═══

    @Slot(str)
    def setSelectedVersion(self, version_id: str):
        self._selected_version = version_id

    @Slot()
    def refreshInstalled(self):
        base = self._game_dir if hasattr(self, '_game_dir') and self._game_dir else MINECRAFT_DIR
        self._installed_ids = get_installed_versions(base)
        self.installedVersionsChanged.emit()
        self._scan_version_details()
        self.versionDetailsChanged.emit()

    # ═══ 安装版本 ═══

    @Slot(str, int)
    def installVersion(self, version_id: str, source_index: int):
        if self._installing:
            return

        if not self._version_manifest:
            self.logMessage.emit("版本列表尚未加载")
            return

        version_info = None
        for v in self._version_manifest.versions:
            if v.id == version_id:
                version_info = v
                break

        if not version_info:
            self.logMessage.emit(f"未找到版本: {version_id}")
            return

        mirror = (
            MIRROR_SOURCES[source_index]
            if 0 <= source_index < len(MIRROR_SOURCES)
            else MIRROR_SOURCES[0]
        )
        version_json = fetch_version_json(version_info)

        self._installing = True
        self._paused = False
        self._install_progress = 0
        self._install_total = 0
        self._install_file = "准备中..."
        self._install_bytes_dl = 0
        self._install_bytes_total = 0
        self._install_phase = "downloading"
        self._install_version_id = version_id
        self._install_speed = 0

        self.logMessage.emit(f"开始安装 {version_id}...")

        def _install():
            try:
                self._installer = VersionDownloader(
                    mirror_source=mirror,
                    minecraft_dir=MINECRAFT_DIR,
                    max_workers=32,
                )
                self._installer.on_progress = self._on_install_progress
                self._installer.on_log = lambda msg: self.logMessage.emit(msg)

                success = self._installer.download_version(version_json, version_id)
                self._installing = False
                self.installFinished.emit(success)

                if success:
                    self.logMessage.emit(f"✅ {version_id} 安装完成")
                    self.refreshInstalled()
                else:
                    self.logMessage.emit(f"❌ {version_id} 安装失败")
            except Exception as e:
                self._installing = False
                self.logMessage.emit(f"安装错误: {e}")
                self.installFinished.emit(False)

        self._install_thread = threading.Thread(target=_install, daemon=True)
        self._install_thread.start()

    def _on_install_progress(self, cf: int, tf: int, db: int, tb: int, name: str):
        import time
        now = time.time()
        elapsed = now - getattr(self, "_last_progress_ts", now)
        if elapsed > 0:
            dl_delta = db - getattr(self, "_last_dl_bytes", db)
            self._install_speed = dl_delta / elapsed if dl_delta > 0 else 0
        self._last_progress_ts = now
        self._last_dl_bytes = db

        self._install_progress = cf
        self._install_total = tf
        self._install_file = name
        self._install_bytes_dl = db
        self._install_bytes_total = tb

        if "校验" in name:
            self._install_phase = "verifying"
        else:
            self._install_phase = "downloading"

        self.installProgressChanged.emit(cf)
        self.installTotalChanged.emit(tf)
        self.installFileProgress.emit(name)
        self.installBytesProgress.emit(db, tb)
        self.installPhaseChanged.emit(self._install_phase)

    @Slot()
    def cancelInstall(self):
        if self._installer:
            self._installer.cancel()
        self._installing = False
        self.logMessage.emit("安装已取消")
