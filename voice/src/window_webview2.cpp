#include "window_webview2.h"
#include "mvi_config.h"
#include "mvi_utils.h"
#include "resource/resource.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <intsafe.h>
#include <nlohmann/json.hpp>
#include <shellapi.h>
#include <WebView2.h>
#include <winuser.h>
#include <wrl.h>
#include <fmt/xchar.h>
#include <dwmapi.h>

namespace window_webview2
{
namespace
{
constexpr UINT WM_APP_TRAY_ICON = WM_APP + 120;
constexpr UINT WM_APP_TRAY_SHOW_MENU = WM_APP + 121;
constexpr UINT WM_APP_TRAY_RESIZE_TO_MENU = WM_APP + 122;
constexpr UINT WM_APP_TRAY_HIDE_MENU = WM_APP + 123;
constexpr UINT k_tray_icon_id = 1;
constexpr wchar_t k_tray_window_class[] = L"MetasequoiaVoiceInput.TrayWindow";
constexpr wchar_t k_tray_menu_window_class[] = L"MetasequoiaVoiceInput.TrayMenuWindow";
constexpr wchar_t k_settings_window_class[] = L"MetasequoiaVoiceInput.SettingsWindow";
constexpr wchar_t k_tray_tooltip[] = L"MetasequoiaVoiceInput";
constexpr wchar_t k_menu_size_prefix[] = L"__menu_size__:";
constexpr int k_settings_min_width = 1200;
constexpr int k_settings_min_height = 800;

using CreateCoreWebView2EnvironmentWithOptionsFn = HRESULT(STDAPICALLTYPE *)(PCWSTR, PCWSTR, ICoreWebView2EnvironmentOptions *, ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *);

struct TrayUiState
{
    TrayMenuConfig config{};

    HWND tray_window = nullptr;
    HWND tray_menu_window = nullptr;
    NOTIFYICONDATAW notify_icon{};
    bool notify_icon_added = false;
    HICON tray_icon = nullptr;

    HMODULE webview2_loader = nullptr;
    CreateCoreWebView2EnvironmentWithOptionsFn create_environment = nullptr;
    Microsoft::WRL::ComPtr<ICoreWebView2Environment> environment;
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller;
    Microsoft::WRL::ComPtr<ICoreWebView2> webview;
    bool webview_creating = false;
    bool webview_ready = false;

    HWND settings_window = nullptr;
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> settings_controller;
    Microsoft::WRL::ComPtr<ICoreWebView2> settings_webview;
    bool settings_webview_creating = false;
    bool settings_webview_ready = false;
    std::wstring settings_html_path;
    std::wstring pending_settings_page;

    bool pending_show = false;
    POINT pending_show_point{};
    POINT last_anchor_pos{};
    HHOOK tray_menu_mouse_hook = nullptr;

    std::wstring tray_menu_html_path;
    std::wstring tray_menu_html_content;
    int current_width = 194;
    int current_height = 299;
};

TrayUiState g_state;

void OpenSettingsWindow(HINSTANCE instance);
LRESULT CALLBACK TrayMenuMouseHookProc(int nCode, WPARAM wParam, LPARAM lParam);

std::string WideToUtf8(const std::wstring &wstr)
{
    if (wstr.empty())
    {
        return "";
    }

    const int required_size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
    if (required_size <= 0)
    {
        return "";
    }

    std::string result(required_size, '\0');
    const int converted = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), result.data(), required_size, nullptr, nullptr);
    if (converted <= 0)
    {
        return "";
    }
    return result;
}

void SendSettingsConfigToWebView()
{
    if (g_state.settings_webview == nullptr)
    {
        return;
    }

    const std::string config_json = mvi_config::ReadConfigAsJson();
    const std::wstring script = L"window.__applyHostConfig && window.__applyHostConfig(" + mvi_utils::utf8_to_wstring(config_json) + L");";
    g_state.settings_webview->ExecuteScript(script.c_str(), nullptr);
}

void SendSettingsSaveResultToWebView(bool success, const std::string &message)
{
    if (g_state.settings_webview == nullptr)
    {
        return;
    }

    const nlohmann::json payload = {{"success", success}, {"message", message}};
    const std::wstring script = L"window.__onHostSaveResult && window.__onHostSaveResult(" + mvi_utils::utf8_to_wstring(payload.dump()) + L");";
    g_state.settings_webview->ExecuteScript(script.c_str(), nullptr);
}

void StartTrayMenuOutsideClickHook()
{
    if (g_state.tray_menu_mouse_hook != nullptr)
    {
        return;
    }

    g_state.tray_menu_mouse_hook = SetWindowsHookExW(WH_MOUSE_LL, TrayMenuMouseHookProc, GetModuleHandleW(nullptr), 0);
    if (g_state.tray_menu_mouse_hook == nullptr)
    {
        printf("[TRAY] Failed to install mouse hook: %lu\n", GetLastError());
        fflush(stdout);
    }
}

void StopTrayMenuOutsideClickHook()
{
    if (g_state.tray_menu_mouse_hook == nullptr)
    {
        return;
    }

    UnhookWindowsHookEx(g_state.tray_menu_mouse_hook);
    g_state.tray_menu_mouse_hook = nullptr;
}

