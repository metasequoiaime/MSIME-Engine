#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace quanpin
{

// Name of the packed table inside the dictionary directory. Shipping it is optional: the decoder works without it.
inline constexpr char kBigramFileName[] = "bigram.bin";

// Word-pair bonuses for the lattice, built by tests/scripts/build_bigram.py.
//
// Each entry is log( P(next | previous) / P(next) ): zero when the two words are independent, positive for a
// collocation, negative for a pair the corpus avoids. The decoder adds this to the unigram path score, so a pair
// the corpus never saw contributes nothing and cannot veto a path. That is the whole reason the table stores a
// ratio rather than a conditional probability - a missing entry has to be harmless, because the corpus behind it
// is always smaller than the language.
//
// Lookup keys are a 64-bit FNV-1a of "previous\0next", sorted for binary search. Storing hashes rather than the
// words themselves is what keeps a million pairs inside twelve megabytes; a collision mis-scores one pair out of
// millions, which is well below the noise the corpus itself carries.
//
// The packed file is little-endian, matching every platform the engine targets.
class BigramTable
{
  public:
    // Returns nullptr when the file is missing or malformed. A host without a table decodes exactly as before.
    static std::unique_ptr<BigramTable> open(const std::filesystem::path &path);

    // The table every session should use. A dozen megabytes is not worth loading once per dictionary instance, and
    // the file never changes under a running process, so the first caller for a path loads it and the rest borrow
    // it. Lives until the process exits; a missing file is remembered as nullptr rather than retried on every key.
    static const BigramTable *shared(const std::filesystem::path &path);

    // Stands in for the start of a sentence, so the first word of a composition has a context too.
    static const std::string &sentence_start();

    // 0 when the pair is not in the table.
    double bonus(const std::string &previous, const std::string &next) const;

    std::size_t size() const
    {
        return keys_.size();
    }

  private:
    std::vector<std::uint64_t> keys_;
    std::vector<float> values_;
};

} // namespace quanpin
