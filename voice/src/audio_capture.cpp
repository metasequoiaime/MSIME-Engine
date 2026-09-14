#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "audio_capture.h"
#include "capture_device_id.h"
#include <atomic>
#include <utility>
namespace metasequoia::voice
{
struct AudioCapture::Impl
{
    ma_device device{};
    ma_context context{};
    ma_device_id selected{};
    bool context_initialized = false;
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
    return start(std::move(callback), {});
}
bool AudioCapture::start(AudioCallback callback, const std::string &device_id)
{
    stop();
    if (!callback)
        return false;
    if (!device_id.empty())
    {
        const auto backend = detail::capture_backend(device_id);
        if (!backend || ma_context_init(&*backend, 1, nullptr, &impl_->context) != MA_SUCCESS)
            return false;
        impl_->context_initialized = true;
        ma_device_info *capture = nullptr;
        ma_uint32 count = 0;
        if (ma_context_get_devices(&impl_->context, nullptr, nullptr, &capture, &count) != MA_SUCCESS)
        {
            stop();
            return false;
        }
        const auto *selected = detail::select_capture_device(impl_->context.backend, capture, count, device_id);
        if (!selected)
        {
            stop();
            return false;
        }
        impl_->selected = selected->id;
    }
    impl_->callback = std::move(callback);
    impl_->failed = false;
    auto config = ma_device_config_init(ma_device_type_capture);
    config.capture.format = ma_format_f32;
    config.capture.channels = 1;
    config.capture.pDeviceID = impl_->context_initialized ? &impl_->selected : nullptr;
    config.sampleRate = 16000;
    config.dataCallback = Impl::receive;
    config.pUserData = impl_.get();
    if (ma_device_init(impl_->context_initialized ? &impl_->context : nullptr, &config, &impl_->device) != MA_SUCCESS)
    {
        stop();
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
    if (impl_->context_initialized)
    {
        ma_context_uninit(&impl_->context);
        impl_->context_initialized = false;
    }
}
std::vector<CaptureDevice> AudioCapture::devices()
{
    struct Context
    {
        ma_context value{};
        bool initialized = false;
        ~Context()
        {
            if (initialized)
                ma_context_uninit(&value);
        }
    } context;
    std::vector<CaptureDevice> result;
    if (ma_context_init(nullptr, 0, nullptr, &context.value) != MA_SUCCESS)
        return result;
    context.initialized = true;
    ma_device_info *capture = nullptr;
    ma_uint32 count = 0;
    if (ma_context_get_devices(&context.value, nullptr, nullptr, &capture, &count) != MA_SUCCESS)
        return result;
    for (ma_uint32 index = 0; index < count && result.size() < 128; ++index)
    {
        auto id = detail::capture_device_id(context.value.backend, capture[index].id);
        if (!id.empty() && capture[index].name[0])
            result.push_back({std::move(id), capture[index].name});
    }
    return result;
}
bool AudioCapture::callback_failed() const
{
    return impl_->failed.load();
}
} // namespace metasequoia::voice
