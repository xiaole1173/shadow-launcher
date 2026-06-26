#pragma once
#include "include/cef_client.h"
#include "include/cef_load_handler.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_display_handler.h"
#include <string>

class HelperClient : public CefClient, public CefLoadHandler, public CefLifeSpanHandler, public CefDisplayHandler {
public:
    explicit HelperClient(const std::string& redirectUri);
    CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
    void OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int httpStatusCode) override;
    void OnAddressChange(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, const CefString& url) override;
    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
    bool DoClose(CefRefPtr<CefBrowser> browser) override;
    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;
    bool hasBrowser() const { return m_browserCount > 0; }
private:
    void checkUrl(const std::string& url);
    std::string m_redirectUri;
    int m_browserCount = 0;
    bool m_codeSent = false;
    IMPLEMENT_REFCOUNTING(HelperClient);
};
