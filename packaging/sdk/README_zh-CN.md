# BML+ SDK

[English](README.md)

这个压缩包用于开发 Mod，不是 BML+ 运行时包，不能直接解压到 Ballance 游戏目录。

## 选择开发路线

### 脚本 Mod

除非明确需要原生 Hook、原生内存访问、生成式 IMC Provider 或性能敏感的原生循环，
否则先从脚本 Mod 开始。

1. 打开 [`share/BML/docs/zh-CN/modding.md`](share/BML/docs/zh-CN/modding.md)。
2. 把 [`templates/script-mod-template`](templates/script-mod-template) 复制到
   `<Ballance>/ModLoader/Mods/<你的Mod>`。
3. 按模板 README 的说明，先原样运行成功，再开始修改。

存在 `templates/script-mod-template` 和 `docs/api/as.predefined` 时，表示该 SDK
包含脚本支持。`as.predefined` 只供编辑器补全使用，不是运行时脚本。

### 原生 Mod

只有确实需要 C++、Virtools SDK 或生成式 IMC 服务时才选择原生路线。

1. 打开 [`share/BML/docs/zh-CN/modding.md`](share/BML/docs/zh-CN/modding.md)。
2. 复制 [`templates/native-mod-template`](templates/native-mod-template)。
3. 按模板 README 的说明，使用该 SDK 和 Virtools SDK 配置 x86 构建。

## 目录用途

| 路径 | 用途 |
| --- | --- |
| `templates/` | 可直接运行的起始项目 |
| `share/BML/docs/en/` | 英文 Mod 开发文档 |
| `share/BML/docs/zh-CN/` | 中文 Mod 开发文档 |
| `docs/api/` | 启用脚本支持时提供的 AngelScript 编辑器声明 |
| `include/`、`lib/` | 原生头文件、库和 CMake package |
| `share/BML/tools/` | 生成式 IMC 工具 |
| `scripts/` | 脚本 Mod 打包工具 |

安装 BML+ 到游戏时应使用运行时发布包 `BMLPlus-<version>.zip`，不能使用 SDK
压缩包代替运行时包。
