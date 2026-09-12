#pragma once

#include "word_item.h"
#include <algorithm>
#include <string>
#include <vector>

// Replace a source group atomically while preserving the provider's order.
inline bool replace_online_candidate_batch(std::vector<WordItem> &list, const std::string &key,
                                           const std::vector<std::string> &words, CandidateSource source)
{
    const size_t limit = source == CandidateSource::AiSuggestion ? 10 : 1;
    if ((source != CandidateSource::AiSuggestion && source != CandidateSource::CloudSuggestion) ||
        words.empty() || words.size() > limit ||
        std::any_of(words.begin(), words.end(), [](const std::string &word) {
            return word.empty() || word.size() > 4096 ||
                   std::any_of(word.begin(), word.end(), [](unsigned char c) { return c < 32 || c == 127; });
        }))
        return false;
    list.erase(std::remove_if(list.begin(), list.end(),
                              [source](const WordItem &item) { return item.source == source; }), list.end());
    size_t index = std::min<size_t>(source == CandidateSource::AiSuggestion ? 2 : 1, list.size());
    for (const auto &word : words)
    {
        if (std::any_of(list.begin(), list.end(), [&](const WordItem &item) { return item.word == word; }))
            continue;
        list.insert(list.begin() + index++, WordItem(key, word, 1, source));
    }
    if (source == CandidateSource::CloudSuggestion)
    {
        std::vector<WordItem> ai;
        for (const auto &item : list)
            if (item.source == CandidateSource::AiSuggestion) ai.push_back(item);
        list.erase(std::remove_if(list.begin(), list.end(), [](const WordItem &item) {
            return item.source == CandidateSource::AiSuggestion;
        }), list.end());
        list.insert(list.begin() + std::min<size_t>(2, list.size()), ai.begin(), ai.end());
    }
    return true;
}
