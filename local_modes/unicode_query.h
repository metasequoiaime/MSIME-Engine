#pragma once

#include "../core/word_item.h"

#include <string>
#include <vector>

namespace metasequoia::local_modes
{
// `hex_part` is the text after the leading U: an optional plus followed by
// one to six hexadecimal digits.
std::vector<WordItem> query_unicode(const std::string &hex_part, int limit = 8);
} // namespace metasequoia::local_modes
