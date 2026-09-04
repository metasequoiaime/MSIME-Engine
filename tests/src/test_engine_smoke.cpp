#include "../../core/ime_session.h"
#include "../../core/data_path.h"
#include "../../english/english_dictionary.h"
#include "../../japanese/romaji_converter.h"
#include "../../user_dictionary/user_dictionary_journal.h"
#include "test_directory_cleanup.h"

#include <sqlite3.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace
{
class Database
{
  public:
    explicit Database(const std::filesystem::path &path)
    {
        if (sqlite3_open(metasequoia::path_to_utf8(path).c_str(), &database_) != SQLITE_OK)
        {
            throw std::runtime_error("Failed to create the test dictionary.");
        }
    }

    ~Database()
    {
        sqlite3_close(database_);
    }

    void execute(const char *sql)
    {
        char *error = nullptr;
        if (sqlite3_exec(database_, sql, nullptr, nullptr, &error) != SQLITE_OK)
        {
            const std::string message = error == nullptr ? "SQLite operation failed." : error;
            sqlite3_free(error);
            throw std::runtime_error(message);
        }
    }

    bool containsUserDictionaryOperation(const std::string &value)
    {
        sqlite3_stmt *statement = nullptr;
        if (sqlite3_prepare_v2(database_,
                               "SELECT 1 FROM user_dictionary_operations WHERE value=?1 LIMIT 1",
                               -1, &statement, nullptr) != SQLITE_OK)
        {
            throw std::runtime_error("Failed to query the user dictionary journal.");
        }
        const bool bound = sqlite3_bind_text(statement, 1, value.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK;
        const bool found = bound && sqlite3_step(statement) == SQLITE_ROW;
        sqlite3_finalize(statement);
        return found;
    }

    std::int64_t queryInteger(const char *sql)
    {
        sqlite3_stmt *statement = nullptr;
        if (sqlite3_prepare_v2(database_, sql, -1, &statement, nullptr) != SQLITE_OK ||
            sqlite3_step(statement) != SQLITE_ROW)
        {
            sqlite3_finalize(statement);
            throw std::runtime_error("Failed to query the test dictionary.");
        }
        const std::int64_t value = sqlite3_column_int64(statement, 0);
        sqlite3_finalize(statement);
        return value;
    }

  private:
    sqlite3 *database_ = nullptr;
};
} // namespace

int run_test()
{
    if (japanese::HiraganaToKatakana("かな") != "カナ" || japanese::HiraganaToRomaji("カナ") != "kana")
    {
        throw std::runtime_error("Kana conversion changed during the platform port.");
    }

    const auto unique_suffix = std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    const std::filesystem::path data_directory = std::filesystem::temp_directory_path() / std::filesystem::u8path("metasequoia-engine-词库-" + unique_suffix);
    metasequoia::test::ScopedDataDirectoryCleanup cleanup(data_directory);
    std::filesystem::create_directories(data_directory);
#ifdef _WIN32
    if (_wputenv_s(L"METASEQUOIA_IME_DATA_DIR", data_directory.c_str()) != 0)
#else
    if (setenv("METASEQUOIA_IME_DATA_DIR", metasequoia::path_to_utf8(data_directory).c_str(), 1) != 0)
#endif
    {
        throw std::runtime_error("Failed to set the data directory override.");
    }

    {
        Database database(data_directory / "msime.db");
        database.execute("CREATE TABLE tbl_2_n(key TEXT, jp TEXT, value TEXT, weight INTEGER)");
        database.execute("INSERT INTO tbl_2_n VALUES('ni''hao', 'nh', '你好', 100)");

        ImeSession session(SchemeType::Quanpin);
        for (const char character : std::string("nihao"))
        {
            const ImeKeyCode key_code = static_cast<ImeKeyCode>(character - ('a' - 'A'));
            session.handle_key(key_code, 0, static_cast<ImeCharacter>(character));
        }

        if (session.get_preedit() != "nihao")
        {
            throw std::runtime_error("Quanpin preedit does not contain the typed input.");
        }
        const auto &candidates = session.get_candidates();
        if (std::none_of(candidates.begin(), candidates.end(), [](const WordItem &item) { return item.word == "你好"; }))
        {
            throw std::runtime_error("Quanpin did not return the candidate stored in the dictionary.");
        }

        session.handle_key(ImeKey::Backspace);
        if (session.get_preedit() != "niha")
        {
            throw std::runtime_error("Backspace did not update the quanpin preedit.");
        }
    }

    const std::filesystem::path user_database = data_directory / "msime_user.db";
    const std::filesystem::path english_database = data_directory / "english.db";
    if (!EnglishDictionary::ensure_schema(metasequoia::path_to_utf8(english_database)) ||
        !std::filesystem::exists(english_database))
    {
        throw std::runtime_error("English schema initialization did not create a missing database.");
    }
    if (!user_dictionary::record_upsert(metasequoia::path_to_utf8(user_database),
                                        user_dictionary::DictionaryKind::Pinyin, "ni'hao", "首次", 100))
    {
        throw std::runtime_error("Failed to open the persistent default user dictionary.");
    }
    user_dictionary::close_default_user_database();
    const std::filesystem::path previous_user_database = data_directory / "msime_user.previous.db";
    std::filesystem::rename(user_database, previous_user_database);
    if (!user_dictionary::record_upsert(metasequoia::path_to_utf8(user_database),
                                        user_dictionary::DictionaryKind::Pinyin, "ni'hao", "重开", 200))
    {
        throw std::runtime_error("The persistent default user dictionary did not reopen after close.");
    }
    user_dictionary::close_default_user_database();
    {
        Database previous(previous_user_database);
        Database current(user_database);
        if (!previous.containsUserDictionaryOperation("首次") ||
            previous.containsUserDictionaryOperation("重开") ||
            !current.containsUserDictionaryOperation("重开") ||
            current.containsUserDictionaryOperation("首次"))
        {
            throw std::runtime_error("The reopened default user dictionary wrote through the stale handle.");
        }
    }

    if (!user_dictionary::record_upsert(metasequoia::path_to_utf8(user_database),
                                        user_dictionary::DictionaryKind::English, "hello", "Hello", 300, "Hello"))
    {
        throw std::runtime_error("Failed to record the English replay fixture.");
    }
    user_dictionary::close_default_user_database();
    const auto successful_replay = user_dictionary::replay(
        metasequoia::path_to_utf8(user_database), metasequoia::path_to_utf8(data_directory / "msime.db"),
        metasequoia::path_to_utf8(english_database));
    if (!successful_replay.error.empty() || successful_replay.applied != 2)
    {
        throw std::runtime_error(
            "Replay did not apply the Pinyin and attached English operations: error=" +
            successful_replay.error + ", applied=" + std::to_string(successful_replay.applied) +
            ", failed=" + std::to_string(successful_replay.failed) +
            ", skipped=" + std::to_string(successful_replay.skipped));
    }
    {
        Database english(english_database);
        if (english.queryInteger("SELECT weight FROM english_words WHERE word='hello' AND display='Hello'") != 300)
        {
            throw std::runtime_error("Replay did not persist the attached English operation.");
        }
    }

    const std::filesystem::path empty_user_database = data_directory / "msime_user.empty.db";
    if (!user_dictionary::ensure_user_database(metasequoia::path_to_utf8(empty_user_database)))
    {
        throw std::runtime_error("Failed to create the empty replay journal.");
    }
    {
        Database locked_main(data_directory / "msime.db");
        locked_main.execute("BEGIN EXCLUSIVE");
        const auto locked_replay = user_dictionary::replay(
            metasequoia::path_to_utf8(empty_user_database),
            metasequoia::path_to_utf8(data_directory / "msime.db"),
            metasequoia::path_to_utf8(english_database));
        locked_main.execute("ROLLBACK");
        if (locked_replay.error.empty())
        {
            throw std::runtime_error("Replay reported success after its main transaction failed to start.");
        }
    }

    const std::filesystem::path unreadable_user_database = data_directory / "msime_user.unreadable.db";
    {
        Database unreadable_journal(unreadable_user_database);
        unreadable_journal.execute(
            "CREATE VIEW user_dictionary_operations AS "
            "SELECT 'pinyin' AS dictionary,'ni' AS key,'你' AS value,'upsert' AS operation,"
            "100 AS weight,'' AS display,1 AS updated_at "
            "UNION ALL "
            "SELECT 'pinyin','bad',value,'upsert',1,'',2 FROM json_each('not-json')");
    }
    const auto unreadable_replay = user_dictionary::replay(
        metasequoia::path_to_utf8(unreadable_user_database),
        metasequoia::path_to_utf8(data_directory / "msime.db"),
        metasequoia::path_to_utf8(english_database));
    if (unreadable_replay.error.empty())
    {
        throw std::runtime_error("Replay reported success after the journal cursor failed.");
    }

    return 0;
}

int main()
{
    try
    {
        return run_test();
    }
    catch (const std::exception &exception)
    {
        std::fprintf(stderr, "%s\n", exception.what());
        return 1;
    }
}
