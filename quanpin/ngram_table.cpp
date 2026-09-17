#include "ngram_table.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>

namespace quanpin
{
namespace
{

constexpr char kMagic[4] = {'M', 'S', 'N', 'G'};
constexpr std::uint32_t kVersion = 1;

// Entry count that would take more memory than any plausible table; a truncated or foreign file that happens to
// carry the magic must not turn into a multi-gigabyte allocation.
constexpr std::uint32_t kMaxEntries = 40u * 1000u * 1000u;

std::uint64_t fnv1a64(std::initializer_list<const std::string *> words)
{
    std::uint64_t digest = 0xCBF29CE484222325ULL;
    const auto mix = [&digest](unsigned char byte) {
        digest ^= byte;
        digest *= 0x100000001B3ULL;
    };
    bool first = true;
    for (const std::string *word : words)
    {
        if (!first)
            mix(0);
        first = false;
        for (unsigned char byte : *word)
            mix(byte);
    }
    return digest;
}

} // namespace

const std::string &NgramTable::sentence_start()
{
    // Not a character any dictionary value can contain, so it cannot be confused with a real first word.
    static const std::string token("\x01");
    return token;
}

std::unique_ptr<NgramTable> NgramTable::open(const std::filesystem::path &path)
{
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error))
        return nullptr;
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return nullptr;

    char magic[4] = {};
    std::uint32_t version = 0;
    std::uint32_t count = 0;
    std::uint32_t reserved = 0;
    file.read(magic, sizeof(magic));
    file.read(reinterpret_cast<char *>(&version), sizeof(version));
    file.read(reinterpret_cast<char *>(&count), sizeof(count));
    file.read(reinterpret_cast<char *>(&reserved), sizeof(reserved));
    if (!file || std::memcmp(magic, kMagic, sizeof(kMagic)) != 0 || version != kVersion || count > kMaxEntries)
        return nullptr;

    auto table = std::unique_ptr<NgramTable>(new NgramTable);
    table->keys_.resize(count);
    table->values_.resize(count);
    if (count)
    {
        file.read(reinterpret_cast<char *>(table->keys_.data()),
                  static_cast<std::streamsize>(count * sizeof(std::uint64_t)));
        file.read(reinterpret_cast<char *>(table->values_.data()),
                  static_cast<std::streamsize>(count * sizeof(float)));
        if (!file)
            return nullptr;
        // Binary search is only correct on a sorted file, and a file built by a different tool might not be.
        if (!std::is_sorted(table->keys_.begin(), table->keys_.end()))
            return nullptr;
    }
    return table;
}

const NgramTable *NgramTable::shared(const std::filesystem::path &path)
{
    static std::mutex mutex;
    static std::map<std::filesystem::path, std::unique_ptr<NgramTable>> loaded;
    const std::lock_guard<std::mutex> lock(mutex);
    const auto found = loaded.find(path);
    if (found != loaded.end())
        return found->second.get();
    return loaded.emplace(path, open(path)).first->second.get();
}

double NgramTable::lookup(std::uint64_t key) const
{
    const auto found = std::lower_bound(keys_.begin(), keys_.end(), key);
    if (found == keys_.end() || *found != key)
        return 0.0;
    return values_[static_cast<std::size_t>(found - keys_.begin())];
}

double NgramTable::bonus(const std::string &previous, const std::string &next) const
{
    if (keys_.empty() || next.empty())
        return 0.0;
    return lookup(fnv1a64({&previous, &next}));
}

double NgramTable::bonus(const std::string &before, const std::string &previous, const std::string &next) const
{
    if (keys_.empty() || next.empty())
        return 0.0;
    return lookup(fnv1a64({&before, &previous, &next}));
}

} // namespace quanpin
