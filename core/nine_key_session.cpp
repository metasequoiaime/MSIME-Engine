#include "nine_key_session.h"
#include "../quanpin/quanpin_utils.h"
#include "../user_dictionary/user_dictionary_journal.h"
#include "../common/helpcode_utils.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <system_error>
#include "contracts/assets/assets.h"
#include <unordered_set>

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
    return "disabled";
}
constexpr std::size_t kPathLimit = 48;
constexpr std::size_t kDigitLimit = 32;
std::string encode(const std::string &pinyin)
{
    constexpr char keys[] = "22233344455566677778889999";
    std::string result;
    for (char c : pinyin)
        if (c >= 'a' && c <= 'z')
            result += keys[c - 'a'];
    return result;
}
bool starts(const std::string &text, const std::string &prefix)
{
    return text.compare(0, prefix.size(), prefix) == 0;
}
struct Spelling
{
    std::string text, digits;
};
const std::vector<Spelling> &spellings()
{
    static const auto values = [] {
        std::vector<Spelling> result;
        std::unordered_set<std::string> seen;
        // Include partial final syllables, but never invent non-pinyin letter combinations.
        for (const auto &syllable : quanpin::intact_pinyin_list())
            for (std::size_t n = 1; n <= syllable.size(); ++n)
            {
                auto prefix = syllable.substr(0, n);
                if (seen.insert(prefix).second)
                    result.push_back({prefix, encode(prefix)});
            }
        return result;
    }();
    return values;
}

// 九键上的字母只印在键面上,打字时按的是数字,所以英文候选得把数字还原成字母才查得到。
// T9 expansion multiplies by three or four per digit, so only the leading digits are expanded and
// the dictionary's own prefix search carries the rest; a word is then kept only if its whole code
// starts with what was typed.
constexpr const char *kDigitLetters[] = {"", "", "abc", "def", "ghi", "jkl", "mno", "pqrs", "tuv", "wxyz"};

std::string LettersForDigit(char digit)
{
    return digit >= '2' && digit <= '9' ? kDigitLetters[digit - '0'] : std::string{};
}

std::string DigitsForWord(const std::string &word)
{
    std::string code;
    code.reserve(word.size());
    for (const unsigned char raw : word)
    {
        const char letter = static_cast<char>(std::tolower(raw));
        if (letter < 'a' || letter > 'z')
            return {};
        for (char digit = '2'; digit <= '9'; ++digit)
        {
            const auto letters = LettersForDigit(digit);
            if (letters.find(letter) != std::string::npos)
            {
                code.push_back(digit);
                break;
            }
        }
    }
    return code;
}

std::vector<std::string> LetterPrefixes(const std::string &digits, std::size_t budget)
{
    std::vector<std::string> prefixes{std::string{}};
    for (const char digit : digits)
    {
        const auto letters = LettersForDigit(digit);
        if (letters.empty() || prefixes.size() * letters.size() > budget)
            break;
        std::vector<std::string> grown;
        grown.reserve(prefixes.size() * letters.size());
        for (const auto &prefix : prefixes)
            for (const char letter : letters)
                grown.push_back(prefix + letter);
        prefixes = std::move(grown);
    }
    return prefixes.size() == 1 && prefixes.front().empty() ? std::vector<std::string>{} : prefixes;
}

using Path = std::vector<std::string>;
std::vector<Path> paths(const std::string &digits)
{
    std::vector<std::vector<Path>> suffix(digits.size() + 1);
    suffix.back().push_back({});
    for (std::size_t offset = digits.size(); offset-- > 0;)
    {
        auto &result = suffix[offset];
        for (const auto &spelling : spellings())
        {
            const auto end = offset + spelling.digits.size();
            if (end > digits.size() || digits.compare(offset, spelling.digits.size(), spelling.digits) != 0)
                continue;
            if (end != digits.size() && !quanpin::intact_pinyin_set().count(spelling.text))
                continue;
            for (const auto &tail : suffix[end])
            {
                Path path{spelling.text};
                path.insert(path.end(), tail.begin(), tail.end());
                result.push_back(std::move(path));
            }
        }
        std::stable_sort(result.begin(), result.end(), [](const Path &a, const Path &b) {
            if (a.size() != b.size())
                return a.size() < b.size();
            const bool ac = quanpin::intact_pinyin_set().count(a.back());
            const bool bc = quanpin::intact_pinyin_set().count(b.back());
            if (ac != bc)
                return ac;
            return a < b;
        });
        if (result.size() > kPathLimit)
            result.resize(kPathLimit);
    }
    return suffix.front();
}
} // namespace

