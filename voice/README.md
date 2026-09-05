# Metasequoia Voice Input(水杉记言)

English | [简体中文](README.zh-CN.md)

This is a voice input module for [MSIME-Windows](https://github.com/metasequoiaime/MSIME-Windows). However, it can be used as a standalone voice input tool for any application without other MetasequoiaIME components.

## How to run

Download release exe file from [releases](https://github.com/metasequoiaime/MetasequoiaVoiceInput/releases).

Then, copy all the contents of this project's `assets` folder to `$env:LOCALAPPDATA\MetasequoiaVoiceInput\`. And replace your siliconflow token in `config.toml`.

Then, run `MetasequoiaVoiceInput.exe`.

## Usage

- **Hotkeys**:
  - RAlt pressed to start recording, release to stop recording and send text to active application
  - RAlt + Space: Lock recording
  - Ctrl + F9: Toggle recording

## Configuration

Edit `$env:LOCALAPPDATA\MetasequoiaVoiceInput\config.toml` (create if not exists) to configure the application.

e.g. in my system, the path is:

```
C:\Users\sonnycalcr\AppData\Local\MetasequoiaVoiceInput\config.toml
```

Below is a template:

```toml
# 自动语音识别（ASR）配置
[asr_api]
# API 基础地址
endpoint = "https://api.siliconflow.cn/v1/audio/transcriptions"
# 服务提供商（如：azure、openai、deepgram 等）
provider = "siliconflow"
# API 访问令牌
token = "<YOUR_OWN_TOKE>"

# 文本润色配置
[polish_api]
# API 基础地址
endpoint = "https://api.siliconflow.cn/v1/chat/completions"
# 服务提供商（如：azure、openai、deepgram 等）
provider = "siliconflow"
# API 访问令牌
token = "<YOUR_OWN_TOKE>"

# 基础设置
[settings]
# 偏好语言
language = "zh-cn"
# 触发时是否播放提示音
notification_sound = true
# 上屏前是否要先进行文本润色
polish_text = false
# 可以选择的值有：local_whisper, cloud_siliconflow
stt_provider = "cloud_siliconflow"
```

You can also change these config in settings window:

![](https://i.imgur.com/Q3Jct2Z.png)

![](https://i.imgur.com/9j2IV9X.png)

![](https://i.imgur.com/1F47neV.png)

## Notice

- Only implemented dark-mode UI now

## License

GPL-3.0.
