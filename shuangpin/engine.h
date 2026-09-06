#pragma once

#include "../core/query_request.h"
#include "../core/word_item.h"
#include "shuangpin_dictionary.h"
#include <string>
#include <vector>

class ShuangpinEngine
{
  public:
    explicit ShuangpinEngine(const ShuangpinProfile &profile = GetXiaoheShuangpinProfile(),
                             metasequoia::RuntimePaths paths = metasequoia::RuntimePaths::legacy());
    std::vector<WordItem> query(const QueryRequest &request);
    bool expand_initial_candidates(const QueryRequest &request, std::vector<WordItem> &candidates);
    std::optional<WordItem> find_candidate(const std::string &key, const std::string &value);
    int create_word(std::string pinyin, std::string word);
    int update_weight_by_pinyin_and_word(std::string pinyin, std::string word);
    int delete_by_pinyin_and_word(std::string pinyin, std::string word);
    int insert_word_to_series_cache(const std::string &pinyin, const std::string &word, CandidateSource source);
    int insert_word_to_active_helpcode_cache(const std::string &pinyin, const std::string &word,
                                             CandidateSource source);
    std::string search_sentence_from_ime_engine(const std::string &user_pinyin);
    void reset_cache();

    void set_helpcode_keymap(HelpcodeUtils::SharedKeymap table)
    {
        dictionary_.set_helpcode_keymap(std::move(table));
    }

  private:
    const ShuangpinProfile profile_;
    ShuangpinDictionary dictionary_;
};