std::size_t NineKeySession::locked_length() const
{
    std::size_t count = 0;
    for (const auto &part : locked_)
        count += part.size();
    return count;
}

std::vector<WordItem> NineKeySession::english_candidates()
{
    if (!english_.mixed_candidates || !locked_.empty() || digits_.size() < english_.minimum_prefix)
        return {};
    if (!english_dictionary_)
    {
        const auto path = paths_.dictionary(assets::english_dictionary);
        std::error_code code;
        if (!std::filesystem::exists(path, code))
            return {};
        english_dictionary_ = std::make_unique<EnglishDictionary>(path.string(), false);
    }
    std::vector<WordItem> words;
    std::unordered_set<std::string> seen;
    for (const auto &prefix : LetterPrefixes(digits_, 64))
    {
        for (auto &word : english_dictionary_->query_prefix(prefix, 5))
        {
            // 只有整串编码对得上的才算,否则展开之外的字母会混进来。
            if (!starts(DigitsForWord(word.word), digits_) || !seen.insert(word.word).second)
                continue;
            words.push_back(std::move(word));
        }
    }
    // 打满几位就先给几个字母的词,同长度再比词频。
    //
    // Prefix matches belong in the list -- a nine-key code is also the start of longer words -- but
    // not ahead of the word the code spells exactly. Ranked on frequency alone, typing 65 for "ok"
    // led with "old", which is simply the more common word and not the one being asked for.
    const auto typed = digits_.size();
    std::stable_sort(words.begin(), words.end(), [typed](const WordItem &a, const WordItem &b) {
        const bool a_exact = a.word.size() == typed;
        const bool b_exact = b.word.size() == typed;
        if (a_exact != b_exact)
            return a_exact;
        return a.weight != b.weight ? a.weight > b.weight : a.word.size() < b.word.size();
    });
    if (words.size() > 5)
        words.resize(5);
    return words;
}