void ApplyPendingSettingsPageIfNeeded()
{
    if (g_state.pending_settings_page.empty() || g_state.settings_webview == nullptr)
    {
        return;
    }

    const wchar_t *script = LR"JS(
(() => {
  const page = "__TARGET_PAGE__";
  const selector = `.nav-item[data-target="${page}"]`;
  const item = document.querySelector(selector);
  if (item) {
    item.click();
  }
})();
)JS";
    std::wstring script_text(script);
    const std::wstring token = L"__TARGET_PAGE__";
    const size_t token_pos = script_text.find(token);
    if (token_pos != std::wstring::npos)
    {
        script_text.replace(token_pos, token.size(), g_state.pending_settings_page);
    }
    g_state.settings_webview->ExecuteScript(script_text.c_str(), nullptr);

    g_state.pending_settings_page.clear();
}

std::wstring GetDefaultTrayMenuHtmlPath(const std::wstring &app_name)
{
    std::wstring html_path;
    char *buf = nullptr;
    size_t sz = 0;
    if (_dupenv_s(&buf, &sz, "LOCALAPPDATA") == 0 && buf != nullptr)
    {
        const std::string local_app_data(buf);
        free(buf);
        html_path = mvi_utils::utf8_to_wstring(local_app_data);
        html_path += L"\\";
        html_path += app_name;
        html_path += L"\\html\\tray_menu.html";
    }
    return html_path;
}

std::wstring GetDefaultSettingsHtmlPath(const std::wstring &app_name)
{
    std::wstring html_path;
    char *buf = nullptr;
    size_t sz = 0;
    if (_dupenv_s(&buf, &sz, "LOCALAPPDATA") == 0 && buf != nullptr)
    {
        const std::string local_app_data(buf);
        free(buf);
        html_path = mvi_utils::utf8_to_wstring(local_app_data);
        html_path += L"\\";
        html_path += app_name;
        html_path += L"\\html\\setting.html";
    }
    return html_path;
}

std::wstring GetFallbackSettingsHtmlPath(const std::wstring &app_name)
{
    std::wstring html_path;
    char *buf = nullptr;
    size_t sz = 0;
    if (_dupenv_s(&buf, &sz, "LOCALAPPDATA") == 0 && buf != nullptr)
    {
        const std::string local_app_data(buf);
        free(buf);
        html_path = mvi_utils::utf8_to_wstring(local_app_data);
        html_path += L"\\";
        html_path += app_name;
        html_path += L"\\html\\settings.html";
    }
    return html_path;
}

bool IsFilePathValid(const std::wstring &path)
{
    if (path.empty())
    {
        return false;
    }
    const DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::wstring BuildFileUriFromPath(std::wstring path)
{
    std::replace(path.begin(), path.end(), L'\\', L'/');
    return L"file:///" + path;
}

std::wstring ReadHtmlUtf8AsWide(const std::wstring &path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        return L"";
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return mvi_utils::utf8_to_wstring(content);
}

void ResizeWebViewBounds()
{
    if (g_state.controller == nullptr || g_state.tray_menu_window == nullptr)
    {
        return;
    }

    RECT client_rect{};
    if (GetClientRect(g_state.tray_menu_window, &client_rect))
    {
        g_state.controller->put_Bounds(client_rect);
    }
}

void ResizeSettingsWebViewBounds()
{
    if (g_state.settings_controller == nullptr || g_state.settings_window == nullptr)
    {
        return;
    }

    RECT client_rect{};
    if (GetClientRect(g_state.settings_window, &client_rect))
    {
        g_state.settings_controller->put_Bounds(client_rect);
    }
}

void HideTrayMenuWindow()
{
    StopTrayMenuOutsideClickHook();

    if (g_state.controller != nullptr)
    {
        g_state.controller->put_IsVisible(FALSE);
    }
    if (g_state.tray_menu_window != nullptr)
    {
        ShowWindow(g_state.tray_menu_window, SW_HIDE);
    }
}

void HideSettingsWindow()
{
    if (g_state.settings_controller != nullptr)
    {
        g_state.settings_controller->put_IsVisible(FALSE);
    }
    if (g_state.settings_window != nullptr)
    {
        ShowWindow(g_state.settings_window, SW_HIDE);
    }
}

void ShowTrayMenuWindowAt(POINT anchor)
{
    if (g_state.tray_menu_window == nullptr)
    {
        return;
    }

    g_state.last_anchor_pos = anchor;

    RECT work_area{};
    HMONITOR monitor = MonitorFromPoint(anchor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitor_info{};
    monitor_info.cbSize = sizeof(monitor_info);
    if (monitor != nullptr && GetMonitorInfoW(monitor, &monitor_info))
    {
        work_area = monitor_info.rcWork;
    }
    else
    {
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0);
    }

    int x = anchor.x;
    int y = anchor.y - g_state.current_height;
    if (x + g_state.current_width > work_area.right)
    {
        x = work_area.right - g_state.current_width;
    }
    if (x < work_area.left)
    {
        x = work_area.left;
    }
    if (y < work_area.top)
    {
        y = anchor.y;
    }
    if (y + g_state.current_height > work_area.bottom)
    {
        y = work_area.bottom - g_state.current_height;
    }

    SetWindowPos(g_state.tray_menu_window, HWND_TOPMOST, x, y, g_state.current_width, g_state.current_height, SWP_SHOWWINDOW);
    ShowWindow(g_state.tray_menu_window, SW_SHOWNOACTIVATE);
    SetForegroundWindow(g_state.tray_menu_window);
    StartTrayMenuOutsideClickHook();

    if (g_state.controller != nullptr)
    {
        ResizeWebViewBounds();
        g_state.controller->put_IsVisible(TRUE);
    }
}

LRESULT CALLBACK TrayMenuMouseHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0 && g_state.tray_menu_window != nullptr && IsWindowVisible(g_state.tray_menu_window))
    {
        switch (wParam)
        {
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_NCLBUTTONDOWN:
        case WM_NCRBUTTONDOWN:
        case WM_NCMBUTTONDOWN: {
            const auto *mouse_info = reinterpret_cast<const MSLLHOOKSTRUCT *>(lParam);
            if (mouse_info != nullptr)
            {
                RECT menu_rect{};
                if (GetWindowRect(g_state.tray_menu_window, &menu_rect) && !PtInRect(&menu_rect, mouse_info->pt))
                {
                    PostMessageW(g_state.tray_window, WM_APP_TRAY_HIDE_MENU, 0, 0);
                }
            }
            break;
        }
        default:
            break;
        }
    }

    return CallNextHookEx(g_state.tray_menu_mouse_hook, nCode, wParam, lParam);
}

