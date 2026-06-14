# BML Script Facade Coverage

This document tracks the script-facing coverage of the native `IBML` surface.
The script facade is intentionally not a raw ABI dump: long-lived callbacks,
dependency mutation, and raw CKAngelScript handles stay outside the script API.
This matrix is about BML-owned facade coverage only. It is not a complete list
of what scripts can do in a BML+ script-capable release. BML script mods run
inside CKAngelScript, can coexist with CKAS runtime scripts and
`AngelScript Component`, and can use shared CKAS namespaces such as `Scene`,
`Behavior`, `BB`, `Param`, `Message`, `Async`, raw CK/Vx SDK bindings, advanced
FFI helpers, and other registered engine-extension namespaces.

## Relationship To CKAngelScript

BML does not try to mirror the whole CKAngelScript API. BML script mods are
CKAS-hosted modules with BML mod identity and BML services. CKAngelScript keeps
ownership of the AngelScript engine, runtime script manager, component Building
Block, object identity refs, behavior graph bridge, parameter bridge,
script-to-script messaging, async scheduler, raw CK/Vx bindings, and advanced
native memory/call helpers.

Use this matrix to answer "which native `IBML` feature is script-facing?" Do
not use it as the upper bound for what a script can do in Virtools.

## Placement Guidance

| Need | Put it here |
| --- | --- |
| BML mod identity, dependency ordering, resource paths, logger/config, commands, timers, exports, DataShare, HUD/menu helpers | BML script facade |
| Safe Virtools object lookup, scene membership, object identity across frames | CKAS `Scene` and `ObjectRef@`-derived handles; use BML `Borrow*ByName` only for convenience |
| Behavior graph search/editing, runtime Building Block spawning/stepping, CK parameter values/sources/operations | CKAS `Behavior`, `BB`, and `Param` |
| Per-object/per-behavior script logic with editor-configured fields | CKAS `AngelScript Component` |
| Runtime script/component communication and cooperative frame work | CKAS `Message` and `Async`; use BML exports/DataShare for stable mod-visible contracts |
| Renderer/input/platform hooks, high-frequency patches, or unsafe native lifetime | Native plugin code plus a guarded CKAS engine extension |
| External library calls or native memory experiments | CKAS `NativePointer`/DynCall as an advanced escape hatch, not a supported substitute for plugin APIs |

## Coverage Matrix