void NineKeySession::refresh()
{
    candidates_.clear();
    spellings_.clear();
    if (!active())
        return;
    if (!dictionary_)
        dictionary_ = std::make_unique<QuanpinDictionary>(std::string{}, paths_);
    const auto remaining = digits_.substr(locked_length());
    for (const auto &syllable : quanpin::intact_pinyin_list())
    {
        const auto code = encode(syllable);
        if (!remaining.empty() && locked_length() + std::max(remaining.size(), code.size()) <= kDigitLimit &&
            (starts(remaining, code) || starts(code, remaining)))
            spellings_.push_back(syllable);
    }
    std::stable_sort(spellings_.begin(), spellings_.end(), [&remaining](const auto &a, const auto &b) {
        const auto al = std::min(a.size(), remaining.size()), bl = std::min(b.size(), remaining.size());
        return al != bl ? al > bl : a < b;
    });
    auto alternatives = paths(remaining);
    // Even an unfinished/invalid tail must still offer the leading syllable for partial selection.
    for (const auto &spelling : spellings_)
        if (spelling.size() <= remaining.size())
            alternatives.push_back({spelling});
    if (remaining.empty())
        alternatives = {{}};
    const auto locked_key = quanpin::join_segments(locked_);
    std::unordered_set<std::string> queried;
    for (auto path : alternatives)
    {
        path.insert(path.begin(), locked_.begin(), locked_.end());
        const auto key = quanpin::join_segments(path);
        if (key.empty() || !queried.insert(key).second)
            continue;
        for (auto candidate : dictionary_->query(key, key, false, fuzzy_))
        {
            const auto canonical = candidate.canonical_pinyin.empty() ? candidate.pinyin : candidate.canonical_pinyin;
            if (std::count(canonical.begin(), canonical.end(), '\'') >= static_cast<long>(path.size()))
                continue;
            const auto matched = candidate.fuzzy ? candidate.pinyin : canonical;
            const auto code = encode(matched);
            if (code.empty() || (!starts(code, digits_) && !starts(digits_, code)))
                continue;
            if (!locked_key.empty() && matched != locked_key && !starts(matched, locked_key + "'") &&
                !starts(locked_key, matched + "'"))
                continue;
            candidate.pinyin = digits_.substr(0, std::min(code.size(), digits_.size()));
            candidate.canonical_pinyin = canonical;
            candidates_.push_back(std::move(candidate));
        }
    }
    std::stable_sort(candidates_.begin(), candidates_.end(), [](const auto &a, const auto &b) {
        if (a.pinyin.size() != b.pinyin.size())
            return a.pinyin.size() > b.pinyin.size();
        if (a.fuzzy != b.fuzzy)
            return !a.fuzzy;
        return a.weight > b.weight;
    });
    std::unordered_set<std::string> seen;
    candidates_.erase(std::remove_if(candidates_.begin(), candidates_.end(),
                                     [&seen](const auto &item) { return !seen.insert(item.word).second; }),
                      candidates_.end());
    if (candidates_.size() > 128)
        candidates_.resize(128);
    auto english = english_candidates();
    if (!english.empty())
    {
        const auto slot = std::min<std::size_t>(1, candidates_.size());
        candidates_.insert(candidates_.begin() + static_cast<std::ptrdiff_t>(slot), english.front());
        candidates_.insert(candidates_.end(), std::make_move_iterator(english.begin() + 1),
                           std::make_move_iterator(english.end()));
    }
    user_dictionary::apply_fixed_positions(path_to_utf8(paths_.user(assets::user_journal)), ranking_context(),
                                           candidates_, false);
    // Keep the most likely reading visible without requiring a horizontal scroll.
    if (!candidates_.empty())
    {
        const auto &canonical = candidates_.front().canonical_pinyin;
        const auto offset = locked_key.empty() ? 0 : locked_key.size() + 1;
        if (offset < canonical.size())
        {
            const auto preferred = canonical.substr(offset, canonical.find('\'', offset) - offset);
            const auto found = std::find(spellings_.begin(), spellings_.end(), preferred);
            if (found != spellings_.end())
                std::rotate(spellings_.begin(), found, found + 1);
        }
    }
}
KeyResult NineKeySession::character(char digit)
{
    if (digit < '2' || digit > '9')
        return {};
    if (digits_.size() >= kDigitLimit)
        return {true, {}, "九键最多输入 32 位，请先选择候选"};
    digits_ += digit;
    refresh();
    return {true, {}, {}};
}
KeyResult NineKeySession::select(std::size_t index)
{
    if (index >= candidates_.size())
        return {};
    const auto selected = candidates_[index];
    std::optional<std::string> diagnostic;
    if (learning_ && frequency_.mode != FrequencyAdjustmentMode::Disabled && index != 0 &&
        selected.fixed_position == 0 && editable(index))
        diagnostic = adjust_frequency(index, false);
    digits_.erase(0, selected.pinyin.size());
    auto consumed = selected.pinyin.size();
    while (!locked_.empty() && consumed >= locked_.front().size())
    {
        consumed -= locked_.front().size();
        locked_.erase(locked_.begin());
    }
    refresh();
    return {true, selected.word, std::move(diagnostic)};
}
KeyResult NineKeySession::finish(std::size_t index)
{
    if (!active())
        return {};
    std::string commit;
    std::optional<std::string> diagnostic;
    if (index >= candidates_.size())
        return command(Command::CommitRaw);
    while (active())
    {
        if (candidates_.empty())
        {
            commit += digits_;
            break;
        }
        const auto result = select(index);
        if (!diagnostic && result.diagnostic)
            diagnostic = result.diagnostic;
        if (result.commit)
            commit += *result.commit;
        index = 0;
    }
    command(Command::Cancel);
    return {true, commit, std::move(diagnostic)};
}

std::string NineKeySession::ranking_context() const
{
    return "nine-key:" + digits_ + ":" + quanpin::join_segments(locked_);
}

bool NineKeySession::editable(std::size_t index) const
{
    if (index >= candidates_.size())
        return false;
    const auto &item = candidates_[index];
    return !item.canonical_pinyin.empty() &&
           (item.source == CandidateSource::Database || item.source == CandidateSource::UserDatabase);
}

