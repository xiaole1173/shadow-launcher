// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 Shadow
//
// SLUpdater — standalone helper process for Shadow Launcher self-update.
//
// Usage (called by ShadowLauncher after downloading a new version):
//   SLUpdater.exe <oldPid> <oldExePath> <newFilePath>
//
// 1. Wait for the main launcher process (oldPid) to exit.
// 2. Replace oldExePath with the new file.
// 3. Restart the launcher.

#include <windows.h>
#include <shellapi.h>
#include <string>
#include <cstdio>

static void showError(const wchar_t* msg, DWORD err = GetLastError())
{
    wchar_t buf[1024];
    swprintf_s(buf, L"%s (error: %lu)", msg, err);
    MessageBoxW(nullptr, buf, L"Shadow Launcher Update", MB_ICONERROR);
}

static bool waitForProcess(DWORD pid, DWORD timeoutMs)
{
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (!h) {
        // Process may already be gone — that's okay
        return true;
    }
    DWORD result = WaitForSingleObject(h, timeoutMs);
    CloseHandle(h);
    return (result == WAIT_OBJECT_0);
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR cmdLine, int)
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(cmdLine, &argc);

    if (argc < 4) {
        MessageBoxW(nullptr,
            L"Usage: SLUpdater.exe <oldPID> <oldExePath> <newFilePath>",
            L"Shadow Launcher Update", MB_ICONINFORMATION);
        LocalFree(argv);
        return 1;
    }

    // Parse arguments
    DWORD oldPid = static_cast<DWORD>(_wtoi(argv[1]));
    std::wstring oldExePath = argv[2];
    std::wstring newFilePath = argv[3];
    LocalFree(argv);

    // Strip quotes if present
    auto stripQuotes = [](std::wstring& s) {
        if (s.size() >= 2 && s.front() == L'"' && s.back() == L'"')
            s = s.substr(1, s.size() - 2);
    };
    stripQuotes(oldExePath);
    stripQuotes(newFilePath);

    std::wstring appDirW = oldExePath.substr(0, oldExePath.find_last_of(L"\\/"));

    OutputDebugStringW(L"[SLUpdater] 开始 oldPid=");
    OutputDebugStringW(argv[1]);
    OutputDebugStringW(L"\n");

    // Step 1: Wait for old process to exit (max 30 seconds)
    if (!waitForProcess(oldPid, 30000)) {
        OutputDebugStringW(L"[SLUpdater] oldPid 未正常退出，强制终止\n");
        HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, oldPid);
        if (h) {
            TerminateProcess(h, 0);
            CloseHandle(h);
            Sleep(1000);
        }
    } else {
        OutputDebugStringW(L"[SLUpdater] oldPid 已退出\n");
    }

    // Small extra delay to ensure file handles are released
    Sleep(500);

    // Step 2: Replace old exe with new file
    // Strategy: rename old -> .old, move new -> old path, delete .old
    std::wstring backupPath = oldExePath + L".old";

    // Delete stale backup from a previous update
    DeleteFileW(backupPath.c_str());

    if (!MoveFileExW(oldExePath.c_str(), backupPath.c_str(),
                     MOVEFILE_REPLACE_EXISTING)) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND) {
            OutputDebugStringW(L"[SLUpdater] 旧 exe 不存在，直接放置新文件\n");
        } else {
            showError(L"Cannot remove old version", err);
            return 2;
        }
    }

    if (!MoveFileExW(newFilePath.c_str(), oldExePath.c_str(),
                     MOVEFILE_REPLACE_EXISTING)) {
        // Rollback: restore backup
        OutputDebugStringW(L"[SLUpdater] 替换失败，回滚中\n");
        MoveFileExW(backupPath.c_str(), oldExePath.c_str(),
                    MOVEFILE_REPLACE_EXISTING);
        showError(L"Cannot install new version, restored old version", GetLastError());
        return 3;
    }
    OutputDebugStringW(L"[SLUpdater] 替换成功\n");

    // Keep backup as .old for rollback; next update will overwrite it

    // Clean up state.json and lock so new launcher doesn't re-trigger install
    std::wstring cleanupState = appDirW + L"\\_update\\state.json";
    std::wstring cleanupLock  = appDirW + L"\\_update\\.install_lock";
    if (DeleteFileW(cleanupState.c_str()))
        OutputDebugStringW(L"[SLUpdater] 已清除 state.json\n");
    if (DeleteFileW(cleanupLock.c_str()))
        OutputDebugStringW(L"[SLUpdater] 已清除安装锁\n");

    // Step 3: Restart the launcher
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(oldExePath.c_str(), nullptr,
                        nullptr, nullptr, FALSE,
                        0, nullptr, nullptr, &si, &pi)) {
        OutputDebugStringW(L"[SLUpdater] 替换完成但无法自动重启，需手动启动\n");
        showError(L"Update completed but cannot auto-restart. Please launch manually.", GetLastError());
        return 0;
    }
    OutputDebugStringW(L"[SLUpdater] 重启成功\n");
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return 0;
}