void CenterSettingsWindow()
{
    if (g_state.settings_window == nullptr)
    {
        return;
    }

    RECT window_rect{};
    if (!GetWindowRect(g_state.settings_window, &window_rect))
    {
        return;
    }

    int window_width = window_rect.right - window_rect.left;
    int window_height = window_rect.bottom - window_rect.top;
    if (window_width <= 0 || window_height <= 0)
    {
        return;
    }

    POINT cursor_pos{};
    GetCursorPos(&cursor_pos);

    RECT work_area{};
    MONITORINFO monitor_info{};
    monitor_info.cbSize = sizeof(monitor_info);
    const HMONITOR monitor = MonitorFromPoint(cursor_pos, MONITOR_DEFAULTTONEAREST);
    if (monitor != nullptr && GetMonitorInfoW(monitor, &monitor_info))
    {
        work_area = monitor_info.rcWork;
    }
    else
    {
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0);
    }

    const int x = work_area.left + ((work_area.right - work_area.left) - window_width) / 2;
    const int y = work_area.top + ((work_area.bottom - work_area.top) - window_height) / 2;

    SetWindowPos(g_state.settings_window, HWND_TOP, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void ShowSettingsWindow()
{
    if (g_state.settings_window == nullptr)
    {
        return;
    }

    CenterSettingsWindow();
    ShowWindow(g_state.settings_window, SW_SHOW);
    HWND hWnd = g_state.settings_window;
    if (IsIconic(g_state.settings_window)) // 是否处于最小化状态
    {
        ShowWindow(hWnd, SW_RESTORE);
    }
    else
    {
        ShowWindow(hWnd, SW_SHOW);
    }
    SetForegroundWindow(g_state.settings_window);

    if (g_state.settings_controller != nullptr)
    {
        ResizeSettingsWebViewBounds();
        g_state.settings_controller->put_IsVisible(TRUE);
    }
}

void RequestContainerSize()
{
    if (g_state.webview == nullptr)
    {
        return;
    }

    const wchar_t *script = LR"JS(
(() => {
  const send = () => {
    if (!chrome.webview) return;
    const menu = document.getElementById('menuContainer');
    if (!menu) return;
    const rect = menu.getBoundingClientRect();
    const w = Math.max(1, Math.ceil(rect.width));
    const h = Math.max(1, Math.ceil(rect.height));
    chrome.webview.postMessage(`__menu_size__:${w}:${h}`);
  };
  send();
  requestAnimationFrame(send);
  setTimeout(send, 60);
})();
)JS";
    g_state.webview->ExecuteScript(script, nullptr);
}

bool ParseBodySizeMessage(const std::wstring &message, int &width, int &height)
{
    const std::wstring prefix = k_menu_size_prefix;
    if (message.rfind(prefix, 0) != 0)
    {
        return false;
    }

    const size_t sep = message.find(L':', prefix.size());
    if (sep == std::wstring::npos)
    {
        return false;
    }

    const std::wstring width_text = message.substr(prefix.size(), sep - prefix.size());
    const std::wstring height_text = message.substr(sep + 1);

    try
    {
        width = std::stoi(width_text);
        height = std::stoi(height_text);
        return width > 0 && height > 0;
    }
    catch (...)
    {
        return false;
    }
}

