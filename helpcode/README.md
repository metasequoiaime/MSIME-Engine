# FanImeHelpCode

Helpcodes for [MetasequoiaImeTsf](https://github.com/metasequoiaime/MetasequoiaImeTsf).

所有辅助码文件统一放在 `helpcodes\` 目录下。当前 server 使用的方案和文件为：

- `lantian`：`helpcode.txt`（蓝天小雨点）
- `ziranma`：`zrm_helpcode_big_unique.txt`（自然码）
- `shouyou2_0`：`shouyou2_0_helpcode.txt`（首右2.0）
- `shouyouplus`：`shouyouplus_helpcode.txt`（首右plus）
- `xiaohe`：`xiaohe_helpcode.txt`（小鹤）

本地调试时，可以把整个目录链接到 server 的数据目录：

```powershell
$target = Join-Path $env:LOCALAPPDATA 'metasequoiaime\helpcodes'
if (Test-Path -LiteralPath $target) {
    Remove-Item -LiteralPath $target -Recurse -Force
}
New-Item -ItemType SymbolicLink -Path $target `
    -Target 'C:\Users\SonnyCalcr\EDisk\CppCodes\IMECodes\MetasequoiaImeHelpCode\helpcodes'
```

## 参考

- 自然码辅助码：<https://github.com/copperay/ZRM_Aux-code>
