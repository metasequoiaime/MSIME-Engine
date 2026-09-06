# 水杉公共语音模块

VoiceInput 已并入 MSIME-Engine。公共 API、构建选项与平台接入见 [公共模块说明](README.md)。公共处理、录音与本地 Whisper 分别由 `MetasequoiaIme::Voice`、`MetasequoiaIme::VoiceCapture`、`MetasequoiaIme::VoiceWhisper` 提供，平台按需链接。

- Windows 输入法的语音宿主位于 [MSIME-Windows/server](https://github.com/metasequoiaime/MSIME-Windows/tree/develop/server)。
- macOS 输入法已有实际接入，使用方法见 [macOS 语音指南](https://github.com/metasequoiaime/MSIME-Docs/blob/main/guides/macos-voice.md)。iOS 键盘尚无语音入口。
- 原独立 Windows 工具保留在 `platforms/windows/`；当前构建命令、配置和快捷键见 [独立工具说明](platforms/windows/README.md)。它使用云识别，公共库的可选 Whisper 能力不会因旧配置中的 `local_whisper` 值自动启用。

当前流式识别仍是 Windows 产品能力，由 `MSIME-Windows/server` 实现。公共
`MetasequoiaIme::Voice` API 提供有界的一次性 HTTP 识别，不提供流式传输或增量文本提交；
macOS 和 Linux 不应假设共享库具备 Windows 的增量转写行为。将流式识别扩展为共享能力需要
跨仓 API 和前端输入状态改造，不是简单替换 Provider。

所有构建命令从 **MSIME-Engine 仓库根目录**执行。独立工具资源在 `voice/assets/`，用户配置仍位于 `%LOCALAPPDATA%\MetasequoiaVoiceInput\config.toml`。保留这个路径可继续使用已有配置。

旧环境生成器会覆盖公共 CMake 工程，现已移除；不再运行旧 `prepare_env.py` 或旧 build preset。历史脚本和提交仍保存在 Git 历史中。[旧仓库 Releases](https://github.com/metasequoiaime/MetasequoiaVoiceInput/releases) 保留已发布版本。

原项目 GPL-3.0 许可证与第三方声明继续保留，合仓不改变它们的授权。
