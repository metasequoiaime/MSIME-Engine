#pragma once
#include "stt_service.h"
namespace metasequoia::voice
{
class CloudSttWorker : public SttService
{
  public:
    explicit CloudSttWorker(RequestOptions options);
    explicit CloudSttWorker(const std::string &api_token);
    std::string recognize(const std::vector<float> &pcm) override;

  private:
    RequestOptions options_;
};
} // namespace metasequoia::voice
