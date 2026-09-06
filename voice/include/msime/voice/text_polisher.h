#pragma once
#include "stt_service.h"
namespace metasequoia::voice
{
class TextPolisher
{
  public:
    TextPolisher(RequestOptions options, std::string system_prompt);
    TextPolisher(const std::string &api_token, const std::string &language);
    // Optional: return the original transcription on timeout, cancellation or error.
    std::string polish(const std::string &original_text) const;

  private:
    RequestOptions options_;
    std::string prompt_;
};
} // namespace metasequoia::voice
