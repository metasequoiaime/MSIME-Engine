#include <msime/voice/cloud_stt_worker.h>
#include <msime/voice/text_polisher.h>
#include <msime/voice/vad.h>
#include <msime/voice/wav_writer.h>
#include <cmath>
#include <future>
#include <iostream>
#include <limits>
#include <thread>
using namespace metasequoia::voice;
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
template<class F> void fails(F action) {
    bool failed = false;
    try { action(); } catch (const VoiceError&) { failed = true; }
    require(failed, "Expected VoiceError");
}
int main(int argc, char** argv) {
    try {
        if (argc == 1) {
            const auto wav = WavWriter::create_wav({0, 1, -1, 2});
            require(wav.size() == 52 && wav[0] == 'R' && wav[24] == 0x80 && wav[25] == 0x3e, "WAV header");
            require(wav[46] == 0xff && wav[47] == 0x7f && wav[48] == 1 && wav[49] == 0x80, "little-endian PCM");
            fails([] { WavWriter::create_wav({std::numeric_limits<float>::quiet_NaN()}); });
            fails([] { WavWriter::create_wav({0}, 48000); });
            fails([] { WavWriter::create_wav(std::vector<float>(maximum_samples + 1)); });
            VadSegmenter vad;
            require(!vad.process(nullptr, 0), "Empty VAD block");
            std::vector<float> silence(3200), speech(1600, 0.1f), tail(8000);
            vad.process(silence.data(), silence.size());
            vad.process(speech.data(), speech.size());
            vad.process(tail.data(), tail.size());
            require(vad.should_flush(), "Speech ending");
            require(vad.take_audio().size() == 12800, "Pre-roll and silence retained");
            require(vad.take_audio().empty() && !vad.should_flush(), "VAD reset");
            vad.process(speech.data(), speech.size());
            require(vad.take_audio().size() == speech.size(), "Second utterance isolated");
            return 0;
        }
        const std::string base = argv[1];
        auto options = [&](const std::string& path) { return RequestOptions{base + path, "fixture-model", "fixture-token", 3000, {}}; };
        const std::vector<float> pcm(160, 0.125f);
        std::cerr << "HTTP: transcription and error cases\n";
        CloudSttWorker asr(options("/asr"));
        require(asr.recognize(pcm) == "水杉 voice", "ASR UTF-8");
        require(asr.recognize({}).empty(), "Empty audio needs no network");
        for (const auto* path : {"/denied", "/invalid", "/missing", "/oversize", "/redirect"}) {
            CloudSttWorker invalid(options(path));
            fails([&] { invalid.recognize(pcm); });
        }
        std::cerr << "HTTP: timeout and cancellation\n";
        auto timeout = options("/slow"); timeout.timeout_ms = 50;
        CloudSttWorker slow(timeout); fails([&] { slow.recognize(pcm); });
        auto cancelled = options("/asr"); cancelled.cancelled = std::make_shared<std::atomic_bool>(true);
        CloudSttWorker stopped(cancelled); fails([&] { stopped.recognize(pcm); });
        auto during = options("/slow"); during.cancelled = std::make_shared<std::atomic_bool>(false);
        CloudSttWorker cancelling(during);
        auto pending = std::async(std::launch::async, [&] { fails([&] { cancelling.recognize(pcm); }); });
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); during.cancelled->store(true); pending.get();
        std::cerr << "HTTP: polish and fallback\n";
        TextPolisher polish(options("/polish"), "Clean transcription only");
        require(polish.polish("嗯 水杉") == "水杉", "Polished text");
        for (const auto* path : {"/denied", "/invalid", "/missing"}) {
            TextPolisher fallback(options(path), "prompt");
            require(fallback.polish("原始文本") == "原始文本", "Preserve original on error");
        }
        TextPolisher fallback(timeout, "prompt");
        require(fallback.polish("原始文本") == "原始文本", "Preserve original on timeout");
        // An independently destroyed provider must not tear down another provider's CURL runtime.
        std::cerr << "HTTP: concurrent provider lifetimes\n";
        std::vector<std::future<void>> workers;
        for (int i = 0; i < 4; ++i) workers.push_back(std::async(std::launch::async, [&] {
            for (int n = 0; n < 5; ++n) {
                CloudSttWorker temporary(options("/asr"));
                require(temporary.recognize(pcm) == "水杉 voice", "Independent provider lifetime");
            }
        }));
        for (auto& worker : workers) worker.get();
        require(asr.recognize(pcm) == "水杉 voice", "Original provider still works");
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
