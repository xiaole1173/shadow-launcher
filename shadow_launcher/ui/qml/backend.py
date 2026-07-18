"""
向后兼容桥接 — 从旧的 backend.py 路径重定向到新的 backend 包
旧导入: from shadow_launcher.ui.qml.backend import ShadowBackend
新导入: from shadow_launcher.ui.backend import ShadowBackend
"""

from shadow_launcher.ui.backend import ShadowBackend  # noqa: F401

__all__ = ["ShadowBackend"]
