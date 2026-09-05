#pragma once

#include "ime_session.h"
#include "../local_modes/date_time_query.h"
#include "../english/english_dictionary.h"
#include "word_item.h"
#include "../quanpin/engine.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace metasequoia
{
// Editing intents a platform frontend maps its own key events onto. Printable input goes through
// handle_character instead, so this covers only the commands that act on an existing composition.
enum class Command
{
    Backspace,
    CommitCandidate,
    CommitRaw,
    Cancel,
};

// Outcome of one dispatched key or selection. `handled` tells the frontend whether to swallow the
// event, and `commit` carries the text the frontend should insert into the client application.
struct KeyResult
{
    bool handled = false;
    std::optional<std::string> commit;
    std::optional<std::string> diagnostic;
};

enum class CandidateEdge
{
    FirstHan,
    LastHan,
};

enum class FrequencyAdjustmentMode
{
    Disabled,
    Pin,
    Halve,
    Linear,
    Promote,
};

struct FrequencyAdjustmentOptions
{
    FrequencyAdjustmentMode mode = FrequencyAdjustmentMode::Disabled;
    int trigger_count = 1;
    int linear_step = 1;
};

enum class LocalInputMode
{
    None,
    Unicode,
    DateTime,
    QuickPhrase,
    Emoji,
    Kaomoji,
    SuperJianpin,
    TemporaryEnglish,
    TemporaryJapanese,
};

struct LocalModeOptions
{
    bool unicode = true;
    bool date_time = true;
    bool quick_phrase = true;
    bool emoji = true;
    bool kaomoji = true;
    bool super_jianpin = true;
    bool temporary_english = true;
    bool temporary_japanese = true;
};

struct EnglishInputOptions
{
    bool mixed_candidates = false;
    std::size_t minimum_prefix = 2;
};

struct MixedExpressiveOptions
{
    bool emoji_candidates = false;
    bool kaomoji_candidates = false;
};

// Immutable description of the current composition for asynchronous providers. Frontends copy
// this value into a request and return it unchanged with the result; InputSession revalidates it
// against the live composition before changing candidates.
struct OnlineQuery
{
    SchemeType scheme = SchemeType::Quanpin;
    std::uint64_t generation = 0;
    std::string identity;
    std::string query_text;
    std::string cache_key;
    std::vector<std::string> pinyin_segments;
    bool cloud_eligible = false;
    bool ai_eligible = false;
};

// Platform-neutral composition session shared by the native frontends. It owns an ImeSession and
// applies the key-handling and commit policy that each frontend would otherwise reimplement, so a
// frontend only has to translate platform key events into these calls.
class InputSession
{
  public:
    // Frontends pass their persisted options at session creation so every platform uses the same
    // engine configuration and commit policy.
    explicit InputSession(SchemeType scheme_type = SchemeType::Quanpin, bool quanpin_autocorrect_enabled = true,
                          bool helpcode_enabled = true, bool chinese_punctuation_enabled = true,
                          bool candidate_learning_enabled = true);
    InputSession(SchemeType scheme_type, const ShuangpinProfile &shuangpin_profile);

    // Feeds one lowercase ASCII letter or an in-composition apostrophe. Other input is rejected as
    // unhandled so the frontend can pass it through to the client application.
    KeyResult handle_character(char character, bool shift_only = false);
    // Applies a command. Every command is unhandled while no composition is active, which keeps
    // Backspace and Escape working normally in the client application.
    KeyResult handle_command(Command command);
    // Maps the visible 1-9 candidate keys and Chinese punctuation independently of platform UI.
    KeyResult handle_candidate_key(char character);
    KeyResult handle_punctuation(char character);
    KeyResult select_candidate(std::size_t index);
    KeyResult select_candidate(const std::string &candidate);
    KeyResult select_candidate_edge(std::size_t index, CandidateEdge edge);
    void set_shuangpin_helpcode_enabled(bool enabled);
    void set_quanpin_helpcode_enabled(bool enabled);
    static bool is_supported_helpcode_schema(const std::string &schema);
    static bool select_helpcode_schema(const std::string &schema);
    bool set_frequency_adjustment(FrequencyAdjustmentOptions options);
    const FrequencyAdjustmentOptions &frequency_adjustment() const;
    void set_local_mode_options(LocalModeOptions options);
    const LocalModeOptions &local_mode_options() const;
    bool set_english_input_options(EnglishInputOptions options);
    const EnglishInputOptions &english_input_options() const;
    void set_mixed_expressive_options(MixedExpressiveOptions options);
    const MixedExpressiveOptions &mixed_expressive_options() const;
    void set_dedicated_english_mode(bool enabled);
    bool dedicated_english_mode() const;
    LocalInputMode local_input_mode() const;
    void set_local_date_time_provider(std::function<local_modes::LocalDateTime()> provider);
    std::optional<OnlineQuery> online_query() const;
    bool apply_online_candidate(const OnlineQuery &query, std::string candidate, CandidateSource source);

    SchemeType scheme_type() const;
    bool quanpin_autocorrect_enabled() const;
    bool helpcode_enabled() const;
    bool chinese_punctuation_enabled() const;
    bool candidate_learning_enabled() const;
    // Switching schemes discards the current composition. A frontend that promises to preserve
    // typed text must commit it before calling this method.
    void switch_scheme(SchemeType scheme_type);
    SchemeType scheme() const;

    bool has_composition() const;
    const std::string &preedit() const;
    const std::string &raw_segmentation() const;
    const std::string &normalized_segmentation() const;
    const std::vector<WordItem> &candidates() const;

    // Advanced composition operations for hosts with their own asynchronous text insertion.
    // They share the same engine/configuration as the portable character/command API.
    struct SelectionTransition
    {
        bool continues_composition = false;
        std::string full_pure_pinyin;
        std::string current_segmentation;
        std::string current_segmentation_with_cases;
        std::string selected_canonical_pinyin;
    };

    struct CloudQueryState
    {
        bool should_query = false;
        std::string query_text;
        std::string cache_key;
        std::string committed_pinyin;
    };

    struct CreatingWordProgress
    {
        std::string pinyin;
        std::string word;
        std::string preedit;
        bool completed = false;
        bool can_store = false;
    };

    void handle_engine_key(ImeKeyCode vk, ImeModifierMask modifiers_down, ImeCharacter wch);
    void recompute_candidates();
    SchemeType current_scheme_type() const;

    void reset_state();
    void reset_cache();

    const std::vector<WordItem> &get_candidates() const;
    bool expand_initial_candidates();
    std::optional<WordItem> find_candidate(const std::string &key, const std::string &value);

    const std::string &get_pinyin_sequence() const;
    const std::string &get_pinyin_sequence_with_cases() const;
    const std::string &get_pure_pinyin_sequence() const;
    const std::string &get_pinyin_segmentation() const;
    std::string get_pinyin_segmentation_with_cases() const;
    std::string get_quanpin() const;
    bool is_all_complete_pure_pinyin() const;
    bool has_active_helpcode() const;

    void set_pinyin_sequence(const std::string &pinyin_sequence);
    void set_pinyin_sequence_with_cases(const std::string &pinyin_sequence);

    int store_user_phrase(std::string pinyin, std::string word);
    int store_user_phrase_from_canonical_pinyin(std::string pinyin, std::string word);
    int pin_candidate(std::string pinyin, std::string word);
    int remove_candidate(std::string pinyin, std::string word);
    int cache_dynamic_candidate(const std::string &pinyin, const std::string &word, CandidateSource source);
    SelectionTransition advance_composition_after_selection(const std::string &selected_pinyin,
                                                            const std::string &selected_word,
                                                            const std::string &selected_canonical_pinyin);
    CloudQueryState get_cloud_query_state() const;
    CreatingWordProgress update_creating_word_progress(const std::string &current_pinyin,
                                                       const std::string &current_word,
                                                       const std::string &selected_word,
                                                       const SelectionTransition &selection_transition) const;

    void set_quanpin_autocorrect_enabled(bool enabled);
    void set_shuangpin_preedit_uses_raw(bool enabled)
    {
        shuangpin_preedit_uses_raw_ = enabled;
    }

  private:
    const QueryRequest &request() const;
    bool is_shuangpin() const;
    bool is_wubi() const;
    bool is_japanese() const;
    void clear_pending_sequence();
    void apply_pending_sequence();

    KeyResult commit(std::size_t index);
    KeyResult handle_local_character(char character);
    std::optional<std::string> update_local_candidates();
    void update_mixed_candidates();
    void update_dedicated_english_candidates();
    EnglishDictionary &english_dictionary();
    void reset_composition();
    std::optional<std::string> learn_candidate(std::size_t index);

    bool shuangpin_preedit_uses_raw_ = true;
    std::unique_ptr<QuanpinEngine> canonical_phrase_engine_;
    std::string pending_pinyin_sequence_;
    std::string pending_pinyin_sequence_with_cases_;
    bool has_pending_pinyin_sequence_ = false;
    bool has_pending_pinyin_sequence_with_cases_ = false;

    ImeSession engine_;
    bool quanpin_autocorrect_enabled_ = true;
    bool quanpin_helpcode_enabled_ = true;
    bool shuangpin_helpcode_enabled_ = true;
    bool chinese_punctuation_enabled_ = true;
    bool candidate_learning_enabled_ = true;
    bool next_double_quote_is_opening_ = true;
    bool next_single_quote_is_opening_ = true;
    const ShuangpinProfile &shuangpin_profile_;
    FrequencyAdjustmentOptions frequency_adjustment_;
    bool frequency_adjustment_configured_ = false;
    LocalModeOptions local_mode_options_;
    EnglishInputOptions english_input_options_;
    MixedExpressiveOptions mixed_expressive_options_;
    std::unique_ptr<EnglishDictionary> english_dictionary_;
    bool dedicated_english_mode_ = false;
    std::string dedicated_english_preedit_;
    std::vector<WordItem> dedicated_english_candidates_;
    std::vector<WordItem> mixed_candidates_;
    LocalInputMode local_input_mode_ = LocalInputMode::None;
    std::optional<SchemeType> temporary_original_scheme_;
    std::string local_preedit_;
    std::vector<WordItem> local_candidates_;
    std::function<local_modes::LocalDateTime()> local_date_time_provider_;
    std::uint64_t online_generation_ = 0;
};
} // namespace metasequoia
