#pragma once

#include <string>
#include <windows.h>

namespace window_webview2
{
struct TrayMenuConfig
{
    DWORD main_thread_id = 0;
    UINT msg_toggle_record = 0;
    UINT msg_exit = 0;
    std::wstring app_name = L"MetasequoiaVoiceInput";
    std::wstring tray_menu_html_path;
    int default_width = 194;
    int default_height = 299;
};

bool InitializeTrayUi(HINSTANCE instance, const TrayMenuConfig &config);
void ShutdownTrayUi(HINSTANCE instance);
} // namespace window_webview2
