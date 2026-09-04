#include "../../core/data_path.h"
#include "../../core/input_session.h"
#include "../../user_dictionary/user_dictionary_journal.h"
#include "test_directory_cleanup.h"

#include <sqlite3.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
class Database
{
  public:
    explicit Database(const std::filesystem::path &path)
    {
        if (sqlite3_open(metasequoia::path_to_utf8(path).c_str(), &database_) != SQLITE_OK)
        {
            throw std::runtime_error("Failed to create a mixed-expression test dictionary.");
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
        require(session.handle_character(character).handled,
                "A mixed-expression test character was not handled.");
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
        throw std::runtime_error("Failed to set the mixed-expression test data directory.");
    }
}

void prepare_main_database(const std::filesystem::path &directory)
{
    std::filesystem::create_directories(directory);
    Database database(directory / "msime.db");
    database.execute(
        "BEGIN;"
        "CREATE TABLE tbl_1_n(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
        "INSERT INTO tbl_1_n VALUES('ni','n','你',200);"
        "INSERT INTO tbl_1_n VALUES('ni','n','倪',100);"
        "COMMIT;");
}

void prepare_english_database(const std::filesystem::path &directory)
{
    Database database(directory / "english.db");
    database.execute(
        "BEGIN;"
        "CREATE TABLE english_words(word TEXT COLLATE BINARY NOT NULL,display TEXT NOT NULL,"
        "weight INTEGER NOT NULL DEFAULT 0,PRIMARY KEY(word,display)) WITHOUT ROWID;"
        "CREATE TABLE en_zh_glosses(english TEXT PRIMARY KEY,chinese_gloss TEXT NOT NULL);"
        "CREATE TABLE zh_en_glosses(chinese TEXT PRIMARY KEY,english_gloss TEXT NOT NULL);"
        "INSERT INTO english_words VALUES('ni','Ni',300);"
        "INSERT INTO english_words VALUES('ninja','Ninja',200);"
        "INSERT INTO english_words VALUES('ni','倪',100);"
        "COMMIT;");
}

void prepare_others_database(const std::filesystem::path &directory, bool emoji_table = true,
                             bool kaomoji_table = true)
{
    Database database(directory / "others.db");
    database.execute("BEGIN");
    if (emoji_table)
    {
        database.execute(
            "CREATE TABLE emoji_pinyin(key TEXT,emoji TEXT,sort_order INTEGER);"
            "INSERT INTO emoji_pinyin VALUES('ni','Ni',0);"
            "INSERT INTO emoji_pinyin VALUES('ni','😀',1);"
            "INSERT INTO emoji_pinyin VALUES('ni','😁',2);");
    }
    if (kaomoji_table)
    {
        database.execute(
            "CREATE TABLE kaomoji(pinyin TEXT,jianpin TEXT,kaomoji TEXT,sort_order INTEGER);"
            "INSERT INTO kaomoji VALUES('ni','n','😀',0);"
            "INSERT INTO kaomoji VALUES('ni','n','(^_^)',1);"
            "INSERT INTO kaomoji VALUES('ni','n','(T_T)',2);");
    }
    database.execute("COMMIT");
}

std::vector<CandidateSource> sources(const metasequoia::InputSession &session)
{
    std::vector<CandidateSource> result;
    result.reserve(session.candidates().size());
    for (const auto &candidate : session.candidates())
    {
        result.push_back(candidate.source);
    }
    return result;
}

bool has_source(const metasequoia::InputSession &session, CandidateSource source)
{
    return std::any_of(session.candidates().begin(), session.candidates().end(),
                       [source](const WordItem &candidate) { return candidate.source == source; });
}
} // namespace

