#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace quanpin
{

// Names of the packed tables inside the dictionary directory. Shipping either is optional: the decoder works
// without both, with only the bigram, or with both.
inline constexpr char kBigramFileName[] = "bigram.bin";
inline constexpr char kTrigramFileName[] = "trigram.bin";

// Word-sequence bonuses for the lattice, built by tests/scripts/build_ngram.py.
//
// A bigram entry holds log( P(next | previous) / P(next) ) and a trigram entry holds
// log( P(next | before, previous) / P(next | previous) ). Both are increments over what the shorter context
// already said, which is what lets them be added to a score rather than replace it: an absent entry contributes
// nothing, and a table built from a corpus smaller than the language cannot veto a phrase it has never seen.
// It also means the two tables compose - the trigram adds only what the bigram did not already know.
//
// Lookup keys are a 64-bit FNV-1a of the words joined by NUL, sorted for binary search. Storing hashes rather
// than the words themselves is what keeps a million entries inside twelve megabytes; a collision mis-scores one
// sequence out of millions, which is well below the noise the corpus itself carries.
//
// The packed file is little-endian, matching every platform the engine targets.
class NgramTable
{
  public:
    // Returns nullptr when the file is missing or malformed. A host without a table decodes exactly as before.
    static std::unique_ptr<NgramTable> open(const std::filesystem::path &path);

    // The table every session should use. A dozen megabytes is not worth loading once per dictionary instance, and
    // the file never changes under a running process, so the first caller for a path loads it and the rest borrow
    // it. Lives until the process exits; a missing file is remembered as nullptr rather than retried on every key.
    static const NgramTable *shared(const std::filesystem::path &path);

    // Stands in for the start of a sentence, so the first words of a composition have a context too.
    static const std::string &sentence_start();

    // 0 when the sequence is not in the table.
    double bonus(const std::string &previous, const std::string &next) const;
    double bonus(const std::string &before, const std::string &previous, const std::string &next) const;

    std::size_t size() const
    {
        return keys_.size();
    }

  private:
    double lookup(std::uint64_t key) const;

    std::vector<std::uint64_t> keys_;
    std::vector<float> values_;
};

} // namespace quanpin
