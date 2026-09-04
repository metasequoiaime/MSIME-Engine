#include "helpcode_utils.h"
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
const std::string kHelpcodeDirectoryName = "helpcodes";
const std::string kLantianHelpcodeFileName = "helpcode.txt";
const std::string kZiranmaHelpcodeFileName = "zrm_helpcode_big_unique.txt";
const std::string kShouyou2_0HelpcodeFileName = "shouyou2_0_helpcode.txt";
const std::string kShouyouplusHelpcodeFileName = "shouyouplus_helpcode.txt";
const std::string kXiaoheHelpcodeFileName = "xiaohe_helpcode.txt";

enum class HelpcodeSchemaIndex
{
    Lantian,
    Ziranma,
    Shouyou2_0,
    Shouyouplus,
    Xiaohe,
};

std::atomic<HelpcodeSchemaIndex> g_helpcode_schema{HelpcodeSchemaIndex::Lantian};

bool is_han_code_point(std::uint32_t code_point)
{
    return code_point == 0x3007 || (code_point >= 0x3400 && code_point <= 0x4DBF) ||
           (code_point >= 0x4E00 && code_point <= 0x9FFF) ||
           (code_point >= 0xF900 && code_point <= 0xFAFF) ||
           (code_point >= 0x20000 && code_point <= 0x2FA1F) ||
           (code_point >= 0x30000 && code_point <= 0x323AF);
}

std::unordered_map<std::string, std::string> initialize_helpcode_keymap(const std::string &file_name)
{
    std::unordered_map<std::string, std::string> result;
    std::ifstream helpcode_path(shuangpin::get_data_file_path(std::filesystem::path(kHelpcodeDirectoryName) / file_name));
    if (!helpcode_path.is_open())
    {
        (void)0;
        return result;
    }

    std::string line;
    while (getline(helpcode_path, line))
    {
        size_t pos = line.find('=');
        if (pos == std::string::npos)
        {
            continue;
        }
        result[line.substr(0, pos)] = line.substr(pos + 1, 2);
    }
    return result;
}
} // namespace

namespace HelpcodeUtils
{

const std::unordered_map<std::string, std::string> &helpcode_keymap()
{
    static const auto lantian_keymap = initialize_helpcode_keymap(kLantianHelpcodeFileName);
    static const auto ziranma_keymap = initialize_helpcode_keymap(kZiranmaHelpcodeFileName);
    static const auto shouyou2_0_keymap = initialize_helpcode_keymap(kShouyou2_0HelpcodeFileName);
    static const auto shouyouplus_keymap = initialize_helpcode_keymap(kShouyouplusHelpcodeFileName);
    static const auto xiaohe_keymap = initialize_helpcode_keymap(kXiaoheHelpcodeFileName);

    switch (g_helpcode_schema.load(std::memory_order_relaxed))
    {
    case HelpcodeSchemaIndex::Ziranma:
        return ziranma_keymap;
    case HelpcodeSchemaIndex::Shouyou2_0:
        return shouyou2_0_keymap;
    case HelpcodeSchemaIndex::Shouyouplus:
        return shouyouplus_keymap;
    case HelpcodeSchemaIndex::Xiaohe:
        return xiaohe_keymap;
    case HelpcodeSchemaIndex::Lantian:
    default:
        return lantian_keymap;
    }
}

bool is_supported_helpcode_schema(const std::string &schema)
{
    return schema == "lantian" || schema == "ziranma" || schema == "shouyou2_0" || schema == "shouyouplus" ||
           schema == "xiaohe";
}

bool select_helpcode_schema(const std::string &schema)
{
    if (!is_supported_helpcode_schema(schema))
        return false;

    HelpcodeSchemaIndex selected_schema;
    if (schema == "lantian")
        selected_schema = HelpcodeSchemaIndex::Lantian;
    else if (schema == "ziranma")
        selected_schema = HelpcodeSchemaIndex::Ziranma;
    else if (schema == "shouyou2_0")
        selected_schema = HelpcodeSchemaIndex::Shouyou2_0;
    else if (schema == "shouyouplus")
        selected_schema = HelpcodeSchemaIndex::Shouyouplus;
    else if (schema == "xiaohe")
        selected_schema = HelpcodeSchemaIndex::Xiaohe;
    else
        return false;

    g_helpcode_schema.store(selected_schema, std::memory_order_relaxed);
    return true;
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

std::string compute_helpcodes(const std::string &words, bool uppercase_all)
{
    std::string helpcodes;
    const auto &keymap = helpcode_keymap();

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

SingleHelpcodeMatch match_single_helpcode(const std::string &word, const std::string &help_code)
{
    if (word.empty() || help_code.size() != 1)
    {
        return SingleHelpcodeMatch::None;
    }

    const auto &keymap = helpcode_keymap();

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

bool matches_double_helpcodes(const std::string &word, const std::string &help_codes)
{
    if (word.empty() || help_codes.size() != 2)
    {
        return false;
    }

    const auto &keymap = helpcode_keymap();

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

} // namespace HelpcodeUtils
