// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 Shadow
//
// SLUpdater — standalone helper process for Shadow Launcher self-update.
//
// Usage (called by ShadowLauncher after downloading a new version):
//   增量: SLUpdater.exe <oldPid> <oldExePath> <newFilePath>
//   全量: SLUpdater.exe <oldPid> <oldExePath> <newFilePath> <fullExtractDir>
//         全量时 newFilePath = <fullExtractDir>/ShadowLauncher.exe
//
// 流程（2026-08-15 方案A重构，替代原 PreInit 内 applyFullUpdate）：
//   1. 显示小型进度窗口（纯 Win32，无 Qt）
//   2. 等旧启动器进程退出（文件解锁）
//   3. 全量：复制 extracted/* → appDir（覆盖+新增），再删除多余旧文件
//   4. 替换 ShadowLauncher.exe（.old 备份 + 回滚）
//   5. 清理 _update，重启新版本
// 全量复制失败时不删 state.json/zip → 下次启动自动重试（自愈）。

#ifndef UNICODE
#  define UNICODE       // 确保 IDC_* / TCHAR 宏展开为宽字符版本（须在 windows.h 之前）
#  define _UNICODE
#endif
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <tlhelp32.h>   // 2026-08-15：枚举/结束 helper 进程（easytier/wv2login 可能锁文件）
#include <string>
#include <cstdio>
#include <cstdarg>

#pragma comment(lib, "comctl32.lib")

static HWND g_hwnd = nullptr;
static HWND g_hTitle = nullptr;
static HWND g_hPhase = nullptr;
static HWND g_hProgress = nullptr;
static HBRUSH g_bgBrush = nullptr;
static HFONT g_titleFont = nullptr;
static HFONT g_phaseFont = nullptr;

static const wchar_t* kWinClass = L"ShadowUpdateWindow";

// ── 消息泵：长任务期间保持窗口响应 ──
static void pumpMessages()
{
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

// ── 阶段推进：percent < 0 = 跑马灯（不定进度）──
static void setPhase(const wchar_t* text, int percent)
{
    if (g_hPhase) SetWindowTextW(g_hPhase, text);
    if (g_hProgress) {
        if (percent < 0) {
            SendMessageW(g_hProgress, PBM_SETMARQUEE, TRUE, 40);
        } else {
            SendMessageW(g_hProgress, PBM_SETMARQUEE, FALSE, 0);
            SendMessageW(g_hProgress, PBM_SETPOS, percent, 0);
        }
    }
    pumpMessages();
}

static void showError(const wchar_t* msg, DWORD err = GetLastError())
{
    wchar_t buf[1024];
    swprintf_s(buf, L"%s (error: %lu)", msg, err);
    MessageBoxW(nullptr, buf, L"Shadow Launcher Update", MB_ICONERROR);
}

// ── 2026-08-15：详细日志（OutputDebugString + _update/sl_updater.log）──
// 之前报错不记录失败文件路径，无法定位 error 32 具体来源。日志写在
// _update/ 下（成功清理 _update 时一并删除；失败保留供诊断）。
static FILE* g_logFile = nullptr;

static void openLog(const std::wstring& appDir)
{
    const std::wstring p = appDir + L"\\_update\\sl_updater.log";
    _wfopen_s(&g_logFile, p.c_str(), L"w, ccs=UTF-8");
}

static void logMsg(const wchar_t* fmt, ...)
{
    wchar_t buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vswprintf_s(buf, fmt, ap);
    va_end(ap);
    OutputDebugStringW(buf);
    OutputDebugStringW(L"\n");
    if (g_logFile) {
        fwprintf(g_logFile, L"%s\n", buf);
        fflush(g_logFile);
    }
}

static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CLOSE:
        return 0;   // 更新进行中禁止关闭（关闭=残缺安装）
    case WM_CTLCOLORSTATIC: {
        HDC hdc = reinterpret_cast<HDC>(wp);
        SetTextColor(hdc, RGB(0xE6, 0xE8, 0xEE));
        SetBkColor(hdc, RGB(0x14, 0x1A, 0x24));
        return reinterpret_cast<LRESULT>(g_bgBrush);
    }
    case WM_ERASEBKGND:
        if (g_bgBrush) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            FillRect(reinterpret_cast<HDC>(wp), &rc, g_bgBrush);
            return 1;
        }
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ── 深色小窗口（对齐启动器卡片风格）──
static void createProgressWindow(HINSTANCE hInst)
{
    g_bgBrush = CreateSolidBrush(RGB(0x14, 0x1A, 0x24));

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = g_bgBrush;
    wc.lpszClassName = kWinClass;
    RegisterClassExW(&wc);

    constexpr int kClientW = 420, kClientH = 150;
    RECT rc = { 0, 0, kClientW, kClientH };
    AdjustWindowRectEx(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE, 0);
    const int w = rc.right - rc.left, h = rc.bottom - rc.top;
    const int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);

    g_hwnd = CreateWindowExW(0, kWinClass, L"Shadow Launcher 更新中",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        (sw - w) / 2, (sh - h) / 2, w, h,
        nullptr, nullptr, hInst, nullptr);

    // 禁用系统菜单的关闭项（灰化）
    HMENU sys = GetSystemMenu(g_hwnd, FALSE);
    if (sys) EnableMenuItem(sys, SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);

    // 标题（大号）
    g_hTitle = CreateWindowExW(0, L"STATIC", L"正在准备更新…",
        WS_CHILD | WS_VISIBLE | SS_CENTER, 0, 18, kClientW, 28,
        g_hwnd, nullptr, hInst, nullptr);
    g_titleFont = CreateFontW(20, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0,
        DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
    SendMessageW(g_hTitle, WM_SETFONT, reinterpret_cast<WPARAM>(g_titleFont), TRUE);

    // 阶段文字
    g_hPhase = CreateWindowExW(0, L"STATIC", L"…",
        WS_CHILD | WS_VISIBLE | SS_CENTER, 0, 56, kClientW, 22,
        g_hwnd, nullptr, hInst, nullptr);
    g_phaseFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, 0, 0, 0,
        DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
    SendMessageW(g_hPhase, WM_SETFONT, reinterpret_cast<WPARAM>(g_phaseFont), TRUE);

    // 进度条
    g_hProgress = CreateWindowExW(0, PROGRESS_CLASSW, L"",
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH | PBS_MARQUEE, 40, 94, kClientW - 80, 16,
        g_hwnd, nullptr, hInst, nullptr);
    SendMessageW(g_hProgress, PBM_SETMARQUEE, FALSE, 0);

    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);
    pumpMessages();
}

