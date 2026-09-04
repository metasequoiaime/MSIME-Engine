#pragma once

#include "../core/word_item.h"
#include <cstddef>
#include <sqlite3.h>
#include <string>
#include <unordered_map>
#include <vector>

class EnglishDictionary
{
  public:
    explicit EnglishDictionary(std::string db_path, bool initialize_schema = true);
    ~EnglishDictionary();

    EnglishDictionary(const EnglishDictionary &) = delete;
    EnglishDictionary &operator=(const EnglishDictionary &) = delete;

    std::vector<WordItem> query_prefix(const std::string &prefix, size_t limit = 5);
    std::string query_chinese_gloss(const std::string &english);
    std::string query_english_gloss(const std::string &chinese);
    bool ready();
    static bool ensure_schema(const std::string &db_path);
    static bool upsert_gloss(const std::string &db_path, bool chinese_to_english, const std::string &key,
                             const std::string &gloss);

  private:
    bool ensure_query_statement();
    bool ensure_gloss_statements();
    void load_custom_translations();
    void close_database();

  private:
    std::string db_path_;
    sqlite3 *db_ = nullptr;
    sqlite3_stmt *query_statement_ = nullptr;
    sqlite3_stmt *en_zh_statement_ = nullptr;
    sqlite3_stmt *zh_en_statement_ = nullptr;
    std::unordered_map<std::string, std::string> custom_en_zh_;
    std::unordered_map<std::string, std::string> custom_zh_en_;
};
