"""
AccountMixin — 账号登录/登出 / 皮肤下载
供 ShadowBackend 多重继承使用
"""

import json
import os
import hashlib
import urllib.request
from pathlib import Path

from PySide6.QtCore import Slot, Property

from shadow_launcher.auth import (
    Account, login_offline, login_microsoft,
    load_account, save_account, clear_account,
)

OFFLINE_HISTORY_FILE = os.path.join(os.path.dirname(__file__), "..", "..", "offline_history.json")

SKIN_CACHE_DIR = os.path.join(os.path.dirname(__file__), "..", "..", "assets", "skins")
SKIN_PLACEHOLDER = os.path.join(os.path.dirname(__file__), "..", "..", "assets", "steve.png")


def _skin_cache_path(username: str) -> str:
    """皮肤缓存路径"""
    h = hashlib.md5(username.lower().encode()).hexdigest()
    return os.path.join(SKIN_CACHE_DIR, f"{h}.png")


class AccountMixin:
    """账号管理功能"""

    _username: str = ""
    _is_logged_in: bool = False
    _is_online: bool = False
    _account_uuid: str = ""
    _account: Account = None
    _offline_usernames: list = []
    _skin_path: str = ""

    def _init_account(self):
        os.makedirs(SKIN_CACHE_DIR, exist_ok=True)
        self._load_offline_history()
        saved = load_account()
        if saved:
            self._account = saved
            self._username = saved.username
            self._is_logged_in = True
            self._is_online = saved.is_online
            self._account_uuid = saved.uuid
            self._skin_path = _skin_cache_path(saved.username)
            self.accountChanged.emit()
            self.logMessage.emit(
                f"已恢复账号: {saved.username} ({'正版' if saved.is_online else '离线'})"
            )
            self._download_skin_async(saved.username)

    def _load_offline_history(self):
        try:
            if os.path.exists(OFFLINE_HISTORY_FILE):
                with open(OFFLINE_HISTORY_FILE, "r", encoding="utf-8") as f:
                    self._offline_usernames = json.load(f)
        except Exception:
            self._offline_usernames = []

    def _save_offline_history(self):
        try:
            os.makedirs(os.path.dirname(OFFLINE_HISTORY_FILE), exist_ok=True)
            with open(OFFLINE_HISTORY_FILE, "w", encoding="utf-8") as f:
                json.dump(self._offline_usernames, f, ensure_ascii=False, indent=2)
        except Exception:
            pass

    # ═══ NameMC 皮肤下载 ═══

    def _download_skin_async(self, username: str):
        """Async download skin. Offline login skips network."""
        import threading
        def _dl():
            path = _skin_cache_path(username)
            # 已有缓存跳过
            if os.path.exists(path) and os.path.getsize(path) > 0:
                self._skin_path = path
                self.skinReady.emit()
                return

            # 离线登录直接用占位符，不请求网络
            if not self._is_online:
                if os.path.exists(SKIN_PLACEHOLDER):
                    self._skin_path = SKIN_PLACEHOLDER
                self.skinReady.emit()
                return

            # NameMC API (online login only)
            urls = [
                f"https://api.namemc.com/skin/{username}/face",
                f"https://crafatar.com/avatars/{username}?size=64&overlay",
            ]
            for url in urls:
                try:
                    req = urllib.request.Request(url, headers={
                        "User-Agent": "ShadowLauncher/0.2"
                    })
                    with urllib.request.urlopen(req, timeout=10) as resp:
                        if resp.status == 200:
                            data = resp.read()
                            with open(path, "wb") as f:
                                f.write(data)
                            self._skin_path = path
                            self.skinReady.emit()
                            return
                except Exception:
                    continue

            # 没找到，用占位符
            if os.path.exists(SKIN_PLACEHOLDER):
                self._skin_path = SKIN_PLACEHOLDER
                self.skinReady.emit()

        threading.Thread(target=_dl, daemon=True).start()

    @Slot(result=str)
    def getSkinPath(self) -> str:
        """返回当前皮肤图片的本地路径"""
        return self._skin_path or ""

    @Slot(result=str)
    def getSkinUrl(self, username: str = "") -> str:
        """返回 NameMC 皮肤头像 URL"""
        name = username or self._username
        if not name:
            return ""
        return f"https://api.namemc.com/skin/{name}/face"

    # ═══ 登录/登出 ═══

    @Slot(str)
    def offlineLogin(self, username: str):
        username = username.strip()
        if not username:
            self.logMessage.emit("用户名不能为空")
            return
        try:
            self._account = login_offline(username)
            save_account(self._account)
            self._username = self._account.username
            self._is_logged_in = True
            self._is_online = False
            self._account_uuid = self._account.uuid
            self._skin_path = _skin_cache_path(username)
            if username not in self._offline_usernames:
                self._offline_usernames.insert(0, username)
                if len(self._offline_usernames) > 20:
                    self._offline_usernames = self._offline_usernames[:20]
                self._save_offline_history()
            self.accountChanged.emit()
            self.logMessage.emit(f"离线登录: {username}")
            self._download_skin_async(username)
        except Exception as e:
            self.logMessage.emit(f"登录失败: {e}")

    @Slot()
    def logout(self):
        self._account = None
        clear_account()
        self._username = ""
        self._is_logged_in = False
        self._is_online = False
        self._account_uuid = ""
        self._skin_path = ""
        self.accountChanged.emit()
        self.logMessage.emit("已登出")

    @Property("QVariantList", constant=True)
    def offlineUsernames(self):
        return self._offline_usernames
