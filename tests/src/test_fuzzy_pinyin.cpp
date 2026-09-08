#include <metasequoia/session.h>
#include "quanpin/fuzzy_pinyin.h"
#include "quanpin/quanpin_dictionary.h"
#include <sqlite3.h>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <stdexcept>

using namespace metasequoia;
void require(bool value, const std::string &message)
{
    if (!value)
        throw std::runtime_error(message);
}
void type(Session &session, const std::string &text)
{
    for (char c : text)
        require(session.character(c).handled, "character rejected");
}
std::size_t index(Session &session, const std::string &word)
{
    const auto snapshot = session.snapshot();
    for (std::size_t i = 0; i < snapshot.candidates.size(); ++i)
        if (snapshot.candidates[i].word == word)
            return i;
    throw std::runtime_error("Missing " + word + " for " + snapshot.preedit);
}
bool contains(const std::vector<WordItem> &items, const std::string &word)
{
    return std::any_of(items.begin(), items.end(), [&](const auto &item) { return item.word == word; });
}
int main()
{
    const auto directory =
        std::filesystem::temp_directory_path() /
        ("msime-fuzzy-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    struct Cleanup
    {
        std::filesystem::path path;
        ~Cleanup()
        {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }
    } cleanup{directory};
    try
    {
        sqlite3 *db = nullptr;
        require(sqlite3_open((directory / "msime.db").u8string().c_str(), &db) == SQLITE_OK, "open fixture");
        const auto insert = [&](const std::string &key, const std::string &word) {
            const auto segments = quanpin::split_segments(key);
            const auto table = quanpin::build_table_name(segments);
            std::string escaped;
            for (char c : key)
            {
                escaped += c;
                if (c == '\'')
                    escaped += c;
            }
            const auto sql = "CREATE TABLE IF NOT EXISTS " + table +
                             "(key TEXT,jp TEXT,value TEXT,weight INTEGER);INSERT INTO " + table + " VALUES('" +
                             escaped + "','','" + word + "',100);";
            require(sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK, "fixture insert " + key);
        };
        const std::vector<std::pair<std::string, std::string>> pairs = {
            {"zan", "zhan"}, {"can", "chan"}, {"san", "shan"}, {"na", "la"},      {"fa", "ha"},     {"ran", "lan"},
            {"ban", "bang"}, {"ben", "beng"}, {"bin", "bing"}, {"lian", "liang"}, {"guan", "guang"}};
        for (std::size_t i = 0; i < pairs.size(); ++i)
        {
            insert(pairs[i].first, "原" + std::to_string(i));
            insert(pairs[i].second, "糊" + std::to_string(i));
        }
        insert("zong", "宗");
        insert("zhong", "中");
        insert("guo", "国");
        insert("zhong'guo", "中国");
        sqlite3_close(db);
        std::filesystem::create_directories(directory / "helpcodes");
        std::ofstream(directory / "helpcodes" / "helpcode.txt") << "中=ab\n宗=cd\n国=ef\n";
        RuntimePaths paths{directory, directory, directory, directory};
        QuanpinDictionary dictionary({}, paths);
        for (std::size_t i = 0; i < pairs.size(); ++i)
        {
            FuzzyPinyinOptions fuzzy{1u << i};
            const auto forward = dictionary.query(pairs[i].first, pairs[i].first, false, fuzzy);
            require(contains(forward, "糊" + std::to_string(i)), "missing forward rule " + std::to_string(i));
            require(
                contains(dictionary.query(pairs[i].second, pairs[i].second, false, fuzzy), "原" + std::to_string(i)),
                "missing reverse rule");
            require(!contains(dictionary.query(pairs[i].first, pairs[i].first, false), "糊" + std::to_string(i)),
                    "fuzzy polluted exact cache");
            require(!contains(dictionary.query(pairs[i].first, pairs[i].first, false,
                                               FuzzyPinyinOptions{1u << ((i + 1) % pairs.size())}),
                              "糊" + std::to_string(i)),
                    "unselected rule expanded");
        }
        require(quanpin::fuzzy_syllables("zh", {0x7ff}) == std::vector<std::string>{"zh"},
                "incomplete initial changed");
        require(quanpin::fuzzy_syllables("bian", {1u << 6}) == std::vector<std::string>{"bian"},
                "an rule changed ian final");
        require(quanpin::fuzzy_segmentations(quanpin::Segments(20, "lan"), {0x7ff}).size() <= 63,
                "unbounded fuzzy beam");
        SessionOptions options;
        options.paths = paths;
        options.helpcode = false;
        options.autocorrect = false;
        options.learning = false;
        options.fuzzy_pinyin.rules = 1;
        Session session(options);
        type(session, "zongguo");
        auto view = session.snapshot();
        const auto selected = view.candidates[index(session, "中国")];
        require(selected.pinyin == "zong'guo" && selected.canonical_pinyin == "zhong'guo",
                "typed/canonical identity lost");
        require(session.select(index(session, "中")).commit == "中" && session.snapshot().editing_text == "guo",
                "partial selection consumed canonical length");
        require(session.finish().commit == "国", "remaining composition failed");
        type(session, "zhong");
        require(session.select(index(session, "宗")).commit == "宗" && session.snapshot().preedit.empty(),
                "reverse selection left input behind");
        session.set_nine_key_enabled(true);
        type(session, "9664");
        require(session.select(index(session, "中")).commit == "中" && session.snapshot().preedit.empty(),
                "nine-key fuzzy consumed wrong digit count");
        options.scheme = SchemeType::Shuangpin;
        Session doublePinyin(options);
        type(doublePinyin, "zsgo");
        require(doublePinyin.select(index(doublePinyin, "中")).commit == "中" &&
                    doublePinyin.snapshot().editing_text == "go",
                "shuangpin fuzzy consumption");
        require(doublePinyin.finish().commit == "国", "shuangpin suffix failed");
        options.helpcode = true;
        Session helped(options);
        type(helped, "zsaB");
        require(helped.select(index(helped, "中")).commit == "中" && helped.snapshot().preedit.empty(),
                "shuangpin helpcode fuzzy selection");
        options.scheme = SchemeType::Quanpin;
        Session helpedFull(options);
        type(helpedFull, "zongAB");
        require(helpedFull.select(index(helpedFull, "中")).commit == "中" && helpedFull.snapshot().preedit.empty(),
                "quanpin helpcode fuzzy selection");
        options.helpcode = false;
        options.learning = true;
        Session learned(options);
        type(learned, "zongguo");
        require(learned.select(index(learned, "中")).commit == "中", "learn prefix");
        require(learned.select(index(learned, "国")).commit == "国", "learn suffix");
        require(dictionary.find_candidate("zhong'guo", "中国").has_value(), "canonical phrase lost");
        require(!dictionary.find_candidate("zong'guo", "中国").has_value(), "learned mistyped pronunciation");
        options.scheme = SchemeType::Shuangpin;
        options.fuzzy_pinyin.rules = 0;
        Session exact(options);
        type(exact, "zsgo");
        require(!contains(exact.snapshot().candidates, "中国"), "session configuration leaked");
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < 200; ++i)
            dictionary.query("zongguo", "zong'guo", false, {0x7ff});
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        require(elapsed < 5000, "warm fuzzy queries exceeded 25 ms per query");
        std::cout << "200 warm fuzzy queries: " << elapsed << " ms\n";
        std::cout << "Fuzzy pinyin rules, cache isolation and session selection passed\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
