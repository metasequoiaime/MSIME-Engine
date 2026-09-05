#pragma once
#include <cstddef>
#include <functional>
#include <memory>
namespace metasequoia::voice {
class AudioCapture {
public:
    using AudioCallback = std::function<void(const float*, std::size_t)>;
    AudioCapture();
    ~AudioCapture();
    AudioCapture(const AudioCapture&) = delete;
    AudioCapture& operator=(const AudioCapture&) = delete;
    // Host grants microphone permission first, and serializes start/stop/destruction.
    // Callback runs on the capture thread; never call stop or destroy from it.
    bool start(AudioCallback callback);
    void stop(); // idempotent; waits for the device callback before releasing it
    bool callback_failed() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
