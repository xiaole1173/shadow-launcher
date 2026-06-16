"""
版本管理核心 — 获取 Minecraft 版本列表、版本元数据
数据源：Mojang 官方 + BMCLAPI 镜像
"""

import json
import os
import urllib.request
import urllib.error
from dataclasses import dataclass, field
from typing import Optional

# 版本清单 API
MANIFEST_URL = "https://launchermeta.mojang.com/mc/game/version_manifest.json"
# BMCLAPI 镜像（国内加速）
MANIFEST_MIRROR = "https://bmclapi2.bangbang93.com/mc/game/version_manifest.json"

# 工作目录
MINECRAFT_DIR = os.path.join(os.path.expanduser("~"), ".shadow", "minecraft")


@dataclass
class VersionInfo:
    """版本基本信息"""
    id: str
    type: str  # "release" | "snapshot" | "old_beta" | "old_alpha"
    url: str
    time: str
    release_time: str


@dataclass
class VersionManifest:
    """版本完整清单"""
    latest_release: str = ""
    latest_snapshot: str = ""
    versions: list[VersionInfo] = field(default_factory=list)


def _http_get(url: str, max_retries: int = 2) -> dict:
    """HTTP GET with fallback mirror"""
    errors = []
    for attempt in range(max_retries + 1):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": "ShadowLauncher/0.1"})
            with urllib.request.urlopen(req, timeout=15) as resp:
                return json.loads(resp.read().decode("utf-8"))
        except Exception as e:
            errors.append(str(e))
    raise ConnectionError(f"无法获取 {url}: {errors}")


def fetch_version_manifest(use_mirror: bool = True) -> VersionManifest:
    """
    获取 Minecraft 版本清单
    - 优先使用 BMCLAPI 镜像（国内快），失败则回退到 Mojang 官方
    """
    urls = [MANIFEST_MIRROR, MANIFEST_URL] if use_mirror else [MANIFEST_URL]

    for url in urls:
        try:
            data = _http_get(url)
            manifest = VersionManifest(
                latest_release=data["latest"]["release"],
                latest_snapshot=data["latest"]["snapshot"],
            )
            manifest.versions = [
                VersionInfo(
                    id=v["id"],
                    type=v["type"],
                    url=v["url"],
                    time=v["time"],
                    release_time=v["releaseTime"],
                )
                for v in data["versions"]
            ]
            return manifest
        except Exception:
            continue

    raise ConnectionError("所有数据源均不可用")


def get_release_versions(manifest: VersionManifest) -> list[VersionInfo]:
    """过滤仅正式版"""
    return [v for v in manifest.versions if v.type == "release"]


def get_snapshot_versions(manifest: VersionManifest) -> list[VersionInfo]:
    """过滤仅快照版"""
    return [v for v in manifest.versions if v.type == "snapshot"]


def fetch_version_json(version: VersionInfo) -> dict:
    """
    获取指定版本的完整元数据（含 libraries, assets, mainClass 等）
    """
    url = version.url
    # 尝试 BMCLAPI 镜像
    mirror_url = url.replace("launchermeta.mojang.com", "bmclapi2.bangbang93.com")
    try:
        return _http_get(mirror_url)
    except Exception:
        return _http_get(url)


def ensure_minecraft_dir() -> str:
    """确保 .minecraft 目录存在，返回路径"""
    os.makedirs(os.path.join(MINECRAFT_DIR, "versions"), exist_ok=True)
    os.makedirs(os.path.join(MINECRAFT_DIR, "libraries"), exist_ok=True)
    os.makedirs(os.path.join(MINECRAFT_DIR, "assets"), exist_ok=True)
    return MINECRAFT_DIR


def get_installed_versions(base_dir: str = None) -> list[str]:
    """扫描已安装的版本"""
    versions_dir = os.path.join(base_dir or MINECRAFT_DIR, "versions")
    if not os.path.exists(versions_dir):
        return []

    installed = []
    for name in os.listdir(versions_dir):
        ver_dir = os.path.join(versions_dir, name)
        json_file = os.path.join(ver_dir, f"{name}.json")
        if os.path.isdir(ver_dir) and os.path.exists(json_file):
            installed.append(name)
    return sorted(installed, reverse=True)


def get_version_info(version_id: str) -> dict:
    """获取已安装版本的详细信息（隔离状态等）"""
    json_file = os.path.join(MINECRAFT_DIR, "versions", version_id, f"{version_id}.json")
    if not os.path.exists(json_file):
        return None
    try:
        with open(json_file, "r", encoding="utf-8") as f:
            data = json.load(f)
        return {
            "id": version_id,
            "inheritsFrom": data.get("inheritsFrom"),
            "jar": data.get("jar"),
            "type": data.get("type", "release"),
        }
    except Exception:
        return None


# ═══ 版本隔离 ═══

ISOLATION_CONFIG_FILE = os.path.join(MINECRAFT_DIR, "config", "version_isolation.json")


def _load_isolation_config() -> dict:
    """加载版本隔离配置"""
    if os.path.exists(ISOLATION_CONFIG_FILE):
        try:
            with open(ISOLATION_CONFIG_FILE, "r", encoding="utf-8") as f:
                return json.load(f)
        except Exception:
            pass
    return {"enabled": True, "isolated_versions": []}


def _save_isolation_config(config: dict):
    """保存版本隔离配置"""
    os.makedirs(os.path.dirname(ISOLATION_CONFIG_FILE), exist_ok=True)
    with open(ISOLATION_CONFIG_FILE, "w", encoding="utf-8") as f:
        json.dump(config, f, indent=2, ensure_ascii=False)


def is_isolation_enabled() -> bool:
    """版本隔离是否开启"""
    return _load_isolation_config().get("enabled", True)


def set_isolation_enabled(enabled: bool):
    """设置版本隔离开关"""
    config = _load_isolation_config()
    config["enabled"] = enabled
    _save_isolation_config(config)


def get_isolated_versions() -> list[str]:
    """获取已启用隔离的版本列表"""
    return _load_isolation_config().get("isolated_versions", [])


def migrate_version_to_isolated(version_id: str) -> bool:
    """
    将旧版本迁移到隔离模式：创建独立的 game/ 目录
    返回是否成功
    """
    ver_dir = os.path.join(MINECRAFT_DIR, "versions", version_id)
    game_dir = os.path.join(ver_dir, "game")
    
    if not os.path.exists(os.path.join(ver_dir, f"{version_id}.json")):
        return False
    
    os.makedirs(game_dir, exist_ok=True)
    
    # 标记已隔离
    marker = os.path.join(ver_dir, ".isolated")
    if not os.path.exists(marker):
        with open(marker, "w") as f:
            f.write("isolated")
    
    # 更新配置
    config = _load_isolation_config()
    if version_id not in config.get("isolated_versions", []):
        config.setdefault("isolated_versions", []).append(version_id)
        _save_isolation_config(config)
    
    return True


def is_version_isolated(version_id: str) -> bool:
    """检查某个版本是否已开启隔离"""
    marker = os.path.join(MINECRAFT_DIR, "versions", version_id, ".isolated")
    return os.path.exists(marker)


def get_version_game_dir(version_id: str) -> str:
    """
    获取版本的游戏目录
    - 隔离开启 + 版本已隔离 → versions/{id}/game/
    - 否则 → MINECRAFT_DIR（共享目录）
    """
    if is_isolation_enabled() and is_version_isolated(version_id):
        return os.path.join(MINECRAFT_DIR, "versions", version_id, "game")
    return MINECRAFT_DIR
