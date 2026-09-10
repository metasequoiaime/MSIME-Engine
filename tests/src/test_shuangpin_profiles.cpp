#include <metasequoia/session.h>
#include "../../core/data_path.h"
#include "../../english/english_dictionary.h"
#include "../../shuangpin/shuangpin_query.h"
#include "../../shuangpin/shuangpin_utils.h"
#include "test_directory_cleanup.h"

#include <sqlite3.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <initializer_list>
#include <stdexcept>
#include <string>

namespace
{
void require(bool condition, const std::string &message)
{
    if (!condition)
        throw std::runtime_error(message);
}

struct SyllableCase
{
    const char *code;
    const char *pinyin;
};

void check_syllables(const ShuangpinProfile &profile, std::initializer_list<SyllableCase> cases)
{
    for (const auto &item : cases)
    {
        const auto actual = ShuangpinUtil::cvt_single_sp_to_pinyin(item.code, profile);
        require(actual == item.pinyin,
                profile.name + ": " + item.code + " expected " + item.pinyin + ", got " + actual);
    }
}

void test_profiles()
{
    const auto &profile = GetShuangpinProfile("jiajia");
    require(&profile == &GetJiajiaShuangpinProfile() && profile.name == "jiajia", "Jiajia is not registered");
    require(&GetShuangpinProfile("xiaohe") == &GetXiaoheShuangpinProfile() &&
                &GetShuangpinProfile("") == &GetXiaoheShuangpinProfile() &&
                &GetShuangpinProfile("unknown-profile") == &GetXiaoheShuangpinProfile(),
            "Unknown profile or explicit Xiaohe stopped selecting Xiaohe");
    require(metasequoia::SessionOptions{}.shuangpin_profile.name == "xiaohe" &&
                ShuangpinUtil::cvt_single_sp_to_pinyin("hc") == "hao",
            "The default profile changed");
    require(&GetShuangpinProfile("ziranma") == &GetZiranmaShuangpinProfile() &&
                &GetShuangpinProfile("shoudao") == &GetShoudaoShuangpinProfile() &&
                &GetShuangpinProfile("microsoft") == &GetMicrosoftShuangpinProfile(),
            "An existing profile is no longer registered");

    // Independent expectations: see docs/shuangpin-jiajia.md. Do not generate these from the profile.
    check_syllables(profile, {{"aa", "a"},
                              {"as", "ai"},
                              {"af", "an"},
                              {"ag", "ang"},
                              {"ad", "ao"},
                              {"ee", "e"},
                              {"ew", "ei"},
                              {"er", "en"},
                              {"et", "eng"},
                              {"eq", "er"},
                              {"oo", "o"},
                              {"op", "ou"}});
    check_syllables(profile,
                    {{"ba", "ba"},  {"pa", "pa"},  {"ma", "ma"},  {"fa", "fa"},    {"da", "da"},    {"ta", "ta"},
                     {"na", "na"},  {"la", "la"},  {"ga", "ga"},  {"ka", "ka"},    {"ha", "ha"},    {"ji", "ji"},
                     {"qi", "qi"},  {"xi", "xi"},  {"ri", "ri"},  {"za", "za"},    {"ca", "ca"},    {"sa", "sa"},
                     {"vi", "zhi"}, {"ui", "chi"}, {"ii", "shi"}, {"vy", "zhong"}, {"ih", "shuang"}});
    check_syllables(profile,
                    {{"nq", "ning"},  {"bw", "bei"},  {"he", "he"},  {"rr", "ren"},  {"dt", "deng"},  {"gy", "gong"},
                     {"jy", "jiong"}, {"gu", "gu"},   {"ni", "ni"},  {"bo", "bo"},   {"go", "guo"},   {"kp", "kou"},
                     {"hs", "hai"},   {"hd", "hao"},  {"hf", "han"}, {"hg", "hang"}, {"jh", "jiang"}, {"gh", "guang"},
                     {"jj", "jian"},  {"jk", "jiao"}, {"jl", "jin"}, {"gz", "gun"},  {"gx", "guai"},  {"jx", "jue"},
                     {"lx", "lve"},   {"gc", "guan"}, {"gv", "gui"}, {"lv", "lv"},   {"jb", "jia"},   {"gb", "gua"},
                     {"jn", "jiu"},   {"jm", "jie"}});
    check_syllables(profile,
                    {{"ju", "ju"},   {"qu", "qu"},   {"xu", "xu"},  {"yu", "yu"},  {"jc", "juan"}, {"qc", "quan"},
                     {"xc", "xuan"}, {"yc", "yuan"}, {"jz", "jun"}, {"qz", "qun"}, {"xz", "xun"},  {"yz", "yun"},
                     {"qx", "que"},  {"xx", "xue"},  {"yx", "yue"}, {"nx", "nve"}, {"nv", "nv"},   {"nu", "nu"},
                     {"lu", "lu"},   {"jv", "ju"},   {"qv", "qu"},  {"xv", "xu"},  {"yv", "yu"}});
    check_syllables(profile, {{"ya", "ya"},
                              {"yf", "yan"},
                              {"yg", "yang"},
                              {"yd", "yao"},
                              {"ye", "ye"},
                              {"yi", "yi"},
                              {"yl", "yin"},
                              {"yq", "ying"},
                              {"yy", "yong"},
                              {"yp", "you"},
                              {"wa", "wa"},
                              {"ws", "wai"},
                              {"wf", "wan"},
                              {"wg", "wang"},
                              {"ww", "wei"},
                              {"wr", "wen"},
                              {"wt", "weng"},
                              {"wo", "wo"},
                              {"wu", "wu"}});
    // The shared decoder naturally accepts these vowel combinations without an alias table.
    check_syllables(profile, {{"ai", "ai"}, {"ao", "ao"}, {"ei", "ei"}, {"ou", "ou"}});
    // Historical decoder coverage and unsupported O-prefix input are unchanged.
    check_syllables(profile, {{"os", ""}, {"n;", ""}, {"qo", ""}, {"yo", ""}, {"a", ""}, {"aaa", ""}});
    require(shuangpin::normalize_input_with_delimiters("opeqasig", profile) == "ou'er'ai'shang",
            "The independently recorded Jiajia phrase no longer segments correctly");
    require(shuangpin::normalize_input_with_delimiters("jbgbjhghjygyjxgx", profile) ==
                "jia'gua'jiang'guang'jiong'gong'jue'guai",
            "Shared final keys do not resolve to valid syllables");

    check_syllables(
        GetXiaoheShuangpinProfile(),
        {{"hc", "hao"}, {"ul", "shuang"}, {"vs", "zhong"}, {"ii", "chi"}, {"ui", "shi"}, {"lv", "lv"}, {"nk", "ning"}});
    check_syllables(GetZiranmaShuangpinProfile(),
                    {{"hk", "hao"}, {"ud", "shuang"}, {"vs", "zhong"}, {"ny", "ning"}, {"lt", "lve"}, {"ah", "ang"}});
    check_syllables(GetShoudaoShuangpinProfile(), {{"hd", "hao"},
                                                   {"ex", "shuang"},
                                                   {"vh", "zhong"},
                                                   {"ue", "e"},
                                                   {"ui", "ei"},
                                                   {"uf", "eng"},
                                                   {"ei", "shi"},
                                                   {"ng", "ning"},
                                                   {"lb", "lve"}});
    check_syllables(GetMicrosoftShuangpinProfile(),
                    {{"hk", "hao"}, {"n;", "ning"}, {"ly", "lv"}, {"nv", "nve"}, {"or", "er"}, {"oh", "ang"}});
}

void create_dictionary(const std::filesystem::path &directory)
{
    std::filesystem::create_directories(directory);
    sqlite3 *database = nullptr;
    const int opened = sqlite3_open(metasequoia::path_to_utf8(directory / "msime.db").c_str(), &database);
    if (opened != SQLITE_OK)
    {
        sqlite3_close(database);
        throw std::runtime_error("Failed to open the shuangpin fixture");
    }
    const int result = sqlite3_exec(database,
                                    "CREATE TABLE tbl_1_n(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
                                    "INSERT INTO tbl_1_n VALUES('ning','n','宁',100);"
                                    "CREATE TABLE tbl_2_n(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
                                    "INSERT INTO tbl_2_n VALUES('ni''hao','nh','你好',100);"
                                    "CREATE TABLE tbl_2_s(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
                                    "INSERT INTO tbl_2_s VALUES('shuang''pin','sp','双拼',100);"
                                    "CREATE TABLE tbl_4_o(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
                                    "INSERT INTO tbl_4_o VALUES('ou''er''ai''shang','oeas','偶尔爱上',100);",
                                    nullptr, nullptr, nullptr);
    const std::string error = sqlite3_errmsg(database);
    sqlite3_close(database);
    require(result == SQLITE_OK, "Failed to create the shuangpin fixture: " + error);
    require(EnglishDictionary::ensure_schema(metasequoia::path_to_utf8(directory / "english.db")),
            "Failed to create the English fixture required by runtime preparation");
}

void type(metasequoia::Session &session, const std::string &input)
{
    for (char character : input)
        require(session.character(character).handled, "Public session rejected input: " + input);
}

void check_composition(metasequoia::Session &session, const std::string &input, const std::string &pinyin)
{
    const auto snapshot = session.snapshot();
    require(snapshot.editing_text == input && snapshot.normalized_segmentation == pinyin,
            "Expected " + input + " / " + pinyin + ", got " + snapshot.editing_text + " / " +
                snapshot.normalized_segmentation);
}

void select_word(metasequoia::Session &session, const std::string &word)
{
    const auto snapshot = session.snapshot();
    const auto found = std::find_if(snapshot.candidates.begin(), snapshot.candidates.end(),
                                    [&](const WordItem &item) { return item.word == word; });
    require(found != snapshot.candidates.end(), "Public session did not find fixture candidate: " + word);
    const auto selected = session.select(static_cast<std::size_t>(found - snapshot.candidates.begin()));
    require(selected.handled && selected.commit == word && session.snapshot().editing_text.empty(),
            "Selecting the fixture candidate did not commit and clear the composition");
}

void test_public_sessions()
{
    using namespace metasequoia;
    const auto root =
        std::filesystem::temp_directory_path() /
        ("msime-shuangpin-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    test::ScopedDataDirectoryCleanup cleanup(root);
    create_dictionary(root / "resources");
    SessionOptions options;
    options.paths = prepare_runtime_paths(root / "resources", root / "user", root / "cache", "fixture-v1");
    options.scheme = SchemeType::Shuangpin;
    options.shuangpin_profile = GetShuangpinProfile("jiajia");
    options.helpcode = false;
    options.learning = false;
    {
        Session session(options);
        type(session, "opeqasig");
        check_composition(session, "opeqasig", "ou'er'ai'shang");
        select_word(session, "偶尔爱上");

        type(session, "ihpl");
        check_composition(session, "ihpl", "shuang'pin");
        select_word(session, "双拼");

        type(session, "ni'hd");
        check_composition(session, "ni'hd", "ni'hao");
        require(session.command(Command::Backspace).handled, "Backspace was not handled");
        check_composition(session, "ni'h", "ni'h");
        type(session, "d");
        check_composition(session, "ni'hd", "ni'hao");
        select_word(session, "你好");

        type(session, "juju");
        check_composition(session, "juju", "ju'ju");
        session.command(Command::Backspace);
        check_composition(session, "juj", "ju'j");
        type(session, "u");
        check_composition(session, "juju", "ju'ju");
        session.command(Command::Cancel);

        type(session, "n");
        require(!session.character(';').handled, "Jiajia consumed Microsoft's ing key");
        check_composition(session, "n", "n");
        type(session, "q");
        select_word(session, "宁");
    }

    struct ExistingProfileCase
    {
        const char *name;
        const char *hello;
    };
    for (const auto &item :
         {ExistingProfileCase{"xiaohe", "nihc"}, {"ziranma", "nihk"}, {"shoudao", "nihd"}, {"microsoft", "nihk"}})
    {
        options.shuangpin_profile = GetShuangpinProfile(item.name);
        Session session(options);
        type(session, item.hello);
        check_composition(session, item.hello, "ni'hao");
        select_word(session, "你好");
        require(!session.character(';').handled, "A profile consumed a leading semicolon");
        type(session, "n");
        if (options.shuangpin_profile.name == "microsoft")
        {
            require(session.character(';').handled, "Microsoft no longer accepts the ing final");
            check_composition(session, "n;", "ning");
            require(!session.character(';').handled, "Microsoft accepted a semicolon outside a final position");
            session.command(Command::Backspace);
            check_composition(session, "n", "n");
            type(session, ";");
            select_word(session, "宁");
        }
        else
        {
            require(!session.character(';').handled, "A non-Microsoft profile consumed the ing key");
            check_composition(session, "n", "n");
        }
    }
}
} // namespace

int main()
{
    try
    {
        test_profiles();
        test_public_sessions();
        return 0;
    }
    catch (const std::exception &error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
