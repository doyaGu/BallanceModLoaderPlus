# BML Script Facade Coverage

This document tracks the script-facing coverage of the native `IBML` surface.
The script facade is intentionally not a raw ABI dump: long-lived callbacks,
dependency mutation, and raw CKAngelScript handles stay outside the script API.

## Coverage Matrix

| Native surface | Script surface | Lifetime and notes |
| --- | --- | --- |
| `GetCKContext`, `GetRenderContext` | `BML::GetCKContext`, `BML::GetRenderContext`, `ModContext` mirrors | Borrowed handles. Do not retain across level unload, reset, or mod unload. |
| CK manager getters | Global `BML::Get*Manager`, `ModContext` mirrors | Borrowed handles. `GetInputManager` returns `BML::InputHook@`. |
| `ExitGame` | `BML::ExitGame`, `ModContext::ExitGame` | Available but not used by smoke tests because it terminates Player. |
| Native timer callbacks | `AddTimer(BML::Timer@+)`, `TimerRef`, `TimerEvent` | Script-owned timer object implementing `BML::Timer`. BML retains the object until cancel/unload. No raw function pointer or userData. |
| Cheat and state | Global helpers, `ModContext` mirrors | State reads are safe outside a level; mutating cheat state is guarded by BML runtime availability. |
| In-game message and command execution | Global helpers, `ModContext` mirrors | Script command objects unregister on mod unload. |
| Command registration | `RegisterCommand(BML::Command@+)`, `CommandRef`, `CommandCompletion` | Script-owned command object implementing the minimal `BML::Command` interface. BML retains the object until unregister/unload. Duplicate names or aliases fail without replacing existing commands. |
| `SetIC`, `RestoreIC`, `Show` | Global helpers, `ModContext` mirrors | Borrowed CK object input. Null object is a no-op. |
| Game state queries | Global helpers, `ModContext` mirrors | Includes score, HUD, SR timer, pause/play/level flags, time/input helpers. |
| Named CK object lookups | Global helpers, `ModContext` mirrors | Borrowed CK object handles. Null when missing. |
| Content registration | Global helpers, `ModContext` mirrors | Only accepted during script `OnLoad`; late calls return false. |
| Mod registry | Global `FindMod/GetMod*`, `ModContext` mirrors | `ModRef` is a resolved facade handle, not a native `IMod*`. |
| Dependencies | Metadata plus `ModRef` read APIs | Runtime dependency mutation remains native-only. Script dependencies use `[bml.require]` and `[bml.optional]`. |
| Exports | `ModRef`, `ExportRef`, `CallFrame`, typed `ExportRef::Call*` | Resolved handles are generation-checked and return not found after unload/unregister. |
| DataShare | Typed global helpers, `DataShareSizeOf`, `RequestDataShare(BML::DataShareRequest@+)`, `DataShareRequestRef`, `DataShareEvent` | Script-owned request object implementing `BML::DataShareRequest`. BML retains the object until completion/cancel/unload. Raw byte callbacks are not exposed. |
| Bui menu helpers | `BML::UI` namespace | Stable subset for render-time titles, text, buttons, paging, navigation, and simple inout inputs. Uses native Bui internally but does not expose `Bui::Window/Page/Menu`, raw draw lists, lambda layout, raw buffers, resources, or internal navigation lifecycle. |
| Dear ImGui | Generated `ImGui` namespace plus `docs/bml-imgui-api.as` | Advanced frame-scope API for custom UI. Generated from cimgui with script-friendly string/list/image/drawlist wrappers. Omits context/platform lifecycle, allocators, raw callbacks, raw `void*`, internal debug helpers, and native resource lifecycle. |

## Deliberately Omitted

- Raw CKAngelScript engine/context/module/function handles.
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
