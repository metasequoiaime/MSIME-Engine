#include "ngram_table.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace quanpin
{
namespace
{

constexpr char kMagic[4] = {'M', 'S', 'N', 'G'};
constexpr std::uint32_t kVersion = 1;

// magic, version, count, reserved. Sixteen bytes keeps the key array eight-byte aligned from the mapping base,
// which is what lets the keys be read in place instead of copied out.
constexpr std::size_t kHeaderBytes = 16;

// Entry count that would take more address space than any plausible table; a truncated or foreign file that
// happens to carry the magic must not turn into a nonsense mapping length.
constexpr std::uint32_t kMaxEntries = 40u * 1000u * 1000u;

std::uint32_t read_u32(const unsigned char *at)
{
    std::uint32_t value = 0;
    std::memcpy(&value, at, sizeof(value));
    return value;
}

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

NgramTable::~NgramTable()
{
#ifdef _WIN32
    if (mapping_)
        UnmapViewOfFile(mapping_);
    if (section_)
        CloseHandle(section_);
    if (file_ && file_ != INVALID_HANDLE_VALUE)
        CloseHandle(file_);
#else
    if (mapping_)
        munmap(mapping_, mapped_bytes_);
#endif
}

std::unique_ptr<NgramTable> NgramTable::open(const std::filesystem::path &path)
{
    std::error_code error;
    const auto file_size = std::filesystem::file_size(path, error);
    if (error || file_size < kHeaderBytes || file_size > static_cast<std::uintmax_t>(SIZE_MAX))
        return nullptr;

    auto table = std::unique_ptr<NgramTable>(new NgramTable);
    table->mapped_bytes_ = static_cast<std::size_t>(file_size);

#ifdef _WIN32
    table->file_ = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
    if (table->file_ == INVALID_HANDLE_VALUE)
        return nullptr;
    table->section_ = CreateFileMappingW(table->file_, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!table->section_)
        return nullptr;
    table->mapping_ = MapViewOfFile(table->section_, FILE_MAP_READ, 0, 0, 0);
    if (!table->mapping_)
        return nullptr;
#else
    const int descriptor = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (descriptor < 0)
        return nullptr;
    table->mapping_ = mmap(nullptr, table->mapped_bytes_, PROT_READ, MAP_PRIVATE, descriptor, 0);
    // The mapping keeps the file alive on its own; holding the descriptor open would only spend one.
    ::close(descriptor);
    if (table->mapping_ == MAP_FAILED)
    {
        table->mapping_ = nullptr;
        return nullptr;
    }
#endif

    const auto *bytes = static_cast<const unsigned char *>(table->mapping_);
    if (std::memcmp(bytes, kMagic, sizeof(kMagic)) != 0 || read_u32(bytes + 4) != kVersion)
        return nullptr;
    const std::uint32_t count = read_u32(bytes + 8);
    if (count > kMaxEntries)
        return nullptr;
    const std::size_t needed = kHeaderBytes + static_cast<std::size_t>(count) * (sizeof(std::uint64_t) + sizeof(float));
    if (needed > table->mapped_bytes_)
        return nullptr;

    table->count_ = count;
    table->keys_ = reinterpret_cast<const std::uint64_t *>(bytes + kHeaderBytes);
    table->values_ = reinterpret_cast<const float *>(bytes + kHeaderBytes + count * sizeof(std::uint64_t));
    // Binary search returns wrong answers rather than misses on an unsorted file, so a file built by a different
    // tool has to be refused here. It costs one sequential pass over pages that stay clean and evictable.
    if (!std::is_sorted(table->keys_, table->keys_ + count))
        return nullptr;
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
    const auto *end = keys_ + count_;
    const auto *found = std::lower_bound(keys_, end, key);
    if (found == end || *found != key)
        return 0.0;
    return values_[static_cast<std::size_t>(found - keys_)];
}

double NgramTable::bonus(const std::string &previous, const std::string &next) const
{
    if (!count_ || next.empty())
        return 0.0;
    return lookup(fnv1a64({&previous, &next}));
}

double NgramTable::bonus(const std::string &before, const std::string &previous, const std::string &next) const
{
    if (!count_ || next.empty())
        return 0.0;
    return lookup(fnv1a64({&before, &previous, &next}));
}

} // namespace quanpin
