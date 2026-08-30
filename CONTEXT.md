# Ballance Mod Loader Runtime Context

This context names the loader-owned runtime features that ship with BML itself. It distinguishes the built-in Mod's lifecycle role from the player-facing features it owns.

## Language

**Built-in Loader Mod**:
The loader-owned Mod registered with the id `BML`. It receives Mod lifecycle callbacks and owns the built-in runtime features without absorbing their state and behavior.
_Avoid_: BML service, built-in feature

**Built-in Console**:
The loader's in-game command surface, comprising the command bar, message board, command history, built-in command registration, and their configuration. One Built-in Loader Mod owns one Built-in Console.
_Avoid_: CommandBar, MessageBoard, console UI

**Built-in HUD**:
The loader's in-game status and custom overlay surface, comprising the HUD tree, title, FPS display, speedrun timer, cheat indicator, HUD command, and their configuration. One Built-in Loader Mod owns one Built-in HUD.
_Avoid_: HUD window, HUD host, HUD service

**Built-in Custom Maps**:
The loader's custom-map feature, comprising map discovery, map selection, temporary map preparation, game-script bindings, level selection, and its configuration. One Built-in Loader Mod owns one Built-in Custom Maps Module.
_Avoid_: map menu, map loader, custom map service

**Built-in Gameplay Tweaks**:
The loader's configurable game corrections, comprising the lantern alpha-test option, life-ball freeze fix, overclock patch, and their Virtools script bindings. One Built-in Loader Mod owns one Built-in Gameplay Tweaks Module.
_Avoid_: script patches, tweak settings, gameplay fixes

**Built-in Game Event Hooks**:
The loader-owned Virtools script adapters that translate game and menu transitions into Mod lifecycle and gameplay callbacks. One Built-in Loader Mod owns one Built-in Game Event Hooks Module.
_Avoid_: EventHookRegistrar, callback patches, event bridge

**Virtools Behavior Runtime**:
The loader-owned deep module that resolves any Building Block Prototype, creates a configured instance, reflects its live layout, binds parameter values or sources, runs its callback lifecycle, and distinguishes immediate, persistent, graph-resident, and cross-frame execution. Settings are applied before final input binding because they may rebuild the instance layout. One loader runtime owns one Virtools Behavior Runtime.
Resolved slots carry the configured instance identity and layout generation; a settings callback or execution makes old slot handles stale instead of silently retargeting an ordinal. Literal sources are independent, runtime-owned CK parameters rather than extra behavior or parent locals. Normal release is delayed past native manager callbacks and sends DETACH/DELETE without misusing RESET; world reset explicitly closes retained state before invalidating CK identities.
_Avoid_: action cache, BB singleton, fixed pin table, synchronous-function wrapper

**Ballance Behavior Presets**:
Thin declarative mappings from Ballance's known Building Blocks to `BehaviorSpec`. A preset contains only the Prototype GUID, selectors, defaults, and setting stages; all creation, ownership, callback, execution, and destruction behavior remains in the Virtools Behavior Runtime.
_Avoid_: graph recipe, BB implementation, action module

**Physics Force Sessions**:
The target-identity-indexed persistent instances of Virtools Physics Force. Create and Shutdown use the same configured instance because the Building Block stores its native handle in a local parameter. Replacement and shutdown wait until a later physics epoch because the Building Block may retain its `CKBehavior` in a pre-simulation callback before writing that handle.
_Avoid_: force action cache, independent Set/Unset calls

**Legacy ExecuteBB Adapter**:
The exported v0.3 compatibility interface that translates existing `ExecuteBB` calls into Behavior Specs and delegates them to the Virtools Behavior Runtime. New loader implementation code does not call this adapter.
_Avoid_: ExecuteBB runtime, Building Block module

## Source layout

The private source tree follows these runtime concepts instead of collecting unrelated code under generic `Core`, `Runtime`, or `Builtin` directories:

- `src/BML.cpp` is the loader composition root.
- `src/Mods/` contains concrete bundled `IMod` implementations. `BMLMod` assembles the built-in modules, while `NewBallTypeMod` is an independent bundled Mod.
- `src/Loader/` owns Mod discovery, registration, invocation, lifecycle, and CK manager integration.
- `src/Api/` adapts the public BML interfaces to loader-owned implementations.
- `src/Console/`, `src/HUD/`, and `src/CustomMaps/` contain the Built-in Console, Built-in HUD, and Built-in Custom Maps modules respectively.
- `src/Gameplay/` contains the loader-owned game session, game event hooks, and gameplay tweaks.
- `src/Config/`, `src/DataShare/`, `src/Imc/`, and `src/Logging/` each keep one cross-cutting runtime concern local.
- `src/UI/`, `src/Hooks/`, and `src/Virtools/` contain concrete UI such as the Mod menu, process/engine hooks, the Virtools Behavior Runtime, Ballance Behavior Presets, Physics Force Sessions, and the Legacy ExecuteBB Adapter.
- `src/AngelScript/` and `src/Utils/` remain independently navigable implementation families.

Private includes use these directory names explicitly, so a caller reveals which module interface it crosses.

## Example dialogue

> **Developer:** Should command history be saved by the Built-in Loader Mod?
>
> **Domain expert:** No. Command history belongs to the Built-in Console; the Built-in Loader Mod only tells it when loading and unloading occur.
>
> **Developer:** Should the Built-in Loader Mod update the speedrun timer and HUD elements itself?
>
> **Domain expert:** No. Those belong to the Built-in HUD; the Built-in Loader Mod only forwards game lifecycle events.

> **Developer:** Should the Built-in Loader Mod know which Virtools parameters a custom map load changes?
>
> **Domain expert:** No. Those bindings and the loading sequence belong to Built-in Custom Maps; the Built-in Loader Mod only forwards the relevant object, script, and game lifecycle events.

> **Developer:** Does the Overclock graph state belong to Built-in Game Event Hooks because both inspect gameplay scripts?
>
> **Domain expert:** No. Built-in Game Event Hooks only translate game transitions into callbacks. Overclock and the other configurable corrections belong to Built-in Gameplay Tweaks.
