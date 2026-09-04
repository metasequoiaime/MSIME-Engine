#include "quanpin_dictionary.h"
#include "../user_dictionary/user_dictionary_journal.h"

#include "../common/helpcode_utils.h"
#include "quanpin_query.h"
#include "quanpin_utils.h"
#include "../googlepinyinime-rev/src/include/pinyinime.h"
#include "../shuangpin/shuangpin_utils.h"
#include <algorithm>
#include <climits>
#include <cstring>
#include <fmt/format.h>
#include <unordered_set>
#include <utf8/cpp17.h>

namespace
{
constexpr size_t kSparsePinyinFallbackThreshold = 8;
constexpr size_t kSyllableGraphPathLimit = 32;
constexpr size_t kMaxSyllablesForMultipleSegmentations = 3;
constexpr int kAlternativeSegmentationCandidateLimit = 128;
constexpr size_t kBestAlternativeSegmentationMaxIndex = 1;

bool is_alpha_vk(ImeKeyCode vk)
{
    return vk >= 'A' && vk <= 'Z';
}

std::string from_utf16(const ime_pinyin::char16 *buf, size_t len)
{
    std::u16string utf16(reinterpret_cast<const char16_t *>(buf), len);
    return utf8::utf16to8(utf16);
}

std::string remove_delimiters(const std::string &segmented)
{
    std::string normalized = segmented;
    normalized.erase(std::remove(normalized.begin(), normalized.end(), '\''), normalized.end());
    return normalized;
}

std::string series_cache_key(const std::string &raw_input, const std::string &segmentation)
{
    const char *prefix = raw_input.find('\'') == std::string::npos ? "A:" : "M:";
    return prefix + (segmentation.empty() ? raw_input : segmentation);
}

std::string escape_sql_text(std::string text)
{
    size_t pos = 0;
    while ((pos = text.find('\'', pos)) != std::string::npos)
    {
        text.insert(pos, 1, '\'');
        pos += 2;
    }
    return text;
}

} // namespace

QuanpinDictionary::QuanpinDictionary(std::string db_path)
    : cache_(128), series_cache_(128), segmentation_cache_(128),
      db_path_(db_path.empty() ? quanpin::get_default_db_path() : std::move(db_path))
{
    ime_pinyin::im_set_max_lens(128, 64);
    decoder_ready_ = ime_pinyin::im_open_decoder(
        metasequoia::path_to_utf8(shuangpin::get_data_file_path("dict_pinyin.dat")).c_str(),
        metasequoia::path_to_utf8(shuangpin::get_data_file_path("user_dict.dat")).c_str());
    if (!decoder_ready_)
    {
        (void)0;
    }

    const int exit = sqlite3_open(db_path_.c_str(), &db_);
    if (exit != SQLITE_OK)
    {
        (void)0;
    }

    quanpin::warm_up(db_, statement_cache_);
    reset_cache_if_database_changed();
}

QuanpinDictionary::~QuanpinDictionary()
{
    for (auto &[sql, stmt] : statement_cache_)
    {
        if (stmt != nullptr)
        {
            sqlite3_finalize(stmt);
        }
    }
    if (db_ != nullptr)
    {
        sqlite3_close(db_);
    }
}

