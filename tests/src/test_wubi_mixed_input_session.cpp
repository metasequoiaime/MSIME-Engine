#include "../../core/input_session.h"
#include "../../include/metasequoia/session.h"

#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>
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

    // A four-letter code the table answers keeps the limit, so wubi as typed is unchanged. This is
    // the half that matters: wode is four letters the table cannot answer, so it says nothing about
    // whether a matched code still refuses a fifth.
    metasequoia::InputSession matched(SchemeType::Wubi);
    matched.set_wubi_input_options(metasequoia::WubiInputOptions{true});
    const auto four = type(matched, "ffff");
    require(!four.empty(), "The four-letter code chosen for having wubi candidates answered with none.");
    matched.handle_character('a');
    require(matched.preedit() == "ffff",
            "A matched four-letter code accepted a fifth letter with mixed input enabled.");
    require(words(matched) == four, "A refused fifth letter still changed the candidates.");

    // Backspacing back under the limit returns to ordinary wubi behaviour.
    metasequoia::InputSession shortened(SchemeType::Wubi);
    shortened.set_wubi_input_options(metasequoia::WubiInputOptions{true});
    type(shortened, "women");
    shortened.handle_command(metasequoia::Command::Backspace);
    require(shortened.preedit() == "wome", "Backspace did not shorten an extended composition.");
}

void committing_a_spelling_keeps_the_rest_of_the_composition()
{
    // Selecting 你好 out of nihaoma commits that spelling and leaves ma composing. Wubi commits the
    // whole composition, so without this the letters the user had already typed were dropped.
    const auto tail_after_selecting = [](SchemeType scheme, bool mixed) {
        metasequoia::InputSession session(scheme);
        session.set_wubi_input_options(metasequoia::WubiInputOptions{mixed});
        type(session, "nihaoma");
        const auto &items = session.candidates();
        for (std::size_t index = 0; index < items.size(); ++index)
        {
            if (items[index].word == "你好")
            {
                session.select_candidate(index);
                return std::make_pair(session.preedit(), words(session));
            }
        }
        throw std::runtime_error("nihaoma did not offer 你好.");
    };

    const auto pinyin = tail_after_selecting(SchemeType::Quanpin, false);
    const auto mixed = tail_after_selecting(SchemeType::Wubi, true);
    require(mixed.first == pinyin.first, "Mixed input dropped the rest of the composition on selection.");
    // The tail stays with pinyin. ma is a code the wubi table happens to know, and answering it with
    // wubi would swap schemes underneath a spelling the user is still in the middle of.
    require(mixed.second == pinyin.second, "The rest of a composition was answered with wubi candidates.");
}

void the_setting_is_off_by_default()
{
    metasequoia::InputSession session(SchemeType::Wubi);
    require(type(session, "wode").empty(), "Mixed wubi input answered an unmatched code without being switched on.");
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
    // The setting decides which dictionary answers the code in hand, so the composition already on
    // screen is asked again rather than left showing the answer from before the switch.
    toggled.set_wubi_mixed_pinyin(true);
    require(!toggled.snapshot().candidates.empty(), "set_wubi_mixed_pinyin did not reach the live composition.");
    toggled.set_wubi_mixed_pinyin(false);
    require(toggled.snapshot().candidates.empty(), "Fallback candidates outlived the setting that produced them.");
}

// z opens no wubi code, so the key is dropped as typed. Dropping it in mixed input would not refuse
// the spelling, it would quietly turn it into another one, so the letter has to be accepted there.
void z_reaches_the_pinyin_fallback()
{
    metasequoia::InputSession plain(SchemeType::Wubi);
    type(plain, "zhongguo");
    require(plain.preedit() == "hong", "Wubi as typed accepted z or grew past four letters.");

    for (const char *code : {"zhongguo", "zi", "zuo"})
    {
        metasequoia::InputSession mixed(SchemeType::Wubi);
        mixed.set_wubi_input_options(metasequoia::WubiInputOptions{true});
        require(type(mixed, code) == quanpin_candidates(code),
                "A spelling containing z answered with something other than what quanpin answers.");
        require(mixed.preedit() == code, "Mixed input dropped the z out of the composition.");
    }
}

// Editing at the caret replaces the whole composition, and the replacement must not be clipped to
// the four letters a wubi code would take: the letters past the fourth would vanish as the caret
// moved through them.
void caret_editing_keeps_an_extended_composition()
{
    metasequoia::InputSession session(SchemeType::Wubi);
    session.set_wubi_input_options(metasequoia::WubiInputOptions{true});
    type(session, "nihao");
    session.handle_command(metasequoia::Command::MoveHome);
    session.handle_command(metasequoia::Command::MoveRight);
    session.handle_character('x');
    require(session.preedit() == "nxihao", "A caret insert lost the tail of an extended composition.");
    session.handle_character('y');
    require(session.preedit() == "nxyihao", "A second caret insert clipped the composition to four letters.");
}
} // namespace

int main()
{
    try
    {
        unmatched_codes_fall_back_to_quanpin();
        matched_codes_keep_their_own_candidates();
        the_length_limit_holds_until_the_table_fails();
        committing_a_spelling_keeps_the_rest_of_the_composition();
        the_setting_is_off_by_default();
        the_public_session_carries_the_setting();
        z_reaches_the_pinyin_fallback();
        caret_editing_keeps_an_extended_composition();
    }
    catch (const std::exception &error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    return 0;
}
