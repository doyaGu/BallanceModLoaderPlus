# 原生与脚本 API 对齐迁移

本轮不兼容旧作者接口，只提供源码迁移。没有旧别名、重载或 Timer/Command 对象注册适配器。
原生 C ABI 的函数表、结构布局和回调保持不变；C++ wrapper 和脚本需使用匹配的 SDK
重新编译。脚本从源码重新构建，不接受旧声明或旧字节码作为兼容输入，也没有新增字节码缓存。

| 旧入口 | 当前入口 |
| --- | --- |
| C++ Registration.Register、命名空间级 Timer 便利函数 | ctx.Timers().Register/SetTimeout/SetInterval 及 ticks 版本 |
| C++ Command::Visit/Find/ExecuteLine、Registration.Register | ctx.Commands().ReadCommands/Find/ExecuteLine/Register |
| BorrowLogger、logger.Info/Warn/Error | ctx.LogInfo/LogWarn/LogError |
| BorrowConfig、GetProperty | ctx.Config().DeclareProperty/FindProperty，错误码与 out 分开 |
| SetBoolean、SetDefaultBoolean 等各类型写入 | Set/SetDefault(ConfigValue) |
| InputHook、BorrowInputManager | ctx.Input().ReadKeyboard/ReadMouse/ReadJoystick |
| 全局屏蔽 token、Block/Unblock/SetBlock | AcquireBlock(mask, InputBlock@ &out)，保存后显式 Release |
| Timer 接口、AddTimer、扁平定时器入口 | ctx.Timers()，Definition 加 delegate |
| Command 接口、CommandCallback/CommandCompletionCallback | CommandDefinition 加 CommandExecute/CommandComplete |
| 按名称注销命令/页面 | 对注册返回的 CommandRef/MenuPageRef 调用 Unregister |
| Mod 信息逐字段 getter | Owner/FindMod/ReadMods，ModRef.Read(ModInfo &out) |
| CheckDependencies() 的 0/1 返回 | CheckDependencies(bool &out satisfied)，返回错误码 |
| CommandEvent.CommandName/ArgCount/GetArg | Name/ArgumentCount/ReadArgument；InvokedAs 保留实际输入别名 |
| LoadObjectEvent.IsDynamic | Dynamic |
| PhysicalizeEvent.AutoCalcMassCenter/Ball* | CalcMassCenter/SphereCount/GetSphereCenter/GetSphereRadius |
| 物理事件中心的 VxVector | BML::Vec3，需要 VxVector 时逐分量转换 |
| 扁平游戏、时间、HUD、对象与管理器入口 | Gameplay/Time/Speedrun/UI/Scene/Engine 命名空间 |
| 仅存在于旧声明文件的 Events::Stream | 删除，不实现第二套事件总线 |

C++ 常用接口返回 Result；检查结果或调用 Take()。脚本普通失败返回负错误码，成功是
`BML::ERROR_OK`，不能保留旧 `if (code)` 的成功判断。命令执行回调的非负业务退出码与
ExecuteLine 的 API 状态分开。Ref 输出失败为 null，其他输出只在成功后使用。UI 控件的
交互 bool、回退 getter 和 Behavior/IMC/DataShare 的既有专用契约不作强行改造。

集合查询的真实声明是 `int ReadMods(? &out)` 与 `int ReadCommands(? &out)`。
分别传入 `array<BML::ModInfo>`、`array<BML::CommandInfo>` 对象，不能传 handle 变量。
类型检查严格；正确类型的输出失败为空、成功完整。依赖使用 Definition.DependencyCount
和 ReadDependency，文本和记录都是拥有数据的快照。

ConfigValue 支持 string/bool/int/float/CKKEYBOARD，默认 NONE。严格 Read* 不做隐式转换；
普通 int 与键值不同。首次具体类型声明、NONE 初始化或不同底层类型的 SetDefault 可能替换
当前值，SetDefault(NONE) 也不是无操作。需要保护旧类型值时，先 NONE 声明再 Read 检查，
由 Mod 决定迁移。Definition 的 Choices 默认空数组，允许 null；宿主接受时复制。

视图、属性和控制 Ref 绑定某一次物理运行期。卸载和替换后永不复活，回滚也重新 Bind。
同一逻辑 owner 对已经结束的 Cancel/Release/Unregister 可幂等成功；其他 owner 始终无权
操作。失败不丢失原请求，退休后的收尾仍由原宿主负责。丢弃脚本 Ref 不等于取消；原生
C++ RAII 行为保持。键盘和 TimerEvent 是拥有捕获值的快照，可在退休后读取；原生键盘
span 和命令 sink 保留各自较短的借用寿命。诊断 Generation 属性不能用来判活。

12 个主回调、已有游戏事件名称和数值、回调时序保留。存在 OnGameEvent 时不再双发对应
独立事件；OnCommandEvent 对独立的命令前后通知同样优先。Gameplay 六种注册继续经过
OnLoad/Capture/Validate 和现有失败回滚，不绕到原生入口。

