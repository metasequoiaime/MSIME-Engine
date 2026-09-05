# 公共仓库合并

合并 Engine、Dict、CustomDict、HelpCode、VoiceInput 的公共职责。Windows、Linux、Apple 平台工程及 Web、Docs 等仓库保持独立。

## 历史与路径

来源提交固定在 [consolidation-sources.json](consolidation-sources.json)。通过不 squash 的 subtree 导入保留原提交、作者和 ancestry；不复制旧 release/tag 到 Engine，也不改写原仓历史。现有 Engine 目录和 CMake target 不变。

Dict 下 Engine 和 CustomDict 的第一方 gitlink 已移除。格式 API 直接从同一 checkout 的 `contracts/dictionary/format.py` 加载，人工覆盖读取 `dictionary/custom/translations.txt`。第三方 googlepinyinime-rev、utfcpp 仍锁定提交。

CustomDict 导入当前 main，而非 Dict 原先固定的 `691ef18`：包含更新的根翻译覆盖和可选专业词包。专业包不自动加入默认产品。迁移校验使用“旧 Dict + 同一份最新 CustomDict”作为基线，区分路径迁移和词条更新；不能声称相对旧 release 内容完全不变。

## 构建和验证

根 `build_profile.py` 是消费端公共入口，参数和 schema v1 不变，默认产物位置改为 `dictionary/out/{desktop,mobile}`。指定 `--output` 可保持消费端自己的目录；`compact_dictionary` 仍可导入。manifest 的 source/format/custom commit 都报告同一 Engine 提交，并明确数据所在路径。

根工作流同时负责原有三平台 Engine CTest 和词库产品 CI。词库 CI 检查来源声明、自定义词包、完整桌面及移动产品，并使用同仓 C++ Engine 实际查询、创建多音节词与回放。迁移还对同输入的桌面/移动出货 payload 做 SHA-256 比较，manifest 因生产仓库/提交变化单独验证。

## 发布和下游切换

1. 先合入本公共仓库变更；平台随后固定已合入默认分支的 Engine 提交。
2. 平台删除各自 Dict/CustomDict/HelpCode 的重复 checkout，辅助码路径改为 Engine 的 `helpcode/helpcodes/`，构建器改为 Engine 根 `build_profile.py`。
3. 既有 `MSIME-Dict/dict-2026.09.05` 及其 digest 锁继续有效。本迁移不删除、重发或重定向旧 release。
4. 后续在 MSIME-Engine 显式发布新 `dict-*` 产品时，平台同时评审产物 repository/tag/digest 锁变更。仅合仓不会自动改变用户安装包。
5. 旧 Dict、CustomDict、HelpCode、VoiceInput 已归档，README 指向本仓对应目录；旧 Windows 组件也已归档，源码在 MSIME-Windows 内按组件分目录维护。历史 Release 和标签保留。

导入数据的来源和许可按 [NOTICE.md](../NOTICE.md) 保留；仓库边界改变不代表统一重新授权。

## VoiceInput 公共化

VoiceInput 也以保留完整历史的 subtree 导入 `voice/`。公共目标只包含 PCM/WAV、RMS VAD、云识别/文本整理、可选 miniaudio 录音和 Whisper；Windows UI 单独移到 `voice/platforms/windows/` 并链接公共目标。macOS 示例和 Apple 输入法的菜单、设置与快捷键直接调用同一实现。

平台接入必须在生产者合入后固定合入提交。Windows Server 保留 Doubao/WebSocket 等传输和平台行为；公共 `provider_protocol.h` 统一 multipart、JSON 与结果解析，平台可沿用自己的 HTTP 策略。Apple 产品已提供 macOS 语音设置、Keychain、录音与上屏接入，iOS 键盘仍不提供语音入口。

迁移验证：默认 Engine 13 项 CTest、词库 Python/自定义包/notice 校验、完整 desktop/mobile 及同仓查询回放通过；相同输入的出货 payload SHA-256 对比保存在 `consolidation-payloads.json`。

首个合仓词库 Release 为 [`dict-v1.0.0`](https://github.com/metasequoiaime/MSIME-Engine/releases/tag/dict-v1.0.0)，来源提交 `d0dc0c2b594b5540b5de99ad12085c786410626e`。下载的全部资产与成功的 Engine 词库 CI 产物逐字节相同；英文释义包含上述 CustomDict 更新，不能据此宣称相对旧 Dict 产品所有字节不变。
