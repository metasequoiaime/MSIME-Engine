#include "mvi_utils.h"
#include "mvi_config.h"
#include <windows.h>

// Convert UTF-8 std::string to std::wstring
std::wstring mvi_utils::utf8_to_wstring(const std::string &str)
{
    if (str.empty())
        return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

std::string mvi_utils::retrive_token()
{
    return mvi_config::GetApiToken();
}

/**
 * @brief Get the Taskbar Height
 *
 * @return int
 */
int mvi_utils::GetTaskbarHeight()
{
    APPBARDATA abd{};
    abd.cbSize = sizeof(abd);

    if (!SHAppBarMessage(ABM_GETTASKBARPOS, &abd))
        return 0;

    RECT &r = abd.rc;

    switch (abd.uEdge)
    {
    case ABE_BOTTOM:
    case ABE_TOP:
        return r.bottom - r.top; // 高度
    case ABE_LEFT:
    case ABE_RIGHT:
        return r.right - r.left; // 宽度（竖向任务栏）
    }

    return 0;
}

FLOAT GetWindowScale(HWND hwnd)
{
    UINT dpi = GetDpiForWindow(hwnd);
    FLOAT scale = dpi / 96.0f;
    return scale;
}

FLOAT mvi_utils::GetForegroundWindowScale()
{
    HWND hwnd = GetForegroundWindow();
    FLOAT scale = GetWindowScale(hwnd);
    return scale;
}

/**
 * @brief 获取当前的窗口所在的显示器的坐标信息
 *
 * @return MonitorCoordinates
 */
RECT mvi_utils::GetMonitorCoordinates()
{
    RECT coordinates = {0};
    HWND hwnd = GetForegroundWindow();
    FLOAT scale = GetWindowScale(hwnd);
    HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (!hMonitor)
    {
        return coordinates;
    }

    MONITORINFO monitorInfo = {sizeof(monitorInfo)};
    if (GetMonitorInfo(hMonitor, &monitorInfo))
    {
        int width = (monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left);
        int height = (monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top);
        coordinates.left = monitorInfo.rcMonitor.left;
        coordinates.top = monitorInfo.rcMonitor.top;
        coordinates.right = coordinates.left + width;
        coordinates.bottom = coordinates.top + height;
    }
    else
    {
    }
    return coordinates;
}

RECT mvi_utils::GetMainMonitorCoordinates()
{
    RECT coordinates{};

    HMONITOR hPrimary = MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);

    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);

    if (GetMonitorInfo(hPrimary, &mi))
    {
        coordinates.left = mi.rcMonitor.left;
        coordinates.top = mi.rcMonitor.top;
        coordinates.right = mi.rcMonitor.right;
        coordinates.bottom = mi.rcMonitor.bottom;
    }

    return coordinates;
}

std::wstring mvi_utils::resolve_asset_audio_path(std::string filename)
{
    std::string audio_path;
    char *buf = nullptr;
    size_t sz = 0;
    // Use _dupenv_s instead of getenv to avoid C4996 warning and ensure thread safety
    if (_dupenv_s(&buf, &sz, "LOCALAPPDATA") == 0 && buf != nullptr)
    {
        audio_path = std::string(buf) + "\\MetasequoiaVoiceInput\\audios\\" + filename;
        free(buf);
    }
    return utf8_to_wstring(audio_path);
}
