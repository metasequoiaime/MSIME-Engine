#include <metasequoia/session.h>

#include "../../contracts/assets/assets.h"
#include "../../core/data_path.h"
#include "../../core/input_session.h"
#include "../../core/runtime_paths.h"
#include "../../english/english_dictionary.h"
#include "test_directory_cleanup.h"

#include <sqlite3.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
using namespace metasequoia;

void require(bool value, const char *message)
{
    if (!value)
    {
        throw std::runtime_error(message);
    }
}

void execute(const std::filesystem::path &path, const std::string &sql)
{
    sqlite3 *database = nullptr;
    require(sqlite3_open(path_to_utf8(path).c_str(), &database) == SQLITE_OK, "Fixture open failed");
    const int result = sqlite3_exec(database, sql.c_str(), nullptr, nullptr, nullptr);
    sqlite3_close(database);
    require(result == SQLITE_OK, "Fixture SQL failed");
}

std::vector<std::string> words(const InputSession &session)
{
    std::vector<std::string> result;
    result.reserve(session.candidates().size());
    for (const auto &candidate : session.candidates())
    {
        result.push_back(candidate.word);
    }
    return result;
}

std::vector<std::string> type(InputSession &session, const std::string &code)
{
    for (const char letter : code)
    {
        session.handle_character(letter);
    }
    return words(session);
}

// w is a simplified code, wq carries one word of its own and three longer codes, and 你 sits under
// both wq and wqi the way a word the shipped table reaches through several codes does. Asserting
// against the shipped dictionary would tie these to whatever a checkout happens to have built.
std::filesystem::path prepare_resources(const std::filesystem::path &root)
{
    const auto resources = root / "resources";
    std::filesystem::create_directories(resources);
    execute(resources / assets::main_dictionary, "CREATE TABLE wubi86(key TEXT,value TEXT,weight INTEGER);"
                                                 "INSERT INTO wubi86 VALUES('w','人',20);"
                                                 "INSERT INTO wubi86 VALUES('wq','你',10);"
                                                 "INSERT INTO wubi86 VALUES('wqb','爷',20);"
                                                 "INSERT INTO wubi86 VALUES('wqi','你',10);"
                                                 "INSERT INTO wubi86 VALUES('wqbb','父子',30);");
    require(EnglishDictionary::ensure_schema(path_to_utf8(resources / assets::english_dictionary)),
            "English schema failed");
    return resources;
}

// InputSession is neither copyable nor movable, so a session is built where it is used.
RuntimePaths paths_for(const std::filesystem::path &resources, const std::filesystem::path &root,
                       const std::string &tag)
{
    return prepare_runtime_paths(resources, root / ("user-" + tag), root / ("cache-" + tag), "v1");
}
} // namespace

int main()
{
    try
    {
        const auto root = std::filesystem::temp_directory_path() /
                          ("msime-wubi-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        test::ScopedDataDirectoryCleanup cleanup(root);
        const auto resources = prepare_resources(root);
        int counter = 0;
        const auto next = [&counter] { return std::to_string(counter++); };

        // An unfinished code offers the codes it can still become, shortest first, rather than the
        // one or two entries that happen to carry the typed letters as a whole code.
        {
            InputSession session(SchemeType::Wubi, GetXiaoheShuangpinProfile(), paths_for(resources, root, next()));
            require(type(session, "wq") == (std::vector<std::string>{"你", "爷", "父子"}),
                    "An unfinished wubi code did not offer the codes it can still become.");
        }

        // A word reachable through several codes is listed once, under the shortest of them, so the
        // key that ranking and removal act on stays the one the candidate was found by.
        {
            InputSession session(SchemeType::Wubi, GetXiaoheShuangpinProfile(), paths_for(resources, root, next()));
            type(session, "wq");
            const auto &items = session.candidates();
            require(std::count_if(items.begin(), items.end(), [](const auto &item) { return item.word == "你"; }) == 1,
                    "A word reachable through several codes was listed more than once.");
            require(items.front().pinyin == "wq", "A candidate did not carry the code it was found by.");
        }

        // A simplified code keeps its own word at the head of the longer codes it prefixes.
        {
            InputSession session(SchemeType::Wubi, GetXiaoheShuangpinProfile(), paths_for(resources, root, next()));
            const auto candidates = type(session, "w");
            require(!candidates.empty() && candidates.front() == "人",
                    "A simplified code lost its own word to the codes it prefixes.");
            require(candidates.size() == 4, "A simplified code did not reach the codes it prefixes.");
        }

        // A complete code answers with itself: no wubi code is longer than four letters, so there
        // is nothing further for the prefix to reach, and a lone four-letter candidate stays lone
        // for the hosts that auto-commit it.
        {
            InputSession session(SchemeType::Wubi, GetXiaoheShuangpinProfile(), paths_for(resources, root, next()));
            require(type(session, "wqbb") == (std::vector<std::string>{"父子"}),
                    "A complete four-letter code did not answer with itself alone.");
        }

        // Letters no code starts with stay unanswered, which is what mixed pinyin waits for.
        {
            InputSession session(SchemeType::Wubi, GetXiaoheShuangpinProfile(), paths_for(resources, root, next()));
            require(type(session, "wx").empty(), "A code no entry starts with was answered anyway.");
        }
    }
    catch (const std::exception &error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    return 0;
}