// ── 等待旧进程退出（分段等待，保持消息泵）──
static bool waitForProcess(DWORD pid, DWORD timeoutMs)
{
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (!h) return true;   // 进程已不在
    const DWORD start = GetTickCount();
    for (;;) {
        DWORD r = WaitForSingleObject(h, 100);
        pumpMessages();
        if (r == WAIT_OBJECT_0) break;
        if (r == WAIT_TIMEOUT && (GetTickCount() - start) >= timeoutMs) {
            CloseHandle(h);
            return false;
        }
    }
    CloseHandle(h);
    return true;
}

// ── 递归删除目录 ──
static void removeDirRecursive(const std::wstring& dir)
{
    std::wstring search = dir + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(search.c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
            std::wstring p = dir + L"\\" + fd.cFileName;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                removeDirRecursive(p);
            else
                DeleteFileW(p.c_str());
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }
    RemoveDirectoryW(dir.c_str());
}

// ── 统计目录内文件总数（复制进度用）──
static DWORD countFilesRecursive(const std::wstring& dir)
{
    DWORD n = 0;
    std::wstring search = dir + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(search.c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
            std::wstring p = dir + L"\\" + fd.cFileName;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                n += countFilesRecursive(p);
            else
                n++;
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }
    return n;
}

// ── 2026-08-15：结束可能锁定更新文件的 helper 进程 ──
// 旧启动器退出后，easytier-core（联机通道）/wv2login（嵌入式登录）可能作为
// 独立进程存活并锁定 bin/ 或 WebView2Loader.dll → 复制报 error 32
// (ERROR_SHARING_VIOLATION)。更新需要替换这些文件，先结束它们是安全的
// （新版本启动时会重新拉起）。
static void killHelperProcesses()
{
    const wchar_t* names[] = {
        L"easytier-core.exe", L"easytier-cli.exe", L"wv2login.exe",
    };
    for (const wchar_t* n : names) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) continue;
        PROCESSENTRY32W pe = { sizeof(pe) };
        if (Process32FirstW(snap, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, n) == 0) {
                    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                    if (h) {
                        TerminateProcess(h, 0);
                        CloseHandle(h);
                    }
                }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
    }
    Sleep(500);   // 等句柄释放
}

// ── 2026-08-15：复制单个文件，失败重试 8 次（0.5/1/1.5/…/4s ≈ 18s）──
// 覆盖目标：DeleteFileW 先行（目标只读/存在）；每次失败记录具体文件路径到
// 日志（此前只报 error 32 无路径，无法定位）。返回 true 成功；失败时
// *failPath 记录是哪个文件。
static bool copyFileOverwrite(const std::wstring& src, const std::wstring& dst,
                              std::wstring* failPath = nullptr)
{
    for (int attempt = 0; attempt < 8; ++attempt) {
        if (CopyFileW(src.c_str(), dst.c_str(), FALSE)) return true;
        const DWORD err = GetLastError();
        DeleteFileW(dst.c_str());
        logMsg(L"[SLUpdater] 复制失败 %s -> %s (err=%lu, attempt=%d)",
               src.c_str(), dst.c_str(), err, attempt + 1);
        Sleep(500 * (attempt + 1));
    }
    if (failPath) *failPath = dst;
    return false;
}

