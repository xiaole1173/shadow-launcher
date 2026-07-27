@echo off
title Shadow Launcher - Dev Mode

set SHADOW_DEV=1
cd /D D:\latest-code\cpp\build

:: Uncomment to skip Beta Key dialog:
:: set SHADOW_SKIP_BETA=1

echo.
echo ============================================
echo   Shadow Launcher - Dev Mode
echo ============================================
echo   SHADOW_DEV=1  - Filesystem QML loading
echo   Build: Release
echo   CWD: %CD%
echo ============================================
echo.
echo Launching ShadowLauncher.exe...
echo.

start "" "ShadowLauncher.exe"
