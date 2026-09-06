# Voice models

Both models in this directory are executed inside the IME process, so each one is pinned by digest and the download path refuses anything that does not match.

## silero_vad.onnx

The voice activity detector, committed to this repository rather than downloaded.

| | |
| --- | --- |
| Upstream | [snakers4/silero-vad](https://github.com/snakers4/silero-vad), `src/silero_vad/data/silero_vad.onnx` |
| Version | v6.2 (identical in v6.2.1); introduced by upstream commit `bfdc0193023f` |
| License | MIT |
| Size | 2327524 bytes |
| SHA256 | `1a153a22f4509e292a94e67d6f9b85e8deb25b4988682b7e174c65279d8788e3` |

`voice.yml` re-checks that digest on every run, so replacing the file without updating this table fails CI.

## How to download whisper ggml models

Run command in terminal:

```powershell
.\download-ggml-model.cmd <model_name>
```

e.g.

```powershell
.\download-ggml-model.cmd medium
```

| Model               | Disk    |
| ------------------- | ------- |
| tiny                | 75 MiB  |
| tiny.en             | 75 MiB  |
| base                | 142 MiB |
| base.en             | 142 MiB |
| small               | 466 MiB |
| small.en            | 466 MiB |
| small.en-tdrz       | 465 MiB |
| medium              | 1.5 GiB |
| medium.en           | 1.5 GiB |
| large-v1            | 2.9 GiB |
| large-v2            | 2.9 GiB |
| large-v2-q5_0       | 1.1 GiB |
| large-v3            | 2.9 GiB |
| large-v3-q5_0       | 1.1 GiB |
| large-v3-turbo      | 1.5 GiB |
| large-v3-turbo-q5_0 | 547 MiB |

This script is copied from <https://github.com/ggml-org/whisper.cpp/blob/master/models/download-ggml-model.cmd>, with two changes: the download is pinned to a fixed Hugging Face revision instead of `main`, and the result is checked against `ggml-models.sha256` and deleted on mismatch.

To move to newer weights, update the two revisions at the top of `:download_model` and regenerate `ggml-models.sha256` from the Hugging Face LFS metadata of those revisions.