std::vector<WordItem> QuanpinDictionary::query(const std::string &raw_input, const std::string &segmentation,
                                               bool enable_autocorrect)
{
    if (raw_input.empty())
    {
        current_candidate_list_.clear();
        return {};
    }

    pinyin_sequence_ = raw_input;
    const auto segments = resolve_segments(raw_input, segmentation);

    // Typing autocorrection: when the spelling is not a legal pinyin
    // combination (the correction cut already fell back to greedy), try to
    // rewrite the whole string into legal syllables. The corrected
    // segmentation becomes the primary key so that selection and weight
    // updates land on the right dictionary entries, and the original
    // (garbage-leaning) candidates stay behind as a fallback tail.
    quanpin::Segments corrected;
    const bool corrected_input = enable_autocorrect && !segments.empty() && raw_input.find('\'') == std::string::npos &&
                                 !quanpin::has_only_complete_pinyin_segments(segments) &&
                                 !(corrected = quanpin::autocorrect_cut(raw_input)).empty();

    pinyin_segmentation_ =
        corrected_input
            ? quanpin::join_segments(corrected)
            : (segmentation.empty() ? (segments.empty() ? raw_input : quanpin::join_segments(segments)) : segmentation);

    // Autocorrected results get their own cache slot so they never leak the
    // fallback tail into plain (correct) spellings sharing the same key.
    const std::string cache_key = (corrected_input ? "C:" : "") + series_cache_key(raw_input, pinyin_segmentation_);
    if (auto cached = series_cache_.get(cache_key))
    {
        reset_cache_if_database_changed();
        if (cached = series_cache_.get(cache_key))
        {
            current_candidate_list_ = cached.value();
            return current_candidate_list_;
        }
    }

    std::vector<quanpin::Segments> alternative_segmentations;
    if (raw_input.find('\'') == std::string::npos && segments.size() <= kMaxSyllablesForMultipleSegmentations &&
        quanpin::has_only_complete_pinyin_segments(segments))
    {
        alternative_segmentations = quanpin::enumerate_complete_segmentations(quanpin::build_syllable_graph(raw_input),
                                                                              kSyllableGraphPathLimit);
        alternative_segmentations.erase(
            std::remove_if(alternative_segmentations.begin(), alternative_segmentations.end(),
                           [&](const quanpin::Segments &candidate) {
                               return quanpin::join_segments(candidate) == pinyin_segmentation_;
                           }),
            alternative_segmentations.end());
    }

    std::vector<WordItem> result;
    if (corrected_input)
    {
        result = query_series(raw_input, pinyin_segmentation_, corrected);
        const std::string fallback_segmentation =
            segmentation.empty() ? quanpin::join_segments(segments) : segmentation;
        append_unique_words(result, query_series(raw_input, fallback_segmentation, segments));
    }
    else
    {
        result = query_series(raw_input, pinyin_segmentation_, segments);
        if (!alternative_segmentations.empty())
        {
            result = merge_alternative_segmentations(raw_input, pinyin_segmentation_, segments,
                                                     alternative_segmentations, std::move(result));
        }
    }
    series_cache_.insert(cache_key, result);
    current_candidate_list_ = result;
    return current_candidate_list_;
}

std::optional<WordItem> QuanpinDictionary::find_candidate(const std::string &key, const std::string &value)
{
    const std::string table = quanpin::build_table_name(quanpin::split_segments(key));
    if (!db_ || table.empty())
        return std::nullopt;
    sqlite3_stmt *stmt = nullptr;
    const std::string sql = "SELECT weight FROM \"" + table + "\" WHERE key=?1 AND value=?2 LIMIT 1";
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return std::nullopt;
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> guard(stmt, sqlite3_finalize);
    if (sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_step(stmt) != SQLITE_ROW)
        return std::nullopt;
    return WordItem(key, value, sqlite3_column_int64(stmt, 0), CandidateSource::Database, key);
}

bool QuanpinDictionary::expand_initial_candidates(const std::string &code, std::vector<WordItem> &candidates)
{
    if (code.size() != 1)
    {
        return false;
    }

    const auto is_limited_initial = [&](const WordItem &item) {
        return item.source == CandidateSource::Database && item.pinyin == code;
    };
    const size_t limited_count =
        static_cast<size_t>(std::count_if(candidates.begin(), candidates.end(), is_limited_initial));
    constexpr size_t kInitialCandidateLimit = 24;
    if (limited_count != kInitialCandidateLimit)
    {
        return false;
    }

    auto expanded = query_initial(code, INT_MAX);
    if (expanded.size() <= limited_count)
    {
        return false;
    }
    for (auto &item : expanded)
    {
        item.canonical_pinyin = item.pinyin;
        item.pinyin = code;
    }

    std::vector<WordItem> merged;
    merged.reserve(candidates.size() - limited_count + expanded.size());
    bool inserted = false;
    for (auto &item : candidates)
    {
        if (is_limited_initial(item))
        {
            if (!inserted)
            {
                merged.insert(merged.end(), expanded.begin(), expanded.end());
                inserted = true;
            }
            continue;
        }
        merged.push_back(std::move(item));
    }

    candidates = std::move(merged);
    cache_.insert(code, expanded);
    series_cache_.insert(series_cache_key(pinyin_sequence_, pinyin_segmentation_), candidates);
    current_candidate_list_ = candidates;
    return true;
}

