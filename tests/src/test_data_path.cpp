#include "../../core/data_path.h"

#include <cstdlib>
#include <filesystem>
#include <stdexcept>

namespace
{
void set_data_directory(const std::filesystem::path &path)
{
#ifdef _WIN32
    if (_wputenv_s(L"METASEQUOIA_IME_DATA_DIR", path.c_str()) != 0)
#else
    if (setenv("METASEQUOIA_IME_DATA_DIR", metasequoia::path_to_utf8(path).c_str(), 1) != 0)
#endif
    {
        throw std::runtime_error("Failed to set the data directory override.");
    }
}
} // namespace

int main()
{
    const std::filesystem::path expected = std::filesystem::temp_directory_path() / std::filesystem::u8path("metasequoia-ime-词库");
    set_data_directory(expected);
    if (metasequoia::data_directory() != expected)
    {
        throw std::runtime_error("The data directory override was ignored.");
    }
    if (metasequoia::data_file_path("msime.db") != expected / "msime.db")
    {
        throw std::runtime_error("The data file path was not joined with the platform separator.");
    }
    if (metasequoia::path_to_utf8(metasequoia::data_file_path(std::filesystem::u8path("词库.db"))) != metasequoia::path_to_utf8(expected / std::filesystem::u8path("词库.db")))
    {
        throw std::runtime_error("The data file path was not preserved as UTF-8.");
    }
    return 0;
}
