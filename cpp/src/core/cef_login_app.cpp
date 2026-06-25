#include "cef_login_app.h"

void CefLoginApp::OnBeforeCommandLineProcessing(const CefString& process_type, CefRefPtr<CefCommandLine> command_line)
{
    command_line->AppendSwitch("do-not-de-elevate");
    command_line->AppendSwitch("disable-gpu");
}