std::vector<WordItem> QuanpinDictionary::query_series(const std::string &raw_input, const std::string &segmentation,
                                                      const quanpin::Segments &segments)
{
    if (segments.empty())
    {
        return query_single_path(raw_input, segmentation, segments);
    }

    std::vector<WordItem> result;
    for (size_t count = segments.size(); count > 0; --count)
    {
        quanpin::Segments partial_segments(segments.begin(), segments.begin() + static_cast<std::ptrdiff_t>(count));
        const std::string partial_segmentation = quanpin::join_segments(partial_segments);
        const std::string partial_input = remove_delimiters(partial_segmentation);
        auto partial_result = query_single_path(partial_input, partial_segmentation, partial_segments);
        result.insert(result.end(), partial_result.begin(), partial_result.end());
    }

    if (result.size() < kSparsePinyinFallbackThreshold)
    {
        result = append_sparse_pinyin_fallbacks(segments, std::move(result));
    }

    return result;
}

std::vector<WordItem> QuanpinDictionary::query_single_path(const std::string &raw_input,
                                                           const std::string &segmentation,
                                                           const quanpin::Segments &segments)
{
    const std::string cache_key = segmentation.empty() ? raw_input : segmentation;
    if (auto cached = cache_.get(cache_key))
    {
        return cached.value();
    }

    std::vector<WordItem> result = query_database(segments, segmentation);
    result = append_ime_fallback(raw_input, segmentation, std::move(result));
    cache_.insert(cache_key, result);
    return result;
}

quanpin::Segments QuanpinDictionary::resolve_segments(const std::string &raw_input, const std::string &segmentation)
{
    if (!segmentation.empty())
    {
        return quanpin::split_segments(segmentation);
    }

    return get_or_compute_segments(raw_input);
}

quanpin::Segments QuanpinDictionary::get_or_compute_segments(const std::string &raw_input)
{
    if (auto cached = segmentation_cache_.get(raw_input))
    {
        return cached.value();
    }

    const auto cuts = quanpin::cut_pinyin_by_mode(raw_input, "correction");
    const auto segments = cuts.empty() ? quanpin::Segments{} : cuts.front();
    segmentation_cache_.insert(raw_input, segments);
    return segments;
}

int QuanpinDictionary::handleVkCode(ImeKeyCode vk, ImeModifierMask modifiers_down, ImeCharacter wch)
{
    (void)modifiers_down;

    if (vk == ImeKey::Backspace)
    {
        if (!pinyin_sequence_.empty())
        {
            pinyin_sequence_.pop_back();
        }
    }
    else if (vk == ImeKey::Escape || vk == ImeKey::Return || vk == ImeKey::Space)
    {
        reset_state();
        return OK;
    }
    else if (vk == ImeKey::Apostrophe)
    {
        pinyin_sequence_.push_back('\'');
    }
    else if (is_alpha_vk(vk))
    {
        if (wch >= u'A' && wch <= u'Z')
        {
            pinyin_sequence_.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(wch))));
        }
        else if (wch >= u'a' && wch <= u'z')
        {
            pinyin_sequence_.push_back(static_cast<char>(wch));
        }
        else
        {
            pinyin_sequence_.push_back(static_cast<char>(vk + ('a' - 'A')));
        }
    }

    query(pinyin_sequence_);
    return OK;
}

