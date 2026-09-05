//
// 测试拼音输入法的核心逻辑，包括双拼和全拼方案，以及动态切换输入方案的功能。
//
#include <Windows.h>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <fmt/core.h>
#include "fmt/base.h"
#include "core/ime_session.h"
#include "quanpin/quanpin_dictionary.h"
#include "sqlite3.h"
#include "shuangpin/shuangpin_dictionary.h"
#include "shuangpin/shuangpin_query.h"
#include "shuangpin/shuangpin_utils.h"
#include "core/data_path.h"
#include "user_dictionary/user_dictionary_journal.h"

using namespace std;

namespace
{
namespace fs = std::filesystem;

class ScopedLocalAppDataOverride
{
  public:
    explicit ScopedLocalAppDataOverride(const std::string &suffix)
    {
        const fs::path source_dir = metasequoia::data_directory();
        if (source_dir.empty())
        {
            throw std::runtime_error("A data directory should be available for regression tests.");
        }
        const wchar_t *current = _wgetenv(kDataDirectoryVariable);
        original_ = current == nullptr ? L"" : current;

        root_ = fs::temp_directory_path() / "msime-regression" / suffix;
        app_dir_ = root_ / "metasequoiaime";
        fs::remove_all(root_);
        fs::create_directories(app_dir_);

        for (const auto &file_name : {"msime.db", "dict_pinyin.dat", "user_dict.dat"})
        {
            const fs::path source = source_dir / file_name;
            const fs::path target = app_dir_ / file_name;
            if (!fs::exists(source))
            {
                throw std::runtime_error(fmt::format("Expected test dependency '{}' to exist.", source.string()));
            }
            fs::copy_file(source, target, fs::copy_options::overwrite_existing);
        }

        if (_wputenv_s(kDataDirectoryVariable, app_dir_.c_str()) != 0)
        {
            throw std::runtime_error("Failed to override the data directory for regression test.");
        }
    }

    ~ScopedLocalAppDataOverride()
    {
        (void)_wputenv_s(kDataDirectoryVariable, original_.c_str());
        std::error_code ec;
        fs::remove_all(root_, ec);
    }

    std::string local_appdata() const
    {
        return root_.string();
    }

  private:
    static constexpr const wchar_t *kDataDirectoryVariable = L"METASEQUOIA_IME_DATA_DIR";

    std::wstring original_;
    fs::path root_;
    fs::path app_dir_;
};

void expect(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

void expect_session_state(const ImeSession &session, const std::string &expected_preedit)
{
    expect(session.get_preedit() == expected_preedit,
           fmt::format("Expected preedit '{}', got '{}'", expected_preedit, session.get_preedit()));
    expect(session.get_request().raw_input == expected_preedit,
           fmt::format("Expected raw_input '{}', got '{}'", expected_preedit, session.get_request().raw_input));
}

} // namespace

std::string hex_dump(const std::string &text)
{
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < text.size(); ++i)
    {
        if (i > 0)
        {
            oss << ' ';
        }
        oss << std::setw(2) << static_cast<int>(static_cast<unsigned char>(text[i]));
    }
    return oss.str();
}

void print_candidates(const std::vector<WordItem> &result)
{
    for (size_t index = 0; index < result.size(); ++index)
    {
        const auto &item = result[index];
        const auto &code = item.pinyin;
        const auto &word = item.word;
        const auto weight = item.weight;
        try
        {
            fmt::println("Candidate #{}: {} [{}] ({})", index, word, code, weight);
        }
        catch (const std::exception &ex)
        {
            throw std::runtime_error(fmt::format(
                "Failed to print candidate #{}; word bytes=[{}], code bytes=[{}], error={}",
                index, hex_dump(word), hex_dump(code), ex.what()));
        }
    }
}

const WordItem *find_candidate(const std::vector<WordItem> &result, const std::string &word)
{
    const auto found =
        std::find_if(result.begin(), result.end(), [&](const WordItem &item) { return item.word == word; });
    return found == result.end() ? nullptr : &(*found);
}

void run_quanpin_query_case(QuanpinDictionary &dictionary, const std::string &query)
{
    const auto start = std::chrono::high_resolution_clock::now();
    const auto result = dictionary.query(query);
    const auto end = std::chrono::high_resolution_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    fmt::println("Query: {}", query);
    fmt::println("Time: {} us", duration.count());
    print_candidates(result);
}

void feed_sequence(ImeSession &session, const vector<UINT> &sequence, const vector<WCHAR> &wch_sequence = {})
{
    for (int i = 0; i < sequence.size(); ++i)
    {
        std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
        session.handle_key(sequence[i], 0, i < wch_sequence.size() ? wch_sequence[i] : 0);
        std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        fmt::println("Preedit: {}", session.get_preedit());
        fmt::println("Time: {} us", duration.count());
    }
}

