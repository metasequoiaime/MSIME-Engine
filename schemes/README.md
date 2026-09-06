# 输入方案

一个「输入方案」负责把按键序列变成一次候选查询。全拼、双拼、五笔、日文罗马字四种方案共用同一个接口，彼此不知道对方存在——加一种新方案（注音、粤拼、仓颉……）不需要改动任何一个已有方案。

这一页说明加一种方案要动哪几处、每一处要做什么。招募文档把「实现新的输入方案」列为门槛最高的方向，但此前这个目录下只有一个 17 行、零注释的纯虚基类。

## 接口

[`input_scheme.h`](input_scheme.h) 定义了全部五个方法：

```cpp
class IInputScheme
{
  public:
    virtual ~IInputScheme() = default;

    virtual void reset() = 0;
    virtual void handle_key(ImeKeyCode vk, ImeModifierMask modifiers_down, ImeCharacter wch) = 0;
    virtual QueryRequest build_request() const = 0;
    virtual std::string get_preedit() const = 0;
    virtual SchemeType type() const = 0;
};
```

| 方法 | 职责 | 注意 |
| --- | --- | --- |
| `reset()` | 清空当前编码串与按键记录 | 上屏、取消、切换方案时都会调用，必须回到干净状态 |
| `handle_key()` | 收一个按键，更新内部状态 | **不查词库、不发网络请求、不碰全局状态。** 这个方法在每一次击键的关键路径上，宿主进程等着它返回 |
| `build_request()` | 把当前状态打包成 `QueryRequest` | `const` 方法，不改状态；同样的状态调用两次必须得到同样的结果 |
| `get_preedit()` | 返回要显示在编辑区的文本 | 用户看到的正在输入的内容，不一定等于原始按键 |
| `type()` | 返回本方案的 `SchemeType` | 候选提供者据此路由 |

`handle_key` 只接受方案自己认得的键。哪些键该由方案处理、哪些该交回宿主（翻页、选词、方向键），由调用方 `ImeSession` 决定，方案不需要关心。

## QueryRequest：方案与候选查询之间的契约

[`../core/query_request.h`](../core/query_request.h) 里的这个结构是方案唯一的输出。字段不必全填，但 `valid` 必须准确——`valid == false` 表示当前编码还不足以查询，会话不会去查词库。

几个容易搞错的字段：

- `raw_input` 是规范化后的编码串（通常是小写），`raw_input_with_cases` 保留原始大小写。英文混输和以词定字要用后者。
- `normalized_input` 是拿去查库的键。全拼里它是纠错、简拼展开之后的结果；一个不做这类处理的方案直接填成和 `raw_input` 一样即可。
- `segmentation` / `raw_segmentation` / `normalized_segmentation` 是分词结果，音节之间用 `'` 分隔。不分音节的方案（五笔）留空。
- `key_strokes` 是原始按键序列。候选窗要还原用户实际敲了什么时用它，比如显示原始编码。
- `enable_*` 那几个开关由会话按用户配置填写，方案不要自己设。

## 加一种新方案要动的地方

以五笔为例走一遍（[`wubi_scheme.h`](wubi_scheme.h) / [`wubi_scheme.cpp`](wubi_scheme.cpp) 是最短的一个实现，适合照着看）：

1. **`../core/scheme_type.h`** — 在 `SchemeType` 里加一个枚举值。

2. **`schemes/<name>_scheme.{h,cpp}`** — 实现 `IInputScheme`。构造函数可以带参数：双拼方案就接收一个 `ShuangpinProfile`，方案表由调用方注入而不是硬编码在方案里。

3. **`../core/ime_session.cpp` 的 `create_scheme()`** — 在 switch 里加一个分支返回你的实现。这是唯一注册点，没有别的注册表或工厂宏。

4. **候选提供者** — 决定查询走哪条路。`../providers/provider_registry.h` 的 `resolve(SchemeType)` 把方案映射到一个 `ICandidateProvider`。如果新方案查的是已有的库（比如注音可以复用拼音库，只是编码转换不同），映射到已有的 provider 即可，不必新写一个。查全新的表才需要实现 [`../providers/candidate_provider.h`](../providers/candidate_provider.h) 的 `ICandidateProvider`——注意它有八个方法，除了 `query()` 还包括造词、调频、删词和动态候选缓存，都要实现。

5. **词库** — 新方案如果需要自己的表，表结构受 [`../contracts/dictionary/format.json`](../contracts/dictionary/format.json) 约束，构建脚本在 [`../dictionary/`](../dictionary/) 下，加一个构建阶段到 `dictionary/build_all.py` 的 `STAGES`。词库数据的来源和许可要求见 [`../dictionary/NOTICE.md`](../dictionary/NOTICE.md)——**没有明确再分发授权的数据不要加进默认构建**。

6. **`../CMakeLists.txt` 与 `../tests/CMakeLists.txt`** — 两份源码列表都要加上新文件。只加前者会导致测试链接失败，只加后者会导致产品里没有这个方案。

7. **测试** — `tests/src/` 下加一个 `test_<name>_scheme.cpp`，并登记到 `tests/CMakeLists.txt`。参考 [`test_shuangpin.cpp`](../tests/src/test_shuangpin.cpp)（方案层的按键与 preedit）与 [`test_pinyin.cpp`](../tests/src/test_pinyin.cpp)（编码与查询）。最低限度要覆盖：一串典型按键得到的 `get_preedit()`、`build_request()` 的 `valid` 何时为真、`reset()` 之后状态确实干净、以及非法按键不会破坏状态。

## 提交之前

- `cmake -S . -B build && cmake --build build && ctest --test-dir build` 全绿。
- 说明你实际敲了什么、看到什么。输入法的正确性很难靠读代码判断，PR 里请写清验证方式（见组织的[贡献指南](https://github.com/metasequoiaime/.github/blob/main/CONTRIBUTING.md)）。
- 方案本身不联网、不读配置文件、不写日志。这些都由上层负责。
