#pragma once
// Included only while compiling the pinned decoder's dicttrie/userdict translation units.
// Their unqualified calls resolve these namespace overloads; no CRT macros or vendor edits.
#ifdef _WIN32
#include <cstdio>
#include <filesystem>
#include <io.h>
#include <string>
namespace ime_pinyin
{
inline FILE *fopen(const char *path, const char *mode)
{
    const std::wstring wide_mode(mode, mode + std::char_traits<char>::length(mode));
    return _wfopen(std::filesystem::u8path(path).c_str(), wide_mode.c_str());
}
inline int open(const char *path, int flags)
{
    return _wopen(std::filesystem::u8path(path).c_str(), flags);
}
} // namespace ime_pinyin
#endif
