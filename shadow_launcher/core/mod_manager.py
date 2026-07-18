"""
Mod 资源管理
- Modrinth API 集成（搜索 + 下载）
- CurseForge 兼容下载路径
- 通用文件下载引擎驱动
"""

import os
import json
import urllib.request
import urllib.parse
import threading
from dataclasses import dataclass, field
from typing import Optional, Callable

from .parallel_downloader import ParallelDownloader, DownloadTask


# ═══ 数据模型 ═══

@dataclass
class ModInfo:
    """Mod 信息"""
    slug: str
    title: str
    description: str
    icon_url: str = ""
    author: str = ""
    downloads: int = 0
    categories: list = field(default_factory=list)
    game_versions: list = field(default_factory=list)
    loaders: list = field(default_factory=list)  # fabric, forge, quilt, neoforge
    latest_version: str = ""


@dataclass
class ModFile:
    """Mod 文件"""
    name: str
    url: str
    filename: str
    size: int = 0
    sha1: str = ""
    game_versions: list = field(default_factory=list)
    loaders: list = field(default_factory=list)
    is_primary: bool = False


@dataclass
class ModSearchResult:
    """搜索结果"""
    mods: list[ModInfo]
    total: int
    offset: int
    limit: int


# ═══ Modrinth API ═══

MODRINTH_API = "https://api.modrinth.com/v2"
MODRINTH_HEADERS = {
    "User-Agent": "ShadowLauncher/0.2 (shadow@launcher.dev)",
}

# 中文社区常用镜像（非官方，但下载快）
MODRINTH_MIRRORS = [
    "https://modrinth.thisisxd.top",    # 国内镜像 (Cloudflare-like)
    "https://ghfast.top/https://cdn.modrinth.com",  # GitHub 加速
]


def search_modrinth(
    query: str = "",
    categories: list[str] = None,
    game_versions: list[str] = None,
    loaders: list[str] = None,
    offset: int = 0,
    limit: int = 20,
    sort: str = "downloads",
) -> ModSearchResult:
    """
    搜索 Modrinth Mod

    参数:
        query: 搜索关键词
        categories: 分类过滤 (adventure, cursed, decoration, economy, equipment, food, game-mechanics, library, magic, management, minigame, mobs, optimization, social, storage, technology, transportation, utility, world-generation)
        game_versions: MC 版本过滤 ["1.20.1", "1.21"]
        loaders: 加载器过滤 ["fabric", "forge", "quilt", "neoforge"]
        offset/limit: 分页
        sort: 排序 (relevance, downloads, follows, newest, updated)
    """
    params = {
        "offset": offset,
        "limit": limit,
        "index": sort,
    }
    if query:
        params["query"] = query
    if categories:
        params["facets"] = json.dumps([["categories:" + c for c in categories]])
    if game_versions:
        if "facets" in params:
            facets = json.loads(params["facets"])
            facets.append(["versions:" + v for v in game_versions])
            params["facets"] = json.dumps(facets)
        else:
            params["facets"] = json.dumps([["versions:" + v for v in game_versions]])
    if loaders:
        if "facets" in params:
            facets = json.loads(params["facets"])
            facets.append(["categories:" + l for l in loaders])
            params["facets"] = json.dumps(facets)
        else:
            params["facets"] = json.dumps([["categories:" + l for l in loaders]])

    url = f"{MODRINTH_API}/search?{urllib.parse.urlencode(params)}"

    try:
        req = urllib.request.Request(url, headers=MODRINTH_HEADERS)
        with urllib.request.urlopen(req, timeout=15) as resp:
            data = json.loads(resp.read())

        mods = []
        for hit in data.get("hits", []):
            mods.append(ModInfo(
                slug=hit.get("slug", ""),
                title=hit.get("title", ""),
                description=hit.get("description", ""),
                icon_url=hit.get("icon_url", ""),
                author=hit.get("author", ""),
                downloads=hit.get("downloads", 0),
                categories=hit.get("categories", []),
                game_versions=hit.get("versions", []),
                loaders=hit.get("categories", []),  # Modrinth uses categories for loaders
                latest_version=hit.get("latest_version", ""),
            ))

        return ModSearchResult(
            mods=mods,
            total=data.get("total_hits", 0),
            offset=data.get("offset", 0),
            limit=data.get("limit", limit),
        )
    except Exception as e:
        return ModSearchResult(mods=[], total=0, offset=0, limit=limit)


