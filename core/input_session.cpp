#include "input_session.h"

#include "../common/helpcode_utils.h"
#include "../local_modes/date_time_query.h"
#include "../local_modes/emoji_query.h"
#include "../local_modes/jianpin_query.h"
#include "../local_modes/kaomoji_query.h"
#include "../local_modes/quick_phrase_query.h"
#include "../local_modes/unicode_query.h"
#include "../user_dictionary/user_dictionary_journal.h"
#include "data_path.h"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <unordered_set>
#include <utility>

namespace metasequoia
{
namespace
{
const char *frequency_mode_name(FrequencyAdjustmentMode mode)
{
    switch (mode)
    {
    case FrequencyAdjustmentMode::Disabled:
        return "disabled";
    case FrequencyAdjustmentMode::Pin:
        return "pin";
    case FrequencyAdjustmentMode::Halve:
        return "halve";
    case FrequencyAdjustmentMode::Linear:
        return "linear";
    case FrequencyAdjustmentMode::Promote:
        return "promote";
    }
    return nullptr;
}
} // namespace

InputSession::InputSession(SchemeType scheme_type, bool quanpin_autocorrect_enabled, bool helpcode_enabled,
                           bool chinese_punctuation_enabled, bool candidate_learning_enabled)
    : engine_(scheme_type), quanpin_autocorrect_enabled_(quanpin_autocorrect_enabled),
      quanpin_helpcode_enabled_(helpcode_enabled), shuangpin_helpcode_enabled_(helpcode_enabled),
      chinese_punctuation_enabled_(chinese_punctuation_enabled),
      candidate_learning_enabled_(candidate_learning_enabled),
      shuangpin_profile_(GetXiaoheShuangpinProfile())
{
    engine_.set_quanpin_autocorrect_enabled(quanpin_autocorrect_enabled_);
    engine_.set_quanpin_helpcode_enabled(quanpin_helpcode_enabled_);
    engine_.set_shuangpin_helpcode_enabled(shuangpin_helpcode_enabled_);
}

InputSession::InputSession(SchemeType scheme_type, const ShuangpinProfile &shuangpin_profile)
    : engine_(scheme_type, shuangpin_profile), shuangpin_profile_(shuangpin_profile)
{
    engine_.set_quanpin_autocorrect_enabled(quanpin_autocorrect_enabled_);
    engine_.set_quanpin_helpcode_enabled(quanpin_helpcode_enabled_);
    engine_.set_shuangpin_helpcode_enabled(shuangpin_helpcode_enabled_);
}

KeyResult InputSession::handle_character(char character, bool shift_only)
{
    if (dedicated_english_mode_)
    {
        const bool ascii_letter = (character >= 'a' && character <= 'z') ||
                                  (character >= 'A' && character <= 'Z');
        if (!ascii_letter)
        {
            return {true, std::nullopt, std::nullopt};
        }
        dedicated_english_preedit_.push_back(character);
        update_dedicated_english_candidates();
        return {true, std::nullopt, std::nullopt};
    }
    if (local_input_mode_ != LocalInputMode::None)
    {
        return handle_local_character(character);
    }

    if (shift_only && character == 'U' && local_mode_options_.unicode && !has_composition() &&
        (scheme() == SchemeType::Quanpin || scheme() == SchemeType::Shuangpin))
    {
        local_input_mode_ = LocalInputMode::Unicode;
        local_preedit_ = "U";
        local_candidates_.clear();
        return {true, std::nullopt, std::nullopt};
    }
    if (shift_only && character == 'T' && local_mode_options_.date_time && !has_composition() &&
        (scheme() == SchemeType::Quanpin || scheme() == SchemeType::Shuangpin))
    {
        local_input_mode_ = LocalInputMode::DateTime;
        local_preedit_ = "T";
        local_candidates_.clear();
        return {true, std::nullopt, std::nullopt};
    }
    if (shift_only && character == 'K' && local_mode_options_.quick_phrase && !has_composition() &&
        (scheme() == SchemeType::Quanpin || scheme() == SchemeType::Shuangpin))
    {
        local_input_mode_ = LocalInputMode::QuickPhrase;
        local_preedit_ = "K";
        local_candidates_.clear();
        return {true, std::nullopt, std::nullopt};
    }
    if (shift_only && character == 'E' && local_mode_options_.emoji && !has_composition() &&
        (scheme() == SchemeType::Quanpin || scheme() == SchemeType::Shuangpin))
    {
        local_input_mode_ = LocalInputMode::Emoji;
        local_preedit_ = "E";
        local_candidates_.clear();
        return {true, std::nullopt, std::nullopt};
    }
    if (shift_only && character == 'M' && local_mode_options_.kaomoji && !has_composition() &&
        (scheme() == SchemeType::Quanpin || scheme() == SchemeType::Shuangpin))
    {
        local_input_mode_ = LocalInputMode::Kaomoji;
        local_preedit_ = "M";
        local_candidates_.clear();
        return {true, std::nullopt, std::nullopt};
    }
    if (shift_only && character == 'J' && local_mode_options_.super_jianpin && !has_composition() &&
        (scheme() == SchemeType::Quanpin || scheme() == SchemeType::Shuangpin))
    {
        local_input_mode_ = LocalInputMode::SuperJianpin;
        local_preedit_ = "J";
        local_candidates_.clear();
        return {true, std::nullopt, std::nullopt};
    }
    if (shift_only && character == 'Y' && local_mode_options_.temporary_english && !has_composition() &&
        (scheme() == SchemeType::Quanpin || scheme() == SchemeType::Shuangpin))
    {
        local_input_mode_ = LocalInputMode::TemporaryEnglish;
        local_preedit_ = "Y";
        local_candidates_.clear();
        return {true, std::nullopt, std::nullopt};
    }
    if (shift_only && character == 'R' && local_mode_options_.temporary_japanese && !has_composition() &&
        (scheme() == SchemeType::Quanpin || scheme() == SchemeType::Shuangpin))
    {
        temporary_original_scheme_ = scheme();
        engine_.switch_scheme(SchemeType::JapaneseRomaji);
        local_input_mode_ = LocalInputMode::TemporaryJapanese;
        local_preedit_ = "R";
        local_candidates_.clear();
        return {true, std::nullopt, std::nullopt};
    }

    const bool lowercase_letter = character >= 'a' && character <= 'z';
    const bool active_helpcode = character >= 'A' && character <= 'Z' && has_composition() &&
                                 ((scheme() == SchemeType::Quanpin && quanpin_helpcode_enabled_) ||
                                  (scheme() == SchemeType::Shuangpin && shuangpin_helpcode_enabled_));
    if (!lowercase_letter && !active_helpcode && character != '\'')
    {
        return {};
    }
    if (character == '\'' && !has_composition())
    {
        return {};
    }

    const std::string previous_preedit = preedit();
    const auto unsigned_character = static_cast<unsigned char>(character);
    const ImeKeyCode key_code =
        character == '\'' ? ImeKey::Apostrophe : static_cast<ImeKeyCode>(std::toupper(unsigned_character));
    engine_.handle_key(key_code, 0, static_cast<ImeCharacter>(unsigned_character));
    update_mixed_candidates();
    return {preedit() != previous_preedit, std::nullopt, std::nullopt};
}

KeyResult InputSession::handle_candidate_key(char character)
{
    if (!has_composition() || character < '1' || character > '9')
    {
        return {};
    }
    return select_candidate(static_cast<std::size_t>(character - '1'));
}

KeyResult InputSession::handle_punctuation(char character)
{
    if (!chinese_punctuation_enabled_)
    {
        return {};
    }

    const char *punctuation = nullptr;
    switch (character)
    {
    case ',':
        punctuation = "，";
        break;
    case '.':
        punctuation = "。";
        break;
    case '?':
        punctuation = "？";
        break;
    case '!':
        punctuation = "！";
        break;
    case ';':
        punctuation = "；";
        break;
    case ':':
        punctuation = "：";
        break;
    case '"':
        punctuation = next_double_quote_is_opening_ ? "“" : "”";
        next_double_quote_is_opening_ = !next_double_quote_is_opening_;
        break;
    case '\'':
        punctuation = next_single_quote_is_opening_ ? "‘" : "’";
        next_single_quote_is_opening_ = !next_single_quote_is_opening_;
        break;
    case '(':
        punctuation = "（";
        break;
    case ')':
        punctuation = "）";
        break;
    case '[':
        punctuation = "【";
        break;
    case ']':
        punctuation = "】";
        break;
    case '<':
        punctuation = "《";
        break;
    case '>':
        punctuation = "》";
        break;
    case '\\':
        punctuation = "、";
        break;
    default:
        return {};
    }

    KeyResult result{true, std::nullopt, std::nullopt};
    if (has_composition())
    {
        result = commit(0);
    }
    std::string text = result.commit.value_or("");
    text += punctuation;
    result.commit = std::move(text);
    return result;
}

KeyResult InputSession::handle_command(Command command)
{
    if (!has_composition())
    {
        return {};
    }

    switch (command)
    {
    case Command::Backspace:
        if (dedicated_english_mode_)
        {
            dedicated_english_preedit_.pop_back();
            update_dedicated_english_candidates();
            return {true, std::nullopt, std::nullopt};
        }
        if (local_input_mode_ != LocalInputMode::None)
        {
            std::optional<std::string> diagnostic;
            if (local_preedit_.size() <= 1)
            {
                reset_composition();
            }
            else if (local_input_mode_ == LocalInputMode::TemporaryJapanese)
            {
                engine_.handle_key(ImeKey::Backspace);
                local_preedit_ = "R" + engine_.get_preedit();
                local_candidates_ = engine_.get_candidates();
            }
            else
            {
                local_preedit_.pop_back();
                diagnostic = update_local_candidates();
            }
            return {true, std::nullopt, std::move(diagnostic)};
        }
        engine_.handle_key(ImeKey::Backspace);
        update_mixed_candidates();
        return {true, std::nullopt, std::nullopt};
    case Command::CommitCandidate:
        return commit(0);
    case Command::CommitRaw: {
        std::string raw = preedit();
        if ((local_input_mode_ == LocalInputMode::TemporaryEnglish ||
             local_input_mode_ == LocalInputMode::TemporaryJapanese) &&
            !raw.empty())
        {
            raw.erase(raw.begin());
        }
        std::optional<std::string> diagnostic;
        if (dedicated_english_mode_ &&
            !user_dictionary::learn_entered_english_word(
                path_to_utf8(data_file_path("english.db")), user_dictionary::default_user_db_path(), raw))
        {
            diagnostic = "English word could not be learned.";
        }
        reset_composition();
        return {true, std::move(raw), std::move(diagnostic)};
    }
    case Command::Cancel:
        reset_composition();
        return {true, std::nullopt, std::nullopt};
    }
    return {};
}

KeyResult InputSession::select_candidate(std::size_t index)
{
    if (!has_composition() || index >= candidates().size())
    {
        return {};
    }
    return commit(index);
}

KeyResult InputSession::select_candidate(const std::string &candidate)
{
    const auto found = std::find_if(candidates().begin(), candidates().end(),
                                    [&](const WordItem &item) { return item.word == candidate; });
    if (found == candidates().end())
    {
        return {};
    }
    return commit(static_cast<std::size_t>(std::distance(candidates().begin(), found)));
}

KeyResult InputSession::select_candidate_edge(std::size_t index, CandidateEdge edge)
{
    if (!has_composition() || index >= candidates().size())
    {
        return {};
    }

    const std::string &candidate = candidates()[index].word;
    std::string character = edge == CandidateEdge::FirstHan ? HelpcodeUtils::get_first_han_char(candidate)
                                                             : HelpcodeUtils::get_last_han_char(candidate);
    if (character.empty())
    {
        return {};
    }

    reset_composition();
    return {true, std::move(character), std::nullopt};
}

void InputSession::set_shuangpin_helpcode_enabled(bool enabled)
{
    shuangpin_helpcode_enabled_ = enabled;
    engine_.set_shuangpin_helpcode_enabled(enabled);
}

void InputSession::set_quanpin_helpcode_enabled(bool enabled)
{
    quanpin_helpcode_enabled_ = enabled;
    engine_.set_quanpin_helpcode_enabled(enabled);
}

bool InputSession::is_supported_helpcode_schema(const std::string &schema)
{
    return HelpcodeUtils::is_supported_helpcode_schema(schema);
}

bool InputSession::select_helpcode_schema(const std::string &schema)
{
    return HelpcodeUtils::select_helpcode_schema(schema);
}

bool InputSession::set_frequency_adjustment(FrequencyAdjustmentOptions options)
{
    if (frequency_mode_name(options.mode) == nullptr || options.trigger_count < 1 || options.trigger_count > 10 ||
        options.linear_step < 1 || options.linear_step > 10)
    {
        return false;
    }
    frequency_adjustment_ = options;
    frequency_adjustment_configured_ = true;
    return true;
}

const FrequencyAdjustmentOptions &InputSession::frequency_adjustment() const
{
    return frequency_adjustment_;
}

void InputSession::set_local_mode_options(LocalModeOptions options)
{
    local_mode_options_ = options;
    if ((local_input_mode_ == LocalInputMode::Unicode && !local_mode_options_.unicode) ||
        (local_input_mode_ == LocalInputMode::DateTime && !local_mode_options_.date_time) ||
        (local_input_mode_ == LocalInputMode::QuickPhrase && !local_mode_options_.quick_phrase) ||
        (local_input_mode_ == LocalInputMode::Emoji && !local_mode_options_.emoji) ||
        (local_input_mode_ == LocalInputMode::Kaomoji && !local_mode_options_.kaomoji) ||
        (local_input_mode_ == LocalInputMode::SuperJianpin && !local_mode_options_.super_jianpin) ||
        (local_input_mode_ == LocalInputMode::TemporaryEnglish && !local_mode_options_.temporary_english) ||
        (local_input_mode_ == LocalInputMode::TemporaryJapanese && !local_mode_options_.temporary_japanese))
    {
        reset_composition();
    }
}

const LocalModeOptions &InputSession::local_mode_options() const
{
    return local_mode_options_;
}

bool InputSession::set_english_input_options(EnglishInputOptions options)
{
    if (options.minimum_prefix < 1 || options.minimum_prefix > 8)
    {
        return false;
    }
    english_input_options_ = options;
    update_mixed_candidates();
    return true;
}

const EnglishInputOptions &InputSession::english_input_options() const
{
    return english_input_options_;
}

void InputSession::set_mixed_expressive_options(MixedExpressiveOptions options)
{
    mixed_expressive_options_ = options;
    update_mixed_candidates();
}

const MixedExpressiveOptions &InputSession::mixed_expressive_options() const
{
    return mixed_expressive_options_;
}

void InputSession::set_dedicated_english_mode(bool enabled)
{
    if (dedicated_english_mode_ == enabled)
    {
        return;
    }
    reset_composition();
    dedicated_english_mode_ = enabled;
}

bool InputSession::dedicated_english_mode() const
{
    return dedicated_english_mode_;
}

LocalInputMode InputSession::local_input_mode() const
{
    return local_input_mode_;
}

void InputSession::set_local_date_time_provider(std::function<local_modes::LocalDateTime()> provider)
{
    local_date_time_provider_ = std::move(provider);
}

void InputSession::switch_scheme(SchemeType scheme_type)
{
    reset_composition();
    engine_.switch_scheme(scheme_type);
    update_mixed_candidates();
}

SchemeType InputSession::scheme() const
{
    return temporary_original_scheme_.value_or(engine_.current_scheme_type());
}

bool InputSession::has_composition() const
{
    if (dedicated_english_mode_)
    {
        return !dedicated_english_preedit_.empty();
    }
    return !preedit().empty();
}

const std::string &InputSession::preedit() const
{
    if (dedicated_english_mode_)
    {
        return dedicated_english_preedit_;
    }
    if (local_input_mode_ != LocalInputMode::None)
    {
        return local_preedit_;
    }
    return engine_.get_preedit();
}

const std::string &InputSession::raw_segmentation() const
{
    if (dedicated_english_mode_)
    {
        return dedicated_english_preedit_;
    }
    if (local_input_mode_ != LocalInputMode::None)
    {
        return local_preedit_;
    }
    return engine_.get_request().raw_segmentation;
}

const std::string &InputSession::normalized_segmentation() const
{
    if (dedicated_english_mode_)
    {
        return dedicated_english_preedit_;
    }
    if (local_input_mode_ != LocalInputMode::None)
    {
        return local_preedit_;
    }
    return engine_.get_request().normalized_segmentation;
}

const std::vector<WordItem> &InputSession::candidates() const
{
    if (dedicated_english_mode_)
    {
        return dedicated_english_candidates_;
    }
    if (local_input_mode_ != LocalInputMode::None)
    {
        return local_candidates_;
    }
    if ((english_input_options_.mixed_candidates || mixed_expressive_options_.emoji_candidates ||
         mixed_expressive_options_.kaomoji_candidates) &&
        (scheme() == SchemeType::Quanpin || scheme() == SchemeType::Shuangpin))
    {
        return mixed_candidates_;
    }
    return engine_.get_candidates();
}

SchemeType InputSession::scheme_type() const
{
    return scheme();
}

bool InputSession::quanpin_autocorrect_enabled() const
{
    return quanpin_autocorrect_enabled_;
}

bool InputSession::helpcode_enabled() const
{
    if (scheme() == SchemeType::Quanpin)
    {
        return quanpin_helpcode_enabled_;
    }
    if (scheme() == SchemeType::Shuangpin)
    {
        return shuangpin_helpcode_enabled_;
    }
    return false;
}

bool InputSession::chinese_punctuation_enabled() const
{
    return chinese_punctuation_enabled_;
}

bool InputSession::candidate_learning_enabled() const
{
    return candidate_learning_enabled_;
}

KeyResult InputSession::commit(std::size_t index)
{
    std::optional<std::string> text;
    if (index < candidates().size())
    {
        text = candidates()[index].word;
    }
    else if (!((local_input_mode_ == LocalInputMode::TemporaryEnglish ||
                local_input_mode_ == LocalInputMode::TemporaryJapanese) &&
               local_preedit_.size() == 1))
    {
        text = preedit();
    }
    std::optional<std::string> diagnostic = learn_candidate(index);
    reset_composition();
    return {true, std::move(text), std::move(diagnostic)};
}

KeyResult InputSession::handle_local_character(char character)
{
    if (local_input_mode_ == LocalInputMode::TemporaryEnglish)
    {
        const bool ascii_letter = (character >= 'a' && character <= 'z') ||
                                  (character >= 'A' && character <= 'Z');
        if (!ascii_letter)
        {
            return {};
        }
        local_preedit_.push_back(character);
        return {true, std::nullopt, update_local_candidates()};
    }
    if (local_input_mode_ == LocalInputMode::TemporaryJapanese)
    {
        const auto unsigned_character = static_cast<unsigned char>(character);
        if (!std::isalpha(unsigned_character) && character != '\'')
        {
            return {};
        }
        const ImeKeyCode key_code = character == '\'' ? ImeKey::Apostrophe :
                                                        static_cast<ImeKeyCode>(std::toupper(unsigned_character));
        engine_.handle_key(key_code, 0, static_cast<ImeCharacter>(unsigned_character));
        local_preedit_ = "R" + engine_.get_preedit();
        local_candidates_ = engine_.get_candidates();
        return {true, std::nullopt, std::nullopt};
    }
    if (local_input_mode_ == LocalInputMode::SuperJianpin)
    {
        const bool ascii_letter = (character >= 'a' && character <= 'z') ||
                                  (character >= 'A' && character <= 'Z');
        if (!ascii_letter)
        {
            return {true, std::nullopt, std::nullopt};
        }
        local_preedit_.push_back(character);
        return {true, std::nullopt, update_local_candidates()};
    }
    if (local_input_mode_ == LocalInputMode::Emoji || local_input_mode_ == LocalInputMode::Kaomoji)
    {
        const bool ascii_letter = (character >= 'a' && character <= 'z') ||
                                  (character >= 'A' && character <= 'Z');
        if (!ascii_letter && character != '\'')
        {
            return {true, std::nullopt, std::nullopt};
        }
        local_preedit_.push_back(character);
        return {true, std::nullopt, update_local_candidates()};
    }
    if (local_input_mode_ == LocalInputMode::QuickPhrase)
    {
        if (character < 'a' || character > 'z')
        {
            return {true, std::nullopt, std::nullopt};
        }
        local_preedit_.push_back(character);
        return {true, std::nullopt, update_local_candidates()};
    }
    if (local_input_mode_ == LocalInputMode::DateTime)
    {
        if (character < 'a' || character > 'z')
        {
            return {true, std::nullopt, std::nullopt};
        }
        local_preedit_.push_back(character);
        return {true, std::nullopt, update_local_candidates()};
    }
    if (local_input_mode_ != LocalInputMode::Unicode)
    {
        return {};
    }

    const auto unsigned_character = static_cast<unsigned char>(character);
    const bool optional_plus = character == '+' && local_preedit_ == "U";
    if (!optional_plus && std::isxdigit(unsigned_character) == 0)
    {
        return {true, std::nullopt, std::nullopt};
    }
    local_preedit_.push_back(character);
    return {true, std::nullopt, update_local_candidates()};
}

std::optional<std::string> InputSession::update_local_candidates()
{
    switch (local_input_mode_)
    {
    case LocalInputMode::Unicode:
        local_candidates_ = local_modes::query_unicode(local_preedit_.substr(1));
        return std::nullopt;
    case LocalInputMode::DateTime:
    {
        const local_modes::LocalDateTime now = local_date_time_provider_ ?
            local_date_time_provider_() : local_modes::current_local_date_time();
        local_candidates_ = local_modes::query_date_time(local_preedit_.substr(1), &now);
        return std::nullopt;
    }
    case LocalInputMode::QuickPhrase:
    {
        local_modes::QuickPhraseQueryResult result =
            local_modes::query_quick_phrases(local_preedit_.substr(1));
        local_candidates_ = std::move(result.candidates);
        return std::move(result.diagnostic);
    }
    case LocalInputMode::Emoji:
    {
        local_modes::LocalQueryResult result = local_modes::query_emoji(
            local_preedit_.substr(1), scheme(), 10, shuangpin_profile_);
        local_candidates_ = std::move(result.candidates);
        return std::move(result.diagnostic);
    }
    case LocalInputMode::Kaomoji:
    {
        local_modes::LocalQueryResult result = local_modes::query_kaomoji(
            local_preedit_.substr(1), scheme(), 10, shuangpin_profile_);
        local_candidates_ = std::move(result.candidates);
        return std::move(result.diagnostic);
    }
    case LocalInputMode::SuperJianpin:
    {
        const std::string code = local_preedit_.substr(1);
        const int limit = code.size() == 1 ? 24 : 100;
        local_modes::LocalQueryResult result =
            local_modes::query_jianpin(code, scheme(), limit, shuangpin_profile_);
        local_candidates_ = std::move(result.candidates);
        return std::move(result.diagnostic);
    }
    case LocalInputMode::TemporaryEnglish:
    {
        const std::string raw = local_preedit_.substr(1);
        local_candidates_.clear();
        if (raw.empty())
        {
            return std::nullopt;
        }
        local_candidates_.emplace_back("", raw, 0, CandidateSource::Generated);
        std::string prefix = raw;
        std::transform(prefix.begin(), prefix.end(), prefix.begin(), [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        auto completions = english_dictionary().query_prefix(prefix, 1000);
        completions.erase(std::remove_if(completions.begin(), completions.end(), [&](const WordItem &candidate) {
                              if (candidate.word.size() != raw.size())
                              {
                                  return false;
                              }
                              return std::equal(candidate.word.begin(), candidate.word.end(), raw.begin(),
                                                [](unsigned char left, unsigned char right) {
                                                    return std::tolower(left) == std::tolower(right);
                                                });
                          }),
                          completions.end());
        local_candidates_.insert(local_candidates_.end(),
                                 std::make_move_iterator(completions.begin()),
                                 std::make_move_iterator(completions.end()));
        return std::nullopt;
    }
    case LocalInputMode::TemporaryJapanese:
        local_candidates_ = engine_.get_candidates();
        return std::nullopt;
    case LocalInputMode::None:
        local_candidates_.clear();
        return std::nullopt;
    }
    return std::nullopt;
}

void InputSession::update_mixed_candidates()
{
    mixed_candidates_ = engine_.get_candidates();
    if ((!english_input_options_.mixed_candidates && !mixed_expressive_options_.emoji_candidates &&
         !mixed_expressive_options_.kaomoji_candidates) ||
        dedicated_english_mode_ || local_input_mode_ != LocalInputMode::None ||
        (scheme() != SchemeType::Quanpin && scheme() != SchemeType::Shuangpin))
    {
        return;
    }

    const std::string &prefix = engine_.get_request().raw_input;
    if (prefix.empty())
    {
        return;
    }

    std::unordered_set<std::string> seen;
    for (const auto &candidate : mixed_candidates_)
    {
        seen.insert(candidate.word);
    }

    const auto collect_unique = [&](std::vector<WordItem> candidates) {
        std::vector<WordItem> unique;
        unique.reserve(candidates.size());
        for (auto &candidate : candidates)
        {
            if (seen.insert(candidate.word).second)
            {
                unique.push_back(std::move(candidate));
            }
        }
        return unique;
    };

    std::vector<WordItem> english_candidates;
    const bool lower_ascii_prefix =
        std::all_of(prefix.begin(), prefix.end(), [](unsigned char character) {
            return character >= 'a' && character <= 'z';
        });
    if (english_input_options_.mixed_candidates &&
        prefix.size() >= english_input_options_.minimum_prefix && lower_ascii_prefix)
    {
        english_candidates = collect_unique(english_dictionary().query_prefix(prefix, 5));
    }

    std::vector<WordItem> emoji_candidates;
    if (mixed_expressive_options_.emoji_candidates && prefix.size() >= 2)
    {
        emoji_candidates = collect_unique(
            local_modes::query_emoji(prefix, scheme(), 3, shuangpin_profile_).candidates);
    }

    std::vector<WordItem> kaomoji_candidates;
    if (mixed_expressive_options_.kaomoji_candidates && prefix.size() >= 2)
    {
        kaomoji_candidates = collect_unique(
            local_modes::query_kaomoji(prefix, scheme(), 3, shuangpin_profile_).candidates);
    }

    std::size_t priority_slot = std::min<std::size_t>(1, mixed_candidates_.size());
    const auto insert_leading = [&](std::vector<WordItem> &source) {
        if (source.empty())
        {
            return;
        }
        mixed_candidates_.insert(mixed_candidates_.begin() + static_cast<std::ptrdiff_t>(priority_slot),
                                 std::move(source.front()));
        source.erase(source.begin());
        ++priority_slot;
    };
    insert_leading(english_candidates);
    insert_leading(emoji_candidates);
    insert_leading(kaomoji_candidates);

    for (auto *source : {&english_candidates, &emoji_candidates, &kaomoji_candidates})
    {
        mixed_candidates_.insert(mixed_candidates_.end(),
                                 std::make_move_iterator(source->begin()),
                                 std::make_move_iterator(source->end()));
    }
}

void InputSession::update_dedicated_english_candidates()
{
    dedicated_english_candidates_.clear();
    if (dedicated_english_preedit_.empty())
    {
        return;
    }

    std::string prefix = dedicated_english_preedit_;
    std::transform(prefix.begin(), prefix.end(), prefix.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    dedicated_english_candidates_ = english_dictionary().query_prefix(prefix, 1000);
    if (dedicated_english_candidates_.empty())
    {
        dedicated_english_candidates_.emplace_back(
            "", dedicated_english_preedit_, 0, CandidateSource::Generated);
    }
}

EnglishDictionary &InputSession::english_dictionary()
{
    if (!english_dictionary_)
    {
        english_dictionary_ = std::make_unique<EnglishDictionary>(
            path_to_utf8(data_file_path("english.db")), false);
    }
    return *english_dictionary_;
}

void InputSession::reset_composition()
{
    const std::optional<SchemeType> original_scheme = temporary_original_scheme_;
    local_input_mode_ = LocalInputMode::None;
    temporary_original_scheme_.reset();
    local_preedit_.clear();
    local_candidates_.clear();
    dedicated_english_preedit_.clear();
    dedicated_english_candidates_.clear();
    mixed_candidates_.clear();
    engine_.reset();
    if (original_scheme.has_value() && engine_.current_scheme_type() != *original_scheme)
    {
        engine_.switch_scheme(*original_scheme);
    }
}

std::optional<std::string> InputSession::learn_candidate(std::size_t index)
{
    if (!candidate_learning_enabled_ || index >= candidates().size())
    {
        return std::nullopt;
    }
    const bool temporary_english = local_input_mode_ == LocalInputMode::TemporaryEnglish;
    if ((dedicated_english_mode_ || temporary_english) && index < candidates().size() &&
        candidates()[index].source == CandidateSource::EnglishDictionary &&
        frequency_adjustment_configured_ && frequency_adjustment_.mode != FrequencyAdjustmentMode::Disabled &&
        index != 0)
    {
        std::string context = temporary_english ? local_preedit_.substr(1) : dedicated_english_preedit_;
        std::transform(context.begin(), context.end(), context.begin(), [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        const WordItem &selected = candidates()[index];
        std::vector<WordItem> ranked_candidates;
        std::copy_if(candidates().begin(), candidates().end(), std::back_inserter(ranked_candidates),
                     [](const WordItem &candidate) {
                         return candidate.source == CandidateSource::EnglishDictionary;
                     });
        bool ranking_changed = false;
        const bool adjusted = user_dictionary::adjust_english_candidate_ranking(
            path_to_utf8(data_file_path("english.db")), user_dictionary::default_user_db_path(),
            "english:" + context, ranked_candidates, selected.pinyin, selected.word,
            frequency_mode_name(frequency_adjustment_.mode), frequency_adjustment_.linear_step,
            frequency_adjustment_.trigger_count, false, &ranking_changed);
        return adjusted ? std::nullopt : std::optional<std::string>(
            "English candidate frequency could not be persisted.");
    }

    const WordItem &selected = candidates()[index];
    if (!frequency_adjustment_configured_)
    {
        if (selected.source == CandidateSource::Database || selected.source == CandidateSource::UserDatabase)
        {
            const std::string &pinyin =
                selected.canonical_pinyin.empty() ? selected.pinyin : selected.canonical_pinyin;
            (void)engine_.update_weight_by_pinyin_and_word(pinyin, selected.word);
        }
        return std::nullopt;
    }
    if (frequency_adjustment_.mode == FrequencyAdjustmentMode::Disabled || index == 0)
    {
        return std::nullopt;
    }
    if ((selected.source != CandidateSource::Database && selected.source != CandidateSource::UserDatabase) ||
        engine_.current_scheme_type() == SchemeType::JapaneseRomaji)
    {
        return std::nullopt;
    }

    const bool super_jianpin = local_input_mode_ == LocalInputMode::SuperJianpin;
    const bool wubi = scheme() == SchemeType::Wubi;
    std::string context_key = super_jianpin ?
        local_modes::jianpin_ranking_context(local_preedit_.substr(1), scheme(), shuangpin_profile_) :
        (wubi ? engine_.get_request().raw_input : engine_.get_request().normalized_segmentation);
    if (!super_jianpin && context_key.empty())
    {
        context_key = engine_.get_request().segmentation;
    }
    const std::string entry_key = (wubi && !super_jianpin) ? selected.pinyin
                                       : (selected.canonical_pinyin.empty() ? context_key
                                                                          : selected.canonical_pinyin);
    bool ranking_changed = false;
    const bool adjusted = user_dictionary::adjust_candidate_ranking(
        path_to_utf8(data_file_path("msime.db")), user_dictionary::default_user_db_path(), context_key,
        candidates(), entry_key, selected.word, frequency_mode_name(frequency_adjustment_.mode),
        frequency_adjustment_.linear_step, frequency_adjustment_.trigger_count, false, &ranking_changed,
        (wubi && !super_jianpin) ? user_dictionary::DictionaryKind::Wubi :
                                  user_dictionary::DictionaryKind::Pinyin);
    if (!adjusted)
    {
        return std::string("Unable to persist candidate frequency adjustment.");
    }
    if (ranking_changed)
    {
        engine_.reset_cache();
    }
    return std::nullopt;
}
} // namespace metasequoia
