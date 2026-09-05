# 来源与授权说明

本仓库**不对外提供统一的开源许可**。`helpcodes/` 下的辅助码表复现的是各家已发表的输入方案，权利归各方案作者，本项目只是整理成统一格式供引擎读取。给整个仓库挂一份 LICENSE 等于替方案作者重新授权，因此这里改为逐项说明来源。

## 辅助码表

| 文件 | 方案 | 来源与权利归属 |
| --- | --- | --- |
| `helpcode.txt` | `lantian` | 蓝天小雨点辅助码，权利归方案作者 |
| `zrm_helpcode_big_unique.txt` | `ziranma` | 自然码辅助码，整理自 [copperay/ZRM_Aux-code](https://github.com/copperay/ZRM_Aux-code)（上游**未声明许可**） |
| `shouyou2_0_helpcode.txt` | `shouyou2_0` | 首右 2.0 辅助码，权利归方案作者 |
| `shouyouplus_helpcode.txt` | `shouyouplus` | 首右 plus 辅助码，权利归方案作者 |
| `xiaohe_helpcode.txt` | `xiaohe` | 小鹤形码，权利归小鹤方案作者 |

## 下游影响

这些文件被 [MSIME-Linux](https://github.com/metasequoiaime/MSIME-Linux) 的 DEB／RPM 包安装到 `share/metasequoiaime/helpcodes/` 下，也被 Windows 与 Apple 前端使用。前端本身以 GPL-3.0 分发，但该许可**不覆盖**这些辅助码表的内容。

## 待解决

上表中没有任何一项拿到了明确的再分发授权。需要逐个与方案作者确认，或改为在运行时由用户自行导入而不随包分发。在澄清之前，请不要假定这些数据可以自由再分发。

## 本项目自建部分

`scripts/` 下的整理脚本由本项目编写，依据 GPL-3.0 提供，与组织内其他仓库一致。