void test_shuangpin_session()
{
    ImeSession session(SchemeType::Shuangpin);
    const vector<UINT> sequence{'C', 'L', 'S'};      // 按键的 vk 码
    const vector<WCHAR> wch_sequence{'c', 'l', 's'}; // 实际的字符，区分大小写输入

    fmt::println("==== Shuangpin ====");
    feed_sequence(session, sequence, wch_sequence);
    print_candidates(session.get_candidates());
}

void test_shuangpin_session02()
{
    // ImeSession session(SchemeType::Quanpin);
    ImeSession session(SchemeType::Shuangpin);
    // const vector<UINT> sequence{'C', 'E', 'L', 'I', 'S', 'H', 'I'};
    const vector<UINT> sequence{'C', 'E', 'L', 'I', 'U', 'I'};
    const vector<WCHAR> wch_sequence{'c', 'e', 'l', 'i', 'u', 'i'};

    fmt::println("==== Shuangpin ====");
    feed_sequence(session, sequence, wch_sequence);
    print_candidates(session.get_candidates());
}

void test_quanpin_session()
{
    ImeSession session(SchemeType::Quanpin);
    const vector<UINT> sequence{'C', 'E', 'S', 'H', 'I'};
    const vector<WCHAR> wch_sequence{'c', 'e', 's', 'h', 'i'};

    fmt::println("==== Quanpin ====");
    feed_sequence(session, sequence, wch_sequence);
    print_candidates(session.get_candidates());
}

void test_dynamic_switch()
{
    ImeSession session(SchemeType::Shuangpin);

    fmt::println("==== Switch Scheme ====");
    feed_sequence(session, {'N', 'I'}, {'n', 'i'});
    fmt::println("Before switch preedit: {}", session.get_preedit());

    session.switch_scheme(SchemeType::Quanpin);
    fmt::println("After switch preedit: {}", session.get_preedit());

    feed_sequence(session, {'N', 'I', 'H', 'A', 'O'}, {'n', 'i', 'h', 'a', 'o'});
    print_candidates(session.get_candidates());
}

void test_quanpin_session_backspace()
{
    ImeSession session(SchemeType::Quanpin);

    fmt::println("==== Quanpin Backspace ====");
    feed_sequence(session, {'C', 'E', 'S', 'H', 'I'}, {'c', 'e', 's', 'h', 'i'});
    expect_session_state(session, "ceshi");
    expect(!session.get_candidates().empty(), "Quanpin session should have candidates before backspace.");

    session.handle_key(VK_BACK);
    fmt::println("Preedit after backspace: {}", session.get_preedit());
    expect_session_state(session, "cesh");
    expect(session.get_request().valid, "Quanpin session request should stay valid after backspace.");
}

void test_shuangpin_session_backspace()
{
    ImeSession session(SchemeType::Shuangpin);

    fmt::println("==== Shuangpin Backspace ====");
    feed_sequence(session, {'C', 'E', 'L', 'I', 'U', 'I'}, {'c', 'e', 'l', 'i', 'u', 'i'});
    expect_session_state(session, "celiui");
    expect(!session.get_candidates().empty(), "Shuangpin session should have candidates before backspace.");

    session.handle_key(VK_BACK);
    fmt::println("Preedit after backspace: {}", session.get_preedit());
    expect_session_state(session, "celiu");
    expect(session.get_request().valid, "Shuangpin session request should stay valid after backspace.");
}

void test_shuangpin_manual_apostrophe()
{
    ImeSession session(SchemeType::Shuangpin);

    fmt::println("==== Shuangpin Manual Apostrophe ====");
    feed_sequence(session, {'J', 'W', VK_OEM_7, 'D'}, {'j', 'w', '\'', 'd'});
    expect_session_state(session, "jw'd");
    expect(session.get_request().raw_segmentation.find('\'') != std::string::npos,
           fmt::format("Expected raw segmentation to preserve apostrophes, got '{}'",
                       session.get_request().raw_segmentation));
    expect(session.get_request().normalized_segmentation.find('\'') != std::string::npos,
           fmt::format("Expected normalized segmentation to preserve apostrophes, got '{}'",
                       session.get_request().normalized_segmentation));
    expect(session.get_request().normalized_input.find('\'') == std::string::npos,
           fmt::format("Expected normalized input to strip apostrophes, got '{}'",
                       session.get_request().normalized_input));

    session.handle_key(VK_BACK);
    expect_session_state(session, "jw'");
    session.handle_key(VK_BACK);
    expect_session_state(session, "jw");
}

