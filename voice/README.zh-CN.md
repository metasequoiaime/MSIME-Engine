# Metasequoia Voice Input（水杉记言）

[English](README.md) | 简体中文

水杉记言是一款语音输入模块，最初为 [MSIME-Windows](https://github.com/metasequoiaime/MSIME-Windows) 设计，但也可以**独立运行**，作为通用语音输入工具用于任何应用程序，而无需安装其他输入法组件。

---

## 运行方法

1. 从 GitHub 的 Releases 页面下载最新版本：

[https://github.com/metasequoiaime/MetasequoiaVoiceInput/releases](https://github.com/metasequoiaime/MetasequoiaVoiceInput/releases)

2. 将本项目 `assets` 文件夹中的**所有内容**复制到：

```
$env:LOCALAPPDATA\MetasequoiaVoiceInput\
```

3. 打开 `config.toml` 文件，填入你自己的 SiliconFlow API Token。

4. 运行：

```
MetasequoiaVoiceInput.exe
```

---

## 使用方法

### 快捷键

- **RAlt 按下开始录音，松开停止录音并将识别结果发送到当前活动应用**
- **RAlt + Space：锁定录音（无需持续按住）**
- **Ctrl + F9：切换录音状态**

支持在任意输入场景下快速语音转文字。

---

## 配置说明

编辑：

```
$env:LOCALAPPDATA\MetasequoiaVoiceInput\config.toml
```

e.g. 在我的系统上，路径为：

```
C:\Users\sonnycalcr\AppData\Local\MetasequoiaVoiceInput\config.toml
```

（如果不存在请手动创建）

下面是一个完整配置示例：

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
# 上屏前是否进行文本润色
polish_text = false
# 可选值：local_whisper, cloud_siliconflow
stt_provider = "cloud_siliconflow"
```

---

## 图形界面设置

除了手动修改 `config.toml`，你也可以通过设置窗口修改配置：

![](https://i.imgur.com/Q3Jct2Z.png)

![](https://i.imgur.com/9j2IV9X.png)

![](https://i.imgur.com/1F47neV.png)

---

## 特性

- 支持本地 Whisper 模型
- 支持 SiliconFlow 云端识别
- 支持文本润色（LLM 优化表达）
- 支持锁定录音模式
- 支持提示音反馈
- 可独立运行，无需输入法环境

## 注意事项

- UI 目前只适配了暗色模式

---

## 许可证

本项目采用 **GPL-3.0 License** 开源协议。
