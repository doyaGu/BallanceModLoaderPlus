# Which native API to use

A native mod reaches the loader in three ways, and for some capabilities more
than one of them works. This page says which spelling exists for each
capability, which one to prefer, and why there is more than one to choose from.

If you are writing a new mod and want a single rule: use the legacy `IBML` and
`IMod` interfaces for everything that hands you an engine object or that only
they offer, use the IMC facades for reading game state and for driving the
loader's own UI, and use IMC for anything you publish to other mods.

## Why there is more than one spelling

`IBML`, `IMod`, `IMessageReceiver`, `IConfig`, and `ICommand` are C++ classes
with virtual functions, and a mod calls them across a DLL boundary. A call
through a virtual function is a jump to a slot number that was fixed when the
mod was compiled, so adding, removing, or reordering a virtual function moves
every slot after it and every already built `.bmodp` calls the wrong one. The
loader pins those slot numbers with static assertions in `src/ModContext.cpp`
and compares its exported symbol set against
`tests/abi/legacy-native-exports-x86-msvc.txt` on every build. These interfaces
are therefore frozen for the current release line: nothing new can be added to
them.

IMC has no such problem. A route is addressed by name, the payload is encoded
field by field, and a reader steps over a field it does not know, so a new RPC
or a new field breaks nothing that was built before it. That is why capability
added after the freeze arrives as an IMC interface, and why the built-in
facades exist: they are the loader's own IMC interfaces, wrapped in inline C++
so that using one looks like calling a function.

The `BML_*` functions of `BML.h` are the third spelling. They are plain C with
no vtable involved, so they can be added to freely, and they cover strings,
paths, files, and allocation rather than anything about the game.

Frozen does not mean deprecated. The legacy interfaces are supported, are still
the only way to reach most of what the loader does, and are the only way to get
at an engine object.

## Capability by capability

`Frozen C++` names are members of `IBML` unless said otherwise. `IMC` names are
the facade spelling; each facade is a header of the same name under
`include/BML/`.