LRESULT CALLBACK SettingsWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_GETMINMAXINFO: {
        auto *min_max_info = reinterpret_cast<MINMAXINFO *>(lParam);
        if (min_max_info != nullptr)
        {
            min_max_info->ptMinTrackSize.x = k_settings_min_width;
            min_max_info->ptMinTrackSize.y = k_settings_min_height;
            return 0;
        }
        break;
    }
    case WM_SIZE:
        ResizeSettingsWebViewBounds();
        return 0;
    case WM_CLOSE:
        HideSettingsWindow();
        return 0;
    case WM_DESTROY:
        if (hwnd == g_state.settings_window)
        {
            g_state.settings_window = nullptr;
            g_state.settings_webview = nullptr;
            g_state.settings_controller = nullptr;
            g_state.settings_webview_ready = false;
            g_state.settings_webview_creating = false;
        }
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK TrayMenuWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE)
        {
            HideTrayMenuWindow();
        }
        return 0;
    case WM_KILLFOCUS:
        if (!IsChild(hwnd, reinterpret_cast<HWND>(wParam)))
        {
            HideTrayMenuWindow();
        }
        return 0;
    case WM_SIZE:
        ResizeWebViewBounds();
        return 0;
    case WM_CLOSE:
        HideTrayMenuWindow();
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            HideTrayMenuWindow();
            return 0;
        }
        break;
    case WM_APP_TRAY_RESIZE_TO_MENU: {
        static float scale = mvi_utils::GetForegroundWindowScale();
        const int width = static_cast<int>(static_cast<short>(LOWORD(lParam))) * scale + 10;
        const int height = static_cast<int>(static_cast<short>(HIWORD(lParam))) * scale;
        if (width > 0 && height > 0)
        {
            g_state.current_width = width;
            g_state.current_height = height;
            if (IsWindowVisible(hwnd))
            {
                ShowTrayMenuWindowAt(g_state.last_anchor_pos);
            }
        }
        return 0;
    }
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool CreateTrayMenuWindow(HINSTANCE instance)
{
    if (g_state.tray_menu_window != nullptr)
    {
        return true;
    }

    WNDCLASSW wc{};
    wc.lpfnWndProc = TrayMenuWindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = k_tray_menu_window_class;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    RegisterClassW(&wc);

    g_state.tray_menu_window = CreateWindowExW(           //
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST, //
        k_tray_menu_window_class,                         //
        L"MetasequoiaVoiceInput.TrayMenu",                //
        WS_POPUP,                                         //
        CW_USEDEFAULT,                                    //
        CW_USEDEFAULT,                                    //
        g_state.current_width,                            //
        g_state.current_height,                           //
        nullptr,                                          //
        nullptr,                                          //
        instance,                                         //
        nullptr);

    if (g_state.tray_menu_window == nullptr)
    {
        printf("[TRAY] Failed to create tray menu window: %lu\n", GetLastError());
        fflush(stdout);
        return false;
    }
    return true;
}

bool CreateSettingsWindow(HINSTANCE instance)
{
    if (g_state.settings_window != nullptr)
    {
        return true;
    }

    WNDCLASSW wc{};
    wc.lpfnWndProc = SettingsWindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = k_settings_window_class;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP_ICON));
    RegisterClassW(&wc);

    const float scale = mvi_utils::GetForegroundWindowScale();
    int initial_width = 1000;
    int initial_height = 800;
    RECT mainMonitorSize = mvi_utils::GetMainMonitorCoordinates();
    if (mainMonitorSize.right - mainMonitorSize.left < 2560)
    {
        initial_width = 800;
    }
    if (mainMonitorSize.bottom - mainMonitorSize.top < 1440)
    {
        initial_height = 600;
    }
    const int width = static_cast<int>(initial_width * scale);
    const int height = static_cast<int>(initial_height * scale);

    g_state.settings_window = CreateWindowExW( //
        WS_EX_APPWINDOW,                       //
        k_settings_window_class,               //
        L"Settings",                           //
        WS_OVERLAPPEDWINDOW,                   //
        CW_USEDEFAULT,                         //
        CW_USEDEFAULT,                         //
        width,                                 //
        height,                                //
        nullptr,                               //
        nullptr,                               //
        instance,                              //
        nullptr);

    if (g_state.settings_window == nullptr)
    {
        printf("[SETTINGS] Failed to create settings window: %lu\n", GetLastError());
        fflush(stdout);
        return false;
    }

    // 使用 DWN 允许透明
    DWM_BLURBEHIND bb = {0};
    bb.dwFlags = DWM_BB_ENABLE;
    bb.fEnable = TRUE;
    bb.hRgnBlur = nullptr;
    DwmEnableBlurBehindWindow(g_state.settings_window, &bb);
    BOOL useDarkMode = TRUE;
    DwmSetWindowAttribute(             //
        g_state.settings_window,       //
        DWMWA_USE_IMMERSIVE_DARK_MODE, //
        &useDarkMode,                  //
        sizeof(useDarkMode)            //
    );
    // 设置 settings 窗口的自定义图标
    HICON hSettingsIcon = LoadIcon(instance, MAKEINTRESOURCE(IDI_SETTINGS_ICON));
    HICON hAppIcon = LoadIcon(instance, MAKEINTRESOURCE(IDI_APP_ICON));
    SendMessage(g_state.settings_window, WM_SETICON, ICON_SMALL, (LPARAM)hSettingsIcon);
    SendMessage(g_state.settings_window, WM_SETICON, ICON_BIG, (LPARAM)hAppIcon);

    return true;
}