std::optional<std::string> NineKeySession::adjust_frequency(std::size_t index, bool force_top)
{
    const auto &item = candidates_[index];
    bool changed = false;
    if (!user_dictionary::adjust_candidate_ranking(
            path_to_utf8(paths_.dictionary(assets::main_dictionary)), path_to_utf8(paths_.user(assets::user_journal)),
            ranking_context(), candidates_, item.canonical_pinyin, item.word,
            force_top ? "pin" : frequency_mode_name(frequency_.mode), frequency_.linear_step, frequency_.trigger_count,
            force_top, &changed))
        return "Unable to persist nine-key candidate frequency adjustment.";
    if (changed && dictionary_)
        dictionary_->reset_cache();
    return {};
}

KeyResult NineKeySession::pin(std::size_t index)
{
    if (!editable(index))
        return {};
    auto diagnostic = adjust_frequency(index, true);
    refresh();
    return {true, {}, std::move(diagnostic)};
}

KeyResult NineKeySession::remove(std::size_t index)
{
    if (!editable(index) || HelpcodeUtils::count_utf8_chars(candidates_[index].word) <= 1)
        return {};
    const auto item = candidates_[index];
    if (!user_dictionary::delete_dictionary_candidate(
            path_to_utf8(paths_.dictionary(assets::main_dictionary)), path_to_utf8(paths_.user(assets::user_journal)),
            user_dictionary::DictionaryKind::Pinyin, item.canonical_pinyin, item.word))
        return {true, {}, "Unable to persist nine-key candidate removal."};
    dictionary_->reset_cache();
    refresh();
    return {true, {}, {}};
}

KeyResult NineKeySession::set_position(std::size_t index, int position)
{
    if (!editable(index) || position < 0 || position > 5)
        return {};
    const auto &item = candidates_[index];
    const auto journal = path_to_utf8(paths_.user(assets::user_journal));
    const bool saved = position == 0 ? user_dictionary::clear_fixed_position(journal, ranking_context(),
                                                                             item.canonical_pinyin, item.word)
                                     : user_dictionary::set_fixed_position(journal, ranking_context(),
                                                                           item.canonical_pinyin, item.word, position);
    if (!saved)
        return {true, {}, "Unable to persist nine-key candidate position."};
    refresh();
    return {true, {}, {}};
}
KeyResult NineKeySession::command(Command command)
{
    if (!active())
        return {};
    if (command == Command::CommitCandidate)
        return select(0);
    if (command == Command::CommitRaw)
    {
        auto raw = digits_;
        this->command(Command::Cancel);
        return {true, raw, {}};
    }
    if (command == Command::Cancel)
    {
        digits_.clear();
        locked_.clear();
    }
    else if (command == Command::Backspace)
    {
        digits_.pop_back();
        while (locked_length() > digits_.size())
            locked_.pop_back();
    }
    else
        return {};
    refresh();
    return {true, {}, {}};
}
KeyResult NineKeySession::choose_spelling(std::size_t index)
{
    if (index >= spellings_.size())
        return {};
    const auto spelling = spellings_[index];
    const auto offset = locked_length();
    digits_.replace(offset, std::min(spelling.size(), digits_.size() - offset), encode(spelling));
    locked_.push_back(spelling);
    refresh();
    return {true, {}, {}};
}
SessionSnapshot NineKeySession::snapshot() const
{
    SessionSnapshot result{};
    result.scheme = SchemeType::Quanpin;
    result.local_mode = LocalInputMode::None;
    result.preedit = quanpin::join_segments(locked_);
    if (active() && locked_length() < digits_.size())
    {
        if (!result.preedit.empty())
            result.preedit += "'";
        result.preedit += digits_.substr(locked_length());
    }
    result.editing_text = digits_;
    result.caret_position = digits_.size();
    result.candidates = candidates_;
    result.candidate_sources.reserve(result.candidates.size());
    for (const auto &candidate : result.candidates)
        result.candidate_sources.push_back(candidate.source);
    result.candidate_annotations.reserve(result.candidates.size());
    for (const auto &candidate : result.candidates)
        result.candidate_annotations.push_back(candidate.corrected_from);
    result.nine_key_spellings = spellings_;
    return result;
}
} // namespace metasequoia
