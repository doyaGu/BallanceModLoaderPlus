# 构建和参与 BML+

Mod 作者通常使用已安装 SDK 和[开发工作流](modding.md)。本文面向修改 Loader、SDK、
绑定或工具的贡献者。

## 环境与引擎选择

准备 CMake 3.25+、C++20 和 Python 3.10+。按目标 Ballanced Player 锁定的版本构建并
安装 NeMo2，包含 CK2、VxMath 动态库和开发文件。引擎、Player 和 BML+ 使用同一
SDL3 开发包。完整安装对应的 Ballanced 构建也会导出 NeMo2 的 CK2 和 VxMath 包。
配置前初始化 BML+ 的子模块。

```sh
git submodule update --init --recursive
cmake -S . -B build/sdl-release -DBML_RUNTIME=sdl \
  -DCMAKE_PREFIX_PATH="/path/to/engine-sdk;/path/to/SDL-prefix" \
  -DCKANGELSCRIPT_ROOT=/path/to/CKAngelScript \
  -DCMAKE_BUILD_TYPE=Release -DBML_BUILD_MODS=ON -DBML_BUILD_TESTS=ON
cmake --build build/sdl-release --config Release --parallel
```

BML+ 直接复用父 CMake 工程已提供的 CK2、VxMath 和 SDL3 目标。独立构建时查找它们的
CMake 安装包，也可以用 `CK2_DIR`、`VxMath_DIR`、`SDL3_DIR` 单独指定。当前 Ballanced
基线使用 SDL 3.4.20。

默认启用脚本。`BML_BUILD_SCRIPT_RUNTIME=ON` 会从 `CKANGELSCRIPT_ROOT` 构建使用
同一引擎目标的 CKAngelScript。无脚本版本使用独立目录并传
`-DBML_ENABLE_ANGELSCRIPT=OFF`。只有头文件不等于部署了脚本运行时。

编译器、架构或配置改变时新建构建目录。本分支只接受 `BML_RUNTIME=sdl` 和动态宿主，
会拒绝静态集成及旧 `VIRTOOLS_SDK_PATH` 缓存。多配置生成器使用 `--config Release`、
`ctest -C Release`；Windows 选择 Player 的实际架构，不要无条件指定 `-A Win32`。

## 安装组件

```sh
cmake --install build/sdl-release --config Release --prefix /path/to/sdk
cmake --install build/sdl-release --config Release \
  --prefix /path/to/disposable-game --component BMLRuntime
cmake -E copy_directory /path/to/sdk/Mods /path/to/disposable-game/ModLoader/Mods
```

第一条建立开发安装树；`BMLRuntime` 只把运行时组件装进预先准备好的匹配 Player
目录。启用 `BML_BUILD_MODS=ON` 后，开发安装树的 `Mods` 目录还包含仓库内 Mod 的
产物；把这些包及 ABI 伴随文件复制到游戏的 `ModLoader/Mods`。
当前没有独立的 `BMLMods` 安装组件。
CK2、VxMath、SDL 由 Player 提供。macOS 的运行时 stage 需包含对应宿主 SDL，供
加载路径规范化和签名使用；使用完整 Player 安装的临时副本，不用 SDK 库覆盖游戏。
打包通过组件安装清单选择属于该组件的文件。

## 实际运行流程与归属

```text
Ballanced SDL Player / CK2
  ModManager：CK 生命周期、ImGui 帧、靠后的 PostProcess、Render 通知
    ModContext：组合服务，借用 CK 管理器
      ModLoader：ModRecord 身份、依赖、实例和调用门
        NativeMod：C 定义、实例、回调
        ScriptMod：CKAngelScript 运行时和每 Mod 上下文投影
        内建 Mod：组合注册与本地回调
```

引擎使用默认物理调度。BML 在靠后的 PostProcess 推进 Behavior 安全点，执行既有服务
和 Mod 工作，最后完成延迟输入清理。它仍位于 `CKContext::Process` 内、该调用返回前。
Render 独立调度，待处理跳帧在下一次 Render 消费；Player 不依赖 BML。

`IMod`、`IBML`、`IMessageReceiver`、`IConfig`、`IProperty`、`ICommand`、`ILogger`
连同内部实现都已移除。`ModRecord` 是组合的注册身份，不是改名的 Mod 继承接口。
公开 `Mod::Instance` 的虚调用仅发生在 Mod 模块内。

