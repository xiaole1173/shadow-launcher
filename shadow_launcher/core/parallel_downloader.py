"""
通用并行下载引擎
- 提取自 VersionDownloader 的核心下载逻辑
- 支持任意文件列表的并发下载
- 多镜像自动回退 + 并行竞速
- SHA1 校验 / 暂停 / 取消
- 进度回调
"""

import os
import hashlib
import urllib.request
import threading
from concurrent.futures import ThreadPoolExecutor, as_completed, Future
from contextlib import contextmanager
from typing import Callable, Optional
from dataclasses import dataclass, field
from enum import Enum

try:
    import urllib3
    URLLIB3_AVAILABLE = True
except ImportError:
    URLLIB3_AVAILABLE = False


class DownloadState(Enum):
    IDLE = "idle"
    RUNNING = "running"
    PAUSED = "paused"
    CANCELLED = "cancelled"
    DONE = "done"
    FAILED = "failed"


@dataclass
class DownloadTask:
    """单个下载任务"""
    url: str
    dest: str
    sha1: str = None
    size_hint: int = 0
    mirrors: list = field(default_factory=list)


class ConnectionPool:
    """全局 HTTP 连接池（urllib3 连接复用）"""

    def __init__(self, pool_size: int = 48, pool_per_host: int = 12):
        self._session = None
        self._pool_size = pool_size
        self._pool_per_host = pool_per_host
        self._lock = threading.Lock()

    @contextmanager
    def get(self):
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


