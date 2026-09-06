#include <metasequoia/session.h>
#include "../../core/data_path.h"
#include "../../core/input_session.h"
#include "../../core/pinyin_decoder.h"
#include "../../contracts/assets/assets.h"
#include "../../user_dictionary/user_dictionary_journal.h"
#include "test_directory_cleanup.h"
#include <sqlite3.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <future>
#include <stdexcept>

namespace
{
using namespace metasequoia;
void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
void execute(const std::filesystem::path &path, const std::string &sql)
{
    sqlite3 *db = nullptr;
    if (sqlite3_open(path_to_utf8(path).c_str(), &db) != SQLITE_OK) throw std::runtime_error("fixture open failed");
    const int result = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
    const std::string error = sqlite3_errmsg(db);
    sqlite3_close(db);
    if (result != SQLITE_OK) throw std::runtime_error(error);
}
std::string bytes(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
void make_japanese_model(const std::filesystem::path &path, const std::string &surface)
{
    // One-token MSJPDT1 fixture, with explicit little-endian fields rather than native packing.
    std::ofstream output(path, std::ios::binary);
    const auto integer = [&](std::uint64_t value, int width) {
        for (int i = 0; i < width; ++i) output.put(static_cast<char>((value >> (i * 8)) & 255));
    };
    const std::string reading = "かな";
    output.write("MSJPDT1", 8);
    integer(1, 4); integer(1, 4); integer(1, 4); integer(0, 4);
    integer(56, 8); integer(76, 8); integer(78, 8); integer(reading.size() + surface.size(), 8);
    integer(0, 4); integer(reading.size(), 2); integer(reading.size(), 4); integer(surface.size(), 2);
    integer(0, 2); integer(0, 2); integer(1, 4);
    integer(0, 2);
    output << reading << surface;
}
void make_resources(const std::filesystem::path &path, const std::string &word)
{
    std::filesystem::create_directories(path / "helpcodes");
    execute(path / assets::main_dictionary,
        "CREATE TABLE tbl_1_n(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
        "INSERT INTO tbl_1_n VALUES('ni','n','" + word + "',10000),('ni','n','拟',9000);"
        "CREATE TABLE quick_parases(key TEXT,value TEXT,weight INTEGER);"
        "INSERT INTO quick_parases VALUES('x','" + word + "短语',10);");
    require(EnglishDictionary::ensure_schema(path_to_utf8(path / assets::english_dictionary)), "English fixture failed");
    execute(path / assets::english_dictionary, "INSERT INTO english_words(word,display,weight) VALUES('hello','hello',10);");
    std::ofstream(path / assets::translations) << "hello\t" << word << "翻译\n";
    std::ofstream(path / assets::helpcode_lantian) << word << "=aa\n拟=cc\n";
    std::ofstream(path / assets::helpcode_xiaohe) << word << "=cc\n拟=aa\n";
}
void type(Session &session, const std::string &input)
{
    for (char ch : input) session.character(ch);
}
}

void test_runtime_isolation()
{
    using namespace metasequoia;
    const auto root = std::filesystem::temp_directory_path() /
        ("msime-runtime-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    test::ScopedDataDirectoryCleanup cleanup(root);
    const auto resource_a = root / std::filesystem::u8path("资源甲");
    const auto resource_b = root / std::filesystem::u8path("资源乙");
    make_resources(resource_a, "你");
    make_resources(resource_b, "妮");
    make_japanese_model(resource_a / assets::japanese_model, "甲");
    make_japanese_model(resource_b / assets::japanese_model, "乙");
    const auto reject_overlap = [&](const std::filesystem::path &resources,
                                    const std::filesystem::path &user, const std::filesystem::path &cache) {
        bool rejected = false;
        try { (void)prepare_runtime_paths(resources, user, cache, "overlap"); }
        catch (const std::invalid_argument &) { rejected = true; }
        require(rejected, "Overlapping runtime roots were accepted");
        require(!std::filesystem::exists(user / "dictionaries/overlap"),
                "Rejected runtime roots still published a generation");
    };
    reject_overlap(resource_a, root / "same-user-cache", root / "same-user-cache");
    reject_overlap(resource_a, root / "cache-parent/user", root / "cache-parent");
    reject_overlap(resource_a, root / "user-parent", root / "user-parent/cache");
    reject_overlap(resource_a, resource_a / "user", root / "separate-cache");
    reject_overlap(resource_a, root / "separate-user", resource_a / "cache");
    reject_overlap(resource_a, root, root / "separate-cache");
    require(!std::filesystem::exists(root / "same-user-cache") &&
            !std::filesystem::exists(root / "cache-parent") &&
            !std::filesystem::exists(resource_a / "user") &&
            !std::filesystem::exists(resource_a / "cache"),
            "Path validation created directories before rejecting overlap");
#ifndef _WIN32
    const auto resource_alias = root / "resource-alias";
    std::filesystem::create_directory_symlink(resource_a, resource_alias);
    reject_overlap(resource_a, root / "alias-user", resource_alias / "cache");
#endif
    const auto original = bytes(resource_a / assets::main_dictionary);
    const auto paths_a = prepare_runtime_paths(resource_a, root / "user-a", root / "cache-a", "v1");
    const auto paths_b = prepare_runtime_paths(resource_b, root / "user-b", root / "cache-b", "v1");
    require(paths_a.resources != paths_a.dictionaries && paths_a.cache != paths_a.dictionaries,
            "Mutable dictionaries must live separately from resources and cache");
    {
        SessionOptions options_a; options_a.paths = paths_a; options_a.learning = false;
        SessionOptions options_b; options_b.paths = paths_b; options_b.learning = false;
        Session a(options_a), b(options_b);
        type(a, "ni"); type(b, "ni");
        require(a.snapshot().candidates.front().word == "你", "Session A used another resource directory");
        require(b.snapshot().candidates.front().word == "妮", "Session B used another resource directory");
        const auto query_a = a.online_query();
        require(query_a.has_value(), "No online request for complete pinyin");
        require(!b.apply_online_candidate(*query_a, "跨会话建议", CandidateSource::CloudSuggestion),
                "Another session's online response was accepted");
        require(a.apply_online_candidate(*query_a, "本会话建议", CandidateSource::CloudSuggestion),
                "Own online response was rejected");
        a.command(Command::Cancel); b.command(Command::Cancel);
        require(a.set_helpcode_schema("lantian") && b.set_helpcode_schema("xiaohe"), "Cannot select helpcodes");
        type(a, "niC"); type(b, "niC");
        require(a.snapshot().candidates.front().word == "拟", "Session A helpcode changed with B");
        require(b.snapshot().candidates.front().word == "妮", "Session B helpcode was not isolated");
        require(a.punctuation('"').commit == "拟“", "Punctuation did not flush composition");
        b.command(Command::Cancel);
        require(b.punctuation('"').commit == "“", "Quotation state leaked across sessions");
        const auto run_session = [](Session &session, const std::string &expected) {
            for (int i = 0; i < 20; ++i)
            {
                session.command(Command::Cancel);
                type(session, "ni");
                const auto snapshot = session.snapshot();
                require(!snapshot.candidates.empty() && snapshot.candidates.front().word == expected,
                        "Concurrent session read another user's dictionary");
            }
        };
        auto first_session = std::async(std::launch::async, run_session, std::ref(a), "你");
        auto second_session = std::async(std::launch::async, run_session, std::ref(b), "妮");
        first_session.get(); second_session.get();
        a.command(Command::Cancel);
        a.character('K', true); a.character('x');
        require(a.snapshot().candidates.front().word == "你短语", "Local mode ignored runtime paths");
    }
    {
        SessionOptions options; options.paths = paths_a; options.learning = false;
        Session session(options);
        type(session, "ni'ni");
        const auto view = session.snapshot();
        const auto selected = std::find_if(view.candidates.begin(), view.candidates.end(),
            [](const auto &item) { return item.word == "拟"; });
        require(selected != view.candidates.end(), "No alternate first-segment candidate");
        require(session.finish(static_cast<std::size_t>(selected - view.candidates.begin())).commit == "拟你",
                "Finishing ignored the highlight or dropped the remaining segment");
        require(session.snapshot().preedit.empty(), "Finishing left an active composition");
        require(!session.finish().handled, "Finishing an empty session consumed input");
        type(session, "ni");
        require(session.finish(9999).commit == "ni", "Invalid highlight did not preserve raw input");

        session.set_helpcode_enabled(false);
        type(session, "ni");
        require(!session.character('C').handled, "Disabled helpcode still consumed uppercase input");
        session.set_helpcode_enabled(true);
        require(session.character('C').handled && session.snapshot().candidates.front().word == "拟",
                "Re-enabling helpcode did not update this composition");
        session.command(Command::Cancel);
        session.set_dedicated_english(true);
        require(session.snapshot().dedicated_english, "Empty dedicated English mode was lost in the snapshot");
        type(session, "hel");
        require(session.snapshot().dedicated_english && session.snapshot().preedit == "hel",
                "Dedicated English state was not represented in the snapshot");
        session.set_dedicated_english(false);
        require(!session.snapshot().dedicated_english && session.snapshot().preedit.empty(),
                "Leaving dedicated English left stale snapshot state");
    }
    {
        SessionOptions options_a; options_a.paths = paths_a; options_a.scheme = SchemeType::JapaneseRomaji;
        SessionOptions options_b; options_b.paths = paths_b; options_b.scheme = SchemeType::JapaneseRomaji;
        Session a(options_a), b(options_b);
        type(a, "kana"); type(b, "kana");
        const auto has = [](const Session &session, const std::string &word) {
            const auto snapshot = session.snapshot();
            return std::any_of(snapshot.candidates.begin(), snapshot.candidates.end(),
                [&](const auto &candidate) { return candidate.word == word; });
        };
        require(has(a, "甲") && !has(a, "乙"), "Japanese session A used another model");
        require(has(b, "乙") && !has(b, "甲"), "Japanese session B reused the first model");
    }
    // Explicit pinning must use the selected canonical key, preserve input, and survive replay.
    const auto pin_resources = root / "pin-resources";
    make_resources(pin_resources, "你");
    execute(pin_resources / assets::main_dictionary,
        "CREATE TABLE tbl_2_n(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
        "INSERT INTO tbl_2_n VALUES('ni''hao','nh','你好',10000),('ni''hao','nh','拟好',9000);"
        "CREATE TABLE wubi86(key TEXT,value TEXT,weight INTEGER);"
        "INSERT INTO wubi86 VALUES('wq','你好',10000),('wq','拟好',9000);");
    execute(pin_resources / assets::english_dictionary,
        "INSERT INTO english_words(word,display,weight) VALUES('help','help',5);");
    const auto pin_main_bytes = bytes(pin_resources / assets::main_dictionary);
    const auto pin_english_bytes = bytes(pin_resources / assets::english_dictionary);
    const auto pin_word = [&](Session &session, const std::string &word) {
        const auto before = session.snapshot();
        const auto selected = std::find_if(before.candidates.begin(), before.candidates.end(),
            [&](const auto &item) { return item.word == word; });
        require(selected != before.candidates.end(), "No candidate for explicit pinning");
        const auto result = session.pin(static_cast<std::size_t>(selected - before.candidates.begin()));
        require(result.handled && !result.commit && !result.diagnostic, "Explicit pin failed or committed text");
        require(session.snapshot().preedit == before.preedit, "Pin changed the composition");
        const auto after = session.snapshot();
        const auto first = std::find_if(after.candidates.begin(), after.candidates.end(),
            [&](const auto &item) { return item.source == selected->source; });
        require(first != after.candidates.end() && first->word == word,
                "Pin did not refresh dictionary candidate order");
        require(!session.pin(before.candidates.size() + 100).handled, "Invalid pin index was accepted");
    };
    for (const auto scheme : {SchemeType::Quanpin, SchemeType::Shuangpin, SchemeType::Wubi})
    {
        const auto suffix = std::to_string(static_cast<int>(scheme));
        const auto user = root / ("pin-user-" + suffix);
        const auto cache = root / ("pin-cache-" + suffix);
        SessionOptions options;
        options.paths = prepare_runtime_paths(pin_resources, user, cache, "v1");
        options.scheme = scheme;
        options.learning = false;
        const std::string input = scheme == SchemeType::Quanpin ? "nihao" :
            (scheme == SchemeType::Shuangpin ? "nihc" : "wq");
        {
            Session session(options);
            require(!session.pin(0).handled, "Empty session accepted pin");
            type(session, input);
            pin_word(session, "拟好");
            require(session.finish().commit == "拟好", "Pin interfered with later selection");
        }
        options.paths = prepare_runtime_paths(pin_resources, user, cache, "v2");
        Session replayed(options);
        type(replayed, input);
        require(replayed.snapshot().candidates.front().word == "拟好", "Pin did not survive generation replay");
    }
    for (const bool temporary : {false, true})
    {
        SessionOptions options;
        const std::string suffix = temporary ? "temporary" : "dedicated";
        const auto user = root / ("pin-english-user-" + suffix);
        const auto cache = root / ("pin-english-cache-" + suffix);
        options.paths = prepare_runtime_paths(pin_resources, user, cache, "v1");
        options.learning = false;
        {
            Session session(options);
            if (temporary) session.character('Y', true);
            else session.set_dedicated_english(true);
            type(session, "he");
            pin_word(session, "help");
        }
        options.paths = prepare_runtime_paths(pin_resources, user, cache, "v2");
        Session replayed(options);
        replayed.set_dedicated_english(true);
        type(replayed, "he");
        require(replayed.snapshot().candidates.front().word == "help", "English pin did not survive replay");
        replayed.command(Command::Cancel);
        type(replayed, "zzzz");
        require(!replayed.pin(0).handled, "Generated candidate accepted dictionary pinning");
    }
    {
        SessionOptions options;
        options.paths = prepare_runtime_paths(pin_resources, root / "pin-mixed-user", root / "pin-mixed-cache", "v1");
        options.learning = false;
        options.english.mixed_candidates = true;
        Session mixed(options);
        type(mixed, "he");
        pin_word(mixed, "help");
    }
    {
        SessionOptions options;
        options.paths = prepare_runtime_paths(pin_resources, root / "pin-failed-user", root / "pin-failed-cache", "v1");
        options.learning = false;
        // A directory in place of the journal is a portable, deterministic write failure.
        std::filesystem::remove(options.paths.user(assets::user_journal));
        std::filesystem::create_directory(options.paths.user(assets::user_journal));
        Session session(options);
        type(session, "nihao");
        const auto before = session.snapshot();
        const auto selected = std::find_if(before.candidates.begin(), before.candidates.end(),
            [](const auto &item) { return item.word == "拟好"; });
        require(selected != before.candidates.end(), "No candidate for failed pin test");
        const auto result = session.pin(static_cast<std::size_t>(selected - before.candidates.begin()));
        require(result.handled && !result.commit && result.diagnostic.has_value(),
                "Failed pin was reported as success or committed text");
        require(session.snapshot().preedit == before.preedit &&
                session.snapshot().candidates.front().word == before.candidates.front().word,
                "Failed pin changed input or candidate order");
    }
    require(bytes(pin_resources / assets::main_dictionary) == pin_main_bytes &&
            bytes(pin_resources / assets::english_dictionary) == pin_english_bytes,
            "Explicit pinning modified immutable resources");
    // Query/learning changes affect only the user working copy and journal; replay retains them.
    {
        QuanpinDictionary dictionary({}, paths_a);
        require(dictionary.create_word_from_canonical_pinyin("ni", "伱") == 0, "User insertion failed");
    }
    require(bytes(resource_a / assets::main_dictionary) == original, "Learning modified immutable source");
    std::ofstream(paths_a.cache / "disposable") << "cache fixture";
    std::filesystem::remove_all(paths_a.cache);
    require(std::filesystem::is_regular_file(paths_a.user(assets::user_journal)),
            "Clearing cache removed the user journal");
    {
        QuanpinDictionary dictionary({}, paths_a);
        require(dictionary.find_candidate("ni", "伱").has_value(), "Clearing cache removed learned vocabulary");
    }
    const auto paths_v2 = prepare_runtime_paths(resource_a, paths_a.user_data, paths_a.cache, "v2");
    {
        QuanpinDictionary dictionary({}, paths_v2);
        require(dictionary.find_candidate("ni", "伱").has_value(), "Upgrade lost user phrase replay");
        EnglishDictionary english(path_to_utf8(paths_v2.dictionary(assets::english_dictionary)), false,
                                  path_to_utf8(paths_v2.resource(assets::translations)));
        require(english.query_chinese_gloss("hello") == "你翻译", "Translation sidecar did not use resource path");
    }
    {
        QuanpinDictionary dictionary({}, paths_v2);
        require(dictionary.create_word_from_canonical_pinyin("ni", "倪") == 0, "Second-generation insertion failed");
    }
    const auto restored_v1 = prepare_runtime_paths(resource_a, paths_a.user_data, paths_a.cache, "v1");
    {
        QuanpinDictionary dictionary({}, restored_v1);
        require(dictionary.find_candidate("ni", "倪").has_value(), "Returning to old generation lost newer learning");
    }
    const auto before_failure = bytes(paths_v2.dictionary(assets::main_dictionary));
    require(user_dictionary::record_upsert(path_to_utf8(paths_a.user(assets::user_journal)),
            user_dictionary::DictionaryKind::Pinyin, "@", "无效词", 10), "Cannot write failure fixture");
    bool rejected = false;
    try { (void)prepare_runtime_paths(resource_a, paths_a.user_data, paths_a.cache, "v3"); }
    catch (const std::runtime_error &) { rejected = true; }
    require(rejected, "Invalid replay did not reject generation");
    require(bytes(paths_v2.dictionary(assets::main_dictionary)) == before_failure, "Failed replay replaced working data");
    require(!std::filesystem::exists(paths_a.user_data / "dictionaries/v3"), "Failed generation was published");
    require(!std::filesystem::exists(paths_a.user_data / "dictionaries/v3.incoming"), "Failed staging was retained");

    // Use the real pinned decoder model. Interleave independent users, invalid model loading,
    // destruction, and concurrent calls; candidate contents must equal isolated decoding.
    const auto model = resource_a / assets::pinyin_model;
    std::filesystem::copy_file(path_from_utf8(METASEQUOIA_TEST_PINYIN_MODEL), model);
    {
        PinyinDecoder decoder_a(model, resource_a / "decoder-a.dat"), decoder_b(model, resource_b / "decoder-b.dat");
        const auto expected_a = decoder_a.sentence("nihao");
        const auto expected_b = decoder_b.sentence("zhongguo");
        require(!expected_a.empty() && !expected_b.empty(), "Real decoder model did not load");
        {
            PinyinDecoder missing(root / "missing.dat", root / "missing-user.dat");
            require(missing.sentence("nihao").empty(), "Missing model used another session's decoder");
        }
        require(decoder_a.sentence("nihao") == expected_a, "Failed model load damaged existing session");
        const auto run = [](const PinyinDecoder &decoder, const std::string &input, const std::string &expected) {
            for (int i = 0; i < 12; ++i)
                require(decoder.sentence(input) == expected, "Concurrent decoder result leaked across sessions");
        };
        auto first = std::async(std::launch::async, run, std::cref(decoder_a), "nihao", expected_a);
        auto second = std::async(std::launch::async, run, std::cref(decoder_b), "zhongguo", expected_b);
        first.get(); second.get();
    }
}
