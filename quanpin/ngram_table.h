#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

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
// The file is mapped, not read. Fifteen megabytes read into a vector is fifteen megabytes of dirty anonymous
// memory, which is the whole budget of an iOS keyboard extension; mapped, the same bytes are clean and file
// backed, so the system can drop them under pressure and fault them back, exactly as it already does for the
// hundred-megabyte dictionary sitting next to it. Layout is little-endian, matching every platform the engine
// targets, and the header is sized so the key array lands eight-byte aligned from the mapping base.
class NgramTable
{
  public:
    ~NgramTable();
    NgramTable(const NgramTable &) = delete;
    NgramTable &operator=(const NgramTable &) = delete;

    // Returns nullptr when the file is missing or malformed. A host without a table decodes exactly as before.
    static std::unique_ptr<NgramTable> open(const std::filesystem::path &path);

    // The table every session should use. The mapping is cheap but not free, and the file never changes under a
    // running process, so the first caller for a path maps it and the rest borrow it. Lives until the process
    // exits; a missing file is remembered as nullptr rather than retried on every key.
    static const NgramTable *shared(const std::filesystem::path &path);

    // Stands in for the start of a sentence, so the first words of a composition have a context too.
    static const std::string &sentence_start();

    // 0 when the sequence is not in the table.
    double bonus(const std::string &previous, const std::string &next) const;
    double bonus(const std::string &before, const std::string &previous, const std::string &next) const;

    std::size_t size() const
    {
        return count_;
    }

  private:
    NgramTable() = default;
    double lookup(std::uint64_t key) const;

    // Owned mapping of the whole file, and the two arrays inside it.
    void *mapping_ = nullptr;
    std::size_t mapped_bytes_ = 0;
#ifdef _WIN32
    void *file_ = nullptr;
    void *section_ = nullptr;
#endif
    const std::uint64_t *keys_ = nullptr;
    const float *values_ = nullptr;
    std::size_t count_ = 0;
};

} // namespace quanpin
