#pragma once

#include "../core/query_request.h"
#include "../core/word_item.h"
#include "../common/cache.h"
#include "quanpin_dictionary.h"
#include <string>
#include <vector>

class QuanpinEngine
{
  public:
    QuanpinEngine();
    ~QuanpinEngine();

    std::vector<WordItem> query(const QueryRequest &request);
    bool expand_initial_candidates(const QueryRequest &request, std::vector<WordItem> &candidates);
    std::optional<WordItem> find_candidate(const std::string &key, const std::string &value);
    int handleVkCode(ImeKeyCode vk, ImeModifierMask modifiers_down, ImeCharacter wch = 0);
    int create_word(std::string pinyin, std::string word);
    int create_word_from_canonical_pinyin(std::string pinyin, std::string word);
    int update_weight_by_word(std::string word);
    int update_weight_by_pinyin_and_word(std::string pinyin, std::string word);
    int delete_by_pinyin_and_word(std::string pinyin, std::string word);
    int insert_word_to_series_cache(const std::string &pinyin, const std::string &word,
                                    CandidateSource source);
    std::string search_sentence_from_ime_engine(const std::string &user_pinyin);
    void reset_state();
    void reset_cache();

  private:
    QuanpinDictionary dictionary_;
};
