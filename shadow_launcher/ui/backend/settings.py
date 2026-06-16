"""
SettingsMixin — 版本隔离/工具方法/路径操作 / Java 检测
供 ShadowBackend 多重继承使用
"""

import os
import re
import shutil
import subprocess
from pathlib import Path

import psutil
from PySide6.QtCore import Slot, Property
from PySide6.QtWidgets import QFileDialog

from shadow_launcher.core.versions import (
    MINECRAFT_DIR, ensure_minecraft_dir,
    set_isolation_enabled, get_isolated_versions,
    migrate_version_to_isolated, is_version_isolated,
    get_version_game_dir,
)


# ═══ Java 检测 ═══

JAVA_SEARCH_PATHS = [
    # Common install paths
    "C:\\Program Files\\Java",
    "C:\\Program Files\\Eclipse Adoptium",
    "C:\\Program Files\\Microsoft",
    "C:\\Program Files\\Eclipse Foundation",
    "C:\\Program Files\\Amazon Corretto",
    "C:\\Program Files\\Azul",
    "C:\\Program Files\\Zulu",
    "C:\\Program Files\\BellSoft",
    "C:\\Program Files (x86)\\Java",
    "C:\\Program Files (x86)\\Eclipse Adoptium",
    "C:\\Program Files (x86)\\Microsoft",
    # Minecraft bundled Java
    "C:\\Program Files (x86)\\Minecraft Launcher\\runtime",
    "C:\\XboxGames\\Minecraft Launcher\\Content\\runtime",
    # Scoop / manual
    os.path.expandvars("%USERPROFILE%\\scoop\\apps"),
    os.path.expandvars("%LOCALAPPDATA%\\Programs"),
]


def find_java_installations():
    """扫描系统找到所有 Java 安装，返回 [(path, version_str, major_version)] 列表，按版本降序排列"""
    results = []
    seen = set()

    # 1. 检查 JAVA_HOME
    java_home = os.environ.get("JAVA_HOME", "")
    if java_home:
        exe = _find_java_exe(java_home)
        if exe and exe not in seen:
            ver = _get_java_version(exe)
            if ver:
                seen.add(exe)
                results.append((exe, ver["str"], ver["major"]))

    # 2. 检查 PATH 中的 java
    path_java = _find_java_on_path()
    if path_java and path_java not in seen:
        ver = _get_java_version(path_java)
        if ver:
            seen.add(path_java)
            results.append((path_java, ver["str"], ver["major"]))

    # 3. 扫描常见安装目录
    for search_dir in JAVA_SEARCH_PATHS:
        if not os.path.isdir(search_dir):
            continue
        try:
            for root, dirs, files in os.walk(search_dir):
                _depth = root.replace(search_dir, "").count(os.sep)
                if _depth > 4:
                    dirs.clear()
                    continue
                for d in list(dirs):
                    lower_d = d.lower()
                    exe = _find_java_exe(os.path.join(root, d))
                    if exe and os.path.isfile(exe) and exe not in seen:
                        ver = _get_java_version(exe)
                        if ver:
                            seen.add(exe)
                            results.append((exe, ver["str"], ver["major"]))
                exe = _find_java_exe(root)
                if exe and os.path.isfile(exe) and exe not in seen:
                    ver = _get_java_version(exe)
                    if ver:
                        seen.add(exe)
                        results.append((exe, ver["str"], ver["major"]))
                if _depth >= 3:
                    dirs.clear()
        except Exception:
            continue

    # 4. 从注册表查找
    try:
        import winreg
        for key_path in [
            r"SOFTWARE\JavaSoft\JDK",
            r"SOFTWARE\JavaSoft\Java Runtime Environment",
            r"SOFTWARE\Eclipse Adoptium\JDK",
            r"SOFTWARE\Eclipse Foundation\JDK",
        ]:
            try:
                key = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, key_path)
                i = 0
                while True:
                    try:
                        subkey_name = winreg.EnumKey(key, i)
                        subkey = winreg.OpenKey(key, subkey_name)
                        path_val, _ = winreg.QueryValueEx(subkey, "JavaHome")
                        winreg.CloseKey(subkey)
                        exe = _find_java_exe(path_val)
                        if exe and os.path.isfile(exe) and exe not in seen:
                            ver = _get_java_version(exe)
                            if ver:
                                seen.add(exe)
                                results.append((exe, ver["str"], ver["major"]))
                        i += 1
                    except OSError:
                        break
                winreg.CloseKey(key)
            except OSError:
                continue
    except Exception:
        pass

    # 按主版本号降序排列
    results.sort(key=lambda x: x[2], reverse=True)
    return results


