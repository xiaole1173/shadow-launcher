# Shadow Launcher 项目索引
> 技术栈：C++17 + Qt 6.5.3 + QML | 编译器：MSVC 2022 | GitHub: xiaole1173/shadow-launcher
> ⚠️ 2026-06-17：项目已从 Python/PySide6 迁移至 C++/Qt6，Python 版已废弃保留在根目录

## 目录结构
```
cpp/                           # C++ 项目根目录（当前主开发线）
  CMakeLists.txt               # CMake 构建配置（MSVC 2022 + Qt 6.5.3）
  src/
    main.cpp                   # 入口：QApplication + QQmlApplicationEngine
    qmlcache/                  # QML 预编译缓存（17个 .qml → .cpp）
    backend/
      shadow_backend.h/cpp     # ShadowBackend — QML 总桥接（聚合所有子 Backend）
      app_backend.h/cpp        # AppBackend — 应用级设置
      account_backend.h/cpp    # AccountBackend — 离线/在线登录
      settings_backend.h/cpp   # SettingsBackend — Java 检测/内存设置
      check_backend.h/cpp      # CheckBackend — 启动前校验
      version_backend.h/cpp    # VersionBackend — 版本安装/校验/管理
      launch_backend.h/cpp     # LaunchBackend — 游戏启动/进程管理
      resource_backend.h/cpp   # ResourceBackend — Modrinth Mod/光影下载
    core/
      version_manager.h/cpp    # Mojang 版本清单 fetch + 缓存
      version_downloader.h/cpp # 版本安装管线（含 MirrorSource 镜像配置）
      parallel_downloader.h/cpp# 分块多源并发下载引擎（v3：HTTP Range + checkpoint）
      downloader.h/cpp         # 单文件下载器（SHA1 + 重试 + 断点续传）
      http_client.h/cpp        # HTTP 客户端（单例 QNetworkAccessManager）
      launcher.h/cpp           # Minecraft QProcess 启动逻辑
      mod_manager.h/cpp        # Modrinth API（搜索/版本/下载）
      version_isolation.h/cpp  # 版本隔离（独立游戏目录）
    utils/
      logger.h/cpp             # Qt 分类日志系统
      types.h                  # 共享数据结构（McVersion/Account/DownloadTask 等）
  qml/
    MainWindow.qml             # 主窗口（无边框+侧边栏+pageContainer）
    DownloadPage.qml           # 下载页（版本/Mod/光影/资源包 Tab）
    HomePage.qml               # 主页
    SettingsPage.qml           # 设置页
    LaunchOverlay.qml          # 启动覆盖层
    ... (14个 QML 文件)
  build/                       # CMake 构建输出（Release/ShadowLauncher.exe）
  package/                     # 打包输出

shadow_launcher/               # Python 旧版（已废弃，保留供参考）
  ui/backend/                  # PySide6 旧版 Backend
  ui/qml/                      # PySide6 旧版 QML
  core/                        # Python 旧版核心逻辑
run_qml.py                     # Python 旧版入口
```

## 核心API速查（C++ 版）

| 模块 | 关键方法 | 说明 |
|------|---------|------|
| ShadowBackend | launch(vid) / cancelLaunch() / killMinecraft() | 启动三件套 |
| ShadowBackend | installVersion(vid, sourceIndex) / getMirrorSources() | 版本安装 + 镜像选择 |
| ShadowBackend | scanJavaInstallations() / autoSelectJava() | Java管理 |
| ShadowBackend | offlineLogin(username) / getSkinUrl() | 账号 |
| ShadowBackend | getPopularMods(loader) / searchMods() / downloadMod() | Modrinth Mod |
| VersionBackend | installVersion / verifyVersion / repairVersion | 版本生命周期 |
| VersionDownloader | downloadVersion(json, vid) / setMirror() | 下载管线 |
| ParallelDownloader | addTasks / start / pause / resume / cancel | 并发下载引擎 |
| CheckBackend | checkJava / checkJar / checkMemory / checkProcess | 启动前校验 |
| ModManager | searchModrinth / downloadMod / getShaderList | Modrinth API | |

## QML页面树
```
MainWindow
├── titleBar (自绘拖拽栏)
├── navColumn (侧边栏: 主页/下载/设置 + killButton)
├── pageContainer (ColumnLayout)
│   ├── loadingBar (Windows级overlay, z:15)
│   ├── Loader: HomePage
│   ├── Loader: DownloadPage
│   └── Loader: SettingsPage
├── Loader: LaunchOverlay (翻页3D)
└── Loader: InstallConfigOverlay / VersionSelectPage / VersionSettingsPage / SubPageOverlays
```

## 关键踩坑
- 中文路径会导致Nuitka Dependency Walker崩溃
- PyInstaller打包后路径解析异常(下载功能)
- ColumnLayout中loadingBar的height变化触发全局重排→黑屏
- Windows进程终止必须用taskkill /F /T
- java -XshowSettings:all耗时2-10s，改为PE头检测0s
- QML的Connections信号名必须与Python Property notify一致
