#pragma once

#include "../core/runtime_paths.h"
#include "../core/word_item.h"
#include "ngram_table.h"
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
// ngram_table.h). Without it every path spelling the same syllables is judged
// on its words' frequencies alone, which is why 配置于权限 used to beat
// 配置与权限: 于 is the commoner character and nothing else had an opinion.
// The term is a bonus rather than a replacement, so an absent pair leaves the
// path exactly where the unigram score put it.
//
// A trigram cannot be searched the same way without carrying two words of
// history in every beam entry, so WordLatticeOptions::trigram is applied after
// the search instead: the n best paths are rescored with what the third word of
// context adds over the second, then reordered. This is the reason to decode
// more paths than are shown. `emit` caps how many reach the candidate list, so
// a caller can search six and display one - the extra five exist to be
// reordered, not to fill the page with near-duplicate sentences.
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
    //
    // The bonus was 3.0, which was never measured against the eval sets - it was a plausible number
    // for a term whose only job was to stop the decoder spelling a sentence out character by
    // character. Swept through the client's convert_eval with the ngram tables present, whole-sentence
    // top-1 rises monotonically up to 20 and stops moving after it: sentences-v1 0.850 -> 0.900,
    // sentences-v2 0.125 -> 0.189, quanpin-words-v1 unchanged at 0.768. Raising it further trades
    // sentences-v1 away for nothing.
    double unigram_z = 1e6;
    double phrase_length_bonus = 20.0;
    // Borrowed, not owned: one table is shared by every session and outlives them.
    const NgramTable *bigram = nullptr;
    const NgramTable *trigram = nullptr;
    // How much each context term is allowed to move a path. See tests/src/eval_sentences.cpp, and the
    // sweep recorded above: at bonus 20 the bigram term is worth doubling - sentences-v2 top-1 0.173
    // -> 0.189 and MRR 0.393 -> 0.401, sentences-v1 unchanged - while 3.0 starts trading page
    // coverage for it. The trigram weight moved nothing at any value tried, which is what a term that
    // only fires on a third word of history looks like on sets this short; it stays at 1.0 rather
    // than being tuned to a number the measurement cannot support.
    double bigram_weight = 2.0;
    double trigram_weight = 1.0;
    // How many of the decoded paths reach the candidate list. 0 emits all of
    // them, which is what a caller measuring the decoder wants; a caller
    // feeding a candidate page wants 1.
    int emit = 0;
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

// The one place the sentence decoder is configured.
//
// Quanpin and shuangpin decode sentences identically and used to each build this block themselves.
// That duplication is how the two paths drift: engine PR #156 found a ranking fix that had been
// applied to one and missed in the other for weeks. Configuring them from here means a change to
// either reaches both, or reaches neither.
//
// `alternatives` is what a host asks for when it intends to reorder the readings itself. The
// default answers with one, which is what a candidate page wants: the other readings are near
// duplicates of it, and no host crops them, so emitting them unasked would push the short
// candidates a user actually wants off the first page.
WordLatticeOptions make_sentence_lattice_options(const metasequoia::RuntimePaths &paths, bool alternatives = false);

} // namespace quanpin
