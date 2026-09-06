#include <metasequoia/session.h>
#include "../../core/data_path.h"
#include "../../contracts/assets/assets.h"
#include "../../english/english_dictionary.h"
#include "../../user_dictionary/user_dictionary_journal.h"
#include "test_directory_cleanup.h"
#include <sqlite3.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <stdexcept>

namespace
{
using namespace metasequoia;
void require(bool value, const char *message)
{
    if (!value)
        throw std::runtime_error(message);
}
void execute(const std::filesystem::path &path, const std::string &sql)
{
    sqlite3 *db = nullptr;
    require(sqlite3_open(path_to_utf8(path).c_str(), &db) == SQLITE_OK, "Fixture open failed");
    const int result = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
    sqlite3_close(db);
    require(result == SQLITE_OK, "Fixture SQL failed");
}
int count(const std::filesystem::path &path, const std::string &sql)
{
    sqlite3 *db = nullptr;
    require(sqlite3_open_v2(path_to_utf8(path).c_str(), &db, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK,
            "Query open failed");
    sqlite3_stmt *stmt = nullptr;
    require(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK, "Query prepare failed");
    require(sqlite3_step(stmt) == SQLITE_ROW, "Query failed");
    const int result = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return result;
}
std::string bytes(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
void type(Session &session, const std::string &input)
{
    for (char c : input)
        session.character(c);
}
bool has(const Session &session, const std::string &word)
{
    const auto view = session.snapshot();
    return std::any_of(view.candidates.begin(), view.candidates.end(),
                       [&](const auto &item) { return item.word == word; });
}
std::size_t index_of(const Session &session, const std::string &word)
{
    const auto view = session.snapshot();
    const auto item = std::find_if(view.candidates.begin(), view.candidates.end(),
                                   [&](const auto &candidate) { return candidate.word == word; });
    require(item != view.candidates.end(), "Missing test candidate");
    return static_cast<std::size_t>(item - view.candidates.begin());
}
} // namespace

int main()
{
    using namespace metasequoia;
    const auto root = std::filesystem::temp_directory_path() /
                      ("msime-remove-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    test::ScopedDataDirectoryCleanup cleanup(root);
    const auto resources = root / "resources";
    std::filesystem::create_directories(resources / "helpcodes");
    std::ofstream(resources / assets::helpcode_lantian) << "你=aa\n拟=cc\n";
    execute(resources / assets::main_dictionary,
            "CREATE TABLE tbl_1_n(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
            "INSERT INTO tbl_1_n VALUES('ni','n','你',10000);"
            "CREATE TABLE tbl_2_n(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
            "INSERT INTO tbl_2_n VALUES('ni''hao','nh','你好',10000),('ni''hao','nh','拟好',9000);"
            "CREATE TABLE wubi86(key TEXT,value TEXT,weight INTEGER);"
            "INSERT INTO wubi86 VALUES('wq','你好',10000),('wq','拟好',9000);");
    require(EnglishDictionary::ensure_schema(path_to_utf8(resources / assets::english_dictionary)),
            "English schema failed");
    execute(resources / assets::english_dictionary,
            "INSERT INTO english_words(word,display,weight) VALUES('hello','hello',100),('help','help',50);");
    const auto main_before = bytes(resources / assets::main_dictionary);
    const auto english_before = bytes(resources / assets::english_dictionary);
    // A later deletion must win over an earlier pin when the journal is replayed.
    for (const bool pin_first : {false, true})
        for (const auto scheme : {SchemeType::Quanpin, SchemeType::Shuangpin, SchemeType::Wubi})
        {
            const auto suffix = std::to_string(static_cast<int>(scheme)) + (pin_first ? "-pinned" : "");
            const auto user = root / ("user-" + suffix);
            const auto cache = root / ("cache-" + suffix);
            SessionOptions options;
            options.paths = prepare_runtime_paths(resources, user, cache, "v1");
            options.scheme = scheme;
            options.learning = false;
            const std::string input =
                scheme == SchemeType::Quanpin ? "nihao" : (scheme == SchemeType::Shuangpin ? "nihc" : "wq");
            {
                Session session(options);
                require(!session.remove(0).handled, "Empty removal consumed input");
                type(session, input);
                const auto before = session.snapshot();
                require(!session.remove(before.candidates.size()).handled, "Out of bounds removal accepted");
                if (pin_first)
                {
                    const auto pinned = session.pin(index_of(session, "拟好"));
                    require(pinned.handled && !pinned.commit && !pinned.diagnostic, "Pin before removal failed");
                }
                const auto result = session.remove(index_of(session, "拟好"));
                require(result.handled && !result.commit && !result.diagnostic, "Removal failed or committed text");
                require(session.snapshot().preedit == before.preedit && !has(session, "拟好") && has(session, "你好"),
                        "Removal did not refresh only the selected candidate");
            }
            options.paths = prepare_runtime_paths(resources, user, cache, "v2");
            {
                Session replayed(options);
                type(replayed, input);
                require(!has(replayed, "拟好") && has(replayed, "你好"),
                        "Removed phrase reappeared after upgrade replay");
            }
            options.paths = prepare_runtime_paths(resources, user, cache, "v1");
            Session rolled_back(options);
            type(rolled_back, input);
            require(!has(rolled_back, "拟好"), "Removed phrase reappeared after rollback");
        }
    for (const bool pin_first : {false, true})
        for (const int mode : {0, 1, 2})
        {
            SessionOptions options;
            const auto suffix = std::to_string(mode) + (pin_first ? "-pinned" : "");
            const auto user = root / ("english-user-" + suffix);
            const auto cache = root / ("english-cache-" + suffix);
            options.paths = prepare_runtime_paths(resources, user, cache, "v1");
            options.learning = false;
            options.english.mixed_candidates = mode == 2;
            {
                Session session(options);
                if (mode == 0)
                    session.set_dedicated_english(true);
                else if (mode == 1)
                    session.character('Y', true);
                type(session, "he");
                const auto before = session.snapshot().preedit;
                if (pin_first)
                {
                    const auto pinned = session.pin(index_of(session, "help"));
                    require(pinned.handled && !pinned.commit && !pinned.diagnostic,
                            "English pin before removal failed");
                }
                const auto result = session.remove(index_of(session, "help"));
                require(result.handled && !result.commit && !result.diagnostic &&
                            session.snapshot().preedit == before && !has(session, "help") && has(session, "hello"),
                        "English removal lost input or failed to refresh candidates");
            }
            options.paths = prepare_runtime_paths(resources, user, cache, "v2");
            Session replayed(options);
            replayed.set_dedicated_english(true);
            type(replayed, "he");
            require(!has(replayed, "help") && has(replayed, "hello"), "English deletion did not survive replay");
        }
    SessionOptions failure_options;
    failure_options.paths = prepare_runtime_paths(resources, root / "failure-user", root / "failure-cache", "v1");
    failure_options.learning = false;
    const auto journal = failure_options.paths.user(assets::user_journal);
    require(user_dictionary::ensure_user_database(path_to_utf8(journal)), "Journal fixture failed");
    execute(journal, "CREATE TRIGGER reject_removal BEFORE INSERT ON user_dictionary_operations "
                     "BEGIN SELECT RAISE(ABORT,'fixture rejection'); END;");
    {
        Session session(failure_options);
        type(session, "nihao");
        const auto before = session.snapshot().preedit;
        const auto result = session.remove(index_of(session, "拟好"));
        require(result.handled && result.diagnostic && !result.commit && has(session, "拟好") &&
                    session.snapshot().preedit == before,
                "Failed journal write lost composition");
        require(count(failure_options.paths.dictionary(assets::main_dictionary),
                      "SELECT count(*) FROM tbl_2_n WHERE value='拟好'") == 1,
                "Journal failure did not roll back deletion");
        session.command(Command::Cancel);
        type(session, "ni");
        require(!session.remove(index_of(session, "你")).handled, "Single character was not protected");
        session.command(Command::Cancel);
        session.set_dedicated_english(true);
        type(session, "he");
        const auto english_result = session.remove(index_of(session, "help"));
        require(english_result.diagnostic && has(session, "help"), "English journal failure was hidden");
        require(count(failure_options.paths.dictionary(assets::english_dictionary),
                      "SELECT count(*) FROM english_words WHERE word='help'") == 1,
                "English deletion was not rolled back");
    }
    // The legacy English caller uses the same rollback guarantee.
    require(!user_dictionary::delete_english_candidate(
                path_to_utf8(failure_options.paths.dictionary(assets::english_dictionary)), path_to_utf8(journal),
                "help", "help"),
            "Legacy English deletion concealed journal failure");
    require(count(failure_options.paths.dictionary(assets::english_dictionary),
                  "SELECT count(*) FROM english_words WHERE word='help'") == 1,
            "Legacy English deletion lost an unjournaled row");
    execute(journal, "DROP TRIGGER reject_removal");
    // Canonical syllable boundaries are data, even when joining them spells another valid key.
    execute(failure_options.paths.dictionary(assets::main_dictionary),
            "CREATE TABLE tbl_1_x(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
            "INSERT INTO tbl_1_x VALUES('xian','x','西安',100);"
            "CREATE TABLE tbl_2_x(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
            "INSERT INTO tbl_2_x VALUES('xi''an','xa','西安',100);");
    require(user_dictionary::delete_dictionary_candidate(
                path_to_utf8(failure_options.paths.dictionary(assets::main_dictionary)), path_to_utf8(journal),
                user_dictionary::DictionaryKind::Pinyin, "xi'an", "西安"),
            "Canonical deletion failed");
    require(count(failure_options.paths.dictionary(assets::main_dictionary),
                  "SELECT count(*) FROM tbl_1_x WHERE key='xian'") == 1 &&
                count(failure_options.paths.dictionary(assets::main_dictionary), "SELECT count(*) FROM tbl_2_x") == 0,
            "Deletion resegmented the canonical key");
    require(count(journal,
                  "SELECT count(*) FROM user_dictionary_operations WHERE operation='delete' AND key='xi''an'") == 1,
            "Deletion journal lost canonical boundaries");
    require(count(journal, "SELECT count(*) FROM user_dictionary_operations") == 1,
            "Failed removal left extra tombstones");
    require(main_before == bytes(resources / assets::main_dictionary) &&
                english_before == bytes(resources / assets::english_dictionary),
            "Removal modified immutable resources");
}
