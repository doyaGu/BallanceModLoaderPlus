# 为 BML+ 开发 Mod

编写 Mod 时应使用已发布的 BML+ SDK。只有修改 Loader、SDK、脚本宿主或内建服务时
才需要构建 BML+ 仓库。

不确定选择哪条路线时，先从脚本 Mod 开始。它不需要 C++ 构建，并具有最短的编辑、
热重载和诊断循环。只有明确需要原生 Hook、原生内存、调用线程 RPC Handler 或
性能敏感循环时，再进入原生路线。

## 选择开发路线

| 路线 | 适用场景 | 主要代价 |
| --- | --- | --- |
| 脚本 Mod | 希望快速修改和测试，需要命令、配置、UI、玩法脚本、CKAngelScript 引擎访问，或不经 C++ 使用生成式 IMC Client/Provider。 | IMC 回调固定在游戏线程；不应承担不安全 Hook 或性能敏感的原生循环。 |
| 原生 Mod | 需要 C++20、直接 Virtools 集成、原生 Hook、调用线程 IMC 执行，或必须严格控制热路径。 | 需要兼容 MSVC ABI 的 Win32 构建，并明确处理 DLL ABI 和所有权。 |
| 原生插件加 CKAngelScript 扩展 | 脚本必须直接借用插件专有的原生对象，或调用无法表示为 IMC Record 的引擎原语。 | 原生插件必须通过 CKAngelScript 注册并维护该脚本接口。 |

不要在 BML 中重新封装一套 CKAngelScript 已有的 Scene、Behavior、Component、
Message 或 Async API。CK/Vx 操作使用 CKAngelScript；Mod 身份、生命周期、配置、
命令、Loader UI 和 Mod 级服务使用 BML+。

## 开始编写脚本 Mod

1. 在平常保存源码的工作区中创建 Script Mod Project：

   ```bat
   "<BML-SDK>\scripts\bml.cmd" new script yourname.my-mod
   ```

   命令会自动推导可读名称与作者，创建合法的类名、入口文件名和项目内
   Developer Workflow。

2. 进入生成目录并运行 `.\bml run`。首次运行会询问 Ballance 目录，确认脚本运行时，
   部署受管副本、启动 Player，并只显示这个 Mod 的新增日志。
3. 编辑时保持 `bml run` 运行。每次保存都会同步到受管副本，Player 原有的热重载
   继续生效。
4. 运行 `.\bml pack` 生成 `dist/<项目名>.zip`，再在未安装开发副本的环境中测试。

已有的目录 Script Mod 可用
`"<BML-SDK>\scripts\bml.cmd" init --project "C:\path\to\mod"` 接入，原有源码不会
被改写。已有单文件 Mod 需先移入独立目录；Player 对该目录的运行行为不变。

