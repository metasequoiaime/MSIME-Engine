#include "cloud_stt_worker.h"
#include "wav_writer.h"
#include "http_client.h"
#include <nlohmann/json.hpp>
#include <utility>
namespace metasequoia::voice {
CloudSttWorker::CloudSttWorker(RequestOptions options) : options_(std::move(options)) {}
CloudSttWorker::CloudSttWorker(const std::string& token)
    : CloudSttWorker(RequestOptions{"https://api.siliconflow.cn/v1/audio/transcriptions", "TeleAI/TeleSpeechASR", token, 10000, {}}) {}
std::string CloudSttWorker::recognize(const std::vector<float>& pcm) {
    if (pcm.empty()) return {};
    const auto wav = WavWriter::create_wav(pcm);
    struct Resources {
        curl_mime* mime = nullptr;
        curl_slist* headers = nullptr;
        ~Resources() { curl_mime_free(mime); curl_slist_free_all(headers); }
    };
    const auto response = detail::request(options_, [&](CURL* curl) -> std::shared_ptr<void> {
        auto resource = std::make_shared<Resources>();
        resource->mime = curl_mime_init(curl);
        resource->headers = curl_slist_append(nullptr, ("Authorization: Bearer " + options_.token).c_str());
        if (!resource->mime || !resource->headers) throw VoiceError("Cannot create transcription request");
        auto* part = curl_mime_addpart(resource->mime);
        if (!part || curl_mime_name(part, "file") || curl_mime_filename(part, "audio.wav") ||
            curl_mime_type(part, "audio/wav") || curl_mime_data(part, reinterpret_cast<const char*>(wav.data()), wav.size()))
            throw VoiceError("Cannot attach audio");
        part = curl_mime_addpart(resource->mime);
        if (!part || curl_mime_name(part, "model") || curl_mime_data(part, options_.model.c_str(), CURL_ZERO_TERMINATED))
            throw VoiceError("Cannot attach model");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, resource->headers);
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, resource->mime);
        return resource;
    });
    try {
        const auto json = nlohmann::json::parse(response);
        if (!json.contains("text") || !json["text"].is_string()) throw VoiceError("Missing transcription text");
        return json["text"].get<std::string>();
    } catch (const nlohmann::json::exception&) { throw VoiceError("Invalid transcription response"); }
}
}
