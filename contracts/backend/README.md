# 后端 HTTP 协议 v1

`protocol.json` 是 MSIME-Backend 和各平台客户端的权威 HTTP 契约，统一定义路径、鉴权、限制、JSON 示例及异步结果约束。共通服务独立于 Windows 命名管道宿主，本地输入行为仍由 Engine 负责。

首版 API 包含云候选、用于联想与润色的非流式 Chat Completions、DeepLX 兼容翻译和 multipart WAV 转写；实时语音通过独立的 WebSocket 入口传输豆包二进制帧。`/v1/capabilities` 明确报告功能是否启用。清单注明了可选的供应商提示字段。未启用的功能明确返回错误，不自动切换到其他网络地址。

Go 后端导入清单的精确副本，并生成常量：

```sh
python3 ../MSIME-Backend/scripts/sync_contract.py --source contracts/backend/protocol.json
python3 ../MSIME-Backend/scripts/sync_contract.py --source contracts/backend/protocol.json --check
```

后端常规 CI 核对随仓清单与生成常量，并通过 HTTP/TLS 执行各操作的示例。跨仓评审还需比较后端副本与本文件。消费者更新 Engine gitlink 前，清单对应提交必须先合入上游；未合并的本地副本只是开发验证材料，不是已发布依赖。

兼容响应可以添加客户端忽略的字段。未知请求字段目前会被拒绝，因此客户端增加字段前应先完成服务端兼容性修改。破坏性语义变化需要新的 API 版本或路径。即便请求文字相同，平台也必须保留 Engine 查询身份、组合输入代次及焦点检查。

RIFF/WAVE 分块遵循 [Microsoft RIFF 规范](https://learn.microsoft.com/en-us/windows/win32/xaudio2/resource-interchange-file-format--riff-)。批量转写允许 PCM（8/16/24/32 位）或 IEEE 浮点（32/64 位）、1–8 声道、8–192 kHz，且不得超过上传大小限制。当前 Engine 宿主生成单声道 16 位 PCM WAV；具体上游可能有更严格的格式要求。