std::vector<WordItem> QuanpinDictionary::query_database(const quanpin::Segments &segments,
                                                        const std::string &segmentation)
{
    if (db_ == nullptr)
    {
        return {};
    }

    try
    {
        if (segments.size() == 1 && segments.front().size() == 1)
        {
            constexpr int kInitialCandidateLimit = 24;
            auto result = query_initial(segments.front(), kInitialCandidateLimit);
            const std::string matched_code = segmentation.empty() ? segments.front() : segmentation;
            for (auto &item : result)
            {
                item.canonical_pinyin = item.pinyin;
                item.pinyin = matched_code;
            }
            return result;
        }

        const auto flat_items = quanpin::query_segments_keyed_flat(segments, db_, statement_cache_, INT_MAX);
        std::vector<WordItem> result;
        result.reserve(flat_items.size());
        const std::string code = segmentation.empty() ? quanpin::join_segments(segments) : segmentation;
        for (const auto &item : flat_items)
        {
            result.emplace_back(code, item.value, item.weight, CandidateSource::Database, item.key);
        }
        return result;
    }
    catch (const std::exception &ex)
    {
        (void)0;
        return {};
    }
}

std::vector<WordItem> QuanpinDictionary::query_initial(const std::string &code, int limit)
{
    if (db_ == nullptr || code.size() != 1)
    {
        return {};
    }

    const auto rows = quanpin::query_initial(db_, code, limit);
    std::vector<WordItem> result;
    result.reserve(rows.size());
    for (const auto &item : rows)
    {
        result.emplace_back(item.key, item.value, item.weight, CandidateSource::Database, item.key);
    }
    return result;
}

std::vector<WordItem> QuanpinDictionary::merge_alternative_segmentations(
    const std::string &raw_input, const std::string &primary_segmentation, const quanpin::Segments &primary_segments,
    const std::vector<quanpin::Segments> &alternative_segmentations, std::vector<WordItem> result)
{
    const auto alternative_items = quanpin::query_exact_segmentations_keyed_flat(
        alternative_segmentations, db_, statement_cache_, kAlternativeSegmentationCandidateLimit);
    if (alternative_items.empty())
    {
        return result;
    }

    const auto primary_full = query_single_path(raw_input, primary_segmentation, primary_segments);
    std::vector<WordItem> alternative_full;
    alternative_full.reserve(alternative_items.size());
    for (const auto &item : alternative_items)
    {
        alternative_full.emplace_back(item.key, item.value, item.weight, CandidateSource::Database, item.key);
    }

    std::vector<WordItem> merged_full = primary_full;
    merged_full.insert(merged_full.end(), alternative_full.begin(), alternative_full.end());
    std::stable_sort(merged_full.begin(), merged_full.end(),
                     [](const WordItem &lhs, const WordItem &rhs) { return lhs.weight > rhs.weight; });
    std::unordered_set<std::string> seen_full_words;
    merged_full.erase(std::remove_if(merged_full.begin(), merged_full.end(),
                                     [&](const WordItem &item) { return !seen_full_words.insert(item.word).second; }),
                      merged_full.end());

    // Weights rank candidates reliably within one pinyin key, but are not directly comparable across
    // different segmentations. Keep the best alternative interpretation visible without letting every
    // segmentation occupy a protected slot on the first page.
    const std::string &best_alternative_word = alternative_items.front().value;
    const auto best_alternative = std::find_if(merged_full.begin(), merged_full.end(), [&](const WordItem &item) {
        return item.word == best_alternative_word;
    });
    if (best_alternative != merged_full.end() &&
        static_cast<size_t>(std::distance(merged_full.begin(), best_alternative)) >
            kBestAlternativeSegmentationMaxIndex)
    {
        WordItem promoted = std::move(*best_alternative);
        merged_full.erase(best_alternative);
        merged_full.insert(merged_full.begin() + static_cast<std::ptrdiff_t>(kBestAlternativeSegmentationMaxIndex),
                           std::move(promoted));
    }

    std::vector<WordItem> merged = std::move(merged_full);
    const size_t primary_full_count = primary_full.size() < result.size() ? primary_full.size() : result.size();
    std::vector<WordItem> remaining(result.begin() + static_cast<std::ptrdiff_t>(primary_full_count), result.end());
    append_unique_words(merged, remaining);
    return merged;
}

