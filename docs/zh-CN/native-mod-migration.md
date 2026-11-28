# 原生 Mod 迁移到 SDL C 协议

SDL SDK 使用 BML_GetModInterface 和模块内 C++ wrapper。原先 IMod/IBML 入口的 SDL
Mod 必须重新编译；独立原 Windows 分支的 ABI 不在本次迁移范围。
现有功能与事件时序保留；旧 I* 类型及实现已删除，不再作为私有兼容层存在。

| 原写法 | 当前写法 |
| --- | --- |
| IMod 派生类、IBML* 构造参数 | Mod::Instance 派生类、默认构造 |
| GetID/GetName/GetVersion/GetAuthor/GetDescription/GetBMLVersion | BML_DEFINE_MOD 的命名字段（或 Mod::Definition 工厂） |
| BMLEntry/BMLExit | BML_DEFINE_MOD(Type, Factory)，或 Mod.h 中的纯 C 表 |
| RegisterDependency/RegisterOptionalDependency | Create 前的 Definition.Dependencies |
| OnLoad/OnUnload/OnProcess | 名称保留，增加 const Mod::Context & |
| 各游戏事件 override | 名称保留，默认 OnGameEvent 分派到原有具名方法 |
| 引擎事件的裸 CK 参数 | 具名记录、ObjectRef、借用 span |
| GetLogger()->Info/Warn/Error | ctx.LogInfo/LogWarn/LogError，UTF-8 文本 |
| GetConfig/GetProperty、IProperty | Config::View(ctx)、DeclareProperty/FindProperty、Property 值 |
| SetDefaultInteger/Float/Boolean/String/Key | Property::SetDefault，分别用 int32_t/float/bool/string/uint32_t |
| SetInteger/Float/Boolean/String/Key | Property::Set，使用相同的类型 |
| AddTimer/AddTimerLoop | 保留 Timer::Registration，显式区分毫秒/tick |
| InputHook 普通/原始查询 | Input::View 的 ReadKeyboard/ReadMouse/ReadJoystick，可选 INPUT_RAW |
| Block/Unblock | 保留 Input::Block，每次获取对应一份独立计数 |
| GetCKContext/GetRenderContext | 可选 Engine::BorrowContext/BorrowRenderContext |
| Get*ByName 后直接操作对象 | Scene ObjectRef，或显式 Engine 下 CKContext 的原有查询 |
| IsIngame/IsPaused/IsPlaying、作弊、分数 | Gameplay 的状态、作弊、energy/highscore 接口 |
| ExitGame、类型注册、渲染选项 | 显式接收 Context 的 Gameplay 方法 |
| SetIC/RestoreIC/Show | Context + ObjectRef 的 Scene 方法 |
| Bui.h、BGui 构造 | Bui.hpp/Gui.hpp，传入 Context；高级适配头单独包含 |
| ExecuteBB | Behavior Session/Blocks/Call/Task/Instance；ScriptHelper 是本地引擎 helper |
| 旧 C++ provider bootstrap | 声明 provider 依赖，复用 Interface.h/.hpp 的 C 表 |

配置文件、Command、Behavior、IMC、DataShare 的既有格式和语义保留。
不要为这次迁移重写 IMC wire schema，也不要改变旧 DataShare/Behavior 的释放约定。

## 已移除的兼容接口

源码和安装后的公共 SDK 均不再包含 IMod.h、IBML.h、IMessageReceiver.h、IConfig.h、
ICommand.h、ILogger.h、InputHook.h、ExecuteBB.h、ModInterface.hpp 和 RefCount.h。
宿主以组合的 ModRecord 和服务实现管理生命周期，不保留旧 I* 继承体系。
旧 `MOD_EXPORT` 和 `DECLARE_BML_VERSION` 宏已移除；C++ 用 `BML_DEFINE_MOD`，
纯 C 入口用 `BML_MOD_ENTRY(int)`，最低版本要求放在 Mod 定义中。

| 已移除导出 | 替代用法 |
| --- | --- |
| BML_GetLoaderPathW/Utf8、BML_GetModRootW/Utf8 | `ctx.GetDirectory(BML_DIR_*)` / `ctx.GetDirectory(BML_MOD_DIRECTORY_ROOT)` |
| BML_UnregisterCommand | 保留 `Command::Registration`，调用 `Unregister()` |
| BML_SetCommandStatus | 命令回调直接返回 shell 状态 |
| BML_GetCommandInput | `Command::Invocation::Input()` |
| BML_Get/SetConfigPropertyEditor、BML_GetConfigPropertyChoiceCount/Choice、BML_SetConfigPropertyChoices | `Config::Property::Read()` / `SetEditor()` / `SetChoices()` |
| ExecuteBB 的 C++ 导出 | Behavior Session/Blocks/Call/Task/Instance |

`BML_DIR_*` 目录编号保留在 `Mod.h`。`BML.h` 中独立的内存、文件、ZIP 和编码工具继续可用。
脚本接口和既有配置文件格式保持原样。

推荐将 Command/Timer/Input 拥有型注册放在 Instance 成员中。
跨回调需要 Context 时保存值，后续回调优先使用自己的 Context 参数。
不要捕获事件 string_view/span 或裸 CK 指针用于延迟工作：复制确有需要的数据，
或保留 ObjectRef/CKObjectHandle，在使用时重新解析。
业务资源在 OnLoad 注册，OnUnload 能处理部分初始化并停止外部线程。
宿主在 Destroy 前清理剩余 SDK 归属注册。

Timer 回调内可自 Cancel。自然完成和重复 wrapper Cancel 是幂等的；真实取消失败仍返回错误并保留
释放责任。原始 C Cancel 对未知句柄继续返回 INVALID_HANDLE。脚本 TimerRef 的丢弃行为不变。

核心只需 BML::BML；直接 CK/ImGui 调用才增加 BML::Engine、BML::ImGui，匹配其完整依赖 ABI。
引擎、SDL 和 ImGui 的共享状态归 Player 进程，不要再次静态链接一份。

SDK 模板和十二个仓库 Mod 已迁移，可作为源码参考。
当前执行记录和未验收目标见[验证状态](validation.md)，发布门槛见
[发布手册](https://github.com/doyaGu/BallanceModLoaderPlus/blob/sdl/RELEASING.md#native-c-protocol-release-gate)。
