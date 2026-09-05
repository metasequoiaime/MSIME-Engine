#include "whisper_worker.h"
#include "wav_writer.h"
#include "whisper.h"
#include <algorithm>
#include <thread>
#include <utility>
namespace metasequoia::voice {
WhisperWorker::WhisperWorker(const char* path, std::string language) : language_(std::move(language)) {
    if (!path || !*path) throw VoiceError("Whisper model path is required");
    ctx_ = whisper_init_from_file_with_params(path, whisper_context_default_params());
    if (!ctx_) throw VoiceError("Cannot load Whisper model");
}
WhisperWorker::~WhisperWorker() { if (ctx_) whisper_free(ctx_); }
std::string WhisperWorker::recognize(const std::vector<float>& pcm) {
    if (pcm.empty()) return {};
    (void)WavWriter::create_wav(pcm); // shared format/length validation
    std::lock_guard<std::mutex> lock(mutex_);
    auto params = whisper_full_default_params(WHISPER_SAMPLING_BEAM_SEARCH);
    params.n_threads = static_cast<int>(std::min(8u, std::max(1u, std::thread::hardware_concurrency())));
    params.language = language_.c_str();
    params.no_timestamps = true;
    params.print_progress = false;
    params.print_realtime = false;
    params.print_timestamps = false;
    params.single_segment = true;
    params.beam_search.beam_size = 5;
    if (whisper_full(ctx_, params, pcm.data(), static_cast<int>(pcm.size())) != 0)
        throw VoiceError("Whisper transcription failed");
    std::string result;
    for (int i = 0; i < whisper_full_n_segments(ctx_); ++i) result += whisper_full_get_segment_text(ctx_, i);
    return result;
}
}
