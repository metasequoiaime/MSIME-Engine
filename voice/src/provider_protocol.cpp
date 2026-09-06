#include "provider_protocol.h"
#include "stt_service.h"
#include <nlohmann/json.hpp>
#include <atomic>

namespace metasequoia::voice
{
namespace
{
constexpr std::size_t maximum_response_bytes = 1024 * 1024;
nlohmann::json parse_response(std::string_view response)
{
    if (response.size() > maximum_response_bytes)
        throw VoiceError("Voice response is too large");
    try
    {
        return nlohmann::json::parse(response.begin(), response.end());
    }
    catch (const nlohmann::json::exception &)
    {
        throw VoiceError("Invalid voice response");
    }
}
std::string text_member(const nlohmann::json &object, const char *key)
{
    if (!object.is_object())
        return {};
    const auto found = object.find(key);
    return found != object.end() && found->is_string() ? found->get<std::string>() : std::string{};
}
} // namespace
MultipartRequest make_transcription_request(std::string_view wav, std::string_view model, std::string_view language)
{
    if (wav.empty() || wav.size() > maximum_encoded_audio_bytes || model.empty())
        throw VoiceError("Audio and transcription model are required within the upload limit");
    // A boundary must not occur in any field, including the arbitrary binary payload.
    static std::atomic<unsigned long long> sequence{0};
    std::string boundary;
    do
    {
        boundary = "----MetasequoiaVoice" + std::to_string(sequence.fetch_add(1));
    } while (wav.find(boundary) != std::string_view::npos || model.find(boundary) != std::string_view::npos || language.find(boundary) != std::string_view::npos);
    MultipartRequest request{"multipart/form-data; boundary=" + boundary, {}};
    auto &body = request.body;
    body.reserve(wav.size() + model.size() + language.size() + 512);
    const auto field = [&](const char *name, std::string_view value) {
        body += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"" + name + "\"\r\n\r\n";
        body.append(value.data(), value.size());
        body += "\r\n";
    };
    field("model", model);
    if (!language.empty())
        field("language", language);
    body += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\nContent-Type: audio/wav\r\n\r\n";
    body.append(wav.data(), wav.size());
    body += "\r\n--" + boundary + "--\r\n";
    return request;
}
std::string parse_transcription(std::string_view response)
{
    const auto json = parse_response(response);
    for (const char *key : {"text", "transcription"})
    {
        auto text = text_member(json, key);
        if (!text.empty())
            return text;
    }
    if (json.is_object() && json.contains("result"))
    {
        auto text = text_member(json["result"], "text");
        if (!text.empty())
            return text;
    }
    throw VoiceError("Missing transcription text");
}
std::string make_polish_request(std::string_view model, std::string_view system_prompt, std::string_view user_message)
{
    if (model.empty() || system_prompt.empty() || user_message.empty())
        throw VoiceError("Polish model, prompt and text are required");
    try
    {
        return nlohmann::json{{"model", std::string(model)}, {"stream", false}, {"messages", {{{"role", "system"}, {"content", std::string(system_prompt)}}, {{"role", "user"}, {"content", std::string(user_message)}}}}}.dump();
    }
    catch (const nlohmann::json::exception &)
    {
        throw VoiceError("Invalid polish request text");
    }
}
std::string parse_polished_text(std::string_view response)
{
    const auto json = parse_response(response);
    try
    {
        const auto text = json.at("choices").at(0).at("message").at("content").get<std::string>();
        if (!text.empty())
            return text;
    }
    catch (const nlohmann::json::exception &)
    {
        throw VoiceError("Missing polished text");
    }
    throw VoiceError("Missing polished text");
}
} // namespace metasequoia::voice
