#pragma once
#include <atomic>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
namespace metasequoia::voice {
inline constexpr int sample_rate = 16000;
inline constexpr std::size_t maximum_samples = sample_rate * 60;
// PCM is mono 16 kHz finite float data in [-1,1]. Methods are synchronous;
// hosts call recognize/polish on a worker queue and commit results on their UI thread.
class VoiceError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};
struct RequestOptions {
    std::string endpoint;
    std::string model;
    std::string token;
    long timeout_ms = 10000;
    std::shared_ptr<std::atomic_bool> cancelled;
};
class SttService {
public:
    virtual ~SttService() = default;
    virtual std::string recognize(const std::vector<float>& pcm) = 0;
};
} // namespace metasequoia::voice
