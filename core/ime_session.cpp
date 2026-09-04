#include "ime_session.h"
#include "../schemes/quanpin_scheme.h"
#include "../schemes/shuangpin_scheme.h"
#include "../schemes/wubi_scheme.h"
#include "../schemes/japanese_romaji_scheme.h"
#include "../shuangpin/shuangpin_query.h"
#include <stdexcept>

namespace
{
void ApplyShuangpinHelpcodeSegmentation(QueryRequest &request, const ShuangpinProfile &profile)
{
    if (request.scheme != SchemeType::Shuangpin || !request.enable_shuangpin_helpcode ||
        shuangpin::detect_active_double_helpcode_length(request.raw_input, request.raw_input_with_cases, profile) != 2)
    {
        return;
    }

    const size_t helpcode_length = 2;
    const std::string base_raw_input = request.raw_input.substr(0, request.raw_input.size() - helpcode_length);
    const std::string base_raw_input_with_cases =
        request.raw_input_with_cases.substr(0, request.raw_input_with_cases.size() - helpcode_length);
    const std::string base_segmentation = shuangpin::segment_input(base_raw_input, profile);
    const std::string help_codes =
        request.raw_input_with_cases.substr(request.raw_input_with_cases.size() - helpcode_length);

    request.raw_segmentation =
        shuangpin::apply_segmentation_cases(base_segmentation, base_raw_input_with_cases) + "'" + help_codes;
    request.normalized_segmentation = shuangpin::to_quanpin_segmentation(base_segmentation, profile) + "'" + help_codes;
    request.segmentation = request.normalized_segmentation;
}
} // namespace

ImeSession::ImeSession(SchemeType scheme_type, const ShuangpinProfile &shuangpin_profile)
    : provider_registry_(shuangpin_profile), shuangpin_profile_(shuangpin_profile), scheme_(create_scheme(scheme_type))
{
}

void ImeSession::handle_key(ImeKeyCode vk, ImeModifierMask modifiers_down, ImeCharacter wch)
{
    scheme_->handle_key(vk, modifiers_down, wch);
    // 查询并更新候选词列表
    refresh_candidates();
}

void ImeSession::switch_scheme(SchemeType scheme_type)
{
    scheme_ = create_scheme(scheme_type);
    state_ = CompositionState{};
}

void ImeSession::set_shuangpin_helpcode_enabled(bool enabled)
{
    enable_shuangpin_helpcode_ = enabled;
}

void ImeSession::set_quanpin_helpcode_enabled(bool enabled)
{
    enable_quanpin_helpcode_ = enabled;
}

void ImeSession::set_quanpin_autocorrect_enabled(bool enabled)
{
    enable_quanpin_autocorrect_ = enabled;
}

void ImeSession::replace_shuangpin_raw_input(const std::string &raw_input, const std::string &raw_input_with_cases)
{
    if (scheme_->type() != SchemeType::Shuangpin)
    {
        return;
    }

    auto *shuangpin_scheme = dynamic_cast<ShuangpinScheme *>(scheme_.get());
    if (!shuangpin_scheme)
    {
        return;
    }

    shuangpin_scheme->set_raw_input(raw_input, raw_input_with_cases);
    refresh_candidates();
}

void ImeSession::replace_quanpin_raw_input(const std::string &raw_input, const std::string &raw_input_with_cases)
{
    if (scheme_->type() != SchemeType::Quanpin)
    {
        return;
    }

    auto *quanpin_scheme = dynamic_cast<QuanpinScheme *>(scheme_.get());
    if (!quanpin_scheme)
    {
        return;
    }

    quanpin_scheme->set_raw_input(raw_input, raw_input_with_cases);
    refresh_candidates();
}

void ImeSession::replace_wubi_raw_input(const std::string &raw_input, const std::string &raw_input_with_cases)
{
    if (scheme_->type() != SchemeType::Wubi)
    {
        return;
    }

    auto *wubi_scheme = dynamic_cast<WubiScheme *>(scheme_.get());
    if (!wubi_scheme)
    {
        return;
    }

    wubi_scheme->set_raw_input(raw_input, raw_input_with_cases);
    refresh_candidates();
}

