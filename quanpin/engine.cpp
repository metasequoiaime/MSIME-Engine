#include "engine.h"
#include "../common/helpcode_utils.h"
#include "quanpin_query.h"
#include "quanpin_utils.h"

QuanpinEngine::QuanpinEngine() = default;

QuanpinEngine::~QuanpinEngine() = default;

std::optional<WordItem> QuanpinEngine::find_candidate(const std::string &key, const std::string &value)
{
    return dictionary_.find_candidate(key, value);
}

std::vector<WordItem> QuanpinEngine::query(const QueryRequest &request)
{
    if (!request.valid)
    {
        return {};
    }

    const size_t helpcode_length =
        request.enable_quanpin_helpcode
            ? quanpin::detect_active_helpcode_length(request.raw_input, request.raw_input_with_cases)
            : 0;

    if (helpcode_length == 2)
    {
        const std::string base_raw_input =
            quanpin::strip_active_helpcodes(request.raw_input, request.raw_input_with_cases);
        const auto cuts = quanpin::cut_pinyin_by_mode(base_raw_input, "correction");
        const std::string base_segmentation = quanpin::join_segments(cuts.front());
        const std::string help_codes = request.raw_input.substr(request.raw_input.size() - 2, 2);
        const auto base_candidates =
            dictionary_.query(base_raw_input, base_segmentation, request.enable_quanpin_autocorrect);
        return HelpcodeUtils::filter_candidates_with_double_helpcodes(base_candidates, help_codes);
    }

    if (helpcode_length == 1)
    {
        const std::string base_raw_input =
            quanpin::strip_active_helpcodes(request.raw_input, request.raw_input_with_cases);
        const auto cuts = quanpin::cut_pinyin_by_mode(base_raw_input, "correction");
        const std::string base_segmentation = quanpin::join_segments(cuts.front());
        const std::string help_code = request.raw_input.substr(request.raw_input.size() - 1, 1);
        const auto base_candidates =
            dictionary_.query(base_raw_input, base_segmentation, request.enable_quanpin_autocorrect);
        return HelpcodeUtils::reorder_candidates_with_single_helpcode(base_candidates, help_code);
    }

    return dictionary_.query(request.raw_input, request.segmentation, request.enable_quanpin_autocorrect);
}

bool QuanpinEngine::expand_initial_candidates(const QueryRequest &request, std::vector<WordItem> &candidates)
{
    if (request.scheme != SchemeType::Quanpin ||
        (request.enable_quanpin_helpcode &&
         quanpin::detect_active_helpcode_length(request.raw_input, request.raw_input_with_cases) > 0))
    {
        return false;
    }

    const auto segments = quanpin::split_segments(request.segmentation);
    if (segments.empty() || segments.front().size() != 1)
    {
        return false;
    }
    return dictionary_.expand_initial_candidates(segments.front(), candidates);
}

int QuanpinEngine::handleVkCode(ImeKeyCode vk, ImeModifierMask modifiers_down, ImeCharacter wch)
{
    return dictionary_.handleVkCode(vk, modifiers_down, wch);
}

int QuanpinEngine::create_word(std::string pinyin, std::string word)
{
    return dictionary_.create_word(std::move(pinyin), std::move(word));
}

int QuanpinEngine::create_word_from_canonical_pinyin(std::string pinyin, std::string word)
{
    return dictionary_.create_word_from_canonical_pinyin(std::move(pinyin), std::move(word));
}

int QuanpinEngine::update_weight_by_word(std::string word)
{
    return dictionary_.update_weight_by_word(std::move(word));
}

int QuanpinEngine::update_weight_by_pinyin_and_word(std::string pinyin, std::string word)
{
    return dictionary_.update_weight_by_pinyin_and_word(std::move(pinyin), std::move(word));
}

int QuanpinEngine::delete_by_pinyin_and_word(std::string pinyin, std::string word)
{
    return dictionary_.delete_by_pinyin_and_word(std::move(pinyin), std::move(word));
}

int QuanpinEngine::insert_word_to_series_cache(const std::string &pinyin, const std::string &word,
                                               CandidateSource source)
{
    return dictionary_.insert_word_to_series_cache(pinyin, word, source);
}

std::string QuanpinEngine::search_sentence_from_ime_engine(const std::string &user_pinyin)
{
    return dictionary_.search_sentence_from_ime_engine(user_pinyin);
}

void QuanpinEngine::reset_state()
{
    dictionary_.reset_state();
}

void QuanpinEngine::reset_cache()
{
    dictionary_.reset_cache();
}
