# 创建 Mod

使用与 Player 匹配的 SDL SDK，把 SDK 放在游戏目录之外，不包含 BML 私有 `src/`
头文件。功能适合脚本时从 Script Mod 开始；需要原生库、Engine/ImGui 扩展或已测量的
性能需求时选择原生 Mod。

| 路线 | 入口 | 构建与部署 |
| --- | --- | --- |
| 脚本 | `[bml.mod]` 类和 `BML::ModContext` 回调 | 配套 CKAngelScript；`.mod.as`、目录或 ZIP |
| 原生 C++20 | `BML::Mod::Instance` + `BML_DEFINE_MOD` | 版本化 C 入口；`.bmodp` + ABI 伴随文件 |
| 原生 C11 | `Mod.h` 的 `BML_GetModInterface` | 同一 C 协议和伴随文件 |
| 原生/脚本服务 | 生成式 IMC RPC 和 Topic | `.imc`、审查后的 lock、语言绑定 |
| 原生 provider C 表 | `Interface.hpp`、明确 provider 依赖 | 进程内 ABI 和 provider 寿命 |

`IMod`、`IBML`、`IConfig`、`IProperty`、`ICommand`、`ILogger`、`IMessageReceiver`
均已移除。旧 SDL 源码先按[迁移指南](native-mod-migration.md)处理。

## 所有平台使用同一工作流

Python 3.10+ 是共同入口，以下写法不依赖 shell 的续行规则：

```text
python /path/to/sdk/scripts/bml.py new script example.hello
python /path/to/sdk/scripts/bml.py new native example.native
```

选择一条创建命令，进入生成的项目目录：

```text
python bml.py build --bml-sdk /path/to/sdk
python bml.py run --bml-sdk /path/to/sdk --ballance-root /path/to/game
python bml.py pack
```

Linux/macOS 也提供可执行的 `scripts/bml` 和项目 `./bml`；Windows 提供
`scripts/bml.cmd` 和项目 `bml.cmd`。替换示例路径并为含空格的路径加引号。
原生项目需要 CMake 和 C++20 编译器，脚本项目不需要原生编译器。

工作流在 `.bml/` 保存本地设置。`build` 只准备产物；`run` 准备、部署受管理副本并启动
Player。脚本 `run` 会持续同步保存的改动，触发游戏内热重载。原生模块为下次 Player
运行重编译和部署，不会替换运行中的二进制。`pack` 在 `dist` 输出发布归档。

`--configuration` 选择 SDK 支持的配置，必须与 SDK 匹配；`--package-root` 增加依赖
前缀，`--generator` 选择 CMake 生成器。普通 C ABI Mod 不需要 CK2/ImGui 扩展头文件；
选择扩展时再提供匹配的依赖开发包。

游戏根目录包含 `base.cmo` 和 `Bin/Player` 或 `Player`；Windows 为 `Player.exe`。
保留的参数名是 `--ballance-root`，传游戏根目录而不是 `Bin`。原生部署会核对 SDK 与游戏
的 ABI 描述。

## 已有项目

已有工程可以采用工作流，不替换源码或 CMake：

```text
python /path/to/sdk/scripts/bml.py init example.existing --project /path/to/project
```

无法推断脚本目录时传 `--kind script`。单文件脚本先放入独立开发目录。
添加的文件包括 `bml.mod.json`、`bml.py`、`bml`、`bml.cmd` 和被忽略的本地设置。
原生 target 或文件名无法推断时使用 `--target`、`--artifact`。新建项目不会覆盖已有目录。

## 保留手工 CMake 流程

```cmake
cmake_minimum_required(VERSION 3.25)
project(MyMod LANGUAGES CXX)
find_package(BML CONFIG REQUIRED)
bml_add_mod(MyMod src/MyMod.cpp)
bml_install_mod(MyMod)
```

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/sdk -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --install build --config Release --prefix /path/to/game/ModLoader
```

`bml_add_mod` 链接核心 `BML::BML`，选择目标平台的模块类型并生成 ABI 伴随文件。
直接使用 CK/Vx/ImGui 时再显式链接 `BML::Engine`、`BML::ImGui`。这些扩展的编译器、
标准库、运行库、配置、SDL 和引擎必须与 Player 匹配。复用 Player 的 SDL；macOS 所需
加载路径及签名处理由 SDK CMake 在构建时完成。

## 示例和进阶 profile

SDK 安装 `examples/native-mod`、`native-c-mod`、`native-interface-provider`、
`native-interface-consumer`；启用脚本时还安装 `script-mod`。基础 C++ 示例使用
`ctx.Config()`、`ctx.Input()`、`ctx.Timers()`、`ctx.Commands()`。服务视图不拥有宿主；
Registration 成员表达注册所有权。检查 Result，或使用 `Take()` 让当前模块回调屏障
报告失败。

```text
python /path/to/sdk/scripts/bml.py new native example.provider --profile interface-provider
python /path/to/sdk/scripts/bml.py new native example.consumer --profile interface-consumer --provider-id example.provider
python /path/to/sdk/scripts/bml.py new native example.imc --profile imc-provider
```

原生 `.bml-interface` schema 使用 `python bml.py interface update` 更新有意的
兼容变更，`interface check` 校验 lock。IMC `.imc` 则使用已配置的
`bml_update_imc_locks` CMake target 更新锁。普通类型化原生/脚本服务使用 [IMC](imc-author-guide.md)，
进程内基础设施使用原生 C 表；脚本确实需要无法由普通服务表示的原生类型时才写
CKAngelScript 扩展。

## 发布与验收

原生 CLI `pack` 只归档模块和 ABI 描述；额外资源由项目自己的打包步骤处理，
可参考仓库的 BallSticky/BMLModuls 包。脚本 `pack` 包含源码与资源。

- 原生：一起分发 `Name.bmodp`、`Name.bmodp.abi.json`，或将两者一起放入 ZIP。
- 脚本：`.mod.as`、目录或 ZIP。不包含补全 stub、`.bml`、编译缓存、本机路径或凭据。
- 资源：名称大小写精确、相对路径可移植、文本使用 UTF-8。路径优先使用 `/`，不假设
  文件系统忽略大小写或 `wchar_t` 只有两个字节。
- 移除开发部署副本后测试最终归档。覆盖依赖、输入/UI 所有权、清理、世界重置和重载失败。
  构建通过与[游戏验收](validation.md)分别记录。

详见[原生 API](native-mod-api.md)、[脚本教程](script-mod-tutorial/README.md)、
[Behavior](behavior-authoring.md)和 [API 路线](native-api-routes.md)。
