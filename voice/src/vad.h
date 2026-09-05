#pragma once
#include <vector>

#include <deque>

class VadSegmenter
{
  public:
    bool process(const float *samples, size_t count);
    bool should_flush() const;
    std::vector<float> take_audio();

  private:
    std::vector<float> buffer_;
    std::deque<float> pre_roll_buffer_;
    float silence_ms_ = 0.0f;
    bool active_ = false;

    static constexpr size_t PRE_ROLL_MS = 200;
    static constexpr size_t SAMPLE_RATE = 16000;
};