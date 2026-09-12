#pragma once

#include "../core/query_request.h"
#include "../core/scheme_type.h"
#include "../core/word_item.h"
#include <string>
#include <vector>
#include <optional>

class ICandidateProvider
{
  public:
    virtual ~ICandidateProvider() = default;

    virtual std::vector<WordItem> query(const QueryRequest &request) = 0;
    virtual std::optional<WordItem> find_candidate(SchemeType scheme, const std::string &key,
                                                   const std::string &value) = 0;
    virtual void reset_cache() = 0;
    virtual int create_word(SchemeType scheme, std::string pinyin, std::string word) = 0;
    virtual int update_weight_by_pinyin_and_word(SchemeType scheme, std::string pinyin, std::string word) = 0;
    virtual int delete_by_pinyin_and_word(SchemeType scheme, std::string pinyin, std::string word) = 0;
    virtual int cache_dynamic_candidate(SchemeType scheme, const std::string &pinyin, const std::string &word,
                                        CandidateSource source) = 0;
    virtual int cache_dynamic_candidate_for_request(const QueryRequest &request, const std::string &word,
                                                    CandidateSource source) = 0;
    virtual int cache_dynamic_candidate_for_request(const QueryRequest &request,
                                                    const std::vector<std::string> &words,
                                                    CandidateSource source)
    {
        return words.size() == 1 ? cache_dynamic_candidate_for_request(request, words.front(), source) : -1;
    }
};
