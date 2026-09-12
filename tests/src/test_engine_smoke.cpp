#include "../../core/ime_session.h"
#include "../../core/data_path.h"
#include "../../core/query_request.h"
#include "../../english/english_dictionary.h"
#include "../../japanese/romaji_converter.h"
#include "../../providers/japanese_candidate_provider.h"
#include "../../user_dictionary/user_dictionary_journal.h"
#include "test_directory_cleanup.h"

#include <sqlite3.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
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
        if (sqlite3_prepare_v2(database_, "SELECT 1 FROM user_dictionary_operations WHERE value=?1 LIMIT 1", -1,
                               &statement, nullptr) != SQLITE_OK)
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

std::string describe(const japanese::RomajiConversion &conversion)
{
    return "hiragana=\"" + conversion.hiragana + "\", pending=\"" + conversion.pending +
           "\", complete=" + (conversion.complete ? "true" : "false");
}

// Every romaji expectation below is a named regression: the label is what broke, the rest of the message is what the
// converter actually produced.
void requireConversion(const std::string &romaji, const std::string &hiragana, const std::string &pending,
                       bool complete, const std::string &regression)
{
    const auto conversion = japanese::ConvertRomaji(romaji);
    if (conversion.hiragana != hiragana || conversion.pending != pending || conversion.complete != complete)
    {
        throw std::runtime_error(regression + ": ConvertRomaji(\"" + romaji + "\") returned " + describe(conversion) +
                                 ", expected hiragana=\"" + hiragana + "\", pending=\"" + pending +
                                 "\", complete=" + (complete ? "true" : "false") + ".");
    }
}

bool containsCandidate(const std::vector<WordItem> &candidates, const std::string &word)
{
    return std::any_of(candidates.begin(), candidates.end(),
                       [&word](const WordItem &item) { return item.word == word; });
}

std::string describeCandidates(const std::vector<WordItem> &candidates)
{
    std::string description;
    for (const auto &item : candidates)
    {
        description += description.empty() ? "" : ", ";
        description += item.word;
    }
    return "[" + description + "]";
}

#pragma pack(push, 1)
struct TestModelHeader
{
    char magic[8];
    std::uint32_t version;
    std::uint32_t token_count;
    std::uint32_t connection_size;
    std::uint32_t reserved;
    std::uint64_t token_offset;
    std::uint64_t connection_offset;
    std::uint64_t string_offset;
    std::uint64_t string_size;
};

struct TestModelToken
{
    std::uint32_t reading_offset;
    std::uint16_t reading_length;
    std::uint32_t surface_offset;
    std::uint16_t surface_length;
    std::uint16_t left_id;
    std::uint16_t right_id;
    std::int32_t word_cost;
};
#pragma pack(pop)

static_assert(sizeof(TestModelHeader) == 56, "The test fixture no longer matches the sentence model header layout.");
static_assert(sizeof(TestModelToken) == 20, "The test fixture no longer matches the sentence model token layout.");

struct TestModelString
{
    std::uint32_t offset = 0;
    std::uint16_t length = 0;
};

// A two-lemma sentence model, enough to drive the Japanese provider's prefix-lemma branch without shipping a
// dictionary. Readings must be stored in byte order because the loader treats the file order as its search index.
void writeJapaneseTestModel(const std::filesystem::path &path)
{
    std::string strings;
    const auto intern = [&strings](const std::string &text) {
        const TestModelString reference{static_cast<std::uint32_t>(strings.size()),
                                        static_cast<std::uint16_t>(text.size())};
        strings += text;
        return reference;
    };
    const TestModelString kanji_reading = intern("かんじ");
    const TestModelString kanji_surface = intern("漢字");
    const TestModelString shishi_reading = intern("しし");
    const TestModelString shishi_surface = intern("四肢");
    const std::vector<TestModelToken> tokens{
        {kanji_reading.offset, kanji_reading.length, kanji_surface.offset, kanji_surface.length, 0, 0, 1000},
        {shishi_reading.offset, shishi_reading.length, shishi_surface.offset, shishi_surface.length, 0, 0, 1200},
    };
    const std::int16_t connection_cost = 0;

    TestModelHeader header{};
    const char magic[8] = {'M', 'S', 'J', 'P', 'D', 'T', '1', '\0'};
    std::memcpy(header.magic, magic, sizeof(magic));
    header.version = 1;
    header.token_count = static_cast<std::uint32_t>(tokens.size());
    header.connection_size = 1;
    header.reserved = 0;
    header.token_offset = sizeof(TestModelHeader);
    header.connection_offset = header.token_offset + tokens.size() * sizeof(TestModelToken);
    header.string_offset = header.connection_offset + sizeof(connection_cost);
    header.string_size = strings.size();

    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char *>(&header), sizeof(header));
    stream.write(reinterpret_cast<const char *>(tokens.data()),
                 static_cast<std::streamsize>(tokens.size() * sizeof(TestModelToken)));
    stream.write(reinterpret_cast<const char *>(&connection_cost), sizeof(connection_cost));
    stream.write(strings.data(), static_cast<std::streamsize>(strings.size()));
    stream.close();
    if (!stream)
    {
        throw std::runtime_error("Failed to write the Japanese sentence model fixture.");
    }
}

