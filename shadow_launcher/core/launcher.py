"""
启动管理 — JVM 参数构建、进程启动
"""

import os
import subprocess
import sys
import zipfile
import shutil
from pathlib import Path

MINECRAFT_DIR = os.path.join(os.path.expanduser("~"), ".shadow", "minecraft")

# 版本隔离相关
try:
    from shadow_launcher.core.versions import get_version_game_dir, is_isolation_enabled, is_version_isolated
except ImportError:
    def get_version_game_dir(vid): return MINECRAFT_DIR
    def is_isolation_enabled(): return False
    def is_version_isolated(vid): return False

# 默认 JVM 参数（优化过的）
DEFAULT_JVM_ARGS = [
    "-XX:+UseG1GC",
    "-XX:+UnlockExperimentalVMOptions",
    "-XX:G1NewSizePercent=20",
    "-XX:G1ReservePercent=20",
    "-XX:MaxGCPauseMillis=50",
    "-XX:G1HeapRegionSize=32M",
    "-XX:+DisableExplicitGC",
    "-Dfml.ignoreInvalidMinecraftCertificates=true",
    "-Dfml.ignorePatchDiscrepancies=true",
    "-Dminecraft.client.jar={version_jar}",
]


def get_java_path() -> str:
    """查找 Java 可执行文件路径"""
    # 1. 检查 JAVA_HOME
    java_home = os.environ.get("JAVA_HOME")
    if java_home:
        path = os.path.join(java_home, "bin", "java.exe")
        if os.path.exists(path):
            return path

    # 2. 检查 PATH
    result = os.popen("where java 2>nul").read().strip()
    if result:
        return result.split("\n")[0]

    return "java"


def get_system_memory() -> int:
    """获取系统可用内存 (MB)"""
    try:
        import psutil
        return psutil.virtual_memory().total // (1024 * 1024)
    except ImportError:
        return 4096  # 默认 4GB


def auto_memory(version_json: dict = None) -> int:
    """
    根据系统当前可用内存动态分配 Minecraft 内存 (MB)
    规则（基于可用内存而非总内存）：
      - 可用 < 2GB   → 1024 MB
      - 可用 2~4GB   → 2048 MB
      - 可用 4~6GB   → 4096 MB
      - 可用 6~10GB  → 6144 MB
      - 可用 > 10GB  → 8192 MB
    同时保证不超过总内存的 60%
    """
    try:
        import psutil
        mem = psutil.virtual_memory()
        available = mem.available // (1024 * 1024)
        total = mem.total // (1024 * 1024)
        
        if available < 2048:
            allocated = 1024
        elif available < 4096:
            allocated = 2048
        elif available < 6144:
            allocated = 4096
        elif available < 10240:
            allocated = 6144
        else:
            allocated = 8192
        
        # 不超过总内存的 60%
        max_allowed = int(total * 0.6)
        allocated = min(allocated, max_allowed)
        # 最低保证 512MB
        allocated = max(allocated, 512)
        
        return allocated
    except ImportError:
        total = get_system_memory()
        if total < 4096:
            return 1024
        elif total < 8192:
            return 2048
        elif total < 16384:
            return 4096
        else:
            return 8192


def get_memory_status() -> dict:
    """
    获取系统内存状态，供 UI 显示
    返回: {total, available, used, percent, recommended}
    """
    try:
        import psutil
        mem = psutil.virtual_memory()
        return {
            "total": mem.total // (1024 * 1024),
            "available": mem.available // (1024 * 1024),
            "used": mem.used // (1024 * 1024),
            "percent": mem.percent,
            "recommended": auto_memory(),
        }
    except ImportError:
        total = get_system_memory()
        return {
            "total": total,
            "available": total // 2,
            "used": total // 2,
            "percent": 50.0,
            "recommended": auto_memory(),
        }


def build_jvm_args(
    version_id: str,
    java_path: str = None,
    max_memory: int = None,
    extra_args: list[str] = None,
) -> list[str]:
    """构建完整的 JVM 启动参数"""
    if java_path is None:
        java_path = get_java_path()

    if max_memory is None:
        max_memory = auto_memory()

    version_dir = os.path.join(MINECRAFT_DIR, "versions", version_id)
    version_jar = os.path.join(version_dir, f"{version_id}.jar")

    args = [java_path]

    # 内存
    args.append(f"-Xmx{max_memory}M")
    args.append(f"-Xms{min(max_memory // 2, 2048)}M")

    # 默认优化参数
    for arg in DEFAULT_JVM_ARGS:
        args.append(arg.format(
            version_jar=version_jar,
            natives_dir=os.path.join(version_dir, f"{version_id}-natives"),
        ))

    # 额外参数
    if extra_args:
        args.extend(extra_args)

    return args


