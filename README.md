# Shadow Launcher

一个基于 PySide6 + QML 的 Minecraft 启动器，注重性能和交互体验。

## ✨ 功能

### 🎮 版本管理
- Minecraft 版本浏览与下载（正式版 / 快照版 / 远古版）
- 版本完整性校验（SHA1 全量扫描 + 进度条）
- 预分类版本列表（Python 后端预处理，秒级刷新）

### 🔐 账户系统
- 离线登录 + NameMC 皮肤支持
- 登录方式记忆（自动恢复上次选择）

### 📦 版本隔离
- 每个版本独立存档 / 截图 / 资源包 / Mod 目录
- 全局开关一键启用所有版本隔离

### ☕ Java 环境
- 自动检测（注册表 / Adoptium / Oracle / PATH 多路径扫描）
- 手动选择 Java 路径
- 32 位 Java 自动限制内存 ≤ 2048MB
- 系统内存状态实时显示

### 🚀 启动引擎
- 启动前 P0 校验（Java 存在性 / 核心 Jar / Natives / 内存 / 进程存活）
- 进程树终止（taskkill /F /T，确保无残留）
- 启动进度实时显示 + 一键取消
- 强制结束按钮（侧边栏底部蓝色圆形）

### 🎨 资源管理
- Modrinth Mod / 光影 / 资源包浏览与下载
- 本地资源列表刷新与删除

### 🖥️ 界面
- 无边框窗口 + 圆角设计
- 侧边栏蓝色选中条弹性动画
- 全局按钮 hover / press 弹性动画
- 下载飞球动画（版本→导航栏视觉引导）
- 列表项入场渐显动画
- Toast 通知系统（右下角天蓝色，弹性滑入，25+ 操作全覆盖）

### 🛠️ 工程化
- 持久化日志系统（5MB 轮转 + Qt 消息拦截器）
- QML 热重载（watchdog，开发模式）
- 全局中文编码兼容（chcp 65001）

## 📦 技术栈

- **Python** 3.12 + **PySide6** 6.11.1
- **QML** 前端（15 个 QML 文件）
- Mojang API（版本元数据）
- Modrinth API（资源获取）

## 🚀 运行

```bash
# 依赖安装
pip install pyside6 requests

# 启动
python run_qml.py
```

## 📦 打包

```bash
pip install pyinstaller
pyinstaller -F -w --add-data "shadow_launcher;shadow_launcher" run_qml.py
```

## ⚠️ 暂未实现

- 正版（Microsoft）账户登录
- Forge / Fabric 加载器支持
- 模组兼容性检测
- 多游戏进程管理
- 磁盘空间检测

## 📄 许可证

MIT
