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

## Source layout

The private source tree follows these runtime concepts instead of collecting unrelated code under generic `Core`, `Runtime`, or `Builtin` directories:

- `src/BML.cpp` is the loader composition root.
- `src/Mods/` contains concrete bundled `IMod` implementations. `BMLMod` assembles the built-in modules, while `NewBallTypeMod` is an independent bundled Mod.
- `src/Loader/` owns Mod discovery, registration, invocation, lifecycle, and CK manager integration.
- `src/Api/` adapts the public BML interfaces to loader-owned implementations.
- `src/Console/`, `src/HUD/`, and `src/CustomMaps/` contain the Built-in Console, Built-in HUD, and Built-in Custom Maps modules respectively.
- `src/Gameplay/` contains the loader-owned game session, game event hooks, and gameplay tweaks.
- `src/Config/`, `src/DataShare/`, `src/Imc/`, and `src/Logging/` each keep one cross-cutting runtime concern local.
- `src/UI/`, `src/Hooks/`, and `src/Virtools/` contain concrete UI such as the Mod menu, process/engine hooks, and Virtools graph/object adapters.
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
