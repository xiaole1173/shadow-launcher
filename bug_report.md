# Shadow Launcher — QML MouseArea 点击失效问题分析报告

**日期:** 2026年6月15日  
**作者:** 小江子 (林万江)  
**项目:** Shadow Launcher (Minecraft 启动器)  
**环境:** PySide6 6.11.1 / Qt 6.11.1 / Python 3.12.10 / Windows 11

---

## 一、问题描述

在 Shadow Launcher 开发过程中，QML 前端出现所有 MouseArea 的 onClicked 事件无法触发的严重 bug。

**正常工作的按钮:**
- 左侧导航栏的三个按钮（启动/下载/设置）——写在 MainWindow.qml 的 Repeater 中

**无法工作的按钮:**
- pageContainer 内部的所有 MouseArea（包括 Loader 加载的子 QML 和直接在 MainWindow 中写的按钮）
- StackLayout 内部的 MouseArea
- 任何用 Loader `source` 加载的子页面中的 MouseArea

**已验证正常的对照:**
- 纯 PySide6 Widget (QPushButton.clicked) ——能正常触发

---

## 二、排查过程

### 2.1 排除 FramelessWindowHint + 透明窗口引起的事件穿透

**假设:** `FramelessWindowHint | Qt.Window` 配合 `color: "transparent"` 导致 Windows 将窗口标记为分层窗口(WS_EX_LAYERED)，鼠标事件穿透到桌面。

**测试:**
1. 将 `color: "transparent"` 改为 `color: "#0e1015"` → 无效
2. 用 ctypes 直接操作 Windows API 检查和移除 WS_EX_LAYERED → 实际检查结果: EXSTYLE = 0x00000000（未设置）
3. InvalidateRect + SetWindowPos(FramChanged) → 依然无效

**结论:** FramelessWindowHint 不是根因。

### 2.2 排除 StackLayout 导致的事件丢失

**假设:** StackLayout 管理子页面 visibility 时，不可见页面覆盖在可见页面上劫持事件。

**测试:**
1. 给 StackLayout 子项加显式 visible 绑定 → 无效
2. 完全移除 StackLayout，改用三个 Loader 并列 + 手动 visible + z 控制 → 无效
3. 在 pageContainer 内部直接写一个不在任何 Loader 中的按钮 → 无效

**结论:** StackLayout 不是根因。

### 2.3 排除 Loader 加载子 QML 时的兼容性问题

**假设:** Loader source 加载的 QML 文件中的 MouseArea 存在事件衰减。

**测试:**
1. 将 HomePage.qml 精简为只有一个蓝色按钮（无任何嵌套组件）→ 无效
2. 将蓝色按钮直接写在 MainWindow.qml 的 pageContainer 中（不用 HomePage Loader）→ 无效
3. 用 `QQuickView` 单独加载 HomePage.qml（完全隔离 MainWindow）→ 无效
4. 用 `QQmlApplicationEngine` + `Window`（有标题栏，无 FramelessWindowHint）单独加载 HomePage → 无效

**结论:** Loader 和 Architecture 都不是根因。任何通过 QML 加载的 MouseArea 均无法触发。

### 2.4 排除 onClicked vs onPressed

**测试:** 将 onClicked 换为 onPressed → 依然无效

**结论:** QML 的 Qt Quick 输入系统完全不可用，而非单个信号的问题。

### 2.5 最终验证：纯 Widget 对照

**测试:** 一个最简单的 PySide6 Widget 窗口（QMainWindow + QVBoxLayout + QPushButton），点击按钮 → clicked/pressed 信号正常触发。

**结论:** Qt 事件循环本身正常，问题限定在 QML 的 Qt Quick 渲染/输入系统与 PySide6 6.11.1 的交互中。

---

## 三、根因分析

经过多轮排除，问题特征可以总结为：

1. 侧边栏的 MouseArea（在 `ApplicationWindow → RowLayout → ColumnLayout → Repeater` 路径中）**能触发**
2. 所有其他位置的 MouseArea（在 `ApplicationWindow → pageContainer → ...` 路径中）**不能触发**
3. 无论用什么 QML 加载方式（QQmlApplicationEngine/QQuickView/Window/ApplicationWindow/StackLayout/Loader/内联），结果一致

**推测根因:** PySide6 6.11.1 的 QML 鼠标事件分发机制在 `renderTarget` 或 QQuickWindow 的 `event()` 过滤中存在 bug，导致特定场景（可能在 ColumnLayout 的嵌套层级或 Item 的 clip/事件吸收属性）下事件无法到达 MouseArea。

**关键线索:** `pageContainer` 是 `Item` 类型（不是 `Rectangle`），其 `clip` 属性或事件传播行为可能与 Qt 6.11.1 的 Windows 平台插件存在兼容性问题。

---

## 四、当前项目状态

