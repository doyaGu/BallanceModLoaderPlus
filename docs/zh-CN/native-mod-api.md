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

MOD_EXPORT IMod *BMLEntry(IBML *bml) { return new MyMod(bml); }
MOD_EXPORT void BMLExit(IMod *mod) { delete mod; }
```

`BMLEntry` 返回的对象由 Mod DLL 分配。Mod 应导出 `BMLExit`，并在其中销毁
同一个对象，确保分配与释放使用相同的 C++ 运行库。对象创建后若注册失败，或
已加载的原生 Mod 被卸载，BML 都会调用 `BMLExit`。为兼容旧 Mod，缺少
`BMLExit` 的 DLL 仍可加载，但 BML 会记录警告，且无法安全销毁该 Mod 实例。

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
| `BML.h` | C ABI：版本、内存、字符串/编码、路径、文件与 Zip 工具 |
| `BMLAll.h` | 一次包含全部原生 SDK 接口的便捷聚合头 |
| `IMod.h`, `IMessageReceiver.h` | Mod 元数据、生命周期、玩法和引擎回调 |
| `IBML.h` | Loader 服务、CK 管理器、对象查找、命令、定时器和依赖管理 |
| `ICommand.h` | 命令执行、补全和基础参数解析 |
| `IConfig.h` | 类型化配置属性 |
| `ILogger.h` | Info、Warn、Error 日志 |
| `DataShare.h` | 低层、同进程的命名字节数据共享 |
| `Imc.h`, `ImcTypes.h`, `ImcWire.hpp`, `ImcCpp.hpp`, `ImcMath.h` | IMC C/C++ 运行时、线格式与基础类型 |
| `Generated/*.hpp` | 从 `.imc` 生成的内置接口绑定；不要手工修改 |
| `Runtime.h`, `Scene.h`, `Gameplay.h`, `UI.h`, `Events.h`, `EventKinds.h` | 内置 IMC 的易用 C++ 门面 |
| `Bui.h` | Ballance 风格 ImGui 控件 |
| `Gui.h`, `Gui/*.h` | `BGui` Virtools 实体/行为 UI 封装 |
| `InputHook.h` | 键盘、鼠标、手柄状态与可配对的输入屏蔽令牌 |
| `ExecuteBB.h` | 执行或创建常用 Building Block |
| `ScriptHelper.h` | 查找、连接、插入和删除行为图节点与参数 |
| `Guids.h`, `Guids/*.h` | Virtools 与 Ballance Building Block GUID 集合 |

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
在它里面绘制，不能放在 `OnRender` 中。参见[三种 UI 接口](#三种-ui-接口)。

`OnRender` 每次收到一个 `CK_RENDER_FLAGS`；原生 API 没有分别命名的
“渲染前/渲染后”回调。需要解耦的事件消费方可以使用
`BML::Events::Stream`，它覆盖游戏流程、对象/脚本加载、物理、命令、配置和
作弊状态事件。

轮询前应先打开事件流。`Poll` 只有在从队列中取出事件时才返回 `BML_OK`；
已打开但没有待处理事件时返回 `BML_ERROR_NOT_FOUND`，事件流未打开时返回
`BML_ERROR_INVALID_HANDLE`。每次轮询都会先重置输出事件，未成功取得事件时
输出仍保持默认值。

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

`ICommand` 提供命令名、别名、说明、作弊标记、执行函数和 Tab 补全，并附带
Integer、Float、Boolean 的基础解析函数。`ILogger` 提供三个日志级别。

`IBML::RegisterCommand` 接收裸 `ICommand *`，Loader 从不删除它。命令对象只需
分配一次，并保持存活到进程结束。不要在 `OnUnload` 中删除它：卸载单个 Mod 不会
从命令表中移除它注册的命令，删除后命令表里会留下悬空指针。`IBML` 没有对应的
注销函数。注册成功时没有任何返回信息，只在失败时写日志；失败的情况包括命令为
空指针、命令名或别名非法、命令名已被注册。

`ParseFloat` 的默认取值范围是整个有限 float 范围。早期版本的默认下界是
`FLT_MIN`，即最小正规格化数，因此负数输入会被静默截断到约 `1.17e-38`。需要更
窄的范围时显式传入上下界。

## 跨 Mod 通信

新接口优先使用 IMC：

- `.imc` 文件只描述接口；字段编号是稳定的线格式标识，不是数组下标。
- `bml_target_imc_api` 在构建时生成 C++ 绑定并加入目标。
- RPC 支持同步调用、Future、取消、超时和完成回调。
- Topic 支持有界订阅队列、退订和丢弃计数。
- 热路径使用已生成的类型与缓存的 ID，不在每次调用时解析文本描述。

内置门面提供：

- `BML::Runtime`：运行状态、时钟和分数。
- `BML::Scene`：对象信息、实体变换和按名查找。
- `BML::Gameplay`：关卡、能量、目录、检查点和重置点。
- `BML::UI`：消息板、Mod/地图菜单和 HUD。
- `BML::Speedrun`：共享 Speedrun 计时器。
- `BML::Events`：带类型化附加数据的事件流。

原生 `BML::Gameplay` 的集合读取函数会在调用方持有的 `std::vector` 中返回完整
快照。目录应在初始化时读取，检查点和重置点应在关卡变化时刷新；这些调用会
传输完整集合，不适合逐帧轮询。

返回 BML 状态码的 C++ IMC 操作均标记为 `[[nodiscard]]`。调用方应处理返回状态；
只有明确忽略尽力清理的结果时才使用显式 `(void)` 转换。

`DataShare` 适合共享少量命名字节数据，调用方必须遵守引用计数和借用指针
有效期。需要演进、跨语言绑定或 RPC/Topic 语义时使用 IMC。

## 三种 UI 接口

- `Bui` 直接绘制 Ballance 风格的 ImGui 控件，适合原生覆盖层界面。
- `BGui` 创建和操作由 Virtools 2D Entity/Behavior 组成的游戏内 UI。
- `BML::UI` 不绘制控件，而是通过 IMC 控制 Loader 已有的消息、菜单和 HUD。

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

`BML.h` 和 `DataShare.h` 可由 C ABI 调用。凡是 BML 返回的新分配字符串、宽
字符串、字符串数组、宽字符串数组或二进制缓冲区，都应使用对应的
`BML_Free*` 函数释放，
不要跨 DLL 直接调用 CRT `free`。`BML_DataShare_Get` 返回借用指针；同一键
再次 Set/Remove 或实例销毁后立即失效，需要稳定副本时使用
`BML_DataShare_CopyEx`。

## 延伸阅读

- [选择 Mod 开发路线](modding.md)
- [IMC 概览](imc.md)
- [IMC 接口编写指南](imc-author-guide.md)
- [原生 Mod 模板](https://github.com/doyaGu/BallanceModLoaderPlus/tree/main/templates/native-mod-template)
- [脚本 Mod 教程](https://doyagu.github.io/BallanceModLoaderPlus/zh-CN/script-mod-tutorial/)
