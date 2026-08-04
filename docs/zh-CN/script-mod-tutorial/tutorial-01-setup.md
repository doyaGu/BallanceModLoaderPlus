# 01 环境搭建

本节的目标：把写脚本 mod 需要的东西全部装好，确认 BML 在 Ballance 里正常工作，确认编辑器能识别 BML 提供的 API。

做完以后，能看到两个现象：游戏画面中上方有 BML 版本字样，`ModLoader/ModLoader.log` 日志文件存在且有内容。

## BML 和 CKAS 是什么

原版 Ballance 不会直接加载 `.mod.as` 脚本。BML 是脚本 mod 的运行环境，通过 Virtools 的 BuildingBlock 机制被 Player.exe 加载，然后接管一部分游戏流程，让外部代码能介入。

CKAS 是 BML 内置的脚本引擎，基于 AngelScript 语言。脚本 mod 用 `.mod.as` 后缀保存在指定目录下，BML 启动时由 CKAS 编译并执行。

它们的关系：

```text
Player.exe  ──加载──->  BMLPlus.dll  ──包含──->  AngelScript.dll (CKAS)
                           │                         │
                           │                         │
                    管理游戏回调              编译并运行 .mod.as 脚本
```

Player.exe 是 Ballance 原版的可执行文件。它加载 `BuildingBlocks/` 目录里所有的 DLL。`BMLPlus.dll` 借此机制启动，然后扫描 `ModLoader/Mods/` 目录，对其中每个 `.mod.as` 文件调用 CKAS 编译并注册。脚本通过 BML 提供的 API 与游戏交互，比如写日志、发送游戏内消息、读按键状态、操作 Virtools 对象。

脚本 mod 的整个生命周期都在游戏进程内。每次要测试改动，需要关闭游戏再重新启动。

## 需要准备的东西

```text
BML 0.3.12（发布包）
VS Code
AngelScript Language Server（VS Code 扩展）
as.predefined（API 描述文件）
```

其中，BML 是运行时必须的。VS Code、AngelScript Language Server 和 `as.predefined` 只在写代码时使用，游戏运行不需要它们。

## 游戏目录

后面提到"游戏目录"，指的是 Ballance 的安装根目录。打开它时，能看到这些内容：

```text
Bin/Player.exe
3D Entities/
BuildingBlocks/
Textures/
```

`Player.exe` 在 `Bin/` 子目录里。接下来解压 BML 时，目标是游戏根目录这一层，不是 `Bin/`。

## 安装 BML

下载玩家用的 BML 发布包，例如：

```text
BMLPlus-v0.3.12.zip
```

把压缩包里的内容解压到游戏目录。解压完成后，应该能看到：

```text
BuildingBlocks/BMLPlus.dll
BuildingBlocks/AngelScript.dll
ModLoader/
```

各部分的作用：

| 路径 | 作用 |
| --- | --- |
| `BuildingBlocks/BMLPlus.dll` | BML 的核心。Player.exe 在启动时自动加载这个 DLL |
| `BuildingBlocks/AngelScript.dll` | CKAS 脚本引擎。BML 用它来编译和运行 `.mod.as` 脚本 |
| `ModLoader/` | BML 的工作目录，日志、配置和 mod 都放在这里 |

这里有一个常见的错误。很多压缩工具解压时会自动创建一个和压缩包同名的子目录：

```text
Ballance/
  BMLPlus-v0.3.12/         <- 多出来的一层
    BuildingBlocks/BMLPlus.dll
```

这是错的。正确的结构是：

```text
Ballance/
  BuildingBlocks/BMLPlus.dll
  BuildingBlocks/AngelScript.dll
  ModLoader/
```

如果发现 BML 没生效，第一步就检查有没有多套目录。

## 启动验证

启动 `Bin/Player.exe`。进入游戏画面后，看画面中上方。能看到：

```text
BML Plus 0.3.12
```

这行字表示 `BMLPlus.dll` 已经被 Player.exe 成功加载，并且初始化没有出错。

