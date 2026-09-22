# BML 原生 Mod API 总览

本文按用途说明 BML SDK 安装后提供的公开头文件。原生公开接口以安装目录中的
`include/BML` 为准。

## 最小入口

原生 Mod 是导出 `BMLEntry` 的动态库，通常使用 `.bmodp` 扩展名：

```cpp
#include <BML/IMod.h>

class MyMod final : public IMod {
public:
    explicit MyMod(IBML *bml) : IMod(bml) {}

    const char *GetID() override { return "MyMod"; }
    const char *GetVersion() override { return "1.0.0"; }
    const char *GetName() override { return "My Mod"; }
    const char *GetAuthor() override { return "Author"; }
    const char *GetDescription() override { return "Example"; }
    DECLARE_BML_VERSION;
};

BML_MOD_ENTRY(IMod *) BMLEntry(IBML *bml) { return new MyMod(bml); }
BML_MOD_ENTRY(void) BMLExit(IMod *mod) { delete mod; }
```

`BMLEntry` 返回的对象由 Mod DLL 分配。Mod 应导出 `BMLExit`，并在其中销毁
同一个对象，确保分配与释放使用相同的 C++ 运行库。对象创建后若注册失败，或
已加载的原生 Mod 被卸载，BML 都会调用 `BMLExit`。为兼容旧 Mod，缺少
`BMLExit` 的 DLL 仍可加载，但 BML 会记录警告，且无法安全销毁该 Mod 实例。

两个导出都应使用 `BML_MOD_ENTRY`。即使 Mod 项目更改了编译器的默认选项，
它也会固定 C linkage 和 calling convention。

推荐通过安装包提供的 CMake 函数创建目标：

```cmake
find_package(BML CONFIG REQUIRED)
bml_add_mod(MyMod MyMod.cpp)
bml_install_mod(MyMod)
```

`bml_add_mod` 链接 `BML::BML`、启用 C++20、关闭编译器扩展，并直接生成
`MyMod.bmodp`。它要求使用兼容 MSVC ABI 的 32 位目标，并让链接器校验精确的
C 符号 `BMLEntry` 和 `BMLExit`。入口缺失或被 C++ 名称修饰时，构建会直接
失败，不会生成 Loader 无法安全使用的 Mod。

`bml_install_mod` 添加标准安装规则。将 `CMAKE_INSTALL_PREFIX` 指向 Ballance
的 `ModLoader` 目录，再使用 CMake 的 `install` 目标构建并部署到
`ModLoader/Mods`。

## 公开头文件

| 头文件 | 用途 |
| --- | --- |
| `Version.h`, `Defines.h` | 版本宏、导出宏、状态码和基础定义 |
| `Result.hpp` | 共用 C++ 结果类型，携带 BML 状态码、可选值和可选的模块专属诊断 |
| `BML.h` | C ABI：版本、Loader 与 Mod 目录、命令注销、内存、字符串/编码、路径、文件与 Zip 工具 |
| `BMLAll.h` | 一次包含全部原生 SDK 接口的便捷聚合头 |
| `IMod.h`, `IMessageReceiver.h` | Mod 元数据、生命周期、玩法和引擎回调 |
| `IBML.h` | Loader 服务、CK 管理器、对象查找、命令、定时器和依赖管理 |
| `ICommand.h` | 命令执行、补全和基础参数解析 |
| `IConfig.h` | 类型化配置属性 |
| `ILogger.h` | Info、Warn、Error 日志 |
| `DataShare.h` / `DataShare.hpp` | 通过 C ABI 或 RAII C++ 封装使用的进程内命名字节共享 |
| `Types.h`, `TypeConvert.h` | 对象引用、向量与矩阵，以及与 Virtools 类型之间的互转 |
| `Interface.h` | Loader 交出的带版本接口结构体，以及取用它的方式 |
| `Behavior.h`, `Behavior.hpp` | Virtools Building Block 发现、编写、执行、行为图检查与编辑 |
| `Command.h/.hpp`, `Runtime.h`, `Scene.h`, `Gameplay.h`, `Speedrun.h`, `UI.h` | 通过接口结构体取用的 Loader 能力，附带 C++ 封装 |
| `ModMenu.h`, `ModMenu.hpp` | 纯 C 的 Mods 菜单页面接口，以及其强类型 C++ 编写层 |
| `Imc.h`, `Imc.hpp`, `ImcWire.hpp` | IMC C/C++ 运行时与线格式 |
| `Bui.h` | Ballance 风格 ImGui 控件 |
| `Gui.h`, `Gui/*.h` | `BGui` Virtools 实体/行为 UI 封装 |
| `InputHook.h` | 键盘、鼠标、手柄状态与可配对的输入屏蔽令牌 |
| `ExecuteBB.h` | 用于执行或创建常用 Building Block 的 v0.3 兼容接口 |
| `ScriptHelper.h` | 查找、连接、插入和删除行为图节点与参数 |
| `Guids.h`, `Guids/*.h` | Virtools 与 Ballance Building Block GUID 集合 |

