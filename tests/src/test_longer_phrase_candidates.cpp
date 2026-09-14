#include <metasequoia/session.h>

#include "../../contracts/assets/assets.h"
#include "../../core/data_path.h"
#include "../../english/english_dictionary.h"
#include "../../core/runtime_paths.h"
#include "test_directory_cleanup.h"

#include <sqlite3.h>

#include <algorithm>
#include <chrono>
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

// 同长度的表只留一个高权重词和一个冷僻同音词,续接词放在音节更多的表里,权重排在冷僻词之上 ——
// 这正是发货词库的形状: ping'guo 整张表是 苹果 1143881、评过 1180、平果 169、平锅 1,而
// 苹果电脑 21495、苹果公司 19725 在 tbl_4_p。
std::filesystem::path prepare_resources(const std::filesystem::path &root)
{
    const auto resources = root / "resources";
    std::filesystem::create_directories(resources);
    execute(resources / assets::main_dictionary,
            "CREATE TABLE tbl_1_n(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
            "INSERT INTO tbl_1_n VALUES('ni','n','你',8000),('ni','n','泥',7000);"
            "CREATE TABLE tbl_2_n(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
            "INSERT INTO tbl_2_n VALUES('ni''hao','nh','你好',10000),('ni''hao','nh','拟好',30);"
            "CREATE TABLE tbl_3_n(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
            "INSERT INTO tbl_3_n VALUES('ni''hao''ma','nhm','你好吗',900);"
            "CREATE TABLE tbl_4_n(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
            "INSERT INTO tbl_4_n VALUES('ni''hao''a''ya','nhay','你好啊呀',500);");
    require(EnglishDictionary::ensure_schema(path_to_utf8(resources / assets::english_dictionary)),
            "English schema failed");
    return resources;
}

std::vector<std::string> words(const Session &session)
{
    std::vector<std::string> result;
    for (const auto &candidate : session.snapshot().candidates)
    {
        result.push_back(candidate.word);
    }
    return result;
}

std::size_t index_of(const std::vector<std::string> &values, const std::string &wanted)
{
    const auto found = std::find(values.begin(), values.end(), wanted);
    require(found != values.end(), "The candidate list is missing an expected word.");
    return static_cast<std::size_t>(found - values.begin());
}
} // namespace

int main()
{
    try
    {
        const auto root =
            std::filesystem::temp_directory_path() /
            ("msime-longer-phrase-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        test::ScopedDataDirectoryCleanup cleanup(root);
        const auto resources = prepare_resources(root);

        {
            SessionOptions options;
            options.paths = prepare_runtime_paths(resources, root / "user-order", root / "cache-order", "v1");
            options.scheme = SchemeType::Quanpin;
            Session session(options);
            for (const char letter : std::string("nihao"))
            {
                session.character(letter);
            }
            const auto listed = words(session);
            require(!listed.empty() && listed.front() == "你好", "The exact match stopped leading the page.");
            // 续接词按权重和等长结果并成一组,所以 900 的 你好吗 排在 30 的 拟好 前面。
            require(index_of(listed, "你好吗") < index_of(listed, "拟好"),
                    "A continuation outweighing the leftover homophone still ranked below it.");
            require(index_of(listed, "你好啊呀") < index_of(listed, "拟好"),
                    "A four-syllable continuation never reached the page.");
            // 只匹配首音节的单字仍排在所有打全了的结果之后。
            require(index_of(listed, "拟好") < index_of(listed, "泥"),
                    "A first-syllable character outranked a full-spelling candidate.");
        }

        {
            SessionOptions options;
            options.paths = prepare_runtime_paths(resources, root / "user-select", root / "cache-select", "v1");
            options.scheme = SchemeType::Quanpin;
            Session session(options);
            for (const char letter : std::string("nihao"))
            {
                session.character(letter);
            }
            const auto snapshot = session.snapshot();
            const auto listed = words(session);
            const auto target = index_of(listed, "你好吗");
            // 推进按键入的那串走,造词持久化按完整读音走 —— WordItem 把两者分开正是为此。
            require(snapshot.candidates[target].pinyin == "ni'hao",
                    "A continuation advertised more typed input than the user gave it.");
            require(snapshot.candidates[target].canonical_pinyin == "ni'hao'ma",
                    "A continuation lost the full reading phrase creation needs.");
            const auto result = session.select(target);
            require(result.handled && result.commit.has_value() && *result.commit == "你好吗",
                    "Selecting a continuation did not commit it.");
            require(session.snapshot().preedit.empty(),
                    "Selecting a continuation left part of the typed input behind.");
        }
    }
    catch (const std::exception &error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    return 0;
}
