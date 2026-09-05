#pragma once

#include <string>

namespace mvi_config
{
std::string GetConfigPath();
std::string GetApiToken(std::string api_type = "asr");
std::string GetLanguage();
bool GetPolishTextEnabled();
std::string GetSTTProvider();
std::string ReadConfigAsJson();
bool WriteConfigFromJson(const std::string &config_json, std::string *error_message = nullptr);
} // namespace mvi_config
