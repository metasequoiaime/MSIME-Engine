#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "audio_capture.h"
#include <atomic>
#include <utility>
namespace metasequoia::voice
{
struct AudioCapture::Impl
{
    ma_device device{};
    bool initialized = false;
    AudioCallback callback;
    std::atomic_bool failed{false};
    static void receive(ma_device *device, void *, const void *input, ma_uint32 frames) noexcept
    {
        auto *self = static_cast<Impl *>(device->pUserData);
        if (!input || self->failed.load())
            return;
        try
        {
            if (self->callback)
                self->callback(static_cast<const float *>(input), frames);
        }
        catch (...)
        {
            self->failed = true;
        }
    }
};
AudioCapture::AudioCapture() : impl_(std::make_unique<Impl>())
{
}
AudioCapture::~AudioCapture()
{
    stop();
}
bool AudioCapture::start(AudioCallback callback)
{
    stop();
    if (!callback)
        return false;
    impl_->callback = std::move(callback);
    impl_->failed = false;
    auto config = ma_device_config_init(ma_device_type_capture);
    config.capture.format = ma_format_f32;
    config.capture.channels = 1;
    config.sampleRate = 16000;
    config.dataCallback = Impl::receive;
    config.pUserData = impl_.get();
    if (ma_device_init(nullptr, &config, &impl_->device) != MA_SUCCESS)
    {
        impl_->callback = {};
        return false;
    }
    impl_->initialized = true;
    if (ma_device_start(&impl_->device) != MA_SUCCESS)
    {
        stop();
        return false;
    }
    return true;
}
void AudioCapture::stop()
{
    if (impl_->initialized)
    {
        ma_device_uninit(&impl_->device);
        impl_->initialized = false;
    }
    impl_->callback = {};
}
bool AudioCapture::callback_failed() const
{
    return impl_->failed.load();
}
} // namespace metasequoia::voice
