# 来源与授权说明

本仓库**不对外提供统一的开源许可**，因为其中绝大部分词库并非本项目的作品，而是从其他项目收集、合并、去重而来（见 [README](README.md)）。给整个仓库挂一份 LICENSE 等于替上游作者重新授权，因此这里改为逐项说明来源与上游条款。使用或再分发本仓库的数据时，请以对应上游的条款为准。

## 中文词库

| 文件 | 上游 | 上游许可 |
| --- | --- | --- |
| `source/BaseDict.txt`、`cn/BaseDictV1.txt` | [wuhgit/CustomPinyinDictionary](https://github.com/wuhgit/CustomPinyinDictionary) | **未声明** |
| `source/BaseDictIce.txt`、`cn/BaseDictIceV1.txt` | [iDvel/rime-ice](https://github.com/iDvel/rime-ice) | GPL-3.0 |
| `cn/BaseDictAllV1Part1.txt`、`cn/BaseDictAllV1Part2.txt` | 上面两者合并去重 | GPL-3.0 与**未声明**的混合 |
| `cn/SingleCharsAllV1.txt` | [iDvel/rime-ice](https://github.com/iDvel/rime-ice)，读音以 [mozillazg/pinyin-data](https://github.com/mozillazg/pinyin-data) 校正 | GPL-3.0 + MIT |
| `source/SampleIMESimplifiedQuanPin.txt` | [microsoft/Windows-classic-samples](https://github.com/microsoft/Windows-classic-samples) | MIT |
| `cn/Wubi86.txt` | [KyleBing/rime-wubi86-jidian](https://github.com/KyleBing/rime-wubi86-jidian) | Apache-2.0 |
| `cn/53013_single.txt` | Unicode 收录的汉字单字表 | 数据本身来自 Unicode 标准；不被任何构建阶段读取，因此不进入任何产物 |
| `cn/SingleCharWhitelist.txt` | **待确认**，见下方「待解决」 | **待确认** |
| `source/FanyExtDict.txt`、`cn/phrases.txt` | 本项目自建 | 见下方「本项目自建部分」 |
| `cn/HelpCode.txt` | 规则参考小鹤形码 | 权利归小鹤方案作者 |

`japanese_lexicon` 表不由本仓库的文件构建，而是构建时从下面这个引用仓库取（版本固定在 `build_all.py` 的 `REFERENCES` 里）：

| 引用仓库 | 上游 | 上游许可 |
| --- | --- | --- |
| `rime-jp_sela`（`jp_sela.dict.yaml`） | [Selaube/rime-jp_sela](https://github.com/Selaube/rime-jp_sela) | **未声明** |

## 英文与符号词库

| 文件 | 上游 | 上游许可 |
| --- | --- | --- |
| `en/BaseDictIceEn.txt` | [iDvel/rime-ice](https://github.com/iDvel/rime-ice) | GPL-3.0 |
| `en/google_count_1_w.txt` | [Google 1/3 million 词频表](https://www.norvig.com/ngrams/count_1w.txt) | 以来源页面说明为准 |
| `en/oaldpe_words.txt` | 自 oaldpe.mdx 提取的词形列表 | 权利归词典出版方 |
| `kaomoji/` | [aoguai/rime_kaomoji_dict](https://github.com/aoguai/rime_kaomoji_dict) | MIT |
| 候选翻译数据 | [skywind3000/ECDICT](https://github.com/skywind3000/ECDICT) | MIT |

## 下游影响

由 `cn/BaseDictAllV1Part1.txt` 与 `cn/BaseDictAllV1Part2.txt` 构建出的 `msime.db` 同时包含 rime-ice（GPL-3.0）与 CustomPinyinDictionary（未声明许可）的内容，其 `japanese_lexicon` 表还包含 rime-jp_sela（未声明许可）的内容。使用该数据库的前端本身以 GPL-3.0 分发，与 rime-ice 兼容，但**必须保留对 rime-ice 的署名**。

三个前端都分发这份数据，署名各自落在这些文件里：

| 前端 | 分发的数据 | 署名现状 |
| --- | --- | --- |
| [MSIME-Linux](https://github.com/metasequoiaime/MSIME-Linux) | DEB／RPM 包内的 `msime.db`、`others.db`、`english.db` 与辅助码 | `THIRD_PARTY_NOTICES.txt`，已覆盖词库 |
| [MSIME-Apple](https://github.com/metasequoiaime/MSIME-Apple) | app bundle 内的 `msime.db` 与辅助码 | `THIRD_PARTY_NOTICES.txt`，已覆盖词库 |
| [MSIME-Windows](https://github.com/metasequoiaime/MSIME-Windows) | 安装包内的 `msime.db`、`others.db`、`english.db`、`dict_japanese.dat` 与辅助码 | `THIRD_PARTY_NOTICES.txt`，已覆盖词库，由安装包装到程序目录 |

改动本文件的来源表时，这几份文件要一起改；它们才是随产物送到用户手上的那一份。

## 待解决

以下部分目前没有明确的再分发授权，需要与上游作者确认后才能补上：

- [wuhgit/CustomPinyinDictionary](https://github.com/wuhgit/CustomPinyinDictionary) 未声明任何许可，而它是 `msime.db` 的主体。
- [Selaube/rime-jp_sela](https://github.com/Selaube/rime-jp_sela) 未声明任何许可，`msime.db` 的 `japanese_lexicon` 表由它构建。
- `cn/SingleCharWhitelist.txt` 的来源没有记录。它参与 `msime.db` 的构建（`makecikudb/quanpindb/makedb/multi_table_has_jp/insert_data.py` 用它过滤单字条目），所以需要补上来源；在补上之前不要假定它可以再分发。
- `en/oaldpe_words.txt` 提取自商业词典。词典本体 `en/oaldpe.mdx` 曾经也在本仓中，现已移除——构建只需要提取好的词形列表，不需要词典本体。需要重新生成词表时，自备 `.mdx` 并作为参数传给 `makecikudb/englishdb/extract_oaldpe_headwords.py`。**注意移除只影响当前版本，该文件仍留在 git 历史中。**改写历史会让所有 fork、clone 以及下游 `product-lock.json` 里锁定的 commit 全部失效，因此暂不改写；是否改写单独决策。
- 辅助码规则参考自小鹤形码，权利归方案作者。

### 构建默认不再包含这些条目

上面这些条目现在**默认不进入构建产物**。判定写在 [`licensing.py`](licensing.py) 里，`build_all.py` 每次运行都会打印它排除了什么、为什么排除、以及换用了什么替代输入：

| 排除的输入 | 替代 | 后果 |
| --- | --- | --- |
| `cn/BaseDictAllV1Part1.txt`、`Part2.txt` | `cn/BaseDictIceV1.txt`（rime-ice，GPL-3.0） | 中文词库召回下降；rime-ice 是合并前的子集，构建不会失败 |
| `cn/SingleCharWhitelist.txt` | 无 | 不做过滤，`SingleCharsAllV1.txt` 里的单字全部收入 |
| `en/oaldpe_words.txt` | 无 | 英文词表只来自 `BaseDictIceEn.txt` |
| `rime-jp_sela` | 无 | `japanese-lexicon` 阶段整段跳过，`msime.db` 不含该表 |

想构建完整词库（本地开发、评估召回率）用 `--include-unlicensed`，或设环境变量 `MSIME_DICT_INCLUDE_UNLICENSED=1`。**这样构建出来的产物不要附到 release 上。**

拿到上游的书面再分发许可之后，把对应条目从 `licensing.py` 的 `UNLICENSED_INPUTS` 里移出，并在同一次改动里更新本文件。

这一节此前写的是「未决条目并不妨碍产品当前正在分发这些数据」。那句话如实记录了当时的状态，但那个状态本身就是风险所在——它是所有发行版渠道的硬门槛，也是唯一一条可能导致已发布产物被要求下架的问题。现在默认构建只包含本项目有权再分发的数据，未决条目仍然要跟上游谈，但发版风险不再取决于谈判进度。

## 本项目自建部分

`source/FanyExtDict.txt`、`cn/phrases.txt` 以及 `makecikudb/` 下的构建脚本由本项目编写，依据 GPL-3.0 提供，与组织内其他仓库一致。
