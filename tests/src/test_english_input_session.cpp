#include "../../core/data_path.h"
#include "../../core/input_session.h"
#include "test_directory_cleanup.h"

#include <sqlite3.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
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
            throw std::runtime_error("Failed to create the English input-session test dictionary.");
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

    std::int64_t query_integer(const char *sql)
    {
        sqlite3_stmt *statement = nullptr;
        if (sqlite3_prepare_v2(database_, sql, -1, &statement, nullptr) != SQLITE_OK ||
            sqlite3_step(statement) != SQLITE_ROW)
        {
            sqlite3_finalize(statement);
            throw std::runtime_error("Failed to query the English input-session test dictionary.");
        }
        const std::int64_t value = sqlite3_column_int64(statement, 0);
        sqlite3_finalize(statement);
        return value;
    }

  private:
    sqlite3 *database_ = nullptr;
};

void require(bool condition, const char *message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

void type(metasequoia::InputSession &session, const std::string &text)
{
    for (const char character : text)
    {
        require(session.handle_character(character).handled, "An English test character was not handled.");
    }
}

void set_data_directory(const std::filesystem::path &directory)
{
#ifdef _WIN32
    if (_wputenv_s(L"METASEQUOIA_IME_DATA_DIR", directory.c_str()) != 0)
#else
    if (setenv("METASEQUOIA_IME_DATA_DIR", metasequoia::path_to_utf8(directory).c_str(), 1) != 0)
#endif
    {
        throw std::runtime_error("Failed to set the English test data directory.");
    }
}

void write_file(const std::filesystem::path &path, const std::string &contents)
{
    std::ofstream stream(path, std::ios::binary);
    stream << contents;
    if (!stream)
    {
        throw std::runtime_error("Failed to prepare the corrupt English dictionary fixture.");
    }
}

void prepare_main_database(const std::filesystem::path &directory, bool two_candidates)
{
    std::filesystem::create_directories(directory);
    Database database(directory / "msime.db");
    database.execute("CREATE TABLE tbl_1_n(key TEXT,jp TEXT,value TEXT,weight INTEGER)");
    database.execute("INSERT INTO tbl_1_n VALUES('ni','n','你',200)");
    if (two_candidates)
    {
        database.execute("INSERT INTO tbl_1_n VALUES('ni','n','倪',100)");
    }
}
} // namespace