`BML::Result<T>` 组合 BML 状态码与可选值。需要结构化诊断的模块可使用
`BML::Result<T, ModuleStatus>`；例如 `Behavior::Result<T>` 传入
`Behavior::Status`，无需改变共用容器或 C ABI。`Failure()` 收到非错误码时会将其
规范化为 `BML_ERROR_FAIL`，因此失败的 `Result<void>` 不会被判为成功。

## 自定义 Mods 菜单页面

原生 Mod 可通过 `BML::ModMenu::Page` 在自己的详情页末尾追加 Ballance 风格
入口。Config 分类保持在前，自定义入口按注册顺序排列，并共用每页四项的分页。
点击入口后会进入该 Mod 完全自定义的页面：

```cpp
#include <BML/Bui.h>
#include <BML/ModMenu.hpp>

class DiagnosticsPage final : public BML::ModMenu::Page {
public:
    DiagnosticsPage()
        : Page("diagnostics", "Diagnostics", "Show live runtime details") {}

protected:
    BML::ModMenu::PageAction OnFrame() override {
        Bui::Title("Diagnostics");
        // 在当前页面中直接绘制 ImGui 或 Bui 控件。
        return Bui::NavBack() ? BML::ModMenu::PageAction::Back
                              : BML::ModMenu::PageAction::None;
    }
};

// 让该对象作为 Mod 状态的一部分持续存活。
DiagnosticsPage diagnostics;

void RegisterMenuPages() { (void) diagnostics.Register(); }
void UnregisterMenuPages() { (void) diagnostics.Unregister(); }
```

调用 `OnFrame` 时，Loader 已经打开覆盖整个 viewport 的 ImGui 页面；直接绘制
内容即可，不要调用 `ImGui::NewFrame` 或 `ImGui::Render`。返回 `Back` 回到 Mod
详情页，返回 `Close` 退出 Mods 菜单。Loader 会复制 id、标签和说明，但页面对象
及回调必须存活到注销为止；owner DLL 卸载前，Loader 还会清除所有遗留注册。
1.0 版仅向原生 Mod 开放这一能力。请在所属 Mod 的 `OnLoad` 和 `OnUnload`
中分别调用上面的注册与注销函数。
`ModMenu.h` 可直接用于 C，不包含 C++ 标准库或类；`ModMenu.hpp` 只是单向建立在
该 C 接口上的 C++ 编写 facade。在 C 边界上，`BML_ModMenuPageDraw` 返回
`BML_OK` 或错误状态，并把延迟执行的导航请求写入
`BML_ModMenuPageFrame` 的 `Action` 成员。可选的 Enter 和 Leave 回调同样返回
状态码，页面生命周期失败不会再被静默当成成功。导航不会复用错误返回值；后续小版本也可借助
`StructSize` 在 frame 末尾追加输入或输出，而不改变 1.0 的回调签名。

## Mod 生命周期与事件

`IMod` 继承 `IMessageReceiver`。实现类必须提供 ID、版本、名称、作者、说明和
BML 版本要求，并可按需重写以下回调：

- 生命周期：`OnLoad`、`OnUnload`、`OnProcess`、`OnRender`。
- 配置和命令：`OnModifyConfig`、`OnPreCommandExecute`、
  `OnPostCommandExecute`、`OnCheatEnabled`。
- 引擎对象：`OnLoadObject`、`OnLoadScript`、`OnPhysicalize`、
  `OnUnphysicalize`。
