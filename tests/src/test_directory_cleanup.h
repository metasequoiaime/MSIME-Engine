#pragma once

#include "../../user_dictionary/user_dictionary_journal.h"

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <system_error>
#include <utility>

namespace metasequoia::test
{
class ScopedDataDirectoryCleanup
{
  public:
    explicit ScopedDataDirectoryCleanup(std::filesystem::path directory) : directory_(std::move(directory))
    {
    }

    ~ScopedDataDirectoryCleanup()
    {
        user_dictionary::close_default_user_database();
        std::error_code error;
        std::filesystem::remove_all(directory_, error);
        if (error)
        {
            std::fprintf(stderr, "Failed to remove test data directory: %s\n", error.message().c_str());
            if (std::uncaught_exceptions() == 0)
            {
                std::abort();
            }
        }
    }

    ScopedDataDirectoryCleanup(const ScopedDataDirectoryCleanup &) = delete;
    ScopedDataDirectoryCleanup &operator=(const ScopedDataDirectoryCleanup &) = delete;

  private:
    std::filesystem::path directory_;
};
} // namespace metasequoia::test
