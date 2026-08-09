// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "stdafx.h"
#include "AppWindow.h"

#include <shlwapi.h>
#include <shlobj.h>
#include <WebView2EnvironmentOptions.h>

using namespace Microsoft::WRL;

namespace {

constexpr wchar_t kClassName[] = L"ShadowWv2LoginWindow";
constexpr wchar_t kAuthUrl[] =
    L"https://login.microsoftonline.com/consumers/oauth2/v2.0/authorize";
constexpr wchar_t kRedirectPrefix[] =
    L"https://login.microsoftonline.com/common/oauth2/nativeclient";
constexpr wchar_t kRedirectUriEncoded[] =
    L"https%3A%2F%2Flogin.microsoftonline.com%2Fcommon%2Foauth2%2Fnativeclient";

} // namespace

std::string AppWindow::toUtf8(const std::wstring& w) {
    if (w.empty()) {
        return {};
    }
    const int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(len > 0 ? len - 1 : 0), '\0');
    if (len > 1) {
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, out.data(), len, nullptr, nullptr);
    }
    return out;
}

AppWindow::AppWindow(std::wstring clientId, std::wstring outputFile, std::wstring userDataFolder)
    : m_clientId(std::move(clientId)),
      m_outputFile(std::move(outputFile)),
      m_userDataFolder(std::move(userDataFolder)) {}

AppWindow::~AppWindow() {}

LRESULT CALLBACK AppWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    AppWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<AppWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<AppWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) {
        switch (msg) {
        case WM_SIZE:
            if (self->m_controller) {
                RECT r{};
                GetClientRect(hwnd, &r);
                self->m_controller->put_Bounds(r);
            }
            return 0;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            // User closed the window without completing login → cancelled.
            self->Finish(1);
            PostQuitMessage(1);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool AppWindow::Initialize(HINSTANCE hInstance, int nCmdShow) {
    WNDCLASSEXW wc{sizeof(WNDCLASSEXW)};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kClassName;
    if (!RegisterClassExW(&wc)) {
        return false;
    }

    m_window = CreateWindowExW(WS_EX_TOPMOST, kClassName, L"Microsoft 登录",
                               WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                               CW_USEDEFAULT, CW_USEDEFAULT, 480, 650, nullptr, nullptr,
                               hInstance, this);
    if (!m_window) {
        return false;
    }
    ShowWindow(m_window, nCmdShow);
    UpdateWindow(m_window);

    // Ensure the user data folder exists before WebView2 tries to use it.
    if (!m_userDataFolder.empty()) {
        SHCreateDirectoryExW(nullptr, m_userDataFolder.c_str(), nullptr);
    }

    CreateWebView2();
    return true;
}

void AppWindow::CreateWebView2() {
    const HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, m_userDataFolder.empty() ? nullptr : m_userDataFolder.c_str(), nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(result) || !env) {
                    MessageBoxW(m_window, L"WebView2 运行时不可用", L"登录", MB_OK);
                    Finish(2);
                    return S_OK;
                }
                m_environment = env; // AddRef — kept alive for async controller creation
                m_environment->CreateCoreWebView2Controller(
                    m_window,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this](HRESULT res2, ICoreWebView2Controller* controller) -> HRESULT {
                            if (FAILED(res2) || !controller) {
                                MessageBoxW(m_window, L"WebView2 控制器创建失败", L"登录", MB_OK);
                                Finish(2);
                                return S_OK;
                            }
                            m_controller = controller;
                            controller->get_CoreWebView2(&m_webView);
                            RECT r{};
                            GetClientRect(m_window, &r);
                            m_controller->put_Bounds(r);

                            // Standard desktop Edge UA — WebView2's default UA
                            // ("Cortana ...") makes login.live.com serve a
                            // different responsive layout (extra close button).
                            wil::com_ptr<ICoreWebView2Settings> settings;
                            if (SUCCEEDED(m_webView->get_Settings(&settings))) {
                                wil::com_ptr<ICoreWebView2Settings2> settings2;
                                if (SUCCEEDED(settings->QueryInterface(IID_PPV_ARGS(&settings2)))) {
                                    settings2->put_UserAgent(
                                        L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                                        L"AppleWebKit/537.36 (KHTML, like Gecko) "
                                        L"Chrome/131.0.0.0 Safari/537.36 Edg/131.0.0.0");
                                }
                            }

                            // Intercept the nativeclient redirect and capture the code.
                            m_webView->add_NavigationStarting(
                                Callback<ICoreWebView2NavigationStartingEventHandler>(
                                    [this](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                                        wil::unique_cotaskmem_string uri;
                                        if (SUCCEEDED(args->get_Uri(&uri)) && uri) {
                                            const std::wstring url(uri.get());
                                            if (url.rfind(kRedirectPrefix, 0) == 0) {
                                                args->put_Cancel(TRUE);
                                                const size_t pos = url.find(L"code=");
                                                if (pos != std::wstring::npos) {
                                                    std::wstring code = url.substr(pos + 5);
                                                    const size_t amp = code.find(L'&');
                                                    if (amp != std::wstring::npos) {
                                                        code = code.substr(0, amp);
                                                    }
                                                    Finish(0, code);
                                                }
                                            }
                                        }
                                        return S_OK;
                                    }).Get(),
                                &m_navToken);

                            std::wstring url = kAuthUrl;
                            url += L"?client_id=" + m_clientId;
                            url += L"&response_type=code";
                            url += L"&redirect_uri=" + std::wstring(kRedirectUriEncoded);
                            url += L"&scope=XboxLive.signin%20offline_access";
                            url += L"&prompt=select_account";
                            m_webView->Navigate(url.c_str());
                            return S_OK;
                        }).Get());
                return S_OK;
            }).Get());

    if (FAILED(hr)) {
        MessageBoxW(m_window, L"WebView2 环境创建失败", L"登录", MB_OK);
        Finish(2);
    }
}

void AppWindow::Finish(int exitCode, const std::wstring& code) {
    if (m_finished) {
        return;
    }
    m_finished = true;

    if (!m_outputFile.empty()) {
        // Write UTF-8 (code is ASCII; UTF-8 keeps the reader side simple).
        std::string utf8;
        if (code.empty()) {
            utf8 = "cancel";
        } else {
            utf8 = "code=" + toUtf8(code);
        }
        HANDLE hFile = CreateFileW(m_outputFile.c_str(), GENERIC_WRITE, 0, nullptr,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            WriteFile(hFile, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
            CloseHandle(hFile);
        }
    }
    PostQuitMessage(exitCode);
}