void test_shuangpin_query_manual_apostrophe()
{
    fmt::println("==== Shuangpin Query Manual Apostrophe ====");
    expect(shuangpin::segment_input("ce'ce") == "ce'ce",
           "Expected manual apostrophe to be preserved in raw segmentation for complete chunks.");
    expect(shuangpin::normalize_input_with_delimiters("ce'ce") == "ce'ce",
           "Expected manual apostrophe to be preserved in normalized segmentation for complete chunks.");
    expect(shuangpin::normalize_input("ce'ce") == "cece",
           "Expected normalized input to strip apostrophes for complete chunks.");
    expect(shuangpin::is_complete_input("ce'ce"), "Expected ce'ce to be recognized as complete shuangpin input.");
    expect(!shuangpin::is_complete_input("jw'"), "Expected trailing manual apostrophe to stay incomplete.");
}

void test_quanpin_dictionary_backspace()
{
    QuanpinDictionary dictionary;

    fmt::println("==== Quanpin Dictionary Backspace ====");
    dictionary.handleVkCode('C', 0, 'c');
    dictionary.handleVkCode('E', 0, 'e');
    dictionary.handleVkCode('S', 0, 's');
    expect(dictionary.get_pinyin_sequence() == "ces",
           fmt::format("Expected quanpin dictionary sequence 'ces', got '{}'", dictionary.get_pinyin_sequence()));

    dictionary.handleVkCode(VK_BACK, 0);
    expect(dictionary.get_pinyin_sequence() == "ce",
           fmt::format("Expected quanpin dictionary sequence 'ce' after backspace, got '{}'",
                       dictionary.get_pinyin_sequence()));
    expect(!dictionary.get_current_candidate_list().empty(),
           "Quanpin dictionary should still have candidates after backspace.");
}

void test_shuangpin_dictionary_backspace()
{
    ShuangpinDictionary dictionary;

    fmt::println("==== Shuangpin Dictionary Backspace ====");
    dictionary.handleVkCode('C', 0, 'c');
    dictionary.handleVkCode('E', 0, 'e');
    dictionary.handleVkCode('L', 0, 'l');
    expect(dictionary.get_pinyin_sequence() == "cel",
           fmt::format("Expected shuangpin dictionary sequence 'cel', got '{}'", dictionary.get_pinyin_sequence()));

    dictionary.handleVkCode(VK_BACK, 0);
    expect(dictionary.get_pinyin_sequence() == "ce",
           fmt::format("Expected shuangpin dictionary sequence 'ce' after backspace, got '{}'",
                       dictionary.get_pinyin_sequence()));
    expect(!dictionary.get_current_candidate_list().empty(),
           "Shuangpin dictionary should still have candidates after backspace.");
}