void ImeSession::reset()
{
    scheme_->reset();
    state_ = CompositionState{};
}

void ImeSession::reset_cache()
{
    provider_registry_.reset_cache(current_scheme_type());
    refresh_candidates();
}

int ImeSession::create_word(std::string pinyin, std::string word)
{
    return provider_registry_.create_word(current_scheme_type(), std::move(pinyin), std::move(word));
}

int ImeSession::update_weight_by_pinyin_and_word(std::string pinyin, std::string word)
{
    return provider_registry_.update_weight_by_pinyin_and_word(current_scheme_type(), std::move(pinyin),
                                                               std::move(word));
}

int ImeSession::delete_by_pinyin_and_word(std::string pinyin, std::string word)
{
    return provider_registry_.delete_by_pinyin_and_word(current_scheme_type(), std::move(pinyin), std::move(word));
}

int ImeSession::cache_dynamic_candidate(const std::string &pinyin, const std::string &word, CandidateSource source)
{
    return provider_registry_.cache_dynamic_candidate(current_scheme_type(), pinyin, word, source);
}

void ImeSession::replace_japanese_raw_input(const std::string &raw_input, const std::string &raw_input_with_cases)
{
    if (scheme_->type() != SchemeType::JapaneseRomaji)
        return;
    auto *japanese_scheme = dynamic_cast<JapaneseRomajiScheme *>(scheme_.get());
    if (!japanese_scheme)
        return;
    japanese_scheme->set_raw_input(raw_input, raw_input_with_cases);
    refresh_candidates();
}

int ImeSession::cache_dynamic_candidate_for_current_request(const std::string &word, CandidateSource source)
{
    return provider_registry_.cache_dynamic_candidate_for_request(state_.request, word, source);
}

SchemeType ImeSession::current_scheme_type() const
{
    return scheme_->type();
}

const std::string &ImeSession::get_preedit() const
{
    return state_.preedit;
}

const QueryRequest &ImeSession::get_request() const
{
    return state_.request;
}

const std::vector<WordItem> &ImeSession::get_candidates() const
{
    return state_.candidates;
}

std::optional<WordItem> ImeSession::find_candidate(const std::string &key, const std::string &value)
{
    return provider_registry_.find_candidate(current_scheme_type(), key, value);
}

bool ImeSession::expand_initial_candidates()
{
    return provider_registry_.expand_initial_candidates(state_.request, state_.candidates);
}

void ImeSession::refresh_candidates()
{
    state_.preedit = scheme_->get_preedit();
    state_.request = scheme_->build_request();
    state_.request.enable_shuangpin_helpcode = enable_shuangpin_helpcode_;
    state_.request.enable_quanpin_helpcode = enable_quanpin_helpcode_;
    state_.request.enable_quanpin_autocorrect = enable_quanpin_autocorrect_;
    ApplyShuangpinHelpcodeSegmentation(state_.request, shuangpin_profile_);

    if (!state_.request.valid)
    {
        state_.candidates.clear();
        return;
    }

    state_.candidates = provider_registry_.resolve(state_.request.scheme).query(state_.request);
}

std::unique_ptr<IInputScheme> ImeSession::create_scheme(SchemeType scheme_type) const
{
    switch (scheme_type)
    {
    case SchemeType::Shuangpin:
        return std::make_unique<ShuangpinScheme>(shuangpin_profile_);
    case SchemeType::Quanpin:
        return std::make_unique<QuanpinScheme>();
    case SchemeType::Wubi:
        return std::make_unique<WubiScheme>();
    case SchemeType::JapaneseRomaji:
        return std::make_unique<JapaneseRomajiScheme>();
    default:
        throw std::runtime_error("Unknown scheme type.");
    }
}
