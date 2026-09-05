//
// Voice Activity Detector
//
#include "vad.h"
#include <cmath>

static constexpr float RMS_THRESHOLD = 0.01f;     // 声音能量阈值
static constexpr float SILENCE_LIMIT_MS = 500.0f; // 连续静音持续时间阈值

bool VadSegmenter::process(const float *samples, size_t count)
{
    float rms = 0.0f; // root mean square, 均方根
    for (size_t i = 0; i < count; ++i)
        rms += samples[i] * samples[i];

    rms = std::sqrt(rms / count);

    if (rms > RMS_THRESHOLD) // 这一段采样的声音的能量大于阈值
    {
        if (!active_) // 如果之前是静音
        {
            active_ = true;
            // Flush pre-roll buffer into main buffer
            buffer_.insert(buffer_.end(), pre_roll_buffer_.begin(), pre_roll_buffer_.end());
            pre_roll_buffer_.clear();
        }
        silence_ms_ = 0.0f;
        buffer_.insert(buffer_.end(), samples, samples + count);
    }
    else if (active_) // 如果之前是语音，现在是静音
    {
        silence_ms_ += (count / (float)SAMPLE_RATE) * 1000.0f;
        buffer_.insert(buffer_.end(), samples, samples + count);
    }
    else // 如果之前是静音，现在还是静音
    {
        // Fill pre-roll buffer
        for (size_t i = 0; i < count; ++i)
        {
            pre_roll_buffer_.push_back(samples[i]);
            if (pre_roll_buffer_.size() > (PRE_ROLL_MS * SAMPLE_RATE / 1000)) // 前摇缓存最多保留 200ms 的音频对应的 samples
            {
                pre_roll_buffer_.pop_front();
            }
        }
    }

    return active_;
}

bool VadSegmenter::should_flush() const
{
    return active_ && silence_ms_ >= SILENCE_LIMIT_MS;
}

std::vector<float> VadSegmenter::take_audio()
{
    active_ = false;
    silence_ms_ = 0.0f;
    pre_roll_buffer_.clear();
    return std::move(buffer_);
}