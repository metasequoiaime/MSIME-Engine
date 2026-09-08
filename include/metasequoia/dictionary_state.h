#pragma once

#include "../../core/runtime_paths.h"
#include "personal_dictionary.h"
#include <functional>
#include <variant>

namespace metasequoia
{
struct DictionaryStateEntry
{
    PersonalDictionaryKind kind = PersonalDictionaryKind::Pinyin;
    std::string key;
    std::string value;
    std::int64_t weight = 0;
    std::string display;
    bool deleted = false;
    bool user_inserted = false;
};
struct DictionaryStatePosition
{
    std::string context;
    std::string key;
    std::string value;
    int position = 0;
};
struct DictionaryStateSelection
{
    std::string context;
    std::string key;
    std::string value;
    int count = 0;
};
using DictionaryStateRecord = std::variant<DictionaryStateEntry, DictionaryStatePosition, DictionaryStateSelection>;

// Streams one consistent journal read transaction. Returning false cancels; errors throw.
// Does not export process handoff receipts, credentials, or platform preferences.
void stream_dictionary_state(const RuntimePaths &paths, const std::function<bool(const DictionaryStateRecord &)> &emit);

// Prepare a complete replacement in an exclusively created generation directory. next returns
// false only at verified EOF and throws on malformed/truncated input or cancellation. Callers
// validate their transport envelope/checksum before returning EOF. At most 500,000 records.
// Duplicate identities/positions are rejected. Failure removes this operation's new directory;
// existing generations are never modified. Returned paths contain the rebuilt dictionaries,
// journal, selection counts and fixed positions together. The host must quiesce input, atomically
// publish its active-generation pointer, and recreate sessions before using the returned paths.
// Preparing a generation alone does not switch input or delete older generations.
RuntimePaths stage_dictionary_state(const std::filesystem::path &resources, const std::filesystem::path &generation,
                                    const std::string &content_id,
                                    const std::function<bool(DictionaryStateRecord &)> &next);
} // namespace metasequoia
