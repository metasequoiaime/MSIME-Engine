#include "silero_vad.h"
#include <onnxruntime/onnxruntime_cxx_api.h>
#include <memory>
#include <array>
#include <stdexcept>
#include <cstring>
#include <mutex>
#include <windows.h>
#include <fmt/xchar.h>

#include <condition_variable>
#include <thread>
#include <atomic>

struct SileroVad::Impl
{
    Ort::Env env;
    Ort::SessionOptions opts;
    Ort::Session session;
    Ort::AllocatorWithDefaultOptions allocator;
    Ort::MemoryInfo memory_info;

    // State tensor for the model (Size: 2 * 1 * 128 = 256)
    std::vector<float> state;
    std::vector<float> context; // 64 samples context
    int64_t sr_val = 16000;
    std::mutex mutex;
    std::condition_variable cv;
    std::atomic<bool> stop_flag{false};
    std::thread worker;

    Impl(const std::wstring &model_path, SileroVad *parent) : env(ORT_LOGGING_LEVEL_FATAL, "silero_vad"), opts(), session(nullptr), memory_info(Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeDefault))
    {
        try
        {
            opts.SetIntraOpNumThreads(1);
            opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_DISABLE_ALL);

            // printf("[VAD] Loading session from path...\n");
            // fflush(stdout);
            session = Ort::Session(env, model_path.c_str(), opts);
            // printf("[VAD] Session created successfully.\n");
            // fflush(stdout);

            state.assign(2 * 1 * 128, 0.0f);
            context.assign(64, 0.0f);

            worker = std::thread([this, parent]() {
                std::vector<float> frame(parent->cfg_.frame_size);
                while (!stop_flag)
                {
                    std::unique_lock<std::mutex> lock(this->mutex);
                    this->cv.wait(lock, [this, parent]() { return this->stop_flag || parent->input_buffer_.size() >= (size_t)parent->cfg_.frame_size; });

                    if (this->stop_flag)
                        break;

                    for (int i = 0; i < parent->cfg_.frame_size; ++i)
                    {
                        frame[i] = parent->input_buffer_.front();
                        parent->input_buffer_.pop_front();
                    }

                    // Release lock during expensive inference
                    lock.unlock();
                    parent->process_frame(frame.data());
                }
            });
        }
        catch (const std::exception &e)
        {
            // printf("[VAD] Error: %s\n", e.what());
            // fflush(stdout);
            throw;
        }
    }
};

SileroVad::SileroVad(const std::wstring &model_path, const Config &cfg) : cfg_(cfg)
{
    if (cfg_.sample_rate != 16000)
    {
        throw std::runtime_error("SileroVad: only 16kHz supported");
    }
    impl_ = std::make_unique<Impl>(model_path, this);
}

SileroVad::~SileroVad()
{
    if (impl_)
    {
        impl_->stop_flag = true;
        impl_->cv.notify_all();
        if (impl_->worker.joinable())
        {
            impl_->worker.join();
        }
    }
}

void SileroVad::push_audio(const float *samples, size_t count)
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    input_buffer_.insert(input_buffer_.end(), samples, samples + count);
    impl_->cv.notify_one();
}

bool SileroVad::has_segment() const
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return !ready_segments_.empty();
}

SpeechSegment SileroVad::pop_segment()
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    SpeechSegment seg = std::move(ready_segments_.front());
    ready_segments_.pop_front();
    return seg;
}

void SileroVad::reset_state()
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_)
    {
        impl_->state.assign(impl_->state.size(), 0.0f);
        impl_->context.assign(64, 0.0f);
    }
    pre_roll_buffer_.clear();
}