std::vector<WordItem> QuanpinDictionary::append_ime_fallback(const std::string &raw_input,
                                                             const std::string &segmentation,
                                                             std::vector<WordItem> result)
{
    if (!result.empty())
    {
        return result;
    }

    const std::string normalized = remove_delimiters(segmentation.empty() ? raw_input : segmentation);
    const std::string sentence = search_sentence_from_ime_engine(normalized);
    if (sentence.empty())
    {
        return result;
    }

    const auto exists =
        std::find_if(result.begin(), result.end(), [&](const WordItem &item) { return item.word == sentence; });
    if (exists == result.end())
    {
        result.emplace_back(segmentation.empty() ? raw_input : segmentation, sentence, 1, CandidateSource::Fallback);
    }
    return result;
}

std::vector<WordItem> QuanpinDictionary::append_sparse_pinyin_fallbacks(const quanpin::Segments &segments,
                                                                        std::vector<WordItem> result)
{
    for (const auto &fallback_segments : quanpin::sparse_pinyin_fallback_segments(segments))
    {
        if (fallback_segments.empty())
        {
            continue;
        }

        const std::string fallback_segmentation = quanpin::join_segments(fallback_segments);
        const std::string fallback_input = remove_delimiters(fallback_segmentation);
        const auto fallback_result = query_single_path(fallback_input, fallback_segmentation, fallback_segments);
        append_unique_words(result, fallback_result);
    }
    return result;
}

void QuanpinDictionary::append_unique_words(std::vector<WordItem> &result, const std::vector<WordItem> &extra)
{
    for (const auto &item : extra)
    {
        const auto exists = std::find_if(result.begin(), result.end(),
                                         [&](const WordItem &existing) { return existing.word == item.word; });
        if (exists == result.end())
        {
            result.push_back(item);
        }
    }
}

int QuanpinDictionary::create_word(std::string pinyin, std::string word)
{
    pinyin = remove_delimiters(pinyin);
    const auto cuts = quanpin::cut_pinyin_by_mode(pinyin, "correction");
    if (cuts.empty())
    {
        return ERROR_CODE;
    }

    pinyin = quanpin::join_segments(cuts.front());
    const std::string jp = quanpin::segments_to_jianpin(cuts.front());
    if (!do_validate(pinyin, jp, word))
    {
        return ERROR_CODE;
    }

    if (check_data(build_sql_for_checking_word(pinyin, jp, word)))
    {
        return OK;
    }

    if (insert_data(build_sql_for_inserting_word(pinyin, jp, word)) != OK)
    {
        return ERROR_CODE;
    }
    (void)user_dictionary::record_user_insert(user_dictionary::default_user_db_path(),
                                              user_dictionary::DictionaryKind::Pinyin, pinyin, word, 10000);
    reset_cache();
    return OK;
}

int QuanpinDictionary::create_word_from_canonical_pinyin(std::string pinyin, std::string word)
{
    const auto segments = quanpin::split_segments(pinyin);
    const size_t han_count = HelpcodeUtils::count_han_chars(word);
    if (segments.empty() || segments.size() != han_count ||
        std::any_of(segments.begin(), segments.end(), [](const std::string &segment) {
            return segment.empty() || !quanpin::is_complete_pinyin_input(segment);
        }))
    {
        return ERROR_CODE;
    }

    pinyin = quanpin::join_segments(segments);
    const std::string jp = quanpin::segments_to_jianpin(segments);
    if (!do_validate(pinyin, jp, word))
    {
        return ERROR_CODE;
    }
    if (check_data(build_sql_for_checking_word(pinyin, jp, word)))
    {
        return OK;
    }
    if (insert_data(build_sql_for_inserting_word(pinyin, jp, word)) != OK)
    {
        return ERROR_CODE;
    }
    (void)user_dictionary::record_user_insert(user_dictionary::default_user_db_path(),
                                              user_dictionary::DictionaryKind::Pinyin, pinyin, word, 10000);
    reset_cache();
    return OK;
}

