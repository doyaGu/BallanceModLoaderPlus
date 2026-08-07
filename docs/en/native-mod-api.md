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

Use the CMake helper installed with the SDK:

```cmake
find_package(BML CONFIG REQUIRED)
bml_add_mod(MyMod MyMod.cpp)
```

`bml_add_mod` links `BML::BML`, enables C++20, disables compiler extensions,
and produces `MyMod.bmodp`. Add `bml_install_mod(MyMod)` when the project needs
an install rule.

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

`OnRender` receives one `CK_RENDER_FLAGS` value. The native API does not expose
separately named before-render and after-render callbacks. Consumers that need
decoupled notifications can use `BML::Events::Stream`, which covers game flow,
object and script loading, physics, commands, configuration, and cheat-state
events.

## `IBML` services

`IBML` is the main loader service passed to a mod. It provides:

- the CK context and Attribute, Behavior, Collision, Input, Message, Path,
  Parameter, Render, Sound, and Time managers;
- `AddTimer` and `AddTimerLoop` scheduling by ticks or time values;
- game state, cheat control, in-game messages, and command registration,
  lookup, and execution;
- named lookup for DataArrays, Groups, Materials, Meshes, 2D/3D Entities,
  Cameras, Lights, Sounds, Textures, and Behaviors;
- Initial Condition and visibility changes, plus skipping the next render tick;
- ball, floor, module, and transformation type registration and SR/HS scores;
- mod enumeration and lookup, plus dependency registration and queries.

Create timers through `IBML`. The SDK does not publish a standalone `Timer.h`;
the loader owns scheduling and callback processing.

## Configuration, commands, and logging

`IConfig` retrieves an `IProperty` by category and key. Properties can be
String, Boolean, Integer, Float, or Keyboard Key and support current values,
defaults, comments, and category comments. There is no separate UTF-16
property API; use the explicit conversion functions in `BML.h` when needed.

`ICommand` provides the command name, aliases, description, cheat flag,
execution, Tab completion, and basic Integer, Float, and Boolean parsers.
`ILogger` provides three log levels.

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

`DataShare` is suitable for small named byte values when both sides obey its
reference-count and borrowed-pointer lifetime rules. Use IMC when an API needs
evolution, cross-language bindings, RPC, or Topic semantics.

## Three UI surfaces

- `Bui` draws Ballance-style ImGui widgets for native overlays.
- `BGui` creates in-game UI from Virtools 2D Entities and Behaviors.
- `BML::UI` does not draw widgets; it controls loader-owned messages, menus,
  and HUD state through IMC.

These surfaces solve different problems and are not interchangeable.

## C API ownership

`BML.h` and `DataShare.h` are callable through the C ABI. Release strings,
wide strings, string arrays, wide-string arrays, and binary buffers allocated
by BML with the matching `BML_Free*` function. Do not call CRT `free` across a
DLL boundary.

`BML_DataShare_Get` returns a borrowed pointer. It becomes invalid when the
same key is set or removed or when the instance is destroyed. Use
`BML_DataShare_CopyEx` when a stable copy is required.

## Further reading

- [Inter-mod communication](imc.md)
- [Create a typed IMC API](imc-author-guide.md)
- [Native mod template](https://github.com/doyaGu/BallanceModLoaderPlus/tree/main/templates/native-mod-template)