完整签名见 [API 声明](https://doyagu.github.io/BallanceModLoaderPlus/api/bml-script-mod-api.as)，逐项历史对照见下方的入口迁移清单。可运行写法见
[命令与配置示例](https://github.com/doyaGu/BallanceModLoaderPlus/blob/sdl/examples/script-mod/command-config/CommandConfig.mod.as)、
[输入示例](https://github.com/doyaGu/BallanceModLoaderPlus/blob/sdl/examples/script-mod/input-ui/InputUi.mod.as)及
[完整示例源码](assets/CompleteMod.mod.as)。

## 逐入口迁移清单


旧声明摘自 `53f928f043bff2e0cfb34ece63b044f4a0c18a22` 的 AngelScriptBindings.cpp
注册表，源行号对应该基线；新完整声明见 [API 声明](https://doyagu.github.io/BallanceModLoaderPlus/api/bml-script-mod-api.as)。以下覆盖本轮变更的核心
类型及已承诺保留的 Context 专用入口，没有“按需要兼容”的待定项。

### ModContext

| 当前完整声明 | 动作 | 最终入口或迁移步骤 | 基线源行 |
| --- | --- | --- | --- |
| `ModContext &opAssign(const ModContext &in)` | 保留行为 | 复制当前运行期弱状态；赋值不重新绑定 owner | 2747 |
| `bool get_HasContext() const` | 迁移 | ctx.IsValid() | 2748 |
| `bool HasContext() const` | 迁移 | ctx.IsValid() | 2749 |
| `string get_ModId() const` | 迁移 | ctx.Owner(out) → ModRef.Read(info) → Definition.Id/Name | 2750 |
| `string GetModId() const` | 迁移 | ctx.Owner(out) → ModRef.Read(info) → Definition.Id/Name | 2751 |
| `string get_ModName() const` | 迁移 | ctx.Owner(out) → ModRef.Read(info) → Definition.Id/Name | 2752 |
| `string GetModName() const` | 迁移 | ctx.Owner(out) → ModRef.Read(info) → Definition.Id/Name | 2753 |
| `void LogInfo(const string &in message) const` | 迁移 | ctx.LogInfo(message)，检查 code == ERROR_OK | 2754 |
| `void LogWarn(const string &in message) const` | 迁移 | ctx.LogWarn(message)，检查 code == ERROR_OK | 2755 |
| `void LogError(const string &in message) const` | 迁移 | ctx.LogError(message)，检查 code == ERROR_OK | 2756 |
| `bool get_IsReloading() const` | 迁移到单一属性 | `bool get_IsReloading() const`；移除重复方法；不判活 | 2757 |
| `bool IsReloading() const` | 迁移到单一属性 | `bool get_IsReloading() const`；移除重复方法；不判活 | 2758 |
| `ReloadPhase get_ReloadPhase() const` | 迁移到单一属性 | `ReloadPhase get_ReloadPhase() const`；移除重复方法；不判活 | 2759 |
| `ReloadPhase GetReloadPhase() const` | 迁移到单一属性 | `ReloadPhase get_ReloadPhase() const`；移除重复方法；不判活 | 2760 |
| `uint get_ReloadAttemptId() const` | 迁移到单一属性 | `uint get_ReloadAttemptId() const`；移除重复方法；不判活 | 2761 |
| `uint GetReloadAttemptId() const` | 迁移到单一属性 | `uint get_ReloadAttemptId() const`；移除重复方法；不判活 | 2762 |
| `uint get_ModGeneration() const` | 迁移到单一属性 | `uint get_ModGeneration() const`；移除重复方法；不判活 | 2763 |
| `uint GetModGeneration() const` | 迁移到单一属性 | `uint get_ModGeneration() const`；移除重复方法；不判活 | 2764 |
| `uint get_RuntimeGeneration() const` | 迁移到单一属性 | `uint get_RuntimeGeneration() const`；移除重复方法；不判活 | 2765 |
| `uint GetRuntimeGeneration() const` | 迁移到单一属性 | `uint get_RuntimeGeneration() const`；移除重复方法；不判活 | 2766 |
| `CKContext@ BorrowCKContext() const` | 迁移 | Engine::BorrowContext(ctx, out) | 2767 |
| `CKRenderContext@ BorrowRenderContext() const` | 迁移 | Engine::BorrowRenderContext(ctx, out) | 2768 |
| `CKAttributeManager@ BorrowAttributeManager() const` | 迁移 | Engine::Borrow对应Manager(ctx, out)，完整签名见当前 API 声明 | 2769 |
| `CKBehaviorManager@ BorrowBehaviorManager() const` | 迁移 | Engine::Borrow对应Manager(ctx, out)，完整签名见当前 API 声明 | 2770 |
| `CKCollisionManager@ BorrowCollisionManager() const` | 迁移 | Engine::Borrow对应Manager(ctx, out)，完整签名见当前 API 声明 | 2771 |
| `InputHook@ BorrowInputManager() const` | 迁移 | ctx.Input() + ReadKeyboard/Mouse/Joystick | 2772 |
| `CKMessageManager@ BorrowMessageManager() const` | 迁移 | Engine::Borrow对应Manager(ctx, out)，完整签名见当前 API 声明 | 2773 |
| `CKPathManager@ BorrowPathManager() const` | 迁移 | Engine::Borrow对应Manager(ctx, out)，完整签名见当前 API 声明 | 2774 |
| `CKParameterManager@ BorrowParameterManager() const` | 迁移 | Engine::Borrow对应Manager(ctx, out)，完整签名见当前 API 声明 | 2775 |
| `CKRenderManager@ BorrowRenderManager() const` | 迁移 | Engine::Borrow对应Manager(ctx, out)，完整签名见当前 API 声明 | 2776 |
| `CKSoundManager@ BorrowSoundManager() const` | 迁移 | Engine::Borrow对应Manager(ctx, out)，完整签名见当前 API 声明 | 2777 |
| `CKTimeManager@ BorrowTimeManager() const` | 迁移 | Engine::Borrow对应Manager(ctx, out)，完整签名见当前 API 声明 | 2778 |
| `bool get_IsInGame() const` | 迁移 | Gameplay::ReadState(out) 中 Ingame/Paused/Playing | 2779 |
| `bool GetIsInGame() const` | 迁移 | Gameplay::ReadState(out) 中 Ingame/Paused/Playing | 2780 |
| `bool get_IsInLevel() const` | 迁移 | Gameplay::ReadState(out)；InLevel = Playing \|\| Paused | 2781 |
| `bool GetIsInLevel() const` | 迁移 | Gameplay::ReadState(out)；InLevel = Playing \|\| Paused | 2782 |
| `bool get_IsPaused() const` | 迁移 | Gameplay::ReadState(out) 中 Ingame/Paused/Playing | 2783 |
| `bool GetIsPaused() const` | 迁移 | Gameplay::ReadState(out) 中 Ingame/Paused/Playing | 2784 |
| `bool get_IsPlaying() const` | 迁移 | Gameplay::ReadState(out) 中 Ingame/Paused/Playing | 2785 |
| `bool GetIsPlaying() const` | 迁移 | Gameplay::ReadState(out) 中 Ingame/Paused/Playing | 2786 |
| `bool get_IsCheatEnabled() const` | 迁移 | Gameplay::ReadCheatEnabled(out) | 2787 |
| `bool GetIsCheatEnabled() const` | 迁移 | Gameplay::ReadCheatEnabled(out) | 2788 |
| `void EnableCheat(bool enable) const` | 迁移 | Gameplay::EnableCheat(ctx, enabled) | 2789 |
| `void ExitGame() const` | 迁移 | Gameplay::ExitGame(ctx) | 2790 |
| `float GetSRScore() const` | 迁移 | Speedrun::ReadTimerState(out) 后读取 ElapsedTime | 2791 |
| `int GetHSScore() const` | 迁移 | Gameplay::ReadHighScore(out) | 2792 |
| `string GetDirectoryUtf8(int type) const` | 迁移 | ctx.GetDirectory(type, out) | 2793 |
| `float GetTimeMs() const` | 迁移 | Time::ReadClock(out) 后读取 TimeMs | 2794 |
| `float GetAbsoluteTimeMs() const` | 迁移 | Time::ReadClock(out) 后读取 AbsoluteMs | 2795 |
| `float GetDeltaTimeMs() const` | 迁移 | Time::ReadClock(out) 后读取 DeltaMs | 2796 |
| `uint GetFrameCount() const` | 迁移 | Time::ReadClock(out) 后读取 MainTickCount | 2797 |
| `CKDataArray@ BorrowDataArrayByName(const string &in name) const` | 迁移 | Scene::FindObject(name, classId, ref) + Engine::BorrowObject(ctx, ref, classId, out) | 2798 |
| `CKGroup@ BorrowGroupByName(const string &in name) const` | 迁移 | Scene::FindObject(name, classId, ref) + Engine::BorrowObject(ctx, ref, classId, out) | 2799 |
| `CKMaterial@ BorrowMaterialByName(const string &in name) const` | 迁移 | Scene::FindObject(name, classId, ref) + Engine::BorrowObject(ctx, ref, classId, out) | 2800 |
| `CKMesh@ BorrowMeshByName(const string &in name) const` | 迁移 | Scene::FindObject(name, classId, ref) + Engine::BorrowObject(ctx, ref, classId, out) | 2801 |
| `CK2dEntity@ Borrow2dEntityByName(const string &in name) const` | 迁移 | Scene::FindObject(name, classId, ref) + Engine::BorrowObject(ctx, ref, classId, out) | 2802 |
| `CK3dEntity@ Borrow3dEntityByName(const string &in name) const` | 迁移 | Scene::FindObject(name, classId, ref) + Engine::BorrowObject(ctx, ref, classId, out) | 2803 |
| `CK3dObject@ Borrow3dObjectByName(const string &in name) const` | 迁移 | Scene::FindObject(name, classId, ref) + Engine::BorrowObject(ctx, ref, classId, out) | 2804 |
| `CKCamera@ BorrowCameraByName(const string &in name) const` | 迁移 | Scene::FindObject(name, classId, ref) + Engine::BorrowObject(ctx, ref, classId, out) | 2805 |
| `CKTargetCamera@ BorrowTargetCameraByName(const string &in name) const` | 迁移 | Scene::FindObject(name, classId, ref) + Engine::BorrowObject(ctx, ref, classId, out) | 2806 |
| `CKLight@ BorrowLightByName(const string &in name) const` | 迁移 | Scene::FindObject(name, classId, ref) + Engine::BorrowObject(ctx, ref, classId, out) | 2807 |
| `CKTargetLight@ BorrowTargetLightByName(const string &in name) const` | 迁移 | Scene::FindObject(name, classId, ref) + Engine::BorrowObject(ctx, ref, classId, out) | 2808 |
| `CKSound@ BorrowSoundByName(const string &in name) const` | 迁移 | Scene::FindObject(name, classId, ref) + Engine::BorrowObject(ctx, ref, classId, out) | 2809 |
| `CKTexture@ BorrowTextureByName(const string &in name) const` | 迁移 | Scene::FindObject(name, classId, ref) + Engine::BorrowObject(ctx, ref, classId, out) | 2810 |
| `CKBehavior@ BorrowScriptByName(const string &in name) const` | 迁移 | Scene::FindObject(name, classId, ref) + Engine::BorrowObject(ctx, ref, classId, out) | 2811 |
| `TimerRef@ AddTimer(Timer@+ timer) const` | 迁移 | ctx.Timers().Register(definition, @object.Tick, out) | 2812 |
| `TimerRef@ SetTimeoutTicks(uint delayTicks, TimerCallback@+ callback, const string &in name = "") const` | 迁移 | ctx.Timers().SetTimeoutTicks(uint64, callback, out, name) | 2813 |
| `TimerRef@ SetTimeout(float delayMs, TimerCallback@+ callback, const string &in name = "") const` | 迁移 | ctx.Timers().SetTimeout(double, callback, out, name) | 2814 |
| `TimerRef@ SetIntervalTicks(uint delayTicks, TimerLoopCallback@+ callback, const string &in name = "") const` | 迁移 | ctx.Timers().SetIntervalTicks(uint64, callback, out, name) | 2815 |
| `TimerRef@ SetInterval(float delayMs, TimerLoopCallback@+ callback, const string &in name = "") const` | 迁移 | ctx.Timers().SetInterval(double, callback, out, name) | 2816 |
| `CommandRef@ RegisterCommand(Command@+ command) const` | 迁移 | ctx.Commands().Register(definition, @Execute, out, @Complete) | 2817 |
| `CommandRef@ RegisterCommand(const CommandDefinition &in definition, CommandCallback@+ execute, CommandCompletionCallback@+ complete = null) const` | 迁移 | ctx.Commands().Register(definition, @Execute, out, @Complete) | 2818 |
| `bool UnregisterCommand(const string &in name) const` | 迁移 | CommandRef.Unregister()；自行保存注册 Ref | 2819 |
| `MenuPageRef@ RegisterMenuPage(const MenuPageDefinition &in definition, MenuPageDraw@+ draw, MenuPageEnter@+ enter = null, MenuPageLeave@+ leave = null) const` | 迁移 | ctx.RegisterMenuPage(definition, draw, out, enter, leave) | 2820 |
| `bool UnregisterMenuPage(const string &in id) const` | 迁移 | MenuPageRef.Unregister()；自行保存注册 Ref | 2821 |
| `DataShareRequestRef@ RequestDataShare(DataShareRequest@+ request) const` | 保留 | DataShareRequestRef@ RequestDataShare(DataShareRequest@+ request) const，现有专用能力 | 2822 |
| `DataShareRequestRef@ RequestDataShare(const string &in key, int type, DataShareCallback@+ callback, const string &in name = "") const` | 保留 | DataShareRequestRef@ RequestDataShare(const string &in key, int type, DataShareCallback@+ callback, const string &in name = "") const，现有专用能力 | 2823 |
| `int _IsImcRpcAvailable(const string &in route, bool &out available) const` | 保留 | int _IsImcRpcAvailable(const string &in route, bool &out available) const，现有专用能力 | 2824 |
| `ImcRequestRef@ _CallImc(const string &in route, const string &in requestPayload, const string &in responsePayload, const BML::Detail::ImcRecord &in request, BML::Detail::ImcCompletion@+ callback, uint timeoutMs = 5000) const` | 保留 | ImcRequestRef@ _CallImc(const string &in route, const string &in requestPayload, const string &in responsePayload, const BML::Detail::ImcRecord &in request, BML::Detail::ImcCompletion@+ callback, uint timeoutMs = 5000) const，现有专用能力 | 2825 |
| `ImcSubscriptionRef@ _SubscribeImc(const string &in topic, const string &in payload, BML::Detail::ImcTopicCallback@+ callback, uint capacity = 256) const` | 保留 | ImcSubscriptionRef@ _SubscribeImc(const string &in topic, const string &in payload, BML::Detail::ImcTopicCallback@+ callback, uint capacity = 256) const，现有专用能力 | 2826 |
| `int _PublishImc(const string &in topic, const string &in payload, const BML::Detail::ImcRecord &in message, uint64 &out delivered) const` | 保留 | int _PublishImc(const string &in topic, const string &in payload, const BML::Detail::ImcRecord &in message, uint64 &out delivered) const，现有专用能力 | 2827 |
| `int _GetImcSubscriberCount(const string &in topic, uint64 &out count) const` | 保留 | int _GetImcSubscriberCount(const string &in topic, uint64 &out count) const，现有专用能力 | 2828 |
| `BML::Detail::ImcProviderRef@ _OpenImcProvider() const` | 保留 | BML::Detail::ImcProviderRef@ _OpenImcProvider() const，现有专用能力 | 2829 |
| `HookBlockRef@ CreateHookBlock(CKBehavior@ ownerScript, HookBlockCallback@+ callback, const string &in name = "", int inputCount = 1, int outputCount = 1) const` | 保留 | HookBlockRef@ CreateHookBlock(CKBehavior@ ownerScript, HookBlockCallback@+ callback, const string &in name = "", int inputCount = 1, int outputCount = 1) const，现有专用能力 | 2830 |
| `HookBlockRef@ InsertHookBlockAfter(CKBehavior@ ownerScript, CKBehavior@ source, HookBlockCallback@+ callback, const string &in name = "", int sourceOutput = 0, int targetInput = -1) const` | 保留 | HookBlockRef@ InsertHookBlockAfter(CKBehavior@ ownerScript, CKBehavior@ source, HookBlockCallback@+ callback, const string &in name = "", int sourceOutput = 0, int targetInput = -1) const，现有专用能力 | 2831 |
| `HookBlockRef@ InsertHookBlockBefore(CKBehavior@ ownerScript, CKBehavior@ target, HookBlockCallback@+ callback, const string &in name = "", int sourceOutput = -1, int targetInput = 0) const` | 保留 | HookBlockRef@ InsertHookBlockBefore(CKBehavior@ ownerScript, CKBehavior@ target, HookBlockCallback@+ callback, const string &in name = "", int sourceOutput = -1, int targetInput = 0) const，现有专用能力 | 2832 |
| `HookBlockRef@ InsertHookBlockBetween(CKBehavior@ ownerScript, CKBehavior@ source, CKBehavior@ target, HookBlockCallback@+ callback, const string &in name = "", int sourceOutput = 0, int targetInput = 0) const` | 保留 | HookBlockRef@ InsertHookBlockBetween(CKBehavior@ ownerScript, CKBehavior@ source, CKBehavior@ target, HookBlockCallback@+ callback, const string &in name = "", int sourceOutput = 0, int targetInput = 0) const，现有专用能力 | 2833 |
| `bool RegisterBallType(const BallTypeDefinition &in definition) const` | 迁移 | Gameplay::RegisterBallType(ctx, definition) | 2834 |
| `bool RegisterFloorType(const FloorTypeDefinition &in definition) const` | 迁移 | Gameplay::RegisterFloorType(ctx, definition) | 2835 |
| `bool RegisterModule(const ModuleBallDefinition &in definition) const` | 迁移 | Gameplay::RegisterModuleBall(ctx, definition) | 2836 |
| `bool RegisterModule(const ModuleConvexDefinition &in definition) const` | 迁移 | Gameplay::RegisterModuleConvex(ctx, definition) | 2837 |
| `bool RegisterModule(const TrafoDefinition &in definition) const` | 迁移 | Gameplay::RegisterTrafo(ctx, definition) | 2838 |
| `bool RegisterModule(const ModuleDefinition &in definition) const` | 迁移 | Gameplay::RegisterModule(ctx, definition) | 2839 |
| `string GetModRootUtf8() const` | 迁移 | ctx.GetDirectory(DIR_MOD_ROOT, out)，与原入口根等价 | 2840 |
| `string ResolveModPathUtf8(const string &in relativePath) const` | 迁移 | ctx.GetResourcePath(relative, out)，新入口要求文件存在 | 2841 |
| `bool ModFileExistsUtf8(const string &in relativePath) const` | 迁移 | ctx.ResourceExists(relative, out) | 2842 |
| `bool ModDirectoryExistsUtf8(const string &in relativePath) const` | 迁移 | Path::IsDirectory(由 GetDirectory 与 Path::Combine 得到的目录路径) | 2843 |
| `string ReadModTextFileUtf8(const string &in relativePath, const string &in defaultValue = "") const` | 迁移 | ctx.ReadResource(relative, bytes)，按 UTF-8 解码 | 2844 |
| `int GetCommandCount() const` | 迁移 | ctx.Commands().ReadCommands(commands) 后读取 length | 2845 |
| `string GetCommandName(int index) const` | 迁移 | CommandInfo.Name | 2846 |
| `string GetCommandAlias(int index) const` | 迁移 | CommandInfo.Alias | 2847 |
| `string GetCommandDescription(int index) const` | 迁移 | CommandInfo.Description | 2848 |
| `bool HasCommand(const string &in name) const` | 迁移 | ctx.Commands().Find(name, info)，NOT_FOUND 表示缺失 | 2849 |
| `bool IsCommandCheat(const string &in name) const` | 迁移 | ctx.Commands().Find(name, info) 后读取 Cheat | 2850 |
| `ModRef@ FindMod(const string &in id) const` | 迁移 | ctx.FindMod(id, out) | 2851 |
| `int GetModCount() const` | 迁移 | ctx.ReadMods(mods) 后读取 length | 2852 |
| `string GetModId(int index) const` | 迁移 | ctx.ReadMods(mods) 的 Id | 2853 |
| `ModRef@ GetMod(int index) const` | 迁移 | ctx.ReadMods(mods) 后按索引读取 ModInfo | 2854 |
| `Logger@ BorrowLogger() const` | 迁移 | ctx.LogInfo/LogWarn/LogError | 2855 |
| `Config@ BorrowConfig() const` | 迁移 | ctx.Config() | 2856 |
| `void ExecuteCommand(const string &in command) const` | 迁移 | ctx.Commands().ExecuteLine(line, status) | 2857 |
| `void SkipRenderForNextTick() const` | 迁移 | UI::SkipNextRender(ctx) | 2858 |

### Logger

| 当前完整声明 | 动作 | 最终入口或迁移步骤 | 基线源行 |
| --- | --- | --- | --- |
| `bool get_IsValid() const` | 移除类型，迁移能力 | ctx.LogInfo/LogWarn/LogError，无 Logger 作者对象 | 2703 |
| `bool IsValid() const` | 移除类型，迁移能力 | ctx.LogInfo/LogWarn/LogError，无 Logger 作者对象 | 2704 |
| `void Info(const string &in message) const` | 移除类型，迁移能力 | ctx.LogInfo/LogWarn/LogError，无 Logger 作者对象 | 2705 |
| `void Warn(const string &in message) const` | 移除类型，迁移能力 | ctx.LogInfo/LogWarn/LogError，无 Logger 作者对象 | 2706 |
| `void Error(const string &in message) const` | 移除类型，迁移能力 | ctx.LogInfo/LogWarn/LogError，无 Logger 作者对象 | 2707 |

### Config

| 当前完整声明 | 动作 | 最终入口或迁移步骤 | 基线源行 |
| --- | --- | --- | --- |
| `bool get_IsValid() const` | 迁移 | IsValid() | 2708 |
| `bool IsValid() const` | 保留 | IsValid() | 2709 |
| `bool HasCategory(const string &in category) const` | 迁移 | HasCategory(category, out) | 2710 |
| `bool HasKey(const string &in category, const string &in key) const` | 迁移 | HasKey(category, key, out) | 2711 |
| `ConfigProperty@ GetProperty(const string &in category, const string &in key) const` | 迁移 | DeclareProperty(definition, out) 或 FindProperty(category, key, out) | 2712 |
| `void SetCategoryComment(const string &in category, const string &in comment) const` | 迁移 | SetCategoryComment(category, comment) 返回 code | 2713 |

### ConfigProperty

| 当前完整声明 | 动作 | 最终入口或迁移步骤 | 基线源行 |
| --- | --- | --- | --- |
| `bool get_IsValid() const` | 迁移 | IsValid() | 2714 |
| `bool IsValid() const` | 保留 | bool IsValid() const | 2715 |
| `ConfigPropertyType get_Type() const` | 迁移 | Read(info) → CurrentValue.Type | 2716 |
| `ConfigPropertyType GetType() const` | 迁移 | Read(info) → CurrentValue.Type | 2717 |
| `ConfigPropertyEditor get_Editor() const` | 迁移 | Read(info) → Definition.Editor | 2718 |
| `ConfigPropertyEditor GetEditor() const` | 迁移 | Read(info) → Definition.Editor | 2719 |
| `string GetString(const string &in defaultValue = "") const` | 保留 | string GetString(const string &in defaultValue = "") const | 2720 |
| `bool GetBoolean(bool defaultValue = false) const` | 保留 | bool GetBoolean(bool defaultValue = false) const | 2721 |
| `int GetInteger(int defaultValue = 0) const` | 保留 | int GetInteger(int defaultValue = 0) const | 2722 |
| `float GetFloat(float defaultValue = 0.0f) const` | 保留 | float GetFloat(float defaultValue = 0.0f) const | 2723 |
| `CKKEYBOARD GetKey(CKKEYBOARD defaultValue = 0) const` | 保留 | CKKEYBOARD GetKey(CKKEYBOARD defaultValue = 0) const | 2724 |
| `void SetString(const string &in value) const` | 迁移 | Set(ConfigValue(value))，检查 code | 2725 |
| `void SetBoolean(bool value) const` | 迁移 | Set(ConfigValue(value))，检查 code | 2726 |
| `void SetInteger(int value) const` | 迁移 | Set(ConfigValue(value))，检查 code | 2727 |
| `void SetFloat(float value) const` | 迁移 | Set(ConfigValue(value))，检查 code | 2728 |
| `void SetKey(CKKEYBOARD value) const` | 迁移 | Set(ConfigValue(value))，检查 code | 2729 |
| `void SetComment(const string &in comment) const` | 迁移 | SetComment(...) 返回 code，code == ERROR_OK 才成功 | 2730 |
| `void SetEditor(ConfigPropertyEditor editor) const` | 迁移 | SetEditor(...) 返回 code，code == ERROR_OK 才成功 | 2731 |
| `bool SetChoices(const array<string> &in choices) const` | 迁移 | SetChoices(...) 返回 code，code == ERROR_OK 才成功 | 2732 |
| `void SetDefaultString(const string &in value) const` | 迁移 | SetDefault(ConfigValue(value))，检查 code | 2733 |
| `void SetDefaultBoolean(bool value) const` | 迁移 | SetDefault(ConfigValue(value))，检查 code | 2734 |
| `void SetDefaultInteger(int value) const` | 迁移 | SetDefault(ConfigValue(value))，检查 code | 2735 |
| `void SetDefaultFloat(float value) const` | 迁移 | SetDefault(ConfigValue(value))，检查 code | 2736 |
| `void SetDefaultKey(CKKEYBOARD value) const` | 迁移 | SetDefault(ConfigValue(value))，检查 code | 2737 |

### InputHook

| 当前完整声明 | 动作 | 最终入口或迁移步骤 | 基线源行 |
| --- | --- | --- | --- |
| `bool get_IsValid() const` | 迁移 | ctx.Input().IsValid() | 2938 |
| `bool IsValid() const` | 迁移 | ctx.Input().IsValid() | 2939 |
| `void EnableKeyboardRepetition(bool enable = true) const` | 迁移 | Input.EnableKeyboardRepetition(...) 返回 code | 2940 |
| `bool IsKeyboardRepetitionEnabled() const` | 迁移 | 读取对应状态后访问 KeyboardState.RepetitionEnabled | 2941 |
| `bool IsKeyboardAttached() const` | 迁移 | 读取对应状态后访问 KeyboardState.Attached | 2942 |
| `bool IsMouseAttached() const` | 迁移 | 读取对应状态后访问 MouseState.Attached | 2943 |
| `bool IsJoystickAttached(int joystick) const` | 迁移 | 读取对应状态后访问 JoystickState.Attached | 2944 |
| `bool IsKeyDown(CKKEYBOARD key) const` | 迁移 | ReadKeyboard(out) 后 KeyboardState.IsKeyDown；stamp 由 ReadKey 读取 | 2945 |
| `bool IsKeyDown(CKKEYBOARD key, uint &out stamp) const` | 迁移 | ReadKeyboard(out) 后 KeyboardState.IsKeyDown；stamp 由 ReadKey 读取 | 2946 |
| `bool IsKeyUp(CKKEYBOARD key) const` | 迁移 | ReadKeyboard(out) 后 KeyboardState.IsKeyUp；stamp 由 ReadKey 读取 | 2947 |
| `bool IsKeyPressed(CKKEYBOARD key) const` | 迁移 | ReadKeyboard(out) 后 KeyboardState.IsKeyPressed；stamp 由 ReadKey 读取 | 2948 |
| `bool IsKeyReleased(CKKEYBOARD key) const` | 迁移 | ReadKeyboard(out) 后 KeyboardState.IsKeyReleased；stamp 由 ReadKey 读取 | 2949 |
| `bool IsKeyToggled(CKKEYBOARD key) const` | 迁移 | ReadKeyboard(out) 后 KeyboardState.IsKeyToggled；stamp 由 ReadKey 读取 | 2950 |
| `bool IsKeyToggled(CKKEYBOARD key, uint &out stamp) const` | 迁移 | ReadKeyboard(out) 后 KeyboardState.IsKeyToggled；stamp 由 ReadKey 读取 | 2951 |
| `string GetKeyName(CKKEYBOARD key) const` | 迁移 | Input.GetKeyName(input, out)，检查 code | 2952 |
| `int GetKeyFromName(const string &in name) const` | 迁移 | Input.GetKeyFromName(input, out)，检查 code | 2953 |
| `int GetKeyboardState(CKKEYBOARD key) const` | 迁移 | KeyboardState.ReadKey(key, state).Down，不再依赖旧 byte 编码 | 2954 |
| `bool IsKeyboardStateDown(CKKEYBOARD key) const` | 迁移 | KeyboardState.ReadKey(key, state).Down，不再依赖旧 byte 编码 | 2955 |
| `int GetNumberOfKeyInBuffer() const` | 迁移 | KeyboardState.EventCount | 2956 |
| `int GetKeyFromBuffer(int index, CKKEYBOARD &out key, uint &out timestamp) const` | 迁移 | KeyboardState.ReadEvent(index, out) 的 Key/State/TimestampMs | 2957 |
| `bool IsMouseButtonDown(CK_MOUSEBUTTON button) const` | 迁移 | ReadMouse(out) 后访问 DownMask | 2958 |
| `bool IsMouseClicked(CK_MOUSEBUTTON button) const` | 迁移 | ReadMouse(out) 后访问 ClickedMask | 2959 |
| `bool IsMouseToggled(CK_MOUSEBUTTON button) const` | 迁移 | ReadMouse(out) 后访问 ToggledMask | 2960 |
| `int GetMouseButtonState(CK_MOUSEBUTTON button) const` | 迁移 | ReadMouse(out)：按钮字节低两位 = downBit + (toggledBit << 1) | 2961 |
| `Vx2DVector GetMousePosition(bool absolute = true) const` | 迁移 | ReadMouse(out) 后访问 ScreenPosition/WindowPosition | 2962 |
| `Vx2DVector GetLastMousePosition() const` | 迁移 | ReadMouse(out) 后访问 LastPosition | 2963 |
| `VxVector GetMouseRelativePosition() const` | 迁移 | ReadMouse(out) 后访问 RelativePosition | 2964 |
| `VxVector GetJoystickPosition(int joystick) const` | 迁移 | ReadJoystick(index, out) 后访问 Position | 2965 |
| `VxVector GetJoystickRotation(int joystick) const` | 迁移 | ReadJoystick(index, out) 后访问 Rotation | 2966 |
| `Vx2DVector GetJoystickSliders(int joystick) const` | 迁移 | ReadJoystick(index, out) 后访问 Sliders | 2967 |
| `float GetJoystickPointOfViewAngle(int joystick) const` | 迁移 | ReadJoystick(index, out) 后访问 PointOfViewAngle | 2968 |
| `uint GetJoystickButtonsState(int joystick) const` | 迁移 | ReadJoystick(index, out) 后访问 Buttons | 2969 |
| `bool IsJoystickButtonDown(int joystick, int button) const` | 迁移 | ReadJoystick(index, out) 后访问 Buttons 按位检查 | 2970 |
| `void Pause(bool pause) const` | 迁移 | Input.Pause(...) 返回 code | 2971 |
| `void ShowCursor(bool show) const` | 迁移 | Input.ShowCursor(...) 返回 code | 2972 |
| `bool GetCursorVisibility() const` | 迁移 | 读取对应状态后访问 CursorState.Visible | 2973 |
| `int GetSystemCursor() const` | 迁移 | 读取对应状态后访问 CursorState.Shape | 2974 |
| `void SetSystemCursor(int cursor) const` | 迁移 | Input.SetSystemCursor(...) 返回 code | 2975 |
| `bool IsBlock() const` | 迁移 | Input.ReadBlockCount(device, out)，count != 0 表示屏蔽 | 2976 |
| `void SetBlock(bool block) const` | 迁移 | Input.AcquireBlock(mask, out) + InputBlock.Release()，保存每个拥有的 Ref | 2977 |
| `int IsBlocked(InputDevice device) const` | 迁移 | Input.ReadBlockCount(device, out)，count != 0 表示屏蔽 | 2978 |
| `void Block(InputDevice device) const` | 迁移 | Input.AcquireBlock(mask, out) + InputBlock.Release()，保存每个拥有的 Ref | 2979 |
| `void Unblock(InputDevice device) const` | 迁移 | Input.AcquireBlock(mask, out) + InputBlock.Release()，保存每个拥有的 Ref | 2980 |
| `uint64 AcquireBlock(uint mask) const` | 迁移 | Input.AcquireBlock(mask, out) + InputBlock.Release()，保存每个拥有的 Ref | 2981 |
| `void ReleaseBlock(uint64 token) const` | 迁移 | Input.AcquireBlock(mask, out) + InputBlock.Release()，保存每个拥有的 Ref | 2982 |

### TimerRef

| 当前完整声明 | 动作 | 最终入口或迁移步骤 | 基线源行 |
| --- | --- | --- | --- |
| `bool get_IsValid() const` | 迁移 | TimerRef.Read(TimerInfo &out)；Id 改 uint64 Handle，其余对应字段 | 2876 |
| `bool IsValid() const` | 保留 | bool IsValid() const | 2877 |
| `int get_Id() const` | 迁移 | TimerRef.Read(TimerInfo &out)；Id 改 uint64 Handle，其余对应字段 | 2878 |
| `string get_Name() const` | 迁移 | TimerRef.Read(TimerInfo &out)；Id 改 uint64 Handle，其余对应字段 | 2879 |
| `int get_State() const` | 迁移 | TimerRef.Read(TimerInfo &out)；Id 改 uint64 Handle，其余对应字段 | 2880 |
| `int get_CompletedIterations() const` | 迁移 | TimerRef.Read(TimerInfo &out)；Id 改 uint64 Handle，其余对应字段 | 2881 |
| `int get_RemainingIterations() const` | 迁移 | TimerRef.Read(TimerInfo &out)；Id 改 uint64 Handle，其余对应字段 | 2882 |
| `float get_Progress() const` | 迁移 | TimerRef.Read(TimerInfo &out)；Id 改 uint64 Handle，其余对应字段 | 2883 |
| `void Pause()` | 迁移 | Pause() 返回 code，只判断 code == ERROR_OK | 2884 |
| `void Resume()` | 迁移 | Resume() 返回 code，只判断 code == ERROR_OK | 2885 |
| `void Cancel()` | 迁移 | Cancel() 返回 code，只判断 code == ERROR_OK | 2886 |

### TimerEvent

| 当前完整声明 | 动作 | 最终入口或迁移步骤 | 基线源行 |
| --- | --- | --- | --- |
| `TimerEvent &opAssign(const TimerEvent &in)` | 保留行为 | 复制捕获的完整 TimerInfo 和有效标记；无运行期 weak-state，退休后仍可读取 | 2688 |
| `bool get_IsValid() const` | 迁移 | TimerEvent.Read(TimerInfo &out)，读取完整 Handle、计数、状态 | 2887 |
| `int get_Id() const` | 迁移 | TimerEvent.Read(TimerInfo &out)，读取完整 Handle、计数、状态 | 2888 |
| `string get_Name() const` | 迁移 | TimerEvent.Read(TimerInfo &out)，读取完整 Handle、计数、状态 | 2889 |
| `int get_State() const` | 迁移 | TimerEvent.Read(TimerInfo &out)，读取完整 Handle、计数、状态 | 2890 |
| `int get_Type() const` | 迁移 | TimerEvent.Read(TimerInfo &out)，读取完整 Handle、计数、状态 | 2891 |
| `int get_TimeBase() const` | 迁移 | TimerEvent.Read(TimerInfo &out)，读取完整 Handle、计数、状态 | 2892 |
| `int get_CompletedIterations() const` | 迁移 | TimerEvent.Read(TimerInfo &out)，读取完整 Handle、计数、状态 | 2893 |
| `int get_RemainingIterations() const` | 迁移 | TimerEvent.Read(TimerInfo &out)，读取完整 Handle、计数、状态 | 2894 |
| `float get_Progress() const` | 迁移 | TimerEvent.Read(TimerInfo &out)，读取完整 Handle、计数、状态 | 2895 |

### CommandRef

| 当前完整声明 | 动作 | 最终入口或迁移步骤 | 基线源行 |
| --- | --- | --- | --- |
| `bool get_IsValid() const` | 迁移 | CommandRef.IsValid() 或 Read(CommandInfo &out) 对应字段 | 3046 |
| `string get_Name() const` | 迁移 | CommandRef.IsValid() 或 Read(CommandInfo &out) 对应字段 | 3047 |
| `string get_Alias() const` | 迁移 | CommandRef.IsValid() 或 Read(CommandInfo &out) 对应字段 | 3048 |
| `bool get_IsCheat() const` | 迁移 | CommandRef.IsValid() 或 Read(CommandInfo &out) 对应字段 | 3049 |
| `bool get_IsEnabled() const` | 迁移 | CommandRef.IsValid() 或 Read(CommandInfo &out) 对应字段 | 3050 |
| `bool SetEnabled(bool enabled)` | 迁移 | SetEnabled(...) 返回 code；修改旧 if(result) | 3051 |
| `bool Unregister()` | 迁移 | Unregister(...) 返回 code；修改旧 if(result) | 3052 |

### CommandCompletion

| 当前完整声明 | 动作 | 最终入口或迁移步骤 | 基线源行 |
| --- | --- | --- | --- |
| `void Add(const string &in value) const` | 迁移 | Add(value) 返回 code | 3043 |
| `int get_Count() const` | 迁移 | ReadCount(uint &out) | 3044 |
| `string At(int index) const` | 迁移 | ReadCandidate(uint, string &out) | 3045 |

### MenuPageRef

| 当前完整声明 | 动作 | 最终入口或迁移步骤 | 基线源行 |
| --- | --- | --- | --- |
| `bool get_IsValid() const` | 迁移 | IsValid() | 3057 |
| `string get_Id() const` | 迁移 | string get_Id() const | 3058 |
| `bool Unregister()` | 迁移 | int Unregister()，检查 code == ERROR_OK | 3059 |

### ModRef

| 当前完整声明 | 动作 | 最终入口或迁移步骤 | 基线源行 |
| --- | --- | --- | --- |
| `string get_Id() const` | 迁移 | ModRef.IsValid()/Read(ModInfo &out)，依赖用 Definition.ReadDependency | 2983 |
| `string GetId() const` | 迁移 | ModRef.IsValid()/Read(ModInfo &out)，依赖用 Definition.ReadDependency | 2984 |
| `string get_Name() const` | 迁移 | ModRef.IsValid()/Read(ModInfo &out)，依赖用 Definition.ReadDependency | 2985 |
| `string get_Version() const` | 迁移 | ModRef.IsValid()/Read(ModInfo &out)，依赖用 Definition.ReadDependency | 2986 |
| `string get_Author() const` | 迁移 | ModRef.IsValid()/Read(ModInfo &out)，依赖用 Definition.ReadDependency | 2987 |
| `string get_Description() const` | 迁移 | ModRef.IsValid()/Read(ModInfo &out)，依赖用 Definition.ReadDependency | 2988 |
| `string get_BMLVersion() const` | 迁移 | ModRef.IsValid()/Read(ModInfo &out)，依赖用 Definition.ReadDependency | 2989 |
| `int get_BMLVersionMajor() const` | 迁移 | ModRef.IsValid()/Read(ModInfo &out)，依赖用 Definition.ReadDependency | 2990 |
| `int get_BMLVersionMinor() const` | 迁移 | ModRef.IsValid()/Read(ModInfo &out)，依赖用 Definition.ReadDependency | 2991 |
| `int get_BMLVersionPatch() const` | 迁移 | ModRef.IsValid()/Read(ModInfo &out)，依赖用 Definition.ReadDependency | 2992 |
| `int get_Kind() const` | 迁移 | ModRef.IsValid()/Read(ModInfo &out)，依赖用 Definition.ReadDependency | 2993 |
| `int get_State() const` | 迁移 | ModRef.IsValid()/Read(ModInfo &out)，依赖用 Definition.ReadDependency | 2994 |
| `bool get_IsValid() const` | 迁移 | ModRef.IsValid()/Read(ModInfo &out)，依赖用 Definition.ReadDependency | 2995 |
| `bool get_IsScript() const` | 迁移 | ModRef.IsValid()/Read(ModInfo &out)，依赖用 Definition.ReadDependency | 2996 |
| `bool get_IsFailed() const` | 迁移 | ModRef.IsValid()/Read(ModInfo &out)，依赖用 Definition.ReadDependency | 2997 |
| `int CheckDependencies() const` | 迁移 | `int CheckDependencies(bool &out satisfied) const`；分别检查 API code 与即时依赖结果，不用 consumer State 代替 | 2998 |
| `int GetDependencyCount() const` | 迁移 | ModRef.IsValid()/Read(ModInfo &out)，依赖用 Definition.ReadDependency | 2999 |
| `string GetDependencyId(int index) const` | 迁移 | ModRef.IsValid()/Read(ModInfo &out)，依赖用 Definition.ReadDependency | 3000 |
| `string GetDependencyVersion(int index) const` | 迁移 | ModRef.IsValid()/Read(ModInfo &out)，依赖用 Definition.ReadDependency | 3001 |
| `int GetDependencyVersionMajor(int index) const` | 迁移 | ModRef.IsValid()/Read(ModInfo &out)，依赖用 Definition.ReadDependency | 3002 |
| `int GetDependencyVersionMinor(int index) const` | 迁移 | ModRef.IsValid()/Read(ModInfo &out)，依赖用 Definition.ReadDependency | 3003 |
| `int GetDependencyVersionPatch(int index) const` | 迁移 | ModRef.IsValid()/Read(ModInfo &out)，依赖用 Definition.ReadDependency | 3004 |
| `bool IsDependencyOptional(int index) const` | 迁移 | ModRef.IsValid()/Read(ModInfo &out)，依赖用 Definition.ReadDependency | 3005 |
| `string get_Diagnostic() const` | 迁移 | ModRef.IsValid()/Read(ModInfo &out)，依赖用 Definition.ReadDependency | 3006 |
| `string GetDiagnostic() const` | 迁移 | ModRef.IsValid()/Read(ModInfo &out)，依赖用 Definition.ReadDependency | 3007 |

## 事件及专用功能

| 现状 | 最终入口 | 动作 |
| --- | --- | --- |
| 12 个主回调、游戏事件枚举值 | ScriptApiSurface.cpp 现有完整声明/数值 | 保留 |
| 原生独立游戏事件方法 | 同名脚本 void 方法(ctx)，与 OnGameEvent 二选一分发 | 脚本新增 |
| Command/CommandCallback/CommandCompletionCallback 对象注册协议 | Definition + CommandExecute/CommandComplete | 移除旧协议，不提供适配 |
| Timer 接口及反射 getter 协议 | TimerDefinition + delegate | 移除旧协议 |
| RenderEvent.GetFlags、CheatEvent.IsEnabled 等重复方法 | Flags、Enabled 标准只读属性 | 移除重复别名 |
| LoadObjectEvent.IsDynamic | Dynamic | 字段名迁移 |
| PhysicalizeEvent.AutoCalcMassCenter | CalcMassCenter | 字段名迁移 |
| PhysicalizeEvent.BallCount/GetBallCenter/GetBallRadius | SphereCount/GetSphereCenter/GetSphereRadius | 命名迁移，中心/半径含义不变 |
| CollisionGroup/CollisionSurface 的空字符串判断 | HasCollisionGroup/HasCollisionSurface + string 属性 | 区分 null/空字符串 |
| CommandEvent.CommandName/ArgCount/GetArg/ArgsText | Name/ArgumentCount/ReadArgument；文本按需在本地拼接 | 载荷迁移，通知点不变 |
| 事件 Borrow* | 同角色方法，内部使用 ObjectRef 校验 | 保留，无裸 ID 重新绑定 |
| 仅文档中的 Events::Stream | 无实际接口 | 删除过时声明，不实现新总线 |
| StateBag、ReloadPhase、DataShare、IMC、Behavior、CK、Physics、Text、Hook、UI 控件 | 当前真实注册的完整专用接口，本指南列出的管理函数变化除外 | 保留既有功能，不是兼容层 |

Gameplay 旧 Context 注册重载迁到Gameplay 命名空间的六个具名函数；每个函数都必须经过原
RegisterScript* 的 OnLoad/Capture/Validate 事务。原注册参数及定义类型不变。
UI 管理函数返回错误码，控件 bool 仍表示交互。Speedrun::GetElapsedTime 改为严格
ReadTimerState 的 ElapsedTime。诊断 Phase/Attempt/Generation 仅保留单一属性，不判活。

目录功能：DIR_MOD_ROOT 对脚本返回 GetEntry().RootDirectory，与旧 GetModRootUtf8
等价，不是 SetModDataPath 的数据路径。无需新增入口根读取函数。当前生产构造路径中
ResourceRootDirectory 与 RootDirectory 相同；不宣称 ZIP 存在另一个入口根。
GetResourcePath 仅返回存在文件；若旧 ResolveModPathUtf8 被用于构造尚未存在的文件或
目录路径，改用 GetDirectory(DIR_MOD_ROOT,out) 加现有 Path::Combine/Normalize，不能
以 GetResourcePath 当通用拼接器。目录存在检查仍使用现有 Path::IsDirectory。

旧 ByName 的 classId 使用原 CKCID，原管理器访问由 BML 的 Engine::Borrow对应Manager 投影复用 CKContext 已有方法，不
依赖 CKAngelScript 是否直接绑定该方法，也不新增引擎接口。

没有列为命名变化的现有事件成员继续作为正常接口保留；实施者不能据“去兼容”任意
删除字段含义或有用的 Borrow 查询。旧重复 getter 的具体移除见当前 API 声明。MassCenter 和 SphereCenter 从 VxVector
改为 BML::Vec3，逐分量复制；原 MassCenterX/Y/Z 改为读取 MassCenter.x/y/z。
