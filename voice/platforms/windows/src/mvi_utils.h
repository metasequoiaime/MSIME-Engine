//
// MetasequoiaVoiceInput Utils
//
#pragma once

#include <string>
#include <Windows.h>

// Convert UTF-8 std::string to std::wstring
namespace mvi_utils
{
std::wstring utf8_to_wstring(const std::string &str);
std::string retrive_token();
int GetTaskbarHeight();
RECT GetMonitorCoordinates();
RECT GetMainMonitorCoordinates();
std::wstring resolve_asset_audio_path(std::string filename);
FLOAT GetForegroundWindowScale();
} // namespace mvi_utils
