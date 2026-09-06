#pragma once
#include <filesystem>
#include <string>

namespace metasequoia
{
// Google Pinyin exposes process-wide singletons. This adapter owns their lifecycle and
// executes a complete, reset-before-search operation under one lock. No caller can retain
// candidate pointers or incremental search state between requests.
class PinyinDecoder
{
public:
    PinyinDecoder(std::filesystem::path model, std::filesystem::path user_dictionary);
    ~PinyinDecoder();
    PinyinDecoder(const PinyinDecoder &) = delete;
    PinyinDecoder &operator=(const PinyinDecoder &) = delete;
    std::string sentence(const std::string &pinyin) const;

private:
    std::filesystem::path model_;
    std::filesystem::path user_dictionary_;
};
}
