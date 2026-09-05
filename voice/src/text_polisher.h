#pragma once

#include <string>

class TextPolisher
{
  public:
    TextPolisher(const std::string &api_token, const std::string &language);
    std::string polish(const std::string &original_text) const;

  private:
    std::string api_token_;
    std::string language_;
    std::string api_url_ = "https://api.siliconflow.cn/v1/chat/completions";
    std::string model_ = "Qwen/Qwen3-8B";
};
