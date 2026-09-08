#include "../include/metasequoia/session.h"
#include "input_session.h"
#include "nine_key_session.h"
#include <stdexcept>

namespace metasequoia
{
class Session::Impl
{
  public:
    explicit Impl(const SessionOptions &options)
        : session(options.scheme, options.shuangpin_profile, options.paths),
          nine_key(options.paths, options.learning, options.frequency, options.fuzzy_pinyin)
    {
        session.set_quanpin_autocorrect_enabled(options.autocorrect);
        session.set_fuzzy_pinyin_options(options.fuzzy_pinyin);
        session.set_quanpin_helpcode_enabled(options.helpcode);
        session.set_shuangpin_helpcode_enabled(options.helpcode);
        if (!session.set_helpcode_schema(options.helpcode_schema) ||
            !session.set_frequency_adjustment(options.frequency) || !session.set_english_input_options(options.english))
            throw std::invalid_argument("Invalid session options");
        session.set_local_mode_options(options.local_modes);
        session.set_mixed_expressive_options(options.expressive);
        session.enable_fixed_positions();
    }
    InputSession session;
    NineKeySession nine_key;
    bool nine_key_enabled = false;
};

Session::Session(SessionOptions options)
{
    options.paths.validate();
    impl_ = std::make_unique<Impl>(options);
    impl_->session.set_chinese_punctuation_enabled(options.chinese_punctuation);
    impl_->session.set_candidate_learning_enabled(options.learning);
}
Session::~Session() = default;
void Session::set_nine_key_enabled(bool enabled)
{
    impl_->nine_key.command(Command::Cancel);
    impl_->nine_key_enabled = enabled;
}
KeyResult Session::choose_nine_key_spelling(std::size_t index)
{
    return impl_->nine_key.choose_spelling(index);
}
KeyResult Session::character(char value, bool shift_only)
{
    if (impl_->nine_key_enabled && impl_->session.scheme() == SchemeType::Quanpin &&
        impl_->session.local_input_mode() == LocalInputMode::None && !impl_->session.dedicated_english_mode() &&
        impl_->session.preedit().empty() && value >= '2' && value <= '9')
        return impl_->nine_key.character(value);
    if (impl_->nine_key.active())
        return {};
    return impl_->session.handle_character(value, shift_only);
}
KeyResult Session::command(Command value)
{
    if (impl_->nine_key.active())
        return impl_->nine_key.command(value);
    return impl_->session.handle_command(value);
}
KeyResult Session::candidate_key(char value)
{
    if (impl_->nine_key.active())
        return value >= '1' && value <= '9' ? impl_->nine_key.select(value - '1') : KeyResult{};
    return impl_->session.handle_candidate_key(value);
}
KeyResult Session::punctuation(char value)
{
    if (impl_->nine_key.active())
    {
        auto result = impl_->session.handle_punctuation(value);
        if (!result.handled)
            return result;
        const auto composition = impl_->nine_key.finish(0);
        result.commit = composition.commit.value_or("") + result.commit.value_or("");
        return result;
    }
    return impl_->session.handle_punctuation(value);
}
KeyResult Session::select(std::size_t index)
{
    if (impl_->nine_key.active())
        return impl_->nine_key.select(index);
    return impl_->session.select_candidate(index);
}
KeyResult Session::select_edge(std::size_t index, CandidateEdge edge)
{
    if (impl_->nine_key.active())
        return {};
    return impl_->session.select_candidate_edge(index, edge);
}
KeyResult Session::pin(std::size_t index)
{
    if (impl_->nine_key.active())
        return impl_->nine_key.pin(index);
    return impl_->session.pin_candidate(index);
}
KeyResult Session::remove(std::size_t index)
{
    if (impl_->nine_key.active())
        return impl_->nine_key.remove(index);
    return impl_->session.remove_candidate(index);
}
KeyResult Session::fix_position(std::size_t index, int position)
{
    if (position < 1 || position > 5)
        return {};
    if (impl_->nine_key.active())
        return impl_->nine_key.set_position(index, position);
    return impl_->session.set_candidate_position(index, position);
}
KeyResult Session::clear_position(std::size_t index)
{
    if (impl_->nine_key.active())
        return impl_->nine_key.set_position(index, 0);
    return impl_->session.set_candidate_position(index, 0);
}
KeyResult Session::finish()
{
    return finish(0);
}
KeyResult Session::finish(std::size_t first_index)
{
    if (impl_->nine_key.active())
        return impl_->nine_key.finish(first_index);
    return impl_->session.finish_composition(first_index);
}
void Session::switch_scheme(SchemeType scheme)
{
    impl_->nine_key.command(Command::Cancel);
    impl_->session.switch_scheme(scheme);
}
bool Session::is_supported_helpcode_schema(const std::string &schema)
{
    return InputSession::is_supported_helpcode_schema(schema);
}
bool Session::set_helpcode_schema(const std::string &schema)
{
    return impl_->session.set_helpcode_schema(schema);
}
void Session::set_helpcode_enabled(bool enabled)
{
    impl_->session.set_quanpin_helpcode_enabled(enabled);
    impl_->session.set_shuangpin_helpcode_enabled(enabled);
}
void Session::set_dedicated_english(bool enabled)
{
    impl_->nine_key.command(Command::Cancel);
    impl_->session.set_dedicated_english_mode(enabled);
}
SessionSnapshot Session::snapshot() const
{
    if (impl_->nine_key.active())
        return impl_->nine_key.snapshot();
    const auto &session = impl_->session;
    return {session.scheme(),
            session.local_input_mode(),
            session.preedit(),
            session.raw_segmentation(),
            session.normalized_segmentation(),
            session.candidates(),
            session.dedicated_english_mode(),
            session.editing_text(),
            session.caret_position()};
}
std::optional<OnlineQuery> Session::online_query() const
{
    if (impl_->nine_key.active())
        return std::nullopt;
    return impl_->session.online_query();
}
bool Session::apply_online_candidate(const OnlineQuery &query, std::string candidate, CandidateSource source)
{
    if (impl_->nine_key.active())
        return false;
    return impl_->session.apply_online_candidate(query, std::move(candidate), source);
}
} // namespace metasequoia
