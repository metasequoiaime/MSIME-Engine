#pragma once
#include <cstddef>
#include <string>
#include <string_view>

namespace metasequoia::voice {
// Platform transports may use these codecs without duplicating provider wire formats.
// Audio is an already encoded WAV; recording duration/format policy belongs to the host.
inline constexpr std::size_t maximum_encoded_audio_bytes = 20 * 1024 * 1024;
struct MultipartRequest {
    std::string content_type;
    std::string body;
};
MultipartRequest make_transcription_request(std::string_view wav, std::string_view model,
                                           std::string_view language = {});
std::string parse_transcription(std::string_view response);
// user_message is sent verbatim. The caller owns prompt policy and any ASR delimiters.
std::string make_polish_request(std::string_view model, std::string_view system_prompt,
                                std::string_view user_message);
std::string parse_polished_text(std::string_view response);
} // namespace metasequoia::voice
