#pragma once
#include <cstdint>

namespace metasequoia
{
// Stable bit assignments shared by platform preferences. Zero preserves exact matching.
enum class FuzzyPinyinRule : std::uint32_t
{
    Z_ZH = 1u << 0,
    C_CH = 1u << 1,
    S_SH = 1u << 2,
    N_L = 1u << 3,
    F_H = 1u << 4,
    R_L = 1u << 5,
    AN_ANG = 1u << 6,
    EN_ENG = 1u << 7,
    IN_ING = 1u << 8,
    IAN_IANG = 1u << 9,
    UAN_UANG = 1u << 10
};
struct FuzzyPinyinOptions
{
    std::uint32_t rules = 0;
    bool enabled(FuzzyPinyinRule rule) const
    {
        return (rules & static_cast<std::uint32_t>(rule)) != 0;
    }
};
} // namespace metasequoia
