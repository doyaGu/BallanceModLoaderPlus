# Which native API to use

A native mod reaches the loader in three ways, and for some capabilities more
than one of them works. This page says which spelling exists for each
capability, which one to prefer, and why there is more than one to choose from.
IMC is not one of the three: it carries no loader capability, and is what one mod
publishes for other mods. See [Inter-mod communication](imc.md) for that.

If you are writing a new mod and want a single rule: use the legacy `IBML` and
`IMod` interfaces for everything that hands you an engine object or that only
they offer, use the interface structs for reading game state, for loader events,
and for driving the loader's own UI, and use IMC for anything you publish to
other mods.

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

The interface structs have no such problem. The loader hands out a struct of
function pointers, a mod asks for one by id and major version through the single
export `BML_GetInterface`, and the struct only ever grows by appending a member
and bumping its minor version. An older mod keeps calling the members it was
built against, and a newer mod asks `BML_IFACE_HAS` whether the running loader
has a member added after it. That is why capability added after the freeze
arrives as an interface struct, and each one ships with an inline C++ namespace
over it, so using one looks like calling a function.

The `BML_*` functions of `BML.h` are the third spelling. They are plain C with
no vtable involved, so they can be added to freely. Only two kinds of capability
belong there. The first is a utility that touches neither the game nor loader
state: strings, paths, files, encoding, and allocation, which is where the
loader and mod directory queries sit. The second is the other half of an
operation whose first half is frozen in C++, which is how
`BML_UnregisterCommand` came to stand beside `IBML::RegisterCommand`. Splitting
one pair of operations across two mechanisms reads worse than either choice
alone, so the reverse operation follows the forward one.

One question decides which of the three a new capability belongs to: who serves
it. The loader serving something a mod reads or drives is an interface struct. A
mod serving something for other mods is an IMC interface, because the loader is
not in that conversation and the two sides ship on their own schedules. A utility
that touches neither the game nor loader state, or the reverse of an operation
frozen in C++, is a C export.

Frozen does not mean deprecated. The legacy interfaces are supported, are still
the only way to reach most of what the loader does, and are the only way to get
at an engine object.

The script side reaches five of these capabilities: `BML::Runtime`,
`BML::Gameplay`, `BML::UI`, `BML::Events`, and `BML::Speedrun`. `BML::Scene` is
native-only. The script `BML::Speedrun` is not the interface projected as it
stands: it is spelled `SetTimerVisible`, `StartTimer`, `PauseTimer`,
`ResetTimer`, and `GetElapsedTime`, and it returns the value or nothing rather
than a status. The script projection is written by hand and nothing checks it
against the interface structs, so neither the missing capability nor the
spelling difference closes by itself.

## Capability by capability

`Frozen C++` names are members of `IBML` unless said otherwise. A `Newer route`
name spelled `Namespace::Function` is the inline C++ over an interface struct,
declared in the header of the same name under `include/BML/`; the rest are the
`BML_*` C exports of `BML.h`.

