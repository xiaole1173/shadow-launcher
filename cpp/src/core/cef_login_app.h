// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once
#include "include/cef_app.h"

/// Minimal CefApp implementation — mirrors cefsimple's SimpleApp.
/// Provides OnBeforeCommandLineProcessing for CEF 149+ de-elevation fix.
class CefLoginApp : public CefApp, public CefBrowserProcessHandler {
public:
    // CefApp
    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }
    void OnBeforeCommandLineProcessing(const CefString& process_type, CefRefPtr<CefCommandLine> command_line) override;

    // CefBrowserProcessHandler
    void OnContextInitialized() override {}

private:
    IMPLEMENT_REFCOUNTING(CefLoginApp);
};
