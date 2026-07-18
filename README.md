# Shadow Launcher

> ⚠️ **声明**：本 README 由 AI（OpenClaw）辅助起草，非作者本人逐字书写。
> 下文所列功能中，部分可能仍在优化中，请以实际运行效果为准。

一个基于 **C++ / Qt6 / QML** 的 Minecraft 启动器。追求流畅的原生体验、完整的加载器支持和细致的交互细节。

[![License: AGPL v3](https://img.shields.io/badge/License-AGPL_v3-blue.svg)](LICENSE)

---

## ✨ 功能

### 🎮 版本与加载器

- **Minecraft 版本浏览与下载**（正式版 / 快照版 / 远古版 / 测试版可切换）
- **Mod 加载器一键安装**：Forge、NeoForge、Fabric、OptiFine
- **Fabric API 推荐安装**（选 Fabric 时自动提示）
- **合并安装**：MC + 加载器 + Fabric API 一趟完成
- **PCL 式滑动窗口速度算法** — 下载速度 3 秒平滑，告别跳变
- **多镜像自动切换**（Mojang 官方 → BMCLAPI，无需手动干预）
- **Forge / NeoForge 绕过 JVM 安装** — ZIP 解压替代 Java 进程，安装速度 30s → 2s

### 🔐 账户系统

- **Microsoft 正版登录**（OAuth 2.0 完整流程）
- **离线登录** + NameMC 皮肤显示
- **会话令牌 AES-256-GCM 加密存储**（DPAPI 保护主密钥）
- 登录方式自动记忆

### 📦 版本隔离

- 每个 MC 版本独立存档 / 截图 / 资源包 / Mod 目录
- 全局开关一键启用版本隔离

### ☕ Java 环境

- 自动检测（注册表 / Adoptium / Oracle / PATH 多路径扫描）
- 手动选择 Java 可执行文件
- 32 位 Java 自动限制内存 ≤ 2048 MB
- PE 头架构检测（0 秒判断 32/64 位，无需 JVM 启动）

### 🚀 启动引擎

- 启动前 P0 全链路校验（Java / 核心 Jar / Natives / 内存 / 进程存活）
- 进程树终止（taskkill /F /T 三重试，确保无残留）
- 启动进度实时反馈 + 一键取消
- 强制结束按钮（侧边栏底部）

### 🎨 资源管理

- **Modrinth Mod / 光影 / 资源包** 浏览与下载
- 本地资源包 / 存档 / 截图管理（刷新、删除、打开文件夹）
- 资源包背景预览（拖放设置、裁剪调整）

### 🖥️ 界面

- 无边框窗口 + 圆角设计 + 自定义标题栏
- 侧边栏蓝色选中条弹性动画（SmoothedAnimation）
- **53+ 按钮全覆盖弹性动画**（scale + ColorAnimation）
- 列表项入场渐显 + 层级错开（SpringAnimation）
- Toast 通知系统（右下角，弹性滑入，25+ 操作覆盖）
- 下载进度弧线旋转（Canvas 自定义绘制）
- 飞球动画（版本→导航栏视觉引导）

### 🛠️ 工程化

- **持久化日志系统**（5 MB 轮转 + Qt 消息拦截器）
- **开发者模式**（`SHADOW_DEV=1`：QML 文件系统加载，改 UI 无需编译）
- CMake 构建系统（Visual Studio 2022 / MSVC）
- 全量 QML 静态分析（qmllint）

---

## 📦 技术栈

| 层 | 技术 |
|----|------|
| 语言 | C++17 |
| UI 框架 | Qt 6.8 + QML（QQmlApplicationEngine） |
| 构建 | CMake + Visual Studio 2022 |
| QML 组件 | ~50 个 QML 文件 |
| C++ 后端 | ~15 个 backend 模块（版本、账户、启动、资源、设置） |
| 加密 | BCrypt + DPAPI（AES-256-GCM） |
| 网络 | Qt Network（Mojang API / Modrinth API / BMCLAPI） |
| 压缩 | QuaZip（ZIP 解压安装器） |

---

## 🚀 从源码构建

### 要求

- **Qt 6.8+**（MSVC 2022 64-bit）
- **Visual Studio 2022**（含 C++ 桌面开发工作负载）
- **CMake 3.20+**
- **Git**

### 构建

```powershell
# 克隆
git clone https://github.com/xiaole1173/shadow-launcher.git
cd shadow-launcher/cpp

# CMake 配置
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64"

# 编译
cmake --build build --target ShadowLauncher --config Release
```

输出：`build/Release/ShadowLauncher.exe`

---

## 📐 架构

```
cpp/
├── src/
│   ├── main.cpp                 # 入口 + QQmlApplicationEngine
│   ├── backend/                 # C++ 后端（~15 个模块）
│   │   ├── shadow_backend       # 核心协调层
│   │   ├── version_backend      # 下载 / 安装 / 进度管理
│   │   ├── mod_loader_installer # Forge / NeoForge / Fabric / OptiFine
│   │   ├── launch_backend       # 游戏启动 + classpath / args 构建
│   │   ├── account_backend      # Microsoft OAuth + 离线登录
│   │   ├── check_backend        # P0 启动前校验
│   │   ├── resource_backend     # Modrinth / 本地资源管理
│   │   └── settings_backend     # 设置读写
│   └── core/
│       ├── download_coordinator # 多镜像自动切换
│       ├── version_downloader   # 版本 JSON + Jar 下载
│       └── microsoft_auth       # Microsoft OAuth 流程
└── qml/                         # ~50 个 QML 组件
    ├── MainWindow.qml           # 主窗口框架
    ├── HomePage.qml             # 首页
    ├── DownloadPage.qml         # 下载页（MC / Mod / 光影 / 资源包）
    ├── InstallPage.qml          # 安装页（MC + 加载器合并）
    ├── SettingsPage.qml         # 设置页
    ├── LaunchOverlay.qml        # 启动过程覆盖层
    └── components/              # 可复用组件
```

---

## 📄 许可证

本项目采用 **GNU Affero General Public License v3 (AGPL v3)** 并附带以下补充条款：

```
a) 署名保留 — 所有副本和修改版必须在用户界面显著位置保留：
   "Based on Shadow Launcher by 影 / Shadow"

b) 名称保护 — 禁止使用 "Shadow Launcher" / "ShadowTeam"
   / "影" / "Shadow" / "xiaole1173" 推广衍生作品。修改版
   必须标明 "Modified Shadow Launcher — XX版"

c) 贡献者协议 — 所有 Pull Request 默认以 AGPL v3 入库，
   无额外 CLA 要求

d) 禁止冒充原作 — 修改版必须在启动界面显著标明
   "Based on Shadow Launcher — 非原作者发行"，并明确不与
   原作者关联。修改版须以明确方式区分其发布渠道与原作者的
   发布渠道，不得暗示或声称其为官方版本或后续版本。

e) 应用标识保护 — 禁止将 Shadow Launcher 的应用图标
   （app.ico）直接用于衍生作品。衍生作品须更换自有图标。
   内部功能图标及 QML 组件不受本条限制。

f) 本地数据保护 — 本软件不包含任何远程上报或数据外传机制。
   所有日志与使用数据仅存储于用户本地设备。衍生作品如需
   引入任何形式的数据上传或远程收集，必须在前述署名位置
   单独声明，且默认为关闭状态（opt-in）。
```

详见 [LICENSE](LICENSE)

---

## 🙏 鸣谢

| 项目 / 作者 | 贡献 |
|-------------|------|
| [**bangbang93 / BMCLAPI**](https://bmclapidoc.bangbang93.com/) | Minecraft / Forge / NeoForge / OptiFine 国内镜像加速 |
| [**z0z0r4 / MCIM**](https://github.com/z0z0r4/mcim) | Modrinth API 国内镜像加速 |
| [**Lucide**](https://lucide.dev) | 启动器全部图标库 |

---

*Shadow Launcher — 影 / Shadow · 2025-2026*
