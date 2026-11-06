# Migrate a Native Mod to the SDL C protocol

The SDL SDK now uses `BML_GetModInterface` with module-local C++ wrappers.
Recompile an existing SDL Mod; its former IMod/IBML binary cannot be loaded by
this protocol. The separate original Windows backend keeps its own ABI.

The implementation of the loader services and old UI families is retained
privately. This migration changes how a Mod reaches them.

| Previous usage | Current usage |
| --- | --- |
| `class Mod : IMod`, constructor taking `IBML*` | `class Mod : BML::Mod::Instance`, default constructor |
| GetID/GetName/GetVersion/GetAuthor/GetDescription/GetBMLVersion | Named fields in `BML_DEFINE_MOD` (or a `Mod::Definition` factory) |
| BMLEntry/BMLExit | `BML_DEFINE_MOD(Type, Factory)` or the C table in `Mod.h` |
| RegisterDependency/RegisterOptionalDependency | `Definition::Dependencies` before Create |
| OnLoad/OnUnload/OnProcess | Same names with `const Mod::Context &` |
| Game event overrides | Same names with Context; the default OnGameEvent dispatches them |
| Raw CK arguments to engine events | Named event records with checked ObjectRefs and borrowed spans |
| `GetLogger()->Info/Warn/Error` | `ctx.LogInfo/LogWarn/LogError` with UTF-8 text |
| GetConfig/GetProperty, IProperty | `Config::View(ctx)`, DeclareProperty/FindProperty and typed Property values |
| SetDefaultInteger/Float/Boolean/String/Key | `Property::SetDefault` with int32_t/float/bool/string/uint32_t |
| SetInteger/Float/Boolean/String/Key | `Property::Set` with the same typed value |
| AddTimer/AddTimerLoop | Retained `Timer::Registration`, explicit milliseconds/ticks |
| InputHook's ordinary/original queries | `Input::View(ctx).ReadKeyboard/ReadMouse/ReadJoystick`, optionally INPUT_RAW |
| Block/Unblock | Retained `Input::Block`; each acquisition is one counted share |
| GetCKContext/GetRenderContext | Optional `Engine::BorrowContext/BorrowRenderContext` |
| Get*ByName + immediate engine operations | Scene ObjectRefs, or optional CKContext public lookup |
| IsIngame/IsPaused/IsPlaying, cheat and scores | Gameplay state/cheat/energy/highscore interfaces |
| ExitGame, registration and render options | Gameplay methods taking explicit Context |
| SetIC/RestoreIC/Show | Scene methods taking Context and ObjectRef |
| Bui.h and BGui constructors | Bui.hpp/Gui.hpp with Context; advanced adapters are optional headers |
| ExecuteBB | Behavior Session/Blocks/Call/Task/Instance; ScriptHelper is a local engine helper |
| Public C++ provider type bootstrap | Declared provider dependency plus existing Interface.h/.hpp C tables |

Configuration values/files, Command, Behavior, IMC and DataShare retain their
existing formats and semantics. Do not rewrite IMC wire schemas or introduce a
new ownership rule for existing DataShare/Behavior callbacks.

## Removed compatibility interfaces

The source and installed public SDK no longer contain IMod.h, IBML.h,
IMessageReceiver.h, IConfig.h, ICommand.h, ILogger.h, InputHook.h, ExecuteBB.h or
ModInterface.hpp. The host's remaining lifecycle, config, command and input
adapters are private; they are not another native ABI. The old `MOD_EXPORT`
and `DECLARE_BML_VERSION` macros are removed. Use `BML_DEFINE_MOD` for C++ or
`BML_MOD_ENTRY(int)` for the C entry, with the minimum version in the definition.

| Removed export | Replacement |
| --- | --- |
| BML_GetLoaderPathW/Utf8, BML_GetModRootW/Utf8 | `ctx.GetDirectory(BML_DIR_*)` / `ctx.GetDirectory(BML_MOD_DIRECTORY_ROOT)` |
| BML_UnregisterCommand | Retained `Command::Registration::Unregister()` |
| BML_SetCommandStatus | Return the shell exit status from the command callback |
| BML_GetCommandInput | `Command::Invocation::Input()` |
| BML_Get/SetConfigPropertyEditor, BML_GetConfigPropertyChoiceCount/Choice, BML_SetConfigPropertyChoices | `Config::Property::Read()` / `SetEditor()` / `SetChoices()` |
| ExecuteBB C++ exports | Behavior Session/Blocks/Call/Task/Instance |

The `BML_DIR_*` ids remain available from `Mod.h`. Standalone allocation,
filesystem, ZIP and encoding utilities in `BML.h` remain available. Script
interfaces and configuration data formats are unchanged.

## Recommended ownership pattern

Keep Command/Timer/Input registrations as Instance members. Capture callback
Context by value only when needed across callbacks; preferably use the Context
supplied to the next callback. Do not capture an event string_view/span or raw
CK pointer for delayed work. Copy data, or retain an ObjectRef/CKObjectHandle and
resolve it again. Register business resources in OnLoad. OnUnload must tolerate
partial initialization and stop external threads. The loader clears remaining
owned SDK registrations before Destroy.

Timer Cancel is safe from its callback. Normal completion and repeated wrapper
Cancel are harmless; a real cancellation failure remains visible and retains
release responsibility. A raw C caller still gets INVALID_HANDLE for an unknown
handle. Script TimerRef dropping semantics are unchanged.

Core headers need only `BML::BML`; add `BML::Engine` and/or `BML::ImGui` explicitly
for direct CK/ImGui calls. Match those extensions' exact dependency/compiler ABI.
The loader process owns the shared engine, SDL and ImGui state.

The SDK templates and all twelve in-tree Mods have been ported as examples.
Release checks and unverified-target requirements are listed in
[the release guide](../../RELEASING.md#native-c-protocol-release-gate).
