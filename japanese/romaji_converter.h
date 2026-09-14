#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace japanese
{
struct RomajiConversion
{
    std::string hiragana;
    std::string pending;
    bool complete = false;
};

RomajiConversion ConvertRomaji(std::string_view input);
std::string HiraganaToKatakana(std::string_view hiragana);
std::string HiraganaToRomaji(std::string_view kana);
bool IsSingleKanaConversion(const RomajiConversion &conversion);

// 小書き → 濁点 → 半濁点 → 原样,循环一圈。
//
// This is the whole job of the 小゛゜ key every Japanese keyboard has: it modifies the kana that
// was just typed rather than inserting a new one. Variants a kana does not have are skipped, so
// か cycles か→が→か while は cycles は→ば→ぱ→は and つ adds っ. A kana with no variants at all
// comes back unchanged.
std::string NextKanaVariant(std::string_view kana);

// Romaji prefixes such as "k" or "ky" map to every table kana whose spelling
// starts with that prefix. This is the Japanese counterpart of Google Pinyin's
// half spelling id (shengmu).
std::vector<std::string> KanaForRomajiPrefix(std::string_view pending);
} // namespace japanese
