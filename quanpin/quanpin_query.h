#pragma once

#include "word_lattice.h"

#include <sqlite3.h>

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <cstdint>

namespace quanpin
{

using Segments = std::vector<std::string>;
using QueryItem = std::pair<std::string, std::int64_t>;

struct KeyedQueryItem
{
    std::string key;
    std::string value;
    std::int64_t weight = 0;
};

enum class QuerySource
{
    Quanpin,
    Shuangpin,
};

struct QueryResultEntry
{
    Segments segments;
    std::string key;
    std::string table;
    std::vector<QueryItem> items;
};

struct QueryResult
{
    std::string pinyin;
    std::string mode;
    std::vector<QueryResultEntry> results;
};

Segments cut_pinyin_greedy(const std::string &pinyin, bool intact_only = false);
std::vector<Segments> cut_pinyin_by_mode(const std::string &pinyin, const std::string &mode = "greedy");
Segments split_segments(const std::string &segmentation);
std::string join_segments(const Segments &segments, const std::string &delimiter = "'");
std::string build_table_name(const Segments &segments);
std::string segments_to_jianpin(const Segments &segments);
std::string get_default_db_path();
void warm_up(sqlite3 *db, std::unordered_map<std::string, sqlite3_stmt *> &statement_cache);
std::vector<KeyedQueryItem> query_initial(sqlite3 *db, const std::string &prefix, int limit);
QueryResult query_words(const std::string &pinyin, const std::string &db_path, const std::string &mode = "greedy",
                        int limit = 8);
QueryResult query_segments(const Segments &segments,
                           const std::string &db_path,
                           int limit = 8,
                           QuerySource source = QuerySource::Quanpin);
std::vector<QueryItem> query_words_flat(const std::string &pinyin, const std::string &db_path,
                                        const std::string &mode = "greedy", int limit = 8);
std::vector<QueryItem> query_segments_flat(const Segments &segments,
                                           const std::string &db_path,
                                           int limit = 8,
                                           QuerySource source = QuerySource::Quanpin);
std::vector<QueryItem> query_segments_flat(const Segments &segments,
                                           sqlite3 *db,
                                           std::unordered_map<std::string, sqlite3_stmt *> &statement_cache,
                                           int limit = 8,
                                           QuerySource source = QuerySource::Quanpin);
std::vector<KeyedQueryItem> query_segments_keyed_flat(
    const Segments &segments,
    const std::string &db_path,
    int limit = 8,
    QuerySource source = QuerySource::Quanpin);
std::vector<KeyedQueryItem> query_segments_keyed_flat(
    const Segments &segments,
    sqlite3 *db,
    std::unordered_map<std::string, sqlite3_stmt *> &statement_cache,
    int limit = 8,
    QuerySource source = QuerySource::Quanpin);
std::vector<KeyedQueryItem> query_exact_segmentations_keyed_flat(
    const std::vector<Segments> &segmentations, sqlite3 *db,
    std::unordered_map<std::string, sqlite3_stmt *> &statement_cache, int limit = 128);

WordLatticeLookup make_lattice_db_lookup(sqlite3 *db, std::unordered_map<std::string, sqlite3_stmt *> &statement_cache,
                                         QuerySource source, int span_limit);

} // namespace quanpin
