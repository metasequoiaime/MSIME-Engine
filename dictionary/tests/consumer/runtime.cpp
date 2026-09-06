#include <metasequoia/session.h>
#include "contracts/assets/assets.h"
#include "core/data_path.h"
#include "quanpin/quanpin_dictionary.h"
#include "user_dictionary/user_dictionary_journal.h"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace
{
void require(bool value, const char *message)
{
    if (!value) throw std::runtime_error(message);
}
void add_phrase(const metasequoia::RuntimePaths &paths, const std::string &key, const std::string &word)
{
    QuanpinDictionary dictionary({}, paths);
    require(dictionary.create_word_from_canonical_pinyin(key, word) == 0, "Cannot learn runtime phrase");
}
void check_phrase(const metasequoia::RuntimePaths &paths, const std::string &key, const std::string &word)
{
    QuanpinDictionary dictionary({}, paths);
    require(dictionary.find_candidate(key, word).has_value(), "Runtime generation lost a learned phrase");
}
}

int main(int argc, char **argv)
{
    if (argc != 3) return 2;
    try
    {
        namespace fs = std::filesystem;
        using namespace metasequoia;
        const auto resources = fs::u8path(argv[1]);
        const auto scratch = fs::u8path(argv[2]);
        const auto user = scratch / "user";
        const auto cache = scratch / "cache";
        const auto first = prepare_runtime_paths(resources, user, cache, "initial");
        {
            SessionOptions options;
            options.paths = first;
            options.learning = false;
            options.helpcode = false;
            Session session(options);
            for (char ch : std::string("nihao")) session.character(ch);
            const auto view = session.snapshot();
            require(std::any_of(view.candidates.begin(), view.candidates.end(),
                                [](const auto &item) { return item.word == "你好"; }),
                    "Public Session cannot query the produced runtime bundle");
        }
        const std::string first_key = "jia'gou'yan'shou'jia", first_word = "架构验收甲";
        const std::string second_key = "jia'gou'yan'shou'yi", second_word = "架构验收乙";
        add_phrase(first, first_key, first_word);
        check_phrase(first, first_key, first_word);
        // Reuse the produced profile to isolate generation switching from dictionary-content changes.
        // Hosts quiesce sessions and writers at this point; all objects above are destroyed.
        const auto upgraded = prepare_runtime_paths(resources, user, cache, "upgrade");
        check_phrase(upgraded, first_key, first_word);
        add_phrase(upgraded, second_key, second_word);
        const auto restored = prepare_runtime_paths(resources, user, cache, "initial");
        check_phrase(restored, second_key, second_word);
        fs::remove_all(cache);
        require(fs::is_regular_file(restored.user(assets::user_journal)), "Cache disposal removed the user journal");
        check_phrase(restored, first_key, first_word);
        check_phrase(restored, second_key, second_word);

        require(user_dictionary::record_upsert(path_to_utf8(restored.user(assets::user_journal)),
                    user_dictionary::DictionaryKind::Pinyin, "@", "无效词", 10), "Cannot create replay failure fixture");
        bool failed = false;
        try { (void)prepare_runtime_paths(resources, user, cache, "failed"); }
        catch (const std::runtime_error &) { failed = true; }
        require(failed, "Invalid replay published a runtime generation");
        require(!fs::exists(user / "dictionaries/failed") && !fs::exists(user / "dictionaries/failed.incoming"),
                "Failed preparation retained a generation or staging directory");
        check_phrase(restored, first_key, first_word);
        check_phrase(restored, second_key, second_word);
        std::cout << "Produced runtime bundle: query, learning, generation replay, rollback, cache disposal and failure recovery passed\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
