# 原生 Mod API

SDL 原生 Mod 使用版本化的 `bml.mod` C 协议。C++ SDK 是头文件内的作者接口：
类、STL 对象、虚调用和回调捕获都留在 Mod 模块内部，宿主只接收 C 记录、句柄和函数指针。
原 Windows 后端在独立分支维护。

## 入口与事件

```cpp
#include <BML/Mod.hpp>

class MyMod final : public BML::Mod::Instance {
public:
    void OnLoad(const BML::Mod::Context &ctx) override {
        (void)ctx.LogInfo("My Mod 已装载");
    }
    void OnStartLevel(const BML::Mod::Context &ctx) override {
        (void)ctx.LogInfo("关卡开始");
    }
};

BML_DEFINE_MOD(MyMod, {
    .Id = "example.my-mod",
    .Name = "My Mod",
    .Version = "1.0.0",
})
```

`BML_DEFINE_MOD` 导出 `BML_GetModInterface`。反复查询得到同一份不可变声明，
查询不会构造 Mod。宿主复制声明、检查协议和依赖后，才执行 Create、OnLoad。
Instance 必须可默认构造，业务资源在 OnLoad 中注册。分配实例的模块负责 Destroy；
析构不得抛异常或开始新的宿主工作。纯 C11 写法见 `examples/native-c-mod`。

在 `BML_DEFINE_MOD` 中直接填写命名字段，可选字段省略即可。依赖也放在同一处：

```cpp
.Dependencies = {{"example.provider", "1.2.0"}, {"example.optional", "", true}},
```

无需独立 Define 函数、身份 getter、入口函数、构造函数或空生命周期回调。
宏生成原有 C 入口、模块内 Create/Destroy 和实际重写的回调。注册对象作为 Instance 成员，
宿主在销毁 Instance 前完成归属清理；自建线程退出等工作仍可放在 OnUnload。
计算元数据时继续支持 `BML_DEFINE_MOD(MyMod, DefineMyMod)`。
两种写法均在首次成功查询时生成一次声明，异常由入口捕获；不增加 DLL 装载期初始化或 C ABI。

Id、Name、Version 必填，Author、Description 可为空。版本使用无前导零、无后缀的
`MAJOR.MINOR.PATCH`。required/optional 依赖写入 Definition.Dependencies；
最低版本为空表示没有版本下限。不要在 OnProcess 中补注册依赖，SDK 也不会自动提高最低 Loader 版本。

按需重写回调，现有事件名称保留：

- OnLoad、OnUnload、OnProcess、OnRender；
- OnLoadObject、OnLoadScript、OnPhysicalize、OnUnphysicalize；
- OnModifyConfig、OnCheatEnabled、OnPreCommandExecute、OnPostCommandExecute；
- 原有菜单、关卡、检查点、球、导航、计数器和生命事件，包括 OnStartLevel、OnDead、
  OnPauseLevel、OnUnpauseLevel。

第一个参数统一为 `const Mod::Context &`。引擎通知使用 Mod.hpp 的具名事件记录，
对象用 ObjectRef 标识，字符串和数组用借用视图表示。C 协议通过 OnGameEvent、
OnCommandEvent 表达游戏事件和命令阶段；C++ 基类默认分派到各原有具名回调。
重写组合回调后，如仍需具名回调，应调用基类实现。

OnProcess 在有效 ImGui 帧内运行，位于引擎管理器 PostProcess 和 Behavior 安全点之后。
Timer、IMC、脚本 pump 保持原顺序，随后完成延迟输入清理和快照更新。
此时仍在 CKContext::Process 内，尚未返回。OnRender 不建立 ImGui 帧，也不推进计时器。
Process 与 Render 可以分开调用；UI::SkipNextRender(ctx) 在下一次 Render 消费请求，
不会按 Process 间隔代替跳帧。