def get_mod_versions(slug: str, loaders: list[str] = None, game_versions: list[str] = None) -> list[ModFile]:
    """
    获取 Mod 的所有文件版本
    """
    url = f"{MODRINTH_API}/project/{slug}/version"
    params = {}
    if loaders:
        params["loaders"] = json.dumps(loaders)
    if game_versions:
        params["game_versions"] = json.dumps(game_versions)
    if params:
        url += "?" + urllib.parse.urlencode(params)

    try:
        req = urllib.request.Request(url, headers=MODRINTH_HEADERS)
        with urllib.request.urlopen(req, timeout=15) as resp:
            data = json.loads(resp.read())

        files = []
        for ver in data:
            for f in ver.get("files", []):
                files.append(ModFile(
                    name=f"{ver.get('name', '')} ({ver.get('version_number', '')})",
                    url=f.get("url", ""),
                    filename=f.get("filename", ""),
                    size=f.get("size", 0),
                    sha1=f.get("hashes", {}).get("sha1", ""),
                    game_versions=ver.get("game_versions", []),
                    loaders=ver.get("loaders", []),
                    is_primary=f.get("primary", False),
                ))
        return files
    except Exception:
        return []


# ═══ Mod 下载管理器 ═══

class ModDownloadManager:
    """
    Mod 下载管理器

    用法:
        mgr = ModDownloadManager(minecraft_dir="...")
        mgr.on_progress = lambda cf, tf, db, tb, name: ...
        mgr.on_log = lambda msg: ...

        mgr.download_mod("sodium", "1.20.1", "fabric", "mods")

    特性:
        - Modrinth API 搜索 + 下载
        - 多镜像并行加速
        - SHA1 校验
        - 自动放到 mods 目录
    """

    def __init__(self, minecraft_dir: str = None, max_workers: int = 16):
        self.minecraft_dir = minecraft_dir or os.path.join(
            os.path.expanduser("~"), ".shadow", "minecraft"
        )
        self.max_workers = max_workers
        self._dl: ParallelDownloader = None

        self.on_progress: Optional[Callable] = None
        self.on_log: Optional[Callable] = None
        self.on_file_done: Optional[Callable] = None
        self.on_finished: Optional[Callable] = None

    def download_mod(
        self,
        slug: str,
        game_version: str,
        loader: str,
        target_dir: str = "mods",
    ) -> bool:
        """
        下载指定 Mod 的最新版本

        Args:
            slug: Mod slug (如 "sodium", "iris")
            game_version: MC 版本 (如 "1.20.1")
            loader: 加载器 ("fabric", "forge", "quilt", "neoforge")
            target_dir: 目标子目录 ("mods", "shaderpacks", "resourcepacks")
        """
        # 获取文件列表
        if self.on_log:
            self.on_log(f"正在获取 {slug} 的文件列表...")

        files = get_mod_versions(slug, loaders=[loader], game_versions=[game_version])
        if not files:
            if self.on_log:
                self.on_log(f"❌ 未找到 {slug} 的 {loader} {game_version} 版本")
            return False

        # 优先取 primary 文件
        primary = [f for f in files if f.is_primary]
        chosen = primary[0] if primary else files[0]

        if self.on_log:
            self.on_log(f"找到 {len(files)} 个文件，选择: {chosen.filename} ({chosen._format_size(chosen.size)})")

        # 构建下载任务
        dest_dir = os.path.join(self.minecraft_dir, target_dir)
        os.makedirs(dest_dir, exist_ok=True)
        dest = os.path.join(dest_dir, chosen.filename)

        # 构建镜像 URL
        mirrors = []
        if chosen.url:
            for mirror_base in MODRINTH_MIRRORS:
                path = chosen.url.replace("https://cdn.modrinth.com", "")
                mirrors.append(mirror_base + path)

        task = DownloadTask(
            url=chosen.url,
            dest=dest,
            sha1=chosen.sha1,
            size_hint=chosen.size,
            mirrors=mirrors,
        )

        dl = ParallelDownloader(max_workers=self.max_workers)
        dl.on_progress = self.on_progress
        dl.on_log = self.on_log
        dl.on_file_done = self.on_file_done

        result = dl.download_files([task])

        if self.on_finished:
            self.on_finished(result)

        return result

    def download_files(self, tasks: list[DownloadTask]) -> bool:
        """直接下载文件列表"""
        self._dl = ParallelDownloader(max_workers=self.max_workers)
        self._dl.on_progress = self.on_progress
        self._dl.on_log = self.on_log
        self._dl.on_file_done = self.on_file_done

        result = self._dl.download_files(tasks)

        if self.on_finished:
            self.on_finished(result)

        return result

    def cancel(self):
        if self._dl:
            self._dl.cancel()

    def pause(self):
        if self._dl:
            self._dl.pause()

    def resume(self):
        if self._dl:
            self._dl.resume()


