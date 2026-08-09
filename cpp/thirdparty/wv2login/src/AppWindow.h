// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

#include "stdafx.h"

// Minimal WebView2 embedded-login window.
// Loads the Microsoft OAuth authorize page, intercepts the
// nativeclient redirect, extracts the authorization code, writes it to
// --output-file and exits (0 = success, 1 = user cancelled, 2 = error).
class AppWindow {
public:
    AppWindow(std::wstring clientId, std::wstring outputFile, std::wstring userDataFolder);
    ~AppWindow();

    bool Initialize(HINSTANCE hInstance, int nCmdShow);

private:
    static std::string toUtf8(const std::wstring& w);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void CreateWebView2();

    // Writes result file (once) and quits the message loop.
    void Finish(int exitCode, const std::wstring& code = L"");

    HWND m_window = nullptr;
    wil::com_ptr<ICoreWebView2Environment> m_environment;
    wil::com_ptr<ICoreWebView2Controller> m_controller;
    wil::com_ptr<ICoreWebView2> m_webView;
    EventRegistrationToken m_navToken{};

    std::wstring m_clientId;
    std::wstring m_outputFile;
    std::wstring m_userDataFolder;
    bool m_finished = false;
};
