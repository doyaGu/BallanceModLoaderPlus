# 安装和使用 BML+

## 先核对运行时

本文对应 **Ballanced SDL** 分支和动态 Player。配套包的 ABI 描述包含
`runtime: "sdl"`，平台、架构、动态宿主链接方式必须匹配；直接使用 Engine/ImGui 的
Mod 还需匹配依赖的编译器、运行库和配置 ABI。原版 Player 的旧归档不是 SDL 包。
本分支尚未实现静态 Player 集成。

使用匹配的完整运行时包；没有配套包时按[参与开发](contributing.md)从对应 Ballanced
基线构建。[支持与验证](validation.md)区分已验收和未运行的目标。

## 安装到游戏根目录

关闭 Player 并备份。根目录包含 `base.cmo`、游戏资源和 `Bin/Player` 或根目录的
`Player`；Windows 使用 `Player.exe`。在这一层解压运行时并保留目录结构：

| 位置 | 用途 |
| --- | --- |
| `Managers/BMLPlus.dll` | Windows Loader |
| `Managers/libBMLPlus.so` / `Managers/libBMLPlus.dylib` | Linux / macOS Loader |
| `BuildingBlocks/AngelScript.dll`、`libAngelScript.so`、`libAngelScript.dylib` | 启用脚本时的配套 CKAngelScript |
| `ModLoader/BML.abi.json` | 运行时目标及扩展 ABI 标识 |
| `ModLoader/Fonts`、`ModLoader/Licenses` | 字体和依赖许可 |
| `ModLoader/Mods` | 用户安装的 Mod |
| `ModLoader/Configs` | Loader 创建并保留的用户配置 |

按包内文件名安装，不要改名假装另一个平台。Loader 在 **Managers**，不是
BuildingBlocks；移除 BuildingBlocks 中遗留的重复 Loader。CK2、VxMath、SDL 由
Player 提供，不要把另一份 SDL 或 SDK 引擎库复制进游戏。

SDK 放在开发工作区，描述位于 `share/BML/BML.abi.json`，不能当运行时覆盖游戏。
POSIX 保留 Player 和工具的可执行权限。macOS SDK/运行时 CMake 完成所需加载路径及
临时签名修正；发行签名和系统安全许可仍需单独验证。

启动 Player 后检查 `ModLoader/ModLoader.log`，确认初始化、目标、版本和 Mod 列表。
无脚本构建会报告脚本不可用；放入脚本文件不能启用未编译的功能。

## 安装 Mod

- 原生：把 `Name.bmodp` 与它自己的 `Name.bmodp.abi.json` 一起放进
  `ModLoader/Mods`，或安装包含二者和资源的 ZIP。各平台统一使用 `.bmodp` 扩展名。
- 脚本：安装单个 `Name.mod.as`、入口目录或 ZIP。目录/ZIP 须恰有一个
  `*.mod.as` 入口；辅助源码使用普通 `.as`。需要脚本版 BML 和配套 CKAngelScript。
- 保留资源相对目录与精确大小写，不安装补全声明、编译缓存、整份 SDK 或重复 Mod ID。

旧 SDL `BMLEntry`/`IMod` 产物需[迁移和重编译](native-mod-migration.md)。不要用别的
Mod 的 JSON 绕过 ABI 校验。目标、协议、选择的扩展 ABI 是兼容门槛；SDK 内容哈希用于
诊断，不表示可以混用引擎 ABI。

## 配置、字体与输入

在 Mod 菜单编辑已声明配置，更新时保留 `ModLoader/Configs`。文本和路径使用 UTF-8。
内建 ImGui 字体图集、游戏菜单字体和引擎 GUI 文字职责独立，`ModLoader/Fonts` 不是
所有游戏文字的统一字体替换入口。

SDL 平台后端负责文本组合与事件；Windows 保留 IMM/TSF，其他平台可使用系统原生
候选窗。焦点、组合、全屏和手柄仍需各平台实机验收。用 `bml system` 和日志收集诊断。

## 控制台

按 `/` 打开命令栏，`help` 查看当前安装可用的命令。支持引号、变量、别名、历史、
`;`、`&&`、`||` 和管道。`$?` 是上个命令的状态；不要把 shell 状态与 BML API 错误码
混为一谈。

```text
set NAME value
set -U NAME value
alias hi='echo hello'
history
bml system
script status
script diag your.mod.id
```

Tab 补全，Up/Down 回溯历史，Ctrl+R 搜索；Ctrl+A/E 到行首/行尾，Ctrl+U/K/W 剪切，
Ctrl+Y 粘贴，Ctrl+L 清除消息板。不完整的引号或尾随 `&&` 会进入续行，整条逻辑命令保留
物理行布局。消息板可选中和复制，点击消息板不会关闭当前输入。

变量、别名、历史保存在 `ModLoader/CommandBar.shell.json` 和
`ModLoader/CommandBar.history`。`BML.cfg` 的 `CommandBar`、`CommandBarTheme`
控制保留输入、补全、历史建议、滚动消息和配色；颜色使用 `#RRGGBB` / `#RRGGBBAA`。
更多语法见[英文命令说明](https://doyagu.github.io/BallanceModLoaderPlus/using-bml/#command-line)。

## 更新、恢复和卸载

按 [Updater 和 doctor](updater-sdl.md)执行更新、诊断、回滚。更新或回滚前关闭 Player。
通道按目标区分并校验签名，保留用户 Mod 和配置；doctor 不能替代游戏启动验收。
手动更新也必须使用匹配的完整运行时，不用 SDK 覆盖游戏。

卸载时关闭 Player，移除 Managers 中的 Loader 和它拥有的运行时文件；保留
`ModLoader` 可以保留 Mod/设置。其他游戏内容依赖脚本插件时不要删除它；不要删除
Player 的 CK2、VxMath 或 SDL。

## 排错

| 现象 | 检查 |
| --- | --- |
| 没有 BML 启动日志 | 根目录、Managers 库、平台/架构、重复旧 Loader |
| 原生模块被拒绝 | ABI 伴随文件、`bml.mod` 协议、provider 版本、扩展 ABI |
| 缺库或镜像架构错误 | doctor、运行时包、可执行权限和宿主依赖 |
| 脚本不可用 | 描述中的 `scripts`、BuildingBlocks 插件、`script status`/`diag` |
| 找不到资源/include | 文件名大小写、相对路径、归档层级和实际打包资源 |
| 输入异常或第二份 SDL | 恢复配套 Player/SDK 部署，不随 Mod 放入另一个 SDL |
| 更新中断 | 保留事务状态，按更新器指南诊断和回滚 |

反馈时提供目标描述、BML/Player 版本、日志、复现步骤、实际安装的包和实机运行范围。
交叉编译成功不代表游戏功能验收成功。
