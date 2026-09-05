#pragma once
#include <vector>
#include <mutex>
#include <functional>

class AudioCapture
{
  public:
    using AudioCallback = std::function<void(const float *, size_t)>;

    bool start(AudioCallback cb);
    void stop();

    void onAudio(const float *data, size_t count);

  private:
    AudioCallback callback_;
};