- 游戏流程：菜单、加载/开始/重置/暂停/退出/下一关、死亡、结算、检查点、
  生命和导航状态等 `IMessageReceiver` 回调。

`OnProcess` 是唯一运行在 ImGui 帧内部的回调。所有 ImGui 和 `Bui` 控件都必须
在它里面绘制，不能放在 `OnRender` 中。参见[三种 UI 接口](#ui)。

`OnRender` 每次收到一个 `CK_RENDER_FLAGS`；原生 API 没有分别命名的
“渲染前/渲染后”回调。Loader 通知通过上面列出的 `IMod` 和
`IMessageReceiver` 虚函数同步到达。需要延后处理时，Mod 应在回调中复制所需
数据并放入自己拥有的队列。

## `IBML` 服务

`IBML` 是 Loader 传给 Mod 的主服务入口，功能分为：

- CK 上下文和 Attribute、Behavior、Collision、Input、Message、Path、
  Parameter、Render、Sound、Time 等管理器访问。
- `AddTimer` / `AddTimerLoop`：按帧数或毫秒安排回调。
- 游戏状态、作弊开关、游戏内消息、命令注册/查找/执行。
- 按名称查找 DataArray、Group、Material、Mesh、2D/3D Entity、Camera、
  Light、Sound、Texture 和 Behavior。
- 设置 Initial Condition、显示状态和跳过下一 Tick 渲染。
- 注册球体、地面、模块和变换类型，读取 SR/HS 分数。
- 枚举/查找 Mod，并注册、检查、读取或清空依赖。

定时器必须通过 `IBML` 创建。SDK 不发布独立 `Timer.h`；Loader 负责调度和
处理这些回调，Mod 不应维护另一套隐式的静态定时器状态。

`AddTimer` 和 `AddTimerLoop` 各有 `CKDWORD` 与 `float` 两个重载：`CKDWORD`
按帧计数，`float` 按毫秒计数。两种单位共用同一个函数名，因此不带后缀的整型
字面量是二义的，无法编译。调用时必须写明后缀：

```cpp
bml->AddTimer(1ul, [] { /* 下一帧 */ });
bml->AddTimer(1000.0f, [] { /* 一秒后 */ });
bml->AddTimerLoop(1.0f, [] { return KeepRunning(); });
```

循环回调返回 `true` 时继续运行。两个重载都不返回句柄，已安排的定时器无法取消，
需要停止时让循环回调返回 `false`。

## 原生 Mod 依赖

依赖必须在 BML 初始化 Mod 之前注册。构造函数是通常的注册位置，因为它在
`BMLEntry` 创建 Mod 时执行，早于任何 `OnLoad` 回调：

```cpp
explicit MyMod(IBML *bml) : IMod(bml) {
    AddDependency("RequiredMod", BMLVersion(1, 2, 0));
    AddOptionalDependency("OptionalMod", BMLVersion(1, 0, 0));
}
```

BML 会调整初始化顺序，使已安装的依赖先于依赖方收到 `OnLoad`。缺失的可选
依赖会被忽略。缺失必需依赖或出现依赖循环时，整个 Mod 初始化阶段不会开始；
日志会指出发起依赖的 Mod、所需 ID 和版本，或受循环影响的 Mod。若依赖已经
安装但版本过低，BML 会跳过依赖方的 `OnLoad`，在日志中同时给出实际版本和
所需版本，并继续初始化其他 Mod。

## 配置、命令与日志

`IConfig` 按 Category/Key 获取 `IProperty`。属性类型为 String、Boolean、
Integer、Float 或 Keyboard Key，支持设置当前值、默认值、注释和 Category
注释。它没有 UTF-16 专用属性接口；需要编码转换时使用 `BML.h` 中的显式
转换函数。
为了保持 `IProperty` 虚表不变，界面提示不增加虚函数。对字符串属性调用
`SetDefaultString` 后，可用
`IConfig.h` 的 C 扩展
`BML_SetConfigPropertyEditor(property, BML_CONFIG_EDITOR_COLOR)` 让 Mod 菜单以
调色盘编辑 `#RRGGBB` 或 `#RRGGBBAA`。有限选项的字符串属性可先用
`BML_SetConfigPropertyChoices` 复制候选值，再选择
`BML_CONFIG_EDITOR_CHOICE`，Mod 菜单就会用 RadioButton 代替文本框；空字符串
显示为 `None`，当前值即使不在列表中也仍可选择。`BML_GetConfigPropertyEditor`
和候选值读取函数可读取这些不写入配置文件的元数据。

新 Native Mod 通过 `Command.hpp` 编写命令。`BML::Command::Registration` 是
`Command.h` 中稳定 `bml.command` 函数表的单向 C++ 封装：注册时复制名称、别名、
说明、用法、分类及策略标记，并返回受所有者约束的句柄。执行回调会直接收到规范名称、
实际输入的名称、去掉命令头的参数、管道输入和输出函数；它返回的非负整数就是 shell
状态。补全接收独立的光标请求，其中明确给出当前参数和前缀，并通过有容量上限的收集器
写入候选；它不能产生命令输出或读取管道输入，也不让 STL 对象跨 DLL ABI。命令可隐藏或禁用，
可通过 `Visit`、`Find` 检查，并会在所属 DLL 释放前自动移除。`Registration` 也可以在
命令派发期间注销或析构：命令会立即停止参与查找，已取得的命令对象和回调状态则保留到
本次派发返回为止。C 调用方若提供 `BML_CommandDefinition::Release`，注册成功后即把
`UserData` 的所有权交给 Loader；命令移除且最后一次派发结束后，Loader 会调用它一次。

`ICommand` 是继续保留的旧接口适配层，仍提供命令元数据、执行、Tab 补全及
Integer、Float、Boolean 基础解析。`ILogger` 提供三个日志级别。

对于旧式 `ICommand`，命令栏会在 `Execute` 运行前解析整行。每次调用只收到一条
已经去除引号的命令；
`args[0]` 保留玩家实际输入的命令名或别名。`IBML::ExecuteCommand` 使用同一解析器。
面向玩家的完整语法见[《使用 BML+》中的命令行说明](using-bml.md)。

`Execute` 返回 `void`，所以 shell 约定的另一半由 `BML.h` 中的两个 C 导出承担。
`BML_SetCommandStatus(int)` 把正在运行的命令标记为失败，供 `&&`、`||` 和 `$?`
使用；不调用它，命令只有在抛出异常或找不到时才算失败。负数属于 API 错误值，
写入时统一转换为普通失败状态 `1`。
`BML_GetCommandInput(size_t *)` 返回 `other | this` 管道送进当前命令的文本，
没有管道时返回空指针；指针属于 Loader，在 `Execute` 返回前有效。作为管道中间
环节运行时，通过 `SendIngameMessage` 写出的内容会交给下一环节，而不是显示在
消息板上。

`IBML::RegisterCommand` 接收裸 `ICommand *`，Loader 从不删除它。注册成功时没有
任何返回信息，只在失败时写日志；失败的情况包括命令为空指针、命令名或别名非法、
命令名已被注册。

`IBML` 没有注销函数，注销命令要用 `BML.h` 里的 `BML_UnregisterCommand`：

```cpp
void MyMod::OnUnload() {
    if (BML_UnregisterCommand("mycmd") == BML_OK)
        delete m_Command;  // 到这一步删除才是安全的
}
```

只有注册该命令的那个 DLL 才能注销它。Loader 记下是哪个模块调用了
`RegisterCommand`，据此判断：注销别人的命令返回 `BML_ERROR_ACCESS_DENIED`，名字
不存在返回 `BML_ERROR_NOT_FOUND`。名字的匹配方式与控制台一致，因此用别名也能指到
同一个命令。该函数应在游戏线程调用。

不调用它，legacy 命令就会一直留在命令表里直到进程结束。特别注意：命令还在注册
状态时不要在 `OnUnload` 里删除 `ICommand`，卸载 Mod 本身不会移除它注册的命令，
删除后命令表里会留下悬空指针，控制台仍会尝试执行它。

`ParseFloat` 的默认取值范围是整个有限 float 范围。早期版本的默认下界是
`FLT_MIN`，即最小正规格化数，因此负数输入会被静默截断到约 `1.17e-38`。需要更
窄的范围时显式传入上下界。

## Loader 能力

旧式 C++ 接口冻结之后新增的能力，都以带版本的 interface struct 发布，通过
`BML_GetInterface` 按 id 与主版本号取用。每个都有自己在 `include/BML` 下的头文件，
每个头文件里还声明了一层 inline C++ 命名空间，把取用与参数检查折进去：

- `BML::Runtime`：运行状态、时钟和分数。
- `BML::Command`：受所有者约束的命令注册、发现和 shell 执行。
- `BML::Scene`：对象信息、实体变换和按名查找。
- `BML::Gameplay`：关卡、能量、目录、检查点和重置点。
- `BML::UI`：消息板、Mod/地图菜单和 HUD。
- `BML::Speedrun`：共享 Speedrun 计时器。
- `BML::Behavior`：Virtools Building Block 发现、配置后的 Run、自持有
  Frame、行为图检查、Watch、Patch、持久 Plan，以及访问已安装 Edit Node 和参数
  Port 的 revisioned installation binding。

版本规则写在 `Interface.h` 里：结构体只能在末尾追加成员并提升次版本号，而
`BML_IFACE_HAS` 用来询问正在运行的 Loader 有没有某个比 Mod 编译时的头文件更晚
加入的成员。

`bml.behavior` 直接沿用 Virtools 的 Prototype、Layout、Setting、Pin、Local、In、
Out、Pout、Graph、Patch 与 Plan 词汇。C interface 是稳定的传输 seam；原生 C++
Mod 通常应使用 `Behavior.hpp`，由它持有字符串、数组、callback、handle 和 Frame
字节。完整的 ownership、线程、world reset、错误恢复与热路径规则见
[Behavior 编写](behavior-authoring.md)。

原生 `BML::Gameplay` 的集合读取函数会在调用方持有的 `std::vector` 中返回完整
快照。目录应在初始化时读取，检查点和重置点应在关卡变化时刷新；这些调用会
传输完整集合，不适合逐帧轮询。

这些 inline C++ 操作都返回 BML 状态码，并标记为 `[[nodiscard]]`。调用方应处理返回
状态；只有明确忽略尽力清理的结果时才使用显式 `(void)` 转换。

## 跨 Mod 通信

按 Mod 之间需要传递的内容选择接口：

| 需求 | 方式 |
| --- | --- |
| 独立发布的原生/脚本 Mod 之间需要类型化 RPC 或通知 | IMC |
| 原生 Mod 之间需要直接函数指针或借用引擎对象 | 通过 `BML_RegisterInterface` 注册带版本的纯 C 函数表 |
| 共享生命周期明确的少量命名字节数据 | `DataShare` |

使用 IMC 时，编写 `.imc` 文件，通过 `bml_target_imc_api` 生成 C++ 或 AngelScript
绑定，并提交 lock 文件。Provider 缺失是运行时状态，不会阻止 Mod 加载。
详见 [IMC 概览](imc.md)和 [IMC 编写指南](imc-author-guide.md)。

纯 C 函数表必须保存在 Provider DLL 的静态存储区。Consumer 应声明必需 Mod
依赖，通过 `BML_GetInterface` 获取函数表，而不是链接 Provider DLL。
`bml_add_generated_interface_package` 可生成纯头文件包并检查版本；
`examples/native-interface-provider` 和 `examples/native-interface-consumer`
提供独立构建的完整工程。C++ 查询可使用
`BML::Interfaces::RequiredInterface` 或 `OptionalInterface`；后者可能返回
`BML_ERROR_NOT_FOUND`。

`DataShare` 只适合少量命名字节数据，不适合可演进的调用接口。
使用时遵守引用计数和借用指针的有效期规则。

## 三种 UI 接口

- `Bui` 直接绘制 Ballance 风格的 ImGui 控件，适合原生覆盖层界面。
- `BGui` 创建和操作由 Virtools 2D Entity/Behavior 组成的游戏内 UI。
- `BML::UI` 不绘制控件，而是控制 Loader 已有的消息、菜单和 HUD，且每个调用
  都必须在游戏线程上发出。

三者解决的问题不同，不应互相替代或混用命名。

### 在 `OnProcess` 中绘制 ImGui

ImGui 帧由 Loader 掌管。它在 Mod 回调之前开帧，并在 `OnProcess` 返回后立即
结束该帧：

1. Loader 在每帧的 Mod 回调之前调用 `ImGui::NewFrame`。
2. 所有 Mod 的 `OnProcess` 在这一帧内部运行。
3. Loader 调用 `ImGui::Render`，该帧结束。
4. `OnRender` 运行。
5. Loader 提交已记录的绘制数据。

因此 `ImGui` 和 `Bui` 调用必须写在 `OnProcess` 里。同样的调用放在 `OnRender`
中时帧已经结束，既不会绘制出任何内容，还可能触发 ImGui 断言。`BML::UI` 的
消息、菜单和 HUD 调用不受影响，因为它们修改的是 Loader 状态，而不是记录绘制
命令。

## C API 的所有权

`BML.h` 和 `DataShare.h` 可由 C ABI 调用；C++ Mod 可以包含
`DataShare.hpp`，以 RAII 管理句柄和可取消请求，而不依赖 Loader 私有实现。
凡是 BML 返回的新分配字符串、宽
字符串、字符串数组、宽字符串数组或二进制缓冲区，都应使用对应的
`BML_Free*` 函数释放，
不要跨 DLL 直接调用 CRT `free`。`BML_DataShare_Get` 返回借用指针；同一键
再次 Set/Remove 或实例销毁后立即失效，需要稳定副本时使用
`BML_DataShare_CopyEx`。排队的 `BML_DataShare_Request` 会返回归属于当前 Mod 的
request handle；自身生命期结束时用 `BML_DataShare_CancelRequest` 取消。Loader 会在
释放 Mod DLL 前取消其余请求。worker thread 或同一 DLL 承载多个 Mod 时，应使用
`BML_DataShare_RequestForOwner` 和 `BML_DataShare_CancelRequestForOwner`。应在
`OnUnload` 返回前停止 worker thread；公开 API 调用不能与调用方 DLL 的释放并发。

## Loader 与 Mod 所在目录

Mod 关于文件系统的两个问题都由 `BML.h` 回答：

```cpp
// Loader 自己的目录。借用指针，进程期间有效，不要释放。
const char *loaderDir = BML_GetLoaderPathUtf8(BML_DIR_LOADER);

// 自己的安装目录。新分配的字符串，用完要释放。
char *modRoot = BML_GetModRootUtf8(nullptr);
// ... 使用 modRoot ...
BML_FreeString(modRoot);
```

`BML_GetLoaderPathW` 和 `BML_GetLoaderPathUtf8` 接收一个 `BML_LoaderDirectory`：
`BML_DIR_WORKING`、`BML_DIR_TEMP`、`BML_DIR_GAME`、`BML_DIR_LOADER` 或
`BML_DIR_CONFIG`。返回的指针归 Loader 所有，不要释放。不要与
`BML_GetDirectoryA/W/Utf8` 混淆：后者只是取出所给路径字符串的目录部分。

`BML_GetModRootW` 和 `BML_GetModRootUtf8` 返回某个 Mod 的安装目录。查询自己时传
`nullptr`，它按调用方 DLL 解析，不需要 Mod 已注册，因此在构造函数中即可使用。传入
Mod id 则查询该 Mod：原生 Mod 返回其 DLL 所在目录，脚本 Mod 返回其脚本根目录。两者
都会分配内存，结果需用 `BML_FreeWString` 或 `BML_FreeString` 释放。

Loader 尚未初始化完成或目录无法解析时两者都返回空指针。`BML_GetModRoot` 还会获取
Loader 的 Mod 注册表锁，因此应在 Mod 内调用，不要放在 `DllMain` 中。

## 延伸阅读

- [原生 API 该走哪条路](native-api-routes.md)
- [选择 Mod 开发路线](modding.md)
- [IMC 概览](imc.md)
- [IMC 接口编写指南](imc-author-guide.md)
- [原生 Mod 模板](https://github.com/doyaGu/BallanceModLoaderPlus/tree/main/templates/native-mod-template)
- [脚本 Mod 教程](https://doyagu.github.io/BallanceModLoaderPlus/zh-CN/script-mod-tutorial/)