def extract_natives(version_json: dict, version_id: str) -> str:
    """
    解压所有 natives 库到版本目录下的 natives 文件夹
    返回 natives 目录路径
    """
    libs_dir = os.path.join(MINECRAFT_DIR, "libraries")
    version_dir = os.path.join(MINECRAFT_DIR, "versions", version_id)
    natives_dir = os.path.join(version_dir, f"{version_id}-natives")

    # 清理旧的 natives
    if os.path.exists(natives_dir):
        shutil.rmtree(natives_dir)
    os.makedirs(natives_dir, exist_ok=True)

    for lib in version_json.get("libraries", []):
        if not _should_include_library(lib):
            continue

        downloads = lib.get("downloads", {})
        classifiers = downloads.get("classifiers", {})

        for cls_name, art in classifiers.items():
            if sys.platform == "win32" and "natives-windows" in cls_name:
                lib_path = os.path.join(libs_dir, art.get("path", ""))
                if os.path.exists(lib_path):
                    _extract_native_jar(lib_path, natives_dir, lib, cls_name)

    return natives_dir


def _extract_native_jar(jar_path: str, natives_dir: str, lib: dict, cls_name: str):
    """从 library jar 中提取 native 文件"""
    exclude = None
    extract_data = lib.get("extract", {})
    if extract_data:
        exclude = extract_data.get("exclude", [])

    try:
        with zipfile.ZipFile(jar_path, "r") as zf:
            for name in zf.namelist():
                # 只解压 native 文件（.dll, .so, .dylib, .jnilib）
                if not _is_native_file(name):
                    continue
                # 排除 META-INF 和指定排除项
                if name.startswith("META-INF"):
                    continue
                if exclude:
                    skip = False
                    for ex in exclude:
                        if name.startswith(ex):
                            skip = True
                            break
                    if skip:
                        continue
                # 解压到 natives 目录
                zf.extract(name, natives_dir)
    except Exception as e:
        pass  # 某些 jar 可能没有 native 内容


def _is_native_file(name: str) -> bool:
    """判断是否为 native 库文件"""
    ext = os.path.splitext(name)[1].lower()
    return ext in (".dll", ".so", ".dylib", ".jnilib")


def build_classpath(version_json: dict, version_id: str) -> str:
    """根据版本 JSON 构建 classpath"""
    libs_dir = os.path.join(MINECRAFT_DIR, "libraries")
    version_jar = os.path.join(MINECRAFT_DIR, "versions", version_id, f"{version_id}.jar")

    cp = [version_jar]

    for lib in version_json.get("libraries", []):
        if _should_include_library(lib):
            lib_path = _resolve_library_path(lib, libs_dir)
            if lib_path and os.path.exists(lib_path):
                cp.append(lib_path)

    return os.pathsep.join(cp)


def _should_include_library(lib: dict) -> bool:
    """
    根据 library rules 判断是否应在当前平台包含该库
    rules 按顺序处理：第一个匹配的 rule 决定结果
    - 无 os 条件的 rule 匹配所有平台
    - 有 os 条件的 rule 仅匹配指定平台
    """
    rules = lib.get("rules", [])
    if not rules:
        return True

    for rule in rules:
        os_cond = rule.get("os", {})
        action = rule.get("action", "allow")

        if os_cond:
            name = os_cond.get("name", "")
            is_match = (
                ("windows" in name.lower() and sys.platform == "win32")
                or ("linux" in name.lower() and sys.platform == "linux")
                or ("osx" in name.lower() and sys.platform == "darwin")
            )
            if is_match:
                return action == "allow"
        else:
            # 无 os 条件 → 匹配所有平台
            return action == "allow"

    return False


def _resolve_library_path(lib: dict, libs_dir: str) -> str | None:
    """解析库文件的实际路径"""
    name = lib.get("name", "")
    # 例如: com.mojang:logging:1.1.1
    parts = name.split(":")
    if len(parts) < 3:
        return None

    group, artifact, version = parts[0], parts[1], parts[2]

    # handles native classifiers
    if len(parts) > 3:
        classifier = parts[3]
    else:
        classifier = None

    group_path = group.replace(".", os.sep)
    jar_name = f"{artifact}-{version}"
    if classifier:
        jar_name += f"-{classifier}"
    jar_name += ".jar"

    return os.path.join(libs_dir, group_path, artifact, version, jar_name)


