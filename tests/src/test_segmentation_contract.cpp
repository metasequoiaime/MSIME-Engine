#include "quanpin/quanpin_dictionary.h"
#include <algorithm>
#include <filesystem>
#include <sqlite3.h>
#include <stdexcept>
#include <iostream>
namespace
{
std::filesystem::path CreatePinyinCacheDatabase()
{
    const auto path = std::filesystem::temp_directory_path() / "msime-segmentation-contract-test.db";
    std::filesystem::remove(path);
    sqlite3 *db = nullptr;
    if (sqlite3_open(path.string().c_str(), &db) != SQLITE_OK)
    {
        throw std::runtime_error("Failed to create temporary pinyin database.");
    }
    const char *sql =
        "PRAGMA journal_mode=WAL;"
        "CREATE TABLE tbl_1_n(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
        "CREATE TABLE tbl_1_a(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
        "CREATE TABLE tbl_1_x(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
        "CREATE TABLE tbl_2_a(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
        "CREATE TABLE tbl_2_x(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
        "CREATE TABLE tbl_3_a(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
        "CREATE TABLE tbl_3_x(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
        "CREATE TABLE tbl_4_x(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
        "CREATE TABLE tbl_5_x(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
        "CREATE TABLE tbl_6_x(key TEXT,jp TEXT,value TEXT,weight INTEGER);"
        "INSERT INTO tbl_3_a VALUES('ao''shi''ke','ask','奥湿克',1);"
        "INSERT INTO tbl_1_x VALUES('xian','x','__primary_xian_1__',1000);"
        "INSERT INTO tbl_1_x VALUES('xian','x','__primary_xian_2__',900);"
        "INSERT INTO tbl_1_x VALUES('xian','x','__primary_xian_3__',800);"
        "INSERT INTO tbl_2_x VALUES('xi''an','xa','__alternative_xi_an__',1);"
        "INSERT INTO tbl_4_x VALUES('xi''an''xian''xian','xaxx','__three_syllable_alternative__',100);"
        "INSERT INTO tbl_5_x VALUES('xi''an''xian''xian''xian','xaxxx','__four_syllable_alternative__',100);"
        "INSERT INTO tbl_6_x VALUES('xi''an''xian''xian''xian''xian','xaxxxx','__five_syllable_alternative__',100);";
    const int result = sqlite3_exec(db, sql, nullptr, nullptr, nullptr);
    sqlite3_close(db);
    if (result != SQLITE_OK)
    {
        std::filesystem::remove(path);
        throw std::runtime_error("Failed to initialize temporary pinyin database.");
    }
    return path;
}
} // namespace
int main()
{
    const auto path = CreatePinyinCacheDatabase();
    int result = 0;
    {
        QuanpinDictionary dictionary(path.string());
        const auto four = dictionary.query("xianxianxianxian", "xian'xian'xian'xian");
        const auto five = dictionary.query("xianxianxianxianxian", "xian'xian'xian'xian'xian");
        const auto manual = dictionary.query("xian'xian'xian'xian", "xian'xian'xian'xian");
        auto has = [](const auto &candidates, const char *word) {
            return std::any_of(candidates.begin(), candidates.end(),
                               [word](const auto &item) { return item.word == word; });
        };
        if (!has(four, "__four_syllable_alternative__") || has(five, "__five_syllable_alternative__") ||
            has(manual, "__four_syllable_alternative__"))
        {
            std::cerr << "Four-syllable ambiguity, five-syllable bound or explicit boundary contract failed\n";
            result = 1;
        }
    }
    std::filesystem::remove(path);
    return result;
}
