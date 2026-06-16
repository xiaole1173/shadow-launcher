"""
下载管理 — 多镜像 · 高并发连接池 · 可暂停 · 可终止
"""

import os
import json
import hashlib
import urllib.request
import threading
import socket
from concurrent.futures import ThreadPoolExecutor, as_completed
from contextlib import contextmanager
from typing import Callable, Optional
from dataclasses import dataclass, field
from enum import Enum

try:
    import urllib3
    URLLIB3_AVAILABLE = True
except ImportError:
    URLLIB3_AVAILABLE = False


# === 下载源配置 ===

@dataclass
class MirrorSource:
    name: str
    desc: str
    manifest_url: str
    version_meta_host: str
    resource_base: str
    library_base: str
    jar_host: str
    is_default: bool = False


MIRROR_SOURCES = [
    MirrorSource(
        name="BMCLAPI",
        desc="国内高速 · 默认推荐",
        manifest_url="https://bmclapi2.bangbang93.com/mc/game/version_manifest.json",
        version_meta_host="bmclapi2.bangbang93.com",
        resource_base="https://bmclapi2.bangbang93.com/assets",
        library_base="https://bmclapi2.bangbang93.com/maven",
        jar_host="bmclapi2.bangbang93.com",
        is_default=True,
    ),
    MirrorSource(
        name="Mojang 官方",
        desc="官方源 · 较慢但权威",
        manifest_url="https://launchermeta.mojang.com/mc/game/version_manifest.json",
        version_meta_host="launchermeta.mojang.com",
        resource_base="https://resources.download.minecraft.net",
        library_base="https://libraries.minecraft.net",
        jar_host="launcher.mojang.com",
    ),
    MirrorSource(
        name="MCBBS 镜像",
        desc="国内备用 · 可靠",
        manifest_url="https://download.mcbbs.net/mc/game/version_manifest.json",
        version_meta_host="download.mcbbs.net",
        resource_base="https://download.mcbbs.net/assets",
        library_base="https://download.mcbbs.net/maven",
        jar_host="download.mcbbs.net",
    ),
]


class DownloadState(Enum):
    IDLE = "idle"
    RUNNING = "running"
    PAUSED = "paused"
    CANCELLED = "cancelled"
    DONE = "done"
    FAILED = "failed"


@dataclass
class DownloadTask:
    url: str
    dest: str
    sha1: str = None
    size_hint: int = 0
    mirrors: list[str] = field(default_factory=list)


class ConnectionPool:
    """
    全局 HTTP 连接池
    - 连接复用（HTTP Keep-Alive）
    - 池大小 = worker 数 × 每 host 连接数
    """

    def __init__(self, pool_size: int = 48, pool_per_host: int = 12):
        self._session = None
        self._pool_size = pool_size
        self._pool_per_host = pool_per_host
        self._lock = threading.Lock()

    @contextmanager
    def get(self):
        """获取一个可复用的 HTTP 会话"""
        if URLLIB3_AVAILABLE:
            with self._lock:
                if self._session is None:
                    self._session = urllib3.PoolManager(
                        num_pools=self._pool_size,
                        maxsize=self._pool_per_host,
                        timeout=urllib3.Timeout(connect=10, read=60),
                        retries=urllib3.Retry(3, backoff_factor=0.3),
                        headers={"User-Agent": "ShadowLauncher/0.2"},
                    )
            yield self._session
        else:
            yield None

    def close(self):
        if self._session:
            self._session.clear()
            self._session = None


