"""
认证模块 — 离线登录 + 微软 OAuth 正版登录

微软 OAuth 流程（授权码流）：
  1. 浏览器打开微软登录页 → 用户授权
  2. 回调 localhost ← 拿到 authorization_code
  3. code → Microsoft Token (access_token)
  4. Microsoft Token → Xbox Live Token
  5. XBL Token → XSTS Token
  6. XSTS Token → Minecraft Token
  7. Minecraft Token → Minecraft Profile (uuid + username)
"""

import json
import os
import urllib.request
import urllib.parse
import webbrowser
import hashlib
import base64
import secrets
import threading
import time
from dataclasses import dataclass
from http.server import HTTPServer, BaseHTTPRequestHandler
from typing import Optional, Callable


# === 常量 ===

# Azure 应用 Client ID（你的）
CLIENT_ID = ""  # 填你的 Client ID

# OAuth 2.0 端点
AUTHORIZE_URL = "https://login.live.com/oauth20_authorize.srf"
TOKEN_URL = "https://login.live.com/oauth20_token.srf"
REDIRECT_URI = "http://localhost:49152"
SCOPE = "XboxLive.signin offline_access"

# Xbox Live 认证链
XBL_AUTH_URL = "https://user.auth.xboxlive.com/user/authenticate"
XSTS_AUTH_URL = "https://xsts.auth.xboxlive.com/xsts/authorize"
MINECRAFT_AUTH_URL = "https://api.minecraftservices.com/authentication/login_with_xbox"
MINECRAFT_PROFILE_URL = "https://api.minecraftservices.com/minecraft/profile"
MINECRAFT_STORE_URL = "https://api.minecraftservices.com/entitlements/mcstore"


ACCOUNTS_FILE = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "config", "account.json"
)


@dataclass
class Account:
    """用户账号信息"""
    username: str
    uuid: str
    access_token: str = "0"
    refresh_token: str = ""
    is_online: bool = False  # True = 正版, False = 离线
    expires_at: float = 0

    def to_dict(self) -> dict:
        return {
            "username": self.username,
            "uuid": self.uuid,
            "access_token": self.access_token,
            "is_online": self.is_online,
        }

    @staticmethod
    def from_dict(data: dict) -> "Account":
        return Account(
            username=data["username"],
            uuid=data["uuid"],
            access_token=data.get("access_token", "0"),
            is_online=data.get("is_online", False),
        )


def save_account(account: Account):
    """保存账号数据到本地"""
    os.makedirs(os.path.dirname(ACCOUNTS_FILE), exist_ok=True)
    with open(ACCOUNTS_FILE, "w", encoding="utf-8") as f:
        json.dump(account.to_dict(), f, ensure_ascii=False, indent=2)


def load_account() -> Account | None:
    """从本地加载账号数据"""
    if not os.path.exists(ACCOUNTS_FILE):
        return None
    try:
        with open(ACCOUNTS_FILE, "r", encoding="utf-8") as f:
            data = json.load(f)
        return Account.from_dict(data)
    except Exception:
        return None


def clear_account():
    """清除本地账号数据"""
    if os.path.exists(ACCOUNTS_FILE):
        os.remove(ACCOUNTS_FILE)


class AuthCallbackHandler(BaseHTTPRequestHandler):
    """OAuth 回调处理器 — 接收 authorization_code"""

    auth_code = None
    error = None

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        params = urllib.parse.parse_qs(parsed.query)

        if "code" in params:
            AuthCallbackHandler.auth_code = params["code"][0]
            self._respond("登录成功！可以关闭此页面。", success=True)
        elif "error" in params:
            AuthCallbackHandler.error = params.get("error_description", [params["error"][0]])[0]
            self._respond("登录失败 :(", success=False)
        else:
            self._respond("正在等待授权...", success=False)

    def _respond(self, message: str, success: bool):
        color = "#6C5CE7" if success else "#e74c3c"
        bg = "#1a1a2e"
        html = f"""<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>Shadow Launcher</title>
<style>body{{background:{bg};color:#e0e0e0;display:flex;align-items:center;
justify-content:center;height:100vh;font-family:'Segoe UI',sans-serif;margin:0}}
.card{{text-align:center;padding:40px;border:1px solid #3a3a55;border-radius:12px}}
h1{{color:{color}}}p{{color:#888}}</style></head><body>
<div class="card"><h1>{message}</h1><p>Shadow Launcher · Minecraft</p></div>
</body></html>"""
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.end_headers()
        self.wfile.write(html.encode("utf-8"))

    def log_message(self, format, *args):
        pass  # 静默