# ═══ 预定义 Mod 数据（离线可用，快速推荐） ═══

POPULAR_MODS = {
    "fabric": {
        "sodium": {"title": "Sodium", "desc": "极致性能优化，FPS提升巨大", "icon": "⚡", "category": "optimization"},
        "iris": {"title": "Iris Shaders", "desc": "光影加载器，Fabric首选", "icon": "✨", "category": "shader"},
        "lithium": {"title": "Lithium", "desc": "服务端性能优化，无兼容性问题", "icon": "🚀", "category": "optimization"},
        "phosphor": {"title": "Phosphor", "desc": "光照引擎优化（已替代为Starlight）", "icon": "💡", "category": "optimization"},
        "starlight": {"title": "Starlight", "desc": "新一代光照引擎，性能飞升", "icon": "🌟", "category": "optimization"},
        "ferritecore": {"title": "FerriteCore", "desc": "大幅降低内存占用", "icon": "🧠", "category": "optimization"},
        "modmenu": {"title": "Mod Menu", "desc": "Fabric必备，Mod列表查看", "icon": "📋", "category": "utility"},
        "cloth-config": {"title": "Cloth Config", "desc": "配置API，很多Mod依赖它", "icon": "⚙️", "category": "library"},
        "sodium-extra": {"title": "Sodium Extra", "desc": "Sodium功能扩展", "icon": "🔧", "category": "optimization"},
        "reeses-sodium-options": {"title": "Reese's Sodium Options", "desc": "Sodium选项增强", "icon": "🎛️", "category": "optimization"},
        "continuity": {"title": "Continuity", "desc": "连接纹理（类似OptiFine的玻璃效果）", "icon": "🧱", "category": "decoration"},
        "lambda": {"title": "LambdaBetterGrass", "desc": "更好的草地渲染", "icon": "🌿", "category": "decoration"},
        "entityculling": {"title": "Entity Culling", "desc": "实体渲染优化", "icon": "👁️", "category": "optimization"},
        "indium": {"title": "Indium", "desc": "Sodium + Fabric渲染API兼容", "icon": "🔄", "category": "library"},
        "fabric-api": {"title": "Fabric API", "desc": "Fabric核心API，必装", "icon": "🧵", "category": "library"},
        "lambdynamiclights": {"title": "LambDynamicLights", "desc": "动态光源（手持火把发光）", "icon": "🔥", "category": "decoration"},
        "zoomify": {"title": "Zoomify", "desc": "缩放Mod（类似OptiFine的C键）", "icon": "🔍", "category": "utility"},
        "detail-armor-bar": {"title": "Detail Armor Bar", "desc": "详细盔甲耐久显示", "icon": "🛡️", "category": "utility"},
        "xaeros-minimap": {"title": "Xaero's Minimap", "desc": "小地图Mod", "icon": "🗺️", "category": "utility"},
        "journeymap": {"title": "JourneyMap", "desc": "实时地图Mod", "icon": "🗺️", "category": "utility"},
    },
    "forge": {
        "jei": {"title": "JEI", "desc": "物品配方查看器，必备", "icon": "📖", "category": "utility"},
        "optifine": {"title": "OptiFine", "desc": "经典优化+光影Mod", "icon": "⚡", "category": "optimization"},
        "create": {"title": "Create", "desc": "机械动力模组，工业化", "icon": "⚙️", "category": "technology"},
        "botania": {"title": "Botania", "desc": "植物魔法，自然魔法", "icon": "🌺", "category": "magic"},
        "tinkers-construct": {"title": "Tinkers' Construct", "desc": "匠魂，自定义工具", "icon": "🔨", "category": "equipment"},
        "iron-chests": {"title": "Iron Chests", "desc": "更多箱子类型", "icon": "📦", "category": "storage"},
        "applied-energistics-2": {"title": "Applied Energistics 2", "desc": "AE2，数字化存储", "icon": "💾", "category": "storage"},
        "thermal-expansion": {"title": "Thermal Expansion", "desc": "热能扩展，科技模组", "icon": "🔥", "category": "technology"},
        "mekanism": {"title": "Mekanism", "desc": "通用机械，高科技工业", "icon": "⚛️", "category": "technology"},
        "biomes-o-plenty": {"title": "Biomes O' Plenty", "desc": "超多生物群系", "icon": "🌲", "category": "world-generation"},
        "twilight-forest": {"title": "Twilight Forest", "desc": "暮色森林，经典冒险", "icon": "🌳", "category": "adventure"},
        "pams-harvestcraft": {"title": "Pam's HarvestCraft", "desc": "更多农作物和食物", "icon": "🌾", "category": "food"},
        "quark": {"title": "Quark", "desc": "夸克，小而美的功能合集", "icon": "🧩", "category": "utility"},
        "chisel": {"title": "Chisel", "desc": "凿子，建筑方块变体", "icon": "🪨", "category": "decoration"},
        "waystones": {"title": "Waystones", "desc": "传送石碑", "icon": "🏛️", "category": "utility"},
        "gravestone": {"title": "GraveStone Mod", "desc": "死亡不掉落，墓碑留存", "icon": "🪦", "category": "utility"},
        "jade": {"title": "Jade", "desc": "方块/实体信息显示", "icon": "ℹ️", "category": "utility"},
        "the-one-probe": {"title": "The One Probe", "desc": "信息探头，查看方块详情", "icon": "🔬", "category": "utility"},
        "curios": {"title": "Curios API", "desc": "饰品API，多模组依赖", "icon": "💍", "category": "library"},
        "patchouli": {"title": "Patchouli", "desc": "模组说明书API", "icon": "📚", "category": "library"},
    },
}

