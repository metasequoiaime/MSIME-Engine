#pragma once
#include "input_session_types.h"
#include "runtime_paths.h"
#include "../english/english_dictionary.h"
#include "../local_modes/local_query_result.h"
#include "../local_modes/date_time_query.h"
#include "../shuangpin/shuangpin_profile.h"
#include <functional>
#include <memory>

namespace metasequoia
{
// Owns resource queries and candidate merging; has no keyboard or composition mutation API.
class CandidateQueries
{
public:
    CandidateQueries(RuntimePaths paths, ShuangpinProfile profile)
        : paths_(std::move(paths)), shuangpin_profile_(std::move(profile)) {}
    local_modes::LocalQueryResult local(LocalInputMode mode, const std::string &preedit,
        SchemeType scheme, const std::function<local_modes::LocalDateTime()> &clock,
        const std::vector<WordItem> &engine_candidates);
    std::vector<WordItem> mixed(std::vector<WordItem> candidates, const std::string &prefix,
        SchemeType scheme, EnglishInputOptions english_options, MixedExpressiveOptions expressive_options,
        bool dedicated_english, LocalInputMode local_mode);
    EnglishDictionary &english_dictionary();
private:
    RuntimePaths paths_;
    ShuangpinProfile shuangpin_profile_;
    std::unique_ptr<EnglishDictionary> english_dictionary_;
};
}
