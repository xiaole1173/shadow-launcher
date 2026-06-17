# Shadow Launcher 项目索引
> 技术栈：Python 3.12 + PySide6 6.11.1 + QML | GitHub: xiaole1173/shadow-launcher

## 目录结构
```
run_qml.py                     # 入口
shadow_launcher/
  ui/backend/
    __init__.py                 # ShadowBackend - 所有Mixin的汇聚类 (Account+Version+Launch+Settings+Resource)
    launch.py                   # LaunchMixin: launch/cancelLaunch/killMinecraft
    version.py                  # VersionMixin: installVersion/refreshInstalled
    settings.py                 # SettingsMixin + find_java_installations/pick_best_java
    resource.py                 # ResourceMixin: Modrinth Mod/Shader下载
    account.py                  # AccountMixin: offlineLogin/skin
    prelaunch_check.py          # PreLaunchChecker: 5项P0启动前校验
  ui/qml/
    MainWindow.qml              # 主窗口布局(无边框+侧边栏+pageContainer)
    backend.py                  # QML Python类型注册
    *.qml (14个)                # 页面+组件
  core/
    versions.py                 # Mojang版本清单API
    downloader.py               # 多线程版本下载器
    launcher.py                 # JVM参数构建+启动逻辑
    mod_manager.py              # Modrinth搜索下载
```

## 核心API速查

| 模块 | 关键方法 | 说明 |
|------|---------|------|
| ShadowBackend | launch(vid) / cancelLaunch() / killMinecraft() | 启动三件套 |
| ShadowBackend | installVersion(vid,src) / refreshInstalled() | 版本安装 |
| ShadowBackend | scanJavaInstallations() / selectJavaByIndex(i) | Java管理 |
| ShadowBackend | offlineLogin(username) / logout() | 账号 |
| PreLaunchChecker | check_java_executable + arch + jar + natives + memory + process | P0校验链 |
| VersionDownloader | download_version(json,vid) | 下载引擎 |

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
