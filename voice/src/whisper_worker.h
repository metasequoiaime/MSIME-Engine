#pragma once
#include "stt_service.h"
#include <string>
#include <vector>

struct whisper_context;

class WhisperWorker : public SttService
{
  public:
    explicit WhisperWorker(const char *model_path);
    ~WhisperWorker();

    std::string recognize(const std::vector<float> &pcm) override;

  private:
    whisper_context *ctx_;
};