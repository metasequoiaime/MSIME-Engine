#pragma once
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>
namespace metasequoia::voice
{
struct CaptureDevice
{
    std::string id;
    std::string label;
};
class AudioCapture
{
  public:
    using AudioCallback = std::function<void(const float *, std::size_t)>;
    AudioCapture();
    ~AudioCapture();
    AudioCapture(const AudioCapture &) = delete;
    AudioCapture &operator=(const AudioCapture &) = delete;
    // Host grants microphone permission first, and serializes start/stop/destruction.
    // Callback runs on the capture thread; never call stop or destroy from it.
    bool start(AudioCallback callback);
    // Empty id preserves the default-device behavior. Nonempty ids must come
    // from devices(); missing/unsupported devices fail without default fallback.
    bool start(AudioCallback callback, const std::string &device_id);
    // Enumerates without starting capture. IDs are backend-qualified endpoint
    // identities, not display names or list positions. Currently supports
    // WASAPI, CoreAudio, PulseAudio and ALSA; other backends return an empty list.
    // Do not log identifiers or labels: they may identify a user's hardware.
    static std::vector<CaptureDevice> devices();
    void stop(); // idempotent; waits for the device callback before releasing it
    bool callback_failed() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace metasequoia::voice