| Capability | Frozen C++ | IMC | Which to use |
| --- | --- | --- | --- |
| CK context, render context, and the engine managers | `GetCKContext`, `GetRenderContext`, `GetInputManager`, `GetTimeManager`, and the rest | none | Frozen C++ only. IMC does not hand out engine pointers, by design: a pointer cannot be given a lifetime the other side can check. |
| Is the game in a level, paused, playing, cheating | `IsIngame`, `IsPaused`, `IsPlaying`, `IsCheatEnabled` | `Runtime::ReadState` | Either. The facade reads all five flags at once and works from a class that is not an `IMod`. |
| Frame time and frame count | `GetTimeManager()` and the CK clocks | `Runtime::ReadClock` | Either. |
| Speedrun time and highscore value | `GetSRScore`, `GetHSScore` | `Runtime::ReadScore`, `Speedrun::ReadTimerState` | Either. `Score::SR` is the elapsed speedrun time in milliseconds, not a score. |
| Start, pause, reset, or show the speedrun timer | none | `Speedrun::StartTimer`, `PauseTimer`, `ResetTimer`, `SetTimerVisible` | IMC only. |
| Find an object by name | `Get3dObjectByName`, `GetGroupByName`, `GetMaterialByName`, and the rest of the family | `Scene::FindObject`, with or without a class id | Frozen C++ when you then have to touch the object with the CK SDK, since it hands back the pointer. The facade hands back a `BML_ObjectRef`, which is what to use when the object is only being identified or passed on. |
| Read an object's class, name, or transform | the CK SDK, through the pointer | `Scene::ReadObject`, `Scene::ReadEntityTransform` | Either. |
| Level state, energy, checkpoints, reset points, level catalog | `GetArrayByName` plus `CKDataArray` column reads | `Gameplay::ReadLevel`, `ReadEnergy`, `ReadCheckpoints`, `ReadResetpoints`, `ReadCatalog` | IMC. The facade already knows the column order of the game's arrays, which is the part that is easy to get wrong. The collection reads copy the whole collection, so they belong in setup or a level change rather than in a frame. |
| In-game message board | `SendIngameMessage` | `UI::AddMessage`, `UI::ClearMessages` | Either. Only the facade can clear the board. |
| HUD parts, mods menu, map menu | none | `UI::SetHUDMode`, `ShowTitle`, `ShowFPS`, `OpenModsMenu`, `CloseModsMenu`, `OpenMapMenu`, `CloseMapMenu` | IMC only. |
| Loader events | the `IMessageReceiver` virtuals on `IMod` | `Events::Stream` | Either, and they carry the same events. The virtuals run inside the loader's dispatch and need an `IMod` subclass. The stream is a queue you drain yourself, which suits code that is not an `IMod`, code that treats every kind the same way, and code that would rather buffer than react at once. |
| Cheat mode | `EnableCheat` to set, `IsCheatEnabled` to read | `Runtime::ReadState` reads it | Read either, set through the frozen C++. |
| Console commands | `RegisterCommand` plus an `ICommand` subclass | none | Frozen C++ only. There is no unregister function yet, so a registered command has to live for the whole process. |
| Configuration | `IMod::GetConfig` plus `IConfig` and `IProperty` | none | Frozen C++ only. |
| Timers | `AddTimer`, `AddTimerLoop` | none | Frozen C++ only. |
| Exit the game, initial conditions, visibility, physics type registration, skipping a render tick | `ExitGame`, `SetIC`, `RestoreIC`, `Show`, `RegisterBallType` and the rest of the registration family, `SkipRenderForNextTick` | none | Frozen C++ only. |
| Which mods are loaded, and dependencies | `GetModCount`, `GetMod`, `FindMod`, `RegisterDependency`, `CheckDependencies` | none | Frozen C++ only. |
| Publishing an API of your own to other mods | none | IMC, ideally generated from a `.imc` file | IMC only. A C++ class of your own would put your vtable layout and your standard library in every consumer's build. |
| Drawing your own UI | `Bui` for ImGui widgets, `BGui` for in-game 2D entities | none | Neither is IMC. `BML::UI` controls the loader's own UI and draws nothing of yours. |
| Strings, paths, files, allocation | none | the `BML_*` functions of `BML.h` | The C exports. Release what they return with the matching `BML_Free*`, never with the CRT `free`. |

## Mixing them

Mixing is expected, and a mod can use all three in the same function. The
built-in IMC interfaces are served by the loader itself, reading the same state
`IBML` reads, so there is no second copy of anything and nothing to keep in
sync. `Runtime::ReadState` and `IsIngame` cannot disagree.

Three differences do show through:

- **How failure is reported.** The frozen C++ functions mostly return the value
  or nothing at all, and a failed `RegisterCommand` only writes to the log. Every
  IMC function returns a status: `BML_OK`, or one of the negative codes in
  `Defines.h`. The C++ IMC operations are `[[nodiscard]]`, so handle the status
  or cast it away on purpose.
- **What you get back.** The frozen C++ lookups hand you an engine pointer,
  which is only good while the object lives and only usable on the game thread.
  The facades hand you a value, either a plain struct or a `BML_ObjectRef` that
  the loader can check before it resolves.
- **Threading.** The frozen C++ interfaces are game-thread only. An IMC call can
  be made from another thread, and the loader runs the handler on its next pump.
  A call made from the game thread to a game-thread handler, which is what every
  built-in facade is, runs inline instead, so the answer is already there when
  the facade's wait looks for it. That is why `Runtime::ReadState` works from
  `OnProcess`. Waiting on a call that is still pending is what the game thread
  refuses, with `BML_ERROR_WRONG_THREAD`.

## Further reading

- [Native mod API overview](native-mod-api.md)
- [Inter-mod communication](imc.md)
- [Create a typed IMC API](imc-author-guide.md)
