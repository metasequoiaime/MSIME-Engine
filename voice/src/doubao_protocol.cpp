#include "msime/voice/doubao_protocol.h"
#include <nlohmann/json.hpp>
#include <zlib.h>
#include <array>
#include <cmath>
#include <stdexcept>

// Wire behavior adapted from MSIME-Windows cd59d776b37ea661fc8242779a5f61b213b57296,
// server/src/voice-input/doubao_asr_client.cpp (GPL-3.0). No platform transport.
namespace metasequoia::voice
{
namespace
{
using Bytes = std::vector<std::uint8_t>;
[[noreturn]] void invalid()
{
    throw std::invalid_argument("Invalid Doubao voice packet");
}
void append32(Bytes &bytes, std::uint32_t value)
{
    for (int shift : {24, 16, 8, 0})
        bytes.push_back(static_cast<std::uint8_t>(value >> shift));
}
std::uint32_t read32(const Bytes &bytes, std::size_t &offset)
{
    if (offset > bytes.size() || bytes.size() - offset < 4)
        invalid();
    std::uint32_t value = 0;
    for (int i = 0; i < 4; ++i)
        value = (value << 8) | bytes[offset++];
    return value;
}
Bytes compress(const Bytes &input)
{
    Bytes output(compressBound(static_cast<uLong>(input.size())) + 32);
    z_stream stream{};
    if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        invalid();
    stream.next_in = const_cast<Bytef *>(input.data());
    stream.avail_in = static_cast<uInt>(input.size());
    stream.next_out = output.data();
    stream.avail_out = static_cast<uInt>(output.size());
    const int status = deflate(&stream, Z_FINISH);
    const auto size = stream.total_out;
    deflateEnd(&stream);
    if (status != Z_STREAM_END)
        invalid();
    output.resize(size);
    return output;
}
Bytes decompress(const Bytes &input)
{
    // Allocate before zlib initialization, so allocation failure cannot leak its state.
    Bytes output(doubao_response_limit + 1);
    z_stream stream{};
    if (inflateInit2(&stream, MAX_WBITS + 16) != Z_OK)
        invalid();
    stream.next_in = const_cast<Bytef *>(input.data());
    stream.avail_in = static_cast<uInt>(input.size());
    stream.next_out = output.data();
    stream.avail_out = static_cast<uInt>(output.size());
    const int status = inflate(&stream, Z_FINISH);
    const auto size = stream.total_out;
    const auto remaining = stream.avail_in;
    inflateEnd(&stream);
    if (status != Z_STREAM_END || remaining || size > doubao_response_limit)
        invalid();
    output.resize(size);
    return output;
}
Bytes packet(std::uint8_t type, std::uint8_t flags, std::int32_t sequence, const Bytes &payload)
{
    const auto compressed = compress(payload);
    Bytes result{0x11, static_cast<std::uint8_t>((type << 4) | flags), 0x11, 0x00};
    append32(result, static_cast<std::uint32_t>(sequence));
    append32(result, static_cast<std::uint32_t>(compressed.size()));
    result.insert(result.end(), compressed.begin(), compressed.end());
    return result;
}
std::string transcript(const nlohmann::json &body)
{
    if (!body.is_object() || !body.contains("result"))
        return {};
    const auto &result = body["result"];
    auto text = [](const nlohmann::json &segment) -> std::string {
        if (!segment.is_object() || !segment.contains("text"))
            return {};
        if (!segment["text"].is_string())
            invalid();
        return segment["text"].get<std::string>();
    };
    if (result.is_object())
        return text(result);
    if (!result.is_array())
        invalid();
    std::string value;
    for (const auto &segment : result)
        value += text(segment);
    return value;
}
} // namespace
std::vector<std::uint8_t> make_doubao_request(const DoubaoRequestOptions &options)
{
    if (options.boosting_table_id.size() > 8192)
        invalid();
    try
    {
        nlohmann::json request = {{"model_name", "bigmodel"}, {"enable_itn", options.enable_itn}, {"enable_punc", options.enable_punc}, {"enable_ddc", options.enable_ddc}, {"show_utterances", false}, {"result_type", "full"}};
        if (!options.boosting_table_id.empty())
            request["corpus"] = {{"boosting_table_id", options.boosting_table_id}};
        const auto json = nlohmann::json{{"user", {{"uid", "metasequoia-ime"}}}, {"audio", {{"format", "pcm"}, {"codec", "raw"}, {"rate", 16000}, {"bits", 16}, {"channel", 1}}}, {"request", std::move(request)}}.dump();
        return packet(1, 1, 1, Bytes(json.begin(), json.end()));
    }
    catch (const nlohmann::json::exception &)
    {
        invalid();
    }
}
std::vector<std::uint8_t> make_doubao_audio(const float *samples, std::size_t count, std::int32_t sequence, bool last)
{
    if (sequence < 2 || count > doubao_chunk_samples || (!samples && count) || (!last && !count))
        invalid();
    Bytes pcm;
    pcm.reserve(count * 2);
    for (std::size_t i = 0; i < count; ++i)
    {
        if (!std::isfinite(samples[i]) || std::fabs(samples[i]) > 1)
            invalid();
        const auto value = static_cast<std::uint16_t>(static_cast<std::int16_t>(samples[i] * 32767.0f));
        pcm.push_back(static_cast<std::uint8_t>(value));
        pcm.push_back(static_cast<std::uint8_t>(value >> 8));
    }
    return packet(2, last ? 3 : 1, last ? -sequence : sequence, pcm);
}
DoubaoResponse parse_doubao_response(const std::vector<std::uint8_t> &message)
{
    if (message.size() < 8 || message.size() > doubao_response_limit || (message[0] >> 4) != 1)
        invalid();
    std::size_t offset = (message[0] & 15) * 4;
    const auto type = message[1] >> 4;
    const auto flags = message[1] & 15;
    const auto serialization = message[2] >> 4;
    const auto compression = message[2] & 15;
    if (offset < 4 || offset > message.size() || flags > 7 || compression > 1 || (type != 9 && type != 15) || (serialization != 1 && type != 15))
        invalid();
    if (flags & 1)
        (void)read32(message, offset);
    if (flags & 4)
        (void)read32(message, offset);
    DoubaoResponse result;
    result.last = (flags & 2) != 0;
    if (type == 15)
        result.code = read32(message, offset);
    const auto size = read32(message, offset);
    if (size != message.size() - offset)
        invalid();
    Bytes payload(message.begin() + static_cast<std::ptrdiff_t>(offset), message.end());
    if (compression)
        payload = decompress(payload);
    // Never expose provider error bodies as recognition text.
    if (type == 15)
    {
        if (!result.code)
            invalid();
        return result;
    }
    try
    {
        // Limit nesting before building a DOM, including syntactically valid hostile JSON.
        const auto json = nlohmann::json::parse(payload.begin(), payload.end(), [](int depth, nlohmann::json::parse_event_t, nlohmann::json &) {
            if (depth > 64)
                invalid();
            return true;
        });
        if (!json.is_object())
            invalid();
        result.text = transcript(json);
        if (result.text.empty() && json.contains("payload_msg"))
            result.text = transcript(json["payload_msg"]);
        return result;
    }
    catch (const nlohmann::json::exception &)
    {
        invalid();
    }
}
} // namespace metasequoia::voice
