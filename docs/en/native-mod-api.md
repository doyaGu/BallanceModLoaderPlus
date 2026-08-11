# BML+ native mod API overview

This page groups the public headers installed by the BML+ SDK by purpose. The
installed `include/BML` directory defines the supported native API.

## Minimal entry point

A native mod is a dynamic library that exports `BMLEntry` and normally uses the
`.bmodp` extension:

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

The object returned by `BMLEntry` is allocated by the Mod DLL. Export
`BMLExit` and destroy that same object there so allocation and deallocation use
the same C++ runtime. BML calls `BMLExit` when registration fails after object
creation and when a loaded native Mod is unloaded. For compatibility, BML can
still load an older DLL without `BMLExit`, but it logs a warning and cannot
destroy that Mod instance safely.

Use the CMake helper installed with the SDK:

```cmake
find_package(BML CONFIG REQUIRED)
bml_add_mod(MyMod MyMod.cpp)
bml_install_mod(MyMod)
```

`bml_add_mod` links `BML::BML`, enables C++20, disables compiler extensions,
and produces `MyMod.bmodp`. It requires an MSVC-compatible 32-bit target and
makes the linker require the exact C symbols `BMLEntry` and `BMLExit`. A missing
or C++-mangled entry point therefore fails the build instead of producing a Mod
that the loader cannot use safely.

`bml_install_mod` adds the standard install rule. Set `CMAKE_INSTALL_PREFIX` to
the Ballance `ModLoader` directory, then use CMake's `install` target to build
and deploy the Mod under `ModLoader/Mods`.

## Public headers

| Header | Purpose |
| --- | --- |
| `Version.h`, `Defines.h` | Version macros, export macros, status codes, and base definitions |
| `BML.h` | C ABI for version, memory, encoding, path, file, and Zip utilities |
| `BMLAll.h` | Convenience header that includes the complete native SDK surface |
| `IMod.h`, `IMessageReceiver.h` | Mod metadata, lifecycle, gameplay, and engine callbacks |
| `IBML.h` | Loader services, CK managers, lookup, commands, timers, and dependencies |
| `ICommand.h` | Command execution, completion, and basic argument parsing |
| `IConfig.h` | Typed configuration properties |
| `ILogger.h` | Info, Warn, and Error logging |
| `DataShare.h` | Low-level, named in-process byte sharing |
| `Imc.h`, `ImcTypes.h`, `ImcWire.hpp`, `ImcCpp.hpp`, `ImcMath.h` | IMC C/C++ runtime, wire format, and base types |
| `Generated/*.hpp` | Built-in bindings generated from `.imc`; do not edit them |
| `Runtime.h`, `Scene.h`, `Gameplay.h`, `UI.h`, `Events.h`, `EventKinds.h` | Convenient C++ facades for built-in IMC services |
| `Bui.h` | Ballance-style ImGui widgets |
| `Gui.h`, `Gui/*.h` | `BGui` wrappers around Virtools entities and behaviours |
| `InputHook.h` | Keyboard, mouse, controller state, and paired input-block tokens |
| `ExecuteBB.h` | Execute or create common Building Blocks |
| `ScriptHelper.h` | Find, connect, insert, and remove behaviour nodes and parameters |
| `Guids.h`, `Guids/*.h` | Virtools and Ballance Building Block GUIDs |

## Mod lifecycle and events

`IMod` inherits `IMessageReceiver`. A mod supplies its ID, version, name,
author, description, and required BML version, then overrides only the
callbacks it needs:

- lifecycle: `OnLoad`, `OnUnload`, `OnProcess`, and `OnRender`;
- configuration and commands: `OnModifyConfig`, `OnPreCommandExecute`,
  `OnPostCommandExecute`, and `OnCheatEnabled`;
- engine objects: `OnLoadObject`, `OnLoadScript`, `OnPhysicalize`, and
  `OnUnphysicalize`;
