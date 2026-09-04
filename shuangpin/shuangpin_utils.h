#pragma once

#include "../core/data_path.h"
#include "shuangpin_profile.h"
#include <filesystem>
#include <string>

namespace shuangpin
{
std::string get_local_appdata_path();
std::string get_app_name();
std::filesystem::path get_data_file_path(const std::filesystem::path &relative_path);
} // namespace shuangpin

class ShuangpinUtil
{
  public:
    static std::string get_local_appdata_path();
    static std::string local_appdata_path;
    static const std::string app_name;

    static std::string cvt_single_sp_to_pinyin(std::string sp_str,
                                               const ShuangpinProfile &profile = GetXiaoheShuangpinProfile());
    static std::string pinyin_segmentation(std::string sp_str,
                                           const ShuangpinProfile &profile = GetXiaoheShuangpinProfile());
    static std::string::size_type get_first_char_size(std::string words);
    static std::string::size_type count_utf8_chars(const std::string &str);
    static std::string extract_preview(std::string candidate);
    static bool is_all_complete_pinyin(std::string pure_pinyin, std::string seg_pinyin);
    static std::string convert_seg_shuangpin_to_seg_complete_pinyin(
        std::string seg_shangpin, const ShuangpinProfile &profile = GetXiaoheShuangpinProfile());

    static bool IsFullHelpMode(std::string pinyin,
                               const ShuangpinProfile &profile = GetXiaoheShuangpinProfile());
    static std::string GetFullHelpCodes(std::string pinyin);
};