int main()
{
    const auto suffix = std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / ("metasequoia-english-session-" + suffix);
    metasequoia::test::ScopedDataDirectoryCleanup cleanup(root);
    const std::filesystem::path english_mode_directory = root / "english-modes";
    prepare_main_database(english_mode_directory, true);
    {
        Database database(english_mode_directory / "english.db");
        database.execute(
            "CREATE TABLE english_words(word TEXT COLLATE BINARY NOT NULL,display TEXT NOT NULL,"
            "weight INTEGER NOT NULL DEFAULT 0,PRIMARY KEY(word,display)) WITHOUT ROWID");
        database.execute("CREATE TABLE en_zh_glosses(english TEXT PRIMARY KEY,chinese_gloss TEXT NOT NULL)");
        database.execute("CREATE TABLE zh_en_glosses(chinese TEXT PRIMARY KEY,english_gloss TEXT NOT NULL)");
        database.execute("INSERT INTO english_words VALUES('ni','Ni',300)");
        database.execute("INSERT INTO english_words VALUES('ninja','Ninja',200)");
        database.execute("INSERT INTO english_words VALUES('nimbus','Nimbus',100)");
        database.execute("INSERT INTO english_words VALUES('ni','倪',50)");
        database.execute("INSERT INTO english_words VALUES('hello','Hello',300)");
        database.execute("INSERT INTO english_words VALUES('help','Help',200)");
    }
    set_data_directory(english_mode_directory);

    metasequoia::EnglishInputOptions mixed_options;
    mixed_options.mixed_candidates = true;
    mixed_options.minimum_prefix = 2;
    metasequoia::InputSession mixed(SchemeType::Quanpin);
    require(mixed.set_english_input_options(mixed_options), "Valid mixed-English options were rejected.");
    type(mixed, "ni");
    require(mixed.candidates().size() == 5 && mixed.candidates()[0].word == "你" &&
                mixed.candidates()[1].word == "Ni" &&
                mixed.candidates()[1].source == CandidateSource::EnglishDictionary &&
                mixed.candidates()[2].word == "倪" && mixed.candidates()[3].word == "Ninja" &&
                mixed.candidates()[4].word == "Nimbus",
            "Mixed English candidates did not occupy the Windows-compatible slot after the leading Chinese candidate.");

    metasequoia::InputSession threshold(SchemeType::Quanpin);
    mixed_options.minimum_prefix = 3;
    require(threshold.set_english_input_options(mixed_options), "A valid English minimum prefix was rejected.");
    type(threshold, "ni");
    require(std::none_of(threshold.candidates().begin(), threshold.candidates().end(),
                         [](const WordItem &item) {
                             return item.source == CandidateSource::EnglishDictionary;
                         }),
            "Mixed English candidates appeared below the configured minimum prefix.");
    require(threshold.handle_character('n').handled &&
                std::any_of(threshold.candidates().begin(), threshold.candidates().end(),
                            [](const WordItem &item) {
                                return item.source == CandidateSource::EnglishDictionary;
                            }),
            "Mixed English candidates did not appear at the configured minimum prefix.");
    mixed_options.minimum_prefix = 0;
    require(!threshold.set_english_input_options(mixed_options), "An invalid English minimum prefix was accepted.");
    mixed_options.minimum_prefix = 9;
    require(!threshold.set_english_input_options(mixed_options),
            "An out-of-range English minimum prefix was accepted.");

    metasequoia::InputSession dedicated(SchemeType::Quanpin);
    dedicated.set_dedicated_english_mode(true);
    require(dedicated.dedicated_english_mode() && !dedicated.has_composition(),
            "Dedicated English mode did not start cleanly.");
    type(dedicated, "HE");
    require(dedicated.preedit() == "HE" && dedicated.candidates().size() == 2 &&
                dedicated.candidates()[0].word == "Hello" && dedicated.candidates()[1].word == "Help" &&
                std::all_of(dedicated.candidates().begin(), dedicated.candidates().end(),
                            [](const WordItem &item) {
                                return item.source == CandidateSource::EnglishDictionary;
                            }),
            "Dedicated English mode did not expose English-only prefix candidates.");
    const auto selected = dedicated.select_candidate(1);
    require(selected.handled && selected.commit == "Help" && dedicated.dedicated_english_mode() &&
                !dedicated.has_composition(),
            "Dedicated English candidate selection left the mode or committed the wrong text.");

    type(dedicated, "Codex");
    require(dedicated.candidates().size() == 1 && dedicated.candidates().front().word == "Codex" &&
                dedicated.candidates().front().source == CandidateSource::Generated,
            "Dedicated English mode did not expose a raw fallback.");
    const auto learned = dedicated.handle_command(metasequoia::Command::CommitRaw);
    require(learned.handled && learned.commit == "Codex" && dedicated.dedicated_english_mode() &&
                !dedicated.has_composition(),
            "Raw Enter did not commit and retain dedicated English mode.");
    {
        Database database(english_mode_directory / "english.db");
        require(database.query_integer(
                    "SELECT COUNT(*) FROM english_words WHERE word='codex' AND display='Codex'") == 1,
                "Raw Enter did not learn the dedicated English word.");
    }
    {
        Database database(english_mode_directory / "msime_user.db");
        require(database.query_integer(
                    "SELECT COUNT(*) FROM user_dictionary_operations WHERE dictionary='english' "
                    "AND key='codex' AND value='Codex' AND operation='upsert'") == 1,
                "Learned dedicated English was not journaled for upgrade replay.");
    }
    dedicated.set_dedicated_english_mode(false);
    require(!dedicated.dedicated_english_mode() && !dedicated.has_composition(),
            "Dedicated English mode did not exit cleanly.");

    const std::filesystem::path corrupt_directory = root / "english-corrupt";
    prepare_main_database(corrupt_directory, false);
    write_file(corrupt_directory / "english.db", "not a sqlite database");
    set_data_directory(corrupt_directory);
    mixed_options = {};
    mixed_options.mixed_candidates = true;
    metasequoia::InputSession corrupt(SchemeType::Quanpin);
    require(corrupt.set_english_input_options(mixed_options),
            "Valid mixed-English options were rejected for failure isolation.");
    type(corrupt, "ni");
    require(!corrupt.candidates().empty() && corrupt.candidates().front().word == "你" &&
                std::none_of(corrupt.candidates().begin(), corrupt.candidates().end(),
                             [](const WordItem &item) {
                                 return item.source == CandidateSource::EnglishDictionary;
                             }),
            "An English database failure suppressed local Chinese candidates.");

    const std::filesystem::path missing_directory = root / "english-missing";
    prepare_main_database(missing_directory, false);
    set_data_directory(missing_directory);
    metasequoia::InputSession missing(SchemeType::Quanpin);
    require(missing.set_english_input_options(mixed_options),
            "Valid mixed-English options were rejected for missing-database isolation.");
    type(missing, "ni");
    require(!missing.candidates().empty() && missing.candidates().front().word == "你" &&
                !std::filesystem::exists(missing_directory / "english.db"),
            "A missing English database suppressed Chinese candidates or was created by a read-only query.");

    return 0;
}
