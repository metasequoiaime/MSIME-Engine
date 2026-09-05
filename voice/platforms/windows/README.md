# Metasequoia Voice Input(水杉记言)

English | [简体中文](../../README.zh-CN.md)

This standalone Windows host is maintained in MSIME-Engine's `voice/platforms/windows/` and links the public voice targets. The Windows input method's own host is maintained in [MSIME-Windows/server](https://github.com/metasequoiaime/MSIME-Windows/tree/main/server).

## Build and run

Run from the **MSIME-Engine repository root** with Visual Studio's C++ tools, CMake and vcpkg installed. Set `VCPKG_ROOT` to your vcpkg checkout first:

```powershell
git submodule update --init voice/third_party/miniaudio
cmake -S voice -B build-voice -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows-static '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>' -DVCPKG_MANIFEST_FEATURES=windows-app -DMSIME_VOICE_WINDOWS_APP=ON
cmake --build build-voice --config Release --parallel
New-Item -ItemType Directory -Force "$env:LOCALAPPDATA/MetasequoiaVoiceInput"
Copy-Item voice/assets/* "$env:LOCALAPPDATA/MetasequoiaVoiceInput" -Recurse -Force
```

Edit the copied `config.toml` with your own SiliconFlow token, then launch `build-voice/platforms/windows/Release/MetasequoiaVoiceInput.exe`. The `assets` directory is at `voice/assets/`, not beside this README. CI compiles this host without launching it or recording audio.

The obsolete `voice/scripts/prepare_env.py` and its build templates have been removed because they overwrote the shared CMake project. Use the commands above. [Archived standalone releases](https://github.com/metasequoiaime/MetasequoiaVoiceInput/releases) remain available as historical binaries; they are not the publishing location for new Engine builds.

This imported host uses cloud recognition. Local Whisper is an optional public-library target used by other hosts; selecting an old `local_whisper` configuration value does not enable it in this standalone app.

## Usage

- **Hotkeys**:
  - RAlt pressed to start recording, release to stop recording and send text to active application
  - RAlt + Space: Lock recording
  - Ctrl + F9: Toggle recording

## Configuration

Edit `$env:LOCALAPPDATA\MetasequoiaVoiceInput\config.toml` (create if not exists) to configure the application.

Below is a template:

```toml
# 自动语音识别（ASR）配置
[asr_api]
# API 基础地址
endpoint = "https://api.siliconflow.cn/v1/audio/transcriptions"
# 服务提供商（如：azure、openai、deepgram 等）
provider = "siliconflow"
# API 访问令牌
token = "<YOUR_OWN_TOKEN>"

# 文本润色配置
[polish_api]
# API 基础地址
endpoint = "https://api.siliconflow.cn/v1/chat/completions"
# 服务提供商（如：azure、openai、deepgram 等）
provider = "siliconflow"
# API 访问令牌
token = "<YOUR_OWN_TOKEN>"

# 基础设置
[settings]
# 偏好语言
language = "zh-cn"
# 触发时是否播放提示音
notification_sound = true
# 上屏前是否要先进行文本润色
polish_text = false
# This standalone host uses cloud recognition.
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
