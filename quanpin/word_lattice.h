#pragma once

#include "../core/word_item.h"
#include "bigram_table.h"
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
// The unigram normalizer supplies the usual "fewer tokens win" bias.
//
// When WordLatticeOptions::bigram is supplied, each transition also earns
// bigram_weight * log(P(next|previous) / P(next)) from the table (see
// bigram_table.h). Without it every path spelling the same syllables is judged
// on its words' frequencies alone, which is why 配置于权限 used to beat
// 配置与权限: 于 is the commoner character and nothing else had an opinion.
// The term is a bonus rather than a replacement, so an absent pair leaves the
// path exactly where the unigram score put it.
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
    // Borrowed, not owned: one table is shared by every session and outlives them.
    const BigramTable *bigram = nullptr;
    // How much the transition term is allowed to move a path. Calibrated on
    // tests/scripts/build_eval_set.py output; see tests/src/eval_sentences.cpp.
    double bigram_weight = 1.0;
};

using WordLatticeLookup = std::function<std::vector<LatticeLexeme>(const Segments &span)>;

std::vector<LatticePath> decode_word_lattice(const Segments &syllables, const WordLatticeLookup &lookup,
                                             const WordLatticeOptions &options = {});

void merge_lattice_candidates(std::vector<WordItem> &candidates, const Segments &syllables,
                              const WordLatticeLookup &lookup, const std::string &typed_pinyin,
                              const WordLatticeOptions &options = {});

// Index of the first row a synthesised whole sentence may take, which is after the leading run of
// exact full-cover Database/UserDatabase hits. Both whole-sentence sources share it so neither can
// displace a dictionary entry that already answers the whole key, per the ranking above.
size_t whole_sentence_insert_position(const std::vector<WordItem> &candidates, size_t n_syllables);

} // namespace quanpin