### 后端（全部完整可用）

| 模块 | 文件 | 状态 |
|------|------|------|
| 主 Backend | `__init__.py` | ✅ 多重继承整合完成 |
| 账号/登录 | `account.py` | ✅ 离线登录 + NameMC 皮肤下载 |
| 版本管理 | `version.py` | ✅ 版本列表/安装/刷新 |
| 启动引擎 | `launch.py` | ✅ 启动/停止/内存/进程 |
| 设置管理 | `settings.py` | ✅ Java 检测/FileDialog/版本隔离 |
| 资源管理 | `resource.py` | ✅ Modrinth API + 搜索/下载 |
| 并行下载 | `parallel_downloader.py` | ✅ 通用多线程下载引擎 |
| Mod管理器 | `mod_manager.py` | ✅ Modrinth/光影预置列表 |

### QML 前端（架构正常，点击失效）

| 页面 | 文件 | 核心功能 | 状态 |
|------|------|----------|------|
| 主窗口 | `MainWindow.qml` | 三导航 + StackLayout/手动切换 | ⚠️ 点击失效 |
| 启动页 | `HomePage.qml` | 登录/启动/版本选择 | ⚠️ 点击失效 |
| 下载页 | `DownloadPage.qml` | MC版本/Mod/光影/资源包 | ⚠️ 点击失效 |
| 设置页 | `SettingsPage.qml` | 导航入口 | ⚠️ 点击失效 |
| Java设置 | `SettingsJavaPage.qml` | 自动搜索/手动选择 | ⚠️ 点击失效 |
| 内存设置 | `SettingsMemoryPage.qml` | 实时内存/滑块分配 | ⚠️ 点击失效 |
| 通用设置 | `SettingsGeneralPage.qml` | 版本隔离/窗口设置 | ⚠️ 点击失效 |
| 版本选择 | `VersionSelectPage.qml` | 已安装版本列表 | ⚠️ 点击失效 |
| 版本设置 | `VersionSettingsPage.qml` | 版本详情/删除 | ⚠️ 点击失效 |
| 启动覆盖 | `LaunchOverlay.qml` | 启动进度/日志 | ⚠️ 点击失效 |

### 主入口

- `run_qml.py` — 主入口（QQmlApplicationEngine + FramelessWindowHint + ctypes WS_EX_LAYERED 修复）
- `test_click.py` — QWidget 按钮测试（正常）
- `test_click2.py` — QQmlApplicationEngine + loadData 测试（失效）
- `test_click3.py` — StackLayout + Loader 测试（页切换正常，内页失效）
- `test_click4.py` — 纯内联无 Component 测试（失效）
- `test_click5.py` — load(文件) 方式测试（失效）
- `test_homepage_alone.py` — QQuickView / QQmlApplicationEngine 单独加载 HomePage（失效）
- `test_widget.py` — 纯 Widget 按钮测试（正常）

---

## 五、建议方案

### 方案 A: 降级 PySide6 到稳定版本
- 尝试 PySide6 6.5.x 或 6.7.x
- 多个 Qt 论坛反馈 6.8+ 存在 QML 输入系统回归 bug

### 方案 B: 替换 QML 为 Qt Widgets
- 后端完美复用，只重写前端 UI
- Widget 性能对启动器完全够用

### 方案 C: 等待 PySide6 更新
- 向 Qt for Python 项目提 bug report（qtbug）
- 不推荐——周期长且不确定

---

## 六、关键代码文件列表

```
shadow-launcher/
├── run_qml.py                          # 主入口
├── shadow_launcher/
│   ├── ui/
│   │   ├── qml/
│   │   │   ├── MainWindow.qml          # 主窗口
│   │   │   ├── HomePage.qml            # 启动页
│   │   │   ├── DownloadPage.qml        # 下载页
│   │   │   ├── SettingsPage.qml        # 设置页
│   │   │   ├── SettingsGeneralPage.qml
│   │   │   ├── SettingsJavaPage.qml
│   │   │   ├── SettingsMemoryPage.qml
│   │   │   ├── VersionSelectPage.qml
│   │   │   ├── VersionSettingsPage.qml
│   │   │   ├── LaunchOverlay.qml
│   │   │   └── ShadowButton.qml
│   │   ├── backend/
│   │   │   ├── __init__.py             # ShadowBackend 主类
│   │   │   ├── account.py
│   │   │   ├── version.py
│   │   │   ├── launch.py
│   │   │   ├── settings.py
│   │   │   └── resource.py
│   │   └── backend.py                  # 兼容桥接
│   └── core/
│       ├── downloader.py               # MC 版本下载器
│       ├── parallel_downloader.py      # 通用并行下载引擎
│       └── mod_manager.py             # Modrinth API + Mod 管理
└── test_*.py                           # 各类测试文件
```
