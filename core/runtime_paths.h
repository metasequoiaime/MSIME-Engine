#pragma once
#include <filesystem>
#include <string>

namespace metasequoia
{
// Paths are captured at construction. Changing process environment variables cannot redirect
// an existing session. Mutable dictionary copies are durable user data, never disposable cache.
struct RuntimePaths
{
    std::filesystem::path resources;
    std::filesystem::path user_data;
    std::filesystem::path cache;
    std::filesystem::path dictionaries;

    static RuntimePaths legacy();
    std::filesystem::path resource(const std::filesystem::path &name) const;
    std::filesystem::path user(const std::filesystem::path &name) const;
    std::filesystem::path dictionary(const std::filesystem::path &name) const;
    void validate() const;
};

// The host verifies the published bundle and supplies its immutable content ID. Quiesce all
// sessions and user-data writers during preparation/switching. New generations are staged,
// replayed and renamed as a directory; failure never replaces a working generation.
RuntimePaths prepare_runtime_paths(const std::filesystem::path &resources,
                                   const std::filesystem::path &user_data,
                                   const std::filesystem::path &cache,
                                   const std::string &content_id);
}
