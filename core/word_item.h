#pragma once

#include <string>
#include <cstdint>
#include <utility>

enum class CandidateSource
{
    Database,
    UserDatabase,
    CloudSuggestion,
    AiSuggestion,
    EnglishDictionary,
    QuickPhrase,
    Emoji,
    Kaomoji,
    Generated,
    Fallback,
};

struct WordItem
{
    // The input code matched by this candidate.  This remains scheme/raw-input
    // oriented because composition advancement must consume exactly what the
    // user typed (including abbreviated pinyin).
    std::string pinyin;
    // The complete quanpin key read from the database.  It is deliberately
    // separate from pinyin: abbreviated quanpin and shuangpin candidates use
    // their typed code for advancement, but phrase creation must persist a
    // complete, unambiguous pronunciation.
    std::string canonical_pinyin;
    std::string word;
    std::int64_t weight = 0;
    CandidateSource source = CandidateSource::Database;
    int fixed_position = 0;

    WordItem() = default;
    WordItem(std::string pinyin_value, std::string word_value, std::int64_t weight_value,
             CandidateSource source_value = CandidateSource::Database, std::string canonical_pinyin_value = {})
        : pinyin(std::move(pinyin_value)), canonical_pinyin(std::move(canonical_pinyin_value)),
          word(std::move(word_value)), weight(weight_value), source(source_value)
    {
    }
};