def _generate_pkce() -> tuple[str, str]:
    """生成 PKCE code_verifier 和 code_challenge"""
    code_verifier = secrets.token_urlsafe(64)[:128]
    sha256 = hashlib.sha256(code_verifier.encode("ascii")).digest()
    code_challenge = base64.urlsafe_b64encode(sha256).rstrip(b"=").decode("ascii")
    return code_verifier, code_challenge


def _http_post(url: str, data: dict = None, headers: dict = None,
               json_body: bool = True) -> dict:
    """HTTP POST 请求"""
    if headers is None:
        headers = {}

    if json_body and data:
        body = json.dumps(data).encode("utf-8")
        headers["Content-Type"] = "application/json"
    elif data:
        body = urllib.parse.urlencode(data).encode("utf-8")
        headers["Content-Type"] = "application/x-www-form-urlencoded"
    else:
        body = b""

    req = urllib.request.Request(url, data=body, headers=headers, method="POST")
    with urllib.request.urlopen(req, timeout=30) as resp:
        return json.loads(resp.read().decode("utf-8"))


def _start_callback_server() -> HTTPServer:
    """启动本地 OAuth 回调服务器"""
    server = HTTPServer(("127.0.0.1", 49152), AuthCallbackHandler)
    server.timeout = 120  # 2分钟超时
    return server


def _wait_for_callback(server: HTTPServer) -> Optional[str]:
    """等待授权回调，返回 authorization_code 或 None"""
    deadline = time.time() + 120
    while time.time() < deadline:
        server.handle_request()
        if AuthCallbackHandler.auth_code:
            return AuthCallbackHandler.auth_code
        if AuthCallbackHandler.error:
            return None
    return None


def _get_authorization_code() -> Optional[str]:
    """
    打开浏览器 → 用户登录授权 → 获取 authorization_code
    """
    if not CLIENT_ID:
        raise ValueError("请先在 auth/__init__.py 中填写你的 CLIENT_ID")

    AuthCallbackHandler.auth_code = None
    AuthCallbackHandler.error = None

    code_verifier, code_challenge = _generate_pkce()

    params = {
        "client_id": CLIENT_ID,
        "response_type": "code",
        "redirect_uri": REDIRECT_URI,
        "scope": SCOPE,
        "code_challenge": code_challenge,
        "code_challenge_method": "S256",
    }
    auth_url = f"{AUTHORIZE_URL}?{urllib.parse.urlencode(params)}"

    server = _start_callback_server()

    # 打开浏览器
    webbrowser.open(auth_url)

    # 等待回调
    code = _wait_for_callback(server)
    server.server_close()

    if code is None:
        raise Exception("授权超时或失败")

    return code, code_verifier


def _exchange_code_for_token(code: str, code_verifier: str) -> dict:
    """用 authorization_code 换微软 access_token"""
    data = {
        "client_id": CLIENT_ID,
        "code": code,
        "code_verifier": code_verifier,
        "redirect_uri": REDIRECT_URI,
        "grant_type": "authorization_code",
    }
    return _http_post(TOKEN_URL, data, json_body=False)


def _refresh_token(refresh_token: str) -> dict:
    """用 refresh_token 刷新 access_token"""
    data = {
        "client_id": CLIENT_ID,
        "refresh_token": refresh_token,
        "grant_type": "refresh_token",
    }
    return _http_post(TOKEN_URL, data, json_body=False)


def _authenticate_xbl(msa_token: str) -> str:
    """微软 Token → Xbox Live Token"""
    data = {
        "Properties": {
            "AuthMethod": "RPS",
            "SiteName": "user.auth.xboxlive.com",
            "RpsTicket": f"d={msa_token}",
        },
        "RelyingParty": "http://auth.xboxlive.com",
        "TokenType": "JWT",
    }
    resp = _http_post(XBL_AUTH_URL, data)
    return resp["Token"]


def _authenticate_xsts(xbl_token: str) -> tuple[str, str]:
    """XBL Token → XSTS Token"""
    data = {
        "Properties": {
            "SandboxId": "RETAIL",
            "UserTokens": [xbl_token],
        },
        "RelyingParty": "rp://api.minecraftservices.com/",
        "TokenType": "JWT",
    }
    resp = _http_post(XSTS_AUTH_URL, data)
    return resp["Token"], resp.get("DisplayClaims", {}).get("xui", [{}])[0].get("uhs", "")


