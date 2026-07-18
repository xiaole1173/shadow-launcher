"""
ResourceMixin — Mod/光影/资源包下载管理
供 ShadowBackend 多重继承使用
"""

import threading
from PySide6.QtCore import Slot, Signal

from shadow_launcher.core.mod_manager import (
    get_popular_mods, get_shader_list,
    ModDownloadManager, search_modrinth,
)


class ResourceMixin:
    """Mod/光影等资源管理"""

    # 资源下载状态（在主 ShadowBackend 声明 Property）
    _resource_dl_progress: int = 0
    _resource_dl_total: int = 0
    _resource_dl_file: str = ""
    _resource_downloading: bool = False

    _mod_dl: ModDownloadManager = None

    resourceDownloadProgress = Signal(int, int, str)
    resourceDownloadDone = Signal(bool)

    @Slot(str, result="QVariantList")
    def getPopularMods(self, loader: str) -> list:
        """获取推荐 Mod 列表"""
        mods = get_popular_mods(loader)
        result = []
        for slug, info in mods.items():
            result.append({
                "slug": slug,
                "title": info["title"],
                "desc": info["desc"],
                "icon": info["icon"],
                "category": info.get("category", ""),
                "downloads": 0,
            })
        self.logMessage.emit(f"加载 {len(result)} 个 {loader.upper()} 推荐Mod")
        return result

    @Slot(result="QVariantList")
    def getShaderList(self) -> list:
        """获取光影列表"""
        shaders = get_shader_list()
        result = []
        for slug, info in shaders.items():
            result.append({
                "slug": slug,
                "title": info["title"],
                "desc": info["desc"],
                "icon": info["icon"],
                "downloads": 0,
            })
        self.logMessage.emit(f"加载 {len(result)} 个光影包")
        return result

    @Slot(str, str)
    def downloadMod(self, slug: str, loader: str):
        """下载指定 Mod（在线查找最新版本）"""
        self._resource_downloading = True
        self.logMessage.emit(f"🔍 正在查找 {slug} ({loader}) 的最新版本...")

        def _dl():
            try:
                # 先搜 Modrinth 找 slug 对应的 project
                self._mod_dl = ModDownloadManager(
                    minecraft_dir=getattr(self, '_minecraft_dir_override', None)
                )
                self._mod_dl.on_log = lambda msg: self.logMessage.emit(msg)
                self._mod_dl.on_progress = self._on_resource_progress

                # 尝试在线下载（搜最新版本）
                # 用 search 拿到准确 slug，然后下载
                result = search_modrinth(query=slug, limit=1)
                if result.mods:
                    exact_slug = result.mods[0].slug
                else:
                    exact_slug = slug

                success = self._mod_dl.download_mod(
                    slug=exact_slug,
                    game_version="1.20.1",  # 默认版本，后续可配置
                    loader=loader,
                    target_dir="mods",
                )

                self._resource_downloading = False
                self.resourceDownloadDone.emit(success)
                if success:
                    self.logMessage.emit(f"✅ {slug} 下载完成 → mods/")
                else:
                    self.logMessage.emit(f"❌ {slug} 下载失败，请检查版本兼容性")

            except Exception as e:
                self._resource_downloading = False
                self.logMessage.emit(f"下载异常: {e}")
                self.resourceDownloadDone.emit(False)

        threading.Thread(target=_dl, daemon=True).start()

    @Slot(str)
    def downloadShader(self, slug: str):
        """下载光影包"""
        self._resource_downloading = True
        self.logMessage.emit(f"🔍 正在查找 {slug} 光影包...")

        def _dl():
            try:
                self._mod_dl = ModDownloadManager(
                    minecraft_dir=getattr(self, '_minecraft_dir_override', None)
                )
                self._mod_dl.on_log = lambda msg: self.logMessage.emit(msg)
                self._mod_dl.on_progress = self._on_resource_progress

                success = self._mod_dl.download_mod(
                    slug=slug,
                    game_version="1.20.1",
                    loader="iris",  # 光影一般用 Iris
                    target_dir="shaderpacks",
                )

                self._resource_downloading = False
                self.resourceDownloadDone.emit(success)
                if success:
                    self.logMessage.emit(f"✅ {slug} 下载完成 → shaderpacks/")
                else:
                    self.logMessage.emit(f"❌ {slug} 下载失败")

            except Exception as e:
                self._resource_downloading = False
                self.logMessage.emit(f"下载异常: {e}")
                self.resourceDownloadDone.emit(False)

        threading.Thread(target=_dl, daemon=True).start()

    @Slot(str, str)
    def searchMods(self, query: str, loader: str):
        """在线搜索 Mod"""
        self.logMessage.emit(f"🔍 正在搜索: {query} ({loader})...")

        def _search():
            try:
                result = search_modrinth(query=query, limit=20, sort="relevance")
                mods_list = []
                for m in result.mods:
                    mods_list.append({
                        "slug": m.slug,
                        "title": m.title,
                        "desc": m.description[:100] if m.description else "",
                        "icon": m.icon_url or "📦",
                        "downloads": m.downloads,
                    })
                self.logMessage.emit(f"找到 {len(mods_list)} 个结果")
                # 触发前端的更新（需要通过信号传递）
                self.searchResultsReady.emit(mods_list)
            except Exception as e:
                self.logMessage.emit(f"搜索失败: {e}")

        threading.Thread(target=_search, daemon=True).start()

    def _on_resource_progress(self, cf: int, tf: int, db: int, tb: int, name: str):
        self._resource_dl_progress = cf
        self._resource_dl_total = tf
        self._resource_dl_file = name
        self.resourceDownloadProgress.emit(cf, tf, name)
