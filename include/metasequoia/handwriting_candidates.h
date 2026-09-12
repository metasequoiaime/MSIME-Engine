#pragma once
#include <algorithm>
#include <string>
#include <vector>

namespace metasequoia::handwriting
{
// Windows HandwritingPanel::ContainsCjk ranges, applied to UTF-8 candidates.
inline bool contains_cjk(const std::string &text)
{
    for (size_t i = 0; i + 2 < text.size(); ++i)
    {
        const auto first = static_cast<unsigned char>(text[i]);
        const auto second = static_cast<unsigned char>(text[i + 1]);
        const auto third = static_cast<unsigned char>(text[i + 2]);
        if (first < 0xe0 || first > 0xef || (second & 0xc0) != 0x80 || (third & 0xc0) != 0x80)
            continue;
        const unsigned cp = ((first & 0x0f) << 12) | ((second & 0x3f) << 6) | (third & 0x3f);
        if ((cp >= 0x3400 && cp <= 0x4dbf) || (cp >= 0x4e00 && cp <= 0x9fff) ||
            (cp >= 0xf900 && cp <= 0xfaff)) return true;
    }
    return false;
}

// Preserve recognizer order within each group; prefer Chinese, then other text.
// This shared policy is independent of the optional offline recognition backend.
inline std::vector<std::string> order_candidates(const std::vector<std::string> &input)
{
    std::vector<std::string> result;
    for (const auto &candidate : input)
    {
        if (!candidate.empty() && candidate.size() <= 4096 &&
            std::find(result.begin(), result.end(), candidate) == result.end())
            result.push_back(candidate);
    }
    std::stable_partition(result.begin(), result.end(), contains_cjk);
    if (result.size() > 12) result.resize(12);
    return result;
}
} // namespace metasequoia::handwriting