int main()
{
    const auto suffix = std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / ("metasequoia-mixed-expression-" + suffix);
    metasequoia::test::ScopedDataDirectoryCleanup cleanup(root);

    const std::filesystem::path complete = root / "complete";
    prepare_main_database(complete);
    prepare_english_database(complete);
    prepare_others_database(complete);
    set_data_directory(complete);

    metasequoia::EnglishInputOptions english_options;
    english_options.mixed_candidates = true;
    metasequoia::MixedExpressiveOptions expressive_options;
    expressive_options.emoji_candidates = true;
    expressive_options.kaomoji_candidates = true;
    metasequoia::InputSession mixed(SchemeType::Quanpin);
    require(mixed.set_english_input_options(english_options), "Valid mixed-English options were rejected.");
    mixed.set_mixed_expressive_options(expressive_options);
    type(mixed, "ni");
    const std::vector<std::string> expected_words{
        "你", "Ni", "😀", "(^_^)", "倪", "Ninja", "😁", "(T_T)"};
    require(mixed.candidates().size() == expected_words.size(),
            "Mixed candidate deduplication produced the wrong candidate count.");
    for (std::size_t index = 0; index < expected_words.size(); ++index)
    {
        require(mixed.candidates()[index].word == expected_words[index],
                "Mixed candidates did not keep the Windows-compatible stable ordering.");
    }
    require(sources(mixed) ==
                std::vector<CandidateSource>{CandidateSource::Database,
                                             CandidateSource::EnglishDictionary,
                                             CandidateSource::Emoji,
                                             CandidateSource::Kaomoji,
                                             CandidateSource::Database,
                                             CandidateSource::EnglishDictionary,
                                             CandidateSource::Emoji,
                                             CandidateSource::Kaomoji},
            "Mixed candidate sources did not retain their priority groups.");

    metasequoia::InputSession below_threshold(SchemeType::Quanpin);
    require(below_threshold.set_english_input_options(english_options),
            "Valid mixed-English options were rejected below the expressive threshold.");
    below_threshold.set_mixed_expressive_options(expressive_options);
    type(below_threshold, "n");
    require(!has_source(below_threshold, CandidateSource::Emoji) &&
                !has_source(below_threshold, CandidateSource::Kaomoji),
            "Mixed expressive candidates appeared below the Windows two-character threshold.");

    metasequoia::MixedExpressiveOptions emoji_only_options;
    emoji_only_options.emoji_candidates = true;
    metasequoia::InputSession emoji_only(SchemeType::Quanpin);
    emoji_only.set_mixed_expressive_options(emoji_only_options);
    type(emoji_only, "ni");
    require(has_source(emoji_only, CandidateSource::Emoji) &&
                !has_source(emoji_only, CandidateSource::EnglishDictionary) &&
                !has_source(emoji_only, CandidateSource::Kaomoji),
            "The mixed Emoji toggle was not independent.");

    metasequoia::MixedExpressiveOptions kaomoji_only_options;
    kaomoji_only_options.kaomoji_candidates = true;
    metasequoia::InputSession kaomoji_only(SchemeType::Quanpin);
    kaomoji_only.set_mixed_expressive_options(kaomoji_only_options);
    type(kaomoji_only, "ni");
    require(has_source(kaomoji_only, CandidateSource::Kaomoji) &&
                !has_source(kaomoji_only, CandidateSource::EnglishDictionary) &&
                !has_source(kaomoji_only, CandidateSource::Emoji),
            "The mixed kaomoji toggle was not independent.");

    const std::filesystem::path missing_emoji_table = root / "missing-emoji-table";
    prepare_main_database(missing_emoji_table);
    prepare_english_database(missing_emoji_table);
    prepare_others_database(missing_emoji_table, false, true);
    set_data_directory(missing_emoji_table);
    metasequoia::InputSession emoji_failure(SchemeType::Quanpin);
    require(emoji_failure.set_english_input_options(english_options),
            "Valid mixed-English options were rejected during Emoji failure isolation.");
    emoji_failure.set_mixed_expressive_options(expressive_options);
    type(emoji_failure, "ni");
    require(emoji_failure.candidates().front().word == "你" &&
                has_source(emoji_failure, CandidateSource::EnglishDictionary) &&
                !has_source(emoji_failure, CandidateSource::Emoji) &&
                has_source(emoji_failure, CandidateSource::Kaomoji),
            "An Emoji query failure suppressed another mixed source or local Chinese candidates.");

    const std::filesystem::path missing_kaomoji_table = root / "missing-kaomoji-table";
    prepare_main_database(missing_kaomoji_table);
    prepare_english_database(missing_kaomoji_table);
    prepare_others_database(missing_kaomoji_table, true, false);
    set_data_directory(missing_kaomoji_table);
    metasequoia::InputSession kaomoji_failure(SchemeType::Quanpin);
    require(kaomoji_failure.set_english_input_options(english_options),
            "Valid mixed-English options were rejected during kaomoji failure isolation.");
    kaomoji_failure.set_mixed_expressive_options(expressive_options);
    type(kaomoji_failure, "ni");
    require(kaomoji_failure.candidates().front().word == "你" &&
                has_source(kaomoji_failure, CandidateSource::EnglishDictionary) &&
                has_source(kaomoji_failure, CandidateSource::Emoji) &&
                !has_source(kaomoji_failure, CandidateSource::Kaomoji),
            "A kaomoji query failure suppressed another mixed source or local Chinese candidates.");

    const std::filesystem::path missing_others = root / "missing-others";
    prepare_main_database(missing_others);
    prepare_english_database(missing_others);
    set_data_directory(missing_others);
    metasequoia::InputSession absent_expressive_database(SchemeType::Quanpin);
    require(absent_expressive_database.set_english_input_options(english_options),
            "Valid mixed-English options were rejected with a missing expressive database.");
    absent_expressive_database.set_mixed_expressive_options(expressive_options);
    type(absent_expressive_database, "ni");
    require(absent_expressive_database.candidates().front().word == "你" &&
                has_source(absent_expressive_database, CandidateSource::EnglishDictionary) &&
                !has_source(absent_expressive_database, CandidateSource::Emoji) &&
                !has_source(absent_expressive_database, CandidateSource::Kaomoji) &&
                !std::filesystem::exists(missing_others / "others.db"),
            "A missing expressive database suppressed local candidates or was created by a mixed query.");

    set_data_directory(complete);
    metasequoia::InputSession dedicated(SchemeType::Quanpin);
    dedicated.set_mixed_expressive_options(expressive_options);
    dedicated.set_dedicated_english_mode(true);
    type(dedicated, "ni");
    require(!has_source(dedicated, CandidateSource::Emoji) &&
                !has_source(dedicated, CandidateSource::Kaomoji),
            "Dedicated English mode leaked mixed expressive candidates.");

    user_dictionary::close_default_user_database();
    metasequoia::InputSession mixed_learning(SchemeType::Quanpin);
    require(mixed_learning.set_english_input_options(english_options),
            "Valid mixed-English options were rejected for local frequency learning.");
    mixed_learning.set_mixed_expressive_options(expressive_options);
    require(mixed_learning.set_frequency_adjustment(
                {metasequoia::FrequencyAdjustmentMode::Promote, 1, 1}),
            "A valid mixed-candidate frequency configuration was rejected.");
    type(mixed_learning, "ni");
    const auto learned_local = mixed_learning.select_candidate(std::string("倪"));
    require(learned_local.handled && learned_local.commit == "倪" &&
                !learned_local.diagnostic.has_value(),
            "Mixed candidates prevented a local Chinese candidate from being selected.");
    metasequoia::InputSession reopened_learning(SchemeType::Quanpin);
    type(reopened_learning, "ni");
    require(!reopened_learning.candidates().empty() && reopened_learning.candidates().front().word == "倪",
            "Mixed candidate sources polluted the local Chinese frequency rank calculation.");
    user_dictionary::close_default_user_database();

    return 0;
}
