# Shadow Launcher

一个基于 PySide6 + QML 的 Minecraft 启动器。

## 功能

- 离线/正版登录
- 版本下载与管理（支持正式版、快照版、远古版）
- Java 自动检测与环境配置
- 模组/光影/资源包管理
- 启动前全链路校验（Java环境、文件完整性、内存分配）
- 进程监控与强制终止

## 技术栈

- Python 3.12 + PySide6
- QML 前端
- Mojang API 版本管理
- Modrinth API 资源获取

## 运行

```bash
pip install pyside6 requests
python run_qml.py
```

## 打包

```bash
pip install pyinstaller
pyinstaller -F -w --add-data "shadow_launcher;shadow_launcher" run_qml.py
```

## 许可证

MIT
