# Dictionary product API

Consumers call `build_profile.py`; paths below `makecikudb/` are implementation
details of the public build. Those stage scripts remain the authoritative builders.

```sh
python -m pip install -r requirements.txt
python build_profile.py --profile desktop --fetch-references
python build_profile.py --profile mobile --source out/desktop/msime.db
python build_profile.py --profile desktop --verify
python build_profile.py --profile mobile --verify
```

`desktop` includes all shipping databases, the Japanese model and its notice.
`mobile` retains every single-character pinyin candidate and phrase entries whose
weight is at least 2000, preserves their indexes and long-phrase tables, and omits
other input modes. `--minimum-weight` deliberately changes the compact product.
`--output` selects a destination; mobile defaults to `out/mobile`, desktop to
`out/desktop`. Without `--source`, the build regenerates the shared `out/` staging
area; run builds sequentially within a checkout.

Each product has `dictionary-manifest.json`: manifest and dictionary format versions,
profile/features, source commit and dirty state, fixed external revisions, and the
size/SHA256 of every artifact. A mobile product also records the full source database
digest. `SHA256SUMS.txt` covers the manifest as well. Release consumers must pin these
digests outside the downloaded files, as in the Windows product lock.

Dictionary format 1 uses `tbl_{1..7}_{letter}` and `tbl_others_{letter}` for pinyin,
the existing `key/jp/value/weight` columns, and the `MSJPDT1` Japanese model. Format
changes require Engine query/write/replay compatibility changes in the same product
combination. Adding fields to this manifest does not change the database format.
Existing releases without a manifest remain usable only through an explicitly
locked legacy input; new releases are built and verified through this API.

表名与格式版本来自固定 Engine `contracts/dictionary/`。建表、插入和索引通过根目录 `dictionary_format.py` 使用同一公共 API。`build_profile.compact_dictionary(source, target, minimum_weight)` 是为旧平台脚本保留的 Python 兼容入口，命令行产品构建仍应优先使用 `build_profile.py`。产品清单记录实际格式契约 commit。

所有 SQLite 出货文件在计算摘要前 checkpoint 并切换为 DELETE journal 模式，避免只读消费者依赖未分发的 WAL/SHM。CI 使用固定 Engine 对实际桌面/移动产品执行查询、七/八/九音节写入和回放，并验证英文/快捷短语/表情查询。