void test_shuangpin_dictionary_create_pin_delete()
{
    ScopedLocalAppDataOverride local_appdata("shuangpin-write-regression");
    expect(ShuangpinUtil::get_local_appdata_path() == local_appdata.local_appdata(),
           fmt::format("Expected shuangpin appdata path '{}', got '{}'.", local_appdata.local_appdata(),
                       ShuangpinUtil::get_local_appdata_path()));
    const std::string expected_user_db = local_appdata.local_appdata() + "\\metasequoiaime\\msime_user.db";
    expect(user_dictionary::default_user_db_path() == expected_user_db,
           fmt::format("Expected user db path '{}', got '{}'.", expected_user_db,
                       user_dictionary::default_user_db_path()));
    const char *process_appdata = std::getenv("LOCALAPPDATA");
    expect(process_appdata != nullptr && std::string(process_appdata) != local_appdata.local_appdata(),
           "Overriding the data root must not touch the process environment.");

    sqlite3 *probe_db = nullptr;
    const std::string probe_db_path =
        local_appdata.local_appdata() + "\\metasequoiaime\\msime.db";
    expect(sqlite3_open(probe_db_path.c_str(), &probe_db) == SQLITE_OK,
           fmt::format("Failed to open probe db '{}'.", probe_db_path));
    char *probe_error = nullptr;
    expect(sqlite3_exec(probe_db,
                        "insert into tbl_2_c (key, jp, value, weight) values ('ce''li', 'cl', '测棂', 10000);",
                        nullptr,
                        nullptr,
                        &probe_error) == SQLITE_OK,
           fmt::format("Expected probe insert to succeed, got '{}'.", probe_error == nullptr ? "" : probe_error));
    sqlite3_free(probe_error);
    probe_error = nullptr;
    expect(sqlite3_exec(probe_db,
                        "delete from tbl_2_c where key = 'ce''li' and value = '测棂';",
                        nullptr,
                        nullptr,
                        &probe_error) == SQLITE_OK,
           fmt::format("Expected probe delete to succeed, got '{}'.", probe_error == nullptr ? "" : probe_error));
    sqlite3_free(probe_error);
    sqlite3_close(probe_db);

    ShuangpinDictionary dictionary;

    const std::string raw_shuangpin = "celi";
    const std::string segmented_shuangpin = shuangpin::segment_input(raw_shuangpin);
    const std::string test_word = "测棂";

    fmt::println("==== Shuangpin Dictionary Create/Pin/Delete ====");

    // Clean up any residue from prior runs so the assertions stay deterministic.
    dictionary.delete_by_pinyin_and_word(raw_shuangpin, test_word);

    const auto before_create = dictionary.generateSeries(raw_shuangpin, segmented_shuangpin);
    expect(find_candidate(before_create, test_word) == nullptr,
           fmt::format("Expected '{}' to be absent before create.", test_word));

    expect(dictionary.create_word(raw_shuangpin, test_word) == ShuangpinDictionary::OK,
           "Shuangpin create_word should succeed.");

    const auto after_create = dictionary.generateSeries(raw_shuangpin, segmented_shuangpin);
    const WordItem *created = find_candidate(after_create, test_word);
    expect(created != nullptr, fmt::format("Expected '{}' to appear after create.", test_word));
    const int created_weight = created->weight;
    const std::string canonical_pinyin = created->canonical_pinyin;
    expect(!canonical_pinyin.empty(), "Created candidate should expose its canonical pinyin.");

    expect(dictionary.update_weight_by_pinyin_and_word(raw_shuangpin, test_word) == ShuangpinDictionary::OK,
           "Shuangpin update_weight_by_pinyin_and_word should succeed.");

    const auto after_pin = dictionary.generateSeries(raw_shuangpin, segmented_shuangpin);
    const WordItem *pinned = find_candidate(after_pin, test_word);
    expect(pinned != nullptr, fmt::format("Expected '{}' to remain after pin.", test_word));
    expect(pinned->weight > created_weight,
           fmt::format("Expected pinned weight to increase from {}, got {}.", created_weight, pinned->weight));

    expect(dictionary.delete_by_pinyin_and_word(canonical_pinyin, test_word) == ShuangpinDictionary::OK,
           "Shuangpin delete_by_pinyin_and_word should accept a canonical pinyin key.");

    const auto after_delete = dictionary.generateSeries(raw_shuangpin, segmented_shuangpin);
    expect(find_candidate(after_delete, test_word) == nullptr,
           fmt::format("Expected '{}' to be absent after delete.", test_word));
}

void test_shuangpin_dictionary_create_pin_delete_three_syllables()
{
    ScopedLocalAppDataOverride local_appdata("shuangpin-write-three-syllables");
    ShuangpinDictionary dictionary;

    const std::string raw_shuangpin = "qbtmuo";
    const std::string segmented_shuangpin = shuangpin::segment_input(raw_shuangpin);
    const std::string test_word = "秦天朔";

    fmt::println("==== Shuangpin Dictionary Create/Pin/Delete Three Syllables ====");

    dictionary.delete_by_pinyin_and_word(raw_shuangpin, test_word);

    const auto before_create = dictionary.generateSeries(raw_shuangpin, segmented_shuangpin);
    expect(find_candidate(before_create, test_word) == nullptr,
           fmt::format("Expected '{}' to be absent before create.", test_word));

    expect(dictionary.create_word(raw_shuangpin, test_word) == ShuangpinDictionary::OK,
           "Three-syllable shuangpin create_word should succeed.");

    const auto after_create = dictionary.generateSeries(raw_shuangpin, segmented_shuangpin);
    const WordItem *created = find_candidate(after_create, test_word);
    expect(created != nullptr, fmt::format("Expected '{}' to appear after create.", test_word));
    const int created_weight = created->weight;

    expect(dictionary.update_weight_by_pinyin_and_word(raw_shuangpin, test_word) == ShuangpinDictionary::OK,
           "Three-syllable shuangpin update_weight_by_pinyin_and_word should succeed.");

    const auto after_pin = dictionary.generateSeries(raw_shuangpin, segmented_shuangpin);
    const WordItem *pinned = find_candidate(after_pin, test_word);
    expect(pinned != nullptr, fmt::format("Expected '{}' to remain after pin.", test_word));
    expect(pinned->weight > created_weight,
           fmt::format("Expected pinned weight to increase from {}, got {}.", created_weight, pinned->weight));

    expect(dictionary.delete_by_pinyin_and_word(raw_shuangpin, test_word) == ShuangpinDictionary::OK,
           "Three-syllable shuangpin delete_by_pinyin_and_word should succeed.");

    const auto after_delete = dictionary.generateSeries(raw_shuangpin, segmented_shuangpin);
    expect(find_candidate(after_delete, test_word) == nullptr,
           fmt::format("Expected '{}' to be absent after delete.", test_word));
}

