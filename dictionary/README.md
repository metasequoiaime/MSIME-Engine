# 介绍

## 初始词库的来源

以下是 5 个中文词库。

- `BaseDict.txt` 词库来自[这里](https://github.com/wuhgit/CustomPinyinDictionary)，取的是 `2023-09-28(No.82)` 这个版本，条目目前有 `1415159` 个条目。
- `BaseDictIce.txt` 词库来自[这里](https://github.com/iDvel/rime-ice)
- `SampleIMESimplifiedQuanPin.txt` 词库来自[这里](https://github.com/microsoft/Windows-classic-samples/tree/main/Samples/IME/cpp/SampleIME/Dictionary)。

上面三个都是原封不动搬过来的。

- `SingleChars.txt` 来自[这里](https://github.com/iDvel/rime-ice)。但是，经过了我的处理，把一些原来不对的拼音，比如，绿(lv)给使用这个[仓库](https://github.com/mozillazg/pinyin-data/blob/master/pinyin.txt)中的数据纠正了过来，保留了原有的雾凇的 8105 简体常用字的同时，对新加入的没有权重的字作了去重，去重的逻辑是把在 8105 中存在的条目给去掉。
- `FanyExtDict.txt` 我自己根据上面的基础添加的一些不与上面的词库重复的一些条目的词库。
- `Wubi86.txt` 来自这个 Rime 的一个[词库](https://github.com/KyleBing/rime-wubi86-jidian)。

以下是 1 个英文词库。

- BaseDictIceEn.txt 词库来自[这里](https://github.com/iDvel/rime-ice)

## 构建词库

`build_all.py` 把 `makecikudb/` 下各目录的分步脚本按正确顺序串起来，一次产出全部四个发布产物。各脚本本身仍是权威，单独执行的用法不变。

```bash
python -m pip install -r requirements.txt
python build_all.py --clean --fetch-references
python tools/verify_dictionaries.py
```

`requirements.txt` 只装出货构建需要的东西，跨平台可用。`makecikudb/` 下那些一次性辅助脚本另有依赖，在 `requirements-tools.txt` 里（其中 `pywin32` 带平台标记，非 Windows 上会自动跳过）。

产物写到 `out/`，同时生成 `out/SHA256SUMS.txt`：

| 产物 | 内容 |
|---|---|
| `msime.db` | 全拼分表、86 五笔、快捷短语、日语词表 |
| `english.db` | 英文候选词表，以及 ECDICT 双向释义表 |
| `others.db` | emoji、颜文字、符号目录 |
| `dict_japanese.dat` | 日语整句解码用的只读 Viterbi 模型 |
| `mozc_dictionary_oss_README.txt` | Mozc 词典的 IPAdic / ICOT / 冲绳授权声明，**必须随 `dict_japanese.dat` 一起分发** |

`--fetch-references` 会把两个外部数据源按固定 revision clone 到仓库同级的 `ReferenceProjects/`：ECDICT 提供候选窗释义，rime-jp_sela 提供日语词表。不加这个参数时，依赖它们的 stage 会被跳过而不是报错；发布构建请配合 `--require-all` 让缺失直接失败。日语整句模型的 Mozc 原始数据由脚本自己下载。

常用参数：

```bash
python build_all.py --list                    # 列出全部 stage
python build_all.py --only quanpin wubi       # 只跑指定 stage
python build_all.py --skip english-glosses    # 跳过指定 stage
```

`tools/verify_dictionaries.py` 校验每张发布表存在且行数不低于下限，用来拦住「stage 跑成功但表是空的」这种情况。GitHub Actions 的 `Build dictionaries` workflow 每次 push 和 PR 都会跑完整构建加校验；手动触发并勾选 `publish` 时，会把四个产物和校验和发布成一个 `dict-YYYY.MM.DD` 的 release，供 `MSIME-Windows` 的安装包发布流程下载。

## 说明

词库的 txt 文件打开时最好使用思源系列的字体或者花园明朝应该也可以，因为有些生僻的汉字在 Windows 下的很多字体是不支持的。

对于从别的仓库搜集来的词库，我不会在其中添加新的条目，但是，如果发现有错误的地方，会作相应的修改。

我自己添加的条目会放到 FanyExtDict.txt 中。

候选窗中英翻译的补丁不要改 ECDICT 大词库，写到 `MetasequoiaImeCustomDict/translations.txt`。

### cn 目录

- BaseDictIceV1.txt 经过我处理的 BaseDictIce.txt 的修改版。
- BaseDictV1.txt 经过我处理的 BaseDict.txt 修改版。
- BaseDictAllV1Part1.txt 和 BaseDictAllV1Part2 这两个合起来就是我把上面那两个去重的结果
- 53013_single.txt 这个是我对所有 unicode 现在支持的汉字

### en 目录

- `google_count_1_w.txt`: <https://www.norvig.com/ngrams/count_1w.txt>，谷歌的 1/3 million 词库。
- `oaldpe_words.txt`: oaldpe.mdx 中提取出来的单词列表。

## 额外说明

由于 Github 的单个文件的限制，我将 ./cn/BaseDictV1.txt 拆成了两个文件，分别是，

- ./cn/BaseDictAllV1Part1.txt
- ./cn/BaseDictAllV1Part2.txt

然后，逐个解释每个词库的来源/里面到底装的是什么东西，

- 53013_single.txt: 汉字的单字，来自 unicode 的文档
- BaseDictAllV1Part1.txt: 把上面所说的百万词库和雾凇的短语词库结合在一起，然后去重，拆分出来的 part1
- BaseDictAllV1Part2.txt: 把上面所说的百万词库和雾凇的短语词库结合在一起，然后去重，拆分出来的 part2
- BaseDictIceV1.txt: 雾凇词库
- BaseDictV1.txt: 这是上面所说的百万词库(https://github.com/wuhgit/CustomPinyinDictionary)
- HelpCode.txt: 辅助码，规则主要参考小鹤的形码
- phrases.txt: 自造短语
- SingleCharsAllV1.txt: 结合了雾凇词库中的单字，然后补充了 uniocde 中的剩下几万个比较常见的字，虽然国家规范常用字只有 8000 个左右

---

感谢：

- <https://github.com/iDvel/rime-ice>
- <https://github.com/wuhgit/CustomPinyinDictionary>
- <https://github.com/aoguai/rime_kaomoji_dict>

## 许可

本仓库**不对外提供统一的开源许可**：其中绝大部分词库并非本项目的作品，给整个仓库挂一份 LICENSE 等于替上游作者重新授权。逐项的来源与上游条款见 [NOTICE.md](NOTICE.md)，使用或再分发前请以对应上游的条款为准。

`makecikudb/` 下由本项目编写的构建脚本以 GPL-3.0 提供，见 [makecikudb/LICENSE](makecikudb/LICENSE)。