class ParallelDownloader:
    """
    通用并行下载引擎

    用法:
        dl = ParallelDownloader(max_workers=16)
        dl.on_progress = lambda cf, tf, db, tb, name: print(f"{cf}/{tf}")
        dl.on_log = lambda msg: print(msg)

        tasks = [
            DownloadTask(url="...", dest="...", sha1="..."),
            ...
        ]
        dl.download_files(tasks)

    特性:
        - 多线程并发（可配置 worker 数）
        - 连接池复用
        - 前2个镜像并行下载竞速
        - 剩余镜像串行回退
        - SHA1 校验（自动跳过已存在且匹配的文件）
        - 暂停/恢复/取消
        - 实时进度回调
    """

    def __init__(self, max_workers: int = 16):
        self.max_workers = max_workers
        self._pool = ConnectionPool(pool_size=max_workers, pool_per_host=12)

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
        self.on_file_done: Optional[Callable[[str, bool], None]] = None  # (dest, success)

    # ═══ 状态控制 ═══

    @property
    def state(self) -> DownloadState:
        return self._state

    @property
    def completed_files(self) -> int:
        return self._completed_files

    @property
    def total_files(self) -> int:
        return self._total_files

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

    def _log(self, msg: str):
        if self.on_log:
            self.on_log(msg)

    def _emit_progress(self, name: str):
        if self.on_progress:
            self.on_progress(
                self._completed_files, self._total_files,
                self._downloaded_bytes, self._total_bytes_estimate,
                os.path.basename(name) if name else ""
            )

    def _emit_file_done(self, dest: str, success: bool):
        if self.on_file_done:
            self.on_file_done(dest, success)

    # ═══ 主入口 ═══

    def download_files(self, tasks: list[DownloadTask]) -> bool:
        """
        并发下载文件列表，返回 True 表示全部成功
        """
        self._state = DownloadState.RUNNING
        self._cancelled.clear()
        self._pause_event.set()

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

        self._executor = ThreadPoolExecutor(max_workers=self.max_workers)
        futures: dict[Future, DownloadTask] = {}
        for task in tasks:
            future = self._executor.submit(self._download_one, task)
            futures[future] = task

        failed = 0
        for future in as_completed(futures):
            if self._state == DownloadState.CANCELLED:
                break
            task = futures[future]
            try:
                result = future.result()
                if not result:
                    failed += 1
                self._emit_file_done(task.dest, result)
            except Exception as e:
                self._log(f"异常: {os.path.basename(task.dest)} - {e}")
                failed += 1
                self._emit_file_done(task.dest, False)

        try:
            self._executor.shutdown(wait=False)
        except TypeError:
            self._executor.shutdown(wait=False)
        self._executor = None

        if self._state == DownloadState.CANCELLED:
            return False

        self._state = DownloadState.FAILED if failed > 0 else DownloadState.DONE
        self._log(
            f"{'✅ 全部完成' if failed == 0 else f'⚠ {failed}/{self._total_files} 失败'}"
        )
        return failed == 0

    # ═══ 单文件下载 ═══

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
        elif os.path.exists(dest) and not task.sha1:
            # 无 SHA1 但文件存在 → 跳过（假设完整）
            with self._lock:
                self._completed_files += 1
                self._downloaded_bytes += os.path.getsize(dest)
            self._emit_progress(task.dest)
            return True

        # URL 列表（去重保序）
        urls = list(dict.fromkeys([task.url] + task.mirrors))

        # 前2个 URL 并行竞速
        if len(urls) >= 2:
            result = self._download_parallel(urls[:2], dest, task)
            if result:
                return True
            urls = urls[2:]

        # 剩余 URL 串行回退
        for url in urls:
            if self._state == DownloadState.CANCELLED:
                return False
            try:
                if self._http_download(url, dest):
                    if not self._validate_download(dest, task):
                        os.remove(dest)
                        continue
                    with self._lock:
                        self._completed_files += 1
                        self._downloaded_bytes += os.path.getsize(dest)
                    self._emit_progress(task.dest)
                    return True
            except Exception:
                continue

        return False

    def _validate_download(self, dest: str, task: DownloadTask) -> bool:
        """下载后验证：SHA1 检查"""
        if not os.path.exists(dest):
            return False
        if task.sha1:
            return self._verify_sha1(dest, task.sha1)
        return True  # 无 SHA1 约束，只要文件存在即可

    def _download_parallel(self, urls: list[str], dest: str, task: DownloadTask) -> bool:
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

        done.wait(timeout=30)

        with lock:
            for idx in sorted(results.keys()):
                ok, tmp = results[idx]
                if ok and os.path.exists(tmp):
                    if not self._validate_download(tmp, task):
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
                        self._downloaded_bytes += os.path.getsize(dest)
                    self._emit_progress(task.dest)
                    return True

        # 清理所有临时文件
        for i in range(len(urls)):
            tmp = dest + f".par.{i}"
            if os.path.exists(tmp):
                try:
                    os.remove(tmp)
                except OSError:
                    pass
        return False

    # ═══ HTTP 下载实现 ═══

    def _http_download(self, url: str, dest: str) -> bool:
        """下载 URL → dest，优先 urllib3 连接池"""
        if URLLIB3_AVAILABLE:
            return self._http_download_urllib3(url, dest)
        return self._http_download_urllib(url, dest)

    def _http_download_inner(self, url: str, dest: str) -> bool:
        """内部下载（无取消/暂停检查），供并行下载使用"""
        if URLLIB3_AVAILABLE:
            with self._pool.get() as http:
                if http is None:
                    return self._http_download_urllib_simple(url, dest)
                try:
                    resp = http.request(
                        "GET", url, preload_content=False,
                        timeout=urllib3.Timeout(connect=10, read=60),
                    )
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

    def _http_download_urllib3(self, url: str, dest: str) -> bool:
        """urllib3 连接池下载带暂停/取消"""
        with self._pool.get() as http:
            if http is None:
                return self._http_download_urllib(url, dest)
            try:
                resp = http.request(
                    "GET", url, preload_content=False,
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
        """标准 urllib 下载（回退方案）"""
        req = urllib.request.Request(url, headers={"User-Agent": "ShadowLauncher/0.2"})
        try:
            with urllib.request.urlopen(req, timeout=60) as resp:
                total = int(resp.headers.get("Content-Length", 0))
                if total > 0 and os.path.exists(dest) and os.path.getsize(dest) == total:
                    return True
                data = bytearray()
                while True:
                    if self._state == DownloadState.CANCELLED:
                        return False
                    self._pause_event.wait()
                    chunk = resp.read(131072)
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

    def _http_download_urllib_simple(self, url: str, dest: str) -> bool:
        """标准 urllib 下载（简化版，不检查暂停/取消）"""
        req = urllib.request.Request(url, headers={"User-Agent": "ShadowLauncher/0.2"})
        try:
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

    # ═══ 工具 ═══

    @staticmethod
    def verify_sha1(filepath: str, expected: str) -> bool:
        sha1 = hashlib.sha1()
        with open(filepath, "rb") as f:
            while chunk := f.read(65536):
                sha1.update(chunk)
        return sha1.hexdigest() == expected

    # 实例别名（向后兼容）
    _verify_sha1 = verify_sha1

    @staticmethod
    def _format_size(size: int) -> str:
        if size < 1024:
            return f"{size} B"
        elif size < 1024 * 1024:
            return f"{size / 1024:.1f} KB"
        else:
            return f"{size / 1024 / 1024:.1f} MB"
