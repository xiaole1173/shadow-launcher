:: Shadow Launcher - Dev Mode 启动脚本
:: 始终以 SHADOW_DEV=1 启动，QML 从文件系统加载，改 QML 无需编译
@echo off
set SHADOW_DEV=1
start "" "D:\latest-code\cpp\build\Release\ShadowLauncher.exe"