int QuanpinDictionary::update_weight_by_word(std::string word)
{
    return update_weight_by_pinyin_and_word(remove_delimiters(pinyin_segmentation_), std::move(word));
}

int QuanpinDictionary::update_weight_by_pinyin_and_word(std::string pinyin, std::string word)
{
    pinyin = remove_delimiters(pinyin);
    const auto cuts = quanpin::cut_pinyin_by_mode(pinyin, "correction");
    if (cuts.empty())
        return ERROR_CODE;
    auto segments = cuts.front();
    const size_t han_count = HelpcodeUtils::count_han_chars(word);
    if (segments.size() > han_count)
        segments.resize(han_count);
    const std::string normalized = quanpin::join_segments(segments);
    if (update_data(build_sql_for_updating_word(normalized, word)) != OK)
    {
        return ERROR_CODE;
    }
    (void)user_dictionary::record_pinyin_upsert_from_database(db_path_, normalized, word);
    reset_cache();
    return OK;
}

int QuanpinDictionary::delete_by_pinyin_and_word(std::string pinyin, std::string word)
{
    pinyin = remove_delimiters(pinyin);
    const auto cuts = quanpin::cut_pinyin_by_mode(pinyin, "correction");
    if (cuts.empty())
        return ERROR_CODE;
    const std::string normalized = quanpin::join_segments(cuts.front());
    if (delete_data(build_sql_for_deleting_word(normalized, word)) != OK)
    {
        return ERROR_CODE;
    }
    (void)user_dictionary::record_delete(user_dictionary::default_user_db_path(),
                                         user_dictionary::DictionaryKind::Pinyin, normalized, word);
    reset_cache();
    return OK;
}

int QuanpinDictionary::insert_word_to_series_cache(const std::string &pinyin, const std::string &word,
                                                   CandidateSource source)
{
    if (pinyin.empty() || word.empty())
    {
        return ERROR_CODE;
    }

    const auto cuts = quanpin::cut_pinyin_by_mode(pinyin, "correction");
    const std::string segmentation = cuts.empty() ? pinyin : quanpin::join_segments(cuts.front());
    const std::string cache_key = series_cache_key(pinyin, segmentation);
    auto list = series_cache_.get(cache_key).value_or(std::vector<WordItem>{});

    // Keep at most one cloud/AI suggestion in the series cache for this key.
    if (source == CandidateSource::AiSuggestion || source == CandidateSource::CloudSuggestion)
    {
        list.erase(
            std::remove_if(list.begin(), list.end(), [source](const WordItem &item) { return item.source == source; }),
            list.end());
    }

    const auto exists = std::find_if(list.begin(), list.end(), [&](const WordItem &item) { return item.word == word; });
    if (exists == list.end())
    {
        if (list.empty())
        {
            list.emplace_back(pinyin, word, 1, source);
        }
        else
        {
            const size_t index = source == CandidateSource::AiSuggestion ? std::min<size_t>(2, list.size()) : 1;
            list.insert(list.begin() + index, WordItem(pinyin, word, 1, source));
        }
    }

    series_cache_.insert(cache_key, list);
    return OK;
}

std::string QuanpinDictionary::search_sentence_from_ime_engine(const std::string &user_pinyin)
{
    if (!decoder_ready_)
    {
        return "";
    }

    const char *pinyin = user_pinyin.c_str();
    const size_t cand_cnt = ime_pinyin::im_search(pinyin, strlen(pinyin));
    for (size_t i = 0; i < cand_cnt; ++i)
    {
        ime_pinyin::char16 buf[256] = {0};
        ime_pinyin::im_get_candidate(i, buf, 255);
        size_t len = 0;
        while (buf[len] != 0 && len < 255)
        {
            ++len;
        }
        if (len > 0)
        {
            return from_utf16(buf, len);
        }
    }
    return "";
}