// ── 递归复制（覆盖+新增）；skipExe=true 跳过两个 exe（单独处理）──
// done/total 用于进度（40%→90%）。
// 2026-08-15：isBin=true 的子树（bin/ 联机组件：easytier/wintun/WinDivert）失败
// 降级跳过（联机可选功能，缺失不阻止启动器更新；日志记录），核心文件失败
// 才整体失败（*firstFail 记录具体文件）。
static bool copyTreeOverwrite(const std::wstring& srcDir, const std::wstring& dstDir,
                              DWORD& done, DWORD total, bool skipExe,
                              bool isBin, int* skipped, std::wstring* firstFail)
{
    CreateDirectoryW(dstDir.c_str(), nullptr);
    std::wstring search = srcDir + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(search.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return true;

    bool ok = true;
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        std::wstring s = srcDir + L"\\" + fd.cFileName;
        std::wstring d = dstDir + L"\\" + fd.cFileName;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            const bool childBin = isBin || wcscmp(fd.cFileName, L"bin") == 0;
            if (!copyTreeOverwrite(s, d, done, total, skipExe, childBin, skipped, firstFail)) {
                ok = false; break;
            }
        } else {
            if (skipExe && (wcscmp(fd.cFileName, L"ShadowLauncher.exe") == 0
                         || wcscmp(fd.cFileName, L"SLUpdater.exe") == 0)) continue;
            if (!copyFileOverwrite(s, d)) {
                if (isBin) {
                    (*skipped)++;
                    logMsg(L"[SLUpdater] bin 联机组件复制失败已跳过: %s", d.c_str());
                    continue;   // 联机可选，降级跳过
                }
                if (firstFail) *firstFail = d;
                ok = false; break;
            }
            done++;
            if (total > 0) {
                const int pct = 40 + static_cast<int>(static_cast<double>(done) * 50.0 / static_cast<double>(total));
                setPhase(L"正在更新文件…", pct);
            }
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
    return ok;
}

// ── 全量后删除多余旧文件：排除保留项 + extracted 内同名（刚复制的新文件）──
static void deleteStaleFiles(const std::wstring& appDir, const std::wstring& keepDir)
{
    std::wstring search = appDir + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(search.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        // 保留：游戏数据 / 日志 / Java 缓存 / 更新临时 / 协议记录 / 两个 exe（已单独处理）
        if (wcscmp(fd.cFileName, L".minecraft") == 0
            || wcscmp(fd.cFileName, L"logs") == 0
            || wcscmp(fd.cFileName, L"java_cache") == 0
            || wcscmp(fd.cFileName, L"_update") == 0
            || wcscmp(fd.cFileName, L"agreement_consent.txt") == 0
            || wcscmp(fd.cFileName, L"ShadowLauncher.exe") == 0
            || wcscmp(fd.cFileName, L"ShadowLauncher.exe.old") == 0
            || wcscmp(fd.cFileName, L"SLUpdater.exe") == 0)
            continue;
        // keepDir 有同名 → 刚复制的新文件，保留
        const std::wstring kept = keepDir + L"\\" + fd.cFileName;
        if (GetFileAttributesW(kept.c_str()) != INVALID_FILE_ATTRIBUTES) continue;
        const std::wstring p = appDir + L"\\" + fd.cFileName;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            removeDirRecursive(p);
        else
            DeleteFileW(p.c_str());
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int)
{
    // 2026-08-11 修复：wWinMain 的 lpCmdLine 不含程序名，必须用 GetCommandLineW()
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    if (argc < 4) {
        MessageBoxW(nullptr,
            L"Usage: SLUpdater.exe <oldPID> <oldExePath> <newFilePath> [fullExtractDir]",
            L"Shadow Launcher Update", MB_ICONINFORMATION);
        LocalFree(argv);
        return 1;
    }

    // 2026-08-15：立即拷贝到本地变量后再 LocalFree（修 use-after-free：
    // 旧代码 LocalFree 后仍用 argv[1] 调 OutputDebugStringW）
    const DWORD oldPid = static_cast<DWORD>(_wtoi(argv[1]));
    std::wstring oldExePath = argv[2];
    std::wstring newFilePath = argv[3];
    const std::wstring fullExtractDir = (argc >= 5) ? argv[4] : std::wstring();
    LocalFree(argv);

    auto stripQuotes = [](std::wstring& s) {
        if (s.size() >= 2 && s.front() == L'"' && s.back() == L'"')
            s = s.substr(1, s.size() - 2);
    };
    stripQuotes(oldExePath);
    stripQuotes(newFilePath);

    std::wstring appDirW = oldExePath.substr(0, oldExePath.find_last_of(L"\\/"));
    const bool isFull = !fullExtractDir.empty();

    // ── 1. 进度窗口 ──
    createProgressWindow(hInst);
    setPhase(L"正在等待旧版本退出…", -1);

    // ── 2. 等旧进程退出（30s 超时强制终止）──
    if (!waitForProcess(oldPid, 30000)) {
        OutputDebugStringW(L"[SLUpdater] oldPid 未正常退出，强制终止\n");
        HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, oldPid);
        if (h) {
            TerminateProcess(h, 0);
            CloseHandle(h);
            Sleep(1000);
        }
    }
    Sleep(500);   // 确保文件句柄释放
    pumpMessages();

    // ── 2026-08-15：结束 helper 进程，避免复制报 error 32（共享冲突）──
    killHelperProcesses();

    // ── 3. 全量：复制新文件（覆盖+新增）→ 删多余旧文件 ──
    if (isFull) {
        openLog(appDirW);
        logMsg(L"[SLUpdater] 全量更新开始 oldPid=%lu isFull=1", oldPid);
        setPhase(L"正在更新文件…", 5);
        DWORD total = countFilesRecursive(fullExtractDir);
        DWORD done = 0;
        int skippedBin = 0;
        std::wstring firstFail;
        if (!copyTreeOverwrite(fullExtractDir, appDirW, done, total, true,
                               false, &skippedBin, &firstFail)) {
            // 失败：不删 state.json / zip → 下次启动 PreInit 自动重试（自愈）
            logMsg(L"[SLUpdater] 全量复制失败，保留现场待重试。失败文件: %s 已复制=%lu/%lu 跳过bin=%d",
                   firstFail.c_str(), done, total, skippedBin);
            if (g_logFile) { fclose(g_logFile); g_logFile = nullptr; }
            wchar_t errBuf[1024];
            swprintf_s(errBuf, L"文件更新失败(%s)，将保留现场，下次启动自动重试。详情见 _update/sl_updater.log",
                       firstFail.c_str());
            showError(errBuf, ERROR_SHARING_VIOLATION);
            DestroyWindow(g_hwnd);
            return 4;
        }
        if (skippedBin > 0)
            logMsg(L"[SLUpdater] 警告: %d 个 bin/ 联机组件复制失败已跳过（不影响启动器本体）", skippedBin);
        setPhase(L"正在清理旧文件…", 92);
        deleteStaleFiles(appDirW, fullExtractDir);
    } else {
        openLog(appDirW);
        logMsg(L"[SLUpdater] 增量更新开始 oldPid=%lu isFull=0", oldPid);
    }

    // ── 4. 替换 exe（增量/全量共用）：rename old → .old，move new → old，失败回滚 ──
    setPhase(L"正在替换主程序…", 95);
    {
        std::wstring backupPath = oldExePath + L".old";
        DeleteFileW(backupPath.c_str());   // 清上次残留备份
        if (!MoveFileExW(oldExePath.c_str(), backupPath.c_str(), MOVEFILE_REPLACE_EXISTING)) {
            DWORD err = GetLastError();
            if (err != ERROR_FILE_NOT_FOUND) {
                showError(L"Cannot remove old version", err);
                DestroyWindow(g_hwnd);
                return 2;
            }
        }
        if (!MoveFileExW(newFilePath.c_str(), oldExePath.c_str(), MOVEFILE_REPLACE_EXISTING)) {
            MoveFileExW(backupPath.c_str(), oldExePath.c_str(), MOVEFILE_REPLACE_EXISTING);   // 回滚
            showError(L"Cannot install new version, restored old version", GetLastError());
            DestroyWindow(g_hwnd);
            return 3;
        }
        DeleteFileW(backupPath.c_str());   // 替换成功，删除旧 exe 备份
    }

    // ── 5. 清理 _update（state/lock/zip/extracted/日志 一并删除）──
    if (g_logFile) { fclose(g_logFile); g_logFile = nullptr; }   // 先关日志再删目录
    setPhase(L"正在清理更新缓存…", 98);
    removeDirRecursive(appDirW + L"\\_update");
    pumpMessages();

    // ── 6. 重启新版本 ──
    setPhase(L"更新完成，正在启动…", 100);
    Sleep(300);
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(oldExePath.c_str(), nullptr, nullptr, nullptr, FALSE,
                        0, nullptr, nullptr, &si, &pi)) {
        showError(L"更新完成，但无法自动重启，请手动启动", GetLastError());
        DestroyWindow(g_hwnd);
        return 0;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    DestroyWindow(g_hwnd);
    return 0;
}
