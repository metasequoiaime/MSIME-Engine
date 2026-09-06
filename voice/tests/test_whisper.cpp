#include <msime/voice/whisper_worker.h>
#include <type_traits>
using namespace metasequoia::voice;
static_assert(!std::is_copy_constructible_v<WhisperWorker>);
int main()
{
    try
    {
        WhisperWorker worker("msime-test-model-that-does-not-exist.bin");
    }
    catch (const VoiceError &)
    {
        return 0;
    }
    return 1;
}