def detect_language() -> str:
    """
    检测用户所在国家/地区，返回 Minecraft 语言代码
    中国大陆 → zh_cn，否则默认 en_us
    """
    try:
        import urllib.request, json
        req = urllib.request.Request(
            "http://ip-api.com/json/?fields=countryCode",
            headers={"User-Agent": "ShadowLauncher/0.1"}
        )
        with urllib.request.urlopen(req, timeout=5) as resp:
            data = json.loads(resp.read())
            cc = data.get("countryCode", "")
            # 中文区
            if cc in ("CN", "TW", "HK", "MO", "SG"):
                return "zh_cn"
            # 日文
            if cc == "JP":
                return "ja_jp"
            # 韩文
            if cc == "KR":
                return "ko_kr"
            return "en_us"
    except Exception:
        return "en_us"


def _set_game_language(game_dir: str, lang: str):
    """在 options.txt 中设置游戏语言"""
    options_path = os.path.join(game_dir, "options.txt")
    lang_line = f"lang:{lang}"

    lines = []
    if os.path.exists(options_path):
        with open(options_path, "r", encoding="utf-8") as f:
            lines = f.readlines()

    found = False
    for i, line in enumerate(lines):
        if line.startswith("lang:"):
            lines[i] = lang_line + "\n"
            found = True
            break

    if not found:
        lines.append(lang_line + "\n")

    os.makedirs(game_dir, exist_ok=True)
    with open(options_path, "w", encoding="utf-8") as f:
        f.writelines(lines)


_LANG_CACHE = None  # 缓存检测结果


def get_or_detect_language(force_refresh: bool = False) -> str:
    """获取检测到的语言（带缓存）"""
    global _LANG_CACHE
    if _LANG_CACHE is None or force_refresh:
        _LANG_CACHE = detect_language()
    return _LANG_CACHE


def launch_minecraft(
    version_id: str,
    version_json: dict,
    username: str,
    uuid: str = "00000000-0000-0000-0000-000000000000",
    access_token: str = "0",
    max_memory: int = None,
    game_dir: str = None,
    extra_jvm_args: list[str] = None,
    logged_in: bool = False,
) -> subprocess.Popen:
    """
    启动 Minecraft
    - logged_in=False: 离线模式
    - logged_in=True: 正版登录模式（需 access_token）
    """
    java_path = get_java_path()
    version_dir = os.path.join(MINECRAFT_DIR, "versions", version_id)

    if game_dir is None:
        game_dir = get_version_game_dir(version_id)
        os.makedirs(game_dir, exist_ok=True)

    # 构建参数
    jvm_args = build_jvm_args(
        version_id=version_id,
        java_path=java_path,
        max_memory=max_memory,
        extra_args=extra_jvm_args,
    )

    classpath = build_classpath(version_json, version_id)
    main_class = version_json.get("mainClass", "net.minecraft.client.main.Main")

    # Minecraft 参数
    mc_args = [
        "--username", username,
        "--version", version_id,
        "--gameDir", game_dir,
        "--assetsDir", os.path.join(MINECRAFT_DIR, "assets"),
        "--assetIndex", version_json.get("assetIndex", {}).get("id", version_id),
        "--uuid", uuid,
        "--accessToken", access_token,
        "--userType", "msa" if logged_in else "legacy",
        "--versionType", version_json.get("type", "release"),
    ]

    # Shadow Launcher 标识
    jvm_args.append("-Dshadow.launcher=true")

    full_cmd = jvm_args + ["-cp", classpath, main_class] + mc_args

    # 解压 natives（每次启动前解压，确保是最新的）
    native_dir = extract_natives(version_json, version_id)

    # 自动设置游戏语言（根据 IP 属地）
    lang = get_or_detect_language()
    _set_game_language(game_dir, lang)

    # 启动进程
    env = os.environ.copy()
    env["APPDATA"] = os.path.dirname(MINECRAFT_DIR)  # 兼容性

    if os.path.exists(native_dir):
        env["PATH"] = native_dir + os.pathsep + env.get("PATH", "")

    process = subprocess.Popen(
        full_cmd,
        env=env,
        cwd=game_dir,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
        creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == "win32" else 0,
    )

    return process