bool EnsureWebView2Loader()
{
    if (g_state.create_environment != nullptr)
    {
        return true;
    }
    if (g_state.webview2_loader == nullptr)
    {
        g_state.webview2_loader = LoadLibraryW(L"WebView2Loader.dll");
    }
    if (g_state.webview2_loader == nullptr)
    {
        printf("[TRAY] WebView2Loader.dll not found.\n");
        fflush(stdout);
        return false;
    }

    auto proc = GetProcAddress(g_state.webview2_loader, "CreateCoreWebView2EnvironmentWithOptions");
    if (proc == nullptr)
    {
        printf("[TRAY] CreateCoreWebView2EnvironmentWithOptions not found in WebView2Loader.dll.\n");
        fflush(stdout);
        return false;
    }

    g_state.create_environment = reinterpret_cast<CreateCoreWebView2EnvironmentWithOptionsFn>(proc);
    return true;
}

HRESULT OnControllerCreatedTrayMenuWnd(HRESULT result, ICoreWebView2Controller *controller)
{
    if (FAILED(result) || controller == nullptr)
    {
        printf("[TRAY] Failed to create WebView2 controller: 0x%08lx\n", static_cast<unsigned long>(result));
        fflush(stdout);
        return S_OK;
    }

    g_state.controller = controller;
    g_state.controller->get_CoreWebView2(&g_state.webview);
    if (g_state.webview == nullptr)
    {
        printf("[TRAY] Failed to get CoreWebView2 instance.\n");
        fflush(stdout);
        return S_OK;
    }

    Microsoft::WRL::ComPtr<ICoreWebView2Controller2> webviewController2MenuWnd;
    // Set transparent background
    if (SUCCEEDED(controller->QueryInterface(IID_PPV_ARGS(&webviewController2MenuWnd))))
    {
        COREWEBVIEW2_COLOR backgroundColor = {0, 0, 0, 0};
        webviewController2MenuWnd->put_DefaultBackgroundColor(backgroundColor);
        // 初始时强制页面的缩放为 1.0
        webviewController2MenuWnd->put_ZoomFactor(1.0);
    }

    Microsoft::WRL::ComPtr<ICoreWebView2Settings> settings;
    if (SUCCEEDED(g_state.webview->get_Settings(&settings)) && settings != nullptr)
    {
        settings->put_IsScriptEnabled(TRUE);
        settings->put_IsWebMessageEnabled(TRUE);
        settings->put_AreDefaultContextMenusEnabled(FALSE);
        settings->put_AreDevToolsEnabled(FALSE);
        settings->put_IsStatusBarEnabled(FALSE);
        // 禁用页面缩放
        settings->put_IsZoomControlEnabled(FALSE);
    }

    g_state.webview->add_WebMessageReceived(                                   //
        Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>( //
            [](ICoreWebView2 *, ICoreWebView2WebMessageReceivedEventArgs *args) -> HRESULT {
                LPWSTR raw_message = nullptr;
                if (args == nullptr || FAILED(args->TryGetWebMessageAsString(&raw_message)) || raw_message == nullptr)
                {
                    return S_OK;
                }

                const std::wstring message(raw_message);
                CoTaskMemFree(raw_message);

                int width = 0;
                int height = 0;
                if (ParseBodySizeMessage(message, width, height))
                {
                    const LPARAM size_lparam = MAKELPARAM(width, height);
                    PostMessageW(g_state.tray_menu_window, WM_APP_TRAY_RESIZE_TO_MENU, 0, size_lparam);
                    return S_OK;
                }

                if (message == L"hide")
                {
                    HideTrayMenuWindow();
                }
                else if (message == L"exit")
                {
                    printf("[TRAY] Exit command received from webview.\n");
                    OutputDebugString(fmt::format(L"[TRAY] Exit").c_str());
                    PostThreadMessage(g_state.config.main_thread_id, g_state.config.msg_exit, 0, 0);
                }
                else if (message == L"toggle_record")
                {
                    PostThreadMessage(g_state.config.main_thread_id, g_state.config.msg_toggle_record, 0, 0);
                }
                else if (message == L"open_settings")
                {
                    HideTrayMenuWindow();
                    g_state.pending_settings_page = L"api-page";
                    OpenSettingsWindow(GetModuleHandleW(nullptr));
                }
                else if (message == L"open_about")
                {
                    HideTrayMenuWindow();
                    g_state.pending_settings_page = L"about-page";
                    OpenSettingsWindow(GetModuleHandleW(nullptr));
                }
                return S_OK;
            })
            .Get(),
        nullptr);

    g_state.webview->add_NavigationCompleted( //
        Microsoft::WRL::Callback<ICoreWebView2NavigationCompletedEventHandler>([](ICoreWebView2 *, ICoreWebView2NavigationCompletedEventArgs *args) -> HRESULT {
            BOOL success = FALSE;
            if (args != nullptr)
            {
                args->get_IsSuccess(&success);
            }
            if (success)
            {
                RequestContainerSize();
            }
            return S_OK;
        }).Get(),
        nullptr);

    g_state.controller->put_IsVisible(FALSE);
    ResizeWebViewBounds();

    g_state.webview->NavigateToString(g_state.tray_menu_html_content.c_str());
    g_state.webview_ready = true;

    if (g_state.pending_show)
    {
        g_state.pending_show = false;
        ShowTrayMenuWindowAt(g_state.pending_show_point);
    }
    return S_OK;
}