def _find_java_exe(base_dir: str) -> str | None:
    """在目录及其 bin 子目录中找 java.exe / javaw.exe"""
    candidates = [
        os.path.join(base_dir, "bin", "java.exe"),
        os.path.join(base_dir, "bin", "javaw.exe"),
        os.path.join(base_dir, "java.exe"),
        os.path.join(base_dir, "javaw.exe"),
    ]
    for c in candidates:
        if os.path.isfile(c):
            return c
    return None


def _find_java_on_path() -> str | None:
    """在 PATH 中找 java"""
    for cmd in ["java", "java.exe"]:
        try:
            result = subprocess.run(
                ["where", cmd], capture_output=True, text=True, timeout=5
            )
            if result.returncode == 0:
                path = result.stdout.strip().split("\n")[0].strip()
                if os.path.isfile(path):
                    # 解析快捷方式/符号链接到真实路径
                    return os.path.realpath(path)
        except Exception:
            continue
    return None


def _get_java_version(java_exe: str) -> dict | None:
    """运行 java -version 获取版本信息，返回 {str, major, full}"""
    try:
        result = subprocess.run(
            [java_exe, "-version"],
            capture_output=True, text=True, timeout=10,
        )
        # java -version 输出到 stderr
        output = result.stderr or result.stdout
        if not output:
            return None

        # 匹配版本号：1.8.0_xxx, 11.0.x, 17.0.x, 21.0.x 等
        # OpenJDK / Oracle: "openjdk version \"1.8.0_392\""
        # 新版: "openjdk version \"21.0.5\" 2024-10-15"
        # 中文: "java version \"1.8.0_202\""
        m = re.search(r'version\s+"([^"]+)"', output)
        if not m:
            # 备选模式
            m = re.search(r'version\s+(\S+)', output)

        if m:
            version_str = m.group(1)
            major = _parse_major_version(version_str)
            return {
                "str": version_str,
                "major": major,
                "full": output.strip(),
            }
    except Exception:
        pass
    return None


def _parse_major_version(version_str: str) -> int:
    """从版本字符串提取主版本号"""
    # "1.8.0_392" → 8
    # "11.0.21" → 11
    # "21.0.5" → 21
    parts = version_str.replace("_", ".").split(".")
    if parts[0] == "1":
        # Java 8 and earlier: 1.x.y
        return int(parts[1]) if len(parts) > 1 else 0
    return int(parts[0]) if parts[0].isdigit() else 0


def pick_best_java(installations: list, min_version: int = 17) -> tuple | None:
    """从安装列表中选出最佳 Java（版本 ≥ min_version 且最高）"""
    candidates = [inst for inst in installations if inst[2] >= min_version]
    if candidates:
        return candidates[0]  # 已按版本降序
    if installations:
        return installations[0]  # 没有足够新的，返回最新的
    return None


