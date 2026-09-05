#pragma once

#include "../core/word_item.h"
#include "quanpin_utils.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace quanpin
{

// Phrase-graph + Viterbi beam search over dictionary spans.
// Algorithm follows libpinyin PinyinLookup2 (unigram path score as a product of
// P(word), beam per syllable step) and sunpinyin's lattice columns.
// Bigram interpolation is omitted until a bigram table exists; the unigram
// normalizer supplies the usual "fewer tokens win" bias.
//
// Ranking when merging into an existing candidate list:
//   1. Exact SQLite full-key hits (CandidateSource::Database / UserDatabase)
//   2. Lattice full-cover sentences (CandidateSource::Generated)
//   3. Google-pinyin Fallback, prefixes, and other remaining items
// Lattice never displaces a leading exact Database/UserDatabase full-cover
// (e.g. 高碳钢 for gktjgh). Fallback must not block lattice (e.g. 高碳钢镊子
// ahead of 高谈刚捏子).
//
// merge_lattice_candidates only runs at 3+ complete syllables. One- and
// two-syllable keys are already covered by exact SQLite lookup. Abbreviated
// quanpin segments (g'k't) are rejected so WordItem.canonical_pinyin stays a
// complete pronunciation.
//
// Lattice WordItem.weight is log_prob * 1000 and is often negative. List
// order is the ranking; do not sort these rows by weight.

struct LatticeLexeme
{
    std::string key;
    std::string value;
    std::int64_t weight = 0;
};

struct LatticePath
{
    std::string sentence;
    std::string key;
    double log_prob = 0;
    std::vector<std::string> words;
};

struct WordLatticeOptions
{
    int beam = 32;
    int nbest = 5;
    // Cap on lexemes per span (injected lookups). DB lookup already applies
    // the same cap in query_segments_keyed_flat.
    int span_limit = 32;
    int max_phrase_syllables = 7;
    // Heuristic unigram normalizer vs phrase-length bonus. Single-char
    // msime.db weights are corpus counts; phrase weights are a smaller scale.
    // Not calibrated on the full dictionary.
    double unigram_z = 1e6;
    double phrase_length_bonus = 3.0;
};

using WordLatticeLookup = std::function<std::vector<LatticeLexeme>(const Segments &span)>;

std::vector<LatticePath> decode_word_lattice(const Segments &syllables, const WordLatticeLookup &lookup,
                                             const WordLatticeOptions &options = {});

void merge_lattice_candidates(std::vector<WordItem> &candidates, const Segments &syllables,
                              const WordLatticeLookup &lookup, const std::string &typed_pinyin,
                              const WordLatticeOptions &options = {});

} // namespace quanpin
