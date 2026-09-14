#include "../src/capture_device_id.h"
#include <msime/voice/audio_capture.h>
#include <cassert>
#include <cstring>

int main()
{
    using namespace metasequoia::voice;
    using namespace metasequoia::voice::detail;
    ma_device_info devices[2]{};
    devices[0].id.wasapi[0] = 'a';
    devices[1].id.wasapi[0] = 'b';
    std::strcpy(devices[0].name, "Synthetic microphone");
    std::strcpy(devices[1].name, "Synthetic microphone");
    const auto id = capture_device_id(ma_backend_wasapi, devices[0].id);
    assert(id == "wasapi:0061");
    assert(capture_backend(id) == ma_backend_wasapi);
    assert(select_capture_device(ma_backend_wasapi, devices, 2, id) == &devices[0]);
    std::swap(devices[0], devices[1]);
    assert(select_capture_device(ma_backend_wasapi, devices, 2, id) == &devices[1]);
    assert(!select_capture_device(ma_backend_wasapi, devices, 1, id));
    assert(!select_capture_device(ma_backend_wasapi, devices, 2, ""));
    assert(!select_capture_device(ma_backend_wasapi, devices, 2, "0"));
    devices[0].id = devices[1].id;
    assert(!select_capture_device(ma_backend_wasapi, devices, 2, id));
    ma_device_id other{};
    std::strcpy(other.coreaudio, "fixture:input");
    const auto apple = capture_device_id(ma_backend_coreaudio, other);
    assert(capture_backend(apple) == ma_backend_coreaudio);
    other = {};
    std::strcpy(other.pulse, "fixture:input");
    assert(capture_backend(capture_device_id(ma_backend_pulseaudio, other)) == ma_backend_pulseaudio);
    other = {};
    std::strcpy(other.alsa, "fixture:input");
    assert(capture_backend(capture_device_id(ma_backend_alsa, other)) == ma_backend_alsa);
    assert(capture_device_id(ma_backend_null, other).empty());
    other = {};
    other.wasapi[0] = 0x4e00;
    assert(capture_device_id(ma_backend_wasapi, other) == "wasapi:4e00");
    for (auto &unit : other.wasapi)
        unit = 'x';
    assert(capture_device_id(ma_backend_wasapi, other).empty());
    for (const auto *invalid : {"0", "wasapi:", "wasapi:001", "wasapi:gggg", "unknown:00"})
    {
        assert(!capture_backend(invalid));
        AudioCapture capture;
        // Invalid syntax is rejected before context/device initialization.
        assert(!capture.start([](const float *, size_t) {}, invalid));
    }
    assert(!capture_backend("wasapi:" + std::string(256, '1')));
}