void test_quanpin_single_letter_jianpin_ranking()
{
    ScopedLocalAppDataOverride local_appdata("single-letter-jianpin-ranking");
    QuanpinDictionary dictionary;

    fmt::println("==== Quanpin Single Letter Jianpin Ranking ====");
    // A single-letter context mixes entry keys: 一 comes from yi, 有 from you.
    const auto before = dictionary.query("y");
    const WordItem *selected = find_candidate(before, "有");
    const WordItem *rival = find_candidate(before, "一");
    expect(selected != nullptr, "Expected '有' among the candidates for 'y'.");
    if (rival == nullptr || rival->weight <= selected->weight)
    {
        fmt::println("Skipped: '一' does not outweigh '有' in this dictionary.");
        return;
    }

    // The server keys a selection by its canonical pinyin, so entry_key is 'you'
    // while context_key stays 'y'.
    const std::string entry_key =
        selected->canonical_pinyin.empty() ? selected->pinyin : selected->canonical_pinyin;
    bool ranking_changed = false;
    expect(user_dictionary::adjust_candidate_ranking(
               local_appdata.local_appdata() + "\\metasequoiaime\\msime.db",
               user_dictionary::default_user_db_path(), "y", before, entry_key, "有", "promote", 1, 1, true,
               &ranking_changed),
           "Expected the single-letter ranking adjustment to succeed.");
    expect(ranking_changed, "Expected the single-letter ranking adjustment to write a weight.");

    QuanpinDictionary reloaded;
    const auto after = reloaded.query("y");
    const WordItem *promoted = find_candidate(after, "有");
    const WordItem *demoted = find_candidate(after, "一");
    expect(promoted != nullptr, "Expected '有' to survive the ranking adjustment.");
    expect(demoted == nullptr || promoted->weight > demoted->weight,
           fmt::format("Expected '有' to outweigh '一' under context 'y', got {} and {}.", promoted->weight,
                       demoted == nullptr ? 0 : demoted->weight));
}

void test_quanpin_query_timings()
{
    QuanpinDictionary dictionary;

    fmt::println("==== Quanpin Query Timings ====");
    run_quanpin_query_case(dictionary, "nih");
    run_quanpin_query_case(dictionary, "niha");
    run_quanpin_query_case(dictionary, "nihao");
    run_quanpin_query_case(dictionary, "ni");
    run_quanpin_query_case(dictionary, "n");
    run_quanpin_query_case(dictionary, "shen");
    run_quanpin_query_case(dictionary, "shenme");
    run_quanpin_query_case(dictionary, "shenmeshi");
    run_quanpin_query_case(dictionary, "shenmeshi");
    run_quanpin_query_case(dictionary, "shenmeshui");
    run_quanpin_query_case(dictionary, "shenmesh");
    run_quanpin_query_case(dictionary, "shenmes");
    run_quanpin_query_case(dictionary, "n");
    run_quanpin_query_case(dictionary, "ni");
    run_quanpin_query_case(dictionary, "nis");
    run_quanpin_query_case(dictionary, "nish");
    run_quanpin_query_case(dictionary, "nishu");
    run_quanpin_query_case(dictionary, "nishuo");
    run_quanpin_query_case(dictionary, "nishuon");
    run_quanpin_query_case(dictionary, "nishuone");
    run_quanpin_query_case(dictionary, "keneng");
}

int main(int argc, char *argv[])
{
    try
    {
        test_shuangpin_session();
        test_shuangpin_session02();
        test_quanpin_session();
        test_dynamic_switch();
        test_quanpin_session_backspace();
        test_shuangpin_session_backspace();
        test_shuangpin_manual_apostrophe();
        test_quanpin_dictionary_backspace();
        test_shuangpin_dictionary_backspace();
        test_shuangpin_dictionary_create_pin_delete();
        test_shuangpin_dictionary_create_pin_delete_three_syllables();
        test_shuangpin_query_manual_apostrophe();
        test_quanpin_single_letter_jianpin_ranking();
        test_quanpin_query_timings();
        fmt::println("All tests passed.");
        return 0;
    }
    catch (const std::exception &ex)
    {
        fmt::println(stderr, "Test failure: {}", ex.what());
        return 1;
    }
}
