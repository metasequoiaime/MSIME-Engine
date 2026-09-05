# Metasequoia IME Engine（水杉输入法引擎）

水杉输入法的输入引擎：拼音切分、候选查询、用户词库、各输入方案。不含任何界面代码，被各平台前端复用。

General IME engine shared by the Metasequoia IME frontends: [MSIME-Windows](https://github.com/metasequoiaime/MSIME-Windows) (through [MSIME-Server](https://github.com/metasequoiaime/MSIME-Server)), [MSIME-Apple](https://github.com/metasequoiaime/MSIME-Apple) and [MSIME-Linux](https://github.com/metasequoiaime/MSIME-Linux).

## 构建与测试

CI 在 Ubuntu、macOS 和 Windows 三个平台上跑的就是下面这套，是当前唯一被持续验证的构建方式。

依赖：CMake 3.25+、支持 C++17 的编译器、Boost、fmt、spdlog、SQLite3。

```bash
git clone --recursive https://github.com/metasequoiaime/MSIME-Engine.git
cd MSIME-Engine
```

Ubuntu：

```bash
sudo apt-get install --no-install-recommends -y build-essential cmake libboost-dev libfmt-dev libspdlog-dev libsqlite3-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure --timeout 20
```

macOS：

```bash
brew install boost fmt spdlog
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(brew --prefix)"
cmake --build build --parallel
ctest --test-dir build --output-on-failure --timeout 20
```

Windows：

```powershell
vcpkg install --triplet x64-windows-static
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_INSTALLATION_ROOT/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows-static
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure --timeout 20
```

`ctest` 跑 11 个测试目标：`platform_api`、`data_path`、`engine_smoke`、`input_session`、`frequency_input_session`、`english_input_session`、`mixed_expressive_input_session`、`jianpin_input_session`、`temporary_input_session`、`local_modes`、`online_input_session`。

## tests/ 下那套独立工程

`tests/CMakeLists.txt` 是一个**独立的 Windows-only 工程**，产出 `imetest`，源文件是 `tests/src/test_pinyin.cpp`。它**不被根 `CMakeLists.txt` 引入**，所以 CI 不构建也不运行它——`test_pinyin.cpp` 里的用例在 CI 中不会执行，往那个文件加测试不会被自动跑到。

它还要求本机装有 Boost 且路径与 `tests/CMakeLists.txt` 里写死的 `Boost_ROOT` 一致，并使用 MSVC 专有编译选项。

```powershell
python .\tests\scripts\prepare_env.py
cd .\tests\
.\scripts\llaunch.ps1
```

新增测试请优先加到根 `CMakeLists.txt` 已经登记的那些目标里，那些才会在三个平台上被跑到。

## 数据

引擎读取的词库由 [MSIME-Dict](https://github.com/metasequoiaime/MSIME-Dict) 构建：`msime.db`（全拼、五笔、快捷短语、日语词表）、`english.db`（英文候选与中英释义）、`others.db`（emoji、颜文字、符号）、`dict_japanese.dat`（日语整句模型）。

候选窗的中英翻译除了 `english.db` 的大表之外，还有一层人工覆盖：`english.db` 同目录下的 `custom_translations.txt`，格式为 `源<Tab>释义`，优先级高于大表（见 `english/english_dictionary.cpp`）。要纠正某条释义时改这里，不要动 ECDICT 生成的大表。

## 跨仓约定

全拼主库的分表命名、IPC 协议等跨仓硬约定，以 [MSIME-Windows 的 AGENTS.md](https://github.com/metasequoiaime/MSIME-Windows/blob/main/AGENTS.md) 为准。其中与本仓最相关的一条：`msime.db` 按音节数加首音节首字母分表，1–7 音节是 `tbl_{N}_{首字母}`，≥8 音节是 `tbl_others_{首字母}`，权威实现是 `quanpin/quanpin_query.cpp` 的 `build_table_name`。建库、查询、设置页加词、用户词库回放四处必须一致。