HRESULT OnEnvironmentCreated(HRESULT result, ICoreWebView2Environment *environment)
{
    g_state.webview_creating = false;
    if (FAILED(result) || environment == nullptr)
    {
        printf("[TRAY] Failed to create WebView2 environment: 0x%08lx\n", static_cast<unsigned long>(result));
        fflush(stdout);
        return S_OK;
    }

    g_state.environment = environment;
    return g_state.environment->CreateCoreWebView2Controller(                                //
        g_state.tray_menu_window,                                                            //
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>( //
            [](HRESULT controller_result, ICoreWebView2Controller *controller) -> HRESULT {  //
                return OnControllerCreatedTrayMenuWnd(controller_result, controller);        //
            })
            .Get());
}

void CreateTrayMenuWebViewIfNeeded()
{
    if (g_state.webview_ready || g_state.webview_creating || g_state.tray_menu_window == nullptr)
    {
        return;
    }
    if (!EnsureWebView2Loader())
    {
        return;
    }

    g_state.webview_creating = true;
    const HRESULT hr = g_state.create_environment(                                            //
        nullptr,                                                                              //
        nullptr,                                                                              //
        nullptr,                                                                              //
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>( //
            [](HRESULT result, ICoreWebView2Environment *environment) -> HRESULT {            //
                return OnEnvironmentCreated(result, environment);                             //
            })                                                                                //
            .Get());                                                                          //

    if (FAILED(hr))
    {
        g_state.webview_creating = false;
        printf("[TRAY] Failed to start WebView2 creation: 0x%08lx\n", static_cast<unsigned long>(hr));
        fflush(stdout);
    }
}