void QuanpinDictionary::reset_state()
{
    pinyin_sequence_.clear();
    pinyin_segmentation_.clear();
    current_candidate_list_.clear();
}

void QuanpinDictionary::reset_cache()
{
    cache_.clear();
    series_cache_.clear();
    segmentation_cache_.clear();
}

void QuanpinDictionary::reset_cache_if_database_changed()
{
    if (db_ == nullptr)
    {
        return;
    }
    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(db_, "PRAGMA data_version", -1, &statement, nullptr) != SQLITE_OK)
    {
        return;
    }
    if (sqlite3_step(statement) != SQLITE_ROW)
    {
        sqlite3_finalize(statement);
        return;
    }
    const sqlite3_int64 current_version = sqlite3_column_int64(statement, 0);
    sqlite3_finalize(statement);
    if (data_version_ >= 0 && current_version != data_version_)
    {
        reset_cache();
    }
    data_version_ = current_version;
}

std::vector<std::string> QuanpinDictionary::select_data(const std::string &sql_str)
{
    std::vector<std::string> candidate_list;
    if (db_ == nullptr)
    {
        return candidate_list;
    }

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql_str.c_str(), -1, &stmt, 0) != SQLITE_OK)
    {
        (void)0;
        return candidate_list;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        candidate_list.push_back(std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2))));
    }
    sqlite3_finalize(stmt);
    return candidate_list;
}

std::vector<WordItem> QuanpinDictionary::select_complete_data(const std::string &sql_str)
{
    std::vector<WordItem> candidate_list;
    if (db_ == nullptr)
    {
        return candidate_list;
    }

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql_str.c_str(), -1, &stmt, 0) != SQLITE_OK)
    {
        (void)0;
        return candidate_list;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        candidate_list.emplace_back(std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0))),
                                    std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2))),
                                    sqlite3_column_int64(stmt, 3), CandidateSource::Database,
                                    std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0))));
    }
    sqlite3_finalize(stmt);
    return candidate_list;
}

int QuanpinDictionary::check_data(const std::string &sql_str)
{
    if (db_ == nullptr)
    {
        return false;
    }

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql_str.c_str(), -1, &stmt, 0) != SQLITE_OK)
    {
        (void)0;
        return false;
    }

    const bool exists = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return exists;
}

int QuanpinDictionary::insert_data(const std::string &sql_str)
{
    if (db_ == nullptr)
    {
        return ERROR_CODE;
    }

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql_str.c_str(), -1, &stmt, 0) != SQLITE_OK)
    {
        (void)0;
        return ERROR_CODE;
    }
    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok ? OK : ERROR_CODE;
}

int QuanpinDictionary::update_data(const std::string &sql_str)
{
    if (db_ == nullptr)
    {
        return ERROR_CODE;
    }

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql_str.c_str(), -1, &stmt, 0) != SQLITE_OK)
    {
        (void)0;
        return ERROR_CODE;
    }
    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok ? OK : ERROR_CODE;
}

int QuanpinDictionary::delete_data(const std::string &sql_str)
{
    if (db_ == nullptr)
    {
        return ERROR_CODE;
    }

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql_str.c_str(), -1, &stmt, 0) != SQLITE_OK)
    {
        (void)0;
        return ERROR_CODE;
    }
    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok ? OK : ERROR_CODE;
}