# 分类中英文对照
CATEGORIES = {
    "adventure": "冒险",
    "cursed": "整活",
    "decoration": "装饰",
    "economy": "经济",
    "equipment": "装备",
    "food": "食物",
    "game-mechanics": "游戏机制",
    "library": "前置库",
    "magic": "魔法",
    "management": "管理",
    "minigame": "小游戏",
    "mobs": "生物",
    "optimization": "优化",
    "social": "社交",
    "storage": "存储",
    "technology": "科技",
    "transportation": "运输",
    "utility": "工具",
    "world-generation": "世界生成",
}

# 光影分类
SHADER_PACKS = {
    "bsl": {"title": "BSL Shaders", "desc": "经典全能光影，性能画质平衡", "icon": "✨"},
    "complementary": {"title": "Complementary Shaders", "desc": "BSL魔改，画面更精美", "icon": "🎨"},
    "complementary-reimagined": {"title": "Complementary Reimagined", "desc": "互补重制版，极致画面", "icon": "🖼️"},
    "sildurs-vibrant": {"title": "Sildur's Vibrant Shaders", "desc": "色彩鲜艳，光影柔和", "icon": "🌈"},
    "sildurs-enhanced-default": {"title": "Sildur's Enhanced Default", "desc": "轻量级，低配机可选", "icon": "💡"},
    "seus": {"title": "SEUS", "desc": "经典光追级光影", "icon": "☀️"},
    "seus-ptgi": {"title": "SEUS PTGI", "desc": "路径追踪光影，画质巅峰", "icon": "🌟"},
    "solas": {"title": "Solas Shader", "desc": "纳斯达克同款 (Derivative)", "icon": "🌅"},
    "rethinking-voxels": {"title": "Rethinking Voxels", "desc": "体素光照，原生风格", "icon": "🔆"},
    "photon": {"title": "Photon Shader", "desc": "新锐光影，画面梦幻", "icon": "💫"},
    "astra": {"title": "AstraLex Shaders", "desc": "BSL分支，特效丰富", "icon": "🌌"},
    "chocapic13": {"title": "Chocapic13's Shaders", "desc": "老牌光影，多配置档", "icon": "🌄"},
    "makeup-ultra-fast": {"title": "MakeUp Ultra Fast", "desc": "超轻量，低配福利", "icon": "⚡"},
    "vanilla-plus": {"title": "Vanilla Plus", "desc": "原版风格增强", "icon": "🌿"},
    "bliss": {"title": "Bliss Shader", "desc": "自然写实风格", "icon": "🏞️"},
}


def get_popular_mods(loader: str) -> dict:
    """获取某加载器的推荐 Mod 列表"""
    return POPULAR_MODS.get(loader, {})


def get_shader_list() -> dict:
    """获取光影列表"""
    return SHADER_PACKS
