#include "../../core/input_session.h"
#include "../../core/data_path.h"
#include "../../user_dictionary/user_dictionary_journal.h"

#include <sqlite3.h>

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
            throw std::runtime_error("Failed to create the input-session test dictionary.");
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

void type(metasequoia::InputSession &session, const std::string &text)
{
    for (const char character : text)
    {
        if (!session.handle_character(character).handled)
        {
            throw std::runtime_error("A pinyin character was not handled.");
        }
    }
}

void require(bool condition, const char *message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}
} // namespace

int run_test()
{
    const auto unique_suffix = std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    const std::filesystem::path data_directory =
        std::filesystem::temp_directory_path() / std::filesystem::u8path("metasequoia-session-词库-" + unique_suffix);
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
        database.execute("INSERT INTO tbl_2_n VALUES('ni''hao', 'nh', '你好', 200)");
        database.execute("INSERT INTO tbl_2_n VALUES('ni''hao', 'nh', '拟好', 100)");
        database.execute("CREATE TABLE tbl_2_b(key TEXT, jp TEXT, value TEXT, weight INTEGER)");
        database.execute("INSERT INTO tbl_2_b VALUES('bu''hao', 'bh', '不好', 200)");
        database.execute("INSERT INTO tbl_2_b VALUES('bu''hao', 'bh', '补好', 100)");

        metasequoia::InputSession default_session;
        require(default_session.scheme_type() == SchemeType::Quanpin,
                "The default input scheme should be full pinyin.");
        require(default_session.quanpin_autocorrect_enabled(), "Pinyin autocorrect should be enabled by default.");
        require(default_session.helpcode_enabled(), "Helpcode should be enabled by default.");
        require(default_session.chinese_punctuation_enabled(), "Chinese punctuation should be enabled by default.");
        require(default_session.candidate_learning_enabled(), "Candidate learning should be enabled by default.");

        metasequoia::InputSession no_autocorrect_session(SchemeType::Quanpin, false);
        require(!no_autocorrect_session.quanpin_autocorrect_enabled(),
                "The requested pinyin autocorrect setting was not retained.");
        metasequoia::InputSession no_helpcode_session(SchemeType::Quanpin, true, false);
        require(!no_helpcode_session.helpcode_enabled(), "The requested helpcode setting was not retained.");

        metasequoia::InputSession shuangpin_session(SchemeType::Shuangpin);
        require(shuangpin_session.scheme_type() == SchemeType::Shuangpin,
                "The requested double-pinyin scheme was not retained.");
        require(shuangpin_session.handle_character('n').handled && shuangpin_session.preedit() == "n",
                "A valid Shuangpin letter was rejected.");
        metasequoia::InputSession japanese_session(SchemeType::JapaneseRomaji);
        require(japanese_session.handle_character('k').handled && japanese_session.preedit() == "k",
                "A valid Japanese romaji letter was rejected.");
        metasequoia::InputSession wubi_session(SchemeType::Wubi);
        require(!wubi_session.handle_character('z').handled && wubi_session.preedit().empty(),
                "An unsupported Wubi letter was swallowed.");
        type(wubi_session, "abcd");
        require(!wubi_session.handle_character('e').handled && wubi_session.preedit() == "abcd",
                "A Wubi letter beyond the four-code limit was swallowed.");
        require(!wubi_session.handle_character('\'').handled && wubi_session.preedit() == "abcd",
                "An unsupported Wubi apostrophe was swallowed.");

        metasequoia::InputSession ascii_punctuation_session(SchemeType::Quanpin, true, true, false);
        require(!ascii_punctuation_session.chinese_punctuation_enabled(),
                "The requested punctuation setting was not retained.");
        require(!ascii_punctuation_session.handle_punctuation('.').handled,
                "Disabled Chinese punctuation swallowed ASCII punctuation.");

        metasequoia::InputSession no_learning_session(SchemeType::Quanpin, true, true, true, false);
        require(!no_learning_session.candidate_learning_enabled(),
                "The requested candidate-learning setting was not retained.");
        type(no_learning_session, "buhao");
        require(no_learning_session.candidates().size() >= 2 && no_learning_session.candidates().front().word == "不好",
                "The learning test dictionary did not preserve its initial order.");
        const auto unlearned_selection = no_learning_session.select_candidate(1);
        require(unlearned_selection.handled && unlearned_selection.commit == "补好",
                "Candidate selection failed while learning was disabled.");
        metasequoia::InputSession verify_unlearned_session;
        type(verify_unlearned_session, "buhao");
        require(!verify_unlearned_session.candidates().empty() &&
                    verify_unlearned_session.candidates().front().word == "不好",
                "A selected candidate was learned while candidate learning was disabled.");

        metasequoia::InputSession uppercase_session;
        require(!uppercase_session.handle_character('N').handled,
                "An uppercase letter was swallowed while no composition was active.");
        type(uppercase_session, "ni");
        require(!uppercase_session.handle_character('H').handled && uppercase_session.preedit() == "ni",
                "An uppercase letter entered the active pinyin composition.");

        metasequoia::InputSession duplicate_apostrophe_session;
        type(duplicate_apostrophe_session, "ni");
        require(duplicate_apostrophe_session.handle_character('\'').handled,
                "The first Pinyin apostrophe was rejected.");
        require(!duplicate_apostrophe_session.handle_character('\'').handled &&
                    duplicate_apostrophe_session.preedit() == "ni'",
                "A duplicate Pinyin apostrophe was swallowed.");

        metasequoia::InputSession session(SchemeType::Quanpin, true, true, true, false);
        type(session, "nihao");
        require(session.preedit() == "nihao", "The preedit did not mirror the raw pinyin.");
        require(session.has_composition(), "Typing pinyin did not start a composition.");
        require(session.candidates().size() >= 2, "The engine did not return both dictionary candidates.");

        const auto selected = session.select_candidate(static_cast<std::size_t>(1));
        require(selected.handled && selected.commit == "拟好", "Selecting the second candidate committed wrong text.");
        require(!session.has_composition(), "Selecting a candidate did not end the composition.");

        type(session, "nihao");
        const auto by_word = session.select_candidate(std::string("拟好"));
        require(by_word.handled && by_word.commit == "拟好", "Selecting a candidate by word committed the wrong text.");

        type(session, "nihao");
        const auto out_of_range = session.select_candidate(session.candidates().size());
        require(!out_of_range.handled, "An out-of-range candidate index was accepted.");
        const auto unknown_word = session.select_candidate(std::string("没有这个词"));
        require(!unknown_word.handled, "An unknown candidate word was accepted.");

        const auto leading = session.handle_command(metasequoia::Command::CommitCandidate);
        require(leading.handled && leading.commit == "你好", "CommitCandidate did not commit the leading candidate.");

        type(session, "nihao");
        const auto composed_punctuation = session.handle_punctuation(',');
        require(composed_punctuation.handled && composed_punctuation.commit == "你好，",
                "Punctuation did not commit the candidate atomically.");
        require(session.handle_punctuation('.').commit == "。", "Idle Chinese punctuation was not converted.");
        require(session.handle_punctuation('"').commit == "“" && session.handle_punctuation('"').commit == "”",
                "Double quotes did not alternate between opening and closing Chinese quotes.");
        require(session.handle_punctuation('\'').commit == "‘" && session.handle_punctuation('\'').commit == "’",
                "Single quotes did not alternate between opening and closing Chinese quotes.");
        require(session.handle_punctuation('(').commit == "（" && session.handle_punctuation(')').commit == "）",
                "Parentheses were not converted to Chinese punctuation.");
        require(session.handle_punctuation('[').commit == "【" && session.handle_punctuation(']').commit == "】",
                "Square brackets were not converted to Chinese punctuation.");
        require(session.handle_punctuation('<').commit == "《" && session.handle_punctuation('>').commit == "》",
                "Book-title brackets were not converted to Chinese punctuation.");
        require(session.handle_punctuation('\\').commit == "、", "The enumeration comma was not converted.");

        type(session, "nihao");
        const auto digit = session.handle_candidate_key('2');
        require(digit.handled && digit.commit == "拟好", "The 2 key did not commit the second candidate.");

        metasequoia::InputSession learning_source_session;
        type(learning_source_session, "nihao");
        const auto learned_selection = learning_source_session.select_candidate(1);
        require(learned_selection.handled && learned_selection.commit == "拟好",
                "The learning source candidate was not selected.");
        metasequoia::InputSession learned_session;
        type(learned_session, "nihao");
        require(!learned_session.candidates().empty() && learned_session.candidates().front().word == "拟好",
                "Selecting a candidate did not promote it for the next matching input.");

        type(session, "nihao");
        session.handle_command(metasequoia::Command::Backspace);
        require(session.preedit() == "niha", "Backspace did not remove the last pinyin character.");
        const auto raw = session.handle_command(metasequoia::Command::CommitRaw);
        require(raw.handled && raw.commit == "niha", "CommitRaw did not commit the typed input.");

        type(session, "nihao");
        const auto cancel = session.handle_command(metasequoia::Command::Cancel);
        require(cancel.handled && !cancel.commit.has_value() && !session.has_composition(),
                "Cancel did not discard the composition.");

        require(!session.handle_character('1').handled, "A digit was swallowed instead of passed through.");
        require(!session.handle_command(metasequoia::Command::Backspace).handled,
                "Backspace was swallowed while no composition was active.");
        require(!session.handle_command(metasequoia::Command::CommitRaw).handled,
                "CommitRaw was swallowed while no composition was active.");
        require(!session.select_candidate(static_cast<std::size_t>(0)).handled,
                "A candidate was selected while no composition was active.");

        require(!session.handle_character('\'').handled, "An idle apostrophe was swallowed.");
    }

    user_dictionary::close_default_user_database();
    std::filesystem::remove_all(data_directory);
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