void CreateSettingsWebViewIfNeeded()
{
    if (g_state.settings_webview_ready || g_state.settings_webview_creating || g_state.settings_window == nullptr)
    {
        return;
    }
    if (!EnsureWebView2Loader())
    {
        return;
    }
    if (g_state.environment == nullptr)
    {
        printf("[SETTINGS] WebView2 environment not ready.\n");
        fflush(stdout);
        return;
    }
    if (!IsFilePathValid(g_state.settings_html_path))
    {
        printf("[SETTINGS] settings html not found: %ls\n", g_state.settings_html_path.c_str());
        fflush(stdout);
        return;
    }

    g_state.settings_webview_creating = true;
    const HRESULT hr = g_state.environment->CreateCoreWebView2Controller(                    //
        g_state.settings_window,                                                             //
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>( //
            [](HRESULT controller_result, ICoreWebView2Controller *controller) -> HRESULT {  //
                g_state.settings_webview_creating = false;
                if (FAILED(controller_result) || controller == nullptr)
                {
                    printf("[SETTINGS] Failed to create WebView2 controller: 0x%08lx\n", static_cast<unsigned long>(controller_result));
                    fflush(stdout);
                    return S_OK;
                }

                g_state.settings_controller = controller;
                g_state.settings_controller->get_CoreWebView2(&g_state.settings_webview);
                if (g_state.settings_webview == nullptr)
                {
                    printf("[SETTINGS] Failed to get CoreWebView2 instance.\n");
                    fflush(stdout);
                    return S_OK;
                }

                Microsoft::WRL::ComPtr<ICoreWebView2Settings> settings;
                if (SUCCEEDED(g_state.settings_webview->get_Settings(&settings)) && settings != nullptr)
                {
                    settings->put_IsScriptEnabled(TRUE);
                    settings->put_IsWebMessageEnabled(TRUE);
                    settings->put_AreDefaultContextMenusEnabled(FALSE);
                    settings->put_AreDevToolsEnabled(FALSE);
                    settings->put_IsStatusBarEnabled(FALSE);
                    // 禁用页面缩放
                    settings->put_IsZoomControlEnabled(FALSE);
                }

                g_state.settings_webview->add_WebMessageReceived(                                        //
                    Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(               //
                        [](ICoreWebView2 *, ICoreWebView2WebMessageReceivedEventArgs *args) -> HRESULT { //
                            if (args == nullptr)
                            {
                                return S_OK;
                            }

                            LPWSTR raw_message_json = nullptr;
                            if (FAILED(args->get_WebMessageAsJson(&raw_message_json)) || raw_message_json == nullptr)
                            {
                                return S_OK;
                            }

                            const std::wstring message_wide(raw_message_json);
                            CoTaskMemFree(raw_message_json);

                            try
                            {
                                const nlohmann::json message = nlohmann::json::parse(WideToUtf8(message_wide));
                                if (!message.is_object())
                                {
                                    return S_OK;
                                }

                                const std::string action = message.value("action", "");
                                if (action == "load_settings")
                                {
                                    SendSettingsConfigToWebView();
                                    return S_OK;
                                }

                                if (action == "save_settings")
                                {
                                    if (!message.contains("config") || !message["config"].is_object())
                                    {
                                        SendSettingsSaveResultToWebView(false, "保存失败: 配置格式无效");
                                        return S_OK;
                                    }

                                    std::string error_message;
                                    const bool ok = mvi_config::WriteConfigFromJson(message["config"].dump(), &error_message);
                                    if (!ok)
                                    {
                                        const std::string msg = error_message.empty() ? "保存失败: 未知错误" : "保存失败: " + error_message;
                                        SendSettingsSaveResultToWebView(false, msg);
                                        return S_OK;
                                    }

                                    SendSettingsSaveResultToWebView(true, "设置已保存到 config.toml");
                                    SendSettingsConfigToWebView();
                                }
                            }
                            catch (const std::exception &e)
                            {
                                SendSettingsSaveResultToWebView(false, std::string("保存失败: ") + e.what());
                            }
                            return S_OK;
                        })
                        .Get(),
                    nullptr);

                Microsoft::WRL::ComPtr<ICoreWebView2Controller2> webviewController2SettingsWnd;
                // Set transparent background
                if (SUCCEEDED(controller->QueryInterface(IID_PPV_ARGS(&webviewController2SettingsWnd))))
                {
                    COREWEBVIEW2_COLOR backgroundColor = {0, 0, 0, 0};
                    webviewController2SettingsWnd->put_DefaultBackgroundColor(backgroundColor);
                    // 初始时强制页面的缩放为 1.0
                    webviewController2SettingsWnd->put_ZoomFactor(1.0);
                }

                g_state.settings_webview->add_NewWindowRequested(                                        //
                    Microsoft::WRL::Callback<ICoreWebView2NewWindowRequestedEventHandler>(               //
                        [](ICoreWebView2 *, ICoreWebView2NewWindowRequestedEventArgs *args) -> HRESULT { //
                            if (args == nullptr)
                            {
                                return S_OK;
                            }

                            LPWSTR raw_uri = nullptr;
                            if (FAILED(args->get_Uri(&raw_uri)) || raw_uri == nullptr)
                            {
                                args->put_Handled(TRUE);
                                return S_OK;
                            }

                            ShellExecuteW(nullptr, L"open", raw_uri, nullptr, nullptr, SW_SHOWNORMAL);
                            CoTaskMemFree(raw_uri);
                            args->put_Handled(TRUE);
                            return S_OK;
                        })
                        .Get(),
                    nullptr);

                const std::wstring settings_url = BuildFileUriFromPath(g_state.settings_html_path);
                g_state.settings_webview->add_NavigationCompleted(                          //
                    Microsoft::WRL::Callback<ICoreWebView2NavigationCompletedEventHandler>( //
                        [](ICoreWebView2 *, ICoreWebView2NavigationCompletedEventArgs *args) -> HRESULT {
                            BOOL success = FALSE;
                            if (args != nullptr)
                            {
                                args->get_IsSuccess(&success);
                            }
                            if (success)
                            {
                                SendSettingsConfigToWebView();
                                ApplyPendingSettingsPageIfNeeded();
                            }
                            return S_OK;
                        })
                        .Get(),
                    nullptr);
                g_state.settings_webview->Navigate(settings_url.c_str());
                ResizeSettingsWebViewBounds();
                g_state.settings_controller->put_IsVisible(TRUE);
                g_state.settings_webview_ready = true;
                ApplyPendingSettingsPageIfNeeded();
                return S_OK;
            })
            .Get());

    if (FAILED(hr))
    {
        g_state.settings_webview_creating = false;
        printf("[SETTINGS] Failed to start WebView2 creation: 0x%08lx\n", static_cast<unsigned long>(hr));
        fflush(stdout);
    }
}

void OpenSettingsWindow(HINSTANCE instance)
{
    if (!CreateSettingsWindow(instance))
    {
        return;
    }

    ShowSettingsWindow();
    if (!g_state.settings_webview_ready)
    {
        CreateSettingsWebViewIfNeeded();
        return;
    }

    SendSettingsConfigToWebView();
    ApplyPendingSettingsPageIfNeeded();
}

void RequestShowTrayMenu()
{
    POINT cursor{};
    GetCursorPos(&cursor);

    if (g_state.webview_ready)
    {
        ShowTrayMenuWindowAt(cursor);
        return;
    }
    g_state.pending_show = true;
    g_state.pending_show_point = cursor;
    CreateTrayMenuWebViewIfNeeded();
}

LRESULT CALLBACK TrayWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_APP_TRAY_ICON:
        if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_CONTEXTMENU)
        {
            PostMessageW(hwnd, WM_APP_TRAY_SHOW_MENU, 0, 0);
            return 0;
        }
        if (LOWORD(lParam) == WM_LBUTTONDBLCLK && g_state.config.msg_toggle_record != 0)
        {
            PostThreadMessage(g_state.config.main_thread_id, g_state.config.msg_toggle_record, 0, 0);
            return 0;
        }
        break;
    case WM_APP_TRAY_SHOW_MENU:
        RequestShowTrayMenu();
        return 0;
    case WM_APP_TRAY_HIDE_MENU:
        HideTrayMenuWindow();
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}
} // namespace

