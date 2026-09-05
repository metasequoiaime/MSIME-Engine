#include "mvi_config.h"
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <toml++/toml.hpp>

namespace
{
struct AppConfigData
{
    std::string asr_provider = "siliconflow";
    std::string asr_token;
    std::string asr_endpoint = "https://api.siliconflow.cn/v1/audio/transcriptions";
    std::string polish_provider = "siliconflow";
    std::string polish_token;
    std::string polish_endpoint = "https://api.siliconflow.cn/v1/chat/completions";
    std::string language = "zh-cn";
    bool polish_text = false;
    bool notification_sound = true;
    std::string stt_provider = "cloud_siliconflow";
};

std::string get_localappdata_dir()
{
    char *buf = nullptr;
    size_t sz = 0;
    std::string localappdata;
    if (_dupenv_s(&buf, &sz, "LOCALAPPDATA") == 0 && buf != nullptr)
    {
        localappdata = buf;
        free(buf);
    }
    return localappdata;
}

AppConfigData load_config_data()
{
    AppConfigData data;
    const std::string config_path = mvi_config::GetConfigPath();
    if (config_path.empty())
    {
        return data;
    }

    try
    {
        const toml::table config = toml::parse_file(config_path);

        if (const toml::node_view<const toml::node> provider_node = config["asr_api"]["provider"]; provider_node.is_string())
        {
            data.asr_provider = provider_node.value_or(data.asr_provider);
        }
        if (const toml::node_view<const toml::node> token_node = config["asr_api"]["token"]; token_node.is_string())
        {
            data.asr_token = token_node.value_or("");
        }
        if (const toml::node_view<const toml::node> endpoint_node = config["asr_api"]["endpoint"]; endpoint_node.is_string())
        {
            data.asr_endpoint = endpoint_node.value_or(data.asr_endpoint);
        }

        if (const toml::node_view<const toml::node> provider_node = config["polish_api"]["provider"]; provider_node.is_string())
        {
            data.polish_provider = provider_node.value_or(data.polish_provider);
        }
        if (const toml::node_view<const toml::node> token_node = config["polish_api"]["token"]; token_node.is_string())
        {
            data.polish_token = token_node.value_or("");
        }
        if (const toml::node_view<const toml::node> endpoint_node = config["polish_api"]["endpoint"]; endpoint_node.is_string())
        {
            data.polish_endpoint = endpoint_node.value_or(data.polish_endpoint);
        }

        if (const toml::node_view<const toml::node> language_node = config["settings"]["language"]; language_node.is_string())
        {
            data.language = language_node.value_or(data.language);
        }
        if (const toml::node_view<const toml::node> polish_node = config["settings"]["polish_text"]; polish_node.is_boolean())
        {
            data.polish_text = polish_node.value_or(data.polish_text);
        }
        if (const toml::node_view<const toml::node> notification_node = config["settings"]["notification_sound"]; notification_node.is_boolean())
        {
            data.notification_sound = notification_node.value_or(data.notification_sound);
        }
        if (const toml::node_view<const toml::node> provider_node = config["settings"]["stt_provider"]; provider_node.is_string())
        {
            data.stt_provider = provider_node.value_or(data.stt_provider);
        }
    }
    catch (const toml::parse_error &)
    {
        return data;
    }
    catch (const std::exception &)
    {
        return data;
    }

    return data;
}

void assign_string_if_present(const nlohmann::json &obj, const char *key, std::string &target)
{
    if (obj.contains(key) && obj[key].is_string())
    {
        target = obj[key].get<std::string>();
    }
}

void assign_bool_if_present(const nlohmann::json &obj, const char *key, bool &target)
{
    if (obj.contains(key) && obj[key].is_boolean())
    {
        target = obj[key].get<bool>();
    }
}
} // namespace

std::string mvi_config::GetConfigPath()
{
    const std::string localappdata = get_localappdata_dir();
    if (localappdata.empty())
    {
        return "";
    }
    return localappdata + "\\MetasequoiaVoiceInput\\config.toml";
}

std::string mvi_config::GetApiToken(std::string api_type)
{
    const AppConfigData data = load_config_data();
    return api_type == "asr" ? data.asr_token : data.polish_token;
}

std::string mvi_config::GetLanguage()
{
    return load_config_data().language;
}

