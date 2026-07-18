#include "helper_client.h"
#include "include/cef_app.h"
#include "include/cef_browser.h"
#include <iostream>

HelperClient::HelperClient(const std::string& redirectUri) : m_redirectUri(redirectUri) {}

void HelperClient::OnAfterCreated(CefRefPtr<CefBrowser> browser) { m_browserCount++; }
bool HelperClient::DoClose(CefRefPtr<CefBrowser> browser) { return false; }

void HelperClient::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
    m_browserCount--;
    if (m_browserCount == 0) {
        std::cout << (m_codeSent ? R"({"status":"closed"})" : R"({"status":"closed","msg":"window closed without code"})") << "\n" << std::flush;
        CefQuitMessageLoop();
    }
}

void HelperClient::OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int httpStatusCode) {
    if (frame->IsMain()) checkUrl(frame->GetURL().ToString());
}

void HelperClient::OnAddressChange(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, const CefString& url) {
    if (frame->IsMain()) checkUrl(url.ToString());
}

void HelperClient::checkUrl(const std::string& url) {
    if (m_codeSent || url.rfind(m_redirectUri, 0) != 0) return;
    auto pos = url.find("code=");
    if (pos == std::string::npos) return;
    std::string code = url.substr(pos + 5);
    auto amp = code.find('&');
    if (amp != std::string::npos) code = code.substr(0, amp);
    if (code.empty()) return;
    auto hex = [](char c) -> char { if (c>='0'&&c<='9') return c-'0'; if (c>='a'&&c<='f') return c-'a'+10; if (c>='A'&&c<='F') return c-'A'+10; return 0; };
    std::string dec;
    for (size_t i = 0; i < code.size(); i++) {
        if (code[i]=='%' && i+2<code.size()) { dec += (hex(code[i+1])<<4)|hex(code[i+2]); i+=2; }
        else if (code[i]=='+') dec += ' ';
        else dec += code[i];
    }
    std::string safe;
    for (char c : dec) {
        if (c=='"') safe += "\\\""; else if (c=='\\') safe += "\\\\";
        else if (c=='\n'||c=='\r') continue; else safe += c;
    }
    std::cout << "{\"status\":\"code\",\"code\":\"" << safe << "\"}\n" << std::flush;
    m_codeSent = true;
}
