#pragma once

#include "../core/word_item.h"

#include <string>
#include <vector>

namespace metasequoia::local_modes
{
struct LocalDateTime
{
    unsigned year = 0;
    unsigned month = 0;
    unsigned day = 0;
    unsigned weekday = 0;
    unsigned hour = 0;
    unsigned minute = 0;
    unsigned second = 0;
};

LocalDateTime current_local_date_time();
bool is_date_time_keyword(const std::string &keyword);
std::vector<WordItem> query_date_time(const std::string &keyword,
                                      const LocalDateTime *now = nullptr,
                                      int limit = 17);
} // namespace metasequoia::local_modes
