#pragma once

#include "local_query_result.h"
#include "../core/scheme_type.h"
#include "../shuangpin/shuangpin_profile.h"

#include <filesystem>
#include <string>

namespace metasequoia::local_modes
{
std::string jianpin_ranking_context(
    const std::string &code, SchemeType scheme = SchemeType::Quanpin,
    const ShuangpinProfile &profile = GetXiaoheShuangpinProfile());

LocalQueryResult query_jianpin(
    const std::string &code, SchemeType scheme, int limit = 100,
    const ShuangpinProfile &profile = GetXiaoheShuangpinProfile());
LocalQueryResult query_jianpin(
    const std::string &code, SchemeType scheme, const std::filesystem::path &database_path,
    int limit = 100, const ShuangpinProfile &profile = GetXiaoheShuangpinProfile());
} // namespace metasequoia::local_modes
