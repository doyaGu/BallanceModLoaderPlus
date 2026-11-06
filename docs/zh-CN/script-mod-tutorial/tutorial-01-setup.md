# 01 环境搭建

本教程针对本仓库 `sdl` 分支的 **Ballanced 动态 Player**。原 Windows Ballance
及其原生 Mod ABI 属于另一条分支。先完成安装和最小脚本验证，再学习引擎对象。
构建覆盖与未验收目标见[验证状态](../validation.md)。

## Player、BML+ 与 CKAngelScript

三者的职责不同：Player 驱动游戏，BML+ 管理 Mod 及服务，CKAngelScript 提供脚本
引擎与 CK/Vx 绑定。正式脚本包需要 `BML_ENABLE_ANGELSCRIPT=ON` 的运行时。
关闭脚本支持的包仍能运行原生 Mod，但无法编译 `.mod.as`。

```text
游戏根目录（包含 base.cmo）
  Bin/Player                  或根目录 Player
  Managers/<BML Loader>       Mod 生命周期与服务
  BuildingBlocks/<AngelScript 插件>  CKAngelScript 共享运行时
  ModLoader/
    BML.abi.json
    Fonts/
    Mods/
    Configs/
    ModLoader.log             首次成功启动后生成
```

对应平台的文件名为：

| 平台 | Loader（Managers） | CKAngelScript（BuildingBlocks） |
| --- | --- | --- |
| Windows | `BMLPlus.dll` | `AngelScript.dll` |
| Linux | `libBMLPlus.so` | `libAngelScript.so` |
| macOS | `libBMLPlus.dylib` | `libAngelScript.dylib` |

Windows Player 带 `.exe`。目录名、资源名和扩展名须保持大小写；Linux 文件系统
尤其不能依赖 Windows 的不区分大小写行为。路径和脚本源码使用 UTF-8。

## 安装配套运行时

1. 准备能正常运行的完整 Ballanced SDL 游戏目录，先确认它能加载原有资源。
2. 选择与 Player 平台、架构、动态链接方式和引擎 ABI 匹配的 BML+ 运行时包。
   不要把原 Windows 包或另一个架构的文件混进来。
3. 把包内 `Managers`、`BuildingBlocks`、`ModLoader` 等目录合并到游戏根目录。
   `BMLPlus-<版本>/Managers` 多套一层时，Player 不会找到它。
4. 保留自己的配置和 Mods。不要另复制 SDK 的 SDL、CK2 或 VxMath 动态库进游戏。
5. 启动 Player，检查 Loader 界面和 `ModLoader/ModLoader.log`。日志应表明 BML
   启动成功，脚本宿主没有 `ckas-host` 错误。

详细安装和字体、输入法、权限处理见[使用 BML+](../using-bml.md)。
SDK 是开发包，放在源码工作区，不要把它当运行时解压进游戏。

启动失败时先运行随包工具（扁平布局去掉 `Bin/`；Windows 使用 `Updater.exe`）：

```sh
./Bin/Updater doctor --game-root "/path/to/game"
```

| 现象 | 检查 |
| --- | --- |
| 没有 Loader 界面或日志 | Loader 是否在 `Managers`，目录是否多套一层 |
| `ckas-host` 错误 | `BuildingBlocks` 中的脚本插件是否缺失或与引擎不匹配 |
| 动态库或架构错误 | Player、Loader、脚本插件是否属于同一目标包；不要一律安装 x86 运行库 |
| POSIX 无法启动 Player | 文件执行权限和依赖路径是否保留 |
| 脚本编译失败 | 先修日志中的第一个编译错误，再看后续连带报错 |

## 用 SDK 创建项目

需要 Python 3.10+。用 `python` 或系统对应的 `python3` 运行，以下流程跨平台通用：

```sh
python "/path/to/BML-SDK/scripts/bml.py" new script yourname.my-mod
cd MyMod
python bml.py build
python bml.py run --ballance-root "/path/to/game"
```

也可以用 POSIX `./bml` 或 Windows `bml.cmd`。`--ballance-root` 仍沿用这个参数名，
填写包含 `base.cmo` 的根目录，而不是 `Bin`。首次运行会保存本机设置。

先不改模板源码。游戏内出现问候语，日志中出现加载消息，才算发现、编译和 `OnLoad`
调用都成功。保持 `run` 与 Player 运行，保存源码会同步到受管理副本，并为已加载
的目录 Mod 触发热重载。新增 Mod、修改 id 或依赖后须重启 Player。

加载失败时，在 BML 控制台依次检查：

```text
script status
script diag yourname.my-mod
script logs error
```

打包使用 `python bml.py pack`，输出 `dist/` 下的 ZIP。开发工作区、已部署目录和
发行 ZIP 不要以同一个 id 同时装入 `ModLoader/Mods`。

## 手动创建脚本

不使用 SDK CLI 时，最小入口是 `ModLoader/Mods/HelloMod.mod.as`。
只有 `*.mod.as` 才是 Mod 入口，普通 `.as` 用作被包含的辅助源码。
Windows 请显示文件扩展名，避免误保存为 `.mod.as.txt`。

目录包或 ZIP 中只能有一个入口；资源跟随各自的 Mod 根目录，不能依赖当前工作目录。
具体布局见[脚本入门](README.md)与[发布](tutorial-21-publish.md)。

## 编辑器补全

VS Code 可使用 [AngelScript Language Server](https://github.com/sashi0034/angel-lsp)。
把源项目目录作为工作区，将**配套 SDK** 的 `docs/api/as.predefined` 复制到工作区根目录：

```text
MyMod/
  as.predefined
  MyMod.mod.as
  bml.mod.json
  bml.py
```

没有本地 SDK 时，可从[API 参考](../api.md)下载声明，并确认其与运行时相符。
`as.predefined` 包含 CKAngelScript、BML+ 和 ImGui 的编辑器声明。
它不在 Player 内执行，禁止 `#include` 到 Mod，也不要装进发行包。
CLI 会排除这类编辑器文件。

试着补全 `BML::UI::AddMessage`、`ctx.LogInfo`。属性按 `ctx.IsInLevel`、
`input.IsValid` 使用，不加函数调用括号。编辑器检查不能替代真实 Player 编译。

## 完成标准

继续下一章前，应能完成以下步骤：

- 匹配的 Player 与 Loader 正常启动，日志无宿主加载错误。
- 最小脚本显示问候语，`script status` 能找到它。
- 修改问候语后成功热重载，日志能定位编译错误。
- 编辑器能识别 BML 名称，发行 ZIP 不含补全声明。

下一节：[02 第一个 Mod](tutorial-02-hello-mod.md)。
