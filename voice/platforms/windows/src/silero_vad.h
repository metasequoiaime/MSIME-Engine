#pragma once

#include <vector>
#include <deque>
#include <string>
#include <cstddef>

#include <memory>

struct SpeechSegment
{
    std::vector<float> samples;
    int sample_rate = 16000;
};

class SileroVad
{
  public:
    struct Config
    {
        int sample_rate = 16000;
        int frame_size = 512;     // samples per frame (512 @ 16kHz = 32ms) - Required by Silero VAD v4/v5
        float threshold = 0.5f;   // speech probability threshold
        int start_frames = 2;     // consecutive speech frames to start
        int end_frames = 10;      // consecutive silence frames to end
        int pre_roll_frames = 10; // frames to keep before speech starts (~300ms)

        Config()
        {
        }
    };

    explicit SileroVad(const std::wstring &model_path, const Config &cfg = Config());
    ~SileroVad();

    // Prevent copying due to unique_ptr
    SileroVad(const SileroVad &) = delete;
    SileroVad &operator=(const SileroVad &) = delete;

    // reset LSTM state
    void reset_state();

    // push raw audio (16kHz mono float [-1, 1])
    void push_audio(const float *samples, size_t count);

    // whether a completed speech segment is available
    bool has_segment() const;

    // get one completed speech segment
    SpeechSegment pop_segment();

  private:
    void process_frame(const float *frame_samples);
    float infer_prob(const float *frame_samples);

  private:
    Config cfg_;

    // ONNX Runtime
    struct Impl;
    std::unique_ptr<Impl> impl_;

    // buffering
    std::deque<float> input_buffer_;
    std::deque<float> pre_roll_buffer_;
    std::vector<float> current_segment_;
    std::deque<SpeechSegment> ready_segments_;

    // state machine
    enum class State
    {
        Silence,
        Speech
    };
    State state_ = State::Silence;
    int speech_count_ = 0;
    int silence_count_ = 0;
};
