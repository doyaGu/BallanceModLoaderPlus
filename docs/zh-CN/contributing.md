# 参与 BML+ 开发

本文面向构建和修改 BML+ Loader 本身的贡献者。如果你只想基于已发布的 BML+
SDK 编写 Mod，请阅读[原生 Mod 指南](native-mod-api.md)或[脚本 Mod
教程](script-mod-tutorial/README.md)。编写 Mod 不需要构建本仓库。

## 运行链路

```text
BallancePlayer / Virtools CK2
            |
       BMLPlus.dll
            |
       ModManager              接收 CK 生命周期和引擎回调
            |
       ModContext              发现、依赖排序、所有权和服务
        /         \
   原生 Mod       脚本宿主      IMod 回调 / CKAngelScript 回调
        \         /
       内建服务和生成式 IMC API
```

`ModManager` 将 Virtools manager 生命周期连接到 BML+。`ModContext` 负责 Mod
发现、依赖顺序、回调分发、公共服务和关闭。原生 Mod 通过已安装的 C++ ABI 进入，
脚本 Mod 通过 CKAngelScript 宿主进入。内建生成式 IMC Provider 只投影 Loader 已有
行为，不维护第二份实现。

## 开发环境

BML+ 是 Virtools CK2 使用的 32 位 Windows 插件。构建 Loader 需要：

- Visual Studio 2019 或更新版本及 C++ 工具链；
- CMake 3.14 或更新版本；
- Python 3.10 或更新版本；
- Virtools SDK 2.1；
- 启用脚本支持时使用 CKAngelScript API 6 或更新版本；
- 本仓库的全部 Git 子模块。

克隆仓库及其子模块：

```powershell
git clone --recursive https://github.com/doyaGu/BallanceModLoaderPlus.git
cd BallanceModLoaderPlus
```

如果克隆时没有使用 `--recursive`，在配置项目前先执行：

```powershell
git submodule update --init --recursive
```

## 配置、构建和测试

必须使用 x86/Win32。64 位二进制无法由 Ballance Player 加载，也不能链接
Virtools SDK 的库。

下列命令面向多配置 Visual Studio 生成器。如果 Visual Studio 不是默认生成器，
请通过 `-G` 指定本机安装的版本。使用 Ninja 时，应先进入 x86 Native Tools
环境，省略 `-A Win32`，添加 `-DCMAKE_BUILD_TYPE=Debug`，并为 Release 另建一个
使用 `-DCMAKE_BUILD_TYPE=Release` 配置的构建目录。

```powershell
cmake -S . -B build-dev `
  -A Win32 `
  -DVIRTOOLS_SDK_PATH="<Virtools-SDK-2.1 路径>" `
  -DCKANGELSCRIPT_ROOT="<CKAngelScript 路径>" `
  -DBML_ENABLE_ANGELSCRIPT=ON `
  -DBML_BUILD_TESTS=ON `
  -DCMAKE_INSTALL_PREFIX="<install-dev 的绝对路径>"

cmake --build build-dev --config Debug
ctest --test-dir build-dev -C Debug --output-on-failure
```

使用 Visual Studio 时，Debug DLL 位于 `build-dev/bin/Debug/BMLPlus.dll`；单配置
生成器的输出为 `build-dev/bin/BMLPlus.dll`。发布前还要构建并测试 Release：

```powershell
cmake --build build-dev --config Release
ctest --test-dir build-dev -C Release --output-on-failure
```

使用下列命令检查安装后的 SDK 目录和消费端辅助文件：

```powershell
cmake --build build-dev --config Release --target install
```

CMake 安装目录包含 `BMLConfig.cmake`、公共头文件、Mod CMake 辅助函数、IMC
生成器和面向 Mod 作者的公开文档。发布脚本再向该安装树加入原生/脚本模板和编辑器
API 文件。

## 运行时验证

单元测试和集成测试不会运行 Virtools 或真实 Player。修改 Hook、生命周期顺序、
渲染、输入、CK 对象访问、原生 Mod 装载或脚本宿主后，还必须在真实 Player 中验证。
验证层级必须能观察到被修改的行为：

- `tests/smoke/Validate-BMLBallance.ps1` 检查部署、Mod 加载、导出的运行时 interface、
  关闭和安装恢复。它是进程集成冒烟测试，不能证明 Building Block 语义正确。
- 定向 Player 场景沿真实游戏路径运行，并检查可观察的世界结果。行为执行、physics
  epoch、行为图修改、生命周期顺序、渲染或输入语义必须使用这一层。

设置 `BML_BALLANCE_ROOT`，或向脚本传入 `-BallanceRoot`：

```powershell
powershell -ExecutionPolicy Bypass `
  -File tests/smoke/Validate-BMLBallance.ps1 `
  -BallanceRoot "<Ballance 根目录>" `
  -BuildDll "build-dev/bin/Debug/BMLPlus.dll"