| Native surface | Script surface | Lifetime and notes |
| --- | --- | --- |
| `GetCKContext`, `GetRenderContext` | `ModContext::BorrowCKContext`, `ModContext::BorrowRenderContext` | Borrowed handles. Do not retain across level unload, reset, or mod unload. |
| CK manager getters | `ModContext::Borrow*Manager` | Borrowed handles. `BorrowInputManager` returns `BML::InputHook@`. |
| `InputHook` public input API | `InputHook` methods plus `InputDevice`, `InputKeyEvent`, `InputButtonState`, `CursorPointer` | Borrowed handle facade. Exposes keyboard repetition/state/buffer, mouse state/position/cursor, joystick reads, and balanced block controls. Omits raw `o*` bypass methods and lifecycle `Process()`. |
| `ExitGame` | `ModContext::ExitGame` | Available but not used by smoke tests because it terminates Player. |
| Native timer callbacks | `ModContext::AddTimer(BML::Timer@+)`, `TimerRef`, `TimerEvent` | Script-owned timer object implementing `BML::Timer`. BML retains the object until cancel/unload. No raw function pointer or userData. |
| `ILogger`, `IConfig`, `IProperty` | `ModContext::BorrowLogger`, `ModContext::BorrowConfig`, `Logger`, `Config`, `ConfigProperty` | Script handles are ref-counted wrappers that re-resolve the owning script mod. `Config.GetProperty(category, key)` mirrors native property creation. |
| Cheat and state | `ModContext` methods/properties | State reads are safe outside a level; mutating cheat state is guarded by BML runtime availability. |
| In-game message and command execution | `ModContext::SendIngameMessage`, `ModContext::ExecuteCommand` | Script command objects unregister on mod unload. |
| Command registration | `ModContext::RegisterCommand(BML::Command@+)`, `CommandRef`, `CommandCompletion` | Script-owned command object implementing the minimal `BML::Command` interface. BML retains the object until unregister/unload. Duplicate names or aliases fail without replacing existing commands. |
| `SetIC`, `RestoreIC`, `Show` | `BML::CK` helpers | Borrowed CK object input. Null object is a no-op. |
| Game state queries | `ModContext` methods/properties | Includes score, HUD, SR timer, pause/play/level flags, and time helpers. Input state is on `InputHook`. |
| Named CK object lookups | `ModContext::Borrow*ByName` | Borrowed CK object handles. Null when missing. |
| DataArray and object load | `BML::CK` helpers plus `ObjectLoadOptions`/`ObjectLoadResult` | Object load result copies CK IDs and resolves borrowed handles on demand; it does not expose `XObjectArray*`. |
| Physics actions | `BML::Physics` helpers plus `PhysicalizeDefinition` | Runtime-level wrappers over `ExecuteBB`; return false for unavailable runtime or null targets. |
| 2D text behavior creation | `BML::Text::Create2DText` plus `Text2DDefinition` | Returns borrowed `CKBehavior@`; definition is value-only and CK handles are explicit arguments. |
| Content registration | `ModContext::RegisterBallType`, `RegisterFloorType`, and `RegisterModule` definition objects | Only accepted during script `OnLoad`; late calls return false. |
| Mod registry | `ModContext::FindMod/GetMod*` | `ModRef` is a resolved facade handle, not a native `IMod*`. |
| Dependencies | Metadata plus `ModRef` read APIs | Runtime dependency mutation remains native-only. Script dependencies use `[bml.require]` and `[bml.optional]`. |
| Exports | `ModRef`, `ExportRef`, `CallFrame`, typed scalar `ExportRef::Call*` helpers | Resolved handles are generation-checked and return stale after unload/unregister. Arrays, `array<uint8>` buffers, and `CKObject@` identity values use explicit `CallFrame` methods. |
| DataShare | Typed global helpers, `DataShareSizeOf`, `RequestDataShare(BML::DataShareRequest@+)`, `DataShareRequestRef`, `DataShareEvent` | Script-owned request object implementing `BML::DataShareRequest`. BML retains the object until completion/cancel/unload. Raw byte callbacks are not exposed. |
| Bui menu helpers | `BML::UI` namespace | Stable subset for render-time titles, text, buttons, key capture/formatting, radio choices, paging, navigation, and simple inout inputs. Uses native Bui internally but does not expose `Bui::Window/Page/Menu`, raw draw lists, lambda layout, raw buffers, resources, callbacks, or internal navigation lifecycle. |
| Dear ImGui | Generated `ImGui` namespace plus `docs/bml-imgui-api.as` | Advanced frame-scope API for custom UI. Generated from cimgui with script-friendly string/list/image/drawlist wrappers. Omits context/platform lifecycle, allocators, raw callbacks, raw `void*`, internal debug helpers, and native resource lifecycle. |

## Deliberately Omitted

- Raw CKAngelScript engine/context/module/function handles.
- Native FFI as a supported substitute for plugin-owned script APIs
  (`NativePointer`, `DynCall`, or writable native memory should remain an
  advanced escape hatch, not the normal interop story).
- Native timer callback and command callback pointers.
- `RegisterDependency`, `RegisterOptionalDependency`, and `ClearDependencies`.
- Raw `DataShareRequest` callback/userData, method-name callbacks, and untyped byte payloads.
- Raw Bui `Window/Page/Menu`, raw text buffer callbacks, resource initialization, and internal script/menu transition helpers.
- Raw Dear ImGui context/platform lifecycle, allocator hooks, callback/userData APIs, raw `void*`, internal debug helpers, and native viewport lifecycle.
- Raw Building Block, behavior slot, behavior prototype, and config internals.
- Generated ImGui coverage is tracked by `src/AngelScript/generated/BMLImGuiAngelScriptBindings.report.md`; the generated script stub is `docs/bml-imgui-api.as`.

## Validation

Release validation should cover:

- `tests/smoke/AngelScript/BMLAngelScriptSmoke` for global and `ModContext` parity.
- Compile/runtime/shutdown smoke directories.
- Native interop smoke for native-to-script and script-to-native exports.
- Real Ballance Player validation under the `BML_BALLANCE_ROOT` test install.
