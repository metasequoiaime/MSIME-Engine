#include "text_polisher.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <mutex>

namespace
{
constexpr long kPolishTimeoutMs = 3000L;

void EnsureCurlInitialized()
{
    static std::once_flag once;
    std::call_once(once, []() { curl_global_init(CURL_GLOBAL_ALL); });
}

size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string *)userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
}
} // namespace

TextPolisher::TextPolisher(const std::string &api_token, const std::string &language) : api_token_(api_token), language_(language)
{
}

std::string TextPolisher::polish(const std::string &original_text) const
{
    if (original_text.empty())
    {
        return original_text;
    }
    if (api_token_.empty())
    {
        return original_text;
    }

    EnsureCurlInitialized();

    CURL *curl = curl_easy_init();
    if (curl == nullptr)
    {
        std::cerr << "[POLISH] curl_easy_init failed" << std::endl;
        return original_text;
    }

    std::string read_buffer;
    std::string auth_header = "Authorization: Bearer " + api_token_;
    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, auth_header.c_str());

    nlohmann::json payload;
    payload["model"] = model_;
    const std::string system_prompt = "你是语音输入法的文本清洗器，只做最小必要修改。\n- 删除无意义停顿词（嗯、啊、哦、呃等）\n- 删除明显重复\n- 不润色、不扩写、不改写句式\n只输出最终文本。\n输出语言与输入保持一致，优先使用配置语言：" + language_;

    payload["messages"] = {
        {{"role", "system"}, {"content", system_prompt}},
        {{"role", "user"}, {"content", original_text}},
    };
    payload["stream"] = false;
    payload["enable_thinking"] = false;

    const std::string payload_str = payload.dump();

    curl_easy_setopt(curl, CURLOPT_URL, api_url_.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload_str.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, payload_str.size());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &read_buffer);
    // Polishing is optional. Abort after three seconds so the original ASR text
    // can be sent immediately by the caller.
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, kPolishTimeoutMs);

    const CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
    {
        std::cerr << "[POLISH] curl_easy_perform failed: " << curl_easy_strerror(res) << std::endl;
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return original_text;
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    try
    {
        const auto json = nlohmann::json::parse(read_buffer);
        if (json.contains("choices") && json["choices"].is_array() && !json["choices"].empty())
        {
            const auto &choice = json["choices"][0];
            if (choice.contains("message") && choice["message"].contains("content"))
            {
                const std::string polished = choice["message"]["content"].get<std::string>();
                if (!polished.empty())
                {
                    return polished;
                }
            }
        }
        if (json.contains("error"))
        {
            std::cerr << "[POLISH] API Error: " << json["error"].dump() << std::endl;
        }
        else
        {
            std::cerr << "[POLISH] Unexpected response: " << read_buffer << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "[POLISH] JSON Parse Error: " << e.what() << " | Raw: " << read_buffer << std::endl;
    }

    return original_text;
}
