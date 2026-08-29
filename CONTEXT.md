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

## Example dialogue

> **Developer:** Should command history be saved by the Built-in Loader Mod?
>
> **Domain expert:** No. Command history belongs to the Built-in Console; the Built-in Loader Mod only tells it when loading and unloading occur.
>
> **Developer:** Should the Built-in Loader Mod update the speedrun timer and HUD elements itself?
>
> **Domain expert:** No. Those belong to the Built-in HUD; the Built-in Loader Mod only forwards game lifecycle events.
