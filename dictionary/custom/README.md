# 水杉输入法自定义词库

这个仓库保存无法从通用词典稳定取得的人工维护条目。根目录保留现有自定义数据，其中
`translations.txt` 会被 MetasequoiaImeDict 构建流程用作候选窗翻译覆盖；`packs/` 下是用户按需导入的专业词库。

## words.txt

`词<TAB>全拼<TAB>权重`，由 `custom-words` stage 并进 `msime.db` 的全拼分表（`build_all.py`）。全拼音节用 `'` 分隔，权重是正整数。

**权重只升不降**：词条已经在通用词库里而且权重更高时保留通用词库的值。这个文件里历史条目大多写着占位的 `1`，按字面覆盖会把同时存在于通用词库的词打到自己那一页的最底下。需要让一个词在整句里胜出就把权重写到同量级的真实词附近（`扛不住` 是 2430，`客气` 是 75460），不要凭感觉填一个极大值。

一行写错整个 stage 会失败，不会静默跳过。改完跑 `python -m unittest discover -s tests -p test_custom_words.py`。

## 专业词库

- [`packs/unreal_houdini`](packs/unreal_houdini)：Unreal Engine、Houdini 和 Houdini Engine for Unreal

专业词库不会自动加入所有用户的候选列表。用户选择导入后，新增内容会作为用户词条保存，
因此既能满足垂直领域输入，也不会用 `SOP`、`TOP`、`PCG` 等短缩写干扰普通用户。

运行以下命令可以检查所有专业词库的字段格式、拼音、重复项和翻译覆盖率：

```powershell
python tools/validate_packs.py
```
