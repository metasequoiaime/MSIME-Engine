#pragma once
#include "stt_service.h"
#include <mutex>
struct whisper_context;
namespace metasequoia::voice
{
class WhisperWorker : public SttService
{
  public:
    explicit WhisperWorker(const char *model_path, std::string language = "zh");
    ~WhisperWorker();
    WhisperWorker(const WhisperWorker &) = delete;
    WhisperWorker &operator=(const WhisperWorker &) = delete;
    std::string recognize(const std::vector<float> &pcm) override;

  private:
    whisper_context *ctx_ = nullptr;
    std::string language_;
    std::mutex mutex_;
};
} // namespace metasequoia::voice
