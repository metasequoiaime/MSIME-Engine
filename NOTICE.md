# 来源与许可

本次合仓保留原始来源与许可声明；根 `LICENSE` 不覆盖或重新授权导入的第三方词库数据。

- 输入引擎：根 `LICENSE`；googlepinyinime-rev、utfcpp 各自保留上游许可。
- 词库及构建脚本：[dictionary/NOTICE.md](dictionary/NOTICE.md)，脚本许可见 `dictionary/makecikudb/LICENSE`。
- 自定义词库：`dictionary/custom/` 保留原仓文件与来源，迁移不新增统一许可声明。
- 辅助码：[helpcode/NOTICE.md](helpcode/NOTICE.md)。
- 公共语音：`voice/LICENSE`；miniaudio 和 whisper.cpp 保留各自子模块许可证。语音模型的来源、版本、许可与 SHA256 见 [voice/assets/models/README.md](voice/assets/models/README.md)：`silero_vad.onnx` 取自 snakers4/silero-vad v6.2（MIT），whisper ggml 权重按 `ggml-models.sha256` 里钉死的 Hugging Face revision 下载。
- 日本语模型发布时必须附带 `mozc_dictionary_oss_README.txt`。

具体导入提交见 [consolidation-sources.json](docs/consolidation-sources.json)。
