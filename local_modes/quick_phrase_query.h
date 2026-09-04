#pragma once

#include "local_query_result.h"

#include <filesystem>
#include <string>

namespace metasequoia::local_modes
{
using QuickPhraseQueryResult = LocalQueryResult;

QuickPhraseQueryResult query_quick_phrases(const std::string &prefix, int limit = 100);
QuickPhraseQueryResult query_quick_phrases(const std::string &prefix,
                                           const std::filesystem::path &database_path,
                                           int limit = 100);
} // namespace metasequoia::local_modes
