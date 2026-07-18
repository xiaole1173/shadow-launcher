#include "helper_app.h"
#include <iostream>

void HelperApp::OnContextInitialized() { std::cout << R"({"status":"ready"})" << "\n" << std::flush; }

void HelperApp::OnBeforeCommandLineProcessing(const CefString& process_type, CefRefPtr<CefCommandLine> command_line) {
    command_line->AppendSwitch("do-not-de-elevate");
    command_line->AppendSwitch("disable-gpu");
}
