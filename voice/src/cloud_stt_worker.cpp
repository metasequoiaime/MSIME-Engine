#include "cloud_stt_worker.h"
#include "wav_writer.h"
#include "http_client.h"
#include "provider_protocol.h"
#include <utility>
namespace metasequoia::voice {
CloudSttWorker::CloudSttWorker(RequestOptions options) : options_(std::move(options)) {}
CloudSttWorker::CloudSttWorker(const std::string& token)
    : CloudSttWorker(RequestOptions{"https://api.siliconflow.cn/v1/audio/transcriptions", "TeleAI/TeleSpeechASR", token, 10000, {}}) {}
std::string CloudSttWorker::recognize(const std::vector<float>& pcm) {
    if (pcm.empty()) return {};
    const auto wav = WavWriter::create_wav(pcm);
    const auto payload = make_transcription_request(
        std::string_view(reinterpret_cast<const char*>(wav.data()), wav.size()), options_.model);
    const auto response = detail::request(options_, [&](CURL* curl) -> std::shared_ptr<void> {
        using Headers = std::shared_ptr<curl_slist>;
        Headers headers(curl_slist_append(nullptr, ("Authorization: Bearer " + options_.token).c_str()), curl_slist_free_all);
        if (!headers) throw VoiceError("Cannot create transcription headers");
        auto* next = curl_slist_append(headers.get(), ("Content-Type: " + payload.content_type).c_str());
        if (!next) throw VoiceError("Cannot create transcription content type");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.get());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.body.data());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(payload.body.size()));
        return headers;
    });
    return parse_transcription(response);
}
}