```

替换已加载的 DLL 前先关闭 Player。测试脚本会备份现有 Loader、安装冒烟测试
资源、启动 Player、检查日志，并在结束后恢复原安装；只有显式传入
`-KeepInstalled` 才会保留测试文件。

Player 验收拆成一个驱动 Mod 加每个被测对象各一个 probe Mod。`PlayerFlowDriver`
只负责自带流程：菜单图进入 Level 01、教程退出握手、抓帧和退出，它不认识任何被测
对象。所有结论都来自导出 `BMLPlayerProbeRead` 的 probe Mod：驱动枚举已加载的
probe，等每个 probe 报告结论，任一 probe 失败即整轮失败。读取 gameplay 状态的
probe 还导出 `BMLPlayerProbeStart`，由驱动决定它何时可以碰球、相机或输入。因此
一轮验收覆盖哪些对象，由 runner 安装了哪些 probe 决定，而不是由驱动决定。

ExecuteBB probe 自己 Physicalize 一个自有物体，因此可以在不依赖原版球的前提下
证明 ExecuteBB 的物理操作：

```powershell
cmake --build build-dev --config RelWithDebInfo --target BML `
  PlayerFlowDriver ExecuteBBTest
powershell -ExecutionPolicy Bypass `
  -File tests/player/Invoke-ExecuteBBTest.ps1 `
  -BallanceRoot "<Ballance 根目录>" `
  -BuildDll "build-dev/bin/RelWithDebInfo/BMLPlus.dll"
```

每个 runner 默认显示 Player 窗口，结束后恢复已安装的 Loader、测试 Mod 和日志。

Behavior Runtime 的各 probe 覆盖运行时语义 fixture、transport 接缝、Patch、公开的
Plan 与 Hook facade，以及由脚本 Mod 退休的脚本 Hook。它们与两个 Virtools fixture
插件一起安装：

```powershell
cmake --build build-dev --config RelWithDebInfo --target BML `
  PlayerFlowDriver BehaviorRuntimeSemanticsTest BehaviorTransportTest `
  BehaviorPatchTest BehaviorFacadeTest BehaviorScriptHookTest `
  BehaviorLifecycleFixture BehaviorTransportFixture
powershell -ExecutionPolicy Bypass `
  -File tests/player/Invoke-BehaviorAcceptanceTest.ps1 `
  -BallanceRoot "<Ballance 根目录>" `
  -BuildDll "build-dev/bin/RelWithDebInfo/BMLPlus.dll"
