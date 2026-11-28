# Migrating to the aligned SDL authoring API

The native C ABI and its function table layouts are unchanged. C++ and script
source interfaces intentionally break compatibility. Rebuild native Mods and
build scripts from source with the matching SDK. No old overloads, aliases,
object registration adapters or old bytecode compatibility paths are provided.
The loader's current script path rebuilds source; this change does not add a
persistent bytecode cache.

| Previous entry | Current entry |
| --- | --- |
| C++ Command::Registration::Register | ctx.Commands().Register, returning Result<Registration> |
| C++ global Command::Find/Visit/ExecuteLine | ctx.Commands().Find/ReadCommands/ExecuteLine |
| C++ global Timer helpers / Registration::Register | ctx.Timers() helpers / Register |
| Script BorrowLogger / logger.Info | ctx.LogInfo/LogWarn/LogError |
| Script BorrowConfig / GetProperty | ctx.Config().DeclareProperty or FindProperty, with status and out |
| Script SetBoolean/SetDefaultBoolean and typed siblings | Set/SetDefault(ConfigValue), returning status |
| Script InputHook / BorrowInputManager | ctx.Input().ReadKeyboard/ReadMouse/ReadJoystick |
| Script SetBlock/Block/Unblock/raw tokens | AcquireBlock(mask, InputBlock &out), then Release |
| Script Timer / AddTimer / flat timeout helpers | ctx.Timers().Register/SetTimeout/SetInterval and tick variants |
| Script Command / CommandCallback / CommandCompletionCallback | CommandDefinition plus CommandExecute / CommandComplete delegates |
| Script flat command register/unregister-by-name | ctx.Commands().Register and CommandRef.Unregister |
| Script flat Mod lookup and metadata getters | ctx.Owner/FindMod/ReadMods; ModRef.Read(ModInfo &out) |
| Script ModRef.CheckDependencies() | CheckDependencies(bool &out satisfied), status separate from result |
| Script command event GetArg / ArgCount / CommandName | ReadArgument(index, string &out), ArgumentCount, Name, InvokedAs |
| Script LoadObjectEvent.IsDynamic | Dynamic |
| Script PhysicalizeEvent.AutoCalcMassCenter / Ball* | CalcMassCenter / SphereCount / GetSphereCenter / GetSphereRadius |
| Script physical event VxVector center | BML::Vec3; copy x/y/z when a CK VxVector is required |
| Script flat gameplay/state/clock/HUD/scene/manager methods | Gameplay / Time / Speedrun / UI / Scene / Engine namespaces |
| Only documented Events::Stream | Removed; no event stream was implemented |

C++ checks `Result` or calls `Take()`. Script checks `code == BML::ERROR_OK`;
`if (code)` reverses success. Command execute's nonnegative business status is
separate from `ExecuteLine`'s API status. Script Ref outputs are null on failure.

`ReadMods(? &out)` and `ReadCommands(? &out)` require exact arrays of
`BML::ModInfo` and `BML::CommandInfo`. Pass the array object, not a handle slot.
`ModDefinition.DependencyCount` and `ReadDependency` replace dependency arrays
and per-field version getters. Read records own their strings and values.
ConfigValue's strict readers reject NONE and incompatible types. Use a NONE
property declaration plus `Read` when preserving and inspecting a previously
loaded value before choosing a migration; a first typed declaration may replace
an incompatible loaded value.

Service views and control Refs belong to one physical bind. Rollback never
reactivates an old view. Script keyboard snapshots and timer notifications own
captures that remain readable after retirement. Native keyboard spans, native
event views and command sinks retain their shorter documented borrow limits.
Dropping script TimerRef/CommandRef/MenuPageRef/InputBlock does not end its
registration. Keep explicit cleanup or let the owning runtime retire it.

All twelve main callback names and signatures, existing game event names and
values, and callback scheduling remain unchanged. An `OnGameEvent` method takes
precedence over individual `OnPreStartMenu` through `OnPostLifeUp` methods.
`OnCommandEvent` similarly takes precedence over the independent pre/post
command notification methods. Each event is delivered once. Optional collision
group/surface presence is available separately from its possibly empty string.

See [services](services.md), the [registered reference](api.md), and the
[updated examples](https://github.com/doyaGu/BallanceModLoaderPlus/blob/sdl/examples/script-mod/README.md). The Chinese
[per-entry migration list](https://doyagu.github.io/BallanceModLoaderPlus/zh-CN/script-mod-tutorial/migration/)
records all 251 former core declarations against the approved baseline.