常用服务可直接通过 `ctx.Config()`、`ctx.Input()`、`ctx.Timers()`、`ctx.Commands()` 取得。
它们是复用既有 wrapper 的轻量非拥有视图，Mod.hpp 已提供这些入口，不隐式包含引擎或 ImGui。
完整示例见 examples/native-mod/Mod.cpp：配置一次写完默认值和说明，命令返回拥有型 Registration，
定时器可用无参数 lambda 或完整 Context/Event 回调。void 循环回调持续到取消，bool 回调可返回
false 结束。失败的 Result 取值仍属于 std::bad_optional_access 异常族，并包含错误码及已有诊断。

## 功能入口

| 头文件 | 用途 |
| --- | --- |
| Mod.h/.hpp | 主协议、Context、元数据、Mod 查询、事件、日志、资源 |
| Config.h/.hpp | 类型化声明、非拥有 Property 视图 |
| Timer.h/.hpp | 拥有型注册、超时、循环、有限重复 |
| Input.h/.hpp | 原始/过滤快照、设备屏蔽、光标、键盘重复 |
| Command.h/.hpp | 控制台命令、参数、补全、shell 状态、管道 |
| Behavior.h/.hpp、Behavior/Blocks/ | 原型、执行、Frames、脚本根和可逆图编辑 |
| Scene.h、Gameplay.h、Time.h/.hpp、Speedrun.h、UI.h | 场景和游戏状态、时钟、游戏注册、渲染选项、HUD 和 Loader 菜单 |
| ModMenu.h/.hpp | 归属 Mod 的可见/隐藏页面、Push/Replace/Back/Close 路由 |
| Bui.h/.hpp | Ballance 风格即时控件及本地 Window/Page/Menu |
| Gui.h/.hpp、Gui/*.h | 本地 BGui 控制器和宿主持有的引擎 UI 元素 |
| DataShare.h/.hpp | 保留原 request/cleanup 约定的命名字节共享 |
| Imc.h/.hpp、ImcWire.hpp | 生成的跨 Mod RPC、future、Topic |
| Interface.h/.hpp | 声明 provider 依赖的进程内 C 表 |
| BML.h | 既有内存、编码、路径、文件、ZIP 工具 |
| Engine.h/.hpp、ImGui.h/.hpp | 显式选择的 CK2/VxMath 和宿主 ImGui 互操作 |

服务 `.h` 可用 C11 编译，`.hpp` 提供模块内 C++20 包装。
BMLAll.h 汇总核心接口，不隐式包含引擎或 ImGui 头。
IMod、IBML、IMessageReceiver、IConfig、IProperty、ICommand、ILogger 连同私有实现
均已移除。InputHook 留在 Loader 私有实现；旧 ExecuteBB 作者接口已移除，脚本便捷
引擎调用归 ScriptEngineCalls。已有 Mod 参照[迁移说明](native-mod-migration.md)。

Context/Config/Input/Timer/Command 作者接口中，无业务返回值的可失败操作
统一返回 `Result<void>`，包括 Set、Pause、Cancel、Release 和 Unregister。
可检查结果，也可调用 `.Take()` 通过现有 Mod 回调边界报告错误。读取/创建使用 `Result<T>`，
纯局部谓词仍返回 bool，`Get<T>` 保留既有的检查后直接取值用法。
补全回调返回 `Result<void>`；命令执行回调返回的是 shell 业务状态。C 表继续返回整数错误码。

UI 同时提供 `BML::Bui`、`BML::Gui` 入口，直接引用原有 `Bui`/`BGui` 实现和对象；
原命名空间及 GUI 虚函数签名继续保留，兼容已有源码。

## 配置

```cpp
#include <BML/Config.hpp>

auto config = ctx.Config();
auto enabled = config.DeclareProperty({"General", "Enabled", true}).Take();
const bool value = enabled.Get<bool>();
enabled.Set(false).Take();
```

声明成功后宿主复制元数据。同 category/key/type 的重复声明返回原属性，不覆盖当前值；
不同类型返回 TYPE_MISMATCH。无默认值的声明为 NONE，可在首次使用时通过 SetDefault
补齐类型；已知属性应通过 FindProperty 再查询。SetDefault 不重置用户值。
Read 返回拥有型 Info，Get 按类型读值并可指定回退值。Property 不拥有配置；owner 或声明代次
结束后失效。

存储沿用原配置实现，保留迁移、修改通知、选项/颜色编辑器和 UTF-8 原子保存。
只保存代码实际声明的键。保存失败保留脏值供显式重试或下次装载重新声明使用，
不因纯文件 I/O 失败保留 Mod 模块。

## 定时器与回调

```cpp
#include <BML/Timer.hpp>

// 在 Instance 成员中保留；丢弃原生 Registration 会请求取消。
auto result = ctx.Timers().SetTimeoutTicks(1, [ctx] {
    (void)ctx.LogInfo("下一次 timer pump");
});
BML::Timer::Registration timer = result.Take();
```

方法名区分毫秒和 tick。TIME、REALTIME 的 Definition 也使用毫秒。
有限 REPEAT 的次数范围为 1..INT32_MAX，无限 RemainingIterations 为 -1，计数达到
INT32_MAX 后饱和。零延迟也从后续 pump 开始。DEBOUNCE/THROTTLE 枚举保留，
创建返回 NOT_IMPLEMENTED。Priority 钳制到 -128..127。

Registration 为 move-only RAII。回调内 Cancel 会阻止后续调用，返回 true 不能复活它。
原生和脚本通过同一私有注册表完成句柄检查、终结和释放。脚本 TimerRef 的原有语义保留：
丢弃 VM 引用不自动取消注册。

新增 C 注册只在成功后移交一份 userdata Release 责任，失败仍归调用方。
Release 等待在途回调和借用退出。显式取消失败保留 wrapper 以便重试；析构无法取消时
停止其业务回调，释放仍由宿主清理完成。已有 DataShare、Behavior 保持原 cleanup/Retain
规则，不能套用 Timer 的所有权约定。

## 输入

```cpp
#include <BML/Input.hpp>

auto input = ctx.Input();
auto raw = input.ReadKeyboard(BML_INPUT_RAW).Take();
auto filtered = input.ReadKeyboard().Take();
auto block = input.AcquireBlock(BML_INPUT_BLOCK_KEYBOARD).Take();
const bool down = raw.IsKeyDown(30); // CK 键码，无需引擎头
```

引擎负责采集设备。现有 InputHook 提供过滤查询和 RAW 对应的原始方法。
RAW 绕过 BML 屏蔽，不绕过输入管理器 Pause。读取不会轮询事件或推进输入。
每个 Block 是独立计数份额，Release 和 owner 清理只移除本份额；键鼠、手柄、光标、
重复和按键边沿行为沿用现有实现。

KeyboardState 中的 span 在所属 Mod/服务回调内借用。同回调重复读取、合法重入及同帧
屏蔽变化不会让旧视图提前失效；新读取反映当前屏蔽状态。复制视图不延长寿命，
延迟工作确实需要数据时自行复制拥有值。

## UI 与引擎互操作

Bui、ImGui 在 OnProcess 绘制。Bui 的 Window/Page/Menu 留在模块内部，构造时传入 Context。
Bui::At 恢复光标后在模块内重新抛出本地异常。Begin/End 必须配对，折叠时同样如此；
其他 owner 或嵌套绘制作用域不能结束你的窗口。

BGui::Gui、Element、Panel、Text、Label、Button、Input、KeyInput 也显式接收 Context，
业务回调不跨模块传递 C++ 对象。宿主持有主/附属 CK 对象，在删除和世界重置时使引用失效。
组合 Add 中途失败会回收尚未发布的元素。三类字体分别负责：Label 用游戏字体，Text 用
CKSpriteText 字体，overlay 用 ImGui 字体。

UI.h 控制 Loader 自己的 HUD、消息和菜单；不等同于 Bui/BGui 绘制。
ModMenu::Page 在 Mod 详情增加入口。Page 在注销前保持存活；需要 Bui 时，派生类按值保存
Context，在 OnLoad/OnUnload 注册/注销。可见/隐藏路由、代次检查及原 C Page 回调保持原样。

直接操作 CK 对象时包含 Engine.hpp 并链接 BML::Engine。
BorrowContext、BorrowRenderContext、BorrowObject 返回非拥有 CK 指针，ReferenceObject
产生既有 ObjectRef。跨世界变化保存 ObjectRef 或 CKObjectHandle，使用时重新解析。
管理器查询复用 CKContext 已有方法。Behavior/Blocks、ScriptHelper、Guids、TypeConvert
属于引擎相关便利层；ScriptHelper 在 Mod 内执行。

直接使用宿主 ImGui 需链接 BML::ImGui 并包含 ImGui.hpp，不得静态编译第二份 ImGui。
Bui/ImGui.hpp 保留高级 ImGui flags/callback 重载，Gui/Engine.hpp 提供 Vx/CK 便利函数。
纹理复用 CKTexture/CKSprite 和已有对象引用，不暴露 rasterizer ABI，也不设计第二套纹理句柄。
自有 draw callback、userdata 和纹理必须活到本帧后端提交或丢弃之后。

## ABI、错误与卸载

新增 C 函数指针统一显式 cdecl，标量使用固定宽度；记录 pack(8) 并恢复调用方 packing。
每种可增长记录都有冻结的 1.0 前缀大小。未知尾部忽略，缺少必填前缀则拒绝，
不会越过调用方容量写入。可选追加函数先检查 BML_IFACE_HAS。
C 字符串为 UTF-8；文本 required/capacity 包含 NUL，二进制不包含。
缓冲区不足报告所需容量，不发布部分文本。

Context 是可按值保存的非拥有 owner 能力，不要保存回调参数引用。
ModHandle 仅用于观察，不是 instance 指针或操作其他 Mod 的权限。
默认服务/引擎操作要求游戏线程；复制 Context 不赋予后台线程权限。
DataShare/IMC 继续遵循各自已有线程约定。

Result 在调用方保存其值，但值中的 span、view 或句柄仍遵循各自的借用寿命，
Take/移动 Result 不会把输入或事件借用数据变成拥有存储。失败无可读 Value。
普通服务错误由调用方处理。
主通知或 Timer callback 失败会停止该 owner 的普通分派，在安全点清理，当前事件仍继续其他 Mod。
C++ trampoline 在模块内部捕获异常并记录可用诊断，不把异常跨 C seam 传播。

卸载先完成消费者，再停用 provider。顺序为停止接纳、排空回调、OnUnload、归属清理、Destroy。
重试不重复成功阶段。OnUnload 或恢复失败时保留必要实例和模块。
Mod 自建线程、直接注册的系统/引擎回调必须在 OnUnload 停止并等待退出；做不到就报告失败。
Destroy/Release 不抛异常，也不能开始新的宿主工作。

## 构建与分发

```cmake
cmake_minimum_required(VERSION 3.25)
project(MyMod LANGUAGES CXX)
find_package(BML CONFIG REQUIRED)
bml_add_mod(MyMod MyMod.cpp)
# 用到才显式添加：
# target_link_libraries(MyMod PRIVATE BML::Engine BML::ImGui)
bml_install_mod(MyMod)
```

纯 C ABI 模块不锁定宿主的 C++ 编译器/标准库；Engine/ImGui 扩展仍要求匹配编译器、
运行库、依赖头和公开编译设置。SDK 仅在最终链接图使用扩展时发现并检查相应依赖，
通过中间 interface library 链接同样生效。

bml_add_mod 生成 .bmodp 和 .bmodp.abi.json。schema 2 检查运行时、平台、架构、宿主链接方式、
bml.mod major，以及实际要求的扩展 ABI。SDK 内容 hash 用于诊断。
旧 SDL C++ 入口产物会收到迁移错误，需重新编译。分发时同时携带模块和伴随文件，或打成 ZIP；
不要打包用户配置。静态宿主集成仍待完成。
macOS/ELF 模块按 SDK 链接规则复用 Player 的依赖，测试 runner 不重定位消费者二进制。
