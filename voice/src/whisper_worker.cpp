#include <thread>
#include "whisper_worker.h"
#include "whisper.h"

WhisperWorker::WhisperWorker(const char *model_path)
{
    ctx_ = whisper_init_from_file(model_path);
}

WhisperWorker::~WhisperWorker()
{
    whisper_free(ctx_);
}

std::string WhisperWorker::recognize(const std::vector<float> &pcm)
{
    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_BEAM_SEARCH);

    params.n_threads = std::thread::hardware_concurrency();
    params.language = "zh";
    params.no_timestamps = true;
    params.single_segment = true;
    params.beam_search.beam_size = 5;

    whisper_full(ctx_, params, pcm.data(), pcm.size());

    std::string result;
    int n = whisper_full_n_segments(ctx_);
    for (int i = 0; i < n; ++i)
        result += whisper_full_get_segment_text(ctx_, i);

    return result;
}