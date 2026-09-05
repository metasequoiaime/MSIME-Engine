# AGENTS.md — MSIME-Engine

组织级约定和跨仓边界以 [组织 AGENTS.md](https://github.com/metasequoiaime/.github/blob/main/AGENTS.md) 为准。本文件补充本仓的实现、数据和验证规则。

本仓是纯输入引擎：拼音切分、候选查询、用户词库、各输入方案。不含界面代码，被 Windows（经 Server）、macOS/iOS、Linux 三个前端复用。

## 构建与测试

```bash
git submodule update --init --recursive   # googlepinyinime-rev 与 utfcpp；新建的 git worktree 不会自动带上它们
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure --timeout 20
```

根 `CMakeLists.txt` 会编译 `googlepinyinime-rev/src/share/` 下的源码并把 `utfcpp/source` 加为 include 目录，两个目录都是 submodule。CI 的三个 job 都用 `submodules: recursive` 检出，所以这一步在本地容易被漏掉：目录为空时配置阶段就会失败。

CI 在 `ubuntu-24.04`、`macos-15`、`windows-2025` 三个平台跑这套，共 13 个 ctest 目标。依赖 CMake 3.25+、Boost、fmt、spdlog、SQLite3，各平台的安装命令见 `.github/workflows/ci.yml`。

**改了引擎就要跑 ctest。** 本仓是三个平台共用的，只在一个平台上想当然很容易漏。

### tests/ 是一个 CI 不构建的独立工程

`tests/CMakeLists.txt` 产出 `imetest`，源文件只有 `tests/src/test_pinyin.cpp`。它**不被根 `CMakeLists.txt` 引入**——根目录没有任何 `add_subdirectory`。所以：

- `tests/src/test_pinyin.cpp` 里的用例**在 CI 中不会执行**
- 往那个文件加测试，PR 的绿勾只代表引擎仍能编译，不代表测试跑过
- 它还写死了 `Boost_ROOT` 并使用 MSVC 专有选项，只有作者的机器能构建

新增测试请加到根 `CMakeLists.txt` 已登记的目标里（`tests/src/test_*_input_session.cpp` 那一批），那些才会在三个平台上跑到。

## 全拼分表命名（跨仓硬约定）

`msime.db` 按音节数加首音节首字母分表。**权威定义是 `contracts/dictionary/format.json`**，C++ 生成头和 Python API 分别供查询/回放与 Dict 建库使用；设置页通过公共 `quanpin::build_table_name` 写入。修改后运行生成检查及七/八/九音节创建、查询、回放回归。

| 音节数 | 表名 | 示例 |
|---|---|---|
| 1–7 | `tbl_{N}_{首字母}` | `ni'hao` → `tbl_2_n` |
| ≥ 8 | `tbl_others_{首字母}` | `shui'shan'shu'ru'fa'hai'ke'yi` → `tbl_others_s` |

首字母取第一个音节的第一个小写拉丁字母。**禁止对 ≥8 音节拼出 `tbl_8_*`**：建库脚本不会创建这些表，写入和回放都会失败，安装升级时的回放失败会导致整批回滚并中止安装。改规则必须更新共享格式契约，禁止在消费端重新拼接表名。

## 用户词库权重

`user_dictionary/user_dictionary_journal.cpp` 的 `adjust_candidate_ranking` 改起来比看上去危险，几条已经踩过的坑：

- 权重要留在出货词典的量级内（`kManagedWeightCeiling`）。曾经有过一次局部 rebalance 写出 10^12 级数值，以及拿权重为 1 的生僻词当阶梯基准写出负权重到邻近 key 上
- 一个上下文里可能混着多个 `entry_key`（单字母简拼 `y` 会同时出现 `yi` 的「一」和 `you` 的「有」）。按 `entry_key` 过滤候选列表会让选中项 rank 恒为 0，早退分支直接返回，一个权重都不写
- 但写入必须按各候选自己的 key 走，否则会把权重写到别的 key 的行上

改这里之前先读懂现有的护栏注释，它们每一条都对应一个修过的缺陷。

## 切分与候选

- 平局时偏好更短的首音节（`quanpin/quanpin_utils.cpp` 的 `cut_one_piece_min_segments`）。**这个判据是对的，不要翻成前向最长匹配**——在 48068 条带词典切分的多音节词上量过，翻向会从 99.605% 掉到 99.393%，因为拼音的 `n`/`ng` 歧义让前向最长匹配在中文上不成立
- 歧义切分交给词典裁决，走 `quanpin_dictionary.cpp` 的备选切分路径，受 `kMaxSyllablesForMultipleSegmentations` 限制
- `query()` 在每次按键上都会跑。往里加无条件的枚举或查询前先量耗时，`tests/src/test_pinyin.cpp` 里那个 timings 用例只打印不断言，拦不住回归

## 数据

词库由 [MSIME-Dict](https://github.com/metasequoiaime/MSIME-Dict) 构建：`msime.db`、`english.db`、`others.db`、`dict_japanese.dat`。本仓不生成词库，只读取。

候选窗的中英翻译除 `english.db` 的大表外还有一层人工覆盖：`english.db` 同目录的 `custom_translations.txt`，格式 `源<Tab>释义`，优先级高于大表（`english/english_dictionary.cpp`）。要纠正个别释义改这里，不要动 ECDICT 生成的大表。

## 提交

提交信息用 `type(scope): 摘要`。不要添加 `Co-Authored-By`、`Generated with` 或其他 AI 生成标记。