bool InitializeTrayUi(HINSTANCE instance, const TrayMenuConfig &config)
{
    g_state = {};
    g_state.config = config;
    float scale = mvi_utils::GetForegroundWindowScale();
    g_state.current_width = config.default_width * scale;
    g_state.current_height = config.default_height * scale;

    g_state.tray_menu_html_path = config.tray_menu_html_path.empty() ? GetDefaultTrayMenuHtmlPath(config.app_name) : config.tray_menu_html_path;
    g_state.settings_html_path = GetDefaultSettingsHtmlPath(config.app_name);
    if (!IsFilePathValid(g_state.settings_html_path))
    {
        const std::wstring fallback_settings_path = GetFallbackSettingsHtmlPath(config.app_name);
        if (IsFilePathValid(fallback_settings_path))
        {
            g_state.settings_html_path = fallback_settings_path;
        }
    }
    if (g_state.tray_menu_html_path.empty())
    {
        printf("[TRAY] tray_menu.html path is empty.\n");
        fflush(stdout);
        return false;
    }

    const DWORD attrs = GetFileAttributesW(g_state.tray_menu_html_path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0)
    {
        printf("[TRAY] tray_menu.html not found: %ls\n", g_state.tray_menu_html_path.c_str());
        fflush(stdout);
        return false;
    }

    g_state.tray_menu_html_content = ReadHtmlUtf8AsWide(g_state.tray_menu_html_path);
    if (g_state.tray_menu_html_content.empty())
    {
        printf("[TRAY] Failed to read tray_menu.html content: %ls\n", g_state.tray_menu_html_path.c_str());
        fflush(stdout);
        return false;
    }

    WNDCLASSW wc{};
    wc.lpfnWndProc = TrayWindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = k_tray_window_class;
    RegisterClassW(&wc);

    g_state.tray_window = CreateWindowExW(0, k_tray_window_class, L"MetasequoiaVoiceInput.TrayHost", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, instance, nullptr);
    if (g_state.tray_window == nullptr)
    {
        printf("[TRAY] Failed to create tray host window: %lu\n", GetLastError());
        fflush(stdout);
        return false;
    }
    if (!CreateTrayMenuWindow(instance))
    {
        return false;
    }

    g_state.tray_icon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE));
    if (g_state.tray_icon == nullptr)
    {
        g_state.tray_icon = LoadIconW(nullptr, IDI_APPLICATION);
    }

    g_state.notify_icon = {};
    g_state.notify_icon.cbSize = sizeof(NOTIFYICONDATAW);
    g_state.notify_icon.hWnd = g_state.tray_window;
    g_state.notify_icon.uID = k_tray_icon_id;
    g_state.notify_icon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_state.notify_icon.uCallbackMessage = WM_APP_TRAY_ICON;
    g_state.notify_icon.hIcon = g_state.tray_icon;
    wcscpy_s(g_state.notify_icon.szTip, k_tray_tooltip);

    if (!Shell_NotifyIconW(NIM_ADD, &g_state.notify_icon))
    {
        printf("[TRAY] Failed to add tray icon.\n");
        fflush(stdout);
        return false;
    }

    g_state.notify_icon_added = true;
    g_state.notify_icon.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &g_state.notify_icon);

    // Warm up WebView2 in background so the first tray menu open is responsive.
    CreateTrayMenuWebViewIfNeeded();

    printf("[TRAY] Tray icon initialized.\n");
    fflush(stdout);
    return true;
}

void ShutdownTrayUi(HINSTANCE instance)
{
    HideTrayMenuWindow();
    HideSettingsWindow();

    if (g_state.notify_icon_added)
    {
        Shell_NotifyIconW(NIM_DELETE, &g_state.notify_icon);
        g_state.notify_icon_added = false;
    }

    g_state.webview = nullptr;
    g_state.controller = nullptr;
    g_state.settings_webview = nullptr;
    g_state.settings_controller = nullptr;
    g_state.environment = nullptr;

    if (g_state.tray_menu_window != nullptr)
    {
        DestroyWindow(g_state.tray_menu_window);
        g_state.tray_menu_window = nullptr;
    }
    if (g_state.settings_window != nullptr)
    {
        DestroyWindow(g_state.settings_window);
        g_state.settings_window = nullptr;
    }
    if (g_state.tray_window != nullptr)
    {
        DestroyWindow(g_state.tray_window);
        g_state.tray_window = nullptr;
    }
    if (g_state.webview2_loader != nullptr)
    {
        FreeLibrary(g_state.webview2_loader);
        g_state.webview2_loader = nullptr;
        g_state.create_environment = nullptr;
    }

    UnregisterClassW(k_tray_menu_window_class, instance);
    UnregisterClassW(k_settings_window_class, instance);
    UnregisterClassW(k_tray_window_class, instance);
}
} // namespace window_webview2
