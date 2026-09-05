# Shared VoiceInput

The user has moved VoiceInput into the public Engine repository. C++17 reusable code is in `src/` with public headers in `include/msime/voice/`. Windows-only host code is in `platforms/windows/`; it must not enter the public targets. Platform hosts own microphone permission, hotkeys, UI, token storage, threading, and committing text.

Build `cmake -S voice -B build-voice`, then build and run CTest. CI covers Windows, Linux, macOS. Capture and Whisper are optional, and core Engine builds must not acquire voice dependencies. Public code uses `metasequoia::voice`, UTF-8 text and mono 16 kHz finite float PCM. Never open a microphone, send user audio or contact paid providers in automated tests; use synthetic samples and loopback HTTP fixtures.

Preserve imported history and licenses. Network code must release handles on success and failure, bound responses, check status, support cancellation and avoid logging response bodies or tokens. Audio device state is per instance. Start/stop are serialized by the host; callbacks must never call stop or destroy the capture object.
