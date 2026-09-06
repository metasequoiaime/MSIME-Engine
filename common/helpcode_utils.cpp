#include "helpcode_utils.h"
#include "../contracts/assets/assets.h"
#include "../core/data_path.h"
#include <mutex>
#include <stdexcept>
#include "../shuangpin/shuangpin_utils.h"

#include <utf8.h>
#include <algorithm>
#include <atomic>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <spdlog/spdlog.h>

namespace
{
std::mutex schema_mutex;
std::string default_schema = "lantian";

bool is_han_code_point(std::uint32_t code_point)
{
    return code_point == 0x3007 || (code_point >= 0x3400 && code_point <= 0x4DBF) ||
           (code_point >= 0x4E00 && code_point <= 0x9FFF) ||
           (code_point >= 0xF900 && code_point <= 0xFAFF) ||
           (code_point >= 0x20000 && code_point <= 0x2FA1F) ||
           (code_point >= 0x30000 && code_point <= 0x323AF);
}

} // namespace

namespace HelpcodeUtils
{
SharedKeymap load_helpcode_keymap(const std::filesystem::path &resources, const std::string &schema)
{
    const auto found = std::find_if(metasequoia::assets::helpcodes.begin(), metasequoia::assets::helpcodes.end(),
        [&](const auto &entry) { return entry.schema == schema; });
    if (found == metasequoia::assets::helpcodes.end()) throw std::invalid_argument("Unknown helpcode schema");
    auto result = std::make_shared<Keymap>();
    std::ifstream input(resources / found->path);
    std::string line;
    while (std::getline(input, line))
    {
        const auto pos = line.find('=');
        if (pos == std::string::npos || pos == 0) continue;
        const auto code = line.substr(pos + 1, 2);
        if (code.empty() || !std::all_of(code.begin(), code.end(), [](char ch) { return ch >= 'a' && ch <= 'z'; })) continue;
        (*result)[line.substr(0, pos)] = code;
    }
    return result;
}

std::string selected_helpcode_schema()
{
    std::lock_guard lock(schema_mutex);
    return default_schema;
}

const Keymap &helpcode_keymap()
{
    // Compatibility API for standalone callers. Sessions hold their own immutable table.
    thread_local SharedKeymap table;
    thread_local std::filesystem::path loaded_path;
    thread_local std::string loaded_schema;
    const auto path = metasequoia::data_directory();
    const auto schema = selected_helpcode_schema();
    if (!table || path != loaded_path || schema != loaded_schema)
    {
        table = load_helpcode_keymap(path, schema);
        loaded_path = path;
        loaded_schema = schema;
    }
    return *table;
}

std::string get_first_han_char(const std::string &words)
{
    auto it = words.begin();
    const auto end = words.end();
    while (it != end)
    {
        const auto start = it;
        const auto code_point = utf8::next(it, end);
        if (is_han_code_point(code_point))
        {
            return std::string(start, it);
        }
    }
    return "";
}

namespace
{
std::string::size_type get_first_char_size(const std::string &words)
{
    size_t cplen = 1;
    if ((words[0] & 0xf8) == 0xf0)
    {
        cplen = 4;
    }
    else if ((words[0] & 0xf0) == 0xe0)
    {
        cplen = 3;
    }
    else if ((words[0] & 0xe0) == 0xc0)
    {
        cplen = 2;
    }
    if (cplen > words.length())
    {
        cplen = 1;
    }
    return cplen;
}
} // namespace

std::string get_last_han_char(const std::string &words)
{
    auto it = words.begin();
    const auto end = words.end();
    std::string result;
    while (it != end)
    {
        const auto start = it;
        const auto code_point = utf8::next(it, end);
        if (is_han_code_point(code_point))
        {
            result.assign(start, it);
        }
    }
    return result;
}

std::string::size_type count_han_chars(const std::string &words)
{
    size_t index = 0;
    size_t cnt = 0;
    while (index < words.size())
    {
        size_t cplen = get_first_char_size(words.substr(index, words.size() - index));
        index += cplen;
        cnt += 1;
    }
    return cnt;
}

std::string::size_type count_utf8_chars(const std::string &text)
{
    return utf8::distance(text.begin(), text.end());
}

std::string compute_helpcodes(const std::string &words, bool uppercase_all, const Keymap *configured_keymap)
{
    std::string helpcodes;
    const auto &keymap = configured_keymap ? *configured_keymap : helpcode_keymap();

    if (count_han_chars(words) == 1)
    {
        const auto found = keymap.find(words);
        if (found != keymap.end())
        {
            helpcodes += found->second;
        }
    }
    else
    {
        const std::string firstHan = get_first_han_char(words);
        const auto first = keymap.find(firstHan);
        if (first != keymap.end())
        {
            helpcodes += first->second.substr(0, 1);
        }
        else
        {
            return "";
        }

        const std::string lastHan = get_last_han_char(words);
        const auto last = keymap.find(lastHan);
        if (last != keymap.end())
        {
            helpcodes += last->second.substr(0, 1);
        }
        else
        {
            return "";
        }
    }

    if (!helpcodes.empty())
    {
        if (uppercase_all)
        {
            std::transform(helpcodes.begin(), helpcodes.end(), helpcodes.begin(), [](unsigned char ch) {
                return static_cast<char>(std::toupper(ch));
            });
        }
        else if (helpcodes.size() >= 2)
        {
            helpcodes[1] = static_cast<char>(toupper(static_cast<unsigned char>(helpcodes[1])));
        }
        helpcodes = "(" + helpcodes + ")";
    }
    return helpcodes;
}

bool is_quanpin_single_help_mode(const std::string &pinyin_with_cases)
{
    if (pinyin_with_cases.size() <= 1)
    {
        return false;
    }

    if (is_quanpin_double_help_mode(pinyin_with_cases))
    {
        return false;
    }

    const char help_code = pinyin_with_cases.back();
    return help_code >= 'A' && help_code <= 'Z';
}

bool is_quanpin_double_help_mode(const std::string &pinyin_with_cases)
{
    if (pinyin_with_cases.size() <= 2)
    {
        return false;
    }

    const char help_code_1 = pinyin_with_cases[pinyin_with_cases.size() - 2];
    const char help_code_2 = pinyin_with_cases[pinyin_with_cases.size() - 1];
    return help_code_1 >= 'A' && help_code_1 <= 'Z' && help_code_2 >= 'A' && help_code_2 <= 'Z';
}

SingleHelpcodeMatch match_single_helpcode(const std::string &word, const std::string &help_code, const Keymap *configured_keymap)
{
    if (word.empty() || help_code.size() != 1)
    {
        return SingleHelpcodeMatch::None;
    }

    const auto &keymap = configured_keymap ? *configured_keymap : helpcode_keymap();

    if (count_han_chars(word) == 1)
    {
        const auto found = keymap.find(word);
        if (found == keymap.end())
        {
            return SingleHelpcodeMatch::None;
        }
        const bool matches_first = found->second[0] == help_code[0];
        const bool matches_last = found->second.size() > 1 && found->second[1] == help_code[0];
        return matches_first && matches_last ? SingleHelpcodeMatch::Both
             : matches_first                 ? SingleHelpcodeMatch::First
             : matches_last                  ? SingleHelpcodeMatch::Last
                                             : SingleHelpcodeMatch::None;
    }

    const auto first = keymap.find(get_first_han_char(word));
    const auto last = keymap.find(get_last_han_char(word));
    const bool matches_first = first != keymap.end() && first->second[0] == help_code[0];
    const bool matches_last = last != keymap.end() && last->second[0] == help_code[0];
    return matches_first && matches_last ? SingleHelpcodeMatch::Both
         : matches_first                 ? SingleHelpcodeMatch::First
         : matches_last                  ? SingleHelpcodeMatch::Last
                                         : SingleHelpcodeMatch::None;
}

bool matches_double_helpcodes(const std::string &word, const std::string &help_codes, const Keymap *configured_keymap)
{
    if (word.empty() || help_codes.size() != 2)
    {
        return false;
    }

    const auto &keymap = configured_keymap ? *configured_keymap : helpcode_keymap();

    if (count_han_chars(word) == 1)
    {
        const auto found = keymap.find(word);
        return found != keymap.end() && found->second.size() > 1 && found->second[0] == help_codes[0] &&
               found->second[1] == help_codes[1];
    }

    const auto first = keymap.find(get_first_han_char(word));
    const auto last = keymap.find(get_last_han_char(word));
    return first != keymap.end() && last != keymap.end() && first->second[0] == help_codes[0] &&
           last->second[0] == help_codes[1];
}

bool is_supported_helpcode_schema(const std::string &schema)
{
    return std::any_of(metasequoia::assets::helpcodes.begin(), metasequoia::assets::helpcodes.end(),
        [&](const auto &entry) { return entry.schema == schema; });
}

bool select_helpcode_schema(const std::string &schema)
{
    if (!is_supported_helpcode_schema(schema)) return false;
    std::lock_guard lock(schema_mutex);
    default_schema = schema;
    return true;
}

} // namespace HelpcodeUtils