看不到这行字时，按下面排查：

| 现象 | 检查 |
| --- | --- |
| 游戏能启动，但没有 `BML Plus 0.3.12` | `BuildingBlocks/BMLPlus.dll` 是否在游戏根目录下的 BuildingBlocks 里 |
| 解压后有 `BMLPlus-v0.3.12/BuildingBlocks` | 多套了一层目录，把里面的内容移到游戏根目录 |
| 启动时报 DLL 或运行库错误 | 安装 Microsoft Visual C++ 2015-2022 x86 运行库 |
| 画面有 BML，但后续脚本功能异常 | `BuildingBlocks/AngelScript.dll` 是否存在 |

## ModLoader.log

BML 启动后会在 `ModLoader/` 目录里生成日志文件：

```text
ModLoader/ModLoader.log
```

以后写脚本时，几乎所有问题都要从这个文件查。脚本有没有被找到、有没有编译错误、有没有执行到指定回调，全部会记录在里面。

建议用 VS Code 或其他文本编辑器打开这个文件，每次重启游戏后刷新查看。Notepad 也行，但它不会自动刷新。

## Mods 目录

BML 扫描脚本 mod 的位置是：

```text
ModLoader/Mods/
```

如果这个目录不存在，手动创建它。脚本 mod 放在这个目录下。

快速入门时，一个脚本 mod 就是一个文件。例如：

```text
ModLoader/Mods/HelloMod.mod.as
```

`.mod.as` 后缀是必须的。BML 通过这个后缀识别哪些文件是脚本 mod 入口。如果后缀不对（比如只写 `.as`），BML 会忽略这个文件。

Windows 资源管理器默认隐藏文件扩展名。这会导致一种隐蔽的错误：你以为文件叫 `HelloMod.mod.as`，实际上它叫 `HelloMod.mod.as.txt`。建议在资源管理器的"查看"选项中打开"文件扩展名"。

## VS Code 和 AngelScript Language Server

在 VS Code 扩展市场搜索并安装 **AngelScript Language Server**（[sashi0034/angel-lsp](https://github.com/sashi0034/angel-lsp)）。它提供语法高亮、自动补全、类型检查和跳转到定义的功能。

安装好之后，用 VS Code 打开 `ModLoader/Mods/` 目录作为工作区。然后把 `as.predefined` 文件放到这个目录里：

```text
ModLoader/Mods/
  as.predefined
  HelloMod.mod.as    <- 后面章节会创建
```

下载 [`as.predefined`](../as.predefined)，放到上面的工作区目录。这个文件描述了 BML 对脚本暴露的所有 API 的类型信息。AngelScript Language Server 读取它之后，编辑器就能识别 `BML::ModContext`、`BML::Logger`、`ImGui::Begin`、`CKDataArray` 等名字，并提供补全和类型检查。

BML 本身不会加载 `as.predefined`，它纯粹是给编辑器用的。

验证方法：在任意 `.mod.as` 文件里输入 `ctx.`（假设 ctx 是 `BML::ModContext` 类型），看能否弹出 `BorrowLogger`、`SendIngameMessage` 等补全项。如果能看到，说明 Language Server 工作正常。

## 安装自检

继续下一节前，逐项确认：

```text
[ ] Ballance 能正常启动
[ ] 游戏画面中上方能看到 BML Plus 0.3.12
[ ] BuildingBlocks/BMLPlus.dll 存在
[ ] BuildingBlocks/AngelScript.dll 存在
[ ] ModLoader/ModLoader.log 存在且有内容
[ ] ModLoader/Mods/ 目录存在
[ ] VS Code 已安装 AngelScript Language Server
[ ] ModLoader/Mods/as.predefined 存在
[ ] 编辑器里输入 ctx. 能看到 BML API 补全
```

全部确认后，环境搭建完成。

-> 下一节：[02 第一个 Mod](tutorial-02-hello-mod.md)
