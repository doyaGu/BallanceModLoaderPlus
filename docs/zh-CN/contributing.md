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
       Loader 内建功能
```

`ModManager` 将 Virtools manager 生命周期连接到 BML+。`ModContext` 负责 Mod
发现、依赖顺序、回调分发、公共服务和关闭。原生 Mod 通过已安装的 C++ ABI 进入，
脚本 Mod 通过 CKAngelScript 宿主进入。Mod 可以通过 IMC 发布自己的接口；
Loader 不发布生成式 IMC 接口。

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

cmake --build build-dev --config Debug --target BML BehaviorExecutionTest
ctest --test-dir build-dev -C Debug -R '^BehaviorExecutionTest$' --output-on-failure
```

使用 Visual Studio 时，Debug DLL 位于 `build-dev/bin/Debug/BMLPlus.dll`；单配置
生成器的输出为 `build-dev/bin/BMLPlus.dll`。发布前还要构建并测试 Release：

```powershell
cmake --build build-dev --config Release --target BML
```

使用下列命令检查安装后的 SDK 目录和消费端辅助文件：

```powershell
cmake --build build-dev --config Release --target install
```

CMake 安装目录包含 `BMLConfig.cmake`、公共头文件、Mod CMake 辅助函数、IMC
生成器和面向 Mod 作者的公开文档。发布脚本再向该安装树加入原生/脚本模板和编辑器
API 文件。

## 运行时验证

选择能观察到改动行为的最小测试：

| 改动 | 验证方式 |
| --- | --- |
| 纯逻辑、解析或公开头文件 | 构建并运行受影响的单元或 ABI 测试 |
| Loader 启动、安装或原生/脚本加载 | 运行 `tests/smoke/` 中对应的冒烟脚本 |
| CK 生命周期、Behavior 执行、物理或玩法 | 运行 `tests/player/` 中对应的真实 Player runner |
| 可见 ImGui、输入或菜单导航 | 运行 `tests/ui/README.md` 指定的场景 |

真实 Player 测试会临时修改游戏安装并在结束后恢复。启动前先退出 Player。
无关改动不必跑整套可视 UI 测试。runner 用法与 probe 归属见
`tests/player/README.md` 和 `tests/ui/README.md`。

## 找到修改的负责区域

| 修改目标 | 负责区域 | 最小定向验证 |
| --- | --- | --- |
| 插件入口、Hook Block 注册或引擎拦截 | `src/BML.cpp`、`src/Behavior/HookBlock.*`、`src/Hooks/` | Win32 构建，以及覆盖该回调或 Hook 的真实 Player 场景 |
| Building Block 配置、执行或行为图插入 | `src/Behavior/Block.*`、`src/Behavior/Runtime.*`、`include/BML/Behavior/Blocks/`、`src/Api/ExecuteBB.cpp` | 定向 Behavior 和 ABI 测试、Win32 构建及受影响的 Player 测试 |
| CK 生命周期和回调时序 | `src/Loader/ModManager.*` | 定向生命周期测试和 Player 冒烟测试 |
| Mod 发现、依赖顺序、服务或关闭 | `src/Loader/ModContext.*` | 对应 Loader/依赖测试和原生/脚本冒烟覆盖 |
| HUD、菜单、命令栏或内建行为 | `src/Mods/BMLMod.*`、`src/HUD/`、`src/Console/`、`src/CustomMaps/`、`src/ModMenu/`、`src/Gameplay/`、`src/UI/` | 定向 UI/服务测试和 Player 画面/输入冒烟测试 |
| 旧式原生 SDK 或 CMake 消费端行为 | `include/BML/`、`cmake/` | ABI/编译测试、模板配置构建和安装后 SDK 检查 |
| IMC 运行时 | `src/Imc/ImcApi.cpp`、`src/Imc/ImcRuntime.*` | IMC 运行时/兼容性测试和原生 IMC 冒烟测试 |
| 内建 interface struct 或其背后的读取实现 | `include/BML/Interface.h`、`src/Api/Interfaces.cpp`、`src/Api/BuiltinCapabilities.*` | 定向 interface 测试、C ABI 编译测试和原生冒烟测试 |
| C/脚本 seam 的不透明 CK 对象引用 | `include/BML/Types.h`、`src/Api/ObjectRefs.*` | `ObjectRefsTest`、C ABI/IMC 编译测试；删除时序改变时再做 Player 生命周期测试 |
| IMC 代码生成器或其示例接口 | `tools/imc_codegen.py`、`tests/codegen/imc/` | 生成器检查、兼容性测试，并一起审查接口、lock 和头文件 |
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

Loader 自己不发布 `.imc` 接口；生成器供 Mod 作者使用。
`tests/codegen/imc/test.sample.imc` 检查生成器、lock 和已提交的头文件。
`tests/smoke/` 与 `tests/player/` 中的接口覆盖运行时互通；Provider 模板另有
入门接口。运行时测试所用的头文件在构建树中生成。

不要手工修改生成头。应修改对应的 `.imc` 文件并运行生成器。对示例接口即：

```powershell
python tools/imc_codegen.py `
  --update-lock `
  --out-dir tests/codegen/imc/generated `
  --input tests/codegen/imc/test.sample.imc
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
