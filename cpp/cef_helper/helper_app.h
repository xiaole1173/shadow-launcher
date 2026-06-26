#pragma once
#include "include/cef_app.h"

class HelperApp : public CefApp, public CefBrowserProcessHandler {
public:
    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }
    void OnContextInitialized() override;
    void OnBeforeCommandLineProcessing(const CefString& process_type, CefRefPtr<CefCommandLine> command_line) override;
private:
    IMPLEMENT_REFCOUNTING(HelperApp);
};