std::string QuanpinDictionary::build_sql_for_creating_word(const std::string &pinyin)
{
    const auto cuts = quanpin::cut_pinyin_by_mode(pinyin, "correction");
    if (cuts.empty())
    {
        return "";
    }

    std::string sql;
    for (size_t i = 1; i <= cuts.front().size(); ++i)
    {
        std::vector<std::string> partial(cuts.front().begin(), cuts.front().begin() + i);
        const std::string key = quanpin::join_segments(partial);
        const std::string table = quanpin::build_table_name(partial);
        const std::string each =
            fmt::format("select * from(select * from {} where key = '{}' order by weight desc)", table, key);
        sql = sql.empty() ? each : each + " union all " + sql;
    }
    return sql;
}

std::string QuanpinDictionary::build_sql_for_checking_word(const std::string &key, const std::string &jp,
                                                           const std::string &value)
{
    const auto cuts = quanpin::cut_pinyin_by_mode(key, "correction");
    if (cuts.empty())
    {
        return "";
    }
    const std::string table = quanpin::build_table_name(cuts.front());
    return fmt::format("select 1 from {} where key = '{}' and value = '{}';", table, escape_sql_text(key),
                       escape_sql_text(value));
}

std::string QuanpinDictionary::build_sql_for_inserting_word(const std::string &key, const std::string &jp,
                                                            const std::string &value)
{
    const auto cuts = quanpin::cut_pinyin_by_mode(key, "correction");
    if (cuts.empty())
    {
        return "";
    }
    const std::string table = quanpin::build_table_name(cuts.front());
    return fmt::format("insert into {} (key, jp, value, weight) values ('{}', '{}', '{}', '{}');", table,
                       escape_sql_text(key), escape_sql_text(jp), escape_sql_text(value), 10000);
}

std::string QuanpinDictionary::build_sql_for_updating_word(const std::string &word)
{
    return build_sql_for_updating_word(remove_delimiters(pinyin_segmentation_), word);
}

std::string QuanpinDictionary::build_sql_for_updating_word(std::string pinyin, const std::string &word)
{
    pinyin = remove_delimiters(pinyin);
    const auto cuts = quanpin::cut_pinyin_by_mode(pinyin, "correction");
    if (cuts.empty())
    {
        return "";
    }

    size_t han_cnt = HelpcodeUtils::count_han_chars(word);
    auto segments = cuts.front();
    if (segments.size() > han_cnt)
    {
        segments.resize(han_cnt);
    }

    pinyin = quanpin::join_segments(segments);
    const std::string jp = quanpin::segments_to_jianpin(segments);
    if (!do_validate(pinyin, jp, word))
    {
        return "";
    }

    const std::string table = quanpin::build_table_name(segments);
    return fmt::format("update {0} set weight = ( select MAX(weight) + 1 from {0} AS sub where sub.key = '{1}') "
                       "where key = '{1}' and value = '{2}';",
                       table, escape_sql_text(pinyin), escape_sql_text(word));
}

std::string QuanpinDictionary::build_sql_for_deleting_word(std::string pinyin, const std::string &word)
{
    pinyin = remove_delimiters(pinyin);
    const auto cuts = quanpin::cut_pinyin_by_mode(pinyin, "correction");
    if (cuts.empty())
    {
        return "";
    }

    const std::string normalized = quanpin::join_segments(cuts.front());
    const std::string jp = quanpin::segments_to_jianpin(cuts.front());
    if (!do_validate(normalized, jp, word))
    {
        return "";
    }

    return fmt::format("delete from {} where key = '{}' and value = '{}';", quanpin::build_table_name(cuts.front()),
                       escape_sql_text(normalized), escape_sql_text(word));
}

bool QuanpinDictionary::do_validate(const std::string &key, const std::string &jp, const std::string &value)
{
    const std::string pure_key = remove_delimiters(key);
    if (pure_key.empty())
    {
        return false;
    }

    const size_t han_count = HelpcodeUtils::count_han_chars(value);
    if (jp.size() != han_count)
    {
        return false;
    }

    const auto cuts = quanpin::cut_pinyin_by_mode(pure_key, "correction");
    if (cuts.empty())
    {
        return false;
    }

    return cuts.front().size() == han_count;
}