class SettingsMixin:
    """设置和工具功能"""

    # Java 相关状态
    _java_path: str = ""
    _java_version: str = ""
    _java_major: int = 0

    # Memory settings
    _min_memory_mb: int = 512
    _max_memory_mb: int = 2048

    # General settings
    _close_after_launch: bool = False
    _log_level: str = "info"

    # ═══ Java 检测 ═══

    @Slot(result="QVariantList")
    def scanJavaInstallations(self):
        """扫描系统中的 Java 安装，返回 [{path, version, major}]"""
        self.logMessage.emit("正在扫描 Java 安装...")
        results = find_java_installations()
        result_list = []
        for path, ver_str, major in results:
            result_list.append({
                "path": path,
                "version": ver_str,
                "major": major,
            })
        if result_list:
            self.logMessage.emit(
                f"找到 {len(result_list)} 个 Java 安装，最新: Java {result_list[0]['major']} ({result_list[0]['path']})"
            )
        else:
            self.logMessage.emit("⚠ 未在系统中找到 Java 安装")
        return result_list

    @Property("QVariantList", constant=True)
    def availableJavaList(self):
        results = find_java_installations()
        result_list = []
        for path, ver_str, major in results:
            result_list.append({"path": path, "version": ver_str, "major": major})
        return result_list

    @Slot(int)
    def selectJavaByIndex(self, index: int):
        results = find_java_installations()
        if 0 <= index < len(results):
            path, ver_str, major = results[index]
            self._java_path = path
            self._java_version = ver_str
            self._java_major = major
            self.logMessage.emit(f"✅ 已切换 Java {major}: {path}")
            self.javaPathChanged.emit()

    @Slot(result=str)
    def autoSelectJava(self) -> str:
        """自动选择最优 Java，返回路径"""
        results = find_java_installations()
        best = pick_best_java(results, min_version=17)
        if best:
            path, ver_str, major = best
            self._java_path = path
            self._java_version = ver_str
            self._java_major = major
            self.logMessage.emit(f"✅ 已选择 Java {major}: {path}")
            self.javaPathChanged.emit()
            return path
        self.logMessage.emit("⚠ 未找到合适的 Java，请手动指定")
        return ""

    @Slot(result=str)
    def openJavaFileDialog(self) -> str:
        """打开文件选择对话框，选择 java.exe"""
        path, _ = QFileDialog.getOpenFileName(
            None,
            "选择 Java 可执行文件",
            "C:\\Program Files\\Java",
            "Java 可执行文件 (java.exe javaw.exe);;所有文件 (*.*)"
        )
        if path:
            self.setJavaPath(path)
            self.javaPathChanged.emit()
            return path
        return ""

    @Slot(str)
    def setJavaPath(self, path: str):
        """手动设置 Java 路径"""
        if os.path.isfile(path):
            ver = _get_java_version(path)
            if ver:
                self._java_path = path
                self._java_version = ver["str"]
                self._java_major = ver["major"]
                self.logMessage.emit(f"✅ Java 已设置: {path} (版本 {ver['str']})")
                self.javaPathChanged.emit()
            else:
                self.logMessage.emit(f"❌ 无法获取 Java 版本: {path}")
        else:
            self.logMessage.emit(f"❌ 路径不存在: {path}")

    @Slot(result=str)
    def getJavaPath(self) -> str:
        return self._java_path

    @Slot(result=str)
    def getJavaVersion(self) -> str:
        return self._java_version

    @Slot(result=int)
    def getJavaMajor(self) -> int:
        return self._java_major


    # ═══ Alias slots for QML compatibility ═══

    @Slot(result="QVariantList")
    def scanJavaInstallations(self):
        """Scan and return all Java installations as [{path, version, major}]"""
        results = find_java_installations()
        return [{"path": p, "version": v, "major": m} for p, v, m in results]

    @Slot(result=str)
    def detectJava(self) -> str:
        """QML-friendly alias: auto-detect and select best Java. Returns path."""
        return self.autoSelectJava()

    @Slot(result=str)
    def browseJava(self) -> str:
        """QML-friendly alias: open file dialog to pick Java. Returns path."""
        return self.openJavaFileDialog()

    # ═══ Memory settings ═══

    @Slot(result="QVariantMap")
    def getMemoryStatus(self):
        """Return system memory info for QML.
        Returns {total: MB, available: MB, percent: float, recommended: int}"""
        try:
            mem = psutil.virtual_memory()
            total_mb = int(mem.total / (1024 * 1024))
            avail_mb = int(mem.available / (1024 * 1024))
            percent = mem.percent
            # Recommended: half of available, capped at 8GB, min 1GB
            recommended = max(1024, min(int(avail_mb * 0.5), 8192))
            return {
                "total": total_mb,
                "available": avail_mb,
                "percent": percent,
                "recommended": recommended,
            }
        except Exception:
            return {"total": 0, "available": 0, "percent": 0, "recommended": 2048}

    @Slot(int)
    def setMinMemory(self, mb: int):
        """Set minimum memory in MB"""
        self._min_memory_mb = max(256, min(mb, self._max_memory_mb))
        self.memorySettingsChanged.emit()

    @Slot(int)
    def setMaxMemory(self, mb: int):
        """Set maximum memory in MB"""
        self._max_memory_mb = max(self._min_memory_mb, min(mb, 32768))
        self.memorySettingsChanged.emit()

    @Slot(int, int)
    def setMemoryPreset(self, lo: int, hi: int):
        """Set both min/max memory at once"""
        self._min_memory_mb = max(256, min(lo, hi))
        self._max_memory_mb = max(self._min_memory_mb, hi)
        self.memorySettingsChanged.emit()

    # ═══ General settings ═══

    @Slot(bool)
    def setCloseAfterLaunch(self, value: bool):
        self._close_after_launch = value
        self.generalSettingsChanged.emit()

    @Slot(str)
    def setLogLevel(self, level: str):
        self._log_level = level
        self.generalSettingsChanged.emit()

    # ═══ 版本隔离方法 ═══

    @Slot(bool)
    def setIsolationEnabled(self, enabled: bool):
        set_isolation_enabled(enabled)
        self._isolation_enabled = enabled
        self.isolationChanged.emit()
        self.logMessage.emit(f"版本隔离已{'开启' if enabled else '关闭'}")

    @Slot(str)
    def migrateToIsolated(self, version_id: str):
        if migrate_version_to_isolated(version_id):
            self._isolated_versions = get_isolated_versions()
            self.isolationChanged.emit()
            self.logMessage.emit(f"✅ {version_id} 已迁移到隔离模式")
        else:
            self.logMessage.emit(f"❌ {version_id} 迁移失败")

    @Slot(str, result=bool)
    def isVersionIsolated(self, version_id: str) -> bool:
        return is_version_isolated(version_id)

    @Slot(str, result=str)
    def versionGameDir(self, version_id: str) -> str:
        return get_version_game_dir(version_id)

    # ═══ 路径工具 ═══

    @Slot(str)
    def openPath(self, path: str):
        """打开任意文件/文件夹"""
        full_path = os.path.abspath(os.path.expanduser(path))
        parent_dir = (
            full_path
            if os.path.isdir(full_path)
            else os.path.dirname(full_path)
        )
        os.makedirs(parent_dir, exist_ok=True)
        if os.path.exists(full_path):
            os.startfile(full_path)
        else:
            self.logMessage.emit(f"路径不存在: {path}")

    @Slot()
    def openGameDir(self):
        ensure_minecraft_dir()
        os.startfile(MINECRAFT_DIR)

    @Slot(str)
    def openVersionDir(self, version_id: str):
        ver_dir = os.path.join(MINECRAFT_DIR, "versions", version_id)
        os.makedirs(ver_dir, exist_ok=True)
        os.startfile(ver_dir)

    @Slot(str)
    def deleteVersion(self, version_id: str):
        ver_dir = os.path.join(MINECRAFT_DIR, "versions", version_id)
        asset_index = os.path.join(
            MINECRAFT_DIR, "assets", "indexes", f"{version_id}.json"
        )

        if os.path.exists(ver_dir):
            shutil.rmtree(ver_dir, ignore_errors=True)
        if os.path.exists(asset_index):
            try:
                os.remove(asset_index)
            except Exception:
                pass

        self._installed_ids = [
            v for v in self._installed_ids if v != version_id
        ]
        self.installedVersionsChanged.emit()
        self.logMessage.emit(f"已删除版本: {version_id}")
