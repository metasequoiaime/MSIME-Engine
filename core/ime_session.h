#pragma once

#include "composition_state.h"
#include "scheme_type.h"
#include "../providers/provider_registry.h"
#include "../schemes/input_scheme.h"
#include "../shuangpin/shuangpin_profile.h"
#include <memory>

class ImeSession
{
  public:
    explicit ImeSession(SchemeType scheme_type = SchemeType::Shuangpin,
                        const ShuangpinProfile &shuangpin_profile = GetXiaoheShuangpinProfile());

    void handle_key(ImeKeyCode vk, ImeModifierMask modifiers_down = 0, ImeCharacter wch = 0);
    void switch_scheme(SchemeType scheme_type);
    void set_shuangpin_helpcode_enabled(bool enabled);
    void set_quanpin_helpcode_enabled(bool enabled);
    void set_quanpin_autocorrect_enabled(bool enabled);
    void replace_shuangpin_raw_input(const std::string &raw_input, const std::string &raw_input_with_cases);
    void replace_quanpin_raw_input(const std::string &raw_input, const std::string &raw_input_with_cases);
    void replace_wubi_raw_input(const std::string &raw_input, const std::string &raw_input_with_cases);
    void replace_japanese_raw_input(const std::string &raw_input, const std::string &raw_input_with_cases);
    void reset();
    void reset_cache();
    int create_word(std::string pinyin, std::string word);
    int update_weight_by_pinyin_and_word(std::string pinyin, std::string word);
    int delete_by_pinyin_and_word(std::string pinyin, std::string word);
    int cache_dynamic_candidate(const std::string &pinyin, const std::string &word, CandidateSource source);
    int cache_dynamic_candidate_for_current_request(const std::string &word, CandidateSource source);
    std::optional<WordItem> find_candidate(const std::string &key, const std::string &value);

    SchemeType current_scheme_type() const;
    const std::string &get_preedit() const;
    const QueryRequest &get_request() const;
    const std::vector<WordItem> &get_candidates() const;
    bool expand_initial_candidates();

  private:
    void refresh_candidates();
    std::unique_ptr<IInputScheme> create_scheme(SchemeType scheme_type) const;

  private:
    ProviderRegistry provider_registry_;
    const ShuangpinProfile &shuangpin_profile_;
    std::unique_ptr<IInputScheme> scheme_;
    CompositionState state_;
    bool enable_shuangpin_helpcode_ = false;
    bool enable_quanpin_helpcode_ = false;
    bool enable_quanpin_autocorrect_ = true;
};
