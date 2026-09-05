# Unreal Engine 与 Houdini 专业词库

面向 Unreal Engine、Houdini 和 Houdini Engine for Unreal 技术美术工作流的可选词库。
当前包含中文候选、带官方大小写的英文候选，以及中英双向术语释义。

## 导入

在水杉输入法设置的词库管理页面中：

1. 选择拼音词库的“编码导入”，导入 `quanpin.txt`。
2. 选择英文词库的“导入”，导入 `english.txt`。

中文文件使用 `词语<Tab>全拼<Tab>权重`，英文文件使用
`输入键<Tab>显示内容<Tab>权重`。两个文件故意不含标题或注释，避免设置程序把说明文字当作词条。

`translations.txt` 是完整的术语对照审校稿。适合全局显示、歧义较小的条目已经同步到仓库根目录的
`translations.txt`，会随官方词库构建进入候选窗翻译；`UE`、`TOP`、`USD`、`Karma` 等有明显歧义的
短词只保留在本专业包中，不覆盖所有用户的通用释义。

## 收录原则

- 只收录实际会输入的产品名、系统名、网络/节点类别和工作流术语。
- Unreal Engine 中文名优先采用 Epic 简体中文文档。
- Houdini 中文释义用于消歧；SideFX 没有给出官方中文名时，不把描述性翻译标成官方译名。
- 不复制文档定义或第三方词库正文，只记录术语和短释义。
- `@P`、`$HIP`、`r.Nanite` 等代码符号暂不收录，因为当前英文输入键只接受字母、连字符和撇号。
- `UE5` 作为 `ue` 输入键的第二个显示候选，因为数字不能出现在英文输入键中。

## 核对来源

以下官方文档访问于 2026-09-03：

- [Unreal Engine 术语](https://dev.epicgames.com/documentation/zh-cn/unreal-engine/unreal-engine-terminology)
- [Unreal Engine 材质](https://dev.epicgames.com/documentation/zh-cn/unreal-engine/unreal-engine-materials)
- [Nanite 虚拟几何体](https://dev.epicgames.com/documentation/zh-cn/unreal-engine/nanite-virtualized-geometry-in-unreal-engine)
- [Lumen 技术细节](https://dev.epicgames.com/documentation/zh-cn/unreal-engine/lumen-technical-details-in-unreal-engine)
- [World Partition](https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition-in-unreal-engine)
- [Houdini 基础概念](https://www.sidefx.com/docs/houdini/basics/intro)
- [Houdini 几何体属性](https://www.sidefx.com/docs/houdini/model/attributes.html)
- [Houdini 网络类型与节点标记](https://www.sidefx.com/docs/houdini/network/flags)
- [Solaris 与 Karma](https://www.sidefx.com/docs/houdini/solaris/index.html)
- [Houdini Engine for Unreal：Landscapes](https://www.sidefx.com/docs/houdini/unreal/landscape/index.html)
- [Houdini Engine for Unreal：Outputs](https://www.sidefx.com/docs/houdini/unreal/outputs.html)
