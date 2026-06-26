// cef_helper.exe - standalone CEF browser host
// stdin/stdout JSON line protocol, /MT CRT, MTL mode.

#include <windows.h>

#include "helper_app.h"
#include "helper_client.h"
#include "include/cef_app.h"
#include "include/cef_browser.h"

#include <iostream>
#include <string>
#include <cctype>
#include <map>

// Simple JSON parser (single-level, no nesting)
class SimpleJson {
public:
    std::map<std::string, std::string> fields;
    static SimpleJson parse(const std::string& line) {
        SimpleJson j;
        size_t i = 0;
        while (i < line.size()) {
            if (line[i] == '{' || line[i] == ',' || line[i] == '}' || std::isspace((unsigned char)line[i])) { i++; continue; }
            if (line[i] == '"') {
                i++;
                std::string key;
                while (i < line.size() && line[i] != '"') { if (line[i] == '\\') i++; if (i < line.size()) key += line[i++]; }
                i++;
                while (i < line.size() && line[i] != ':') i++; i++;
                while (i < line.size() && std::isspace((unsigned char)line[i])) i++;
                if (i < line.size() && line[i] == '"') {
                    i++;
                    std::string val;
                    while (i < line.size() && line[i] != '"') {
                        if (line[i] == '\\' && i + 1 < line.size()) { i++;
                            switch (line[i]) {
                                case '"': val += '"'; break; case '\\': val += '\\'; break;
                                case 'n': val += '\n'; break; case 'r': val += '\r'; break;
                                case 't': val += '\t'; break; default: val += '\\'; val += line[i]; break;
                            }
                        } else { val += line[i]; }
                        i++;
                    }
                    i++; j.fields[key] = val;
                }
            }
        }
        return j;
    }
};

static CefRefPtr<HelperClient> g_client;

static void handleCommand(const std::string& line) {
    auto json = SimpleJson::parse(line);
    std::string cmd = json.fields["cmd"];
    if (cmd == "open") {
        std::string url = json.fields["url"];
        std::string redirect = json.fields["redirect_uri"];
        if (url.empty()) { std::cout << R"({"status":"error","msg":"missing url"})" << "\n" << std::flush; return; }
        CefWindowInfo wi; wi.SetAsPopup(nullptr, "ShadowLauncher Login"); wi.bounds.width = 480; wi.bounds.height = 650;
        CefBrowserSettings bs;
        g_client = new HelperClient(redirect);
        CefBrowserHost::CreateBrowser(wi, g_client, CefString(url), bs, nullptr, nullptr);
    } else if (cmd == "close") {
        if (g_client && g_client->hasBrowser()) { /* OnBeforeClose handles shutdown */ }
        else { std::cout << R"({"status":"closed"})" << "\n" << std::flush; CefQuitMessageLoop(); }
    }
}

int main() {
    CefMainArgs args(GetModuleHandle(nullptr));
    CefRefPtr<HelperApp> app = new HelperApp();
    int ec = CefExecuteProcess(args, app, nullptr);
    if (ec >= 0) return ec;

    CefSettings s{}; s.size = sizeof(CefSettings); s.no_sandbox = true; s.multi_threaded_message_loop = true;

    wchar_t path[MAX_PATH]; GetModuleFileNameW(nullptr, path, MAX_PATH);
    wchar_t* slash = wcsrchr(path, L'\\'); if (slash) *slash = 0;
    char ap[MAX_PATH]; WideCharToMultiByte(CP_UTF8, 0, path, -1, ap, MAX_PATH, nullptr, nullptr);
    std::string dir(ap);

    CefString(&s.resources_dir_path).FromASCII(dir.c_str());
    CefString(&s.locales_dir_path).FromASCII((dir + "\\locales").c_str());
    CefString(&s.cache_path).FromASCII((dir + "\\cef_cache").c_str());
    CefString(&s.log_file).FromASCII((dir + "\\cef_helper.log").c_str());

    if (!CefInitialize(args, s, app, nullptr)) { std::cout << R"({"status":"error","msg":"CEF init failed"})" << "\n" << std::flush; return 1; }

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty() || line[0] != '{') continue;
        try { handleCommand(line); } catch (...) {}
    }
    g_client = nullptr; CefShutdown(); return 0;
}