QueryRequest japaneseRequest(const std::string &raw_input)
{
    QueryRequest request;
    request.scheme = SchemeType::JapaneseRomaji;
    request.raw_input = raw_input;
    request.raw_input_with_cases = raw_input;
    request.valid = true;
    return request;
}
} // namespace

int run_test()
{
    if (japanese::HiraganaToKatakana("かな") != "カナ" || japanese::HiraganaToRomaji("カナ") != "kana")
    {
        throw std::runtime_error("Kana conversion changed during the platform port.");
    }

    // KanaToRomajiTable is built by sorting an unordered_map with a comparator that once left equal-length spellings
    // tied, so HiraganaToRomaji returned whichever of じ's spellings that standard library's bucket order happened to
    // sort first: this build produced "kanzi" for かんじ. These two readings each have several equally long spellings
    // and pin the tiebreak that makes the choice the same everywhere.
    if (japanese::HiraganaToRomaji("かんじ") != "kanji" || japanese::HiraganaToRomaji("しゃしん") != "shashin")
    {
        throw std::runtime_error("HiraganaToRomaji picked a standard-library-dependent spelling; the equal-length "
                                 "tiebreak in KanaToRomajiTable is gone.");
    }

    // Doubling the final n is the common way to type ん. Consuming only the first n left the second one to
    // become a second ん, so "nihonn" read にほんん and no dictionary reading could ever match it.
    requireConversion("nihonn", "にほん", "", true, "a doubled final n spells two ん again");
    requireConversion("kann", "かん", "", true, "a doubled final n spells two ん again");
    requireConversion("nn", "ん", "", true, "nn on its own no longer spells a single ん");
    requireConversion("nnn", "んん", "", true, "nnn no longer spells exactly two ん");
    requireConversion("gennki", "げんき", "", true, "a word-internal nn before a consonant spells two ん again");
    if (!japanese::IsSingleKanaConversion(japanese::ConvertRomaji("nn")))
    {
        throw std::runtime_error("nn no longer converts to one kana, so typing nn lost the ん/ン candidates.");
    }
    // The same rule must leave every spelling where the second n does start a kana alone.
    requireConversion("n", "ん", "", true, "a lone trailing n no longer spells ん");
    requireConversion("kanji", "かんじ", "", true, "n before a consonant no longer spells ん");
    requireConversion("nna", "んな", "", true, "nn before a vowel stopped splitting into ん + な");
    requireConversion("annai", "あんない", "", true, "nn before a vowel stopped splitting into ん + な");
    requireConversion("kanna", "かんな", "", true, "nn before a vowel stopped splitting into ん + な");
    requireConversion("sannin", "さんにん", "", true, "nn before a vowel stopped splitting into ん + に");
    requireConversion("konnichiha", "こんにちは", "", true, "nn before a vowel stopped splitting into ん + に");
    requireConversion("nnya", "んにゃ", "", true, "nn before y stopped splitting into ん + にゃ");
    requireConversion("n'a", "んあ", "", true, "an apostrophe no longer closes ん");

    // Hepburn writes っち as "tch", which is not a doubled consonant: "matcha" used to stall as ま plus a
    // pending "tcha" that no table key could ever consume, so 抹茶 was unreachable.
    requireConversion("matcha", "まっちゃ", "", true, "Hepburn tch no longer spells a sokuon");
    requireConversion("kotchi", "こっち", "", true, "Hepburn tch no longer spells a sokuon");
    requireConversion("itchi", "いっち", "", true, "Hepburn tch no longer spells a sokuon");
    requireConversion("tchi", "っち", "", true, "a leading tch no longer spells っ + ち");
    requireConversion("match", "まっ", "ch", false, "a half-typed tch no longer leaves just ch pending");
    requireConversion("matc", "ま", "tc", false, "tc without the h is being treated as a sokuon");
    requireConversion("mat", "ま", "t", false, "a trailing t reads past the end of the input");
    // Genuine doubled consonants and the ty/cch spellings must keep their existing readings.
    requireConversion("kitte", "きって", "", true, "a genuine doubled consonant no longer spells っ");
    requireConversion("kitto", "きっと", "", true, "a genuine doubled consonant no longer spells っ");
    requireConversion("maccha", "まっちゃ", "", true, "the cch spelling of っちゃ changed");
    requireConversion("mattya", "まっちゃ", "", true, "the tty spelling of っちゃ changed");
    requireConversion("ecchi", "えっち", "", true, "the cch spelling of っち changed");

    const auto unique_suffix = std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    const std::filesystem::path data_directory =
        std::filesystem::temp_directory_path() / std::filesystem::u8path("metasequoia-engine-词库-" + unique_suffix);
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
        if (std::none_of(candidates.begin(), candidates.end(),
                         [](const WordItem &item) { return item.word == "你好"; }))
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
        if (!previous.containsUserDictionaryOperation("首次") || previous.containsUserDictionaryOperation("重开") ||
            !current.containsUserDictionaryOperation("重开") || current.containsUserDictionaryOperation("首次"))
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
    const auto successful_replay = user_dictionary::replay(metasequoia::path_to_utf8(user_database),
                                                           metasequoia::path_to_utf8(data_directory / "msime.db"),
                                                           metasequoia::path_to_utf8(english_database));
    if (!successful_replay.error.empty() || successful_replay.applied != 2)
    {
        throw std::runtime_error("Replay did not apply the Pinyin and attached English operations: error=" +
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
        const auto locked_replay = user_dictionary::replay(metasequoia::path_to_utf8(empty_user_database),
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
        unreadable_journal.execute("CREATE VIEW user_dictionary_operations AS "
                                   "SELECT 'pinyin' AS dictionary,'ni' AS key,'你' AS value,'upsert' AS operation,"
                                   "100 AS weight,'' AS display,1 AS updated_at "
                                   "UNION ALL "
                                   "SELECT 'pinyin','bad',value,'upsert',1,'',2 FROM json_each('not-json')");
    }
    const auto unreadable_replay = user_dictionary::replay(metasequoia::path_to_utf8(unreadable_user_database),
                                                           metasequoia::path_to_utf8(data_directory / "msime.db"),
                                                           metasequoia::path_to_utf8(english_database));
    if (unreadable_replay.error.empty())
    {
        throw std::runtime_error("Replay reported success after the journal cursor failed.");
    }

    // The Japanese provider used to re-derive romaji from each prefix lemma's reading and drop the lemma when the
    // derived spelling did not prefix-match the typed letters. A reading has several valid spellings and
    // HiraganaToRomaji returns only one of them, so the filter rejected correct lemmas: HiraganaToRomaji("しし") is
    // "shishi", which does not start with the typed "sis", and the しし lemma fell behind the kana fallbacks even
    // though KanaForRomajiPrefix("s") had already matched it.
    const std::filesystem::path japanese_model = data_directory / "dict_japanese_test.dat";
    writeJapaneseTestModel(japanese_model);
    {
        // The SQLite side of the provider is deliberately absent; this exercises the sentence-model branch only.
        JapaneseCandidateProvider provider(metasequoia::path_to_utf8(data_directory / "japanese_absent.db"),
                                           metasequoia::path_to_utf8(japanese_model));

        const auto shishi = provider.query(japaneseRequest("sis"));
        if (!containsCandidate(shishi, "四肢"))
        {
            throw std::runtime_error("The Japanese sentence model fixture did not load, so the prefix-lemma "
                                     "regression was not exercised. Candidates: " +
                                     describeCandidates(shishi));
        }
        if (shishi.front().word != "四肢" || shishi.front().source != CandidateSource::Database)
        {
            throw std::runtime_error("A prefix lemma whose canonical romaji differs from the typed letters lost the "
                                     "top of the candidate list for \"sis\"; the romaji re-derivation filter is "
                                     "dropping it again. Candidates: " +
                                     describeCandidates(shishi));
        }
        if (!containsCandidate(shishi, "し") || !containsCandidate(shishi, "シ"))
        {
            throw std::runtime_error("The kana fallbacks disappeared from the Japanese candidates for \"sis\": " +
                                     describeCandidates(shishi));
        }
        if (std::any_of(shishi.begin(), shishi.end(), [](const WordItem &item) { return item.word.empty(); }))
        {
            throw std::runtime_error("The Japanese provider returned a blank candidate for \"sis\".");
        }

        // The reported case: "kanj" must reach the かんじ lemmas whatever spelling HiraganaToRomaji picks for じ.
        const auto kanji = provider.query(japaneseRequest("kanj"));
        if (kanji.empty() || kanji.front().word != "漢字")
        {
            throw std::runtime_error("The かんじ prefix lemma no longer leads the candidates for \"kanj\": " +
                                     describeCandidates(kanji));
        }
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