void SileroVad::process_frame(const float *frame_samples)
{
    try
    {
        // std::cerr << "[VAD] Processing frame..." << std::endl;
        float prob = infer_prob(frame_samples);

        if (prob >= cfg_.threshold)
        {
            speech_count_++;
            silence_count_ = 0;
        }
        else
        {
            silence_count_++;
            speech_count_ = 0;
        }

        if (state_ == State::Silence)
        {
            if (speech_count_ > 0)
            {
                if (current_segment_.empty())
                {
                    current_segment_.insert(current_segment_.end(), pre_roll_buffer_.begin(), pre_roll_buffer_.end());
                    pre_roll_buffer_.clear();
                }
                current_segment_.insert(current_segment_.end(), frame_samples, frame_samples + cfg_.frame_size);
            }
            else
            {
                current_segment_.clear();
                pre_roll_buffer_.insert(pre_roll_buffer_.end(), frame_samples, frame_samples + cfg_.frame_size);
                while (pre_roll_buffer_.size() > (size_t)cfg_.pre_roll_frames * cfg_.frame_size)
                {
                    pre_roll_buffer_.erase(pre_roll_buffer_.begin(), pre_roll_buffer_.begin() + cfg_.frame_size);
                }
            }

            if (speech_count_ >= cfg_.start_frames)
            {
                state_ = State::Speech;
                // printf("[VAD] Transition to SPEECH\n");
                // fflush(stdout);
            }
        }
        else if (state_ == State::Speech)
        {
            current_segment_.insert(current_segment_.end(), frame_samples, frame_samples + cfg_.frame_size);

            if (silence_count_ >= cfg_.end_frames)
            {
                state_ = State::Silence;
                // printf("[VAD] Transition to SILENCE\n");
                // fflush(stdout);
                speech_count_ = 0;
                silence_count_ = 0;

                if (!current_segment_.empty())
                {
                    size_t silence_samples = (size_t)cfg_.end_frames * cfg_.frame_size;
                    if (current_segment_.size() > silence_samples)
                    {
                        current_segment_.resize(current_segment_.size() - silence_samples);
                    }

                    SpeechSegment seg;
                    seg.samples = std::move(current_segment_);
                    seg.sample_rate = cfg_.sample_rate;
                    ready_segments_.push_back(std::move(seg));
                    // printf("[VAD] Finalized segment (%zu samples)\n", seg.samples.size());
                    // fflush(stdout);
                }
                current_segment_.clear();
                reset_state();
            }
        }
    }
    catch (const std::exception &e)
    {
        printf("[VAD] Exception in process_frame: %s\n", e.what());
        fflush(stdout);
    }
    catch (...)
    {
        printf("[VAD] Unknown exception in process_frame\n");
        fflush(stdout);
    }
}
// Inference Implementation
float SileroVad::infer_prob(const float *frame_samples)
{
    // No extra lock here, push_audio already holds impl_->mutex
    // diagnostics & DC removal
    // Diagnostics & DC Removal
    std::vector<float> processed_frame(cfg_.frame_size);
    double sum = 0.0;
    for (int i = 0; i < cfg_.frame_size; ++i)
        sum += frame_samples[i];
    float avg = (float)(sum / cfg_.frame_size);

    double sum_sq = 0.0;
    for (int i = 0; i < cfg_.frame_size; ++i)
    {
        processed_frame[i] = frame_samples[i] - avg;
        sum_sq += processed_frame[i] * processed_frame[i];
    }
    float rms = (float)std::sqrt(sum_sq / cfg_.frame_size);

    // Shapes from metadata
    // Effective window size = 512 (current) + 64 (context) = 576
    std::array<int64_t, 2> input_shape{1, 576};
    std::array<int64_t, 3> state_shape{2, 1, 128};
    std::array<int64_t, 1> sr_shape{1};
    int64_t sr_val = 16000;

    // Build new input: first 64 samples from context, then 512 from frame
    std::vector<float> input_data(576);
    std::memcpy(input_data.data(), impl_->context.data(), 64 * sizeof(float));
    std::memcpy(input_data.data() + 64, processed_frame.data(), cfg_.frame_size * sizeof(float));

    try
    {
        // Checkpoint 1: Tensor Creation
        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(impl_->memory_info, input_data.data(), input_data.size(), input_shape.data(), input_shape.size());
        Ort::Value state_tensor = Ort::Value::CreateTensor<float>(impl_->memory_info, impl_->state.data(), impl_->state.size(), state_shape.data(), state_shape.size());

        // Checkpoint 2: SR Tensor (1D {1})
        Ort::Value sr_tensor = Ort::Value::CreateTensor<int64_t>(impl_->memory_info, &sr_val, 1, sr_shape.data(), sr_shape.size());

        const char *input_names[] = {"input", "state", "sr"};
        const char *output_names[] = {"output", "stateN"};
        Ort::Value inputs[] = {std::move(input_tensor), std::move(state_tensor), std::move(sr_tensor)};

        // Checkpoint 3: Run
        // printf("[VAD] session.Run START\n"); fflush(stdout);
        auto outputs = impl_->session.Run(Ort::RunOptions{}, input_names, inputs, 3, output_names, 2);
        // printf("[VAD] session.Run END\n"); fflush(stdout);

        if (outputs.size() < 2)
            throw std::runtime_error("VAD: sess.Run returned < 2 outputs");

        float *prob_ptr = outputs[0].GetTensorMutableData<float>();
        float *new_state_ptr = outputs[1].GetTensorMutableData<float>();

        if (!prob_ptr || !new_state_ptr)
            throw std::runtime_error("VAD: failed to get tensor data pointers");

        // Check state change
        float diff = 0.0f;
        for (int i = 0; i < 256; ++i)
            diff += std::abs(impl_->state[i] - new_state_ptr[i]);

        // Update persistence
        std::memcpy(impl_->state.data(), new_state_ptr, 256 * sizeof(float));

        // Update context: copy the last 64 samples from input_data
        std::memcpy(impl_->context.data(), input_data.data() + 512, 64 * sizeof(float));

        float prob = prob_ptr[0];

        static int count = 0;
        if (count++ % 20 == 0 || prob > 0.01f || rms > 0.05f)
        {
            // printf("[VAD] RMS: %.4f | Prob: %.6f | StateD: %.8f\n", rms, prob, diff);
            // fflush(stdout);
        }

        return prob;
    }
    catch (const Ort::Exception &e)
    {
        printf("[VAD] ORT Error: %s\n", e.what());
        fflush(stdout);
        return 0.0f;
    }
    catch (const std::exception &e)
    {
        printf("[VAD] Error: %s\n", e.what());
        fflush(stdout);
        return 0.0f;
    }
}