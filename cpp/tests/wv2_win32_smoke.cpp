// Pure Win32 WebView2 smoke test v2 — mirrors the official WebView2APISample
// exactly: Microsoft::WRL::Callback handlers + ComPtr-held environment.
// NO Qt, NO custom COM classes. Isolates whether the failure was in our
// hand-written COM handler / raw env pointer handling.
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <utility>
#include <wrl/event.h>
#include <WebView2.h>

using namespace Microsoft::WRL;

static FILE* g_out = nullptr;
static HWND g_hwnd = nullptr;
static ComPtr<ICoreWebView2Environment> g_env;
static ComPtr<ICoreWebView2Controller> g_ctrl;
static ComPtr<ICoreWebView2> g_web;
static int g_result = 2; // timeout

static void logmsg(const char* msg) {
    printf("[win32smoke] %s\n", msg);
    fflush(stdout);
    if (g_out) { fprintf(g_out, "[win32smoke] %s\n", msg); fflush(g_out); }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_SIZE:
        if (g_ctrl) {
            RECT r; GetClientRect(hwnd, &r);
            g_ctrl->put_Bounds(r);
        }
        return 0;
    }
    return DefWindowProc(hwnd, msg, w, l);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    AllocConsole();
    FILE* f; freopen_s(&f, "CONOUT$", "w", stdout);
    fopen_s(&g_out, "wv2_smoke_out.txt", "w");

    logmsg("start (WRL pattern)");
    HRESULT cohr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    char buf[128];
    sprintf_s(buf, "CoInit=0x%08X", (unsigned)cohr);
    logmsg(buf);

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"Wv2Smoke";
    RegisterClassW(&wc);
    g_hwnd = CreateWindowW(L"Wv2Smoke", L"WebView2 Win32 Smoke", WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, nullptr, nullptr, hInst, nullptr);
    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);
    logmsg("window created");

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, nullptr, nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                char buf[160];
                sprintf_s(buf, "env callback result=0x%08X env=%s", (unsigned)result, env ? "OK" : "null");
                logmsg(buf);
                if (FAILED(result) || !env) { g_result = 1; PostQuitMessage(0); return S_OK; }
                g_env = env; // AddRef — exactly like the official sample
                g_env->CreateCoreWebView2Controller(
                    g_hwnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [](HRESULT res2, ICoreWebView2Controller* controller) -> HRESULT {
                            char buf2[160];
                            sprintf_s(buf2, "controller callback result=0x%08X ctrl=%s", (unsigned)res2, controller ? "OK" : "null");
                            logmsg(buf2);
                            if (FAILED(res2) || !controller) { g_result = 1; PostQuitMessage(0); return S_OK; }
                            g_ctrl = controller;
                            controller->get_CoreWebView2(&g_web);
                            RECT r; GetClientRect(g_hwnd, &r);
                            g_ctrl->put_Bounds(r);
                            EventRegistrationToken tok;
                            g_web->add_NavigationCompleted(
                                Callback<ICoreWebView2NavigationCompletedEventHandler>(
                                    [](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                                        BOOL ok = FALSE;
                                        args->get_IsSuccess(&ok);
                                        char buf3[128];
                                        sprintf_s(buf3, "NAVIGATION COMPLETED ok=%d — WebView2 WORKS", ok == TRUE ? 1 : 0);
                                        logmsg(buf3);
                                        g_result = 0;
                                        PostQuitMessage(0);
                                        return S_OK;
                                    }).Get(),
                                &tok);
                            logmsg("Navigate https://www.bing.com");
                            HRESULT nhr = g_web->Navigate(L"https://www.bing.com");
                            sprintf_s(buf2, "Navigate returned 0x%08X", (unsigned)nhr);
                            logmsg(buf2);
                            return S_OK;
                        }).Get());
                return S_OK;
            }).Get());
    sprintf_s(buf, "CreateEnvironment returned 0x%08X", (unsigned)hr);
    logmsg(buf);
    if (FAILED(hr)) { g_result = 1; }

    SetTimer(g_hwnd, 1, 30000, nullptr);
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_TIMER && msg.wParam == 1) {
            logmsg("TIMEOUT 30s — no navigation completed");
            g_result = 2;
            PostQuitMessage(0);
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    logmsg(g_result == 0 ? "RESULT: OK" : g_result == 1 ? "RESULT: ERROR" : "RESULT: TIMEOUT");
    if (g_out) fclose(g_out);
    return g_result;
}
