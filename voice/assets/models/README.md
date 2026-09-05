# How to download whisper ggml models

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

This script is copied from <https://github.com/ggml-org/whisper.cpp/blob/master/models/download-ggml-model.cmd>.