def _authenticate_minecraft(xsts_token: str, uhs: str) -> str:
    """XSTS Token → Minecraft Token"""
    data = {"identityToken": f"XBL3.0 x={uhs};{xsts_token}"}
    resp = _http_post(MINECRAFT_AUTH_URL, data)
    return resp["access_token"]


def _get_profile(mc_token: str) -> dict:
    """用 Minecraft Token 获取玩家信息"""
    headers = {"Authorization": f"Bearer {mc_token}"}
    req = urllib.request.Request(MINECRAFT_PROFILE_URL, headers=headers)
    with urllib.request.urlopen(req, timeout=15) as resp:
        return json.loads(resp.read().decode("utf-8"))


def _check_ownership(mc_token: str) -> bool:
    """检查是否拥有 Minecraft Java 版"""
    headers = {"Authorization": f"Bearer {mc_token}"}
    req = urllib.request.Request(MINECRAFT_STORE_URL, headers=headers)
    try:
        with urllib.request.urlopen(req, timeout=15) as resp:
            data = json.loads(resp.read().decode("utf-8"))
            items = data.get("items", [])
            return any(
                item.get("name") == "product_minecraft"
                for item in items
            )
    except Exception:
        return False


# === 公开 API ===

def login_offline(username: str) -> Account:
    """离线登录"""
    if not username.strip():
        raise ValueError("用户名不能为空")

    # 离线 UUID 生成（基于用户名的离线 UUID）
    offline_uuid = hashlib.md5(f"OfflinePlayer:{username}".encode()).hexdigest()
    # 格式化为标准 UUID
    uuid_str = (
        f"{offline_uuid[:8]}-{offline_uuid[8:12]}-{offline_uuid[12:16]}"
        f"-{offline_uuid[16:20]}-{offline_uuid[20:]}"
    )

    return Account(
        username=username.strip(),
        uuid=uuid_str,
        access_token="0",
        is_online=False,
    )


def login_microsoft(progress_callback: Callable[[str], None] = None) -> Account:
    """
    正版登录（完整 OAuth 流程）
    progress_callback: 接收步骤描述字符串
    """
    def _progress(msg):
        if progress_callback:
            progress_callback(msg)

    try:
        # Step 1: 获取授权码
        _progress("正在打开浏览器登录...")
        code, code_verifier = _get_authorization_code()

        # Step 2: 换取微软 Token
        _progress("正在验证微软账号...")
        msa_data = _exchange_code_for_token(code, code_verifier)
        msa_token = msa_data["access_token"]
        refresh_token = msa_data.get("refresh_token", "")

        # Step 3: XBL 认证
        _progress("正在连接 Xbox Live...")
        xbl_token = _authenticate_xbl(msa_token)

        # Step 4: XSTS 认证
        _progress("正在验证 Xbox...")
        xsts_token, uhs = _authenticate_xsts(xbl_token)

        # Step 5: Minecraft 认证
        _progress("正在连接 Minecraft...")
        mc_token = _authenticate_minecraft(xsts_token, uhs)

        # Step 6: 获取档案
        _progress("正在获取玩家信息...")
        profile = _get_profile(mc_token)

        username = profile["name"]
        uuid = profile["id"]

        # Step 7: 检查所有权
        _progress("正在验证游戏所有权...")
        owns_game = _check_ownership(mc_token)

        if not owns_game:
            _progress("⚠️ 该账号未拥有 Minecraft Java 版")

        _progress("登录成功！")

        return Account(
            username=username,
            uuid=uuid,
            access_token=mc_token,
            refresh_token=refresh_token,
            is_online=True,
            expires_at=time.time() + msa_data.get("expires_in", 3600),
        )

    except Exception as e:
        raise Exception(f"登录失败: {str(e)}")


def refresh_account(account: Account) -> Account:
    """刷新正版账号 Token"""
    if not account.is_online:
        return account

    try:
        msa_data = _refresh_token(account.refresh_token)
        msa_token = msa_data["access_token"]

        xbl_token = _authenticate_xbl(msa_token)
        xsts_token, uhs = _authenticate_xsts(xbl_token)
        mc_token = _authenticate_minecraft(xsts_token, uhs)

        account.access_token = mc_token
        account.refresh_token = msa_data.get("refresh_token", account.refresh_token)
        account.expires_at = time.time() + msa_data.get("expires_in", 3600)

        return account

    except Exception:
        # Token 刷新失败，保持原状
        return account