| Capability | Frozen C++ | Newer route | Which to use |
| --- | --- | --- | --- |
| CK context, render context, and the engine managers | `GetCKContext`, `GetRenderContext`, `GetInputManager`, `GetTimeManager`, and the rest | none | Frozen C++ only. The interface structs do not hand out engine pointers, by design: a pointer cannot be given a lifetime the other side can check. |
| Is the game in a level, paused, playing, cheating | `IsIngame`, `IsPaused`, `IsPlaying`, `IsCheatEnabled` | `Runtime::ReadState` | Either. The facade reads all five flags at once and works from a class that is not an `IMod`. |
| Frame time and frame count | `GetTimeManager()` and the CK clocks | `Runtime::ReadClock` | Either. |
| Speedrun time and highscore value | `GetSRScore`, `GetHSScore` | `Runtime::ReadScore`, `Speedrun::ReadTimerState` | Either. `Score::SR` is the elapsed speedrun time in milliseconds, not a score. |
| Start, pause, reset, or show the speedrun timer | none | `Speedrun::StartTimer`, `PauseTimer`, `ResetTimer`, `SetTimerVisible` | The interface struct only. |
| Find an object by name | `Get3dObjectByName`, `GetGroupByName`, `GetMaterialByName`, and the rest of the family | `Scene::FindObject`, with or without a class id | Frozen C++ when you then have to touch the object with the CK SDK, since it hands back the pointer. `Scene::FindObject` hands back a `BML_ObjectRef`, which is what to use when the object is only being identified or passed on. |
| Read an object's class, name, or transform | the CK SDK, through the pointer | `Scene::ReadObject`, `Scene::ReadEntityTransform` | Either. |
| Level state, energy, checkpoints, reset points, level catalog | `GetArrayByName` plus `CKDataArray` column reads | `Gameplay::ReadLevel`, `ReadEnergy`, `ReadCheckpoints`, `ReadResetpoints`, `ReadCatalog` | The interface struct. It already knows the column order of the game's arrays, which is the part that is easy to get wrong. The collection reads copy the whole collection, so they belong in setup or a level change rather than in a frame. |
| In-game message board | `SendIngameMessage` | `UI::AddMessage`, `UI::ClearMessages` | Either. Only the facade can clear the board. |
| HUD parts, mods menu, map menu | none | `UI::SetHUDMode`, `ShowTitle`, `ShowFPS`, `OpenModsMenu`, `CloseModsMenu`, `OpenMapMenu`, `CloseMapMenu` | The interface struct only. |
| Loader events | the `IMessageReceiver` virtuals on `IMod` | `Events::Stream` | Either, and they carry the same events. The virtuals run inside the loader's dispatch and need an `IMod` subclass. The stream is a queue you drain yourself, which suits code that is not an `IMod`, code that treats every kind the same way, and code that would rather buffer than react at once. |
| Cheat mode | `EnableCheat` to set, `IsCheatEnabled` to read | `Runtime::ReadState` reads it | Read either, set through the frozen C++. |
| Console commands | `RegisterCommand` plus an `ICommand` subclass | none | Frozen C++ to register. Removing one again is a C export, `BML_UnregisterCommand`, because `IBML` could not grow the function. |
| Configuration | `IMod::GetConfig` plus `IConfig` and `IProperty` | none | Frozen C++ only. |
| Timers | `AddTimer`, `AddTimerLoop` | none | Frozen C++ only. |
| Exit the game, initial conditions, visibility, physics type registration, skipping a render tick | `ExitGame`, `SetIC`, `RestoreIC`, `Show`, `RegisterBallType` and the rest of the registration family, `SkipRenderForNextTick` | none | Frozen C++ only. |
| Which mods are loaded, and dependencies | `GetModCount`, `GetMod`, `FindMod`, `RegisterDependency`, `CheckDependencies` | none | Frozen C++ only. |
| Publishing an API of your own to other mods | none | IMC, ideally generated from a `.imc` file | IMC only. A C++ class of your own would put your vtable layout and your standard library in every consumer's build, and `BML_GetInterface` is no alternative: it hands out the loader's own interfaces and a mod cannot add to it. IMC reaches native consumers: a script mod can currently neither call another mod's route nor publish one of its own. |
| Drawing your own UI | `Bui` for ImGui widgets, `BGui` for in-game 2D entities | none | Neither of these is `BML::UI`, which controls the loader's own UI and draws nothing of yours. |
| Strings, paths, files, allocation | none | the `BML_*` functions of `BML.h` | The C exports. Release what they return with the matching `BML_Free*`, never with the CRT `free`. |
| The loader's directories, and where your mod is installed | none | `BML_GetLoaderPathW`, `BML_GetLoaderPathUtf8`, `BML_GetModRootW`, `BML_GetModRootUtf8`, also C exports of `BML.h` | The C exports. `IBML` never offered these. The loader path is borrowed and the mod root is allocated, so only the second needs freeing. |

## Mixing them

Mixing is expected, and a mod can use all three in the same function. The
interface structs are served by the loader itself, reading the same state `IBML`
reads, so there is no second copy of anything and nothing to keep in sync. `Runtime::ReadState` and `IsIngame` cannot disagree.

Three differences do show through:

- **How failure is reported.** The frozen C++ functions mostly return the value
  or nothing at all, and a failed `RegisterCommand` only writes to the log. Every
  interface struct function returns a status: `BML_OK`, or one of the negative
  codes in `Defines.h`. Their inline C++ wrappers are `[[nodiscard]]`, so handle
  the status or cast it away on purpose.
- **What you get back.** The frozen C++ lookups hand you an engine pointer,
  which is only good while the object lives and only usable on the game thread.
  The facades hand you a value, either a plain struct or a `BML_ObjectRef` that
  the loader can check before it resolves.
- **Threading.** The frozen C++ interfaces are game-thread only. An interface
  struct call is a direct call into the loader on the calling thread, so nothing
  is queued and there is nothing to wait for, which is why `Runtime::ReadState`
  works from `OnProcess`. `BML::Gameplay`, `BML::Scene`, and `BML::UI` touch the
  game's arrays, its objects, and the UI the loader draws, and `BML::Events`
  keeps queues that carry no locks, so all four answer `BML_ERROR_WRONG_THREAD`
  when called from any other thread. `BML::Runtime` and `BML::Speedrun` do not
  refuse another thread, but they are meant for the game thread too. Before the
  loader has loaded its mods every one of them answers `BML_ERROR_FAIL`.

## Further reading

- [Native mod API overview](native-mod-api.md)
- [Inter-mod communication](imc.md)
- [Create a typed IMC API](imc-author-guide.md)
