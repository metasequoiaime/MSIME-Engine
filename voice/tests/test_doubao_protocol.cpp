#include "msime/voice/doubao_protocol.h"
#include <nlohmann/json.hpp>
#include <zlib.h>
#include <algorithm>
#include <cassert>
#include <limits>
#include <stdexcept>

using namespace metasequoia::voice;
using Bytes = std::vector<std::uint8_t>;
// Independent fixture framing; outbound packets are decoded with zlib and JSON,
// not with the codec under test.
static void word(Bytes &bytes, std::uint32_t value)
{
    bytes.push_back(static_cast<std::uint8_t>(value >> 24));
    bytes.push_back(static_cast<std::uint8_t>(value >> 16));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
    bytes.push_back(static_cast<std::uint8_t>(value));
}
static Bytes gzip(const std::string &value)
{
    Bytes result(compressBound(static_cast<uLong>(value.size())) + 32);
    z_stream state{};
    assert(deflateInit2(&state, 6, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) == Z_OK);
    state.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(value.data()));
    state.avail_in = static_cast<uInt>(value.size());
    state.next_out = result.data();
    state.avail_out = static_cast<uInt>(result.size());
    assert(deflate(&state, Z_FINISH) == Z_STREAM_END);
    result.resize(state.total_out);
    assert(deflateEnd(&state) == Z_OK);
    return result;
}
static Bytes unpack(const Bytes &packet)
{
    assert(packet.size() >= 12);
    const auto length = (std::uint32_t(packet[8]) << 24) | (std::uint32_t(packet[9]) << 16) |
                        (std::uint32_t(packet[10]) << 8) | packet[11];
    assert(length == packet.size() - 12);
    Bytes result(20000);
    z_stream state{};
    assert(inflateInit2(&state, 31) == Z_OK);
    state.next_in = const_cast<Bytef *>(packet.data() + 12);
    state.avail_in = length;
    state.next_out = result.data();
    state.avail_out = static_cast<uInt>(result.size());
    assert(inflate(&state, Z_FINISH) == Z_STREAM_END && state.avail_in == 0);
    result.resize(state.total_out);
    assert(inflateEnd(&state) == Z_OK);
    return result;
}
static Bytes response(std::string text, bool compressed = false, std::uint8_t flags = 3)
{
    Bytes result{0x11, static_cast<std::uint8_t>(0x90 | flags), static_cast<std::uint8_t>(compressed ? 0x11 : 0x10), 0};
    if (flags & 1) word(result, flags & 2 ? 0xfffffffe : 2);
    if (flags & 4) word(result, 42);
    const auto body = compressed ? gzip(text) : Bytes(text.begin(), text.end());
    word(result, static_cast<std::uint32_t>(body.size()));
    result.insert(result.end(), body.begin(), body.end());
    return result;
}
template<class F> static void rejects(F f)
{
    bool rejected = false;
    try { f(); }
    catch (const std::invalid_argument &error)
    {
        rejected = true;
        assert(std::string(error.what()) == "Invalid Doubao voice packet");
    }
    assert(rejected);
}
int main()
{
    auto request = make_doubao_request();
    assert(std::equal(request.begin(), request.begin() + 8, Bytes{0x11, 0x11, 0x11, 0, 0, 0, 0, 1}.begin()));
    auto raw = unpack(request);
    auto json = nlohmann::json::parse(raw);
    assert(json["user"]["uid"] == "metasequoia-ime");
    assert(json["audio"] == (nlohmann::json{{"format", "pcm"}, {"codec", "raw"}, {"rate", 16000}, {"bits", 16}, {"channel", 1}}));
    assert(json["request"]["model_name"] == "bigmodel" && json["request"]["enable_itn"] == true);
    assert(json["request"]["enable_punc"] == true && json["request"]["enable_ddc"] == false);
    assert(!json["request"].contains("corpus") && json["request"]["show_utterances"] == false);
    assert(json["request"]["result_type"] == "full");
    raw = unpack(make_doubao_request({false, false, true, "synthetic-table"}));
    json = nlohmann::json::parse(raw);
    assert(json["request"]["enable_itn"] == false && json["request"]["enable_punc"] == false);
    assert(json["request"]["enable_ddc"] == true && json["request"]["corpus"]["boosting_table_id"] == "synthetic-table");
    rejects([] { make_doubao_request({true, true, false, std::string(8193, 'x')}); });
    rejects([] { make_doubao_request({true, true, false, std::string(1, '\xff')}); });
    const float samples[]{-1, -0.5f, 0, 0.5f, 1};
    auto audio = make_doubao_audio(samples, 5, 2, false);
    assert(std::equal(audio.begin(), audio.begin() + 8, Bytes{0x11, 0x21, 0x11, 0, 0, 0, 0, 2}.begin()));
    assert(unpack(audio) == (Bytes{1, 128, 1, 192, 0, 0, 255, 63, 255, 127}));
    audio = make_doubao_audio(nullptr, 0, 3, true);
    assert(std::equal(audio.begin(), audio.begin() + 8, Bytes{0x11, 0x23, 0x11, 0, 255, 255, 255, 253}.begin()));
    assert(unpack(audio).empty());
    std::vector<float> chunk(doubao_chunk_samples, 0);
    assert(unpack(make_doubao_audio(chunk.data(), chunk.size(), 2, true)).size() == 6400);
    rejects([&] { make_doubao_audio(chunk.data(), chunk.size() + 1, 2, false); });
    rejects([] { make_doubao_audio(nullptr, 1, 2, false); });
    rejects([] { make_doubao_audio(nullptr, 0, 2, false); });
    for (int sequence : {0, 1, -2, std::numeric_limits<int>::min()})
        rejects([&] { make_doubao_audio(nullptr, 0, sequence, true); });
    for (float sample : {1.01f, -1.01f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        rejects([&] { make_doubao_audio(&sample, 1, 2, true); });
    for (bool compressed : {false, true})
    {
        for (std::uint8_t flags = 0; flags < 8; ++flags)
        {
            const auto parsed = parse_doubao_response(response(R"({"result":{"text":"synthetic"}})", compressed, flags));
            assert(parsed.text == "synthetic" && parsed.code == 0 && parsed.last == bool(flags & 2));
        }
        const auto parsed = parse_doubao_response(response(R"({"payload_msg":{"result":[{"text":"part"},{"text":" two"}]}})", compressed));
        assert(parsed.text == "part two" && parsed.last);
        assert(parse_doubao_response(response("{}", compressed)).text.empty());
    }
    Bytes failure{0x11, 0xf0, 0x00, 0};
    word(failure, 45000001);
    word(failure, 9);
    failure.insert(failure.end(), {'s','y','n','t','h','e','t','i','c'});
    assert(parse_doubao_response(failure).code == 45000001 && parse_doubao_response(failure).text.empty());
    const auto good = response(R"({"result":{"text":"synthetic"}})", true);
    for (std::size_t size = 0; size < good.size(); ++size)
        rejects([&] { parse_doubao_response(Bytes(good.begin(), good.begin() + size)); });
    for (auto pair : {std::pair<int, int>{0, 0x10}, {0, 0x21}, {1, 0x83}, {1, 0x98}, {2, 0x12}, {2, 0x21}, {11, 0xff}})
    {
        auto bad = good;
        bad[pair.first] = static_cast<std::uint8_t>(pair.second);
        rejects([&] { parse_doubao_response(bad); });
    }
    auto bad = good;
    bad.back() ^= 0xff;
    rejects([&] { parse_doubao_response(bad); });
    bad = good;
    bad.push_back(0);
    rejects([&] { parse_doubao_response(bad); });
    rejects([] { parse_doubao_response(response(std::string(doubao_response_limit + 1, 'x'), true)); });
    rejects([] { parse_doubao_response(Bytes(doubao_response_limit + 1, 0)); });
    for (const char *body : {"invalid", "[]", R"({"result":{"text":2}})", R"({"result":false})"})
        rejects([&] { parse_doubao_response(response(body)); });
    rejects([] { parse_doubao_response(response(std::string(80, '[') + "0" + std::string(80, ']'))); });
}
