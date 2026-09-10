# BML+ SDK

[English](README.md)

这个压缩包用于开发 Mod，不是 BML+ 运行时包，不能直接解压到 Ballance 游戏目录。

## 选择开发路线

### 脚本 Mod

除非明确需要原生 Hook、原生内存访问、生成式 IMC Provider 或性能敏感的原生循环，
否则先从脚本 Mod 开始。

1. 打开 [`share/BML/docs/zh-CN/modding.md`](share/BML/docs/zh-CN/modding.md)。
2. 在平常保存源码的工作区中创建 Mod；省略名称和作者时会自动推导：

   ```bat
   "<BML-SDK>\scripts\bml.cmd" new script yourname.my-mod
   ```

3. 进入项目并运行 `bml run`。首次运行会询问 Ballance 目录、部署受管副本、启动
   Player，并持续同步源码以保留热重载。运行 `bml pack` 会生成
   `dist/<项目名>.zip`。

需要自行编写自动化流程时，也可以手动复制
[`templates/script-mod-template`](templates/script-mod-template)。

模板运行成功后，从 [`examples/script-mod`](examples/script-mod) 复制一个目录，
通过可运行示例学习命令与配置、输入与界面或游戏状态访问。

存在 `templates/script-mod-template` 和 `docs/api/as.predefined` 时，表示该 SDK
包含脚本支持。`as.predefined` 只供编辑器补全使用，不是运行时脚本。

### 原生 Mod

只有确实需要 C++、Virtools SDK 或生成式 IMC 服务时才选择原生路线。

1. 打开 [`share/BML/docs/zh-CN/modding.md`](share/BML/docs/zh-CN/modding.md)。
2. 在源码工作区中创建项目；省略名称和作者时会自动推导：

   ```bat
   "<BML-SDK>\scripts\bml.cmd" new native yourname.my-mod
   ```

3. 进入项目后运行完整开发循环；首次运行后会在本地记住路径：

   ```bat
   .\bml run
   ```

   首次运行会询问 Virtools SDK 和 Ballance 目录并记住答案。之后会自动构建、部署、
   启动 Player，并在退出后只显示这个 Mod 本次新增的日志。

也可以手动复制 [`templates/native-mod-template`](templates/native-mod-template)。

需要发布仅限原生侧使用的 provider interface 时，参见两个独立工程：
[`examples/native-interface-provider`](examples/native-interface-provider) 和
[`examples/native-interface-consumer`](examples/native-interface-consumer)。提供方使用
`bml_add_interface_package`；使用方只取得安装后的头文件 target，不链接提供方二进制。

## 目录用途

| 路径 | 用途 |
| --- | --- |
| `templates/` | 可直接运行的起始项目 |
| `examples/` | 聚焦单一主题的后续示例 |
| `share/BML/docs/en/` | 英文 Mod 开发文档 |
| `share/BML/docs/zh-CN/` | 中文 Mod 开发文档 |
| `docs/api/` | 启用脚本支持时提供的 AngelScript 编辑器声明 |
| `include/`、`lib/` | 原生头文件、库和 CMake package |
| `share/BML/tools/` | 原生 interface 与 IMC 代码生成器 |
| `scripts/` | 统一的原生与脚本 Mod Developer Workflow |

安装 BML+ 到游戏时应使用运行时发布包 `BMLPlus-<version>.zip`，不能使用 SDK
压缩包代替运行时包。