class VersionDownloader:
    """
    版本下载管理器
    - 先下载 asset index JSON，解析出所有 asset objects
    - 再一次性并发下载所有文件（jar + libraries + assets）
    - 高并发连接池（默认 32 workers，连接复用）
    - 可暂停 / 可终止
    - 多镜像源自动回退
    - SHA1 校验
    """

    def __init__(
        self,
        mirror_source: MirrorSource = None,
        minecraft_dir: str = None,
        max_workers: int = 32,
    ):
        self.mirror = mirror_source or _default_mirror()
        self.minecraft_dir = minecraft_dir or os.path.join(
            os.path.expanduser("~"), ".shadow", "minecraft"
        )
        self.max_workers = max_workers
        self._pool = ConnectionPool(pool_size=max_workers, pool_per_host=16)

        self._state = DownloadState.IDLE
        self._pause_event = threading.Event()
        self._pause_event.set()
        self._cancelled = threading.Event()
        self._lock = threading.Lock()
        self._executor: ThreadPoolExecutor = None

        self._total_files = 0
        self._completed_files = 0
        self._total_bytes_estimate = 0
        self._downloaded_bytes = 0

        self.on_progress: Optional[Callable[[int, int, int, int, str], None]] = None
        self.on_log: Optional[Callable[[str], None]] = None

    @property
    def state(self):
        return self._state

    def pause(self):
        if self._state == DownloadState.RUNNING:
            self._state = DownloadState.PAUSED
            self._pause_event.clear()
            self._log("⏸ 下载已暂停")

    def resume(self):
        if self._state == DownloadState.PAUSED:
            self._state = DownloadState.RUNNING
            self._pause_event.set()
            self._log("▶ 下载已恢复")

    def cancel(self):
        if self._state in (DownloadState.RUNNING, DownloadState.PAUSED):
            self._cancelled.set()
            self._state = DownloadState.CANCELLED
            self._pool.close()
            if self._executor:
                try:
                    self._executor.shutdown(wait=False, cancel_futures=True)
                except TypeError:
                    self._executor.shutdown(wait=False)
            self._log("✕ 下载已取消")

    # ===== 主流程 =====

    def download_version(self, version_json: dict, version_id: str) -> bool:
        self._state = DownloadState.RUNNING
        self._cancelled.clear()
        self._pause_event.set()

        version_dir = os.path.join(self.minecraft_dir, "versions", version_id)
        os.makedirs(version_dir, exist_ok=True)

        # 1. 保存版本 JSON
        json_path = os.path.join(version_dir, f"{version_id}.json")
        with open(json_path, "w", encoding="utf-8") as f:
            json.dump(version_json, f, indent=2)

        # 2. 先下载 asset index JSON（它比大部分 asset 小，先拿到才能解析 object 列表）
        asset_objects = {}
        asset_idx = version_json.get("assetIndex", {})
        if asset_idx:
            idx_success = self._download_asset_index(asset_idx)
            if idx_success:
                asset_objects = self._parse_asset_index(asset_idx)
                self._log(f"资源清单: {len(asset_objects)} 个文件需要下载")
            else:
                self._log("⚠ 资源索引下载失败，跳过资源文件")

        # 3. 收集所有下载任务（jar + libs + asset objects）
        tasks: list[DownloadTask] = []
        self._collect_tasks(version_json, version_id, asset_objects, tasks)

        self._total_files = len(tasks)
        self._completed_files = 0
        self._total_bytes_estimate = sum(t.size_hint for t in tasks)
        self._downloaded_bytes = 0

        self._log(
            f"准备下载 {self._total_files} 个文件, "
            f"预估 {self._format_size(self._total_bytes_estimate)}"
        )

        if not tasks:
            self._state = DownloadState.DONE
            return True

        # 4. 多线程并发下载
        self._executor = ThreadPoolExecutor(max_workers=self.max_workers)
        futures = {}
        for task in tasks:
            future = self._executor.submit(self._download_one, task)
            futures[future] = task

        failed = 0
        for future in as_completed(futures):
            if self._state == DownloadState.CANCELLED:
                break
            task = futures[future]
            try:
                if not future.result():
                    failed += 1
            except Exception as e:
                self._log(f"异常: {os.path.basename(task.dest)} - {e}")
                failed += 1

        try:
            self._executor.shutdown(wait=False)
        except TypeError:
            self._executor.shutdown(wait=False)
        self._executor = None

        if self._state == DownloadState.CANCELLED:
            return False

        # 5. 完整性检查（带进度）
        # 立即发射初始进度，避免校验期间 UI 显示"卡在99%"
        self._log("🔍 正在进行完整性校验...")
        self._emit_progress("🔍 完整性校验开始...")
        self._completed_files = 0
        self._downloaded_bytes = 0
        missing = self._verify_integrity(version_json, version_id)
        self._completed_files = self._total_files
        self._emit_progress("校验完成")

        if missing:
            self._log(f"⚠ 完整性检查: {len(missing)} 个文件缺失")
            for m in missing[:10]:
                self._log(f"  缺失: {m}")
            if len(missing) > 10:
                self._log(f"  ... 共 {len(missing)} 个")

        if failed > 0 or missing:
            self._state = DownloadState.FAILED if missing else DownloadState.DONE
            return not missing
        else:
            self._state = DownloadState.DONE
            self._log("✅ 下载完成，所有文件通过校验")
            # 最后一个进度信号确保进度条到100%
            self._emit_progress("✅ 完成")
            return True

    # ===== asset index 下载与解析 =====

    def _download_asset_index(self, asset_idx: dict) -> bool:
        """单独下载 asset index JSON 文件"""
        idx_url = asset_idx.get("url", "")
        if not idx_url:
            return False
        idx_mirror = self._build_mirror_url(idx_url, kind="meta")
        idx_path = os.path.join(
            self.minecraft_dir, "assets", "indexes",
            f"{asset_idx.get('id', 'legacy')}.json"
        )
        os.makedirs(os.path.dirname(idx_path), exist_ok=True)

        urls = [idx_mirror]
        if idx_mirror != idx_url:
            urls.append(idx_url)

        for u in urls:
            try:
                self._http_download(u, idx_path)
                if os.path.exists(idx_path):
                    return True
            except Exception:
                continue
        return False

    def _parse_asset_index(self, asset_idx: dict) -> dict:
        """从本地 asset index JSON 解析 object 列表"""
        idx_path = os.path.join(
            self.minecraft_dir, "assets", "indexes",
            f"{asset_idx.get('id', 'legacy')}.json"
        )
        if not os.path.exists(idx_path):
            return {}
        try:
            with open(idx_path, "r", encoding="utf-8") as f:
                data = json.load(f)
            return data.get("objects", {})
        except Exception:
            return {}

    # ===== 任务收集 =====

    def _collect_tasks(
        self, version_json: dict, version_id: str,
        asset_objects: dict, tasks: list
    ):
        version_dir = os.path.join(self.minecraft_dir, "versions", version_id)

        # --- 客户端 jar ---
        client = version_json.get("downloads", {}).get("client", {})
        if client.get("url"):
            orig = client["url"]
            sz = client.get("size", 0)
            tasks.append(DownloadTask(
                url=self._build_mirror_url(orig, kind="jar"),
                dest=os.path.join(version_dir, f"{version_id}.jar"),
                sha1=client.get("sha1"),
                size_hint=sz,
                mirrors=[orig],
            ))

        # --- libraries ---
        for lib in version_json.get("libraries", []):
            self._add_library_tasks(lib, tasks)

        # --- asset objects（多镜像并行） ---
        objects_dir = os.path.join(self.minecraft_dir, "assets", "objects")
        alt_mirror = self._get_alt_mirror()
        for _name, obj in asset_objects.items():
            sha1 = obj["hash"]
            prefix = sha1[:2]
            dest = os.path.join(objects_dir, prefix, sha1)
            orig = f"https://resources.download.minecraft.net/{prefix}/{sha1}"
            mirrors = [orig]
            # 额外镜像 URL
            if self.mirror.resource_base != "https://resources.download.minecraft.net":
                mirrors.append(orig.replace(
                    "resources.download.minecraft.net",
                    self.mirror.resource_base.split("://")[1]
                ))
            if alt_mirror:
                mirrors.append(orig.replace(
                    "resources.download.minecraft.net",
                    alt_mirror.resource_base.split("://")[1]
                ))
            tasks.append(DownloadTask(
                url=mirrors[0],
                dest=dest,
                sha1=sha1,
                size_hint=obj.get("size", 0),
                mirrors=mirrors,
            ))

    def _get_alt_mirror(self) -> Optional:
        """获取备用镜像源（与主源不同）"""
        for m in MIRROR_SOURCES:
            if m.name != self.mirror.name:
                return m
        return None

    def _add_library_tasks(self, lib: dict, tasks: list):
        if not self._should_download_library(lib):
            return
        downloads = lib.get("downloads", {})

        def add(art):
            url = art.get("url", "")
            path = art.get("path", "")
            sha1 = art.get("sha1")
            sz = art.get("size", 0)
            if url and path:
                dest = os.path.join(self.minecraft_dir, "libraries", path)
                tasks.append(DownloadTask(
                    url=self._build_mirror_url(url, kind="library"),
                    dest=dest,
                    sha1=sha1,
                    size_hint=sz,
                    mirrors=[url],
                ))

        artifact = downloads.get("artifact")
        if artifact:
            add(artifact)

        classifiers = downloads.get("classifiers", {})
        if classifiers:
            import sys
            for cls_name, art in classifiers.items():
                if sys.platform == "win32" and "natives-windows" in cls_name:
                    add(art)

    def _should_download_library(self, lib: dict) -> bool:
        rules = lib.get("rules", [])
        if not rules:
            return True
        import sys
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
                return action == "allow"
        return False

    # ===== 单文件下载 =====

    def _download_one(self, task: DownloadTask) -> bool:
        if self._state == DownloadState.CANCELLED:
            return False
        self._pause_event.wait()

        dest = task.dest
        os.makedirs(os.path.dirname(dest), exist_ok=True)

        # 本地已有 + SHA1 匹配 → 跳过
        if os.path.exists(dest) and task.sha1:
            if self._verify_sha1(dest, task.sha1):
                with self._lock:
                    self._completed_files += 1
                    self._downloaded_bytes += os.path.getsize(dest)
                self._emit_progress(task.dest)
                return True
            else:
                os.remove(dest)

        # URL 列表：主 URL + 所有镜像
        urls = list(dict.fromkeys([task.url] + task.mirrors))  # 去重保序

        # 多镜像并行尝试：前2个 URL 同时下载，谁先完成用谁
        if len(urls) >= 2:
            result = self._download_parallel(urls[:2], dest, task)
            if result:
                return True
            # 并行失败则串行回退剩余 URL
            for url in urls[2:]:
                if self._state == DownloadState.CANCELLED:
                    return False
                try:
                    if self._http_download(url, dest):
                        sz = os.path.getsize(dest)
                        if task.sha1 and not self._verify_sha1(dest, task.sha1):
                            os.remove(dest)
                            continue
                        with self._lock:
                            self._completed_files += 1
                            self._downloaded_bytes += sz
                        self._emit_progress(task.dest)
                        return True
                except Exception:
                    continue
            return False

        # 单 URL 串行
        for url in urls:
            if self._state == DownloadState.CANCELLED:
                return False
            try:
                if self._http_download(url, dest):
                    sz = os.path.getsize(dest)
                    if task.sha1 and not self._verify_sha1(dest, task.sha1):
                        os.remove(dest)
                        continue
                    with self._lock:
                        self._completed_files += 1
                        self._downloaded_bytes += sz
                    self._emit_progress(task.dest)
                    return True
            except Exception:
                continue
        return False

    def _download_parallel(self, urls: list[str], dest: str, task) -> bool:
        """两个 URL 并行下载，取先完成的"""
        import tempfile
        results = {}
        lock = threading.Lock()
        done = threading.Event()

        def _try(url, idx):
            if done.is_set():
                return
            try:
                tmp = dest + f".par.{idx}"
                ok = self._http_download_inner(url, tmp)
                with lock:
                    if not done.is_set():
                        results[idx] = (ok, tmp)
                        if ok:
                            done.set()
            except Exception:
                pass

        threads = []
        for i, url in enumerate(urls):
            t = threading.Thread(target=_try, args=(url, i))
            t.daemon = True
            t.start()
            threads.append(t)

        # 等第一个完成或全部失败
        done.wait(timeout=30)

        # 取成功的（取第一个完成的）
        with lock:
            for idx in sorted(results.keys()):
                ok, tmp = results[idx]
                if ok and os.path.exists(tmp):
                    sz = os.path.getsize(tmp)
                    if task.sha1 and not self._verify_sha1(tmp, task.sha1):
                        os.remove(tmp)
                        continue
                    if os.path.exists(dest):
                        os.remove(dest)
                    os.rename(tmp, dest)
                    # 清理其他临时文件
                    for j in range(len(urls)):
                        other = dest + f".par.{j}"
                        if os.path.exists(other) and other != tmp:
                            try:
                                os.remove(other)
                            except OSError:
                                pass
                    with self._lock:
                        self._completed_files += 1
                        self._downloaded_bytes += sz
                    self._emit_progress(task.dest)
                    return True

        # 清理失败的临时文件
        for i in range(len(urls)):
            tmp = dest + f".par.{i}"
            if os.path.exists(tmp):
                try:
                    os.remove(tmp)
                except OSError:
                    pass
        return False

    def _http_download_inner(self, url: str, dest: str) -> bool:
        """内部下载（无取消/暂停检查），供并行下载使用"""
        if URLLIB3_AVAILABLE:
            with self._pool.get() as http:
                if http is None:
                    return self._http_download_urllib_simple(url, dest)
                try:
                    resp = http.request("GET", url, preload_content=False,
                                       timeout=urllib3.Timeout(connect=10, read=60))
                except Exception:
                    return False
                if resp.status != 200:
                    resp.release_conn()
                    return False
                try:
                    with open(dest, "wb") as f:
                        for chunk in resp.stream(65536):
                            f.write(chunk)
                    resp.release_conn()
                    return True
                except Exception:
                    resp.release_conn()
                    return False
        return self._http_download_urllib_simple(url, dest)

    def _http_download_urllib_simple(self, url: str, dest: str) -> bool:
        try:
            req = urllib.request.Request(
                url, headers={"User-Agent": "ShadowLauncher/0.1"}
            )
            with urllib.request.urlopen(req, timeout=60) as resp:
                with open(dest, "wb") as f:
                    while True:
                        chunk = resp.read(131072)
                        if not chunk:
                            break
                        f.write(chunk)
            return True
        except Exception:
            return False

    def _http_download(self, url: str, dest: str) -> bool:
        """
        下载单个文件到 dest
        优先使用 urllib3 连接池（HTTP Keep-Alive），回退到 urllib
        """
        if URLLIB3_AVAILABLE:
            return self._http_download_urllib3(url, dest)
        return self._http_download_urllib(url, dest)

    def _http_download_urllib3(self, url: str, dest: str) -> bool:
        """urllib3 连接池下载 — 复用 TCP 连接"""
        with self._pool.get() as http:
            if http is None:
                return self._http_download_urllib(url, dest)

            try:
                resp = http.request(
                    "GET", url,
                    preload_content=False,
                    timeout=urllib3.Timeout(connect=10, read=60),
                )
            except Exception:
                return False

            if resp.status != 200:
                resp.release_conn()
                return False

            total = int(resp.headers.get("Content-Length", 0))
            if total > 0 and os.path.exists(dest) and os.path.getsize(dest) == total:
                resp.release_conn()
                return True

            tmp = dest + ".tmp"
            try:
                with open(tmp, "wb") as f:
                    for chunk in resp.stream(65536):
                        if self._state == DownloadState.CANCELLED:
                            resp.release_conn()
                            return False
                        self._pause_event.wait()
                        f.write(chunk)
                resp.release_conn()
            except Exception:
                resp.release_conn()
                if os.path.exists(tmp):
                    os.remove(tmp)
                return False

            if os.path.exists(dest):
                os.remove(dest)
            os.rename(tmp, dest)
            return True

    def _http_download_urllib(self, url: str, dest: str) -> bool:
        """标准 urllib 下载 — 无连接池回退"""
        req = urllib.request.Request(url, headers={"User-Agent": "ShadowLauncher/0.1"})
        # 增大 socket 缓冲区
        socket_timeout = 60
        try:
            with urllib.request.urlopen(req, timeout=socket_timeout) as resp:
                total = int(resp.headers.get("Content-Length", 0))
                if total > 0 and os.path.exists(dest) and os.path.getsize(dest) == total:
                    return True
                data = bytearray()
                while True:
                    if self._state == DownloadState.CANCELLED:
                        return False
                    self._pause_event.wait()
                    chunk = resp.read(131072)  # 128KB 缓冲
                    if not chunk:
                        break
                    data.extend(chunk)
                tmp = dest + ".tmp"
                with open(tmp, "wb") as f:
                    f.write(data)
                if os.path.exists(dest):
                    os.remove(dest)
                os.rename(tmp, dest)
                return True
        except Exception:
            return False

    # ===== 完整性检查 =====

    def _verify_integrity(self, version_json: dict, version_id: str) -> list[str]:
        missing = []
        version_dir = os.path.join(self.minecraft_dir, "versions", version_id)

        total_items = 1  # jar
        libs_to_check = []
        for lib in version_json.get("libraries", []):
            if self._should_download_library(lib):
                downloads = lib.get("downloads", {})
                for art_key in ("artifact",):
                    a = downloads.get(art_key)
                    if a:
                        libs_to_check.append(a)
                for cls_name, art in downloads.get("classifiers", {}).items():
                    if "natives-windows" in cls_name:
                        libs_to_check.append(art)
        total_items += len(libs_to_check)

        # 预估 assets 数量
        asset_idx = version_json.get("assetIndex", {})
        idx_path = os.path.join(
            self.minecraft_dir, "assets", "indexes",
            f"{asset_idx.get('id', 'legacy')}.json"
        )
        asset_count = 0
        if asset_idx and os.path.exists(idx_path):
            try:
                with open(idx_path, "r", encoding="utf-8") as f:
                    idx_data = json.load(f)
                asset_count = len(idx_data.get("objects", {}))
            except Exception:
                pass
        total_items += asset_count

        # 入口立即发零进度，防止 UI 僵死
        self._emit_verify_progress(0, total_items)
        checked = 0

        # jar
        jar = os.path.join(version_dir, f"{version_id}.jar")
        checked += 1
        self._emit_verify_progress(checked, total_items)
        if not os.path.exists(jar):
            missing.append(f"versions/{version_id}/{version_id}.jar")

        # libraries
        for art in libs_to_check:
            checked += 1
            if checked % 10 == 0:
                self._emit_verify_progress(checked, total_items)
            p = art.get("path", "")
            d = os.path.join(self.minecraft_dir, "libraries", p)
            if not os.path.exists(d):
                missing.append(f"libraries/{p}")
            elif art.get("sha1") and not self._verify_sha1(d, art["sha1"]):
                missing.append(f"libraries/{p} (SHA1)")

        # assets
        if asset_idx and asset_count > 0 and os.path.exists(idx_path):
            try:
                with open(idx_path, "r", encoding="utf-8") as f:
                    idx_data = json.load(f)
                od = os.path.join(self.minecraft_dir, "assets", "objects")
                batch = 0
                for name, obj in idx_data.get("objects", {}).items():
                    checked += 1
                    batch += 1
                    if batch % 100 == 0:
                        self._emit_verify_progress(checked, total_items)
                    sha = obj["hash"]
                    pf = sha[:2]
                    d = os.path.join(od, pf, sha)
                    if not os.path.exists(d):
                        missing.append(f"assets/{name}")
                    elif not self._verify_sha1(d, sha):
                        missing.append(f"assets/{name} (SHA1)")
            except Exception:
                missing.append("assets/index (解析失败)")

        self._emit_verify_progress(total_items, total_items)
        return missing

    def _emit_verify_progress(self, checked: int, total: int):
        if self.on_progress:
            self.on_progress(
                checked, total,
                0, 0,
                f"校验中... {checked}/{total}"
            )

    def _emit_progress(self, name: str):
        if self.on_progress:
            self.on_progress(
                self._completed_files, self._total_files,
                self._downloaded_bytes, self._total_bytes_estimate,
                os.path.basename(name)
            )

    # ===== URL 镜像 =====

    def _build_mirror_url(self, original_url: str, kind: str = "jar") -> str:
        if not original_url or self.mirror.name == "Mojang 官方":
            return original_url
        rep = {
            "launcher.mojang.com": self.mirror.jar_host,
            "launchermeta.mojang.com": self.mirror.version_meta_host,
            "libraries.minecraft.net": self.mirror.library_base.split("://")[1],
            "resources.download.minecraft.net": self.mirror.resource_base.split("://")[1],
        }
        for old, new in rep.items():
            if old in original_url:
                return original_url.replace(old, new)
        return original_url

    def _log(self, msg: str):
        if self.on_log:
            self.on_log(msg)

    @staticmethod
    def _verify_sha1(filepath: str, expected: str) -> bool:
        sha1 = hashlib.sha1()
        with open(filepath, "rb") as f:
            while chunk := f.read(65536):
                sha1.update(chunk)
        return sha1.hexdigest() == expected

    @staticmethod
    def _format_size(size: int) -> str:
        if size < 1024:
            return f"{size} B"
        elif size < 1024 * 1024:
            return f"{size / 1024:.1f} KB"
        else:
            return f"{size / 1024 / 1024:.1f} MB"


def _default_mirror() -> MirrorSource:
    return MIRROR_SOURCES[0]


# 兼容旧接口
class Downloader:
    def __init__(self, progress_callback=None):
        self.callback = progress_callback

    def download_file(self, url, dest, sha1=None, mirrors=None, max_retries=2):
        vd = VersionDownloader()
        task = DownloadTask(url=url, dest=dest, sha1=sha1, mirrors=mirrors or [])
        return vd._download_one(task)