bool mvi_config::GetPolishTextEnabled()
{
    return load_config_data().polish_text;
}

std::string mvi_config::GetSTTProvider()
{
    return load_config_data().stt_provider;
}

std::string mvi_config::ReadConfigAsJson()
{
    const AppConfigData data = load_config_data();
    nlohmann::json root = {{"asr_api", {{"provider", data.asr_provider}, {"token", data.asr_token}, {"endpoint", data.asr_endpoint}}},
                           {"polish_api", {{"provider", data.polish_provider}, {"token", data.polish_token}, {"endpoint", data.polish_endpoint}}},
                           {"settings", {{"language", data.language}, {"polish_text", data.polish_text}, {"notification_sound", data.notification_sound}, {"stt_provider", data.stt_provider}}}};
    return root.dump();
}

bool mvi_config::WriteConfigFromJson(const std::string &config_json, std::string *error_message)
{
    auto set_error = [error_message](const std::string &msg) {
        if (error_message != nullptr)
        {
            *error_message = msg;
        }
    };

    AppConfigData data = load_config_data();

    nlohmann::json root;
    try
    {
        root = nlohmann::json::parse(config_json);
    }
    catch (const std::exception &e)
    {
        set_error(std::string("invalid json: ") + e.what());
        return false;
    }

    if (!root.is_object())
    {
        set_error("json root is not object");
        return false;
    }

    if (root.contains("asr_api") && root["asr_api"].is_object())
    {
        const nlohmann::json &asr = root["asr_api"];
        assign_string_if_present(asr, "provider", data.asr_provider);
        assign_string_if_present(asr, "token", data.asr_token);
        assign_string_if_present(asr, "endpoint", data.asr_endpoint);
    }

    if (root.contains("polish_api") && root["polish_api"].is_object())
    {
        const nlohmann::json &polish = root["polish_api"];
        assign_string_if_present(polish, "provider", data.polish_provider);
        assign_string_if_present(polish, "token", data.polish_token);
        assign_string_if_present(polish, "endpoint", data.polish_endpoint);
    }

    if (root.contains("settings") && root["settings"].is_object())
    {
        const nlohmann::json &settings = root["settings"];
        assign_string_if_present(settings, "language", data.language);
        assign_bool_if_present(settings, "polish_text", data.polish_text);
        assign_bool_if_present(settings, "notification_sound", data.notification_sound);
        assign_string_if_present(settings, "stt_provider", data.stt_provider);
    }

    toml::table table;
    toml::table asr;
    asr.insert_or_assign("provider", data.asr_provider);
    asr.insert_or_assign("token", data.asr_token);
    asr.insert_or_assign("endpoint", data.asr_endpoint);
    table.insert_or_assign("asr_api", asr);

    toml::table polish;
    polish.insert_or_assign("provider", data.polish_provider);
    polish.insert_or_assign("token", data.polish_token);
    polish.insert_or_assign("endpoint", data.polish_endpoint);
    table.insert_or_assign("polish_api", polish);

    toml::table settings;
    settings.insert_or_assign("language", data.language);
    settings.insert_or_assign("polish_text", data.polish_text);
    settings.insert_or_assign("notification_sound", data.notification_sound);
    settings.insert_or_assign("stt_provider", data.stt_provider);
    table.insert_or_assign("settings", settings);

    const std::string config_path = GetConfigPath();
    if (config_path.empty())
    {
        set_error("LOCALAPPDATA is empty");
        return false;
    }

    try
    {
        const std::filesystem::path fs_path(config_path);
        const std::filesystem::path parent = fs_path.parent_path();
        if (!parent.empty())
        {
            std::error_code ec;
            std::filesystem::create_directories(parent, ec);
            if (ec)
            {
                set_error(std::string("create directory failed: ") + ec.message());
                return false;
            }
        }

        std::ofstream out(config_path, std::ios::binary | std::ios::trunc);
        if (!out.is_open())
        {
            set_error("open config file failed");
            return false;
        }
        out << table;
        out.close();
        if (!out)
        {
            set_error("write config file failed");
            return false;
        }
    }
    catch (const std::exception &e)
    {
        set_error(std::string("write config exception: ") + e.what());
        return false;
    }

    if (error_message != nullptr)
    {
        error_message->clear();
    }
    return true;
}
