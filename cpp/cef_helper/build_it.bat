@echo off
cd /d D:\latest-code\cpp\cef_helper
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release -- /maxcpucount:4
