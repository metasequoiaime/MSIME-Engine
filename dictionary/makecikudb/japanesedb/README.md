# 日语 SQLite 候选表

`build_japanese_db.py` 将 `ReferenceProjects/rime-jp_sela/jp_sela.dict.yaml`
导入 `out/msime.db` 的 `japanese_lexicon` 表：

```powershell
python makecikudb/japanesedb/build_japanese_db.py
```

该表只负责假名、单字和短候选。整句解码使用的词成本、左右文 ID、连接矩阵
应由独立构建器生成 `dict_japanese.dat`，不要写入这个表。

发布前需要确认并保留 `rime-jp_sela` 原仓库声明的授权和署名信息。

## 整句二进制模型

从 Mozc OSS 词典下载原始词条、上下文 ID 和连接成本矩阵，并生成运行时只读模型：

```powershell
python makecikudb/japanesedb/build_sentence_model.py --download
```

输出为 `out/dict_japanese.dat`。安装时将它放到
`%LOCALAPPDATA%/metasequoiaime/dict_japanese.dat`。构建器同时保存 Mozc 的
`README.txt`；发布二进制模型时必须随产品保留其中的 IPAdic/ICOT/Okinawa 授权声明。
