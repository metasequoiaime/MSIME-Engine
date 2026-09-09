#include "../../core/input_session.h"
#include "../../include/metasequoia/session.h"

#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
void require(bool condition, const char *message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

std::vector<std::string> words(const metasequoia::InputSession &session)
{
    std::vector<std::string> result;
    result.reserve(session.candidates().size());
    for (const auto &candidate : session.candidates())
    {
        result.push_back(candidate.word);
    }
    return result;
}

std::vector<std::string> type(metasequoia::InputSession &session, const std::string &code)
{
    for (const char letter : code)
    {
        session.handle_character(letter);
    }
    return words(session);
}

std::vector<std::string> wubi_candidates(const std::string &code, bool mixed)
{
    metasequoia::InputSession session(SchemeType::Wubi);
    session.set_wubi_input_options(metasequoia::WubiInputOptions{mixed});
    return type(session, code);
}

std::vector<std::string> quanpin_candidates(const std::string &code)
{
    metasequoia::InputSession session(SchemeType::Quanpin);
    return type(session, code);
}

// Codes the wubi table cannot answer, each a spelling quanpin knows.
// women and nihao also exercise the relaxed length limit: both are longer than a wubi code.
const char *const kUnmatchedCodes[] = {"wode", "shu", "women", "nihao"};
// Codes the wubi table answers on its own. The mixed setting must not touch these: a fluent wubi
// typist would otherwise find pinyin candidates arriving alongside the code they meant.
const char *const kMatchedCodes[] = {"wo", "ni", "de"};

void unmatched_codes_fall_back_to_quanpin()
{
    for (const char *code : kUnmatchedCodes)
    {
        require(wubi_candidates(code, false).empty(),
                "A code chosen for having no wubi candidates answered with some.");
        const auto mixed = wubi_candidates(code, true);
        require(!mixed.empty(), "An unmatched wubi code stayed empty with mixed input enabled.");
        require(mixed == quanpin_candidates(code),
                "Mixed input answered an unmatched wubi code with something other than what quanpin "
                "answers for the same letters.");
    }
}

void matched_codes_keep_their_own_candidates()
{
    for (const char *code : kMatchedCodes)
    {
        const auto plain = wubi_candidates(code, false);
        require(!plain.empty(), "A code chosen for having wubi candidates answered with none.");
        require(wubi_candidates(code, true) == plain,
                "Mixed input changed the candidates for a code the wubi table already answers.");
    }
}

void the_length_limit_holds_until_the_table_fails()
{
    metasequoia::InputSession plain(SchemeType::Wubi);
    type(plain, "women");
    require(plain.preedit() == "wome", "A wubi composition grew past four letters without mixed input.");

    metasequoia::InputSession mixed(SchemeType::Wubi);
    mixed.set_wubi_input_options(metasequoia::WubiInputOptions{true});
    type(mixed, "women");
    require(mixed.preedit() == "women", "Mixed input did not let an unmatched code reach a full spelling.");

    // A code the table answers keeps the four-letter limit, so nothing changes for wubi as typed.
    metasequoia::InputSession matched(SchemeType::Wubi);
    matched.set_wubi_input_options(metasequoia::WubiInputOptions{true});
    type(matched, "wode");
    require(!matched.candidates().empty(), "The chosen four-letter code answered with nothing.");
}

void the_setting_is_off_by_default()
{
    metasequoia::InputSession session(SchemeType::Wubi);
    require(type(session, "wode").empty(),
            "Mixed wubi input answered an unmatched code without being switched on.");
}

void the_public_session_carries_the_setting()
{
    metasequoia::SessionOptions options;
    options.paths = metasequoia::RuntimePaths::legacy();
    options.scheme = SchemeType::Wubi;
    options.wubi.mixed_pinyin = true;
    metasequoia::Session session(options);
    for (const char letter : std::string("wode"))
    {
        session.character(letter);
    }
    require(!session.snapshot().candidates.empty(),
            "SessionOptions did not carry the mixed wubi setting into the session.");

    metasequoia::SessionOptions plain;
    plain.paths = metasequoia::RuntimePaths::legacy();
    plain.scheme = SchemeType::Wubi;
    metasequoia::Session toggled(plain);
    for (const char letter : std::string("wode"))
    {
        toggled.character(letter);
    }
    require(toggled.snapshot().candidates.empty(), "Mixed wubi input was on without being asked for.");
    toggled.set_wubi_mixed_pinyin(true);
    // The setting applies to the next query rather than rewriting the one already answered.
    toggled.command(metasequoia::Command::Cancel);
    for (const char letter : std::string("wode"))
    {
        toggled.character(letter);
    }
    require(!toggled.snapshot().candidates.empty(), "set_wubi_mixed_pinyin did not reach the session.");
}
} // namespace

int main()
{
    try
    {
        unmatched_codes_fall_back_to_quanpin();
        matched_codes_keep_their_own_candidates();
        the_length_limit_holds_until_the_table_fails();
        the_setting_is_off_by_default();
        the_public_session_carries_the_setting();
    }
    catch (const std::exception &error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    return 0;
}
