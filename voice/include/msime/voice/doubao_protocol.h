#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace metasequoia::voice
{
// Stateless wire codec. Hosts own authentication, sockets, sequencing, cancellation,
// recording and committing text. Invalid input throws std::invalid_argument;
// errors never include provider payloads, audio or credentials.
inline constexpr std::size_t doubao_chunk_samples = 3200; // 200 ms at 16 kHz.
inline constexpr std::size_t doubao_response_limit = 1024 * 1024;
struct DoubaoRequestOptions
{
    bool enable_itn = true;
    bool enable_punc = true;
    bool enable_ddc = false;
    std::string boosting_table_id;
};
struct DoubaoResponse
{
    bool last = false;
    std::uint32_t code = 0;
    std::string text;
};
// Full request is sequence 1; audio sequence magnitudes start at 2.
std::vector<std::uint8_t> make_doubao_request(const DoubaoRequestOptions &options = {});
// Finite mono float PCM in [-1, 1], encoded as little-endian signed 16-bit.
// At most one 200 ms chunk; only the final chunk may be empty.
std::vector<std::uint8_t> make_doubao_audio(const float *samples, std::size_t count, std::int32_t sequence, bool last);
// One complete WebSocket binary message, not an individual network fragment.
DoubaoResponse parse_doubao_response(const std::vector<std::uint8_t> &message);
} // namespace metasequoia::voice