先阅读[脚本 Mod 教程导读](https://doyagu.github.io/BallanceModLoaderPlus/zh-CN/script-mod-tutorial/)，
需要准确声明时使用其中的 API 参考。支持脚本 Mod 的 SDK 会把同一套页面安装到
`share/BML/docs/zh-CN`。

## 开始编写原生 Mod

1. 在存放源码项目的目录中打开终端，用 SDK 模板创建 Mod：

   ```bat
   "<BML-SDK>\scripts\bml.cmd" new native yourname.my-mod
   ```

   命令会让 CMake target、C++ 类名、源文件名和元数据保持一致，默认使用 `basic`
   profile。下面三个 profile 会把跨 Mod API 的重复样板一起生成：

   ```bat
   rem 发布类型安全的进程内函数表，并安装独立头文件包。
   "<BML-SDK>\scripts\bml.cmd" new native yourname.value-provider ^
     --profile interface-provider

   rem 消费对应的包；包名、头文件路径和 Traits 名会自动推导。
   "<BML-SDK>\scripts\bml.cmd" new native yourname.value-consumer ^
     --profile interface-consumer --provider-id yourname.value-provider

   rem 定义生成式 IMC RPC provider，并同时生成已审核的 schema lock。
   "<BML-SDK>\scripts\bml.cmd" new native yourname.remote-api ^
     --profile imc-provider
   ```

   `interface-consumer` 必须提供 `--provider-id`；其余 profile 会拒绝这个参数，避免
   拼错命令后静默生成错误依赖。也可以手动复制 `templates` 下的对应目录。

2. 进入生成目录，只运行一条开发命令：

   ```bat
   .\bml run
   ```

   首次运行会询问 Virtools SDK 和 Ballance 目录。之后会选择本机最新的 Visual
   Studio 生成器，配置 Win32，构建并部署
   `RelWithDebInfo`，启动 Player，并在退出后只打印这个 Mod 本次新增的日志。两个
   路径和生成器会缓存到已忽略的 `.bml/settings.json`；之后只需运行 `.\bml run`。
3. 修改生成的源码，然后重复同一条命令。interface provider 修改
   `api/*.bml-interface`；确认兼容变化后运行 `.\bml interface update`。生成的头文件
   不需要手工维护。
4. 原生接口会让 C++ 对象
   跨越 DLL 边界，因此原生 Mod 必须与装载它的 Loader 链接同一套 MSVC 运行库。
   `BMLPlus-<version>.zip` 中的运行时基于 Release 版 MSVC 运行库构建，Debug
   `.bmodp` 与之不具备 ABI 兼容性；`RelWithDebInfo` 在使用兼容运行库的同时保留调试
   信息。`bml_add_mod` 会把 Mod 的运行库固定为所配置 SDK 使用的那一套，并在
   `CMAKE_MSVC_RUNTIME_LIBRARY` 与之冲突时直接让配置失败，所以两者不会在无声中错位。
   `BMLPlus-SDK-<version>-Debug.zip` 是受支持的例外。它包含 Debug 版
   `bin/BMLPlus.dll` 及其 `.pdb`，只要同时用这个 Debug Loader 覆盖
   `BuildingBlocks/BMLPlus.dll`，Debug Mod 就是有效的。Loader 和所有已安装的原生
   Mod 必须处于同一侧；测试待发布产物前要换回 Release 版 Loader。
5. 发布前运行 `.\bml run --configuration Release` 并测试该产物，然后运行
   `.\bml pack` 将 Release `.bmodp` 复制到 `dist`。只有在 CI 或排查构建系统本身时，
   才需要使用生成 README 中的手工 CMake 命令。

SDK 的 CMake 入口为：

```cmake
find_package(BML CONFIG REQUIRED)
bml_add_mod(MyMod MyMod.cpp)
bml_install_mod(MyMod)
```

增加所有权、回调、UI 或跨 Mod 服务前，先阅读[原生 Mod API 总览](native-mod-api.md)；
某项能力有不止一种写法时，参见[原生 API 该走哪条路](native-api-routes.md)。

## 两条路线共同遵守的规则

- 保持 Mod id 稳定。其他 Mod 会用它声明依赖和服务所有权。
- 在加载前声明依赖，不要在逐帧回调中临时寻找必需的 Mod。
- CK 借用对象不转移所有权；关卡和对象变化后要重新验证。
- 限制逐 Tick、渲染、引擎 Hook 和同步 RPC 中的工作量。
- 开发期间记录一条明确的启动日志，并在干净的 `ModLoader/Mods` 目录中测试发布包。
- 在 Mod README 中写明所需的 BML+、CKAngelScript、原生插件和依赖版本。

## 选择跨 Mod 通信方式

| 需求 | 使用 |
| --- | --- |
| 同一进程内少量具名标量或字节值 | DataShare |
| 原生和/或脚本 Mod 之间的类型化请求与响应、异步结果、Topic 或版本化数据 | 生成式 IMC 接口；两种语言都可以消费或提供 |
| BML+ 内建的运行时、玩法、事件、UI 或速通服务 | 对应语言已有的 BML+ 类型化 API |
| CKAngelScript runtime script 或 Component 之间通信 | 在执行模型合适时使用 CKAngelScript `Message` 或 `Async` |

不要自定义 JSON 消息格式，也不要手写字段编号。编写 `.imc` 接口，由
`bml_target_imc_api` 生成 C++ 和/或 AngelScript 绑定，并让 schema lock 与接口一起维护。参见
[跨 Mod 通信](imc.md)和[创建类型化 IMC API](imc-author-guide.md)。

只有脚本必须直接借用插件专有原生对象，或调用无法表示为类型化 IMC 数据的引擎原语时，
才增加 CKAngelScript 扩展。常规 native/script 服务边界应共用一份生成式 IMC 契约，
不要再维护第二套手写 API。

## 性能与所有权

BML 回调和同步 IMC handler 通常在游戏线程执行。稳定的查询结果应缓存为 id 或可
重新验证的引用；准备工作移出热回调；Topic 使用有界容量并监控丢弃数。

需要修改引擎内部、高频执行、控制原生内存或提供高吞吐服务时使用原生 Mod。策略、
配置和低频控制调用可以留在脚本层，使开发和调试更直接。

## 发布

- 原生 Mod 通常发布为 `.bmodp`，并同时导出 `BMLEntry(IBML*)` 和
  `BMLExit(IMod*)`。
- 脚本 Mod 发布为单个 `*.mod.as` 文件，或包含且只包含一个入口的 zip；
  `.bmodp` 只属于原生 Mod。
- 测试实际发布的产物，而不只是开发目录或 `RelWithDebInfo` 构建。
- Mod README 应写明依赖、支持版本、安装、配置和有效的问题反馈方式。
