# 公共仓库合并

合并 Engine、Dict、CustomDict、HelpCode 的公共职责。Windows、Linux、Apple 平台工程及 Web、Docs 等仓库保持独立。

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
5. 所有平台迁完后再单独处理旧仓归档与入口说明；本变更不归档仓库。

导入数据的来源和许可按 [NOTICE.md](../NOTICE.md) 保留；仓库边界改变不代表统一重新授权。

## VoiceInput 公共化

VoiceInput 也以保留完整历史的 subtree 导入 `voice/`。公共目标只包含 PCM/WAV、RMS VAD、云识别/文本整理、可选 miniaudio 录音和 Whisper；Windows UI 单独移到 `voice/platforms/windows/` 并链接公共目标。macOS 示例直接调用同一实现。

平台接入必须在生产者合入后固定合入提交。当前 Server 的 Doubao/WebSocket 等新增能力没有回退到旧 VoiceInput，也未在本变更中迁移。Apple 的产品 UI 和 iOS 主 App/键盘通信属于消费端接入，本变更提供可编译运行的 macOS 示例及公共库。

迁移验证：默认 Engine 13 项 CTest、词库 Python/自定义包/notice 校验、完整 desktop/mobile 及同仓查询回放通过；相同输入的出货 payload SHA-256 对比保存在 `consolidation-payloads.json`。