- game flow: menu, load, start, reset, pause, exit, next-level, death, finish,
  checkpoint, life, and navigation callbacks from `IMessageReceiver`.

`OnProcess` is the only callback that runs inside the active ImGui frame. Draw
every ImGui and `Bui` control from it, and never from `OnRender`. See
[Three UI surfaces](#three-ui-surfaces).

`OnRender` receives one `CK_RENDER_FLAGS` value. The native API does not expose
separately named before-render and after-render callbacks. Consumers that need
decoupled notifications can use `BML::Events::Stream`, which covers game flow,
object and script loading, physics, commands, configuration, and cheat-state
events.

Open the stream before polling it. `Poll` returns `BML_OK` only when it removes
an event from the queue, `BML_ERROR_NOT_FOUND` while an open stream has no
queued event, and `BML_ERROR_INVALID_HANDLE` when the stream is not open. The
output event is reset before every poll, including unsuccessful polls.

## `IBML` services

`IBML` is the main loader service passed to a mod. It provides:

- the CK context and Attribute, Behavior, Collision, Input, Message, Path,
  Parameter, Render, Sound, and Time managers;
- `AddTimer` and `AddTimerLoop` scheduling by frames or by milliseconds;
- game state, cheat control, in-game messages, and command registration,
  lookup, and execution;
- named lookup for DataArrays, Groups, Materials, Meshes, 2D/3D Entities,
  Cameras, Lights, Sounds, Textures, and Behaviors;
- Initial Condition and visibility changes, plus skipping the next render tick;
- ball, floor, module, and transformation type registration and SR/HS scores;
- mod enumeration and lookup, plus dependency registration and queries.

Create timers through `IBML`. The SDK does not publish a standalone `Timer.h`;
the loader owns scheduling and callback processing.

`AddTimer` and `AddTimerLoop` are each overloaded on `CKDWORD` and `float`. The
`CKDWORD` overloads count frames, the `float` overloads count milliseconds, and
both units share one name, so an unsuffixed integer literal is ambiguous and
fails to compile. Write the suffix explicitly:

```cpp
bml->AddTimer(1ul, [] { /* next frame */ });
bml->AddTimer(1000.0f, [] { /* one second later */ });
bml->AddTimerLoop(1.0f, [] { return KeepRunning(); });
```

The loop callbacks keep running while they return `true`. Neither overload
returns a handle, so a scheduled timer cannot be cancelled; make the loop
callback return `false` instead.

## Native mod dependencies

Register dependencies before BML initializes mods. The constructor is the
usual place because it runs while `BMLEntry` creates the mod and before any
`OnLoad` callback:

```cpp
explicit MyMod(IBML *bml) : IMod(bml) {
    AddDependency("RequiredMod", BMLVersion(1, 2, 0));
    AddOptionalDependency("OptionalMod", BMLVersion(1, 0, 0));
}
```

BML orders mods so installed dependencies receive `OnLoad` before their
dependents. A missing optional dependency is ignored. A missing required
dependency or a dependency cycle prevents the mod initialization phase from
starting; the log identifies the requesting mod, required id and version, or
the mods affected by the cycle. If an installed dependency is older than the
requested version, BML skips the dependent mod's `OnLoad` and reports both
versions while continuing with other mods.

## Configuration, commands, and logging

`IConfig` retrieves an `IProperty` by category and key. Properties can be
String, Boolean, Integer, Float, or Keyboard Key and support current values,
defaults, comments, and category comments. There is no separate UTF-16
property API; use the explicit conversion functions in `BML.h` when needed.

`ICommand` provides the command name, aliases, description, cheat flag,
execution, Tab completion, and basic Integer, Float, and Boolean parsers.
`ILogger` provides three log levels.

`IBML::RegisterCommand` takes a raw `ICommand *` and the loader never deletes
it. Allocate the command once and keep it alive for the whole process lifetime.
Do not delete it in `OnUnload`: unloading a single mod does not remove its
commands from the command table, so a deleted command leaves a dangling entry
there. `IBML` has no matching unregister function. Registration returns `void`
and only writes to the log when it fails, which happens for a null command, an
invalid name or alias, and an already registered name.

`ParseFloat` clamps to the whole finite float range by default. Earlier releases
defaulted its lower bound to `FLT_MIN`, the smallest positive normal value, so
negative input was silently clamped to about `1.17e-38`. Pass explicit bounds
when a command needs a narrower range.

## Inter-mod communication

Prefer IMC for new public integrations:

- a `.imc` file contains interface declarations only; field IDs are permanent
  wire identifiers rather than array positions;
- `bml_target_imc_api` generates C++ bindings and adds them to a target;
- RPC supports synchronous calls, futures, cancellation, timeouts, and
  completion callbacks;
- Topic supports bounded subscriber queues, unsubscribe, and drop counts;
- generated types and cached route IDs keep text parsing out of hot paths.

Built-in facades include:

- `BML::Runtime` for runtime state, clock, and scores;
- `BML::Scene` for object information, transforms, and named lookup;
- `BML::Gameplay` for level, energy, directory, checkpoint, and reset data;
- `BML::UI` for the message board, mod/map menus, and HUD;
- `BML::Speedrun` for the shared speedrun timer;
- `BML::Events` for the typed event stream.

The native `BML::Gameplay` collection reads return complete snapshots in a
caller-owned `std::vector`. Read the catalog during setup and refresh level
checkpoints or reset points when the level changes; these calls transfer the
complete collection and are not intended for per-frame polling.

C++ IMC operations that return a BML status are marked `[[nodiscard]]`. Handle
the returned status, or use an explicit `(void)` cast when deliberately
discarding the result of best-effort cleanup.

`DataShare` is suitable for small named byte values when both sides obey its
reference-count and borrowed-pointer lifetime rules. Use IMC when an API needs
evolution, cross-language bindings, RPC, or Topic semantics.

## Three UI surfaces

- `Bui` draws Ballance-style ImGui widgets for native overlays.
- `BGui` creates in-game UI from Virtools 2D Entities and Behaviors.
- `BML::UI` does not draw widgets; it controls loader-owned messages, menus,
  and HUD state through IMC.

These surfaces solve different problems and are not interchangeable.

### Draw ImGui from `OnProcess`

The loader owns the ImGui frame. It opens the frame before mod callbacks run and
ends it immediately after `OnProcess` returns:

1. the loader calls `ImGui::NewFrame` before the per-frame mod callbacks;
2. every mod's `OnProcess` runs inside that frame;
3. the loader calls `ImGui::Render`, which ends the frame;
4. `OnRender` runs;
5. the loader submits the recorded draw data.

So `ImGui` and `Bui` calls belong in `OnProcess`. The same calls made from
`OnRender` happen after the frame has ended: they draw nothing and may trip an
ImGui assertion. `BML::UI` message, menu, and HUD calls are not affected,
because they change loader state instead of recording draw commands.

## C API ownership

`BML.h` and `DataShare.h` are callable through the C ABI. Release strings,
wide strings, string arrays, wide-string arrays, and binary buffers allocated
by BML with the matching `BML_Free*` function. Do not call CRT `free` across a
DLL boundary.

`BML_DataShare_Get` returns a borrowed pointer. It becomes invalid when the
same key is set or removed or when the instance is destroyed. Use
`BML_DataShare_CopyEx` when a stable copy is required.

## Further reading

- [Which native API to use](native-api-routes.md)
- [Choose a mod development route](modding.md)
- [Inter-mod communication](imc.md)
- [Create a typed IMC API](imc-author-guide.md)
- [Native mod template](https://github.com/doyaGu/BallanceModLoaderPlus/tree/main/templates/native-mod-template)
