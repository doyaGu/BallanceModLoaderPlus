# BML+ SDK

[English](README.md)

这个压缩包用于开发 Mod，不是 BML+ 运行时包，不能直接解压到 Ballance 游戏目录。

## 选择开发路线

### 脚本 Mod

除非明确需要原生 Hook、原生内存访问、生成式 IMC Provider 或性能敏感的原生循环，
否则先从脚本 Mod 开始。

1. 打开 [`share/BML/docs/zh-CN/modding.md`](share/BML/docs/zh-CN/modding.md)。
2. 在 `ModLoader/Mods` 中创建 Mod：

   ```powershell
   Set-Location "<Ballance>/ModLoader/Mods"
   & "<BML-SDK>/scripts/New-BMLScriptMod.ps1" `
     -Id "yourname.my-mod" -Name "My Mod" -Author "Your Name"
   ```

3. 按生成的 README，先原样运行成功，再开始修改。

需要自行编写自动化流程时，也可以手动复制
[`templates/script-mod-template`](templates/script-mod-template)。

模板运行成功后，从 [`examples/script-mod`](examples/script-mod) 复制一个目录，
通过可运行示例学习命令与配置、输入与界面或游戏状态访问。

存在 `templates/script-mod-template` 和 `docs/api/as.predefined` 时，表示该 SDK
包含脚本支持。`as.predefined` 只供编辑器补全使用，不是运行时脚本。

### 原生 Mod

只有确实需要 C++、Virtools SDK 或生成式 IMC 服务时才选择原生路线。

1. 打开 [`share/BML/docs/zh-CN/modding.md`](share/BML/docs/zh-CN/modding.md)。
2. 在你的源码工作区中创建项目：

   ```powershell
   & "<BML-SDK>/scripts/New-BMLNativeMod.ps1" `
     -Id "yourname.my-mod" -Name "My Mod" -Author "Your Name"
   ```

3. 按生成的 README，使用该 SDK 和 Virtools SDK 配置 x86 构建。

也可以手动复制 [`templates/native-mod-template`](templates/native-mod-template)。

## 目录用途

| 路径 | 用途 |
| --- | --- |
| `templates/` | 可直接运行的起始项目 |
| `examples/` | 聚焦单一主题的后续示例 |
| `share/BML/docs/en/` | 英文 Mod 开发文档 |
| `share/BML/docs/zh-CN/` | 中文 Mod 开发文档 |
| `docs/api/` | 启用脚本支持时提供的 AngelScript 编辑器声明 |
| `include/`、`lib/` | 原生头文件、库和 CMake package |
| `share/BML/tools/` | 生成式 IMC 工具 |
| `scripts/` | Mod 创建和脚本 Mod 打包工具 |

安装 BML+ 到游戏时应使用运行时发布包 `BMLPlus-<version>.zip`，不能使用 SDK
压缩包代替运行时包。
