# 公共辅助码

供 Windows、Apple 和 Linux 共用的辅助码数据。

所有辅助码文件统一放在 `helpcodes\` 目录下。当前 server 使用的方案和文件为：

- `lantian`：`helpcode.txt`（蓝天小雨点）
- `ziranma`：`zrm_helpcode_big_unique.txt`（自然码）
- `shouyou2_0`：`shouyou2_0_helpcode.txt`（首右2.0）
- `shouyouplus`：`shouyouplus_helpcode.txt`（首右plus）
- `xiaohe`：`xiaohe_helpcode.txt`（小鹤）

公共数据位于 Engine 的 `helpcode/helpcodes/`，平台打包时复制需要的文件到应用资源目录。运行时路径由平台传给引擎，不依赖作者机器的绝对路径。合仓后的消费端无需再单独检出 HelpCode 仓库。

## 参考

- 自然码辅助码：<https://github.com/copperay/ZRM_Aux-code>
