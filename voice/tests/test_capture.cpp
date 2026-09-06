#include <msime/voice/audio_capture.h>
#include <type_traits>
using metasequoia::voice::AudioCapture;
static_assert(!std::is_copy_constructible_v<AudioCapture>);
int main()
{
    AudioCapture first, second;
    first.stop();
    first.stop();
    second.stop();
    // Empty callback fails before opening any physical device.
    return first.start({}) || first.callback_failed() || second.callback_failed() ? 1 : 0;
}
