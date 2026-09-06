#include "pinyin_decoder.h"
#include "data_path.h"
#include "../googlepinyinime-rev/src/include/pinyinime.h"
#include "../googlepinyinime-rev/src/include/lpicache.h"
#include <mutex>
#include <utf8.h>

namespace metasequoia
{
namespace
{
struct DecoderService
{
    std::mutex mutex;
    std::filesystem::path model;
    std::filesystem::path user_dictionary;
    bool ready = false;
    std::size_t clients = 0;
    ~DecoderService() { ime_pinyin::im_close_decoder(); }
};

DecoderService &service()
{
    static DecoderService instance;
    return instance;
}
}

PinyinDecoder::PinyinDecoder(std::filesystem::path model, std::filesystem::path user_dictionary)
    : model_(std::move(model)), user_dictionary_(std::move(user_dictionary))
{
    auto &decoder = service();
    std::lock_guard lock(decoder.mutex);
    ++decoder.clients;
}

PinyinDecoder::~PinyinDecoder()
{
    auto &decoder = service();
    std::lock_guard lock(decoder.mutex);
    if (--decoder.clients == 0)
    {
        ime_pinyin::im_close_decoder();
        decoder.ready = false;
        decoder.model.clear();
        decoder.user_dictionary.clear();
    }
}

std::string PinyinDecoder::sentence(const std::string &pinyin) const
{
    if (pinyin.empty() || pinyin.size() > 128 || model_.empty() || user_dictionary_.empty()) return {};
    auto &decoder = service();
    std::lock_guard lock(decoder.mutex);
    if (decoder.model != model_ || decoder.user_dictionary != user_dictionary_)
    {
        ime_pinyin::im_close_decoder();
        decoder.ready = false;
        // The upstream lemma cache is also global and contains model-specific IDs.
        auto &cache = ime_pinyin::LpiCache::get_instance();
        for (ime_pinyin::uint16 id = 0; id < ime_pinyin::kFullSplIdStart; ++id)
            cache.put_cache(id, nullptr, 0);
        decoder.model = model_;
        decoder.user_dictionary = user_dictionary_;
        decoder.ready = ime_pinyin::im_open_decoder(path_to_utf8(model_).c_str(),
                                                   path_to_utf8(user_dictionary_).c_str());
        if (!decoder.ready) return {};
        ime_pinyin::im_set_max_lens(128, 64);
    }
    if (!decoder.ready) return {};
    ime_pinyin::im_reset_search();
    const auto count = ime_pinyin::im_search(pinyin.data(), pinyin.size());
    for (std::size_t i = 0; i < count; ++i)
    {
        ime_pinyin::char16 buffer[256] = {};
        if (!ime_pinyin::im_get_candidate(i, buffer, 255)) continue;
        std::size_t length = 0;
        while (length < 255 && buffer[length] != 0) ++length;
        if (length)
            return utf8::utf16to8(std::u16string(reinterpret_cast<const char16_t *>(buffer), length));
    }
    return {};
}
}
