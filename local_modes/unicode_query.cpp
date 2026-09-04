#include "unicode_query.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <utf8.h>

namespace metasequoia::local_modes
{
namespace
{
bool is_hex_character(unsigned char character)
{
    return std::isxdigit(character) != 0;
}

bool is_unicode_scalar(std::uint32_t codepoint)
{
    return codepoint <= 0x10ffffU && (codepoint < 0xd800U || codepoint > 0xdfffU);
}

std::string codepoint_to_utf8(std::uint32_t codepoint)
{
    std::string result;
    utf8::append(static_cast<utf8::utfchar32_t>(codepoint), std::back_inserter(result));
    return result;
}

std::string codepoint_label(std::uint32_t codepoint)
{
    char buffer[16] = {};
    std::snprintf(buffer, sizeof(buffer), "U+%04X", codepoint);
    return buffer;
}
} // namespace

std::vector<WordItem> query_unicode(const std::string &hex_part, int limit)
{
    if (limit <= 0)
    {
        return {};
    }

    std::size_t offset = 0;
    if (!hex_part.empty() && hex_part.front() == '+')
    {
        offset = 1;
    }
    if (offset >= hex_part.size())
    {
        return {};
    }

    const std::string hex = hex_part.substr(offset);
    if (hex.empty() || hex.size() > 6 ||
        !std::all_of(hex.begin(), hex.end(), [](unsigned char character) {
            return is_hex_character(character);
        }))
    {
        return {};
    }

    std::uint32_t codepoint = 0;
    for (const unsigned char character : hex)
    {
        codepoint <<= 4;
        if (character >= '0' && character <= '9')
        {
            codepoint |= static_cast<std::uint32_t>(character - '0');
        }
        else if (character >= 'a' && character <= 'f')
        {
            codepoint |= static_cast<std::uint32_t>(character - 'a' + 10);
        }
        else
        {
            codepoint |= static_cast<std::uint32_t>(character - 'A' + 10);
        }
    }
    if (!is_unicode_scalar(codepoint))
    {
        return {};
    }

    return {WordItem(codepoint_label(codepoint), codepoint_to_utf8(codepoint),
                     static_cast<std::int64_t>(hex.size()), CandidateSource::Generated)};
}
} // namespace metasequoia::local_modes
