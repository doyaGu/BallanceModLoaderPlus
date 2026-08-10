# 为 BML+ 开发 Mod

编写 Mod 时应使用已发布的 BML+ SDK。只有修改 Loader、SDK、脚本宿主或内建服务时
才需要构建 BML+ 仓库。

不确定选择哪条路线时，先从脚本 Mod 开始。它不需要 C++ 构建，并具有最短的编辑、
热重载和诊断循环。只有明确需要原生 Hook、原生内存、生成式 IMC Provider 或
性能敏感循环时，再进入原生路线。

## 选择开发路线

| 路线 | 适用场景 | 主要代价 |
| --- | --- | --- |
| 脚本 Mod | 希望快速修改和测试，需要命令、配置、UI、玩法脚本或 CKAngelScript 引擎访问，但不想配置 C++ 构建。 | 不能提供自定义 IMC Provider，也不应承担不安全 Hook 或性能敏感的原生循环。 |
| 原生 Mod | 需要 C++20、直接 Virtools 集成、原生 Hook、生成式 IMC Provider，或必须严格控制热路径。 | 需要兼容 MSVC ABI 的 Win32 构建，并明确处理 DLL ABI 和所有权。 |
| 原生插件加 CKAngelScript 扩展 | 不安全或性能敏感的服务由原生代码负责，但脚本需要小型、类型化的控制接口。 | 原生插件必须通过 CKAngelScript 注册并维护该脚本接口。 |

不要在 BML 中重新封装一套 CKAngelScript 已有的 Scene、Behavior、Component、
Message 或 Async API。CK/Vx 操作使用 CKAngelScript；Mod 身份、生命周期、配置、
命令、Loader UI 和 Mod 级服务使用 BML+。

## 开始编写脚本 Mod

1. 在 `ModLoader/Mods` 中打开 PowerShell，用 SDK 模板创建 Mod：

   ```powershell
   & "<BML-SDK>/scripts/New-BMLScriptMod.ps1" `
     -Id "yourname.my-mod" -Name "My Mod" -Author "Your Name"
   ```

   命令会创建目标目录、合法的类名和入口文件名，并写入你的元数据。也可以手动复制
   `templates/script-mod-template`。

2. 打开生成的目录和 README。
3. 确认配套的 `BuildingBlocks/AngelScript.dll` 已安装。
4. 不修改生成的源码，直接启动 Player，同时确认游戏内问候语和
   `ModLoader/ModLoader.log` 中的加载日志。
5. 保持生成的 id 稳定；以后修改 id 会成为另一个 Mod，并且需要重启 Player。
6. 保持 Player 运行。保存已加载目录中的源码会自动热重载；只有新增入口、修改 id
   或修改依赖时才需要重启。
7. 在 Mod 目录中运行 `scripts/Pack-BMLScriptMod.ps1`；产物位于
   `dist/<目录名>.zip`。再在没有开发目录副本的环境中测试该 zip。

先阅读[脚本 Mod 教程导读](https://doyagu.github.io/BallanceModLoaderPlus/zh-CN/script-mod-tutorial/)，
需要准确声明时使用其中的 API 参考。支持脚本 Mod 的 SDK 会把同一套页面安装到
`share/BML/docs/zh-CN`。

## 开始编写原生 Mod

1. 在存放源码项目的目录中打开 PowerShell，用 SDK 模板创建 Mod：

   ```powershell
   & "<BML-SDK>/scripts/New-BMLNativeMod.ps1" `
     -Id "yourname.my-mod" -Name "My Mod" -Author "Your Name"
   ```

   命令会让 CMake target、C++ 类名、源文件名和元数据保持一致。也可以手动复制
   `templates/native-mod-template`。

2. 打开生成的 README。使用兼容 MSVC ABI 的 Win32 目标配置项目，并让
   `CMAKE_PREFIX_PATH` 指向解压后的
   BML+ SDK。
3. 让 `VIRTOOLS_SDK_PATH` 指向 Virtools SDK 2.1。
4. 构建 Debug Mod，并安装到 `ModLoader/Mods`。
5. 启动 Player，在 `ModLoader/ModLoader.log` 中确认生成 Mod 的加载日志。
6. 构建 Release，并测试准备发布的同一个产物。

SDK 的 CMake 入口为：

```cmake
find_package(BML CONFIG REQUIRED)
bml_add_mod(MyMod MyMod.cpp)
bml_install_mod(MyMod)
```

增加所有权、回调、UI 或跨 Mod 服务前，先阅读[原生 Mod API 总览](native-mod-api.md)。

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
| 类型化请求与响应、异步结果、Topic、版本化数据或高吞吐 | 由原生 Mod 实现的生成式 IMC 接口 |
| BML+ 内建的运行时、玩法、事件、UI 或速通服务 | 对应语言已有的 BML+ 类型化 API |
| CKAngelScript runtime script 或 Component 之间通信 | 在执行模型合适时使用 CKAngelScript `Message` 或 `Async` |

不要自定义 JSON 消息格式，也不要手写字段编号。编写 `.imc` 接口，由
`bml_add_imc_interface` 生成 C++ 绑定，并让 schema lock 与接口一起维护。参见
[跨 Mod 通信](imc.md)和[创建类型化 IMC API](imc-author-guide.md)。

脚本 Mod 可以使用 BML+ 类型化脚本 API 和 DataShare。自定义 IMC Provider 仍由
原生 Mod 提供。如果脚本必须调用原生服务，应由该原生插件提供小型、类型化的
CKAngelScript 扩展。

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
- 测试实际发布的产物，而不只是开发目录或 Debug DLL。
- Mod README 应写明依赖、支持版本、安装、配置和有效的问题反馈方式。
