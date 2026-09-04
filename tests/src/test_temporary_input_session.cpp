#include "../../core/data_path.h"
#include "../../core/input_session.h"
#include "test_directory_cleanup.h"

#include <sqlite3.h>

#include <algorithm>
#include <chrono>
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
            throw std::runtime_error("Failed to create a temporary-mode test dictionary.");
        }
    }

    ~Database() { sqlite3_close(database_); }

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

bool contains(const std::vector<WordItem> &candidates, const std::string &word)
{
    return std::any_of(candidates.begin(), candidates.end(),
                       [&](const WordItem &candidate) { return candidate.word == word; });
}

void type(metasequoia::InputSession &session, const std::string &text)
{
    for (const char character : text)
    {
        require(session.handle_character(character).handled,
                "A temporary-mode test character was not handled.");
    }
}
} // namespace

int main()
{
    const auto suffix = std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    const auto data_directory =
        std::filesystem::temp_directory_path() / ("metasequoia-temporary-mode-" + suffix);
    metasequoia::test::ScopedDataDirectoryCleanup cleanup(data_directory);
    std::filesystem::create_directories(data_directory);
#ifdef _WIN32
    if (_wputenv_s(L"METASEQUOIA_IME_DATA_DIR", data_directory.c_str()) != 0)
#else
    if (setenv("METASEQUOIA_IME_DATA_DIR", metasequoia::path_to_utf8(data_directory).c_str(), 1) != 0)
#endif
    {
        throw std::runtime_error("Failed to set the temporary-mode test data directory.");
    }
    {
        Database main_database(data_directory / "msime.db");
        Database english_database(data_directory / "english.db");
        english_database.execute(
            "CREATE TABLE english_words(word TEXT COLLATE BINARY NOT NULL,display TEXT NOT NULL,"
            "weight INTEGER NOT NULL DEFAULT 0,PRIMARY KEY(word,display)) WITHOUT ROWID;"
            "CREATE TABLE en_zh_glosses(english TEXT PRIMARY KEY,chinese_gloss TEXT NOT NULL);"
            "CREATE TABLE zh_en_glosses(chinese TEXT PRIMARY KEY,english_gloss TEXT NOT NULL);"
            "INSERT INTO english_words VALUES('he','HE',110);"
            "INSERT INTO english_words VALUES('hello','Hello',100);"
            "INSERT INTO english_words VALUES('help','Help',90);");
    }

    metasequoia::InputSession english(SchemeType::Quanpin);
    require(english.handle_character('Y', true).handled &&
                english.local_input_mode() == metasequoia::LocalInputMode::TemporaryEnglish &&
                english.preedit() == "Y" && english.candidates().empty(),
            "Shift+Y did not enter an empty temporary English composition.");
    const auto bare_english_punctuation = english.handle_punctuation(',');
    require(bare_english_punctuation.handled && bare_english_punctuation.commit == "，" &&
                english.local_input_mode() == metasequoia::LocalInputMode::None && !english.has_composition(),
            "Engine punctuation committed or retained the bare temporary-English display prefix.");
    require(english.handle_character('Y', true).handled,
            "Temporary English could not restart after engine punctuation.");
    type(english, "he");
    require(english.preedit() == "Yhe" && english.candidates().size() == 3 &&
                english.candidates()[0].word == "he" &&
                english.candidates()[0].source == CandidateSource::Generated &&
                english.candidates()[1].word == "Hello" && english.candidates()[2].word == "Help" &&
                std::none_of(english.candidates().begin() + 1, english.candidates().end(),
                             [](const WordItem &candidate) { return candidate.word == "HE"; }),
            "Temporary English did not put raw input before deduplicated completions.");
    const auto english_commit = english.select_candidate(2);
    require(english_commit.handled && english_commit.commit == "Help" &&
                english.local_input_mode() == metasequoia::LocalInputMode::None &&
                english.scheme() == SchemeType::Quanpin,
            "Temporary English selection did not commit and return to Quanpin.");

    require(english.handle_character('Y', true).handled &&
                english.handle_command(metasequoia::Command::Backspace).handled &&
                english.local_input_mode() == metasequoia::LocalInputMode::None,
            "Backspace on a bare Y prefix did not leave temporary English mode.");
    require(english.handle_character('Y', true).handled,
            "Temporary English could not start for a bare-prefix commit.");
    const auto bare_english_commit = english.handle_command(metasequoia::Command::CommitCandidate);
    require(bare_english_commit.handled && !bare_english_commit.commit.has_value() &&
                english.local_input_mode() == metasequoia::LocalInputMode::None,
            "A bare temporary-English display prefix escaped into committed text.");
    require(english.handle_character('Y', true).handled, "Temporary English could not be re-entered.");
    type(english, "he");
    require(english.handle_command(metasequoia::Command::Cancel).handled &&
                english.local_input_mode() == metasequoia::LocalInputMode::None,
            "Cancel did not leave temporary English mode.");
    require(english.handle_character('Y', true).handled, "Temporary English could not start before a scheme switch.");
    english.switch_scheme(SchemeType::Shuangpin);
    require(english.scheme() == SchemeType::Shuangpin &&
                english.local_input_mode() == metasequoia::LocalInputMode::None && !english.has_composition(),
            "A scheme switch did not clear temporary English mode.");

    metasequoia::InputSession english_enter(SchemeType::Quanpin);
    require(english_enter.handle_character('Y', true).handled,
            "Temporary English could not start for raw Enter commit.");
    type(english_enter, "he");
    const auto english_raw = english_enter.handle_command(metasequoia::Command::CommitRaw);
    require(english_raw.handled && english_raw.commit == "he" &&
                english_enter.local_input_mode() == metasequoia::LocalInputMode::None,
            "Enter committed the display-only Y prefix with temporary English raw input.");

    metasequoia::InputSession english_frequency(SchemeType::Quanpin);
    require(english_frequency.set_frequency_adjustment(
                {metasequoia::FrequencyAdjustmentMode::Promote, 1, 1}),
            "A valid temporary-English frequency configuration was rejected.");
    require(english_frequency.handle_character('Y', true).handled,
            "Temporary English could not start for frequency learning.");
    type(english_frequency, "he");
    require(english_frequency.select_candidate(std::string("Help")).commit == "Help",
            "Temporary English could not select a completion for frequency learning.");
    metasequoia::InputSession english_reopened(SchemeType::Quanpin);
    require(english_reopened.handle_character('Y', true).handled,
            "Temporary English could not reopen after frequency learning.");
    type(english_reopened, "he");
    require(english_reopened.candidates().size() == 3 &&
                english_reopened.candidates()[0].word == "he" &&
                english_reopened.candidates()[1].word == "Help" &&
                english_reopened.candidates()[2].word == "Hello",
            "Temporary English frequency learning included the fixed raw candidate in ranking.");

    metasequoia::InputSession japanese(SchemeType::Quanpin);
    require(japanese.handle_character('R', true).handled &&
                japanese.local_input_mode() == metasequoia::LocalInputMode::TemporaryJapanese &&
                japanese.preedit() == "R" && japanese.scheme() == SchemeType::Quanpin &&
                japanese.scheme_type() == SchemeType::Quanpin,
            "Shift+R did not enter a temporary Japanese session with a visible prefix.");
    const auto bare_japanese_punctuation = japanese.handle_punctuation(',');
    require(bare_japanese_punctuation.handled && bare_japanese_punctuation.commit == "，" &&
                japanese.local_input_mode() == metasequoia::LocalInputMode::None &&
                japanese.scheme() == SchemeType::Quanpin && japanese.scheme_type() == SchemeType::Quanpin,
            "Engine punctuation committed the bare R prefix or failed to restore Quanpin.");
    require(japanese.handle_character('R', true).handled,
            "Temporary Japanese could not restart after engine punctuation.");
    type(japanese, "ka");
    require(japanese.preedit() == "Rka" && contains(japanese.candidates(), "か"),
            "Temporary Japanese did not reuse Romaji conversion candidates.");
    const auto japanese_commit = japanese.select_candidate(std::string("か"));
    require(japanese_commit.handled && japanese_commit.commit == "か" &&
                japanese.local_input_mode() == metasequoia::LocalInputMode::None &&
                japanese.scheme() == SchemeType::Quanpin,
            "Temporary Japanese selection did not restore the original Chinese scheme.");

    require(japanese.handle_character('R', true).handled &&
                japanese.handle_command(metasequoia::Command::Backspace).handled &&
                japanese.local_input_mode() == metasequoia::LocalInputMode::None &&
                japanese.scheme() == SchemeType::Quanpin,
            "Backspace on a bare R prefix did not restore Quanpin.");
    require(japanese.handle_character('R', true).handled,
            "Temporary Japanese could not start for a bare-prefix commit.");
    const auto bare_japanese_commit = japanese.handle_command(metasequoia::Command::CommitCandidate);
    require(bare_japanese_commit.handled && !bare_japanese_commit.commit.has_value() &&
                japanese.local_input_mode() == metasequoia::LocalInputMode::None &&
                japanese.scheme() == SchemeType::Quanpin,
            "A bare temporary-Japanese display prefix escaped into committed text.");
    require(japanese.handle_character('R', true).handled, "Temporary Japanese could not be re-entered.");
    type(japanese, "ka");
    require(japanese.handle_command(metasequoia::Command::Cancel).handled &&
                japanese.scheme() == SchemeType::Quanpin &&
                japanese.local_input_mode() == metasequoia::LocalInputMode::None,
            "Cancel did not restore the original scheme from temporary Japanese.");
    require(japanese.handle_character('R', true).handled, "Temporary Japanese could not start before a scheme switch.");
    japanese.switch_scheme(SchemeType::Shuangpin);
    require(japanese.scheme() == SchemeType::Shuangpin &&
                japanese.local_input_mode() == metasequoia::LocalInputMode::None && !japanese.has_composition(),
            "An explicit scheme switch did not supersede temporary Japanese restoration.");

    metasequoia::InputSession japanese_enter(SchemeType::Quanpin);
    require(japanese_enter.handle_character('R', true).handled,
            "Temporary Japanese could not start for raw Enter commit.");
    type(japanese_enter, "ka");
    const auto japanese_raw = japanese_enter.handle_command(metasequoia::Command::CommitRaw);
    require(japanese_raw.handled && japanese_raw.commit == "ka" &&
                japanese_enter.local_input_mode() == metasequoia::LocalInputMode::None &&
                japanese_enter.scheme() == SchemeType::Quanpin,
            "Enter committed the display-only R prefix or failed to restore Quanpin.");

    metasequoia::InputSession shuangpin_japanese(SchemeType::Shuangpin);
    require(shuangpin_japanese.handle_character('R', true).handled,
            "Temporary Japanese did not start from Shuangpin.");
    type(shuangpin_japanese, "ka");
    require(shuangpin_japanese.select_candidate(std::string("か")).commit == "か" &&
                shuangpin_japanese.scheme() == SchemeType::Shuangpin,
            "Temporary Japanese did not return to the original Shuangpin scheme.");

    metasequoia::LocalModeOptions disabled_options;
    disabled_options.temporary_english = false;
    disabled_options.temporary_japanese = false;
    metasequoia::InputSession disabled(SchemeType::Quanpin);
    disabled.set_local_mode_options(disabled_options);
    require(!disabled.handle_character('Y', true).handled && !disabled.has_composition() &&
                disabled.local_input_mode() == metasequoia::LocalInputMode::None,
            "Disabled temporary English swallowed Shift+Y.");
    require(!disabled.handle_character('R', true).handled && !disabled.has_composition() &&
                disabled.local_input_mode() == metasequoia::LocalInputMode::None,
            "Disabled temporary Japanese swallowed Shift+R.");

    return 0;
}