```

各 probe 路径默认取 Loader DLL 所在目录，只传 `-BuildDll` 即可。安装目录没有
`AngelScript.dll` 时补 `-DisableAngelScript`，脚本 Hook probe 会报告 SKIPPED 而
不是判定失败。

所有 runner 共用 `tests/player/BMLPlayerHarness.psm1`。`Invoke-BMLPlayerRun` 负责
备份并安装被改动的文件、启动 Player、应答 FullScreen Setup 对话框、抓取窗口截图、
驱动教程退出键并恢复安装；`Get-BMLPlayerFlowChecks` 把驱动日志行和各 probe 结论
转成所有 runner 共用的检查项。启动流程、验收流程或 probe 协议变化时改这个模块，
不要各自改 runner。

## 找到修改的负责区域

| 修改目标 | 负责区域 | 最小定向验证 |
| --- | --- | --- |
| 插件入口、Hook Block 注册或引擎拦截 | `src/BML.cpp`、`src/Behavior/HookBlock.*`、`src/Hooks/` | Win32 构建，以及覆盖该回调或 Hook 的真实 Player 场景 |
| Building Block Prototype 配置、执行或行为图插入 | `src/Behavior/Runtime.*`、对应的 `src/Behavior/<BuildingBlock>.*` 模块、`src/Api/ExecuteBB.cpp` | 依赖测试、导出 ABI 测试、Win32 构建和受影响的 Player 测试 |
| CK 生命周期和回调时序 | `src/Loader/ModManager.*` | 定向生命周期测试和 Player 冒烟测试 |
| Mod 发现、依赖顺序、服务或关闭 | `src/Loader/ModContext.*` | 对应 Loader/依赖测试和原生/脚本冒烟覆盖 |
| HUD、菜单、命令栏或内建行为 | `src/Mods/BMLMod.*`、`src/HUD/`、`src/Console/`、`src/CustomMaps/`、`src/Gameplay/`、`src/UI/` | 定向 UI/服务测试和 Player 画面/输入冒烟测试 |
| 旧式原生 SDK 或 CMake 消费端行为 | `include/BML/`、`cmake/` | ABI/编译测试、模板配置构建和安装后 SDK 检查 |
| IMC 运行时 | `src/Imc/ImcApi.cpp`、`src/Imc/ImcRuntime.*` | IMC 运行时/兼容性测试和原生 IMC 冒烟测试 |
| 内建 interface struct 或其背后的读取实现 | `include/BML/Interface.h`、`src/Api/Interfaces.cpp`、`src/Api/BuiltinCapabilities.*` | 定向 interface 测试、C ABI 编译测试和原生冒烟测试 |
| C/脚本 seam 的不透明 CK 对象引用 | `include/BML/Types.h`、`src/Api/ObjectRefs.*` | `ObjectRefsTest`、C ABI/IMC 编译测试；删除时序改变时再做 Player 生命周期测试 |
| IMC 代码生成器或其示例接口 | `tools/imc_codegen.py`、`tests/imc/` | 生成器检查、兼容性测试，并一起审查接口、lock 和头文件 |
| 脚本发现、绑定、执行或重载 | `src/AngelScript/`、`docs/api/` | 定向脚本测试、API stub 检查和脚本版 Player 冒烟测试 |
| 公开文档或发布目录 | `docs/`、`src/CMakeLists.txt`、`scripts/Package-BMLRelease.ps1` | 中英文严格文档构建、CMake install 和 SDK stage 校验 |

阅读代码时先看 `ModManager.cpp`，再看 `IMod.h`、`IMessageReceiver.h`、
`IBML.h` 和 `ModContext.h` 的声明。进入 `ModContext.cpp` 时按函数搜索，不建议
从第一行顺读整个文件。

## 公共接口规则

`IBML`、`IMod`、`IMessageReceiver` 等旧式原生 C++ 接口跨 DLL 边界，当前
发布线冻结其 ABI。不要修改虚函数签名或顺序、对象布局、所有权规则和跨边界
传递的类型。

`BML_*` C API 和 IMC 使用显式 handle、状态码及分配函数。修改时必须保持文档
规定的所有权和兼容性。新增 Loader 能力应放进经由 `BML_GetInterface` 取得的
带版本 interface struct；某个 Mod 向其他 Mod 提供的普通服务应使用生成式 IMC。
只有需要进程内指针并能强制必需 Mod 依赖的原生基础 Mod 才注册 provider interface；
不得增加临时 C++ ABI。

脚本 API 是公开的源码接口。修改绑定时，必须在同一次变更中更新脚本 API
参考、作者文档和运行时冒烟测试覆盖。

## 生成式接口

Loader 自己不发布任何 `.imc` 接口，生成器是给发布接口的 Mod 用的编写工具。
本仓库里有两个 `.imc` 文件，都属于测试。`tests/imc/test.sample.imc` 让生成器、
lock 格式和已提交的输出都保持在测试覆盖之下。`tests/smoke/smoke.native.imc`
属于原生冒烟 Mod，它在 Player 中既发布该接口又反过来消费它，让 Loader 的 IMC
导出在运行期持续被覆盖；`bml_target_imc_api` 把它的生成头写进构建树，因此仓库
里只提交 `.imc` 和它的 lock。

不要手工修改生成头。应修改对应的 `.imc` 文件并运行生成器。对示例接口即：

```powershell
python tools/imc_codegen.py `
  --update-lock `
  --out-dir tests/imc/generated `
  --input tests/imc/test.sample.imc
```

`.imc`、`.imc.lock` 和生成头应一起审查和提交。lock 保存稳定的字段与端点标识，
不要手工调整其中的数字。BML+ 的正常构建会对该示例以检查模式运行生成器，并在
已提交的绑定过期时失败。冒烟接口的生成头由构建本身重新生成，其 lock 与 `.imc`
不再匹配时构建会失败，并给出应运行的 `bml_update_imc_locks` 目标。

面向 Mod 作者的公开文档由 CMake 安装到 `share/BML/docs/<语言>`，发布脚本复制
该安装树。不要在打包脚本中增加第二套源码文档复制规则。编辑器 API stub 仍位于
`docs/api`，因为它们是工具输入，不是阅读文档。

## 修改约束

- 保持游戏线程回调短小。不要在 `OnProcess`、`OnRender`、引擎 Hook 或游戏线程
  IMC 处理函数中阻塞。
- CK 借用对象不转移所有权。对象删除、关卡变化和 CK reset 后要重新验证保存的
  对象引用。
- Mod 回调仍在执行时不能卸载其 DLL。
- 使用生成式 IMC 绑定，不要手写消息载荷编解码。
- 测试可观察行为，不要只搜索源文件中是否存在某段实现文本。
- 同一项公开行为同时有中英文文档时，两种语言应在同一次变更中更新。

Commit 标题使用简短的英文祈使句，例如：

```text
Fix native mod instance cleanup
Document native mod build requirements
Reject malformed event payloads
```

能够独立审查的行为、文档、生成物和清理工作应分别提交，不要混入无关修改。

## 提交前检查

- 使用 Win32 构建并测试 Debug 和 Release。
- 运行相关的定向测试和完整测试套件。
- 运行时相关修改完成与其语义匹配的真实 Player 验证。只有被测对象就是进程集成时，
  冒烟测试才足够；游戏语义必须使用定向 Player 场景。
- 修改接口后检查生成式 IMC 头和 `.imc.lock`。
- 以严格模式构建中英文文档站点。
- 确认没有意外修改旧式原生头文件和 DLL 导出。
- 暂存前检查最终 diff，排除无关的本地文件。
