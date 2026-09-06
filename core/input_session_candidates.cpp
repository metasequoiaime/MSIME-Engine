#include "input_session.h"
#include "data_path.h"
#include "../common/helpcode_utils.h"
#include "../contracts/assets/assets.h"
#include "../user_dictionary/user_dictionary_journal.h"

namespace metasequoia
{
KeyResult InputSession::remove_candidate(std::size_t index)
{
    if (index >= candidates().size()) return {};
    const auto selected = candidates()[index];
    const bool english = selected.source == CandidateSource::EnglishDictionary;
    if (!english &&
        ((selected.source != CandidateSource::Database && selected.source != CandidateSource::UserDatabase) ||
         scheme() == SchemeType::JapaneseRomaji || HelpcodeUtils::count_utf8_chars(selected.word) <= 1))
        return {};

    const bool wubi = scheme() == SchemeType::Wubi && local_input_mode_ != LocalInputMode::SuperJianpin;
    const auto kind = english ? user_dictionary::DictionaryKind::English :
        (wubi ? user_dictionary::DictionaryKind::Wubi : user_dictionary::DictionaryKind::Pinyin);
    // A displayed pinyin candidate already carries its exact dictionary key. Re-segmenting it
    // (or expanding shuangpin again) can delete a different pronunciation of the same word.
    const auto &key = english || wubi ? selected.pinyin : selected.canonical_pinyin;
    if (key.empty()) return {};
    if (!user_dictionary::delete_dictionary_candidate(
            path_to_utf8(paths_.dictionary(english ? assets::english_dictionary : assets::main_dictionary)),
            path_to_utf8(paths_.user(assets::user_journal)), kind, key, selected.word))
        return {true, std::nullopt, "Unable to persist candidate removal."};

    reset_cache();
    if (dedicated_english_mode_) update_dedicated_english_candidates();
    else if (local_input_mode_ != LocalInputMode::None) return {true, std::nullopt, update_local_candidates()};
    else recompute_candidates();
    return {true, std::nullopt, std::nullopt};
}
} // namespace metasequoia
