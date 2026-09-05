# Shared VoiceInput（水杉公共语音模块）

VoiceInput is now part of MSIME-Engine. Windows, macOS and Linux hosts can link the same C++17 library without linking the keyboard engine or copying recognition code. Apple callers use Objective-C++; a complete macOS host is in `examples/macos/`.

| Target | Responsibility | Dependencies |
|---|---|---|
| `MetasequoiaIme::Voice` | Mono PCM/WAV, RMS voice segmentation, configurable HTTP transcription and optional text cleanup | libcurl, nlohmann-json |
| `MetasequoiaIme::VoiceCapture` | Per-instance microphone capture, converted to mono 16 kHz float PCM | pinned miniaudio |
| `MetasequoiaIme::VoiceWhisper` | Optional local recognition | pinned whisper.cpp; model supplied by host |

UI, microphone permissions, hotkeys, token storage and committing text belong to platform hosts. The library returns UTF-8 text and does not simulate keys. Root Engine builds have `METASEQUOIA_IME_BUILD_VOICE=OFF` by default. For a root vcpkg build with voice enabled, add `VCPKG_MANIFEST_FEATURES=voice`. Voice can also be built independently, so a host that only needs speech does not acquire Boost, SQLite or dictionary dependencies.

## Build

From the Engine root:

```sh
git submodule update --init voice/third_party/miniaudio
# macOS
brew install nlohmann-json
# Ubuntu: sudo apt-get install cmake libcurl4-openssl-dev nlohmann-json3-dev
cmake -S voice -B build-voice -DCMAKE_BUILD_TYPE=Release
cmake --build build-voice --parallel
ctest --test-dir build-voice --output-on-failure
```

Windows uses the manifest in `voice/vcpkg.json`. Select the same MSVC runtime as the dependency triplet; the static triplet below requires `/MT` (and `/MTd` for Debug). When embedding Voice in a root/host build, set this at the host level so all linked C++ targets agree:

```powershell
cmake -S voice -B build-voice -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_INSTALLATION_ROOT/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows-static '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>'
cmake --build build-voice --config Release --parallel
ctest --test-dir build-voice -C Release --output-on-failure
```

Set `MSIME_VOICE_CAPTURE=OFF` if the host already supplies PCM. For local Whisper, initialize `voice/third_party/whisper.cpp` and set `MSIME_VOICE_WHISPER=ON`. Models are not downloaded by the library. Imported Silero/ONNX code was not built by the old application either; it remains historical Windows-host code, outside the supported public targets. The supported VAD here is RMS-based.

## Call from a host

```cmake
# After adding Engine with METASEQUOIA_IME_BUILD_VOICE=ON, or independently:
add_subdirectory(path/to/MSIME-Engine/voice voice)
target_link_libraries(MyHost PRIVATE MetasequoiaIme::Voice MetasequoiaIme::VoiceCapture)
```

```cpp
#include <msime/voice/cloud_stt_worker.h>
using namespace metasequoia::voice;
RequestOptions options{endpoint, model, token, 10000, cancellationFlag};
CloudSttWorker recognizer(options);
std::string text = recognizer.recognize(pcm); // worker queue, mono 16 kHz floats
// Host dispatches text to its UI/input-method thread and commits it.
```

`CloudSttWorker` supports multipart `file` + `model` with JSON `text`, `transcription` or `result.text` responses. This is a protocol adapter, not a claim that every provider supports that protocol. Endpoints/models are host configuration; the token-only constructor retains the old SiliconFlow defaults for the imported Windows host. WebSocket/Doubao and Server-specific providers have not been migrated by this change.

`provider_protocol.h` exposes the same multipart and JSON codecs to hosts with an existing HTTP transport. `make_transcription_request` takes an encoded WAV (up to 20 MiB), preserves binary bytes and accepts an optional language field; omit language for services that reject it. `make_polish_request` sends the supplied user message verbatim, so hosts retain their prompt/delimiter policy. Response parsers reject malformed, missing, empty or oversized text responses with `VoiceError`. Hosts retain endpoint validation, credentials, timeouts, cancellation, status checks and stricter upload/response limits. The PCM recognizer still enforces the 60-second contract below.

Each request has a timeout and optional shared atomic cancellation flag. Set that flag to abort an in-flight request; a cancelled flag stays cancelled until the host supplies a new one. Recognition errors throw `VoiceError`. `TextPolisher` returns the original text on failure, timeout, cancellation or an empty result. It never logs tokens, audio or response bodies. Redirects are rejected, HTTP status is checked and responses are capped at 1 MiB.

Input PCM must be finite mono 16 kHz float samples, at most 60 seconds per request. WAV encoding clips samples to [-1,1]. `WavWriter::create_wav` accepts an explicit sample limit for hosts with a different bounded upload duration, such as Windows clips with provider padding; omitting it retains the 60-second limit. RIFF size overflow is always rejected. Hosts serialize capture start/stop/destruction, keep callback work short, and never stop or destroy capture from its callback. `stop()` is idempotent and waits for callbacks. A callback exception is contained and reported by `callback_failed()`. VAD callers drain `take_audio()` when appropriate; oversized unconsumed blocks throw rather than growing without limit.

## Apple

Build a standalone macOS example that links the public targets directly:

```sh
cmake -S voice -B build-voice -DMSIME_VOICE_APPLE_EXAMPLE=ON
cmake --build build-voice --parallel
# Provide endpoint, model and token through your local environment, then launch:
build-voice/examples/macos/MSIMEVoiceExample.app/Contents/MacOS/MSIMEVoiceExample
```

The example reads `MSIME_ASR_ENDPOINT`, `MSIME_ASR_MODEL` and `MSIME_ASR_TOKEN`. It requests microphone permission only when Start is pressed, captures with `VoiceCapture`, calls the shared recognizer on a worker queue, displays the result on the main thread, and cancels work when the window closes. CI builds it without launching it or accessing a microphone. This validates Apple linkage; it does not automatically add a voice button to the MSIME-Apple input method.

A product host must declare `NSMicrophoneUsageDescription` and request audio access, as shown in [Apple's capture authorization documentation](https://developer.apple.com/documentation/bundleresources/requesting-authorization-for-media-capture-on-macos). Hardened/sandboxed products also configure their audio-input entitlements. iOS may reuse the processing API through its containing app, but a custom keyboard extension cannot directly record audio; see [Apple's custom keyboard restrictions](https://developer.apple.com/library/archive/documentation/General/Conceptual/ExtensibilityPG/CustomKeyboard.html). This change verifies macOS; it does not implement an iOS host/extension handoff.

## Imported Windows host and migration

The original standalone UI and hotkeys remain under `platforms/windows/`. Build it with `MSIME_VOICE_WINDOWS_APP=ON` and `VCPKG_MANIFEST_FEATURES=windows-app`. It links the shared targets. See [the imported usage guide](platforms/windows/README.md) for the asset/config layout. Historical developer scripts are retained as references and are not the supported build entry point.

MSIME-Server still has its own evolved voice service; this change does not replace or downgrade it. After this producer change merges, platform consumer changes can pin the merged Engine commit and move their adapters to the public API. Existing standalone releases remain available in the old repository.

Original VoiceInput history and GPL-3.0 `LICENSE` are preserved; third-party libraries retain their own licenses. Import source: `413f734e1d4694748d3cf88b8df95f37528e8a97` in `metasequoiaime/MetasequoiaVoiceInput`.