| 改动 | 源码归属 | 定向验证 |
| --- | --- | --- |
| CK 插件入口与管理器生命周期 | `src/BML.cpp`、`src/Loader/ModManager.*` | 构建、实机启动停止 |
| 发现、依赖、卸载及回调归属 | `src/Loader/ModLoader.*`、`NativeMod.*`、`ModRecord.*` | 注册表、依赖及 Player 清理/重载 |
| 运行时组合与借用 | `src/Loader/ModContext.*` | 服务测试、脚本开/关、关闭流程 |
| C 记录与模块内 C++ SDK | `include/BML/`、`src/Api/`、`cmake/` | C/C++ ABI、安装及移动后 SDK 消费者 |
| Behavior 图、执行、世界寿命 | `src/Behavior/`、`src/Api/Behavior/` | Behavior 与对象引用测试 |
| 脚本 VM、投影及引擎便捷调用 | `src/AngelScript/` | 绑定声明、脚本和 Player 测试 |
| 输入拦截与 Render 请求 | `src/Hooks/` | 输入阶段、借用、Process/Render 分离 |
| 命令、配置、定时器、IMC、DataShare | `src/` 对应目录 | 模块契约、回调和清理 |
| 字体、控制台、菜单、HUD | `src/UI/`、`Console/`、`ModMenu/`、`HUD/` | 无窗口测试及实机画面/输入 |
| SDK、CLI、更新工具 | `scripts/`、`tools/`、`tests/sdk`、`tests/updater/sdl` | 包、ABI、进程及签名事务样例 |

`ScriptEngineCalls` 仅在启用脚本时编译，在内建 Mod 的 Behavior Session 中执行便捷
引擎操作；每 Mod 的 Behavior 作者接口保留独立归属。RefCount 仅属于 DataShare 私有
实现。不再存在 `ExecuteBBAdapter`、`ScriptFacadeAccess` 或旧 I* 兼容层。

## 验证

```sh
ctest --test-dir build/sdl-release -C Release --output-on-failure --parallel 6
cmake -S tests/sdk -B build/sdk-contracts
ctest --test-dir build/sdk-contracts --output-on-failure
cmake -S tests/ui -B build/ui-framework
cmake --build build/ui-framework --config Release
ctest --test-dir build/ui-framework -C Release --output-on-failure
```

独立 SDK 和 UI 框架契约测试不需要引擎。默认测试包含 UI 场景目录和 Session 检查；
无窗口 Bui 测试和带测试代码的 Loader 另需 `BML_BUILD_UI_AUTOMATION=ON`。
使用已安装的引擎包时，完整测试还会用同一组 CK2、VxMath 和 SDL3 包构建移动后的
BML SDK 消费者。
无脚本配置另行构建测试。进程管理与更新器测试会检查 Player 进程，使用串行调度。

[Player 验收](https://github.com/doyaGu/BallanceModLoaderPlus/tree/sdl/tests/player/sdl)
需要桌面和匹配的游戏资源。运行器使用临时副本并比较 Mod 原始/部署后哈希，不能靠修改
Mod 二进制修补 SDK 重定位。`--benchmark-api` 测量 API 耗时，不作为功能成败阈值。
[验证记录](validation.md) 区分已运行目标和待完成内容。

## 公开变更与生成文件

保持 C 表前缀、调用约定、记录所有权、借用寿命及错误码。C++ 值和回调留在模块内；
Engine/ImGui 扩展另匹配对应 C++ 依赖 ABI。内部重构不应隐式改变事件名称或调用顺序。

修改 IMC `.imc` 或原生 interface schema 后，用生成器或工作流更新相邻 lock，并一起
审查源文件、lock 和生成文件；不要手改生成编解码器。CMake 管理的 IMC lock 使用
`bml_update_imc_locks`，作者项目的 interface lock 使用 `bml interface update`。

文档使用仓库已声明的依赖，分别严格构建两种语言：

```sh
python -m pip install -r requirements-docs.txt
python -m mkdocs build --strict --config-file mkdocs.yml
python -m mkdocs build --strict --config-file mkdocs.zh-CN.yml
```

同步更新对应语言、示例、声明和安装布局。原生示例按已安装 SDK 编译，脚本按实际注册
的属性与方法验证。补全 stub 不能放进运行时脚本包。历史设计稿不是当前作者接口契约。

文档来源、脚本声明再生成和示例校验见[文档维护说明](https://github.com/doyaGu/BallanceModLoaderPlus/blob/sdl/docs/README.md)。
