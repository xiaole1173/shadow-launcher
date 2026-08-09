// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "stdafx.h"
#include "AppWindow.h"

#include <shellapi.h>
#include <shellscalingapi.h>

// Usage:
//   wv2login.exe --client-id=<id> [--output-file=<path>] [--user-data-folder=<path>]
// Exit codes: 0 = code captured (written to --output-file), 1 = cancelled,
//             2 = error (missing args / WebView2 unavailable).
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    // DPI awareness — without it the window is bitmap-stretched on high-DPI
    // displays (blurry login page, misaligned layout).
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    std::wstring clientId;
    std::wstring outputFile;
    std::wstring userDataFolder;

    int paramCount = 0;
    LPWSTR* params = CommandLineToArgvW(GetCommandLineW(), &paramCount);
    if (params) {
        for (int i = 1; i < paramCount; ++i) {
            const std::wstring arg(params[i]);
            if (arg.rfind(L"--client-id=", 0) == 0) {
                clientId = arg.substr(arg.find(L'=') + 1);
            } else if (arg.rfind(L"--output-file=", 0) == 0) {
                outputFile = arg.substr(arg.find(L'=') + 1);
            } else if (arg.rfind(L"--user-data-folder=", 0) == 0) {
                userDataFolder = arg.substr(arg.find(L'=') + 1);
            }
        }
        LocalFree(params);
    }

    if (clientId.empty()) {
        MessageBoxW(nullptr, L"缺少 --client-id 参数", L"wv2login", MB_OK);
        return 2;
    }

    AppWindow app(clientId, outputFile, userDataFolder);
    if (!app.Initialize(hInstance, nCmdShow)) {
        return 2;
    }

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
